//
//  SPPriorityQueue.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#ifndef SpinTimer_SPPriorityQueue_h
#define SpinTimer_SPPriorityQueue_h

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>



struct _SPPriorityQueueNode;

typedef struct _SPPriorityQueueNode SPPriorityQueueNode;


typedef uint64_t SPPriorityQueueKey;

#define ST_PQ_KEY_MAX LLONG_MAX
#define ST_PQ_KEY_MIN LLONG_MIN



/**
 *  A concurrent priority queue structure.
 */
struct SPPriorityQueue {
    SPPriorityQueueNode          *_head;
    SPPriorityQueueNode          *_tail;
    SPPriorityQueueNode *volatile _freeList;
    void                         *_storage;
    size_t                        _size;
};

typedef struct SPPriorityQueue SPPriorityQueue;



/**
 *  Initialize a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param length The priority queue length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPPriorityQueueInit(SPPriorityQueue *pqueue, size_t length);


/**
 *  Dispose of a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 */
void SPPriorityQueueDispose(SPPriorityQueue *pqueue);


/**
 *  Insert an element into a lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param key    A key.
 *  @param data   The element.
 *
 *  @return true if successful.
 */
bool SPPriorityQueueInsertElement(SPPriorityQueue *pqueue, SPPriorityQueueKey key, void *data);


/**
 *  Delete and return the element with the minimum key value.
 *
 *  @param pqueue A lock-free priority queue.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPPriorityQueueExtractMinimumElement(SPPriorityQueue *pqueue, SPPriorityQueueKey *outKey);


/**
 *  Peek at the current minimum element in the queue without actually deleting it.
 *
 *  Valid for MPSC (multi-producer, single-consumer) model only.
 *
 *  @param pqueue A priority queue.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key.
 */
void *SPPriorityQueuePeek_MPSC(SPPriorityQueue *pqueue, SPPriorityQueueKey *outputKey);



#endif
