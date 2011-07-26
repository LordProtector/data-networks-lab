#include <cnet.h>

#define MAX_SEGMENT_SIZE  sizeof(SEGMENT)
#define MAX_DATAGRAM_SIZE sizeof(DATAGRAM)

/* Data structures for application layer */

typedef struct
{
  char[MAX_MESSAGE_SIZE] data;
  size_t size;
} MESSAGE;


/* Data structures for transport layer */

typedef struct
{
  uint16_t id;     // seqguence number of segment / also encodes is_first flag
  uint16_t ackid;  // sequence number of last continuosly received segment
} segment_header;

typedef struct
{
  segment_header header;
  char payload[MAX_MESSAGE_SIZE];
} SEGMENT;


/* Data structures for network layer. */

typedef struct
{
  uint8_t srcaddr;  // 0 - 255
  uint8_t destaddr;
  uint8_t hoplimit; // time to live + routing flag: 1 = routing protocol, 0 = network protocol
} datagram_header;

typedef struct 
{
  datagram_header header;
  char payload[MAX_SEGMENT_SIZE];
} DATAGRAM;


/* Data structure for link layer. */

typedef struct
{
  uint16_t size;     // length of payload, most significant bit is is_first flag
  uint16_t checksum; // checksum of header
  uint8_t  id;       // id of datagram
  uint8_t  ordering; // determines position of playload within datagram
} frame_header;

typedef struct
{
  frame_header header;
  char payload[MAX_DATAGRAM_SIZE];
} FRAME;
