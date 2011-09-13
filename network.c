/**
 * network.c
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of the network layer.
 *
 * The network layer takes a segment, packs it into a datagram and passes it to
 * the link layer. It calculates the outgoing link by its forwarding table.
 * 
 * When receiving a datagram it either forwards it to the next hop (using the
 * forwarding table) or hands it to the upper layer if the host is the
 * destination host.
 * 
 * The second task is to build the forwarding table.
 * TODO using which algorithm?????
 * 
 * For each datagram it is checked if it is "normal" data or a routing packet.
 */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"
#include "transport.h"


/**
 * Max number of hops a datagram is allowed to travel before it gets dropped.
 */
#define HOP_LIMIT 8

void int2string(char* s, int i);

/**
 * Stores which route a packet should travel for a given destination.
 */
HASHTABLE forwarding_table;

//TODO 
HASHTABLE routing_table;



/**
 * Takes a segment and delivers it to addr.
 * @param addr destination address
 * @param data segment to send
 * @param size size of segment
 */
void network_transmit(CnetAddr addr, char *data, size_t size)
{
	int link =  network_lookup(addr);
	transmit_datagram(link, false, addr, data, size);
}

/**
 * Takes a segment, adds datagram header and passes datagram
 * to the link layer.
 * @param link    link to send the segment on
 * @param routing segment contains routing data
 * @param addr    destination address
 * @param data    segment to send
 * @param size    size of segment
 */
void transmit_datagram(int link, bool routing, CnetAddr addr, char *data, size_t size)
{
	/* datagram header */
	datagram_header header;
	header.srcaddr = nodeinfo.address;
	header.destaddr = addr;
	header.hoplimit = HOP_LIMIT;
	header.routing = routing;

	/* assemble datagram */
	DATAGRAM datagram;
	datagram.header = header;
	memcpy(&datagram.payload, data, size);
	size_t datagramSize = size + sizeof(datagram_header);

	/* send datagram */
	link_transmit(link, (char*) &datagram, datagramSize);
}

/**
 * Takes a datagram and checks its destination.
 * Either unpacks segment from the datagram and hands it to the upper layer
 * or forwards the datagram to the next hop (on the route to its destination).
 * @param link Link which received the datagram.
 * @param data The received datagram.
 * @param size Size of the datagram.
 */
void network_receive(int link, char *data, size_t size)
{
	DATAGRAM *datagram = (DATAGRAM*) data;
	CnetAddr srcaddr = datagram->header.srcaddr;
	CnetAddr destaddr = datagram->header.destaddr;

	if(0 >= datagram->header.hoplimit)
		return; //hoplimit exceeded -> drop data

	if(nodeinfo.address == destaddr) {
		/* datagram destination = this node -> hand to upper layer */
		//TODO evaluate datagram->routing_flag
		assert(!datagram->header.routing);
		char* segment = datagram->payload;
		size_t segmentSize = size - sizeof(datagram_header);
		transport_receive(srcaddr, segment, segmentSize);
	}
	else {
		/* datagram destination = foreign node -> forward */
		int link = network_lookup(destaddr);

		datagram->header.hoplimit--;
		link_transmit(link, (char*) datagram, size);
	}
}

/**
 * Initializes the network layer.
 * 
 * Must be called before the network layer can be used after reboot.
 */
void network_init()
{
	forwarding_table = hashtable_new(0);

  #define SB  134
	#define HOM 96
	#define SLS 182

	/* static forwarding_table creation for saarnet_3 */
	char key[5];
	int* one = malloc(sizeof(one)); *one = 1;
	int* two = malloc(sizeof(two)); *two = 2;

	switch(nodeinfo.address) {
	case SB:
		int2string(key, HOM);	hashtable_add(forwarding_table, key, one, sizeof(int));
		int2string(key, SLS);	hashtable_add(forwarding_table, key, two, sizeof(int));
		break;
	case HOM:
		int2string(key, SB);	hashtable_add(forwarding_table, key, one, sizeof(int));
    int2string(key, SLS);	hashtable_add(forwarding_table, key, one, sizeof(int));
		break;
	case SLS:
		int2string(key, SB);	hashtable_add(forwarding_table, key, one, sizeof(int));
    int2string(key, HOM);	hashtable_add(forwarding_table, key, one, sizeof(int));
		break;
	}
}

/**
 * Lookup link in forwarding_table and returns the corresponding link.
 * 
 * @param addr The address to look up.
 * @return The link to send the data over.
 */
int network_lookup(CnetAddr addr)
{
	char key[5];
	int2string(key, addr);
	return *((int*) hashtable_find(forwarding_table, key, NULL));
}

/**
 * Returns a node's own network address.
 */
CnetAddr network_get_address()
{
	return nodeinfo.address;
}


/**********************************************************/
/*					Routing								*/
/**********************************************************/

#define MAX_NEIGHBOURS 100

/**
 * Timeout in usec when a routing segment needs to be resend.
 */
#define ROUTING_TIMEOUT 1000000

NEIGHBOUR *neighbours; //for simplicity neighbours[0] == myself (is empty)

/**
 * @param distance_info Distance information to send.
 * @param size Size of distance information.
 * @param link Link to send the distance information on.
 */
void transmit_distance_info(DISTANCE_INFO *distance_info, size_t size, int link)
{
	NEIGHBOUR nb = neighbours[link];
	
	ROUTING_SEGMENT *rSeg = malloc(sizeof(*rSeg));
	rSeg->header.seq_num = nb.nextSeqNum++;
	rSeg->header.ack_num = nb.nextAckNum;
	memcpy(rSeg->distance_info, distance_info, size);
	
	OUT_ROUTING_SEGMENT *outSeg = malloc(sizeof(*outSeg));
	outSeg->link = link;
	outSeg->rSeg = rSeg;
	outSeg->size = size + sizeof(routing_header);
	
	int pos = vector_nitems(nb.outRoutingSegments);
	vector_append(nb.outRoutingSegments, outSeg, sizeof(*outSeg));
	free(outSeg);
	outSeg = vector_peek(nb.outRoutingSegments, pos);

	link_transmit(link, (char *)outSeg->rSeg, outSeg->size);
	outSeg->timerId = CNET_start_timer(ROUTING_TIMER, ROUTING_TIMEOUT, (CnetData) outSeg);
}

void broadcast_distance_info(DISTANCE_INFO *distance_info, size_t size)
{
	int num_neighbours = link_num_links();
	for(int i=1; i<=num_neighbours; i++) {
		transmit_distance_info(distance_info, size, i);
	}
}

void routing_receive(int link, char *data, size_t size)
{
	ROUTING_SEGMENT *rSeg = (ROUTING_SEGMENT *)data;
	NEIGHBOUR nb = neighbours[link];
	
	if(nb.nextAckNum != rSeg->header.seq_num) {
		return; //ignore out of order routing segment
	}
	
	/* process acknowledgement */
	if(nb.nextAckNum == rSeg->header.seq_num) {
		nb.nextAckNum++;
		//~ while(nb.nextAckNum == squeue_peek(nb.inUnAckSeqNum)) {
			//~ nb.nextAckNum++;
			//~ squeue_pop(nb.inUnAckSeqNum);
		//~ }
	}
	else {
		//~ if(!squeue_contains(nb.inUnAckSeqNum)) {
			//~ squeue_insert(nb.inUnAckSeqNum, rSeg->header.seq_num;
		//~ }
	}
	
	/* process routing information */
	int distInfoLength = (size - sizeof(routing_header)) / sizeof(DISTANCE_INFO);
	DISTANCE_INFO sendDistInfo[distInfoLength];
	int updates = 0;
	for(int i=0; i<distInfoLength; i++) {
		DISTANCE_INFO distInfo = rSeg->distance_info[i];
		if(update_routing_table(link, distInfo, &(sendDistInfo[updates]))) {
			updates++;
		}
	}
	
	/* broadcast distance updates */
	if(updates > 0)
		broadcast_distance_info(sendDistInfo, updates * sizeof(DISTANCE_INFO));
}

/**
 * Updates the routing table with the given distance information.
 */
bool update_routing_table(int link, DISTANCE_INFO inDistInfo, DISTANCE_INFO *outDistInfo)
{
	ROUTING_ENTRY *entry = routing_lookup(inDistInfo.destAddr);
	int bestChoice = 0, bestWeigth = INT_MAX;
	if(NULL == entry) {
		char key[5];
		int2string(key, addr);
		ROUTING_ENTRY[link_num_links()] newEntry;
		for(int i=0; i<link_num_links(); i++) {
			newEntry[i].weight = INT_MAX;
			newEntry[i].minMTU = INT_MAX;
		}
		hashtable_add(routing_table, key, newEntry, sizeof(entry);
		entry = routing_lookup(inDistInfo.destAddr);
		bestChoice = link;
	}
	else {
		for(int i=0; i<link_num_links(); i++) {
			if(entry[i].weight < bestWeight)
				bestChoice = i;
		}
	}
	
	assert(NULL != entry);
	
	entry[link].weight = inDistInfo.weigth + get_weigth(link);
	entry[link].minMTU = MIN(inDistInfo.minMTU, link_get_mtu(link);
	
	bool bestChoiceChanged = (bestChoice == link);
	if(bestChoiceChanged) {
		outDistInfo->destAddr = inDistInfo.destAddr;
		outDistInfo->weight = entry[link].weight;
		outDistInfo->minMTU = entry[link].minMTU;
	}

	/* enable message delivery to that node */
	CNET_enable_application(inDistInfo.destAddr);
	
	return bestChoiceChanged;
}

/**
 * Calculates costs for transmiting data over given link.
 * 
 * @param link Link.
 */
int get_weigth(int link)
{
	return 10000000 * 1.0 / link_get_bandwidth(link);
}

/**
 * Looks up an address in the routing table.
 */
ROUTING_ENTRY *routing_lookup(CnetAddr addr)
{
	char key[5];
	int2string(key, addr);
	return (ROUTING_ENTRY *) hashtable_find(routing_table, key, NULL);
}
	

void routing_init()
{
	int num_neighbours = link_num_links();
	neighbours = malloc(sizeof(*neighbours) * (num_neighbours + 1));
	
	for(int i=0; i<=num_neighbours; i++) {
		neighbours[i].nextSeqNum = 0;
		neighbours[i].nextAckNum = 0;
		//neighbours[i].inUnAckSeqNum = squeue_new();
		neighbours[i].outRoutingSegments = vector_new();
	}
		
	DISTANCE_INFO[1] distInfo;
	distInfo[0].weight = 0;
	distInfo[0].minMTU = INT_MAX;
	distInfo[0].destAddr = network_get_address();
	
	broadcast_distance_info(distInfo, sizeof(*distInfo));
}
