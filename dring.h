/**
 * dring.h
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Header file for a double ring.
 */

#ifndef DRING_H_
#define DRING_H_

typedef void * DRING;

DRING dring_new(int windowSize);

void dring_free(DRING d);

void dring_insert(DRING d, int data);

int dring_peek(DRING d);

int dring_pop(DRING d);

int dring_nitems(DRING d);

#endif
