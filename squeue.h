#ifndef SQUEUE_H_
#define SQUEUE_H_

typedef void * SQUEUE;

SQUEUE squeue_new();

void squeue_free(SQUEUE s);

void squeue_insert(SQUEUE s, int data);

int squeue_peek(SQUEUE s);

int squeue_pop(SQUEUE s);

int squeue_nitems(SQUEUE s);

int squeue_peek_tail(SQUEUE s);

#endif
