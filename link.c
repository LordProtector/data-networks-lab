/** Link layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"
#include "transport.h"


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
#define QUEUE_MIN_MSGS (QUEUE_MAX_MSGS / 2)

/**
 * Used for setting and querieing isLast flag of a frame
 */
#define IS_LAST (1 << 7)

/**
 * Size of an input buffer
 */
#define BUFFER_SIZE MAX_DATAGRAM_SIZE

#define FRAME_ID_LIMIT (UINT8_MAX >> 2)


/* Macros */

/**
 * Computes the bigger of two numbers
 */
#define MIN(a,b) (a < b ? a : b);


/* Structs */

/**
 * Represents a link
 */
typedef struct link_t
{
  bool     busy;                // is the link sending something?
  QUEUE    queue;               // link's output queue
  uint8_t  sendId;              // id of current datagram in sending process
  size_t   maxPayloadSize;      // the maximum payload sendable in one frame
  bool     corrupt;             // is current datagram corrupt
  uint8_t  recId;               // id of last received frame
  uint8_t  ordering;            // expected ordering of next received frame
  char     buffer[BUFFER_SIZE]; // input buffer
  size_t   size;                // how far the buffer is filled
} link_t;

typedef unsigned char * buf_t;


/* Variables */

/**
 * Status and data of links of this host
 */
link_t *linkData;


/* Private functions */

/**
 * Encodes frame header for effcient transmission.
 * Adds computed checksum.
 *
 * @param header The header to encode
 * @param frame The frame for which the header is to be encoded
 * @return The complete size of the frame
 */
void marshal_frame_header(frame_header_simple *header, FRAME *frame, size_t size)
{
  frame->header.id       = header->id;
  frame->header.ordering = header->ordering;
  frame->header.checksum = 0;

  if (header->isLast) {
    frame->header.id |= IS_LAST;
  }
	//TODO: compute checksum on header only, if error correction is implemented
  frame->header.checksum = CNET_crc16((buf_t) frame, size);
}

/**
 * Decodes frame header.
 * Checks checksum. 
 *
 * @param header The decoded header
 * @param frame The frame from which the header is to be decoded
 * @return Whether decoding was successful
 */
bool unmarshal_frame_header(frame_header_simple *header, FRAME *frame, size_t size)
{
  header->id             = frame->header.id & (IS_LAST ^ UINT8_MAX);
  header->ordering       = frame->header.ordering;
  header->isLast         = frame->header.id & IS_LAST;
  uint16_t checksum      = frame->header.checksum;
  frame->header.checksum = 0;

  return CNET_crc16((buf_t) frame, size) == checksum;
}

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
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status == ER_TOOBUSY) {
      // If link is still busy wait another microsecond
      timeout = 1;
    } else {
      CHECK(ph_status);
      //~ printf(" DATA transmitted: %d bytes\n", length);
      msg = queue_remove(linkData[link].queue, &length);
      free(msg);
      timeout = transmission_delay(length, link) + LINK_DELAY;
    }
    CNET_start_timer(EV_TIMER1, timeout, link);
    linkData[link].busy = true;
  } else {
    linkData[link].busy = false;
  }

  // FIXME should be controlled in transport layer
  if (queue_nitems(linkData[link].queue) <= QUEUE_MIN_MSGS) {
    CNET_enable_application(ALLNODES);
  }
}


/* API functions */

/**
 * Sends data over a link.
 * Splits data into several frames if neccessary.
 *
 * @param data Pointer to the data to send.
 * @param size Size of the data.
 * @param link The link to send messages over.
 */
void link_transmit(int link, char *data, size_t size)
{
  FRAME  frame;
  frame_header_simple header;
  size_t remainingBytes = size;
  size_t processedBytes = 0;

  header.id = linkData[link].sendId++ % FRAME_ID_LIMIT;

  /* split datagram into several frames */
  for (int i = 0; remainingBytes > 0; i++) {
    size_t payloadSize = MIN(remainingBytes, linkData[link].maxPayloadSize);
    header.ordering = i;
    memcpy(frame.payload, data + processedBytes, payloadSize);

    remainingBytes  -= payloadSize;
    processedBytes  += payloadSize;
    size_t frameSize = payloadSize + sizeof(frame_header);

    header.isLast = !remainingBytes;

    marshal_frame_header(&header, &frame, frameSize);

    queue_add(linkData[link].queue, &frame, frameSize);
  }

  // FIXME should be controlled in transport layer
  if (queue_nitems(linkData[link].queue) >= QUEUE_MAX_MSGS) {
    CNET_disable_application(ALLNODES);
  }

	/* send frame when timer not running (initial sending) */
  if (!linkData[link].busy) {
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
void link_receive(int link, char *data, size_t size)
{
  FRAME *frame = (FRAME *) data;
  frame_header_simple header;

  if (!unmarshal_frame_header(&header, frame, size)) {
    linkData[link].corrupt = true;
    return;
  }

  if (header.id == linkData[link].recId) {
    if (linkData[link].corrupt || header.ordering != linkData[link].ordering) {
      linkData[link].corrupt = true;
      return;
    }
  } else { //new datagram
    linkData[link].recId = header.id;

		/* first frame has ordering = 0 */
    if (header.ordering == 0) {
      linkData[link].corrupt = false;
      linkData[link].size = 0;
    } else {
      linkData[link].corrupt = true;
      return;
    }
  }

  size_t payloadSize = size - sizeof(frame_header);
	assert(linkData[link].size + payloadSize <= BUFFER_SIZE);
  memcpy(linkData[link].buffer + linkData[link].size, frame->payload, payloadSize);
  linkData[link].ordering = header.ordering + 1;
  linkData[link].size += payloadSize;

  if (header.isLast && !linkData[link].corrupt) {
    network_receive(link, linkData[link].buffer, linkData[link].size);
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
    linkData[i].busy           = false;
    linkData[i].queue          = queue_new();
    linkData[i].sendId         = 0;
    linkData[i].maxPayloadSize = linkinfo[i].mtu - sizeof(frame_header);
    linkData[i].corrupt        = false;
    linkData[i].recId          = 0;
    linkData[i].ordering       = 0;
    linkData[i].size           = 0;
  }
}
