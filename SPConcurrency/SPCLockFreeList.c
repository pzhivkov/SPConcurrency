//
<<<<<<< HEAD:SPConcurrency/SPLockFreeList.c
//  SPLockFreeList.c
//  Peter Zhivkov.
=======
//  SPCLockFreeList.c
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCLockFreeList.c
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#include "SPCLockFreeList.h"

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SPUtils.h"

#include "SPCPrimitives.h"
#include "SPCMemoryReclamation.h"



/**
 *  A concurrent lock-free list node.
 */
struct _SPCLockFreeListNode {
    volatile long  _cmem_refCount_c; // Markable in the lowest bit - claim flag.
    markable_ptr_t _next_d;          // Markable in the lowest bit - del flag.
    long           _key;
    void          *_data;
};



#pragma mark - Forward declarations



/**
 *  Forward declarations.
 */


static bool unlinkNode(SPCLockFreeList *list, SPCLockFreeListNode *nodeToUnlink, SPCLockFreeListNode **rPrevPtr);



#pragma mark - Node access



/**
 *  Create a new node.
 *
 *  @param list A lock-free list.
 *  @param key  The node key.
 *  @param data The node data.
 *
 *  @return A newly created node with the requested key and data.
 */
static FORCE_INLINE SPCLockFreeListNode *createNode(SPCLockFreeList *list, long key, void *data)
{
    assert(list);

    SPCLockFreeListNode *newNode = cmem_allocNode((void *volatile *)(&list->_freeList),
                                                  offsetof(SPCLockFreeListNode, _cmem_refCount_c),
                                                  offsetof(SPCLockFreeListNode, _next_d));
    if (!newNode)
        return NULL;
    
    newNode->_key    = key;
    newNode->_data   = data;
    newNode->_next_d = toMarkable(NULL, false);

    SPC_MEMORY_BARRIER_STORE();
    
    return newNode;
}


/**
 *  Retain a node.
 *
 *  @param node A node.
 *
 *  @return The retained node.
 */
static FORCE_INLINE SPCLockFreeListNode *retainNode(SPCLockFreeListNode *node)
{
    assert(cmem_isRetained(node, offsetof(SPCLockFreeListNode, _cmem_refCount_c)));
    
    cmem_retainNode(node, offsetof(SPCLockFreeListNode, _cmem_refCount_c));
    
    return node;
}


/**
 *  Release a node.
 *
 *  @param list A list.
 *  @param node A node.
 */
static FORCE_INLINE void releaseNode(SPCLockFreeList *list, SPCLockFreeListNode *node)
{
    assert(cmem_isRetained(node, offsetof(SPCLockFreeListNode, _cmem_refCount_c)));
    
    return cmem_releaseNode(node,
                            offsetof(SPCLockFreeListNode, _cmem_refCount_c),
                            offsetof(SPCLockFreeListNode, _next_d),
                            1,
                            -1,
                            (void *volatile *)(&list->_freeList));
}


/**
 *  Read a node, checking the deleted flag, and retaining the node.
 *
 *  @param list       A list.
 *  @param node_d_Ptr A pointer to a flagged pointer to a node.
 *  @param flagged    A pointer to a bool notifying the caller whether the node pointer is flagged.
 *
 *  @return A regular pointer to a retained node.
 */
static FORCE_INLINE SPCLockFreeListNode *readAndRetainNode_d(SPCLockFreeList *list, markable_ptr_t *node_d_Ptr)
{
    assert(list);
    assert(node_d_Ptr);
    
    for (;;) {
        // We need to have the node retained before it changes,
        // as this whole operation is considered to be atomic.
        markable_ptr_t node_d = *node_d_Ptr;
        if (isMarked_m(node_d))
            return NULL;
        
        SPCLockFreeListNode *node = toPtr_m(node_d);
        assert(node);

        cmem_retainNode(node, offsetof(SPCLockFreeListNode, _cmem_refCount_c));
        SPC_MEMORY_BARRIER_FULL();

        if (node_d == *node_d_Ptr) {
            assert(cmem_isRetained(node, offsetof(SPCLockFreeListNode, _cmem_refCount_c)));
            
            return node;
        } else {
            // This should never need to use the free list unless we preempted a reclaim.
            cmem_releaseNode(node,
                             offsetof(SPCLockFreeListNode, _cmem_refCount_c),
                             offsetof(SPCLockFreeListNode, _next_d),
                             1,
                             -1,
                             (void *volatile *)(&list->_freeList));
        }
    }
}



#pragma mark - Initialization



/**
 *  Initialize a concurrent lock-free list.
 *
 *  @param list   A pointer to a lock-free list.
 *  @param length The list length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPCLockFreeListInit(SPCLockFreeList *list, size_t length)
{
    assert(list);
    
    //
    // Allocate memory for the nodes.
    // (Reserve 2 nodes for head and tail.)
    //
    size_t listLength = length + 2;
    list->_storage = calloc(listLength, sizeof(SPCLockFreeListNode));
    if (!list->_storage) {
        STD_OUTPUT_ERROR("list allocation", "FAILURE");
        
        SPCLockFreeListDispose(list);
        return false;
    }
    
    list->_size = listLength;
    
    //
    // Prepare the free list for the custom lock-free memory allocator.
    //
    cmem_init((void *volatile *)(&list->_freeList),
              offsetof(SPCLockFreeListNode, _cmem_refCount_c),
              offsetof(SPCLockFreeListNode, _next_d),
              sizeof(SPCLockFreeListNode),
              list->_storage,
              listLength);
    
    list->_head = createNode(list, LONG_MIN, NULL);
    list->_tail = createNode(list, LONG_MAX, NULL);
    
    list->_head->_next_d = toMarkable(list->_tail, false);
    list->_tail->_next_d = toMarkable(NULL, false);
    
    // Prevent future changes from being observed before the queue is fully setup.
    SPC_MEMORY_BARRIER_STORE();
    
    return true;
}


/**
 *  Dispose of a concurrent lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 */
void SPCLockFreeListDispose(SPCLockFreeList *list)
{
    // Prevent old changes from being observed happening after the queue release.
    SPC_MEMORY_BARRIER_STORE();
    
    free(list->_storage);
    
    memset(list, 0, sizeof(SPCLockFreeList));
}



#pragma mark - Node traversal



/**
 *  Read next node.
 *
 *  Traverses to the next node of rNodePtr while helping any marked nodes finish the deletion.
 *  If the starting point is already marked for deletion, as a side effect, it writes the new prev node of the next node to rNodePtr.
 *  The next node is retained unless it is marked for deletion. rNodePtr is assumed to contain a retained pointer.
 *  If rNodePtr is changed, the old node will be released, and the new one will be retained.
 *
 *  @param list      A pointer to a lock-free list.
 *  @param rNodePtr  A pointer to a retained pointer to a node.
 *  @param rPrevNode A pointer to the starting node.
 *
 *  @return The next node (retained) or NULL if a deletion on the way to the next node fails.
 */
static SPCLockFreeListNode *readNextNode_r(SPCLockFreeList *list, SPCLockFreeListNode **rNodePtr, SPCLockFreeListNode *rPrevNode)
{
    assert(list);
    assert(rNodePtr && *rNodePtr);
    assert(rPrevNode);
    assert(cmem_isRetained(*rNodePtr, offsetof(SPCLockFreeListNode, _cmem_refCount_c)));

    if (*rNodePtr == list->_tail)
        return NULL;
    
    SPCLockFreeListNode *rNextNode = readAndRetainNode_d(list, &(*rNodePtr)->_next_d);
    while (!rNextNode) {

        // Try to delete the marked node.
        //
        if (!unlinkNode(list, *rNodePtr, &rPrevNode)) {
            return NULL;
        }
        releaseNode(list, *rNodePtr);
        *rNodePtr = readAndRetainNode_d(list, &rPrevNode->_next_d);
        if (!*rNodePtr)
            return NULL;
        
        
        // Move on to the next node until we find an unflagged one.
        //
        if (*rNodePtr == list->_tail) {
            break;
        }
        
        rNextNode = readAndRetainNode_d(list,  &(*rNodePtr)->_next_d);
    }

    return rNextNode;
}



/**
 *  Scan for a node with key.
 *
 *  Traverses in several steps through the next pointers (starting from rStartingNodePtr)
 *  until it finds a node that has the same or higher key than the given one.
 *
 *  If the given node is marked for deletion or no longer valid, the correct prev node of the found node
 *  is written in rStartingNodePtr. Both the returned node and rNodePtr are retained pointers, so if *rStartingNodePtr is
 *  changed, the old value will be released and the new one will be retained.
 *
 *  @param list             A list.
 *  @param rStartingNodePtr A pointer to a pointer to a node defining the starting point.
 *  @param key              A given key.
 *
 *  @return A node with the same or higher key than the given one (retained).
 */
SPCLockFreeListNode *scanForKey_r(SPCLockFreeList *list, SPCLockFreeListNode **rPrevPtr, long key)
{
    assert(list);
    assert(rPrevPtr);
    assert(!*rPrevPtr);
    
retry:
    for (;;) {
        
        // Start the search from the head.
        //
        *rPrevPtr = retainNode(list->_head);
        SPCLockFreeListNode *rCurNode = readAndRetainNode_d(list, &(*rPrevPtr)->_next_d);
        
        for (;;) {
            // Check if the next node is reachable.
            //
            if (!rCurNode) {
                releaseNode(list, *rPrevPtr);
                goto retry;
            }
            
            SPCLockFreeListNode *rNextNode = readNextNode_r(list, &rCurNode, *rPrevPtr);
            if (!rCurNode) {
                releaseNode(list, *rPrevPtr);
                goto retry;
            }
            if (!rNextNode && rCurNode != list->_tail) {
                releaseNode(list, *rPrevPtr);
                releaseNode(list, rCurNode);
                
                goto retry;
            }

            // Verify the key and return if we have a match.
            //
            if (rCurNode->_key >= key) {
                if (rNextNode)
                    releaseNode(list, rNextNode);
                
                return rCurNode;
            }
            
            assert(rNextNode);
            
            // Move on to the next node.
            //
            releaseNode(list, *rPrevPtr);
            *rPrevPtr = rCurNode;
            rCurNode  = rNextNode;
        }
    }
}



/**
 *  Insert an element into a lock-free list.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A key.
 *  @param data The element.
 *
 *  @return true if successful.
 */
bool SPCLockFreeListInsertElement(SPCLockFreeList *list, long key, void *data)
{
    assert(list);
    
    // Reject misaligned data.
    if (!IS_PTR_ALIGNED(data))
        return false;
    
    //
    // Create a new node.
    //
    SPCLockFreeListNode *rNewNode = createNode(list, key, data);
    if (!rNewNode)
        return false;
    
    retainNode(rNewNode);
    
    for (;;) {
        //
        // Search for a position in the list after which to insert the new node.
        //
        SPCLockFreeListNode *rInsertionPoint = 0;
        
        SPCLockFreeListNode *rNextNode = scanForKey_r(list, &rInsertionPoint, key);
        if (rNextNode->_key == key) {
            releaseNode(list, rNextNode);
            releaseNode(list, rInsertionPoint);
            releaseNode(list, rNewNode);
            rNewNode->_next_d = toMarkable(NULL, false);
            releaseNode(list, rNewNode); // Delete the node.
            
            return true; // maybe should return false?
        } else {

            // Insert the new node.
            //
            rNewNode->_next_d = toMarkable(rNextNode, false);
            SPC_MEMORY_BARRIER_STORE();
            if (SPC_ATOMIC_COMPARE_AND_SWAP(&rInsertionPoint->_next_d, toMarkable(rNextNode, false), toMarkable(rNewNode, false))) {

                releaseNode(list, rNextNode);
                releaseNode(list, rInsertionPoint);
                releaseNode(list, rNewNode);
                return true;
            }
            
            releaseNode(list, rInsertionPoint);
            releaseNode(list, rNextNode);
        }
    }
}



#pragma mark - Deletion



/**
 *  Remove a node from the linked list structure using a given hint (prevPtr) for the previous node.
 *
 *  @param list         A pointer to a lock-free list.
 *  @param nodeToUnlink A node to unlink.
 *  @param rPrevPtr     A retained pointer to a prev pointer.
 */
static bool unlinkNode(SPCLockFreeList *list, SPCLockFreeListNode *nodeToUnlink, SPCLockFreeListNode **rPrevPtr)
{
    assert(list);
    assert(nodeToUnlink);
    assert(rPrevPtr && *rPrevPtr);
    
    markable_ptr_t nextNode_m = toMarkable_m(nodeToUnlink->_next_d, false);
    
    if (!SPC_ATOMIC_COMPARE_AND_SWAP(&(*rPrevPtr)->_next_d, toMarkable(nodeToUnlink, false), nextNode_m)) {
        // Retry.
        return false;
    }
    
    assert(nodeToUnlink != list->_head);
    
    // Successfully deleted.
    //
    SPC_MEMORY_BARRIER_STORE();
    nodeToUnlink->_next_d = NULL_D;
    
    return true;
}


/**
 *  Delete and return the element with the minimum priority value.
 *
 *  @param list   A lock-free list.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPCLockFreeListExtractMinimumElement(SPCLockFreeList *list, long *outKey)
{
    assert(list);
    
    void *retData;
    long  retKey;
    
    //
    // Start from the head and get the first node not marked for deletion.
    //
    
    SPCLockFreeListNode *rPrev = retainNode(list->_head);
    for (;;) {
        
        // Start the search from the head.
        //
        if (!rPrev) { rPrev = retainNode(list->_head); continue; }
        SPCLockFreeListNode *rFirstNode = readAndRetainNode_d(list, &rPrev->_next_d);
        if (!rFirstNode) continue;
        if (rFirstNode == list->_head) {
            releaseNode(list, rFirstNode);
            continue;
        }
        
        if (rFirstNode == list->_tail) { // The queue is empty.
            releaseNode(list, rPrev);
            releaseNode(list, rFirstNode);
            
            return NULL;
        }
        
        // Check if the next node is reachable.
        //
        SPCLockFreeListNode *rNextNode = readNextNode_r(list, &rFirstNode, rPrev);
        if (!rFirstNode) continue;
        if (!rNextNode) {
            releaseNode(list, rFirstNode);
            continue;
        }
        if (rFirstNode == list->_head) {
            releaseNode(list, rFirstNode);
            releaseNode(list, rNextNode);
            continue;
        }

        
        // Got it!
        //
        assert(rFirstNode != list->_tail);
        assert(rFirstNode != list->_head);
        
        // Extract the element, by first flagging, and then deleting the node.
        //
        SPC_MEMORY_BARRIER_STORE();
        assert(rNextNode);
        if (!isMarked_m(rFirstNode->_next_d) &&
            !SPC_ATOMIC_COMPARE_AND_SWAP(&rFirstNode->_next_d, toMarkable(rNextNode, false), toMarkable(rNextNode, true))) {

            releaseNode(list, rFirstNode);
            releaseNode(list, rNextNode);
            continue;
        }
        
        
        // Delete.
        //
        retData = rFirstNode->_data;
        retKey  = rFirstNode->_key;
        if (!unlinkNode(list, rFirstNode, &rPrev)) {
            
            releaseNode(list, rFirstNode);
            releaseNode(list, rNextNode);
            continue;
        }

        releaseNode(list, rNextNode);
        releaseNode(list, rPrev);
        releaseNode(list, rFirstNode);
        rFirstNode->_next_d = NULL_D;
        releaseNode(list, rFirstNode); // Delete the node.
        
        if (outKey)
            *outKey = retKey;
        return retData;
    }
}


/**
 *  Extract an element with a given key.
 *
 *  @param list A pointer to a lock-free list.
 *  @param key  A given key.
 *
 *  @return The element corresponding to the given key; NULL if not found.
 */
void *SPCLockFreeListExtractElementWithKey(SPCLockFreeList *list, long key)
{
    assert(list);
    
    
    for (;;) {
        
        //
        // Search for a node with the given key.
        //
        SPCLockFreeListNode *rPrev = 0;
        SPCLockFreeListNode *rNode = scanForKey_r(list, &rPrev, key);
        if (rNode->_key != key) {
            
            // Not found. Most likely due to reaching the tail, which has the maximum key.
            //
            releaseNode(list, rPrev);
            releaseNode(list, rNode);
            
            return NULL;
        } else {
            
            // Extract the element, by first flagging, and then deleting the node.
            //
            SPCLockFreeListNode *rNextNode = retainNode(toPtr_m(rNode->_next_d));
            if (!SPC_ATOMIC_COMPARE_AND_SWAP(&rNode->_next_d, toMarkable(rNextNode, false), toMarkable(rNextNode, true))) {
                releaseNode(list, rPrev);
                releaseNode(list, rNode);
                releaseNode(list, rNextNode);
                continue;
            }
            
            SPC_MEMORY_BARRIER_STORE();
            
            if (!SPC_ATOMIC_COMPARE_AND_SWAP(&rPrev->_next_d, rNode, rNextNode)) {
                releaseNode(list, rPrev);
                releaseNode(list, rNode);
                releaseNode(list, rNextNode);
                continue;
            }
            
            void *data = rNode->_data;
            SPC_MEMORY_BARRIER_STORE();
            
            releaseNode(list, rPrev);
            releaseNode(list, rNextNode);
            
            releaseNode(list, rNode);
            releaseNode(list, rNode); // Delete the node.
            
            return data;
        }
    }
}

