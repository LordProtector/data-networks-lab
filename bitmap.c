#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "bitmap.h"

/**
 * Internal bitmap structure.
 */
typedef struct _BITMAP
{
  size_t  len;
  uint8_t *map;
} _BITMAP;

/**
 * Creates a new circular bitmap of length 'len' and returns a handle to it.
 *
 * @param len The length of the bitmap to create
 * @return Handle for created bitmap
 */
BITMAP bitmap_new(size_t len)
{
  _BITMAP *bitmap = malloc(sizeof(*bitmap));
  bitmap->len = len;
  bitmap->map = calloc(len / 8 + 1, sizeof(*bitmap->map));

  return (BITMAP) bitmap;
}

/**
 * Frees all resources allocated for given bitmap.
 * The handle is invalid afterwards.
 *
 * @param b Handle of bitmap to destroy
 */
void bitmap_free(BITMAP b)
{
  _BITMAP *bitmap = (_BITMAP *)b;

  free(bitmap->map);
  free(bitmap);
}

/**
 * Sets bit at position 'pos' of bitmap 'b'.
 *
 * @param b Handle of bitmap
 * @param pos Position of bit to set
 */
void bitmap_set(BITMAP b, size_t pos)
{
  _BITMAP *bitmap = (_BITMAP *)b;

  pos %= bitmap->len;
  size_t byte  = pos / 8;
  uint8_t mask = 1 << (pos % 8);

  bitmap->map[byte] |= mask;
}

/**
 * Sets 'len' bits from position 'pos' of bitmap 'b' on.
 *
 * @param b Handle of bitmap
 * @param pos Position of first bit to set
 * @param len Number of bits to set
 */
void bitmap_set_range(BITMAP b, size_t pos, size_t len)
{
  size_t i;
  size_t end = pos + len;

  for (i = pos; i < end; i++) {
    bitmap_set(b, i);
  }
}

/**
 * Clears bit at position 'pos' of bitmap 'b'.
 *
 * @param b Handle of bitmap
 * @param pos Position of bit to clear
 */
void bitmap_clear(BITMAP b, size_t pos)
{
  _BITMAP *bitmap = (_BITMAP *)b;

  pos %= bitmap->len;
  size_t byte  = pos / 8;
  uint8_t mask = 1 << (pos % 8);

  if (bitmap->map[byte] | mask) {
    bitmap->map[byte] ^= mask;
  }
}

/**
 * Clears 'len' bits from position 'pos' of bitmap 'b' on.
 *
 * @param b Handle of bitmap
 * @param pos Position of first bit to clear
 * @param len Number of bits to clear
 */
void bitmap_clear_range(BITMAP b, size_t pos, size_t len)
{
  size_t i;
  size_t end = pos + len;

  for (i = pos; i < end; i++) {
    bitmap_clear(b, i);
  }
}

/**
 * Flips bit at position 'pos' of bitmap 'b'.
 *
 * @param b Handle of bitmap
 * @param pos Position of bit to flip
 */
void bitmap_flip(BITMAP b, size_t pos)
{
  _BITMAP *bitmap = (_BITMAP *)b;

  pos %= bitmap->len;
  size_t byte  = pos / 8;
  uint8_t mask = 1 << (pos % 8);

  bitmap->map[byte] ^= mask;
}

/**
 * Flips 'len' bits from position 'pos' of bitmap 'b' on.
 *
 * @param b Handle of bitmap
 * @param pos Position of first bit to flip
 * @param len Number of bits to flip
 */
void bitmap_flip_range(BITMAP b, size_t pos, size_t len)
{
  size_t i;
  size_t end = pos + len;

  for (i = pos; i < end; i++) {
    bitmap_flip(b, i);
  }
}

/**
 * Checks whether bit at position 'pos' of bitmap 'b' is set.
 *
 * @param b Handle of bitmap
 * @param pos Position of bit to return
 * @return Whether the requested bit is set
 */
bool bitmap_check(BITMAP b, size_t pos)
{
  _BITMAP *bitmap = (_BITMAP *)b;

  pos %= bitmap->len;
  size_t byte  = pos / 8;
  uint8_t mask = 1 << (pos % 8);

  return bitmap->map[byte] & mask;
}

/**
 * Checks whether 'len' continuous bits from position 'pos' on are set.
 *
 * @param b Handle of bitmap
 * @param pos Position of first bit to check
 * @param len Number of bits to check
 * @return Whether bits are set
 */
bool bitmap_check_range(BITMAP b, size_t pos, size_t len)
{
  size_t i;
  size_t end = pos + len;

  for (i = pos; i < end; i++) {
    if (!bitmap_check(b, i)) {
      return false;
    }
  }

  return true;
}

/**
 * Returns position of first unset bit, beginning at pos
 *
 * @param b Handle of bitmap
 * @param pos Position of first bit to check
 * @param len Number of bits to check
 * @return position of first unset bit, beginning at pos
 */
size_t bitmap_next_unset(BITMAP b, size_t pos)
{
  _BITMAP *bitmap = (_BITMAP *)b;
	size_t i;
	size_t end = pos + bitmap->len;

	for (i = pos; i < end; i++) {
		if (!bitmap_check(b, i)) {
			break;
		}
	}

	return i;  //FIXME What to return, if all bits are set?
	//either -1 or i+1, we should discuss what is better
}
