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
#include "dring.h"

#define SEGMENT_SIZE 1024

#define MAX_WINDOW_SIZE 10

#define MAX_WINDOW_OFFSET (MAX_WINDOW_SIZE * SEGMENT_SIZE)

#define TRANSPORT_TIMEOUT 1000000

#define MAX_SEGMENT_OFFSET (UINT16_MAX >> 1)

#define TRANSPORT_BUFFER_SIZE MAX_SEGMENT_OFFSET

typedef struct
{
  /* for receiving */
	BUFFER inBuf;
	DRING lasts;
	size_t bufferStart;  // first byte of first incomplete message

	/* for sending */
	VECTOR outSegments; // sent and queued segments without a received ACK
	size_t numSentSegments; // which of the outSegments have been send
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
	con.lasts = dring_new(MAX_WINDOW_OFFSET);
	con.bufferStart = 0;
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
bool acknowledged(size_t offset, size_t ackOffset)
{
	offset    %= MAX_SEGMENT_OFFSET;
	ackOffset %= MAX_SEGMENT_OFFSET;

	return (offset <= ackOffset && ackOffset - offset < MAX_WINDOW_OFFSET) ||
				 ((MAX_SEGMENT_OFFSET - offset) + ackOffset < MAX_WINDOW_OFFSET);
}

size_t marshal_segment(SEGMENT *seg, segment_header *header, char *payload, size_t size)
{
	seg->header.offset    = header->offset | (header->isLast ? (1 << 15) : 0);
	seg->header.ackOffset = header->ackOffset;

	memcpy(seg->payload, payload, size);

	return size + sizeof(seg->header);
}

size_t unmarshal_segment(SEGMENT *seg, segment_header *header, char **payload, size_t size)
{
	header->isLast    = seg->header.offset & (1 << 15);
	header->offset    = seg->header.offset ^ (header->isLast ? (1 << 15) : 0);
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
	size_t ackOffset   = buffer_next_invalid(con->inBuf, con->bufferStart);

	/* ignore duplicated segments */
	if (acknowledged(header.offset + payloadSize, ackOffset)) {
		return;
	}

	/* accumulate segments in buffer */
	buffer_store(con->inBuf, header.offset, payload, payloadSize);

	if (header.isLast) {
		size_t endOffset = (header.offset + payloadSize) % MAX_SEGMENT_OFFSET;
		dring_insert(con->lasts, endOffset);
	}

	/* check if buffer contains complete messages -> forward to application */
	ackOffset       = buffer_next_invalid(con->inBuf, con->bufferStart);
	size_t nextLast = dring_peek(con->lasts);

	while (nextLast != -1 && acknowledged(nextLast, ackOffset)) {
		dring_pop(con->lasts);
		char msg[MAX_MESSAGE_SIZE];
		size_t msgSize = nextLast - con->bufferStart;
		buffer_load(con->inBuf, con->bufferStart, msg, msgSize);
		CHECK(CNET_write_application(msg, &msgSize));

		con->bufferStart = nextLast;
		nextLast = dring_peek(con->lasts);
	}

	/* process acknowledgement */
	if(vector_nitems(con->outSegments) > 0) {
		size_t segmentSize;
		OUT_SEGMENT *outSeg = vector_peek(con->outSegments, 0, &segmentSize);
		SEGMENT *seg = outSeg->seg;
		size_t endOffset = seg->header.offset + segmentSize - sizeof(seg->header);
		endOffset %= MAX_SEGMENT_OFFSET;

		while (acknowledged(endOffset, header.ackOffset)) {
			outSeg = vector_remove(con->outSegments, 0, NULL);
			CNET_stop_timer(outSeg->timerId);
			free(outSeg->seg);
			free(outSeg);
			con->numSentSegments--;

			if(vector_nitems(con->outSegments) == 0) break; // no more elements available

			outSeg = vector_peek(con->outSegments, 0, &segmentSize);
			seg = outSeg->seg;
			endOffset  = seg->header.offset + segmentSize - sizeof(seg->header);
			endOffset %= MAX_SEGMENT_OFFSET;
		}
	}

	transmit_segments(addr);
}

void transport_init()
{
	connections = hashtable_new(0);
}
