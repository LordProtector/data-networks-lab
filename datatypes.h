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
  char* payload;
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
  char* payload;
} DATAGRAM;

/* Data structure for link layer. */

struct frame_header
{
  size_t length;
  int checksum;
  unsigned int id;
  short ordering;
  bool isfirst;
};

typedef struct
{
  struct frame_header header;
  char* payload;
} FRAME;
