#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <cnet.h>

#define MAX_SEGMENT_SIZE  sizeof(SEGMENT)
#define MAX_DATAGRAM_SIZE sizeof(DATAGRAM)

/**
 * Maximal number of direct neighbours
 */
#define MAX_NEIGHBOURS 100

#define LINK_TIMER EV_TIMER1
#define TRANSPORT_TIMER EV_TIMER2
#define ROUTING_TIMER EV_TIMER3
#define GEARING_TIMER EV_TIMER4

/**
 * Computes the smaller of two numbers
 */
#define MIN(a,b) (a < b ? a : b);

#define MAX(a,b) (a > b ? a : b);

/* Data structures for application layer */

typedef struct
{
  char data[MAX_MESSAGE_SIZE];
  size_t size;
} MESSAGE;


/* Data structures for transport layer */

typedef struct
{
  uint32_t offset;     // sequence number of segment
  uint32_t ackOffset;  // sequence number last continuously received segment + 1
  bool     isLast;     // last segment of a message
} segment_header;

typedef struct
{
  uint32_t offset;     // sequence number of segment + isLast
  uint32_t ackOffset;  // sequence number last continuously received segment + 1
} marshaled_segment_header;

typedef struct
{
  marshaled_segment_header header;
  char payload[MAX_MESSAGE_SIZE];
} SEGMENT;


/* Data structures for network layer. */

typedef struct
{
  uint8_t srcaddr;  // 0 - 255
  uint8_t destaddr;
  uint8_t hoplimit; // time to live
  bool    routing;	// 1 = routing protocol, 0 = network protocol
} datagram_header;

typedef struct
{
  datagram_header header;
  char payload[MAX_SEGMENT_SIZE];
} DATAGRAM;


/* Data structures for routing. */

typedef struct
{
  uint16_t seq_num;     // sequence number of routing segment
  uint16_t ack_num;     // sequence number last received routing segment + 1
} routing_header;

typedef struct {
	CnetAddr destAddr;	// destination address
	int weight;			// weight to reach destination
	int minMTU;			// minimal MTU on path to destination
	int minBWD;			// minimal bandwidth on path to destination
} DISTANCE_INFO;

typedef struct
{
 routing_header header;
  DISTANCE_INFO distance_info[MAX_NEIGHBOURS];
} ROUTING_SEGMENT;


/* Data structures for link layer. */

typedef struct
{
  uint8_t  id;       // id of datagram
  uint8_t  ordering; // determines position of payload within datagram
  bool     isLast;   // is this the last for this id
} frame_header;

typedef struct
{
  uint16_t checksum;  // checksum of header
  uint8_t  id_isLast; // id of datagram, most significant bit is is_last flag
  uint8_t  ordering;  // determines position of payload within datagram
} marshaled_frame_header;

typedef struct
{
  marshaled_frame_header header;
  char payload[MAX_DATAGRAM_SIZE];
} FRAME;

void int2string(char* s, int i);

#endif
