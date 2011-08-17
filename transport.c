/** Transport layer */

/* include headers */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cnet.h>
#include <cnetsupport.h>
#include "datatypes.h"
#include "network.h"
#include "transport.h"
#include "bitmap.h"
#include "buffer.h"
#include "squeue.h"

#define SEGMENT_SIZE 1024

#define MAX_WINDOW_SIZE 10

#define MAX_WINDOW_OFFSET (MAX_WINDOW_SIZE * SEGMENT_SIZE)

#define TRANSPORT_TIMEOUT 1000000

#define MAX_SEGMENT_OFFSET (UINT16_MAX >> 1)

#define TRANSPORT_BUFFER_SIZE MAX_SEGMENT_OFFSET

typedef struct
{
	BUFFER inBuf;
	SQUEUE lasts;
	size_t recAckOffset; // first byte of first incomplete message
	VECTOR outSegments;
	size_t numSentSegments;
	size_t windowSize;
	size_t nextOffset;
} CONNECTION;

typedef struct
{
	CnetTimerID timerId;
	CnetAddr addr;
	size_t size;
	SEGMENT *seg;
} OUT_SEGMENT;

// a new entry is added whenever message from/to previously unknown host arrives
HASHTABLE connections; // host address -> CONNECTION

CONNECTION *create_connection(CnetAddr addr)
{
	CONNECTION con;

	con.inBuf = buffer_new(TRANSPORT_BUFFER_SIZE);
	con.lasts = vector_new();
	con.recAckOffset = 0;
	con.outSegments = vector_new();
	con.numSentSegments = 0;
	con.windowSize = MAX_WINDOW_SIZE;
	con.nextOffset = 0;

	char key[5];
	int2string(key, addr);
	hashtable_add(connections, key, &con, sizeof(con));

	CONNECTION *ret = hashtable_find(connections, key, NULL);

	return ret;
}

/** Checks whether offset is affected by ackOffset */
bool compareToAck(size_t offset, size_t ackOffset, size_t range, size_t limit)
{
	return (offset < ackOffset && ackOffset - offset < range) ||
				 ((limit - offset) + ackOffset < range);
}

size_t marshal_segment(SEGMENT *seg, segment_header *header, char *payload, size_t size)
{
	seg->header.offset    = header->offset | header->isLast ? 1 << 15 : 0;
	seg->header.ackOffset = header->ackOffset;

	memcpy(seg->payload, payload, size);

	return size + sizeof(seg->header);
}

size_t unmarshal_segment(SEGMENT *seg, segment_header *header, char **payload, size_t size)
{
	header->isLast    = seg->header.offset & 1 << 15;
	header->offset    = seg->header.offset ^ header->isLast ? 1 << 15 : 0;
	header->ackOffset = seg->header.ackOffset;

	size_t payloadSize = size - sizeof(seg->header);
	*payload = seg->payload;

	return payloadSize;
}

/** Hands segment to network layer and starts timer */
void transmit_segment(OUT_SEGMENT *outSeg)
{
	network_transmit(outSeg->addr, (char *)outSeg->seg, outSeg->size);
	outSeg->timerId = CNET_start_timer(TRANSPORT_TIMER, TRANSPORT_TIMEOUT, (CnetData) outSeg);
}

void transmit_segments(CnetAddr addr)
{
	char key[5];
	int2string(key, addr);
	CONNECTION *con = hashtable_find(connections, key, NULL);
	assert(con != NULL);

  //window not saturated and segments available
	while (con->numSentSegments < con->windowSize &&
				 con->numSentSegments < vector_nitems(con->outSegments)) {
		size_t size;
		OUT_SEGMENT *outSeg = vector_peek(con->outSegments, con->numSentSegments, &size);
		transmit_segment(outSeg);
		con->numSentSegments++;
	}
	//TODO flow/congestion control
}

//takes one message, splits it into segments and adds them to outgoing queue and triggers sending of segments 
void transport_transmit(CnetAddr addr, char *data, size_t size)
{
	char key[5];
	int2string(key, addr);
	CONNECTION *con = hashtable_find(connections, key, NULL);

	if (con == NULL) {
		con = create_connection(addr);
	}

	SEGMENT *seg = malloc(sizeof(*seg));
	segment_header header;
	size_t remainingBytes = size;
	size_t processedBytes = 0;

	/* split message into several segments */
	for (int i = 0; remainingBytes > 0; i++) {
		assert(remainingBytes + processedBytes == size);
		size_t payloadSize = MIN(remainingBytes, SEGMENT_SIZE);
		header.offset    = con->nextOffset;
		header.ackOffset = con->recAckOffset;
		header.isLast    = remainingBytes == payloadSize;

		size_t segSize = marshal_segment(seg, &header, data + processedBytes, payloadSize);

		remainingBytes -= payloadSize;
		processedBytes += payloadSize;
		con->nextOffset  = (con->nextOffset + payloadSize) % MAX_SEGMENT_OFFSET;

		OUT_SEGMENT outSeg;
		outSeg.addr = addr;
		outSeg.seg = seg;
		outSeg.size = segSize;

		vector_append(con->outSegments, &outSeg, segSize);
	}

	transmit_segments(addr);
}

void transport_receive(CnetAddr addr, char *data, size_t size)
{
	char key[5];
	int2string(key, addr);
	CONNECTION *con = hashtable_find(connections, key, NULL);

	if (con == NULL) {
		con = create_connection(addr);
	}

	SEGMENT *segment = (SEGMENT *)data;
	segment_header header;
	char *payload;

	size_t payloadSize = unmarshal_segment(segment, &header, &payload, size);

	/* accumulate segments in buffer */
	buffer_store(con->inBuf, header.offset, payload, payloadSize);

	if (header.isLast) {
		size_t endOffset = (header.offset + payloadSize) % MAX_SEGMENT_OFFSET;
		squeue_insert(con->lasts, endOffset);
	}

	/* check if buffer contains complete messages -> forward to application */
	size_t bufferStart = con->recAckOffset;
	con->recAckOffset = buffer_next_invalid(con->inBuf, con->recAckOffset);
	size_t nextLast = squeue_peek(con->lasts);

	while (nextLast <= con->recAckOffset) {
		squeue_pop(con->lasts);
		char msg[MAX_MESSAGE_SIZE];
		size_t msgSize = nextLast - bufferStart;
		buffer_load(con->inBuf, bufferStart, msg, msgSize);
		CHECK(CNET_write_application(msg, &msgSize));
		bufferStart = nextLast + 1;
		nextLast = squeue_peek(con->lasts);
	}

	/* process acknowledgement */
	size_t segmentSize;
	OUT_SEGMENT *outSeg = vector_peek(con->outSegments, 0, &segmentSize);
	SEGMENT *seg = outSeg->seg;
	size_t endOffset = (seg->header.offset + segmentSize - sizeof(seg->header)) % MAX_SEGMENT_OFFSET;

	while (compareToAck(endOffset, header.ackOffset, MAX_WINDOW_OFFSET, MAX_SEGMENT_OFFSET)) {
		outSeg = vector_remove(con->outSegments, 0, NULL);
		CNET_stop_timer(outSeg->timerId);
		free(outSeg->seg);
		free(outSeg);
		con->numSentSegments--;

		outSeg = vector_peek(con->outSegments, 0, &segmentSize);
		seg = outSeg->seg;
		endOffset = seg->header.offset + segmentSize - sizeof(seg->header) % MAX_SEGMENT_OFFSET;
	}

	transmit_segments(addr);
}

void transport_init()
{
	connections = hashtable_new(0);
}
