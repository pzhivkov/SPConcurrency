//
//  SPMemoryReclamationTests.m
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 04/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <XCTest/XCTest.h>


#include "SPUtils.h"
#include "SPCPrimitives.h"
#include "SPCMemoryReclamation.h"



/**
 *  A test node.
 */
typedef struct SPTestNode SPTestNode;


struct SPTestNode {
    volatile markable_ptr_t _next_d;             // Markable in the lowest bit - del flag.
    volatile long           _cmem_refCount_c;    // Markable in the lowest bit - claim flag.
};



@interface SPMemoryReclamationTests : XCTestCase
{
    void *_storage;
    SPTestNode *volatile _freeList;
}

@end



@implementation SPMemoryReclamationTests



const size_t kNumElems = 512;


- (void)setUp
{
    [super setUp];

    _storage = calloc(kNumElems, sizeof(SPTestNode));
    if (!_storage) {
        XCTAssert(false, @"Can't initialize");
    }
    
    cmem_init((void *volatile *)(&_freeList),
              offsetof(SPTestNode, _cmem_refCount_c),
              offsetof(SPTestNode, _next_d),
              sizeof(SPTestNode),
              _storage,
              kNumElems);
    
    SPC_MEMORY_BARRIER_STORE();
}


- (void)tearDown
{
    free(_storage);
    _storage  = NULL;
    _freeList = NULL;

    [super tearDown];
}


- (void)allocAndReleaseElements:(size_t)number
                   checkIfEmpty:(BOOL)check
{
    SPTestNode *allocElems[number];
    
    // check the free list.
    for (int idx = 0; idx < number; ++idx) {
        
        SPTestNode *allocElem = cmem_allocNode((void *volatile *)(&_freeList),
                                               offsetof(SPTestNode, _cmem_refCount_c),
                                               offsetof(SPTestNode, _next_d));
        assert(allocElem);
        cmem_retainNode(allocElem,
                        offsetof(SPTestNode, _cmem_refCount_c));
        allocElems[idx] = allocElem;
        allocElem->_next_d = (markable_ptr_t)0x1111;
    }
    
    if (check) {
        SPTestNode *allocElem = cmem_allocNode((void *volatile *)(&_freeList),
                                               offsetof(SPTestNode, _cmem_refCount_c),
                                               offsetof(SPTestNode, _next_d));
        assert(!allocElem);
        XCTAssert(!allocElem, @"List does not have extra elements.");
    }
    
    for (int idx = 0; idx < number; ++idx) {
        
        assert(allocElems[idx]);
        allocElems[idx]->_next_d = NULL_D;
        cmem_releaseNode(allocElems[idx],
                         offsetof(SPTestNode, _cmem_refCount_c),
                         offsetof(SPTestNode, _next_d),
                         1,
                         -1,
                         (void *volatile *)(&_freeList));
        cmem_releaseNode(allocElems[idx],
                         offsetof(SPTestNode, _cmem_refCount_c),
                         offsetof(SPTestNode, _next_d),
                         1,
                         -1,
                         (void *volatile *)(&_freeList));
    }
}


- (void)testAllocAndRelease
{
    for (int reps = 0; reps < 100; ++reps) {
        [self allocAndReleaseElements:kNumElems checkIfEmpty:YES];
    }
}


- (void)testConcurrentAllocAndRelease
{
    for (int reps = 0; reps < 500; ++reps) {
        
        const size_t numThreads = 256;
        
        dispatch_group_t group = dispatch_group_create();
        dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestAllocator", DISPATCH_QUEUE_CONCURRENT);
        
        dispatch_suspend(queue);
        
        // Insert and delete.
        for (int iter = 1; iter <= numThreads; ++iter) {
            
            dispatch_group_async(group, queue, ^{

                SPTestNode *allocElem = cmem_allocNode((void *volatile *)(&_freeList),
                                                       offsetof(SPTestNode, _cmem_refCount_c),
                                                       offsetof(SPTestNode, _next_d));
                assert(allocElem);
                cmem_retainNode(allocElem,
                                offsetof(SPTestNode, _cmem_refCount_c));
                if (allocElem)
                    allocElem->_next_d = NULL_D;
                cmem_releaseNode(allocElem,
                                 offsetof(SPTestNode, _cmem_refCount_c),
                                 offsetof(SPTestNode, _next_d),
                                 1,
                                 -1,
                                 (void *volatile *)(&_freeList));
                cmem_releaseNode(allocElem,
                                 offsetof(SPTestNode, _cmem_refCount_c),
                                 offsetof(SPTestNode, _next_d),
                                 1,
                                 -1,
                                 (void *volatile *)(&_freeList));
                
                
                [self allocAndReleaseElements:floor(kNumElems / numThreads) checkIfEmpty:NO];
            });

        }
        
        dispatch_resume(queue);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        // check the free list.
        [self allocAndReleaseElements:kNumElems checkIfEmpty:YES];
    }
}



@end
