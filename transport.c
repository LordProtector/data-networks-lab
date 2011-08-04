/** Transport layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "network.h"
#include "transport.h"
#include "bitmap.h"
#include "buffer.h"

typedef struct
{
  BUFFER buf;
  BITMAP bm;
  VECTOR lasts;
  size_t ackOffset; // first byte of first incomplete message
  //TODO sender side
} CONNECTION;

// a new entry is added whenever message from/to previously unknown host arrives
HASHTABLE connections; // host address -> CONNECTION

void transport_transmit(CnetAddr addr, char *data, size_t size)
{
  network_transmit(addr, data, size);
  // TODO reliability, flow/congestion control, segmenatation
}

void transport_receive(CnetAddr addr, char *data, size_t size)
{
  CHECK(CNET_write_application(data, &size));
  // TODO reliability, desegmentatation
}

void transport_init()
{
  connections = hashtable_new(0);
  //TODO init
}
