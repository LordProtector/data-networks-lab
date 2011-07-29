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
 * Maximal length of queues.
 */
#define QUEUE_MAX_MSGS  20

/**
 * Minimal length of queues.
 */
#define QUEUE_MIN_MSGS  10

/**
 * Used for setting or querieing isFirst flag of a frame
 */
#define IS_LAST 1 << 15


/* Macros */

/**
 * Computes the bigger of two numbers
 */
#define MIN(a,b) (a < b ? a : b);


/* Structs */

/**
 * Represents an ouput link
 */
typedef struct
{
  bool    timerStarted;
  QUEUE   queue;
  uint8_t curId;
} out_link;

/**
 * Represents an input link
 */
typedef struct
{
  bool     datagramCorrupt;
  uint8_t  curId;
  uint8_t  ordering;
  DATAGRAM buffer;
  size_t   size;
} in_link;


/* Variables */

/**
 * Is the timer started or not.
 */
bool *timerStarted;

/**
 * Array of output queues (one per link)
 */
QUEUE *linkQueues;

/**
 * Array of datagram-ids currently transmitting (one per link)
 */
uint8_t *sendId;

/**
 * Array of datagram-ids currently receiving (one per link)
 */
uint8_t *recId;

/**
 * Indicates per link whether the datagram the last received frame belongs to is corrupted
 */
bool *datagramCorrupt;

/**
 * Buffers for frames belonging to same datagram (one per link)
 */
DATAGRAM *buffers;

/**
 * Store how far the buffers are filled
 */
size_t *bufferSizes;

/**
 * Ordering of the next expected frame (per link)
 */
uint8_t *orderings;

/**
 * Ouput links of this host
 */
out_link *outLinks;

/**
 * Input links of this host
 */
in_link *inLinks;


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

  if (queue_nitems(linkQueues[link])) {
    msg = queue_peek(linkQueues[link], &length);
    if (queue_nitems(linkQueues[link]) <= QUEUE_MIN_MSGS) {
      CNET_enable_application(ALLNODES);
    }
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status == ER_TOOBUSY) {
      // If link is still busy wait another microsecond
      timeout = 1;
    } else {
      CHECK(ph_status);
      printf(" DATA transmitted: %d bytes\n", length);
      msg = queue_remove(linkQueues[link], &length);
      free(msg);
      timeout = transmission_delay(length, link) + LINK_DELAY;
    }
    CNET_start_timer(EV_TIMER1, timeout, link);
    timerStarted[link] = true;
  } else {
    timerStarted[link] = false;
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

  frame.header.id = sendId[link]++;

  for (int i = 0; remainingBytes > 0; i++) {
    frame.header.size = MIN(remainingBytes, maxPayloadSize);
    if (remainingBytes < maxPayloadSize) {
      frame.header.size |= IS_LAST;
    }
    frame.header.ordering = i;
    frame.header.checksum = 0;
    frame.header.checksum = CNET_crc16((unsigned char *) &frame, sizeof(frame_header) + frame.header.size);
    memcpy(frame.payload, datagram + processedBytes, frame.header.size);

    queue_add(linkQueues[link], &frame, frame.header.size + sizeof(frame_header));

    remainingBytes -= frame.header.size;
    processedBytes += frame.header.size;
  }

  if (!timerStarted[link]) {
    transmit_frame(link);
  }
}

/**
 *
 *
 * @param data The received data.
 * @param size The size of the data.
 * @param link The link the data was received from.
 */
void link_receive(char *data, size_t size, int link)
{
  FRAME *frame = (FRAME *) data;

  if (frame->header.id == recId[link]) {
    if (datagramCorrupt[link] || frame->header.ordering != orderings[link]) {
      datagramCorrupt[link] = true;
      return;
    }
  } else {
    if (frame->header.ordering == 0) {
      datagramCorrupt[link] = false;
      bufferSizes[link] = 0;
      orderings[link] = 0;
    } else {
      datagramCorrupt[link] = true;
      return;
    }
    recId[link] = frame->header.id;
  }
  uint16_t checksum = frame->header.checksum;
  frame->header.checksum = 0;

  if (CNET_crc16((unsigned char *) frame, sizeof(frame_header) + frame->header.size) == checksum) {
    orderings[link] = frame->header.ordering;
    memcpy(&buffers[link] + bufferSizes[link], frame->payload, frame->header.size);
    bufferSizes[link] += frame->header.size;
  } else {
    datagramCorrupt[link] = true;
    return;
  }
  if (frame->header.size & IS_LAST && !datagramCorrupt[link]) {
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
  linkQueues      = malloc((nodeinfo.nlinks + 1) * sizeof(*linkQueues));
  datagramCorrupt = malloc((nodeinfo.nlinks + 1) * sizeof(*datagramCorrupt));
  sendId          = malloc((nodeinfo.nlinks + 1) * sizeof(*sendId));
  recId           = malloc((nodeinfo.nlinks + 1) * sizeof(*recId));
  timerStarted    = malloc((nodeinfo.nlinks + 1) * sizeof(*timerStarted));
  buffers         = malloc((nodeinfo.nlinks + 1) * sizeof(DATAGRAM));
  bufferSizes     = malloc((nodeinfo.nlinks + 1) * sizeof(*bufferSizes));
  orderings       = malloc((nodeinfo.nlinks + 1) * sizeof(*orderings));

  for (int i = 0; i <= nodeinfo.nlinks; i++) {
    linkQueues[i]      = queue_new();
    datagramCorrupt[i] = false;
    sendId[i]          = 0;
    recId[i]           = 0;
    timerStarted[i]    = false;
    bufferSizes[i]     = 0;
    orderings[i]       = 0;
  }
}
