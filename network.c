/** Network layer */

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

#define HOP_LIMIT 8

void int2string(char* s, int i);

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
	/* lookup link in forwarding_table */
	char key[5];
	int2string(key, addr);
	int link = *((int*) hashtable_find(forwarding_table, key, NULL)); 
  
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
		return; //hoplimit exceeded

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
		char key[5];
		int2string(key, destaddr);
		int link = *((int*) hashtable_find(forwarding_table, key, NULL)); 
  	
		datagram->header.hoplimit--;
		link_transmit(link, (char*) datagram, size);
	}
}

void network_init()
{
	forwarding_table = hashtable_new(0);

  #define SB  134
	#define HOM 96
	#define SLS 182

  
	/* static forwarding_table creation */
	char key[5];
	int* one = malloc(sizeof(one)); *one = 1;
	int* two = malloc(sizeof(two)); *two = 2;

	int2string(key, 111);
	
	switch(nodeinfo.address) {
	case SB:
		int2string(key, HOM);	hashtable_add(forwarding_table, key, one, sizeof(int));
		int2string(key, SLS);	hashtable_add(forwarding_table, key, two, sizeof(int));
		break;
	case HOM:
		int2string(key, SB);	hashtable_add(forwarding_table, key, one, sizeof(int));
		break;
	case SLS:
		int2string(key, SB);	hashtable_add(forwarding_table, key, one, sizeof(int));
		break;
	}
}

void int2string(char* s, int i) {
	sprintf(s, "%d", i);
}
