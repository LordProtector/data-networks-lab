/**
 * squeue.c
 *
 * @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
 *
 * Implementation of a sorted queue. It has the ability to add elements to
 * the queue in a numerically sorted way. Small elements are at the head of
 * the queue and large elements are at the end. Insertion can be done in O(n)
 * where n is the number of elements so far in the queue. Getting the lowest
 * and the largest element of the queue can be done in constant time.
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "squeue.h"

/**
 * Data structure for one entry of the queue.
 */
typedef struct ENTRY
{
  int data;           // Data of the entry. It is used for sorting.
  struct ENTRY *next; // Pointer to the next entry.
  struct ENTRY *prev; // Pointer to the previous entry.
} ENTRY;


/**
 * Data structure for the sorted queue.
 */
typedef struct _SQUEUE
{
  size_t len;  // Length of the queue.
  ENTRY *head; // Pointer to the head of the queue.
  ENTRY *tail; // Pointer to the tail of the queue.
} _SQUEUE;


/**
 * Creates a new sorted queue.
 *
 * @return Handle for the created sorted queue.
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
 * @param s Handle of queue to destroy.
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
 * The ordering is numerical. The lowest values are at the beginning of the
 * queue. Insertion can be done in O(n) where n is the number of elements in
 * the queue so far.
 *
 * @param s Handle to the sorted queue.
 * @param data To be inserted data.
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
  //find position for insertion
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
 * Removes and returns the first value of the queue in constant time and -1
 * if the queue is empty.
 *
 * @param s Handle to the sorted queue.
 * @return First value of the given queue or -1 if the queue is empty.
 */
int squeue_pop(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  ENTRY *entry = squeue->head;
  if (NULL != entry) { //are there elements in the queue?
    squeue->head = entry->next;
    //update pointers
    if(NULL != squeue->head)
      squeue->head->prev = NULL;
    //is the queue empty after removing one element?
    if(squeue->tail == entry)
      squeue->tail = NULL;
    int ret = entry->data;
    free(entry);
    squeue->len--;
    return ret;
  }
  else { //there are no elements to return
    return -1;
  }
}


/**
 * Returns (but keeps) the first value of the queue in constant time and -1
 * if the queue is empty.
 *
 * @param s Handle to the sorted queue.
 * @return First value of the given queue or -1 if the queue is empty.
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

/** Checks whether the given sorted queue contains data.
 *
 * @param s Handle to the sorted queue.
 * @param data The data to be searched.
 * @return True if data is in queue, false otherwise.
 */
bool squeue_contains(SQUEUE s, int data)
{
	_SQUEUE *squeue = (_SQUEUE *)s;
  ENTRY *iter;

	for (iter = squeue->head; iter != NULL && iter->data <= data; iter = iter->next) {
		if (iter->data == data) {
			return true;
		}
	}
	return false;
}


/**
 * Returns number of entries in queue in constant time.
 *
 * @param s Handle to the sorted queue.
 * @return Number of entries in the given queue.
 */
int squeue_nitems(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  return squeue->len;
}


/**
 * Returns (but keeps) the last value of the queue in constant time or -1
 * if the queue is empty.
 *
 * @param s Handle to the sorted queue.
 * @return Last element of the sorted queue or -1 if the queue is empty.
 */
int squeue_peek_tail(SQUEUE s)
{
  _SQUEUE *squeue = (_SQUEUE *)s;
  if(NULL != squeue->tail) {
    ENTRY *entry = squeue->tail;
    return entry->data;
  }
  else {
    return -1;
  }
}
