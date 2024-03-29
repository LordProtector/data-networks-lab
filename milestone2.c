/**
 * milstone2.c
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of milstone 2 of the data networks lab 2011
 */

#define MILESTONE_2

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "network.h"
#include "link.c"

/**
 * Message of MAX_MESSAGE_SIZE.
 */
char msg[MAX_MESSAGE_SIZE];

/** Emulates a network layer for the link layer */
void network_receive(int link, char *data, size_t size)
{
	CHECK(CNET_write_application(data, &size));
}

/**
 * aplication_ready() event-handler.
 *
 * Handler is called when the application is ready to create a new message.
 * It reads a new message from the application and passes it to the transport layer.
 */
static EVENT_HANDLER(application_ready)
{
  CnetAddr destaddr;
  size_t length = sizeof(msg);

  CHECK(CNET_read_application(&destaddr, msg, &length));
  link_transmit(1, msg, length);
}

/**
 * physical_ready() event-handler.
 *
 * It is called when the physical layer is ready. It simply reads from the
 * physical layer and passes the message to the link layer.
 */
static EVENT_HANDLER(physical_ready)
{
  size_t length;
  int link;

  length = sizeof(msg);
  CHECK(CNET_read_physical(&link, msg, &length));
  link_receive(link, msg, length);
}

/**
 * link_ready() event-handler.
 *
 * It is called whenever a timeout indicating complete transmission of
 * a message occurs, which means the link is not busy any more.
 * It calls <code>transmit()</code>.
 */
static EVENT_HANDLER(link_ready)
{
  transmit_frame(data); // data = link (to send over)
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

	link_init();
	CNET_enable_application(ALLNODES);
}

