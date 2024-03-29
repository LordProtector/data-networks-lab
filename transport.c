/**
 * transport.c
 *
 * @authors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of the transport layer.
 * 
 * The transport layer segments messages and streams them to the receiver.
 * It implements reliable data transfer using cumulative acknowledgments
 * and resending of unacknowledged segments.
 * 
 * For flow control a sliding window of maximal sendable segments is 
 * maintained. The max window size depends on the number of open
 * connections. The Reno algorithm is used for adapting the window size
 * to the current network congestion.
 *
 */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "network.h"
#include "transport.h"
#include "buffer.h"
#include "dring.h"


/**
 * Size of a segment in byte.
 */
#define SEGMENT_SIZE 1024

/**
 * Maximal number of segments in storage and under transmission.
 */

#define MAX_WINDOW_SIZE 32

/**
 * Maximal offset of the window in byte.
 */
#define MAX_WINDOW_OFFSET (MAX_WINDOW_SIZE * SEGMENT_SIZE)

/**
 * Timeout in usec when a segment needs to be resend.
 */
#define TRANSPORT_TIMEOUT 1000000

/**
 * Maximal offset of the segment in byte (UINT15).
 */
#define MAX_SEGMENT_OFFSET (1 << 18)

/**
 * Maximal offset of the segment in byte (UINT15).
 */
#define TRANSPORT_BUFFER_SIZE MAX_SEGMENT_OFFSET

/**
 * Minimum time before we send another ACK.
 */
#define ACK_TIME 10000

/**
 * If true, newly created segments are transmitted asynchronously,
 * to not overwhelm the lower layers.
 */
#define USE_GEARING true

/**
 * If true, segments are explicitly acknowledged, when it is unlikely,
 * that a segment, which could piggyback it, is send in the other direction, soon.
 */
#define EXPLICIT_ACK true

/**
 * If true, the first sent segment still waiting for acknowledgment
 * is retransmitted after receiving three times the same acknowledgment.
 */
#define USE_RENO true


/**
 * Data structure for a connection.
 */
typedef struct
{
  /* for receiving */
	BUFFER inBuf;           // Buffer for incoming data.
	DRING lasts;            // Stores the cyclic sequence numbers which are received.
	size_t bufferStart;     // First byte of first incomplete message.

	/* for sending */
	VECTOR outSegments;     // Sent and queued segments without a received ACK.
	size_t numSentSegments; // Which of the outSegments have been send (which is the next segment to send).
	size_t windowSize;      // Size of the window.
	size_t threshold;       // When to stop the slow start phase
	size_t windowLimit;	    // Maximal size of window
	size_t nextOffset;      // The next offset the connection will send if the window moves. Initially it is 0.

	CnetAddr addr;          // Address of the connected node
	CnetTime estimatedRTT;  // Estimated round time trip (RTT).
	CnetTime deviation;	    // Safety margin for the variation in estimatedRTT.
	CnetTime lastSendAck;   // Time the last acknowledgment was transmitted
	int ackCounter;	        // Congestion control: counts duplicated ACKs
	size_t lastAckOffset;   // Congestion control: stores the last ACK received
} CONNECTION;


/**
 * Data structure for one out segment. Out segments are segments which are not
 * acknowledged. These segments are not send or waiting for acknowledgment
 * (waiting in queue).
 */
typedef struct
{
	CnetTime sendTime;        // Time when the segment was send last.
	CnetTimerID timerId;      // The ID of the timer to count the timeout.
	CnetAddr addr;            // The destination address of the segment.
	size_t size;              // Size of the out segment
	SEGMENT *seg;             // Pointer to the data stored in the segment.
	int timesSend;						// number of times this segment was already transmitted
	uint32_t offset;					// offset of the segments payload
} OUT_SEGMENT;


/**
 * Stores the connections a host holds.
 * A new entry is added whenever message from/to previously unknown host arrives.
 * 
 * host address -> CONNECTION
 */
HASHTABLE connections;


CONNECTION* get_connection(CnetAddr addr);
CnetTime get_timeout(CONNECTION *con);


/**
 * Updates window limit for the given connection
 * dependent on the number of open connections and the connections bandwidth.
 * 
 * @param con The connection for which the window limit should be updated.
 */
void update_window_limit(CONNECTION *con)
{
	int maxWindow = MAX_WINDOW_SIZE;
	
	con->windowLimit = ((maxWindow - hashtable_nitems(connections))
											* network_get_bandwidth(con->addr)) / 10000000;
	con->windowLimit = MIN(con->windowLimit, maxWindow);  // limit windowLimit
	con->windowLimit = MAX(con->windowLimit, 1);          // ensure window limit is >0
}


/**
 * Creates a new connection for the given address. A connection contains
 * a buffer for incoming data and data managing the buffered data (storage of
 * the sequence numbers).
 *
 * It also stores data for sending. It queues segments which are not send, need
 * to be resend because the segments gets lost, or waiting for an acknowledgment.
 *
 * Old connection data for a given address are overwritten.
 *
 * @param addr The address to create a connection for.
 * @return The created connection.
 */
CONNECTION *create_connection(CnetAddr addr)
{
	CONNECTION con;

	con.inBuf = buffer_new(TRANSPORT_BUFFER_SIZE);
	con.lasts = dring_new(MAX_WINDOW_OFFSET);
	con.bufferStart = 0;
	con.outSegments = vector_new();
	con.numSentSegments = 0;
	con.windowSize = 1;
	con.threshold = 8;
	con.nextOffset = 0;
	con.addr = addr;
	con.estimatedRTT = TRANSPORT_TIMEOUT;
	con.deviation = TRANSPORT_TIMEOUT;
	con.lastSendAck = 0;
	con.ackCounter = 0;
	con.lastAckOffset = 0;

	char key[5];
	int2string(key, addr);
	assert(!hashtable_find(connections, key, NULL));
	hashtable_add(connections, key, &con, sizeof(con));

	CONNECTION *ret = hashtable_find(connections, key, NULL);

	return ret;
}


/**
 * Lookup address in the connections table.
 * If it does not exists a new entry is created.
 *
 * @param addr The address to look up.
 * @return The corresponding CONNECTION from the connection table.
 */
CONNECTION* get_connection(CnetAddr addr)
{
	char key[5];
	int2string(key, addr);
	CONNECTION *con = hashtable_find(connections, key, NULL);

	if (con == NULL) {
		con = create_connection(addr);
	}

	return con;
}


/**
 * Checks whether offset is affected by ackOffset.
 * Returns true if offset has already been acknowledged.
 *
 * @param offset Offset to test for.
 * @param ackOffset First invalid offset.
 * @return Returns true if offset has already been acknowledged.
 */
bool acknowledged(size_t offset, size_t ackOffset)
{
	offset    %= MAX_SEGMENT_OFFSET;
	ackOffset %= MAX_SEGMENT_OFFSET;

	return (offset <= ackOffset && ackOffset - offset <= MAX_WINDOW_OFFSET) ||
				 ((MAX_SEGMENT_OFFSET - offset) + ackOffset <= MAX_WINDOW_OFFSET);
}


/**
 * Updates the estimatedRTT and the Deviation value
 * of the given connection with the given sampleRTT.
 * The calculation to estimate the RTT is based on
 * exponential weighted moving average.
 * 
 * @param con The connection for that RTT should be updated.
 * @param sampleRTT New measured RTT.
 */
void update_rtt(CONNECTION *con, CnetTime sampleRTT)
{
	double x = 0.125;
	double y = 0.25;

	if (con->estimatedRTT != TRANSPORT_TIMEOUT) {
		con->estimatedRTT = (1-x) * con->estimatedRTT + x * sampleRTT;
		con->deviation =    (1-y) * con->deviation    + y * abs(sampleRTT - con->estimatedRTT);
	} else {
		con->estimatedRTT = sampleRTT;
	}
	#if LOGGING == true
		printf("%lld: [update_rtt] to_node: %d sampleRTT: %d new_estRTT: %d new_dev: %d timeout: %d\n",
					 nodeinfo.time_in_usec, con->addr, sampleRTT, con->estimatedRTT, con->deviation, get_timeout(con));
	#endif
}


/**
 * Returns and proper timeout value for the given connection.
 * 
 * @param con The connection, the timeout should be computed for.
 * @return An adequate timeout for the connection.
 */
CnetTime get_timeout(CONNECTION *con)
{
	return con->estimatedRTT + 4 * con->deviation;
}


/**
 * Calculates the distance between startOffset and endOffset. 
 * This corresponds to the length of the message.
 *
 * @param startOffset Start offset for the calculation.
 * @param endOffset End offset of the calculation.
 * @return Distance between start and end offset.
 */
size_t distance(size_t startOffset, size_t endOffset)
{
	if (endOffset > startOffset) {
		return endOffset - startOffset;
	} else {
		return (MAX_SEGMENT_OFFSET - startOffset) + endOffset;
	}
}


/**
 * Marshals segment for efficient transmission. 
 * Segment needs to be unmarshaled before usage.
 *
 * @param seg Marshaled segment.
 * @param header Header of the segment.
 * @param payload Payload of the segment.
 * @param size Size of the payload.
 * @return Size of the marshaled segment.
 */
size_t marshal_segment(SEGMENT *seg, segment_header *header, char *payload, size_t size)
{
	/* encode isLast in offset */
	seg->header.offset    = header->offset | (header->isLast ? MAX_SEGMENT_OFFSET : 0);
	seg->header.ackOffset = header->ackOffset;

	memcpy(seg->payload, payload, size);

	return size + sizeof(seg->header);
}


/**
 * Unmarshals segment.
 *
 * @param seg Segment to unmarshal.
 * @param header Unmarshaled header.
 * @param payload Unmarshald payload.
 * @param size Size of the unmarshaled payload.
 */
size_t unmarshal_segment(SEGMENT *seg, segment_header *header, char **payload, size_t size)
{
	/* decode isLast from offset */
	header->isLast    = seg->header.offset & MAX_SEGMENT_OFFSET;
	header->offset    = seg->header.offset ^ (header->isLast ? MAX_SEGMENT_OFFSET : 0);
	header->ackOffset = seg->header.ackOffset;

	size_t payloadSize = size - sizeof(seg->header);
	*payload = seg->payload;

	return payloadSize;
}


/**
 * Sends an empty segment over the given connection
 * in order to explicitly acknowledge received segments.
 * 
 * @param con The connection to send the data over.
 */
void transmit_ack(CONNECTION *con)
{
	SEGMENT *seg = malloc(sizeof(marshaled_segment_header));
	segment_header header;

	header.offset    = con->nextOffset - 1;
	header.ackOffset = buffer_next_invalid(con->inBuf, con->bufferStart);
	header.isLast    = true;
	#if LOGGING == true
		printf("%lld: [send_not_piggybacked_ack] to_node: %d\n", nodeinfo.time_in_usec, con->addr);
	#endif

	size_t segSize = marshal_segment(seg, &header, NULL, 0);
	network_transmit(con->addr, (char *)seg, segSize);
	con->lastSendAck = nodeinfo.time_in_usec;
	free(seg);
}


/**
 * Hands segment to network layer and starts timer.
 *
 * @param outSeg Segment to hand over to network layer.
 */
void transmit_segment(OUT_SEGMENT *outSeg)
{
	CONNECTION *con = get_connection(outSeg->addr);

	/* Congestion control */
	if (outSeg->timesSend > 1 && con->windowSize > 1) {
		con->threshold = con->windowSize / 2;
		con->windowSize = 1;
	}
	
	OUT_SEGMENT *winSeg = vector_peek(con->outSegments, con->windowSize, NULL);

	if (winSeg == NULL || acknowledged(outSeg->offset, winSeg->offset)) {
		#if LOGGING == true
		printf("%lld: [transmit_segment] to_node: %d threshold: %d \
						window_size: %d numOutSeg: %d numSentSegments %d\n",
						nodeinfo.time_in_usec, outSeg->addr, con->threshold,
						con->windowSize, vector_nitems(con->outSegments), con->numSentSegments);
		#endif

		outSeg->timesSend++;
		outSeg->seg->header.ackOffset = buffer_next_invalid(con->inBuf, con->bufferStart);
		network_transmit(outSeg->addr, (char *)outSeg->seg, outSeg->size);
		CnetTime timeout = outSeg->timesSend * get_timeout(con);
		outSeg->timerId = CNET_start_timer(TRANSPORT_TIMER, timeout, (CnetData) outSeg);
		con->lastSendAck = nodeinfo.time_in_usec;
	} else {
		outSeg->timerId = -1;
		con->numSentSegments--;
	}
}


/**
 * Transmits segments to address 'addr'
 * if window is not saturated and segments are available.
 *
 * @param addr Address to transmit segments to.
 */
void transmit_segments(CnetAddr addr)
{
	CONNECTION *con = get_connection(addr);
	int timeout = 1;

	/* window not saturated and segments available */
	for (int i = 0; i < con->windowSize && i < vector_nitems(con->outSegments); i++) {
		OUT_SEGMENT *outSeg = vector_peek(con->outSegments, i, NULL);
		if (outSeg->timerId == -1) {
			if (USE_GEARING) {
				outSeg->timerId = CNET_start_timer(GEARING_TIMER, timeout, (CnetData) outSeg);
			} else {
				transmit_segment(outSeg);
			}
			con->numSentSegments++;
			outSeg->sendTime = nodeinfo.time_in_usec;
			timeout += 500;
		}
	}
}


/**
 * Transmits a message.
 *
 * A new connection is created if it is the first to send to or receive from
 * 'addr'. If necessary the message is split into several parts (size is given
 * by SEGMENT_SIZE) which are added to the outgoing queue.
 * Finally it triggers the sending of segments.
 *
 * @param addr Address to send the message to.
 * @param data Data to send to address 'addr'.
 * @param size Size of the message.
 */
void transport_transmit(CnetAddr addr, char *data, size_t size)
{
	CONNECTION *con = get_connection(addr);
	update_window_limit(con);

	size_t remainingBytes = size;
	size_t processedBytes = 0;

	/* split message into several segments */
	for (int i = 0; remainingBytes > 0; i++) {
		SEGMENT *seg = malloc(sizeof(*seg));
		segment_header header;

		assert(remainingBytes + processedBytes == size);
		size_t payloadSize = MIN(remainingBytes, SEGMENT_SIZE);
		header.offset    = con->nextOffset;
		header.ackOffset = buffer_next_invalid(con->inBuf, con->bufferStart);
		header.isLast    = remainingBytes == payloadSize;

		size_t segSize = marshal_segment(seg, &header, data + processedBytes, payloadSize);

		remainingBytes -= payloadSize;
		processedBytes += payloadSize;
		con->nextOffset  = (con->nextOffset + payloadSize) % MAX_SEGMENT_OFFSET;

		OUT_SEGMENT outSeg;
		outSeg.addr = addr;
		outSeg.seg = seg;
		outSeg.size = segSize;
		outSeg.timesSend = 0;
		outSeg.timerId = -1;
		outSeg.offset = header.offset;

		/* add created segment to vector of sendable segments */
		vector_append(con->outSegments, &outSeg, sizeof(outSeg));
	}

	/* stop the application if list of outsegments exceeds threshold */
	if (vector_nitems(con->outSegments) >= con->windowSize) {
		#if LOGGING == true
			printf("%lld: [disable_application_window_saturated] to_node: %d\n", nodeinfo.time_in_usec, addr);
		#endif
		CNET_disable_application(addr);
	}

	transmit_segments(addr);
}


/**
 * Receive a message.
 *
 * A new connection is created if it is the first to send to or receive from 'addr'.
 * Received data are stored in a buffer if they are not already acknowledged.
 * If the buffer contains complete messages,
 * they are forwarded to the application and deleted from the buffer.
 *
 * Finally it triggers the sending of segments.
 *
 * @param addr Source address.
 * @param data Data to receive.
 * @param size Size of the data to receive.
 */
void transport_receive(CnetAddr addr, char *data, size_t size)
{
	CONNECTION *con = get_connection(addr);

	#if LOGGING == true
	printf("%lld: [receive_segment] from_node: %d threshold: %d \
					window_size: %d numOutSeg: %d numSentSegments: %d\n",
					nodeinfo.time_in_usec, addr, con->threshold, con->windowSize,
					vector_nitems(con->outSegments), con->numSentSegments);
	#endif

	SEGMENT *segment = (SEGMENT *)data;
	segment_header header;
	char *payload;

	int numSentSegments = con->numSentSegments;
	size_t payloadSize = unmarshal_segment(segment, &header, &payload, size);
	size_t ackOffset   = buffer_next_invalid(con->inBuf, con->bufferStart); //the offset the node is waiting for

	/* congestion control ala Reno */
#if USE_RENO == true
	if(header.ackOffset == con->lastAckOffset) {
		if(payloadSize == 0){ // only increase if a non piggybacked ACK is received
			con->ackCounter++;
		}
	} else {
		con->ackCounter = 0;
		con->lastAckOffset = header.ackOffset;
	}

	/* calculate if fast recovery phase should be started */
	if (con->ackCounter > 3 && payloadSize != 0) {
		con->ackCounter = 0;
		if(con->windowSize > 1) {
			con->threshold  = con->windowSize / 2;
			con->windowSize = con->threshold;
		}
		#if LOGGING == true
		printf("%lld: [Reno_3_dup_ack] to_node: %d threshold: %d window_size: %d numOutSeg: %d\n",
					 nodeinfo.time_in_usec, con->addr, con->threshold, con->windowSize, vector_nitems(con->outSegments));
		#endif

		/* perform fast retransmit */
		if(vector_nitems(con->outSegments) > 0) {
			size_t segmentSize;
			OUT_SEGMENT *outSeg = vector_peek(con->outSegments, 0, &segmentSize);
			if(outSeg->timerId != -1){
				CHECK(CNET_stop_timer(outSeg->timerId));
			}
			transmit_segment(outSeg);
		}
	}
#endif

	/* ignore duplicated segments */
	if (!acknowledged(header.offset + payloadSize, ackOffset) &&
			!buffer_check(con->inBuf, header.offset) && payloadSize > 0)
	{
		/* accumulate segments in buffer */
		buffer_store(con->inBuf, header.offset, payload, payloadSize);

		if (header.isLast) { //store endOffset if segment is the last one
			size_t endOffset = (header.offset + payloadSize) % MAX_SEGMENT_OFFSET;
			dring_insert(con->lasts, endOffset);
		}

		/* check if buffer contains complete messages -> forward to application */
		ackOffset       = buffer_next_invalid(con->inBuf, con->bufferStart);
		size_t nextLast = dring_peek(con->lasts);

		while (nextLast != -1 && acknowledged(nextLast, ackOffset)) {
			dring_pop(con->lasts);
			char msg[MAX_MESSAGE_SIZE];
			size_t msgSize = distance(con->bufferStart, nextLast);
			buffer_load(con->inBuf, con->bufferStart, msg, msgSize);
			CHECK(CNET_write_application(msg, &msgSize));

			con->bufferStart = nextLast;
			nextLast = dring_peek(con->lasts);
		}
	}

	/* process acknowledgment */
	if(vector_nitems(con->outSegments) > 0) {
		OUT_SEGMENT *outSeg = vector_peek(con->outSegments, 0, NULL);

		SEGMENT *seg = outSeg->seg;
		size_t endOffset = outSeg->offset + (outSeg->size - sizeof(seg->header));
		endOffset %= MAX_SEGMENT_OFFSET;
		assert(acknowledged(outSeg->offset, header.ackOffset));

		/* Remove all acknowledged segments from output buffer */
		while (acknowledged(endOffset, header.ackOffset)) {
			outSeg = vector_remove(con->outSegments, 0, NULL);
			CnetTime sampleRTT = nodeinfo.time_in_usec - outSeg->sendTime;
			update_rtt(con, sampleRTT);
			if (outSeg->timerId != -1) {
				CHECK(CNET_stop_timer(outSeg->timerId));
				con->numSentSegments--;
			}
			free(outSeg->seg);
			free(outSeg);

			if(vector_nitems(con->outSegments) == 0) break; // no more elements available

			outSeg = vector_peek(con->outSegments, 0, NULL);
			seg = outSeg->seg;
			endOffset  = outSeg->offset + (outSeg->size - sizeof(seg->header));
			endOffset %= MAX_SEGMENT_OFFSET;

			/* Congestion control */
			if (con->windowSize < con->threshold) { // slow start
				con->windowSize = MIN(2 * con->windowSize, con->windowLimit);
			} 
			else if (con->windowSize < con->windowLimit) { // congestion avoidance
				con->windowSize++;
			}
		}
		numSentSegments = con->numSentSegments;

		if (vector_nitems(con->outSegments) < con->windowSize) {
			#if LOGGING == true
			printf("%lld: [enable_application_window_unsaturated] to_node: %d\n", nodeinfo.time_in_usec, addr);
			#endif
			CNET_enable_application(addr);
		}

		transmit_segments(addr);
	}

	#if EXPLICIT_ACK == true
	/* In case piggybacking ACK is not possible, send it directly */
	if (payloadSize != 0 && numSentSegments == con->numSentSegments && 
			nodeinfo.time_in_usec - con->lastSendAck > ACK_TIME)
	{
		transmit_ack(con);
	}
	#endif
}


/**
 * Initializes the transport layer.
 *
 * Must be called before the transport layer can be used after reboot.
 */
void transport_init()
{
	connections = hashtable_new(0);
}
