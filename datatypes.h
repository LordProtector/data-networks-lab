#include <cnet.h>

#define MAX_SEGMENT_SIZE  sizeof(SEGMENT)
#define MAX_DATAGRAM_SIZE sizeof(DATAGRAM)

typedef struct
{
  char* message;
  size_t size;
} MESSAGE;

/* Data structures for transport layer */

struct segment_header
{
  unsigned int id;
  short ordering;
  bool isfirst;
  unsigned int ackid;
  short ackordering;  
};

typedef struct
{
  struct segment_header header;
  char payload[MAX_MESSAGE_SIZE];
} SEGMENT;

/* Data structures for network layer. */
struct datagram_header
{
  CnetAddr srcaddr;
  CnetAddr destaddr;
  char hoplimit;
  bool routing; //upper layer: 1 = routing protocol, 0 = network protocol
};

typedef struct 
{
  struct datagram_header header;
  char payload[MAX_SEGMENT_SIZE];
} DATAGRAM;

/* Data structure for link layer. */

typedef struct
{
  uint16_t length;   // length of payload, most significant bit is is_first flag
  uint16_t checksum; // checksum of header
  uint8_t  id;       // id of datagram
  uint8_t  ordering; // determines position of playload within datagram
} frame_header;

typedef struct
{
  frame_header header;
  char payload[MAX_DATAGRAM_SIZE];
} FRAME;
