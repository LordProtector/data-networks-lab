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

/**
 * Takes a segment, add datagram header and passes datagram
 * to the link layer.
 * @param addr destination address
 * @param data segment to send
 * @param size size of segment
 */
void network_transmit(CnetAddr addr, char *data, size_t size)
{

	int link =  network_lookup(addr);

	/* datagram header */
	datagram_header header;
	header.srcaddr = nodeinfo.address;
	header.destaddr = addr;
	header.hoplimit = HOP_LIMIT;
	header.routing = false;

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
		int link =network_lookup(destaddr);

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
 * Lookup link in forwarding_table and returns the coresponding link.
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