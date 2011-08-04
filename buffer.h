#ifndef BUFFER_H_
#define BUFFER_H_

typedef void * BUFFER;

BUFFER buffer_new(size_t len);

void buffer_free(BUFFER b);

void buffer_store(BUFFER b, size_t pos, char *data, size_t size);

void buffer_load(BUFFER b, size_t pos, char *data, size_t size);

#endif
