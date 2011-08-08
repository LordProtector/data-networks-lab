#ifndef BITMAP_H_
#define BITMAP_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef void * BITMAP;

BITMAP bitmap_new(size_t len);

void bitmap_free(BITMAP b);

void bitmap_set(BITMAP b, size_t pos);

void bitmap_set_range(BITMAP b, size_t pos, size_t len);

void bitmap_clear(BITMAP b, size_t pos);

void bitmap_clear_range(BITMAP b, size_t pos, size_t len);

void bitmap_flip(BITMAP b, size_t pos);

void bitmap_flip_range(BITMAP b, size_t pos, size_t len);

bool bitmap_check(BITMAP b, size_t pos);

bool bitmap_check_range(BITMAP b, size_t pos, size_t len);

#endif
