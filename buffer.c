#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"

typedef struct _BUFFER
{
  size_t len;
  char *data;
} _BUFFER;

/**
 * Creates a new circular buffer and returns a handle for it.
 *
 * @param len The length of the buffer
 * @return Handle for the buffer
 */
BUFFER buffer_new(size_t len)
{
  _BUFFER *buf = malloc(sizeof(*buf));

  buf->len = len;
  buf->data = malloc(len);

  return (BUFFER) buf;
}

/**
 * Frees all resources allocated for given buffer.
 * The handle is invalid afterwards.
 *
 * @param b Handle of buffer to destroy
 */
void buffer_free(BUFFER b)
{
  _BUFFER *buf = (_BUFFER *)b;

  free(buf->data);
  free(buf);
}

/**
 * Stores data 'data' in buffer at position pos.
 *
 * @param b Handle of buffer
 * @param pos Position in buffer at which first byte of data is stored
 * @param data Data to store in buffer
 * @param size Size of data
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
}

/**
 * Loads data from buffer at position 'pos' into 'data'.
 *
 * @param b Handle of buffer
 * @param pos Position in buffer
 * @param data Array to which the data is loaded
 * @param size Size of data
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
}
