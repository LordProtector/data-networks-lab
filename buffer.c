/**
 * buffer.c
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of a  cyclic buffer.
 * 
 * Data are stored in a cyclic buffer using a char array for the data and a
 * bitmap which stores which of the data are currently valid. Data can be
 * stored given a starting position, the data and the length of the data and
 * also be loaded from the buffer.
 *
 * 
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "buffer.h"


/**
 * Data structure for the buffer.
 */
typedef struct _BUFFER
{
	size_t  len;     // Length of the buffer.
	char    *data;   // Stored data of the buffer.
	uint8_t *bitmap; // Stores which data are valid.
} _BUFFER;

void buffer_validate(BUFFER b, size_t pos);
void buffer_validate_range(BUFFER b, size_t pos, size_t len);
void buffer_invalidate(BUFFER b, size_t pos);
void buffer_invalidate_range(BUFFER b, size_t pos, size_t len);

/**
 * Creates a new circular buffer and returns a handle for it.
 *
 * @param len The length of the buffer.
 * @return Handle for the buffer.
 */
BUFFER buffer_new(size_t len)
{
  _BUFFER *buf = malloc(sizeof(*buf));

	buf->len    = len;
	buf->data   = malloc(len);
	buf->bitmap = calloc(len / 8 + 1, sizeof(*buf->bitmap));

  return (BUFFER) buf;
}

/**
 * Frees all resources allocated for given buffer.
 * The handle is invalid afterwards.
 *
 * @param b Handle of buffer to destroy.
 */
void buffer_free(BUFFER b)
{
  _BUFFER *buf = (_BUFFER *)b;

  free(buf->data);
	free(buf->bitmap);
  free(buf);
}

/**
 * Stores data 'data' in buffer at position pos.
 *
 * @param b Handle of buffer.
 * @param pos Position in buffer at which first byte of data is stored.
 * @param data Data to store in buffer.
 * @param size Size of data.
 */
void buffer_store(BUFFER b, size_t pos, char *data, size_t size)
{
  _BUFFER *buf = (_BUFFER *)b;

  pos %= buf->len;
  int wrapping = pos + size - buf->len;

  if (wrapping < 0) {
    memcpy(buf->data + pos, data, size);
  } else {
    memcpy(buf->data + pos, data, size - wrapping);
    memcpy(buf->data, data + size - wrapping, wrapping);
  }

	buffer_validate_range(b, pos, size);
}

/**
 * Loads data from buffer at position 'pos' into 'data'.
 *
 * @param b Handle of buffer.
 * @param pos Position in buffer.
 * @param data Array to which the data is loaded.
 * @param size Size of data.
 */
void buffer_load(BUFFER b, size_t pos, char *data, size_t size)
{
  _BUFFER *buf = (_BUFFER *)b;

  pos %= buf->len;
  int wrapping = pos + size - buf->len;

  if (wrapping < 0) {
    memcpy(data, buf->data + pos, size);
  } else {
    memcpy(data, buf->data + pos, size - wrapping);
    memcpy(data + size - wrapping, buf->data, wrapping);
  }

	buffer_invalidate_range(b, pos, size);
}

/**
 * Validates byte at position 'pos' of buffer 'b'.
 *
 * @param b Handle of buffer.
 * @param pos Position of bit to set.
 */
void buffer_validate(BUFFER b, size_t pos)
{
	_BUFFER *buf = (_BUFFER *)b;

	pos %= buf->len;
	size_t byte  = pos / 8;
	uint8_t mask = 1 << (pos % 8);

	//TODO what about receiving duplicated data
	assert(!(buf->bitmap[byte] & mask)); //do not overwrite valid data
	buf->bitmap[byte] |= mask;
}

/**
 * Validates 'len' bytes from position 'pos' of buffer 'b' on.
 *
 * @param b Handle of buffer.
 * @param pos Position of first byte to validate.
 * @param len Number of bytes to validate.
 */
void buffer_validate_range(BUFFER b, size_t pos, size_t len)
{
	size_t i;
	size_t end = pos + len;

	for (i = pos; i < end; i++) {
		buffer_validate(b, i);
	}
}

/**
 * Clears byte at position 'pos' of buffer 'b'.
 *
 * @param b Handle of buffer.
 * @param pos Position of byte to invalidate.
 */
void buffer_invalidate(BUFFER b, size_t pos)
{
	_BUFFER *buf = (_BUFFER *)b;

	pos %= buf->len;
	size_t byte  = pos / 8;
	uint8_t mask = 1 << (pos % 8);

	if (buf->bitmap[byte] | mask) {
		buf->bitmap[byte] ^= mask;
	}
}

/**
 * Invalidates 'len' bytes from position 'pos' of buffer 'b' on.
 *
 * @param b Handle of buffer.
 * @param pos Position of first bit to invalidate.
 * @param len Number of bytes to invalidate.
 */
void buffer_invalidate_range(BUFFER b, size_t pos, size_t len)
{
	size_t i;
	size_t end = pos + len;

	for (i = pos; i < end; i++) {
		buffer_invalidate(b, i);
	}
}

/**
 * Checks whether byte at position 'pos' of bitmap 'b' is valid.
 *
 * @param b Handle of buffer.
 * @param pos Position of byte to check.
 * @return Whether the requested byte is valid.
 */
bool buffer_check(BUFFER b, size_t pos)
{
	_BUFFER *buf = (_BUFFER *)b;

	pos %= buf->len;
	size_t byte  = pos / 8;
	uint8_t mask = 1 << (pos % 8);

	return buf->bitmap[byte] & mask;
}

/**
 * Checks whether 'len' continuous bytes from position 'pos' on are valid.
 *
 * @param b Handle of buffer.
 * @param pos Position of first byte to check.
 * @param len Number of bytes to check.
 * @return Whether bytes are valid.
 */
bool buffer_check_range(BUFFER b, size_t pos, size_t len)
{
	size_t i;
	size_t end = pos + len;

	for (i = pos; i < end; i++) {
		if (!buffer_check(b, i)) {
			return false;
		}
	}

	return true;
}

/**
 * Returns position of invalid unset byte, beginning at pos.
 * Returns "-1" if all bits are set.
 *
 * @param b Handle of buffer.
 * @param pos Position of first byte to check.
 * @return Position of first invalid byte, beginning at pos.
 */
size_t buffer_next_invalid(BUFFER b, size_t pos)
{
	_BUFFER *buf = (_BUFFER *)b;
	size_t i;
	size_t end = pos + buf->len;

	for (i = pos; i < end; i++) {
		if (!buffer_check(b, i)) {
		  return i % buf->len;
		}
	}

	return -1;
}
