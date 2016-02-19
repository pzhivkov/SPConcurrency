//
//  SPCPriorityQueue.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#ifndef PZ_SPCPriorityQueue_h
#define PZ_SPCPriorityQueue_h

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>



struct _SPCPriorityQueueNode;

typedef struct _SPCPriorityQueueNode SPCPriorityQueueNode;


typedef uint64_t SPCPriorityQueueKey;

#define SPC_PQ_KEY_MAX LLONG_MAX
#define SPC_PQ_KEY_MIN LLONG_MIN



/**
 *  A concurrent priority queue structure.
 */
struct SPCPriorityQueue {
    SPCPriorityQueueNode          *_head;
    SPCPriorityQueueNode          *_tail;
    SPCPriorityQueueNode *volatile _freeList;
    void                          *_storage;
    size_t                         _size;
};

typedef struct SPCPriorityQueue SPCPriorityQueue;



/**
 *  Initialize a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param length The priority queue length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPCPriorityQueueInit(SPCPriorityQueue *pqueue, size_t length);


/**
 *  Dispose of a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 */
void SPCPriorityQueueDispose(SPCPriorityQueue *pqueue);


/**
 *  Insert an element into a lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param key    A key.
 *  @param data   The element.
 *
 *  @return true if successful.
 */
bool SPCPriorityQueueInsertElement(SPCPriorityQueue *pqueue, SPCPriorityQueueKey key, void *data);


/**
 *  Delete and return the element with the minimum key value.
 *
 *  @param pqueue A lock-free priority queue.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPCPriorityQueueExtractMinimumElement(SPCPriorityQueue *pqueue, SPCPriorityQueueKey *outKey);


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
void *SPCPriorityQueuePeek_MPSC(SPCPriorityQueue *pqueue, SPCPriorityQueueKey *outputKey);



#endif
