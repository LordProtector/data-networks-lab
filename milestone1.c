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

//calcute how long a frame needs to travel over the link
double transmission_delay(size_t length, int link)
{
  double bandwidth = (double) linkinfo[link].bandwidth * MICRO;
  double transmission_delay = (double) (length * BYTE_LENGTH) / bandwidth;

  return transmission_delay;
}

void transmit(int	link)
{
  char *msg;
  size_t length;
  double timeout;
  
  if (queue_nitems(msgQ)) {
    msg = queue_peek(msgQ, &length);
    if (queue_nitems(msgQ) <= QUEUE_MIN_MSGS) {
      CNET_enable_application(ALLNODES);
    }
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status == ER_TOOBUSY) {
      // If link is still busy wait another microsecond
      timeout = 1.;
    } else {
      CHECK(ph_status);
      printf(" DATA transmitted: %d bytes\n", length);
      msg = queue_remove(msgQ, &length);
      free(msg);
      timeout = transmission_delay(length, link) + LINK_DELAY;
    }
    CNET_start_timer(EV_TIMER1, timeout, 0);
    timer_started = true;
  } else {
    timer_started = false;
  }
}

static EVENT_HANDLER(application_ready)
{
  CnetAddr destaddr;
  int link = 1;
  size_t length = sizeof(msg);

  CHECK(CNET_read_application(&destaddr, msg, &length));
  if (queue_nitems(msgQ) >= QUEUE_MAX_MSGS) {
    CNET_disable_application(ALLNODES);
  }
  queue_add(msgQ, msg, length);
  if (!timer_started) {
    transmit(link);
  }
}

static EVENT_HANDLER(link_ready)
{
  int link = 1;
  
  transmit(link);
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
