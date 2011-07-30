/** Network layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"
#include "transport.h"

void network_transmit(CnetAddr addr, char *data, size_t size)
{
  link_transmit(1, data, size);
  // TODO routing
}

void network_receive(int link, char *data, size_t size)
{
  transport_receive(1, data, size);
  // TODO forwarding
}

void network_init()
{
  //TODO init
}
