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
#include <limits.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"
#include "transport.h"


/**
 * Max number of hops a datagram is allowed to travel before it gets dropped.
 */
#define HOP_LIMIT 32

/**
 * An entry of the routing table
 */
typedef struct {
	int weight;			// weight to reach destination
	int minMTU;			// minimal MTU on path to destination
	int minBWD;			// minimal bandwidth on path to destination
} ROUTING_ENTRY;

void int2string(char* s, int i);
void routing_init();
void routing_receive(int link, char *data, size_t size);
ROUTING_ENTRY *routing_lookup(CnetAddr addr);
void update_forwarding_table(CnetAddr destAddr, int nextHop);

/**
 * Stores which route a packet should travel for a given destination.
 *
 * destination address -> next hop
 */
HASHTABLE forwarding_table;

/**
 * Stores the distance information for each destination relative to
 * all delivery of each direct neighbour.
 *
 * destination address, outgoing link -> ROUTING_ENTRY
 */
HASHTABLE routing_table;


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
 * Takes a segment and delivers it to addr.
 * @param addr destination address
 * @param data segment to send
 * @param size size of segment
 */
void network_transmit(CnetAddr addr, char *data, size_t size)
{
	int link = network_lookup(addr);
	transmit_datagram(link, false, addr, data, size);
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

	if (datagram->header.routing) {
		routing_receive(link, datagram->payload, size);
	}
	else if(nodeinfo.address == destaddr) {
		/* datagram destination = this node -> hand to upper layer */
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

	routing_init();
}

/**
 * Lookup link in forwarding_table and returns the corresponding link.
 *
 * @param addr The address to look up.
 * @return The link to send the data over.
 */
int network_lookup(CnetAddr addr)
{
	//~ ROUTING_ENTRY *entry = routing_lookup(addr);
	//~ assert(entry != NULL);
	//~ int bestChoice = 0, bestWeight = INT_MAX;
//~
	//~ for(int i = 1; i <= link_num_links(); i++) {
		//~ if(entry[i].weight < bestWeight) {
			//~ bestChoice = i;
			//~ bestWeight = entry[i].weight;
		//~ }
	//~ }
	//~ assert(bestWeight != INT_MAX);
//~
	//~ return bestChoice;

	char key[5];
	int2string(key, addr);
	return *((int *) hashtable_find(forwarding_table, key, NULL));
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

/**
 * Timeout in usec when a routing segment needs to be resend.
 */
#define ROUTING_TIMEOUT 100000


typedef struct
{
	VECTOR outRoutingSegments;		// Sent routing segements.
	size_t nextSeqNum;     		    // Sequence number for next routing segment.
	size_t nextAckNum;				// The next awaited sequence number. Initially it is 0.
} NEIGHBOUR;

typedef struct
{
	CnetTimerID timerId; 			// The ID of the timer to count the timeout.
	int link;       	 			// The destination link for the routing segment.
	size_t size;         			// Size of the out routing segment.
	ROUTING_SEGMENT *rSeg;			// Pointer to the routing segment.
} OUT_ROUTING_SEGMENT;

/**
 * All direct neigbours of this node.
 * For simplicity neighbours[0], refers to this node (just ignore it!).
 */
NEIGHBOUR *neighbours;

bool update_routing_table(int link, DISTANCE_INFO inDistInfo, DISTANCE_INFO *outDistInfo);
int get_weight(int link);

/**
 * Hands routing segment to the link layer (!) and starts timer.
 */
void transmit_routing_segment(OUT_ROUTING_SEGMENT *outSeg)
{
	outSeg->rSeg->header.ack_num = neighbours[outSeg->link].nextAckNum;
	//printf("transmit_routing_segment seq_num: %d ack_num: %d\n\n", outSeg->rSeg->header.seq_num, outSeg->rSeg->header.ack_num);
	transmit_datagram(outSeg->link, true, 0, (char *)outSeg->rSeg, outSeg->size);
	outSeg->timerId = CNET_start_timer(ROUTING_TIMER, ROUTING_TIMEOUT, (CnetData) outSeg);
}

/**
 * Packs distance information and an outgoing link
 * as a routing segment and delivers it.
 *
 * @param distance_info Distance information to send.
 * @param size Size of distance information.
 * @param link Link to send the distance information on.
 */
void transmit_distance_info(DISTANCE_INFO *distance_info, size_t size, int link)
{
	NEIGHBOUR *nb = &neighbours[link];

	ROUTING_SEGMENT *rSeg = malloc(sizeof(*rSeg));
	rSeg->header.seq_num = nb->nextSeqNum++;
	rSeg->header.ack_num = nb->nextAckNum;
	memcpy(rSeg->distance_info, distance_info, size);

	OUT_ROUTING_SEGMENT *outSeg = malloc(sizeof(*outSeg));
	outSeg->link = link;
	outSeg->rSeg = rSeg;
	outSeg->size = size + sizeof(routing_header);

	int pos = vector_nitems(nb->outRoutingSegments);
	vector_append(nb->outRoutingSegments, outSeg, sizeof(*outSeg));
	free(outSeg);
	outSeg = vector_peek(nb->outRoutingSegments, pos, NULL);

	transmit_routing_segment(outSeg);
}

/**
 * Broadcasts the given distance information to all neigbours.
 *
 * @param distance_info Distance information to send.
 * @param size Size of distance information.
 */
void broadcast_distance_info(DISTANCE_INFO *distance_info, size_t size)
{
	int num_neighbours = link_num_links();

	for(int i=1; i<=num_neighbours; i++) {
		transmit_distance_info(distance_info, size, i);
	}
}

/**
 * Acknowledges received distance info
 *
 * @param link Link to send the acknowledgement on.
 */
void transmit_distance_ack(int link)
{
	NEIGHBOUR *nb = &neighbours[link];

	ROUTING_SEGMENT rSeg;
	rSeg.header.seq_num = 0;
	rSeg.header.ack_num = nb->nextAckNum;

	transmit_datagram(link, true, 0, (char *)&rSeg, sizeof(routing_header));
}

/**
 * Receive a routing segment.
 *
 * Reads the routing information from the routing segment,
 * changes the routing table accordingly and potentially broadcasts
 * changes in forwarding decisions.
 * Drops all out of order routing segments (relies on ordered resend).
 * @param link Link which received the routing segment.
 * @param data The received routing segment.
 * @param size Size of the routing segment.
 */
void routing_receive(int link, char *data, size_t size)
{
	ROUTING_SEGMENT *rSeg = (ROUTING_SEGMENT *)data;
	NEIGHBOUR *nb = &neighbours[link];

	/* process acknowledgement */
	for (OUT_ROUTING_SEGMENT *ackSeg = vector_peek(nb->outRoutingSegments, 0, NULL);
			 vector_nitems(nb->outRoutingSegments)
				&& ackSeg->rSeg->header.seq_num < rSeg->header.ack_num;
			 ackSeg = vector_peek(nb->outRoutingSegments, 0, NULL))
	{
		vector_remove(nb->outRoutingSegments, 0, NULL);
		CNET_stop_timer(ackSeg->timerId);
		free(ackSeg->rSeg);
		free(ackSeg);
	}

	int distInfoLength = (size - sizeof(routing_header)) / sizeof(DISTANCE_INFO);

	if(nb->nextAckNum == rSeg->header.seq_num) { // in order
		/* process routing information */
		int updates = 0;
		DISTANCE_INFO sendDistInfo[distInfoLength];
		nb->nextAckNum++;

		for(int i=0; i<distInfoLength; i++) {
			DISTANCE_INFO distInfo = rSeg->distance_info[i];
			if(distInfo.destAddr != nodeinfo.address
				&& update_routing_table(link, distInfo, &(sendDistInfo[updates]))) {
				updates++;
			}
		}

		/* broadcast distance updates */
		if(updates > 0) {
			broadcast_distance_info(sendDistInfo, updates * sizeof(DISTANCE_INFO));
		} else {
			transmit_distance_ack(link);
		}
	} else if (distInfoLength != 0) { // out of order, no ack
		transmit_distance_ack(link);
	}
}

/**
 * Updates the routing table with the given distance information
 * and generates own new routing information to broadcast it to the
 * neighbours if the update led to changes in the forward decision.
 * Returns whether the update led to changes in the forward decision.
 * @param link Link that received the incoming distance information.
 * @param inDistInfo Incoming distance information.
 * @param outDistInfo Outgoing distance information.
 */
bool update_routing_table(int link, DISTANCE_INFO inDistInfo, DISTANCE_INFO *outDistInfo)
{
	ROUTING_ENTRY *entry = routing_lookup(inDistInfo.destAddr);
	int bestChoice = 0, bestWeight = INT_MAX;
	if(NULL == entry) {
		/* new routing table entry */
		char key[5];
		int2string(key, inDistInfo.destAddr);
		ROUTING_ENTRY newEntry[link_num_links() + 1];
		for(int i = 0; i <= link_num_links(); i++) {
			newEntry[i].weight = INT_MAX;
			newEntry[i].minMTU = INT_MAX;
			newEntry[i].minBWD = INT_MAX;
		}
		hashtable_add(routing_table, key, newEntry, sizeof(newEntry));
		entry = routing_lookup(inDistInfo.destAddr);
		bestChoice = link;
	}
	else {
		for(int i = 1; i <= link_num_links(); i++) {
			if(entry[i].weight < bestWeight) {
				bestChoice = i;
				bestWeight = entry[i].weight;
			}
		}
	}

	assert(NULL != entry);

	/* update routing table */
	entry[link].weight = inDistInfo.weight + get_weight(link);
	entry[link].minMTU = MIN(inDistInfo.minMTU, link_get_mtu(link));
	entry[link].minBWD = MIN(inDistInfo.minBWD, link_get_bandwidth(link));

	/* Did the update led to changes in the forward decision? */
	bool bestChoiceChanged = (bestChoice == link);
	if(bestChoiceChanged) {
		outDistInfo->destAddr = inDistInfo.destAddr;
		outDistInfo->weight = entry[link].weight;
		outDistInfo->minMTU = entry[link].minMTU;
		outDistInfo->minBWD = entry[link].minBWD;

		/* update forwarding table */
		update_forwarding_table(inDistInfo.destAddr, bestChoice);
	}

	/* enable message delivery to that node */
	CNET_enable_application(inDistInfo.destAddr);

// 	printf("Routing table updated on node %d for destination %d\n", nodeinfo.address, inDistInfo.destAddr);
// 	for(int i = 1; i <= link_num_links(); i++) {
// 		printf("weight %d minBWD: %d ", entry[i].weight, entry[i].minBWD);
// 	}
// 	puts("");puts("");

	return bestChoiceChanged;
}

/**
 * Updates an entry in the forwarding table
 * @param destAddr destination address (key of entry to get changed)
 * @param nextHop next hop in path to destination
 */
void update_forwarding_table(CnetAddr destAddr, int nextHop)
{
	char key[5];
	int2string(key, destAddr);
	hashtable_remove(forwarding_table, key, NULL);
	hashtable_add(forwarding_table, key, &nextHop, sizeof(nextHop));
}


/**
 * Looks up an address in the routing table.
 * @param addr Address to look up.
 */
ROUTING_ENTRY *routing_lookup(CnetAddr addr)
{
	char key[5];
	int2string(key, addr);
	return (ROUTING_ENTRY *) hashtable_find(routing_table, key, NULL);
}


/**
 * Calculates costs for transmitting data over given link.
 *
 * @param link Link.
 */
int get_weight(int link)
{
	//return 10000000 * 1.0 / link_get_bandwidth(link);
	double base = (100000. / link_get_bandwidth(link) - 5.);
	double weight = 10. * (-0.04 * (base * base * base) + 6.);

	return weight;
}



/**
 * Initializes the routing algorithm.
 *
 * Must be called before the routing algorithm can be used after reboot.
 */
void routing_init()
{
	routing_table = hashtable_new(0);

	/* initialize data structures */
	int num_neighbours = link_num_links();
	neighbours = malloc(sizeof(*neighbours) * (num_neighbours + 1));

	for(int i=0; i<=num_neighbours; i++) {
		neighbours[i].nextSeqNum = 0;
		neighbours[i].nextAckNum = 0;
		neighbours[i].outRoutingSegments = vector_new();
	}

	/* distribute initial distance information */
	DISTANCE_INFO distInfo[1];
	distInfo[0].weight = 0;
	distInfo[0].minMTU = INT_MAX;
	distInfo[0].minBWD = INT_MAX;
	distInfo[0].destAddr = network_get_address();
	

	broadcast_distance_info(distInfo, sizeof(distInfo));
}
