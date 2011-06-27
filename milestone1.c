#include <cnet.h>
#include <stdlib.h>
#include <string.h>

char msg[MAX_MESSAGE_SIZE];

static EVENT_HANDLER(application_ready)
{
  CnetAddr destaddr;
  int	link = 1;

  size_t length = sizeof(msg);
  CHECK(CNET_read_application(&destaddr, msg, &length));

  CHECK(CNET_write_physical(link, msg, &length));
  printf(" DATA transmitted: '%s' (%d bytes)\n", msg, length);
}

static EVENT_HANDLER(physical_ready)
{
  size_t length;
  int link;

  length = sizeof(msg);
  CHECK(CNET_read_physical(&link, msg, &length));
  printf("\t\t\t\tDATA received: '%s' (%d bytes), ", msg, length);
  CHECK(CNET_write_application(msg, &length));
  printf("up to application\n");
}

EVENT_HANDLER(reboot_node)
{
    CHECK(CNET_set_handler( EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));

    if(nodeinfo.nodenumber == 0)
      CNET_enable_application(ALLNODES);
}
