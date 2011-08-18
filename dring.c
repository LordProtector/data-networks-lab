/**
 * dring.c
 *  
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of a double ring.
 *
 * It is used for cyclic sequence numbers whereas the number of different sequence
 * numbers should be at least twice as large as the window size.
 *
 * The double ring stores data in two sorted queues. The first queue always
 * contains the "smaller" elements of the two queues. Elements are added to
 * the second queue if they are "larger" than elements of the first queue.
 * This is the case if they are numerically smaller AND the difference to the
 * largest element of the first queue is larger than the window size.
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "squeue.h"
#include "dring.h"

/**
 * Data structure for the double ring.
 */
typedef struct _DRING
{
	SQUEUE a; // queue with the "smaller" elements
	SQUEUE b; // queue with the "larger" elements
	int windowSize; // size of the window
} _DRING;


/**
 * Creates a new double ring.
 * 
 * @param windowSize Size of the window.
 * @return returns the created ring.
 */
DRING dring_new(int windowSize)
{
	_DRING *dring = malloc(sizeof(*dring));
	dring->a = squeue_new();
	dring->b = squeue_new();
	dring->windowSize = windowSize;
	return (DRING)dring;
}


/**
 * Frees all resources allocated for the given double ring.
 * 
 * @param d Handle of double ring to destroy.
 */
void dring_free(DRING d)
{
	_DRING *dring = (_DRING *)d;
	squeue_free(dring->a);
	squeue_free(dring->b);
	free(dring);
}

/**
 * Insert an element in the double ring.
 * The data is inserted in the ring with the smallest distance to the existing data.
 * This ensures that data are handled correctly.
 *
 * @param d Handle of the double ring.
 * @param data The Data to be added to the ring.
 */
void dring_insert(DRING d, int data)
{
	_DRING *dring = (_DRING *)d;

	assert(data < dring->windowSize*2);
	int maxFirstRing = squeue_peek_tail(d->a);
	if(abs(data - maxFirstRing) < dring->windowSize) {
		squeue_insert(dring->a, data);
	}
	else {
		squeue_insert(dring->b, data);
	}
}

/**
 * Returns (but keeps) the "smallest" value of the double ring.
 * Returns -1 if double ring is empty.
 * @param d Double ring
 * @return "Smallest" value of the given double ring or -1 if it is empty
 */
int dring_peek(DRING d)
{
	_DRING *dring = (_DRING *)d;
	
	if(squeue_nitems(dring->a) == 0) {
		/* dring empty */
		assert(squeue_nitems(dring->b) == 0);
		return -1;
	}
	else {
		return squeue_peek(dring->a);
	}
}

/**
 * Returns the "smallest" element from the double ring.
 *
 * @param d Handle of the double ring.
 * @return The "smallest" element of the double ring.
 */
int dring_pop(DRING d)
{
	_DRING *dring = (_DRING *)d;
	
	if(squeue_nitems(dring->a) == 0) {
		/* dring empty */
		assert(squeue_nitems(dring->b) == 0);
		return -1;
	}
	else {
		int ret = squeue_pop(dring->a);

		if(squeue_nitems(dring->a) == 0) {
			/* dring has become empty -> switch rings */
			SQUEUE *tmp = dring->b;
			dring->b = dring->a;
			dring->a = tmp;
		}
		return ret;
	}
}


/**
 * Returns the number of elements of the double ring.
 * This is the total number of elements of both rings.
 *
 * @param d Handle of the double ring.
 * @return Number of elements of the double ring.
 */
int dring_nitems(DRING d)
{
	_DRING *dring = (_DRING *)d;
	return squeue_nitems(dring->a) + squeue_nitems(dring->b);
}

int main() {
	DRING d = dring_new(4);
	dring_insert(d, 8)
	return 0;
}
