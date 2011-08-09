#ifndef BUFFER_H_
#define BUFFER_H_

typedef void * BUFFER;

BUFFER buffer_new(size_t len);

void buffer_free(BUFFER b);

void buffer_store(BUFFER b, size_t pos, char *data, size_t size);

void buffer_load(BUFFER b, size_t pos, char *data, size_t size);

bool buffer_check(BUFFER b, size_t pos);

bool buffer_check_range(BUFFER b, size_t pos, size_t len);

size_t buffer_next_invalid(BUFFER b, size_t pos);

#endif
