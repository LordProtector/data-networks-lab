struct message
{
  char* message;
  size_t size;
}

/* Data structures for transport layer */

struct segment_header
{
  unsigned int id;
  short ordering;
  bool isfirst;
  unsigned int ackid;
  short ackordering;  
}

struct segment
{
  struct segment_header header;
  char* payload;
}

/* Data structures for network layer. */
struct datagram_header
{
  CnetAddr srcaddr;
  CnetAddr destaddr;
  char hoplimit;
  bool routing; //upper layer: 1 = routing protocol, 0 = network protocol
}

struct datagram 
{
  struct datagram_header header;
  char* payload;
}

/* Data structure for link layer. */

struct frame_header
{
  size_t length;
  int checksum;
  unsigned int id;
  short ordering;
  bool isfirst;
}

struct frame
{
  struct frame_header header;
  char* payload;
}
