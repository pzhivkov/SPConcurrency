//
<<<<<<< HEAD:SPConcurrency/SPPriorityQueue.c
//  SPPriorityQueue.c
//  Peter Zhivkov.
=======
//  SPCPriorityQueue.c
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCPriorityQueue.c
//
//  Created by Peter Zhivkov on 09/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#include "SPCPriorityQueue.h"

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "SPUtils.h"

#include "SPCPrimitives.h"
#include "SPCMemoryReclamation.h"


static const size_t kProbabilityExponent = 1;  // A new level is added with probability (0.5)^kProbabilityExponent.
static const size_t kMaxLevels           = 6;  // Max number of levels that we'll allocate fixed storage for.



/**
 *  A concurrent lock-free priority queue node.
 */
struct _SPCPriorityQueueNode {
    volatile long                      _cmem_refCount_c;     // Markable in the lowest bit - claim flag.
    volatile markable_ptr_t            _next_d[kMaxLevels];  // Markable in the lowest bit - del flag.
    SPCPriorityQueueNode     *volatile _rPrev;               // Contains a retained pointer.
    SPCPriorityQueueKey                _key;
    void                     *volatile _data_d;              // Markable in the lowest bit - del flag.
    size_t                             _height;
    volatile size_t                    _validToHeight;
};


/*
   A note on memory management:
 
     To prevent retain cycles in the queue (i.e. two nodes pointing to each other and always maintaining a retain
     count of one and thus preventing the release of any one of them), all the nodes will be retained by the queue itself.
     That is, when a node is in the queue and is connected in the linked list structure, it will always have a retain count
     of one, regardless of whether other nodes contain references to it or not.
     
     The only exception to this rule is _rPrev, which is the only retained pointer (the r prefix stands for retained) that a
     node is allowed to temporarily retain while it is being deleted. The purpose of _rPrev is to allow other nodes that still
     reach it to be able to find the correct previous pointer in the list. The memory manager takes responsibility of releasing 
     _rPrev after the node is deleted.
     
     Otherwise, any node can have its reference count increased while a running insertion or deletion process is currently
     traversing it or working with it.
 */



#pragma mark - Forward declarations



/**
 * Forward declarations.
 */


static SPCPriorityQueueNode *scanForKey_r(SPCPriorityQueue *pqueue, SPCPriorityQueueNode **rStartingNodePtr, size_t level, SPCPriorityQueueKey key);
static SPCPriorityQueueNode *helpDeleteAndReleaseNode_r(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *rNodeToDelete, size_t level);
static void unlinkNodeAtLevel(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *rNodeToUnlink, SPCPriorityQueueNode **rPrevPtr, size_t level);



#pragma mark - Node access



/**
 *  Check if a node is retained (i.e. isn't in the free list).
 *
 *  @param node A node to check.
 *
 *  @return true if retained; false, otherwise.
 */
static FORCE_INLINE bool isNodeRetained(SPCPriorityQueueNode *node)
{
    return cmem_isRetained(node, offsetof(SPCPriorityQueueNode, _cmem_refCount_c));
}


/**
 *  Create a new node.
 *
 *  @param pqueue A lock-free priority queue.
 *  @param key    The node key.
 *  @param data   The node data.
 *
 *  @return A newly created node with the requested key and data.
 */
static FORCE_INLINE SPCPriorityQueueNode *createNode(SPCPriorityQueue *pqueue, size_t height, SPCPriorityQueueKey key, void *data)
{
    assert(pqueue);

    SPCPriorityQueueNode *newNode = cmem_allocNode((void *volatile *)(&pqueue->_freeList),
                                                   offsetof(SPCPriorityQueueNode, _cmem_refCount_c),
                                                   offsetof(SPCPriorityQueueNode, _next_d));
    if (!newNode)
        return NULL;
    
    assert(isNodeRetained(newNode));
    
    newNode->_key           = key;
    newNode->_data_d        = data;
    newNode->_rPrev         = NULL;
    newNode->_height        = (typeof(newNode->_height))(height);
    newNode->_validToHeight = 0;
    
    for (int iterLevel = 0; iterLevel < kMaxLevels; ++iterLevel)
        newNode->_next_d[iterLevel] = toMarkable(NULL, false);
    
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
static FORCE_INLINE SPCPriorityQueueNode *retainNode(SPCPriorityQueueNode *node)
{
    assert(isNodeRetained(node));
    
    cmem_retainNode(node, offsetof(SPCPriorityQueueNode, _cmem_refCount_c));
    
    return node;
}


/**
 *  Release a node.
 *
 *  @param pqueue A pointer to a priority queue.
 *  @param node   A pointer to the node to release.
 */
static FORCE_INLINE void releaseNode(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *node)
{
    assert(isNodeRetained(node));
    
    return cmem_releaseNode(node,
                            offsetof(SPCPriorityQueueNode, _cmem_refCount_c),
                            offsetof(SPCPriorityQueueNode, _next_d),
                            kMaxLevels,
                            offsetof(SPCPriorityQueueNode, _rPrev),
                            (void *volatile *)(&pqueue->_freeList));
}


/**
 *  Read a node, checking the deleted mark, and retaining the node.
 *
 *  @param pqueue     A priority queue.
 *  @param node_d_Ptr A pointer to a markable pointer to a node.
 *
 *  @return A regular pointer to a retained node.
 */
static FORCE_INLINE SPCPriorityQueueNode *readAndRetainNode_d(SPCPriorityQueue *pqueue, volatile markable_ptr_t *node_d_Ptr)
{
    assert(pqueue);
    assert(node_d_Ptr);
    
    for (;;) {
        // We need to have the node retained before it changes,
        // as this whole operation is considered to be atomic.
        markable_ptr_t node_d = (markable_ptr_t)(SPC_ATOMIC_LOAD(node_d_Ptr));
        if (isMarked_m(node_d))
            return NULL;
        
        SPCPriorityQueueNode *node = toPtr_m(node_d);
        assert(node);

        cmem_retainNode(node, offsetof(SPCPriorityQueueNode, _cmem_refCount_c));
        SPC_MEMORY_BARRIER_FULL(); // Synchronize a load of the node ptr and a store in the node's reference count simultaneously.

        if (node_d == (markable_ptr_t)(SPC_ATOMIC_LOAD(node_d_Ptr))) {
            assert(isNodeRetained(node));
            
            return node;
        } else {
            // This should never need to use the free list unless we preempted a reclaim.
            cmem_releaseNode(node,
                             offsetof(SPCPriorityQueueNode, _cmem_refCount_c),
                             offsetof(SPCPriorityQueueNode, _next_d),
                             kMaxLevels,
                             offsetof(SPCPriorityQueueNode, _rPrev),
                             (void *volatile *)(&pqueue->_freeList));
        }
    }
}



#pragma mark - Initialization



/**
 *  Initialize a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param length The priority queue length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPCPriorityQueueInit(SPCPriorityQueue *pqueue, size_t length)
{
    assert(pqueue);
    
    //
    // Allocate memory for the nodes.
    // (Reserve 2 nodes for head and tail.)
    //
    size_t queueLength = length + 2;
    pqueue->_storage = calloc(queueLength, sizeof(SPCPriorityQueueNode));
    if (!pqueue->_storage) {
        STD_OUTPUT_ERROR("priority queue allocation", "FAILURE");
        
        SPCPriorityQueueDispose(pqueue);
        return false;
    }
    
    pqueue->_size = queueLength;
    
    //
    // Prepare the free list for the custom lock-free memory allocator.
    //
    cmem_init((void *volatile *)(&pqueue->_freeList),
              offsetof(SPCPriorityQueueNode, _cmem_refCount_c),
              offsetof(SPCPriorityQueueNode, _next_d),
              sizeof(SPCPriorityQueueNode),
              pqueue->_storage,
              queueLength);
    
    //
    // Prepare the queue.
    //
    pqueue->_head = createNode(pqueue, kMaxLevels, SPC_PQ_KEY_MIN, NULL);
    pqueue->_tail = createNode(pqueue, kMaxLevels, SPC_PQ_KEY_MAX, NULL);
    
    pqueue->_head->_validToHeight = kMaxLevels;
    pqueue->_tail->_validToHeight = kMaxLevels;
    
    for (int iterLevel = 0; iterLevel < pqueue->_head->_height; ++iterLevel) {
        pqueue->_head->_next_d[iterLevel] = toMarkable(pqueue->_tail, false);
        pqueue->_tail->_next_d[iterLevel] = toMarkable(NULL, false);
    }
    
    // Prevent future changes from being observed before the queue is fully setup.
    SPC_MEMORY_BARRIER_STORE();
    
    return true;
}


/**
 *  Dispose of a concurrent lock-free priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 */
void SPCPriorityQueueDispose(SPCPriorityQueue *pqueue)
{
    // Prevent old changes from being observed happening after the queue release.
    SPC_MEMORY_BARRIER_STORE();
    
    free(pqueue->_storage);
    
    memset(pqueue, 0, sizeof(SPCPriorityQueue));
}



#pragma mark - Node traversal



/**
 *  Read next node.
 *
 *  Traverses to the next node of rNodePtr on the given level while helping any marked nodes finish the deletion.
 *  If the starting point is already marked for deletion, as a side effect, it writes the new prev node of the next node to rNodePtr.
 *  The next node is retained unless it is marked for deletion. rNodePtr is assumed to contain a retained pointer.
 *  If rNodePtr is changed, the old node will be released, and the new one will be retained.
 *
 *  @param pqueue   A priority queue.
 *  @param rNodePtr A pointer to a retained pointer to a node.
 *  @param level    A given level.
 *
 *  @return The next node (retained).
 */
static SPCPriorityQueueNode *readNextNode_r(SPCPriorityQueue *pqueue, SPCPriorityQueueNode **rNodePtr, size_t level)
{
    assert(pqueue);
    assert(rNodePtr);
    assert(isNodeRetained(*rNodePtr));
    
    if (isMarked_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&(*rNodePtr)->_data_d))))
        *rNodePtr = helpDeleteAndReleaseNode_r(pqueue, *rNodePtr, level);
    
    SPCPriorityQueueNode *rNextNode = readAndRetainNode_d(pqueue, &(*rNodePtr)->_next_d[level]);
    while (!rNextNode) {
        *rNodePtr = helpDeleteAndReleaseNode_r(pqueue, *rNodePtr, level);
        rNextNode = readAndRetainNode_d(pqueue, &(*rNodePtr)->_next_d[level]);
    }
    
    assert(level < rNextNode->_height);
    assert(isNodeRetained(rNextNode));
    assert(isNodeRetained(*rNodePtr));
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
 *  @param pqueue           A priority queue.
 *  @param rStartingNodePtr A pointer to a pointer to a node defining the starting point.
 *  @param level            A given level.
 *  @param key              A given key.
 *
 *  @return A node with the same or higher key than the given one (retained).
 */
static SPCPriorityQueueNode *scanForKey_r(SPCPriorityQueue *pqueue, SPCPriorityQueueNode **rStartingNodePtr, size_t level, SPCPriorityQueueKey key)
{
    assert(rStartingNodePtr);
    assert(isNodeRetained(*rStartingNodePtr));
  
    SPCPriorityQueueNode *rNextNode = readNextNode_r(pqueue, rStartingNodePtr, level);
    while (rNextNode->_key < key) { // We can check if we've reached the tail, but since the tail
                                    // always has key ST_PQ_MAX, this is not necessary.
        releaseNode(pqueue, *rStartingNodePtr);
        *rStartingNodePtr = rNextNode;
        rNextNode         = readNextNode_r(pqueue, rStartingNodePtr, level);
    }
    
    assert(isNodeRetained(rNextNode));
    return rNextNode;
}



#pragma mark - Node insertion



/**
 *  Return a random bit with (0.5)^pow probability.
 *
 *  @param pow The probability exponent.
 *
 *  @return A random bit.
 */
static bool getRandomBit(unsigned int pow)
{
    // Initialize the random seed.
    //
    static bool s_init = false;
    if (!s_init) {
        s_init = true;
        srand48((long)(time(NULL)));
    }
    
    // Get a random word and keep returning the bits one by one, until we've used them all.
    //
    static long s_randomBitSequence;
    static int  s_currentBit = sizeof(long) * CHAR_BIT - 1;
    
    while (pow--) {
        if (s_currentBit >= sizeof(long) * CHAR_BIT - 1) {
            s_currentBit = -1;
            s_randomBitSequence = lrand48();
        }
        ++s_currentBit;
        
        if (!((s_randomBitSequence & (1 << s_currentBit)) >> s_currentBit))
            return false;
    }
    
    return true;
}


/**
 *  Choose a random height according to a probability distribution of (0.5)^kProbabilityExponent for level increase.
 *
 *  @param maxHeight Current maximum level.
 *
 *  @return A random height.
 */
static FORCE_INLINE size_t chooseRandomHeight(size_t maxHeight)
{
    // Limit the maximum level to available storage capacity.
    if (maxHeight > kMaxLevels)
        maxHeight = kMaxLevels;
    
    size_t height = 1;
    while (getRandomBit(kProbabilityExponent) && height < maxHeight)
        ++height;
    
    return height;
}


/**
 *  Insert an element into a priority queue.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param key    An element key.
 *  @param data   The element.
 *
 *  @return true if successful.
 */
bool SPCPriorityQueueInsertElement(SPCPriorityQueue *pqueue, SPCPriorityQueueKey key, void *data)
{
    assert(pqueue);
    
    // Reject misaligned data.
    if (!IS_PTR_ALIGNED(data))
        return false;
    
    //
    // Create a new node with a height chosen according to the list's probability distribution.
    //
    size_t newNodeHeight = chooseRandomHeight(pqueue->_head->_height);
    
    SPCPriorityQueueNode *rNewNode = createNode(pqueue, newNodeHeight, key, data);
    if (!rNewNode)
        return false;
    
    retainNode(rNewNode);
    
    //
    // Search for a position in the list after which to insert the new node.
    //
    // The search starts with the head node at the highest level and traverses down to the lowest level
    // until the correct node is found. When going down one level, the last node traversed on that level
    // is remembered for later use (this is where we insert the new node at that level).
    //
    SPCPriorityQueueNode *rSavedNodes[kMaxLevels];
    memset(rSavedNodes, 0, kMaxLevels * sizeof(SPCPriorityQueueNode *));
    
    SPCPriorityQueueNode *rInsertionPoint = retainNode(pqueue->_head);
    for (int iterLevel = (signed int)(pqueue->_head->_height - 1); iterLevel >= 1; --iterLevel) {

        SPCPriorityQueueNode *rTemp = scanForKey_r(pqueue, &rInsertionPoint, iterLevel, key);
        releaseNode(pqueue, rTemp);
        
        if (iterLevel < newNodeHeight)
            rSavedNodes[iterLevel] = retainNode(rInsertionPoint);
    }
    
    //
    // Insert the new node into the queue structure on the lowest level.
    //
    for (spc_backoff_t backoffCounter = SPC_BACKOFF_INIT;;) {
  
        SPCPriorityQueueNode *rNextNode = scanForKey_r(pqueue, &rInsertionPoint, 0, key);
  
        // If there exists a node with the same priority as the new node, change the value of the old node atomically.
        //
        markable_ptr_t oldData_d = (markable_ptr_t)(SPC_ATOMIC_LOAD(&rNextNode->_data_d));
        if (!isMarked_m(oldData_d) && rNextNode->_key == key) {
            
            if (SPC_ATOMIC_COMPARE_AND_SWAP(&rNextNode->_data_d, oldData_d, data)) {
                
                // If we succeeded in swapping out the old data, then release everything and return.
                releaseNode(pqueue, rInsertionPoint);
                releaseNode(pqueue, rNextNode);
                
                for (int iterLevel = 1; iterLevel < newNodeHeight; ++iterLevel)
                    releaseNode(pqueue, rSavedNodes[iterLevel]);
                
                releaseNode(pqueue, rNewNode);
                assert((({ // Clear the next pointer for the memory allocator.
                    SPC_ATOMIC_STORE(&rNewNode->_next_d[0], toMarkable(NULL, false));
                }), true));
                
                releaseNode(pqueue, rNewNode); // Delete the node.
                
                return true;
                
            } else {
                
                // Try again.
                releaseNode(pqueue, rNextNode);
                continue;
            }
        }
 
        // Otherwise, just add the new node in front of rNextNode (at rInsertionPoint).
        //
        SPC_ATOMIC_STORE(&rNewNode->_next_d[0], toMarkable(rNextNode, false));
        SPC_MEMORY_BARRIER_STORE();
        // Since _next_d shouldn't retain pointers, we'll release rNextNode later below,
        // but we shouldn't release it before the CAS, or we might have an ABA problem.
        
        if (SPC_ATOMIC_COMPARE_AND_SWAP(&rInsertionPoint->_next_d[0], toMarkable(rNextNode, false), toMarkable(rNewNode, false))) {
            SPC_MEMORY_BARRIER_STORE();
            releaseNode(pqueue, rNextNode);
            releaseNode(pqueue, rInsertionPoint);
            break;
        }

        releaseNode(pqueue, rNextNode);

        // Back off.
        SPC_BackoffExponential(&backoffCounter);
    }
    
    
    //
    // Insert node on higher levels.
    //
    for (int iterLevel = 1; iterLevel < newNodeHeight; ++iterLevel) {
        
        SPC_ATOMIC_STORE(&rNewNode->_validToHeight, (long)iterLevel);
        
        rInsertionPoint = rSavedNodes[iterLevel];
        for (spc_backoff_t backoffCounter = SPC_BACKOFF_INIT;;) {
            SPCPriorityQueueNode *rNextNode = scanForKey_r(pqueue, &rInsertionPoint, iterLevel, key);
            
            SPC_ATOMIC_STORE(&rNewNode->_next_d[iterLevel], toMarkable(rNextNode, false));
            SPC_MEMORY_BARRIER_STORE(); // Update of _next_d[iterLevel] of the new node is observed before insertion point change.
            // Since _next_d shouldn't retain pointers, we'll release rNextNode later below,
            // but we shouldn't release it before the CAS, or we might have an ABA problem.
            
            bool isMarked = false;
            bool isLinked = false;
            if ((isMarked = isMarked_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNewNode->_data_d)))) ||
                (isLinked = SPC_ATOMIC_COMPARE_AND_SWAP(&rInsertionPoint->_next_d[iterLevel],
                                                        toMarkable(rNextNode, false),
                                                        toMarkable(rNewNode, false)))) {
                SPC_MEMORY_BARRIER_STORE();
                if (isMarked) {
                    // If this was already marked, then clear the next pointer for the memory allocator.
                    //
                    assert((({
                        SPC_ATOMIC_STORE(&rNewNode->_next_d[iterLevel], toMarkable(NULL, true));
                        SPC_MEMORY_BARRIER_STORE();
                    }), true));
                } else if (isLinked &&
                           isMarked_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNewNode->_data_d))) &&
                           (markable_ptr_t)(SPC_ATOMIC_LOAD(&rNewNode->_next_d[iterLevel])) != NULL_D) {
                    // If we managed to link the node on this level, but now the mark was set,
                    // make sure it will be unlinked. Otherwise we may get a data race where the thread
                    // that marked the node for deletion already thinks it is unlinked on this level and
                    // will simply release it, leading to corruption of the skip list structure.
                    //
                    unlinkNodeAtLevel(pqueue, rNewNode, &rInsertionPoint, iterLevel);
                }
                
                releaseNode(pqueue, rNextNode);
                releaseNode(pqueue, rInsertionPoint);
                break;
            }
            
            releaseNode(pqueue, rNextNode);
            
            // Back off.
            SPC_BackoffExponential(&backoffCounter);
        }
    }
    
    //
    // If insertion was successful on all levels, set the validLevel. Also, check for deletion.
    //
    SPC_ATOMIC_STORE(&rNewNode->_validToHeight, newNodeHeight);
    if (isMarked_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNewNode->_data_d))))
        rNewNode = helpDeleteAndReleaseNode_r(pqueue, rNewNode, 0);
    
    releaseNode(pqueue, rNewNode);
    
    return true;
}



#pragma mark - Node extraction



/**
 *  Check if a given node is unlinked at the given level of the list structure.
 *
 *  The traversal starts from the node in rPrev and continues until either the given node or a node with a higher priority is found.
 *  rPrev is a retained pointer, and if it's changed, then new value will be retained as well.
 *
 *  @param pqueue       A priority queue.
 *  @param rNodeToCheck A given node to check.
 *  @param rPrevPtr     A starting point of the traversal.
 *  @param level        The level on which to carry out the traversal
 *
 *  @return true if unlinked; false, otherwise.
 */
static FORCE_INLINE bool isNodeUnlinkedAtLevel(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *rNodeToCheck, SPCPriorityQueueNode **rPrevPtr, size_t level)
{
    assert(rNodeToCheck);
    assert(rPrevPtr && *rPrevPtr);
    assert(isNodeRetained(*rPrevPtr));
    assert(isNodeRetained(rNodeToCheck));
    
    const SPCPriorityQueueKey key = rNodeToCheck->_key;
    
    SPCPriorityQueueNode *rNextNode = scanForKey_r(pqueue, rPrevPtr, level, key);
    if (rNextNode != rNodeToCheck && rNextNode->_key == key) {
        //
        // We encountered a duplicate entry with the same key.
        // Try to go forward and see if our node is still somewhere behind.
        //
        SPCPriorityQueueNode *rTempPrev = retainNode(*rPrevPtr);
        
        rNextNode = readNextNode_r(pqueue, &rTempPrev, level);
        while (rNextNode->_key == key && rNextNode != rNodeToCheck) {
            releaseNode(pqueue, rTempPrev);
            rTempPrev = rNextNode;
            rNextNode = readNextNode_r(pqueue, &rTempPrev, level);
        }
        
        if (rNextNode == rNodeToCheck) {
            releaseNode(pqueue, *rPrevPtr);
            *rPrevPtr = rTempPrev;
        } else
            releaseNode(pqueue, rTempPrev);
    }
    releaseNode(pqueue, rNextNode);

    return rNextNode->_key > key;
}



/**
 *  Remove a node from the linked list structure at the given level using a given hint (prevPtr) for the previous node.
 *
 *  @param pqueue       A pointer to a lock-free priority queue.
 *  @param nodeToUnlink A node to unlink.
 *  @param rPrevPtr     A retained pointer to a prev pointer.
 *  @param level        A given level.
 */
static void unlinkNodeAtLevel(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *rNodeToUnlink, SPCPriorityQueueNode **rPrevPtr, size_t level)
{
    assert(pqueue);
    assert(rNodeToUnlink);
    assert(rPrevPtr && *rPrevPtr);
    assert(isNodeRetained(rNodeToUnlink));
    assert(isNodeRetained(*rPrevPtr));
    assert(level < rNodeToUnlink->_height);
    
    for (spc_backoff_t backOffCounter = SPC_BACKOFF_INIT;;) {
        
        if ((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNodeToUnlink->_next_d[level])) == NULL_D)
            break;
        
        //
        // Verify if the node is still part of the linked list structure.
        //
        if (isNodeUnlinkedAtLevel(pqueue, rNodeToUnlink, rPrevPtr, level) || (markable_ptr_t)(SPC_ATOMIC_LOAD(&rNodeToUnlink->_next_d[level])) == NULL_D)
            break;
    
        //
        // Try to change the next pointer of the prev node.
        //
        if (SPC_ATOMIC_COMPARE_AND_SWAP(&(*rPrevPtr)->_next_d[level],
                                        toMarkable(rNodeToUnlink, false),
                                        toMarkable_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNodeToUnlink->_next_d[level])), false))) {
            SPC_MEMORY_BARRIER_STORE(); // Prevent the following store operation from being observed before
                                        // the CAS above in a concurrent execution of this function on the same node.
            SPC_ATOMIC_STORE(&rNodeToUnlink->_next_d[level], NULL_D);
            SPC_MEMORY_BARRIER_STORE();
            break;
        }
        
        if ((markable_ptr_t)(SPC_ATOMIC_LOAD(&rNodeToUnlink->_next_d[level])) == NULL_D)
            break;
        
        // Back off.
        SPC_BackoffExponential(&backOffCounter);
    }
}


/**
 *  Help delete and release a node.
 *
 *  @param pqueue        A pointer to a lock-free priority queue.
 *  @param rNodeToDelete A flagged retained node.
 *  @param level         The current level.
 *
 *  @return The correct previous node of the given flagged node (retained).
 */
static SPCPriorityQueueNode *helpDeleteAndReleaseNode_r(SPCPriorityQueue *pqueue, SPCPriorityQueueNode *rNodeToDelete, size_t level)
{
    assert(pqueue);
    assert(rNodeToDelete);
    assert(isNodeRetained(rNodeToDelete));
    assert(level < rNodeToDelete->_height);
    
    //
    // Set the deletion mark on this level and on higher levels.
    //
    for (size_t iterLevel = level; iterLevel < rNodeToDelete->_height; ++iterLevel)
        for (;;) {
            markable_ptr_t nextNode = (markable_ptr_t)(SPC_ATOMIC_LOAD(&rNodeToDelete->_next_d[iterLevel]));
            if (isMarked_m(nextNode) ||
                SPC_ATOMIC_COMPARE_AND_SWAP(&rNodeToDelete->_next_d[iterLevel], toMarkable_m(nextNode, false), toMarkable_m(nextNode, true))) {
                SPC_MEMORY_BARRIER_STORE();
                break;
            }
        }

    //
    // Check if prev node is valid for deletion.
    // If not, then search for the correct prev node.
    //
    SPCPriorityQueueNode *rPrev = (void *)(SPC_ATOMIC_LOAD(&rNodeToDelete->_rPrev));
    if (!rPrev || level >= (size_t)(SPC_ATOMIC_LOAD(&rPrev->_validToHeight))) {
        rPrev = retainNode(pqueue->_head);

        for (int iterLevel = (signed)(pqueue->_head->_height - 1); iterLevel >= (signed)(level); --iterLevel) {
            
            SPCPriorityQueueNode *rTmpNode = scanForKey_r(pqueue, &rPrev, iterLevel, rNodeToDelete->_key);
            releaseNode(pqueue, rTmpNode);
        }
    } else
        retainNode(rPrev);
    
    //
    // Delete the node on the current level.
    //
    unlinkNodeAtLevel(pqueue, rNodeToDelete, &rPrev, level);
    releaseNode(pqueue, rNodeToDelete);
    
    return rPrev;
}


/**
 *  Delete and return the element with the minimum key value.
 *
 *  @param pqueue A pointer to a lock-free priority queue.
 *  @param outKey An optional pointer that if passed, will be set to the key of the element.
 *
 *  @return The element with the minimum key value.
 */
void *SPCPriorityQueueExtractMinimumElement(SPCPriorityQueue *pqueue, SPCPriorityQueueKey *outKey)
{
    assert(pqueue);
    
    markable_ptr_t     retData_d = 0;
    SPCPriorityQueueKey retKey;
    
    //
    // Start from the head and get the first node not marked for deletion.
    //
    SPCPriorityQueueNode *rFirstNode = 0;
    for (SPCPriorityQueueNode *rPrev = retainNode(pqueue->_head);;) {
        
        rFirstNode = readNextNode_r(pqueue, &rPrev, 0);
        if (rFirstNode == pqueue->_tail) { // The queue is empty.
            releaseNode(pqueue, rPrev);
            releaseNode(pqueue, rFirstNode);
            
            return NULL;
        }
        
    retry:
        //
        // Check if this is still the first node.
        //
        if (rFirstNode != toPtr_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&rPrev->_next_d[0])))) {
            releaseNode(pqueue, rFirstNode);
            continue;
        }
        
        //
        // Get the data and key, and then set the deletion mark.
        //
        retData_d = (markable_ptr_t)(SPC_ATOMIC_LOAD(&rFirstNode->_data_d));
        retKey    = rFirstNode->_key;
        
        if (!isMarked_m((markable_ptr_t)(retData_d))) {
            
            if (SPC_ATOMIC_COMPARE_AND_SWAP(&rFirstNode->_data_d, toMarkable_m(retData_d, false), toMarkable_m(retData_d, true))) {
                SPC_MEMORY_BARRIER_STORE(); // _rPrev update is observed after the deletion marker is set.
                SPC_ATOMIC_STORE(&rFirstNode->_rPrev, rPrev);
                break;
            } else
                goto retry;
            
        } else // Already flagged for deletion
            rFirstNode = helpDeleteAndReleaseNode_r(pqueue, rFirstNode, 0);
        
        releaseNode(pqueue, rPrev);
        rPrev = rFirstNode;
    }
    
    //
    // Set delete mark on all pointers to next nodes.
    //
    for (int iterLevel = 0; iterLevel < rFirstNode->_height; ++iterLevel) {
        for (;;) {
            markable_ptr_t nextNode = (markable_ptr_t)(SPC_ATOMIC_LOAD(&rFirstNode->_next_d[iterLevel]));
            if (isMarked_m(nextNode) ||
                SPC_ATOMIC_COMPARE_AND_SWAP(&rFirstNode->_next_d[iterLevel], toMarkable_m(nextNode, false), toMarkable_m(nextNode, true))) {
                SPC_MEMORY_BARRIER_STORE();
                break;
            }
        }
    }
    
    //
    // Unlink the node and then delete it.
    //
    SPCPriorityQueueNode *rPrev = retainNode(pqueue->_head);
    for (int iterLevel = (signed)(rFirstNode->_height - 1); iterLevel >= 0; --iterLevel)
        unlinkNodeAtLevel(pqueue, rFirstNode, &rPrev, iterLevel);
    releaseNode(pqueue, rPrev);

    releaseNode(pqueue, rFirstNode);
    releaseNode(pqueue, rFirstNode); // Delete the node.
    
    //
    // Return the data.
    //
    if (outKey)
        *outKey = retKey;
    
    return toPtr_m(retData_d);
}



#pragma mark - MPSC methods



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
void *SPCPriorityQueuePeek_MPSC(SPCPriorityQueue *pqueue, SPCPriorityQueueKey *outputKey)
{
    assert(pqueue);
    
    // Get the first node.
    //
    SPCPriorityQueueNode *firstNode = toPtr_m((markable_ptr_t)(SPC_ATOMIC_LOAD(&pqueue->_head->_next_d[0])));
    if (firstNode == pqueue->_tail) // The queue is empty.
        return NULL;
    
    // Get the data and key.
    //
    if (outputKey)
        *outputKey = firstNode->_key;
    
    return firstNode->_data_d;
}


