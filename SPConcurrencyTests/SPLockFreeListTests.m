//
//  SPLockFreeListTests.m
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 10/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "SPLockFreeList.h"


struct test_elem_t {
    long key;
    void  *data;
};

typedef struct test_elem_t test_elem_t;




@interface SPLockFreeListTests : XCTestCase

@property (nonatomic) SPLockFreeList list;

@end


const size_t kDefaultListSize = 4096;



@implementation SPLockFreeListTests

- (void)setUp
{
    [super setUp];

    SPLockFreeListInit(&_list, kDefaultListSize);
}

- (void)tearDown
{
    SPLockFreeListDispose(&_list);
    [super tearDown];
}



- (void)testInitsAndDisposesCorrectly
{
    SPLockFreeListDispose(&_list);
    
    for (size_t numElems = 512; numElems <= 1048576; numElems *= 2) {
        
        XCTAssertTrue(SPLockFreeListInit(&_list, numElems),
                      @"List can't be initialized.");
        SPLockFreeListDispose(&_list);
    }
    
    XCTAssertTrue(SPLockFreeListInit(&_list, kDefaultListSize),
                  @"List can't be initialized.");
}



- (void)testDoesNotInsertDuplicates
{
    test_elem_t retrieveOneElem;
    test_elem_t oneElem = {
        .key  = 1.0,
        .data = (void *)(sizeof(void *))
    };
    
    XCTAssertTrue(SPLockFreeListInsertElement(&_list, oneElem.key, oneElem.data),
                  @"Can't insert element into list.");
    
    XCTAssertTrue(SPLockFreeListInsertElement(&_list, oneElem.key, oneElem.data),
                  @"Can't insert element into list twice.");
    
    retrieveOneElem.data = SPLockFreeListExtractMinimumElement(&_list, &retrieveOneElem.key);
    
    XCTAssertTrue(retrieveOneElem.data == oneElem.data && retrieveOneElem.key == oneElem.key,
                  @"List returns corrupt element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPLockFreeListExtractMinimumElement(&_list, 0) == NULL,
                  @"List fails to delete minimum element after extraction.");
}



- (void)testEnforcesPriorities
{
    test_elem_t retrieveElem;
    test_elem_t oneElem = {
        .key  = 1.0,
        .data = (void *)(sizeof(void *) * 1)
    };
    
    test_elem_t twoElem = {
        .key  = 2.0,
        .data = (void *)(sizeof(void *) * 2)
    };
    
    test_elem_t threeElem = {
        .key  = 3.0,
        .data = (void *)(sizeof(void *) * 3)
    };
    
    for (int dups = 0; dups < kDefaultListSize * 4; ++dups) {
        XCTAssertTrue(SPLockFreeListInsertElement(&_list, threeElem.key, threeElem.data),
                      @"Can't insert element into list.");
        XCTAssertTrue(SPLockFreeListInsertElement(&_list, oneElem.key, oneElem.data),
                      @"Can't insert element into list.");
        XCTAssertTrue(SPLockFreeListInsertElement(&_list, twoElem.key, twoElem.data),
                      @"Can't insert element into list.");
    }
    
    retrieveElem.data = SPLockFreeListExtractMinimumElement(&_list, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == oneElem.data && retrieveElem.key == oneElem.key,
                  @"List returns wrong element.");
    
    retrieveElem.data = SPLockFreeListExtractMinimumElement(&_list, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == twoElem.data && retrieveElem.key == twoElem.key,
                  @"List returns wrong element.");
    
    retrieveElem.data = SPLockFreeListExtractMinimumElement(&_list, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == threeElem.data && retrieveElem.key == threeElem.key,
                  @"List returns wrong element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPLockFreeListExtractMinimumElement(&_list, 0) == NULL,
                  @"List still holds elements after extracting everything from it.");
}


- (void)testParallelInsertAndDelete
{
    [self runParallelInsertAndDeleteForNumberOfElems:5 numberOfThreads:1 numberOfRuns:100];
    [self runParallelInsertAndDeleteForNumberOfElems:5 numberOfThreads:1024 numberOfRuns:50];
    [self runParallelInsertAndDeleteForNumberOfElems:512 numberOfThreads:64 numberOfRuns:10];
}



- (void)runParallelInsertAndDeleteForNumberOfElems:(size_t)numElems
                                   numberOfThreads:(size_t)numThreads
                                      numberOfRuns:(size_t)numRuns
{
    //SPLockFreeListDispose(&_list);
    __block SPLockFreeList list;
    for (int reps = 0; reps < numRuns; ++reps) {
        fprintf(stderr, ".");
        const size_t totalNumElems = numElems;
        XCTAssertTrue(SPLockFreeListInit(&list, totalNumElems * numThreads));
        
        
        dispatch_group_t group = dispatch_group_create();
        dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestQueue", DISPATCH_QUEUE_CONCURRENT);
        
        dispatch_suspend(queue);
        
        // Insert and delete.
        for (int iter = 1; iter <= numThreads; ++iter) {
            
            dispatch_group_async(group, queue, ^{
                
                [self fillListWithOrderedElements:&list
                                     startingFrom:1
                                             upTo:totalNumElems];
            });
            dispatch_group_async(group, queue, ^{
                
                // Retrieve the elements.
                for (int numElem = 0; numElem < totalNumElems * 2; ++numElem) {
                    test_elem_t retrieveElem;
                    
                    retrieveElem.data = SPLockFreeListExtractMinimumElement(&list, &retrieveElem.key);
                }
            });
        }
        
        dispatch_resume(queue);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        SPLockFreeListDispose(&list);
    }
    //SPLockFreeListInit(&_list, 4096);
    fprintf(stderr, "\n");
}


- (void)fillListWithOrderedElements:(SPLockFreeList *)localList
                       startingFrom:(const size_t)startIdx
                               upTo:(const size_t)endIdx
{
    // Add even elements.
    const int startElemFront = (int)((startIdx % 2 == 0) ? startIdx : startIdx + 1);
    for (int numElem = startElemFront; numElem <= endIdx; numElem += 2) {
        test_elem_t oneElem = {
            .key  = (long)numElem,
            .data = (void *)(sizeof(void *) * numElem)
        };
        
        XCTAssertTrue(SPLockFreeListInsertElement(localList, oneElem.key, oneElem.data),
                      @"Can't insert element into list.");
    }
    
    // Then add odd elements backwards.
    const int startElemBack = (int)((endIdx % 2 == 0) ? endIdx - 1 : endIdx);
    for (int numElem = startElemBack; numElem >= 1; numElem -= 2) {
        test_elem_t oneElem = {
            .key  = (long)numElem,
            .data = (void *)(sizeof(void *) * numElem)
        };
        
        XCTAssertTrue(SPLockFreeListInsertElement(localList, oneElem.key, oneElem.data),
                      @"Can't insert element into list.");
    }
}


@end
