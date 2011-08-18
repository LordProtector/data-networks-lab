#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "squeue.h"
#include "dring.h"

typedef struct _DRING
{
	SQUEUE a;
	SQUEUE b;
	int windowSize;
} _DRING;

DRING dring_new(int windowSize)
{
	_DRING *dring = malloc(sizeof(*dring));
	dring->a = squeue_new();
	dring->b = squeue_new();
	dring->windowSize = windowSize;
	return (DRING)dring;
}

void dring_free(DRING d)
{
	_DRING *dring = (_DRING *)d;
	squeue_free(dring->a);
	squeue_free(dring->b);
	free(dring);
}

void dring_insert(DRING d, int data)
{
	_DRING *dring = (_DRING *)d;

	int maxFirstRing = squeue_peek_tail(d->a);
	if(abs(data - maxFirstRing) < dring->windowSize) {
		squeue_insert(dring->a, data);
	}
	else {
		squeue_insert(dring->b, data);
	}
}

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

int dring_nitems(DRING d)
{
	_DRING *dring = (_DRING *)d;
	return squeue_nitems(dring->a) + squeue_nitems(dring->b);
}
