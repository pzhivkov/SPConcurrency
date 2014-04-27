//
//  SPMemoryReclamation.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 14/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#ifndef PZ_SPMemoryReclamation_h
#define PZ_SPMemoryReclamation_h



#include "SPCPrimitives.h"


/**
 *  A fixed-size lock-free memory reclamation scheme adapted from the corrected version of Valois's algorithm (Michael & Scott).
 *
 *  References:
 *
 *  - Michael, Maged M., and Michael L. Scott. Correction of a Memory Management Method
 *      for Lock-Free Data Structures. No. TR-599. ROCHESTER UNIV NY DEPT OF COMPUTER SCIENCE, 1995.
 *
 *  - John David Valois. 1996. Lock-Free Data Structures. Ph.D. Dissertation.
 *      Rensselaer Polytechnic Institute, Troy, NY, USA. UMI Order No. GAX95-44082.
 */




#pragma mark - Concurrent memory management



/**
 *  Decrement and test and set a reference count in a single atomic operation.
 *  This is achieved by combining a test bit (lowest bit) and a reference count (remaining bits) in a single value.
 *  The reference count is decreased and once it reaches 0, the claimed bit is set.
 *
 *  @param refCountPtr A pointer to a reference count value.
 *
 *  @return True if the claim bit was set; false, otherwise.
 */
static FORCE_INLINE bool cmem_decrementAndTestAndSet(volatile long *refCountPtr)
{
    assert(refCountPtr);

    for (;;) {
        
        long oldValue = (long)(SPC_ATOMIC_LOAD(refCountPtr));
        long newValue = (oldValue == 2) ? 1 : oldValue - 2;
        
        if (SPC_ATOMIC_COMPARE_AND_SWAP(refCountPtr, oldValue, newValue)) {
     
            SPC_MEMORY_BARRIER_STORE();
            return (oldValue - newValue) & 1;
        }
        
        SPC_STALL();
    }
}


/**
 *  Check if node has a forward link.
 *
 *  @param node          A node.
 *  @param nextPtrOffset Next pointer offset.
 *
 *  @return true if the pointer to the next node is non-null.
 */
static FORCE_INLINE bool cmem_nodeHasNoForwardLinks(void *node, ptrdiff_t nextPtrOffset, size_t numNextPtrs)
{
    for (int idx = 0; idx < numNextPtrs; ++idx)
        if (toMarkable_m((markable_ptr_t)(SPC_ATOMIC_LOAD(node + nextPtrOffset + nextPtrOffset * idx)), false))
            return false;
    return true;
}


/**
 *  Release a node.
 *  This functions decrements the reference count, and if nobody else is pointing to the node,
 *  it tries to claim it. If that succeeds, it returns it back to the free node list after releasing
 *  any retained links.
 *
 *  @param node               The node to release.
 *  @param refCountOffset     The offset to the reference count field in the node.
 *  @param nextPtrOffset      The offset to the next pointer field in the node.
 *  @param retainedLinkOffset An offset to a retained link, (-1 if none).
 *  @param freeListPtr        A pointer to the free list's head pointer.
 */
static FORCE_INLINE void cmem_releaseNode(void           *node,
                                          ptrdiff_t       refCountOffset,
                                          ptrdiff_t       nextPtrOffset,
                                          size_t          numNextPtrs,
                                          ptrdiff_t       retainedLinkOffset,
                                          void *volatile *freeListPtr)
{
    if (!node) return;
    
    //
    // Decrement the ref count and then attempt to claim the node.
    //
    if (!cmem_decrementAndTestAndSet(node + refCountOffset))
        return;
    
    assert(cmem_nodeHasNoForwardLinks(node, nextPtrOffset, numNextPtrs));
    
    //
    // If we claimed the node, release any retained links.
    //
    if (retainedLinkOffset >= 0) {
        cmem_releaseNode((void *)(SPC_ATOMIC_LOAD(node + retainedLinkOffset)),
                         refCountOffset,
                         nextPtrOffset,
                         numNextPtrs,
                         retainedLinkOffset,
                         freeListPtr);
        
        SPC_ATOMIC_STORE(node + retainedLinkOffset, NULL);
    }
    
    //
    // Reclaim the node (i.e. by adding it to the top of the free list).
    //
    assert(freeListPtr);
    
    for (;;) {
        void *freeListHead = (void *)(SPC_ATOMIC_LOAD(freeListPtr));
        SPC_ATOMIC_STORE(node + nextPtrOffset, freeListHead);
        
        // Use a barrier to make sure that the change is observed to happen after
        // we have claimed the node and updated the _next_d[0] pointer of the node.
        SPC_MEMORY_BARRIER_STORE();
        
        if (SPC_ATOMIC_COMPARE_AND_SWAP(freeListPtr, *(void *volatile *)(node + nextPtrOffset), node))
            break;
    }
}


/**
 *  Atomically retain a node.
 *
 *  @param node           A pointer to a node.
 *  @param refCountOffset The offset to the reference count field in the node.
 */
static FORCE_INLINE void cmem_retainNode(void *node, ptrdiff_t refCountOffset)
{
    (void)SPC_ATOMIC_FETCH_AND_ADD(node + refCountOffset, 2);
}


/**
 *  A safe read of the freelist head that increases the reference count of the node.
 *
 *  @param freeListPtr    A reference to the free list head.
 *  @param refCountOffset The offset to the reference count field in free list nodes.
 *  @param nextPtrOffset  The offset to the next pointer field in free list nodes.
 *
 *  @return A retained reference to the node.
 */
static FORCE_INLINE void *cmem_safeReadHead(void *volatile *freeListPtr, ptrdiff_t refCountOffset, ptrdiff_t nextPtrOffset)
{
    assert(freeListPtr);
    
    for (;;) {
        void *node = (void *)(SPC_ATOMIC_LOAD(freeListPtr));
        if (!node)
            return NULL;
        
        cmem_retainNode(node, refCountOffset);
        
        if (node == (void *)(SPC_ATOMIC_LOAD(freeListPtr)))
            return node;
        else {
            // This should never need to use the free list unless we preempted a reclaim.
            cmem_releaseNode(node, refCountOffset, nextPtrOffset, 1, -1, freeListPtr);
        }
    }
}


/**
 *  Allocate a new node from the fixed pool of a queue.
 *
 *  @param freeListPtr    A pointer to the free list's head pointer.
 *  @param refCountOffset The offset to the reference count field in free list nodes.
 *  @param nextPtrOffset  The offset to the next pointer field in free list nodes.
 *
 *  @return A node available for use.
 */
static FORCE_INLINE void *cmem_allocNode(void *volatile *freeListPtr, ptrdiff_t refCountOffset, ptrdiff_t nextPtrOffset)
{
    assert(freeListPtr);
    
    for (;;) {
        void *newNode = cmem_safeReadHead(freeListPtr, refCountOffset, nextPtrOffset);
        if (!newNode) {
            STD_OUTPUT_ERROR("cmem_allocNode", "out of memory in the fixed pool");
            return NULL;
        }
        
        //
        // Move the head of the free list.
        //
        if (SPC_ATOMIC_COMPARE_AND_SWAP(freeListPtr, newNode, *(void *volatile *)(newNode + nextPtrOffset))) {
            
            // Add a barrier to make sure the free list head update is observed to happen strictly before
            // any changes to the node.
            SPC_MEMORY_BARRIER_STORE();
            
            // Clear the claimed bit.
            //
            for (;;) {
                long oldValue = (long)(SPC_ATOMIC_LOAD(newNode + refCountOffset));
                
                if (SPC_ATOMIC_COMPARE_AND_SWAP(newNode + refCountOffset, oldValue, oldValue - 1)) {
                    
                    SPC_ATOMIC_STORE(newNode + nextPtrOffset, toMarkable(NULL, false));
                    SPC_MEMORY_BARRIER_STORE();
                    return newNode;
                }
                
                SPC_STALL();
            }
        } else {
            
            // Release the node.
            //
            cmem_releaseNode(newNode, refCountOffset, nextPtrOffset, 1, -1, freeListPtr);
        }
    }
}


/**
 *  Verify if the memory the node occupies is reclaimed.
 *
 *  @param node A node to verify.
 *
 *  @return true if reclaimed; false, otherwise.
 */
static FORCE_INLINE bool cmem_isRetained(const void *node, ptrdiff_t refCountOffset)
{
    return !((long)(SPC_ATOMIC_LOAD(node + refCountOffset)) % 2);
}


/**
 *  Initialize a free list for a memory reclamation scheme based on fixed-size nodes and fixed storage.
 *
 *  @param freeListPtr    A pointer to the free list pointer.
 *  @param refCountOffset The offset of the reference count field used by free list nodes.
 *  @param nextPtrOffset  The offset of the next pointer field used by free list nodes.
 *  @param nodeSize       The size of a node (fixed).
 *  @param storage        A pointer to a block of free memory large enough to accomodate the requested number of nodes.
 *  @param totalNumNodes  The total number of nodes on the free list.
 */
static FORCE_INLINE void cmem_init(void *volatile *freeListPtr,
                                   ptrdiff_t       refCountOffset,
                                   ptrdiff_t       nextPtrOffset,
                                   size_t          nodeSize,
                                   void           *storage,
                                   size_t          totalNumNodes)
{
    assert(freeListPtr);
    assert(storage);
    
    void *currentNode = storage;
    
    for (int idx = 0; idx < totalNumNodes; ++idx) {
        SPC_ATOMIC_STORE(currentNode + refCountOffset, 1);
        SPC_ATOMIC_STORE(currentNode + nextPtrOffset, toMarkable(((idx == totalNumNodes - 1) ? NULL : currentNode + nodeSize), false));
        currentNode = toPtr_m((markable_ptr_t)(SPC_ATOMIC_LOAD(currentNode + nextPtrOffset)));
    }
    
    *freeListPtr = storage;
}



#endif
