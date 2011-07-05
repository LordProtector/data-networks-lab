struct message
{
  char* message;
  size_t size;
}

/* Data structures for transport layer */

struct segment_header
{
}

struct segment
{
  struct segment_header header;
}

/* Data structures for network layer. */
struct datagram_header
{
}

struct datagram 
{
  struct datagram_header header;
}

/* Data structure for link layer. */

struct frame_header
{

}

struct frame
{
  struct frame_header header;
}
