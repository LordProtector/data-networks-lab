#ifndef SQUEUE_H_
#define SQUEUE_H_

typedef void * DRING;

DRING dring_new();

void dring_free(DRING d);

void dring_insert(DRING d, int data);

int dring_peek(DRING d);

int dring_pop(DRING d);

int dring_nitems(DRING d);

int dring_peek_tail(DRING d);

#endif
