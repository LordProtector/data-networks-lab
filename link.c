/** Link layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"


/* Constants */

/**
 * Muliply a second with this constant to get a microsecond.
 */
#define MICRO 0.000001

/**
 * Number of bits forming a byte.
 */
#define BYTE_LENGTH 8

/**
 * Link delay.
 */
#define LINK_DELAY 1

/**
 * Maximal length of queues.
 */
#define QUEUE_MAX_MSGS 20

/**
 * Minimal length of queues.
 */
#define QUEUE_MIN_MSGS 10

/**
 * Used for setting and querieing isLast flag of a frame
 */
#define IS_LAST 1 << 15


/* Macros */

/**
 * Computes the bigger of two numbers
 */
#define MIN(a,b) (a < b ? a : b);


/* Structs */

/**
 * Represents a link
 */
typedef struct
{
  bool     busy;     // is the link sending something?
  QUEUE    queue;    // link's output queue
  uint8_t  sendId;   // id of current datagram in sending process
  bool     corrupt;  // is current datagram corrupt
  uint8_t  recId;    // id of last received frame
  uint8_t  ordering; // expected ordering of next received frame
  DATAGRAM buffer;   // input buffer
  size_t   size;     // how far the buffer is filled
} link_t;

typedef unsigned char * buf_t;


/* Variables */

/**
 * Status and data of links of this host
 */
link_t *linkData;


/* Private functions */

/**
 * Returns the transmission delay.
 *
 * The transmission delay is the length of the message divided by the bandwidth
 * of the link.
 *
 * @param Length the length of the message.
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
void transmit_frame(int link)
{
  char *msg;
  size_t length;
  double timeout;

  if (queue_nitems(linkData[link].queue)) {
    msg = queue_peek(linkData[link].queue, &length);
    //~ if (queue_nitems(linkData[link].queues) <= QUEUE_MIN_MSGS) {
      //~ CNET_enable_application(ALLNODES);
    //~ }
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status == ER_TOOBUSY) {
      // If link is still busy wait another microsecond
      timeout = 1;
    } else {
      CHECK(ph_status);
      printf(" DATA transmitted: %d bytes\n", length);
      msg = queue_remove(linkData[link].queue, &length);
      free(msg);
      timeout = transmission_delay(length, link) + LINK_DELAY;
    }
    CNET_start_timer(EV_TIMER1, timeout, link);
    linkData[link].busy = true;
  } else {
    linkData[link].busy = false;
  }
}


/* API functions */

/**
 * Sends a datagram over a link.
 *
 * @param datagram Pointer to the data to send.
 * @param size Size of the datagram.
 * @param link The link to send messages over.
 */
void link_transmit(char *datagram, size_t size, int link)
{
  FRAME  frame;
  size_t remainingBytes = size;
  size_t processedBytes = 0;
  size_t maxPayloadSize = linkinfo[link].mtu - sizeof(frame_header);

  frame.header.id = linkData[link].sendId++;

  for (int i = 0; remainingBytes > 0; i++) {
    frame.header.size = MIN(remainingBytes, maxPayloadSize);
    size_t frameSize = sizeof(frame_header) + frame.header.size;
    if (remainingBytes < maxPayloadSize) {
      frame.header.size |= IS_LAST;
    }
    frame.header.ordering = i;
    frame.header.checksum = 0;
    frame.header.checksum = CNET_crc16((buf_t) &frame, frameSize);
    memcpy(frame.payload, datagram + processedBytes, frame.header.size);

    queue_add(linkData[link].queue, &frame, frameSize);

    remainingBytes -= frame.header.size;
    processedBytes += frame.header.size;
  }

  if (linkData[link].busy) {
    transmit_frame(link);
  }
}

/**
 * Takes a received frame and prepares a datagram for upper layer from it.
 *
 * @param data The received data.
 * @param size The size of the data.
 * @param link The link the data was received from.
 */
void link_receive(char *data, size_t size, int link)
{
  FRAME *frame = (FRAME *) data;
  uint16_t checksum = frame->header.checksum;
  frame->header.checksum = 0;

  if (CNET_crc16((buf_t) frame, size) != checksum) {
    linkData[link].corrupt = true;
    return;
  }

  if (frame->header.id == linkData[link].recId) {
    if (linkData[link].corrupt || frame->header.ordering != linkData[link].ordering) {
      linkData[link].corrupt = true;
      return;
    }
  } else {
    if (frame->header.ordering == 0) {
      linkData[link].corrupt = false;
      linkData[link].size = 0;
    } else {
      linkData[link].corrupt = true;
      return;
    }
    linkData[link].recId = frame->header.id;
  }

  linkData[link].ordering = frame->header.ordering + 1;
  linkData[link].size += frame->header.size;
  memcpy(&linkData[link].buffer + linkData[link].size, frame->payload, frame->header.size);

  if (frame->header.size & IS_LAST && !linkData[link].corrupt) {
    // TODO give datagram to upper layer
  }
}

/**
 * Initializes the link layer
 *
 * Must be called before the link layer can be used after reboot.
 */
void link_init()
{
  linkData = malloc((nodeinfo.nlinks + 1) * sizeof(*linkData));

  for (int i = 0; i <= nodeinfo.nlinks; i++) {
    linkData[i].busy     = false;
    linkData[i].queue    = queue_new();
    linkData[i].sendId   = 0;
    linkData[i].corrupt  = false;
    linkData[i].recId    = 0;
    linkData[i].ordering = 0;
    linkData[i].size     = 0;
  }
}
