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
  printf(" DATA read from applicaiton: '%s' (%d bytes)\n", msg, length);
  CNET_disable_application(destaddr);
  printf("propagationdelay: %d µs\n",linkinfo[link].propagationdelay);
  CHECK(CNET_write_physical(link, msg, &length));
  //calcute how long a frame needs to travel over the link
  double bandwidth = (double) linkinfo[link].bandwidth / 1000000.;
  double a = (double)(length*8.) / bandwidth; 
  double t = a + linkinfo[link].propagationdelay;
  printf("bandwidth: %f bit/µsec \n", bandwidth);
  printf("length: %f bit\n", length*8.);
  printf("a: %f\n", a);
  printf("timer: %f µsec\n", t);
  CNET_start_timer(EV_TIMER1, t, 0);
}

static EVENT_HANDLER(timeout)
{	
  printf("timeout\n");
  CNET_enable_application(ALLNODES);
}

static EVENT_HANDLER(physical_ready)
{
  size_t length;
  int link;

  length = sizeof(msg);
  CHECK(CNET_read_physical(&link, msg, &length));
  CNET_enable_application(ALLNODES);
  //printf("\t\t\t\tDATA received: '%s' (%d bytes), ", msg, length);
  CHECK(CNET_write_application(msg, &length));
  //printf("up to application\n");
}

EVENT_HANDLER(reboot_node)
{
    CHECK(CNET_set_handler( EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));

    if(nodeinfo.nodenumber == 0)
      CNET_enable_application(ALLNODES);

    CHECK(CNET_set_handler(EV_TIMER1, timeout, 0));
}
