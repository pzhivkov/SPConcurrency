//
<<<<<<< HEAD:SPConcurrency/SPLockFreeList.h
//  SPLockFreeList.h
//  Peter Zhivkov.
=======
//  SPCLockFreeList.h
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCLockFreeList.h
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

<<<<<<< HEAD:SPConcurrency/SPLockFreeList.h
#ifndef PZ_SPLockFreeList_h
#define PZ_SPLockFreeList_h
=======
#ifndef PZ_SPCLockFreeList_h
#define PZ_SPCLockFreeList_h
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCLockFreeList.h

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>



struct _SPCLockFreeListNode;

typedef struct _SPCLockFreeListNode SPCLockFreeListNode;


/**
 *  A concurrent lock-free sorted list structure.
 */
struct SPCLockFreeList {
    SPCLockFreeListNode          *_head;
    SPCLockFreeListNode          *_tail;
    SPCLockFreeListNode *volatile _freeList;
    void                         *_storage;
    size_t                        _size;
};

typedef struct SPCLockFreeList SPCLockFreeList;



/**
 *  Initialize a concurrent lock-free list.
 *
 *  @param list   A pointer to a lock-free list.
 *  @param length The list length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPCLockFreeListInit(SPCLockFreeList *list, size_t length);


/**
 *  Dispose of a concurrent lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 */
void SPCLockFreeListDispose(SPCLockFreeList *list);


/**
 *  Insert an element into a lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A key.
 *  @param data The element.
 *
 *  @return true if successful.
 */
bool SPCLockFreeListInsertElement(SPCLockFreeList *list, long key, void *data);


/**
 *  Extract an element with a given key.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A given key.
 *
 *  @return The data corresponding to the given key; NULL if not found.
 */
void *SPCLockFreeListExtractElementWithKey(SPCLockFreeList *list, long key);


/**
 *  Delete and return the element with the minimum priority value.
 *
 *  @param list   A lock-free list.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPCLockFreeListExtractMinimumElement(SPCLockFreeList *list, long *outKey);



#endif
