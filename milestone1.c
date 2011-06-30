#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>

#define MICRO            0.000001
#define BYTE_LENGTH      8
#define LINK_DELAY       1
#define QUEUE_MAX_MSGS  20
#define QUEUE_MIN_MSGS  10

QUEUE	msgQ;

char msg[MAX_MESSAGE_SIZE];

bool timer_started;

void set_timeout(size_t length)
{
  printf(" DATA transmitted: %d bytes\n", length);
  //calcute how long a frame needs to travel over the link
  double bandwidth = (double) linkinfo[1].bandwidth * MICRO;
  double transmission_delay = (double) (length * BYTE_LENGTH) / bandwidth; 
  double t = transmission_delay + LINK_DELAY;
  CNET_start_timer(EV_TIMER1, t, 0);
}

static EVENT_HANDLER(application_ready)
{
  int	link = 1;
  CnetAddr destaddr;
  size_t length = sizeof(msg);

  CHECK(CNET_read_application(&destaddr, msg, &length));
  if (queue_nitems(msgQ) >= QUEUE_MAX_MSGS) CNET_disable_application(ALLNODES);
  if (timer_started) {
    queue_add(msgQ, msg, length);
  } else {
    CHECK(CNET_write_physical(link, msg, &length));
    set_timeout(length);
    timer_started = true;
  }
}

static EVENT_HANDLER(link_ready)
{
  int	link = 1;
  size_t length;
  char *msg;

  if (queue_nitems(msgQ)) {
    msg = queue_peek(msgQ, &length);
    if (queue_nitems(msgQ) <= QUEUE_MIN_MSGS) CNET_enable_application(ALLNODES);
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status == ER_TOOBUSY) {
      CNET_start_timer(EV_TIMER1, 1, 0);
    } else {
      CHECK(ph_status);
      set_timeout(length);
      msg = queue_remove(msgQ, &length);
      free(msg);
    }
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
  printf("\t\t\t\tDATA received: %d bytes\n", length);
  CHECK(CNET_write_application(msg, &length));
}

EVENT_HANDLER(reboot_node)
{
    CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1,           link_ready, 0));
    
    CNET_enable_application(ALLNODES);

    msgQ = queue_new();
    timer_started = false;
}
