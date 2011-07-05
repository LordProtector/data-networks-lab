/**
 * milstone1.c
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of milstone 1 of the data networks lab 2011
 */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>

/**
 * Muliply a second with this constant to get a microsecond.
 */
#define MICRO            0.000001

/**
 * Number of bits forming a byte.
 */
#define BYTE_LENGTH      8

/**
 * Link delay.
 */
#define LINK_DELAY       1

/**
 * Maximal lenght of queues.
 */
#define QUEUE_MAX_MSGS  20

/**
 * Minimal lenght of queues.
 */
#define QUEUE_MIN_MSGS  10

/**
 * Queue for messages.
 */
QUEUE msgQ;

/**
 * Message of MAX_MESSAGE_SIZE.
 */
char msg[MAX_MESSAGE_SIZE];

/**
 * Is the timer started or not.
 */
bool timer_started;

/**
 * Returns the transmission delay.
 *
 * The transmission delay is the lenght of the message divided by the bandwidth
 * of the link.
 *
 * @param Length the lenght of the message.
 * @param Link the link to send the message over.
 * @return Returns the calculated transmission delay.
 */
double transmission_delay(size_t length, int link)
{
  double bandwidth = (double) linkinfo[link].bandwidth * MICRO;
  double transmission_delay = (double) (length * BYTE_LENGTH) / bandwidth;

  return transmission_delay;
}

/**
 * Writes a message on a physical link if possible.
 *
 * Sends the first message of the queue over link <code>link</code> if the
 * physical layer is not busy and starts the timer <code> EV_TIMER1</code>.
 * If the link is still busy the timer is started for a short time to wait
 * until the link is ready again.
 * Additionally the application is enabled if the queue has free space.
 *
 * @param link The link to send messages over.
 */
void transmit(int link)
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

/**
 * aplication_ready() event-handler.
 *
 * Handler is called when the application is ready to create a new message.
 * It reads a new message from the application and adds it to the queue. If the
 * queue is full the application is disabled. If no message is under transmission
 * (timer is not started) <code>transmit()</code> is called to transmit the next
 * message.
 */
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

/**
 * link_ready() event-handler.
 *
 * It is a timer which counts how long a message needs to be transmitted over
 * a link and then calls the <code>transmit()</code> methode to send the next
 * message.
 */
static EVENT_HANDLER(link_ready)
{
  int link = 1;
  
  transmit(link);
}

/**
 * physical_ready() event-handler.
 *
 * It is called when the physical layer is ready. It simply reads from the
 * physical layer and passes the message to the application.
 */
static EVENT_HANDLER(physical_ready)
{
  size_t length;
  int link;

  length = sizeof(msg);
  CHECK(CNET_read_physical(&link, msg, &length));
  printf("\t\t\t\tDATA received: %d bytes\n", length);
  CHECK(CNET_write_application(msg, &length));
}

/**
 * reboot_node() event-handler.
 *
 * It is always the first function to be called by the cnet schedule.
 * Handlers are set, application is enabled.
 */
EVENT_HANDLER(reboot_node)
{
    CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1,           link_ready, 0));
    
    CNET_enable_application(ALLNODES);

    msgQ = queue_new();
    timer_started = false;
}
