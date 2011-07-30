/** Transport layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"
#include "transport.h"

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
  //TODO init
}
