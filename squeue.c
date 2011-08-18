#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "squeue.h"

typedef struct ENTRY
{
  int data;
  struct ENTRY *next;
  struct ENTRY *prev;
} ENTRY;

typedef struct _SQUEUE
{
  size_t len;
  ENTRY *head;
  ENTRY *tail;
} _SQUEUE;


/**
 * Creates a new sorted queue.
 * @return Handle for the sorted queue
 */
SQUEUE squeue_new()
{
  _SQUEUE *squeue = malloc(sizeof(*squeue));
  squeue->len = 0;
  squeue->head = NULL;
  squeue->tail = NULL;
  return (SQUEUE)squeue;
}

/**
 * Frees all resources allocated for given queue.
 * The handle is invalid afterwards.
 *
 * @param s Handle of queue to destroy
 */
void squeue_free(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  ENTRY *iter, *iter_delete;
  for(iter = squeue->head; NULL != iter; ) {
    iter_delete = iter;
    iter = iter->next;
    free(iter_delete);
  }
  free(squeue);
}


/**
 * Insert data at the correct position.
 * @param s Sorted queue
 * @param data To be inserted data
 */
void squeue_insert(SQUEUE s, int data)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  squeue->len++;
  ENTRY *entry = malloc(sizeof(*entry));
  entry->data = data;
  entry->next = NULL;
  entry->prev = NULL;

  ENTRY* iter;
  for(iter = squeue->head; NULL != iter; iter = iter->next) {
    if(iter->data > entry->data) {
      break;
    }
  }
  
  if (NULL != iter) {
    // iter = first squeue entry with higher data
    entry->next = iter;
    entry->prev = iter->prev;
    
    if(NULL != iter->prev)
      iter->prev->next = entry;
    iter->prev = entry;
    if(squeue->head == iter)
      squeue->head = entry;
  }
  else {
    if (NULL == squeue->head) {
      // squeue empty
      assert(NULL == squeue->tail);
      assert(1 == squeue->len);
      entry->next = NULL;
      entry->prev = NULL;
      squeue->head = squeue->tail = entry;
    }
    else {
      // insert at the end of squeue
      entry->next = NULL;
      entry->prev = squeue->tail;
      squeue->tail->next = entry;
      squeue->tail = entry;
    }
  }
}


/**
 * Removes and returns the first value of the queue.
 * @param s Sorted queue
 * @return first value of the given queue
 */
int squeue_pop(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  ENTRY *entry = squeue->head;
  if (NULL != entry) {
    squeue->head = entry->next;
    if(NULL != squeue->head)
      squeue->head->prev = NULL;
    if(squeue->tail == entry)
      squeue->tail = NULL;
    int ret = entry->data;
    free(entry);
    squeue->len--;
    return ret;
  }
  else {
    return -1;
  }
}


/**
 * Returns (but keeps) the first value of the queue.
 * @param s Sorted queue
 * @return first value of the given queue
 */
int squeue_peek(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  ENTRY *entry = squeue->head;
  if (NULL != entry) {
    return entry->data;
  }
  else {
    return -1;
  }
}

/**
 * Returns number of entries in queue.
 * @param s Sorted queue
 * @return number of entries in the given queue
 */
int squeue_nitems(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  return squeue->len;
}

int squeue_peek_tail(SQUEUE s)
{
	_SQUEUE *squeue = (_SQUEUE *)s;
	if(NULL != squeue->tail) {
		 ENTRY *entry = squeue->tail;
		return entry->data;
	}
	else
		return -1;
}

int main() {
  SQUEUE s = squeue_new();
  squeue_pop(s);
  squeue_insert(s, 4);
  squeue_pop(s);
  squeue_insert(s, 3);
  squeue_insert(s, 5);
  squeue_insert(s, -2);
  squeue_pop(s);
  squeue_pop(s);
  squeue_insert(s, 10);
  squeue_pop(s);
  squeue_insert(s, 6);
  squeue_insert(s, 13);
  squeue_insert(s, 11);

  int j;
  for(j = 10000000; j>0; j--) {
    squeue_insert(s, j);
  }
  printf("n: %d\n", squeue_nitems(s));
  squeue_free(s);

  while(1) {}

  printf("peek: %d\n", squeue_peek(s));

  int i;
  int n = squeue_nitems(s);
  for(i=0; i<n; i++)
    printf("%d\n", squeue_pop(s));
  return 0;
}
