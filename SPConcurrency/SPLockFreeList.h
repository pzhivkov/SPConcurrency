//
//  SPLockFreeList.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#ifndef SpinTimer_SPLockFreeList_h
#define SpinTimer_SPLockFreeList_h

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>



struct _SPLockFreeListNode;

typedef struct _SPLockFreeListNode SPLockFreeListNode;


/**
 *  A concurrent lock-free sorted list structure.
 */
struct SPLockFreeList {
    SPLockFreeListNode          *_head;
    SPLockFreeListNode          *_tail;
    SPLockFreeListNode *volatile _freeList;
    void                        *_storage;
    size_t                       _size;
};

typedef struct SPLockFreeList SPLockFreeList;



/**
 *  Initialize a concurrent lock-free list.
 *
 *  @param list   A pointer to a lock-free list.
 *  @param length The list length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPLockFreeListInit(SPLockFreeList *list, size_t length);


/**
 *  Dispose of a concurrent lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 */
void SPLockFreeListDispose(SPLockFreeList *list);


/**
 *  Insert an element into a lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A key.
 *  @param data The element.
 *
 *  @return true if successful.
 */
bool SPLockFreeListInsertElement(SPLockFreeList *list, long key, void *data);


/**
 *  Extract an element with a given key.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A given key.
 *
 *  @return The data corresponding to the given key; NULL if not found.
 */
void *SPLockFreeListExtractElementWithKey(SPLockFreeList *list, long key);


/**
 *  Delete and return the element with the minimum priority value.
 *
 *  @param list   A lock-free list.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPLockFreeListExtractMinimumElement(SPLockFreeList *list, long *outKey);



#endif
