/**
 * link.c
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of the link layer.
 *
 * A link must be initialized before usage by calling link_init().
 *
 * The link layer has the possibility to send data over a desired link.Thereby
 * it splits data into several frames if necessary.
 *
 * The link layer uses a queue to buffer frames to reduces the amount of time
 * being idle between the transmission of two frames.
 *
 * If a frame is received fully without errors it is handed over to the upper
 * layer. Corrupted frames are dropped.
 *
 * Error correction could be implemented here, but is actually not done,
 * because it causes more overhead than it can avoid.
 */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "link.h"
#include "network.h"


/**
 * output data of the queue length
 */
#define SHOW_QUEUE_LENGTH true


/* Constants */

/**
 * Multiply a second with this constant to get a microsecond.
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
 * Used for setting and querying isLast flag of a frame.
 */
#define IS_LAST (1 << 7)

/**
 * Size of an input buffer.
 */
#define BUFFER_SIZE MAX_DATAGRAM_SIZE

/**
 * The largest allowed id for frames.
 */
#define FRAME_ID_LIMIT (UINT8_MAX >> 1)

/**
 * Interval in which the load should be calculated
 * currently 10s
 */
#define INTERVALL_CALCULATE_LOAD 10000000


/* Structs */

/**
 * Represents a link.
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
  CnetTime busyTime;            // number of microseconds this link is busy
  CnetTime lastStatusChange;    // time busy status changed the last time
  size_t   sendBits;            // how many bits are send during the interval
  QUEUE    frameSizeCounter;    // stores the last sizes of frames being send
} link_t;

typedef unsigned char * buf_t;

typedef struct size_element_t
{
	CnetTime time;				// time point when element was send
	size_t   size;				// size of the element
	
} size_element_t;


/* Variables */

/**
 * Status and data of links of this host.
 */
link_t *linkData;


/* Private functions */

void add_load(int link, size_t size);


/**
 * Encodes payload, such that some error correction is possible.
 * Returns size of encoded payload.
 *
 * @param frame Frame where the encoded payload shall be placed.
 * @param payload The payload to encode.
 * @param size Size of the payload.
 * @return Size of encoded payload.
 */
size_t encode_payload(FRAME *frame, char *payload, size_t size)
{
  //This is the appropiate place to implement an error correction scheme.
  //We decided not to implement error correction for performance reasons.
  memcpy(frame->payload, payload, size);

  return size;
}


/**
 * Decodes payload from frame.
 * Returns size of encoded payload or 0 if correction fails.
 *
 * @param frame The frame from which the payload is to be decoded.
 * @param payload Position where the decoded payload is to be stored.
 * @param size Size of the decoded payload.
 * @return Size of decoded payload or 0 if decoding fails.
 */
size_t decode_payload(FRAME *frame, char *payload, size_t size)
{
  //We decided not to implement error correction for performance reasons.
  memcpy(payload, frame->payload, size);

  return size;
}


/**
 * Marshals frame for efficient transmission and encodes the payload for
 * error correction.
 * Adds computed checksum.
 *
 * @param header The header to encode.
 * @param frame The marshaled frame.
 * @param payload Payload for the frame.
 * @param size Size of payload.
 * @return Size of the frame.
 */
size_t marshal_frame(FRAME *frame, frame_header *header, char* payload, size_t size)
{
  frame->header.id_isLast = header->id;
  frame->header.ordering  = header->ordering;
  frame->header.checksum  = 0;
  size_t frameSize = encode_payload(frame, payload, size) + sizeof(marshaled_frame_header);

  if (header->isLast) {
    frame->header.id_isLast |= IS_LAST;
  }
  frame->header.checksum = CNET_crc16((buf_t) frame, frameSize);

  return frameSize;
}


/**
 * Unmarshals frame.
 * Checks checksum.
 *
 * @param header The unmarshaled header.
 * @param frame The frame from which the header is to be unmarshaled.
 * @param payload Where to store the frames decoded payload.
 * @param size Size of frame.
 * @return Size of payload or 0 in case of uncorrectable error.
 */
size_t unmarshal_frame(FRAME *frame, frame_header *header, char *payload, size_t size)
{
  header->id             = frame->header.id_isLast & (IS_LAST ^ UINT8_MAX);
  header->ordering       = frame->header.ordering;
  header->isLast         = frame->header.id_isLast & IS_LAST;
  uint16_t checksum      = frame->header.checksum;
  frame->header.checksum = 0;

  if (CNET_crc16((buf_t) frame, size) == checksum) {
    return decode_payload(frame, payload, size - sizeof(marshaled_frame_header));
  } else {
    return 0;
  }
}


/**
 * Returns the transmission delay.
 *
 * The transmission delay is the length of the message divided by the bandwidth
 * of the link.
 *
 * @param length The length of the message.
 * @param link The link to send the message over.
 * @return The calculated transmission delay.
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

  //are there data to send for the link?
  if (queue_nitems(linkData[link].queue)) {
    msg = queue_peek(linkData[link].queue, &length);
    int ph_status = CNET_write_physical(link, msg, &length);
    if (ph_status != 0 && (cnet_errno == ER_NOTREADY || cnet_errno == ER_TOOBUSY)) {
      // If link is still busy wait another microsecond
      timeout = 1;
    } else {
      CHECK(ph_status);
      //~ printf(" DATA transmitted: %d bytes\n", length);
      msg = queue_remove(linkData[link].queue, &length);	  
      free(msg);
      timeout = transmission_delay(length, link) + LINK_DELAY;

	  add_load(link, length * 8);
    }
    CNET_start_timer(LINK_TIMER, timeout, link);
    if (!linkData[link].busy) {
      linkData[link].busy = true;
      linkData[link].lastStatusChange = nodeinfo.time_in_usec;
      #if SHOW_QUEUE_LENGTH == true
      int utilization = 100 * linkData[link].busyTime / MAX(nodeinfo.time_in_usec, 1);
      printf("%lld: [utilization] %d for link %d\n ", nodeinfo.time_in_usec, utilization, link);
      #endif
    }
  } else {
    assert(linkData[link].busy);
    linkData[link].busy = false;
    linkData[link].busyTime += nodeinfo.time_in_usec - linkData[link].lastStatusChange;
    #if SHOW_QUEUE_LENGTH == true
    int utilization = 100 * linkData[link].busyTime / MAX(nodeinfo.time_in_usec, 1);
    printf("%lld: [utilization] %d for link %d\n ", nodeinfo.time_in_usec, utilization, link);
    #endif
  }

  #ifdef MILESTONE_2
  if (queue_nitems(linkData[link].queue) <= QUEUE_MIN_MSGS) {
    CNET_enable_application(ALLNODES);
  }
  #endif
}


/* API functions */

/**
 * Sends data over a link.
 * Splits data into several frames if necessary.
 *
 * @param data Pointer to the data to send.
 * @param size Size of the data.
 * @param link The link to send messages over.
 */
void link_transmit(int link, char *data, size_t size)
{
	/* avoid unlimited increase of output queue */
  if (queue_nitems(linkData[link].queue) >= 10000) {
    return;
  }

  link_get_load(link);
  FRAME frame;
  frame_header header;
  size_t remainingBytes = size;
  size_t processedBytes = 0;

  header.id = linkData[link].sendId++ % FRAME_ID_LIMIT;

  /* split datagram into several frames */
  for (int i = 0; remainingBytes > 0; i++) {
    size_t payloadSize = MIN(remainingBytes, linkData[link].maxPayloadSize);
    header.ordering = i;
    header.isLast   = remainingBytes == payloadSize;

    size_t frameSize = marshal_frame(&frame, &header, data + processedBytes, payloadSize);

    remainingBytes  -= payloadSize;
    processedBytes  += payloadSize;

    queue_add(linkData[link].queue, &frame, frameSize);
  }
#if SHOW_QUEUE_LENGTH == true
  printf("%lld: [queue_length]\t ", nodeinfo.time_in_usec);
  for(int i = 0; i < link_num_links()+1; i++){
	  printf("%d\t ", queue_nitems(linkData[i].queue));
  }
  printf("\n");
#endif

  #ifdef MILESTONE_2
  if (queue_nitems(linkData[link].queue) >= QUEUE_MAX_MSGS) {
    CNET_disable_application(ALLNODES);
  }
  #endif

	/* send frame when timer not running (initial sending) */
  if (!linkData[link].busy) {
    transmit_frame(link);
  }
}

/**
 * Takes a received frame and prepares a datagram for upper layer from it.
 * Only valid data are transmitted to upper layer. Thus, corruption becomes
 * frame loss.
 *
 * @param data The received data.
 * @param size The size of the data.
 * @param link The link the data was received from.
 */
void link_receive(int link, char *data, size_t size)
{
  FRAME *frame = (FRAME *) data;
  frame_header header;
  char payload[size];
  size_t payloadSize = unmarshal_frame(frame, &header, payload, size);

  //messages with zero length are corrupt
  if (!payloadSize) {
    linkData[link].corrupt = true;
    return;
  }

  if (header.id == linkData[link].recId) {
    //is the ordering the one I expect?
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

  assert(linkData[link].size + payloadSize <= BUFFER_SIZE);
  //store data in buffer for later usage
  memcpy(linkData[link].buffer + linkData[link].size, payload, payloadSize);
  //for the next frame we expect an incremented ordering number
  linkData[link].ordering = header.ordering + 1;
  linkData[link].size += payloadSize;

  //is the frame received fully without errors?
  if (header.isLast && !linkData[link].corrupt) {
    //send data to upper layer
    network_receive(link, linkData[link].buffer, linkData[link].size);
  }
}


/**
 * Removes old data from the queue of the frameSizeCounter which are older
 * than the limit in INTERVALL_CALCULATE_LOAD.
 * @param link The link to remove load for.
 */
void remove_load(int link)
{
	size_t len;
	size_element_t *tmp_element;
	size_t num_items = queue_nitems(linkData[link].frameSizeCounter);

	if (num_items) {
		tmp_element = queue_peek(linkData[link].frameSizeCounter, &len);

		//remove old data
		while(nodeinfo.time_in_usec - tmp_element->time > INTERVALL_CALCULATE_LOAD) {
			tmp_element = queue_remove(linkData[link].frameSizeCounter, &len);
			linkData[link].sendBits -= tmp_element->size;
			free(tmp_element);
			num_items--;
			if(num_items) {
				tmp_element = queue_peek(linkData[link].frameSizeCounter, &len);
			} else {
				break;
			}
		}
	}
}


/**
 * Adds load to the load queue. Should be called if data are transmitted over
 * a link.
 */
void add_load(int link, size_t size)
{
	size_element_t tmp_element;
	tmp_element.size = size;
	tmp_element.time = nodeinfo.time_in_usec;
	queue_add(linkData[link].frameSizeCounter, &tmp_element, sizeof(tmp_element));
	linkData[link].sendBits  += size;
	remove_load(link);
}


/**
 * Returns the load of the given link.
 * @param link Link to calculate the load for.
 */
float link_get_load(int link)
{
	//ensure that the calculations are done only for the latest data send
	remove_load(link);
	size_t bits_in_queue = 0; //TODO calculate #bits in queue
	size_t bits = linkData[link].sendBits + bits_in_queue;
	float time = INTERVALL_CALCULATE_LOAD;
	if(time > nodeinfo.time_in_usec) {
		time = nodeinfo.time_in_usec;
	}
	float load = ((float) bits / ((float) time * MICRO)) / (float) linkinfo[link].bandwidth;

	//fprintf(stderr, "Load at node %d of link %d is %f\n", nodeinfo.address, link, load);

	return load;
 
}


/**
 * Returns the bandwidth for the given link.
 *  @param link Link to get the bandwidth for.
 */
int link_get_bandwidth(int link)
{
	assert(link <= nodeinfo.nlinks);
	return linkinfo[link].bandwidth;
}


/**
 * Returns the MTU for the given link.
 * @param link Link to get the MTU for.
 */
int link_get_mtu(int link)
{
	assert(link <= nodeinfo.nlinks);
	return linkinfo[link].mtu;
}


/**
 * Returns the number of pending frames
 * on the given outgoing link.
 * @param link Link to get the queue size for.
 */
int link_get_queue_size(int link)
{
	assert(link <= nodeinfo.nlinks);
	return queue_nitems(linkData[link].queue);
}


/**
 * Returns the number of direct neighbours.
 */
int link_num_links()
{
	return nodeinfo.nlinks;
}


/**
 * Initializes the link layer.
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
    linkData[i].maxPayloadSize = linkinfo[i].mtu - sizeof(marshaled_frame_header);
    linkData[i].corrupt        = false;
    linkData[i].recId          = 0;
    linkData[i].ordering       = 0;
    linkData[i].size           = 0;
    linkData[i].busyTime       = 0;
    linkData[i].lastStatusChange = 0;
		linkData[i].frameSizeCounter = queue_new();
		linkData[i].sendBits       = 0;
  }
}
