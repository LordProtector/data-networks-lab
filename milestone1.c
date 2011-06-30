#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>

QUEUE	msgQ;

char msg[MAX_MESSAGE_SIZE];

bool timer_started;

static EVENT_HANDLER(application_ready)
{
  int	link = 1;
  CnetAddr destaddr;

  size_t length = sizeof(msg);
  CHECK(CNET_read_application(&destaddr, msg, &length));
  printf(" DATA read from applicaiton: %d bytes\n", length);
  if (queue_nitems(msgQ) >= 20) CNET_disable_application(ALLNODES);
  if (timer_started) {
    queue_add(msgQ, msg, length);
  } else {
    CHECK(CNET_write_physical(link, msg, &length));
    double bandwidth = (double) linkinfo[link].bandwidth / 1000000.;
    double transmission_delay = (double) (length * 8) / bandwidth; 
    double t = transmission_delay + 1 /*linkinfo[link].propagationdelay*/;
    CNET_start_timer(EV_TIMER1, t, 0);
    timer_started = true;
  }
}

static EVENT_HANDLER(timeout)
{
  int	link = 1;
  size_t length;
  char *msg;

  if (queue_nitems(msgQ)) {
    msg = queue_remove(msgQ, &length);
    if (queue_nitems(msgQ) <= 10) CNET_enable_application(ALLNODES);
    CHECK(CNET_write_physical(link, msg, &length));
    //calcute how long a frame needs to travel over the link
    double bandwidth = (double) linkinfo[link].bandwidth / 1000000.;
    double transmission_delay = (double) (length * 8) / bandwidth; 
    double t = transmission_delay + 1 /*linkinfo[link].propagationdelay*/;
    //~ printf("propagationdelay: %d µs\n",linkinfo[link].propagationdelay);
    //~ printf("bandwidth: %f bit/µsec \n", bandwidth);
    //~ printf("length: %f bit\n", length * 8);
    //~ printf("transmission delay: %f\n", transmission_delay);
    //~ printf("timer: %f µsec\n", t);
    CNET_start_timer(EV_TIMER1, t, 0);
    free(msg);
  } else {
    timer_started = false;
  }
}

static EVENT_HANDLER(physical_ready)
{
  size_t length;
  int link;

  length = sizeof(msg);
  CHECK(CNET_read_physical(&link, msg, &length));
  printf("\t\t\t\t\tDATA received: %d bytes\n", length);
  CHECK(CNET_write_application(msg, &length));
}

EVENT_HANDLER(reboot_node)
{
    CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1,           timeout, 0));
    
    CNET_enable_application(ALLNODES);

    msgQ = queue_new();
    timer_started = false;
}
