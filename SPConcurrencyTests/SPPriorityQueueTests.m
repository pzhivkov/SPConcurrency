//
//  SPPriorityQueueTests.m
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 01/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <XCTest/XCTest.h>

#import "SPPriorityQueue.h"


struct test_elem_t {
    SPPriorityQueueKey     key;
    void                  *data;
};

typedef struct test_elem_t test_elem_t;



@interface SPPriorityQueueTests : XCTestCase

@property (nonatomic) SPPriorityQueue pqueue;

@end

const size_t kDefaultQueueSize = 4096;

@implementation SPPriorityQueueTests


- (void)setUp
{
    [super setUp];

    XCTAssertTrue(SPPriorityQueueInit(&_pqueue, kDefaultQueueSize));
}


- (void)tearDown
{
    SPPriorityQueueDispose(&_pqueue);
    
    [super tearDown];
}


- (void)testInitsAndDisposesCorrectly
{
    SPPriorityQueueDispose(&_pqueue);
    
    for (size_t numElems = 512; numElems <= 1048576; numElems *= 2) {
        
        XCTAssertTrue(SPPriorityQueueInit(&_pqueue, numElems),
                      @"Priority queue can't be initialized.");
        SPPriorityQueueDispose(&_pqueue);
    }
    
    XCTAssertTrue(SPPriorityQueueInit(&_pqueue, kDefaultQueueSize),
                  @"Priority queue can't be initialized.");
}


- (void)testRejectsMisalignedData
{
    test_elem_t oneElem = {
        .key  = 1.0,
        .data = (void *)(0x1)
    };
    
    XCTAssertFalse(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                   @"Priority queue accepts misaligned data.");
}


- (void)testInsertAndDeleteOneElement
{
    test_elem_t retrieveOneElem;
    test_elem_t oneElem = {
        .key  = 1.0,
        .data = (void *)(sizeof(void *))
    };

    XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                  @"Can't insert element into queue.");

    retrieveOneElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveOneElem.key);
    
    XCTAssertTrue(retrieveOneElem.data == oneElem.data && retrieveOneElem.key == oneElem.key,
                  @"Queue returns corrupt element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&_pqueue, 0) == NULL,
                  @"Queue fails to delete minimum element after extraction.");
}


- (void)testDoesNotInsertDuplicates
{
    test_elem_t retrieveOneElem;
    test_elem_t oneElem = {
        .key = 1.0,
        .data     = (void *)(sizeof(void *))
    };
    
    XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                  @"Can't insert element into queue.");
    
    XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                  @"Can't insert element into queue twice.");
    
    retrieveOneElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveOneElem.key);
    
    XCTAssertTrue(retrieveOneElem.data == oneElem.data && retrieveOneElem.key == oneElem.key,
                  @"Queue returns corrupt element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&_pqueue, 0) == NULL,
                  @"Queue fails to delete minimum element after extraction.");
}


- (void)testMemoryAllocatorWorks
{
    test_elem_t retrieveElem;
    test_elem_t oneElem = {
        .key = 1.0,
        .data     = (void *)(sizeof(void *))
    };
    
    for (int dups = 0; dups < 1024; ++dups) {
        XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                      @"Can't insert element into queue.");
    }
    
    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == oneElem.data && retrieveElem.key == oneElem.key,
                  @"Queue returns wrong element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&_pqueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
}


- (void)testEnforcesPriorities
{
    test_elem_t retrieveElem;
    test_elem_t oneElem = {
        .key = 1.0,
        .data     = (void *)(sizeof(void *) * 1)
    };
    
    test_elem_t twoElem = {
        .key = 2.0,
        .data     = (void *)(sizeof(void *) * 2)
    };
    
    test_elem_t threeElem = {
        .key = 3.0,
        .data     = (void *)(sizeof(void *) * 3)
    };
    
    for (int dups = 0; dups < kDefaultQueueSize * 4; ++dups) {
        XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, threeElem.key, threeElem.data),
                      @"Can't insert element into queue.");
        XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                      @"Can't insert element into queue.");
        XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, twoElem.key, twoElem.data),
                      @"Can't insert element into queue.");
    }
    
    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == oneElem.data && retrieveElem.key == oneElem.key,
                  @"Queue returns wrong element.");
    
    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == twoElem.data && retrieveElem.key == twoElem.key,
                  @"Queue returns wrong element.");
    
    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
    XCTAssertTrue(retrieveElem.data == threeElem.data && retrieveElem.key == threeElem.key,
                  @"Queue returns wrong element.");
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&_pqueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
}


- (void)testEnforcesLengthLimits
{
    SPPriorityQueue localQueue;
    
    const size_t totalNumElems = 16384;
    XCTAssertTrue(SPPriorityQueueInit(&localQueue, totalNumElems));
    
    for (int iter = 0; iter < 3; ++iter) {
        
        // Add stuff.
        for (int numElem = 1; numElem <= totalNumElems; ++numElem) {
            test_elem_t oneElem = {
                .key = (double)numElem,
                .data     = (void *)(sizeof(void *) * numElem)
            };
            XCTAssertTrue(SPPriorityQueueInsertElement(&localQueue, oneElem.key, oneElem.data),
                          @"Can't insert element into queue.");
        }
        
        // Attempt to overflow it.
        XCTAssertFalse(SPPriorityQueueInsertElement(&localQueue, totalNumElems * 1.0 + 1.0, (void *)(sizeof(void *) * (totalNumElems + 1))),
                       @"Another element was inserted into a full queue.");
        XCTAssertFalse(SPPriorityQueueInsertElement(&localQueue, totalNumElems * 1.0 + 2.0, (void *)(sizeof(void *) * (totalNumElems + 2))),
                       @"Another element was inserted into a full queue.");
        
        // Extract everything.
        for (int numElem = 1; numElem <= totalNumElems; ++numElem) {
            test_elem_t retrieveElem;
            
            retrieveElem.data = SPPriorityQueueExtractMinimumElement(&localQueue, &retrieveElem.key);
            XCTAssertTrue(retrieveElem.data == (void *)(sizeof(void *) * numElem) && retrieveElem.key == (double)numElem,
                          @"Queue returns wrong element.");
        }
        XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                      @"Queue still holds elements after extracting everything from it.");
    }
    
    SPPriorityQueueDispose(&localQueue);
}



- (void)fillQueueWithOrderedElements:(SPPriorityQueue *)localQueue
                        startingFrom:(const size_t)startIdx
                                upTo:(const size_t)endIdx
{
    // Add even elements.
    const int startElemFront = (int)((startIdx % 2 == 0) ? startIdx : startIdx + 1);
    for (int numElem = startElemFront; numElem <= endIdx; numElem += 2) {
        test_elem_t oneElem = {
            .key = (double)numElem,
            .data     = (void *)(sizeof(void *) * numElem)
        };
        
        XCTAssertTrue(SPPriorityQueueInsertElement(localQueue, oneElem.key, oneElem.data),
                      @"Can't insert element into queue.");
    }
    
    // Then add odd elements backwards.
    const int startElemBack = (int)((endIdx % 2 == 0) ? endIdx - 1 : endIdx);
    for (int numElem = startElemBack; numElem >= 1; numElem -= 2) {
        test_elem_t oneElem = {
            .key = (double)numElem,
            .data     = (void *)(sizeof(void *) * numElem)
        };
        
        XCTAssertTrue(SPPriorityQueueInsertElement(localQueue, oneElem.key, oneElem.data),
                      @"Can't insert element into queue.");
    }
}

- (void)extractOrderedElementsFromQueue:(SPPriorityQueue *)localQueue
                                   upTo:(const size_t)totalNumElems
{
    // Retrieve everything.
    for (int numElem = 1; numElem <= totalNumElems; ++numElem) {
        test_elem_t retrieveElem;
        
        retrieveElem.data = SPPriorityQueueExtractMinimumElement(localQueue, &retrieveElem.key);
        XCTAssertTrue(retrieveElem.data == (void *)(sizeof(void *) * numElem) && retrieveElem.key == (double)numElem,
                      @"Queue returns wrong element.");
    }
}


- (void)testHasSaturationResilience
{
    SPPriorityQueue localQueue;
    
    const size_t totalNumElems = 1024;
    XCTAssertTrue(SPPriorityQueueInit(&localQueue, totalNumElems));

    
    [self fillQueueWithOrderedElements:&localQueue
                          startingFrom:1
                                  upTo:totalNumElems];
    
    // Queue should be full now. Try to add some more stuff.
    for (int numElem = 1; numElem <= 4; ++numElem) {
        test_elem_t oneElem = {
            .key = -(double)numElem,
            .data     = (void *)(sizeof(void *) * numElem)
        };
        
        XCTAssertFalse(SPPriorityQueueInsertElement(&localQueue, oneElem.key, oneElem.data),
                       @"Another element %d was inserted into a full queue.", numElem);
    }
    
    [self extractOrderedElementsFromQueue:&localQueue
                                     upTo:totalNumElems];
    
    // Make sure the queue is empty.
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
    
    
    // Then do it all over again.
    for (int iter = 1; iter < 50; ++iter) {
        [self fillQueueWithOrderedElements:&localQueue
                              startingFrom:1
                                      upTo:totalNumElems];
        [self extractOrderedElementsFromQueue:&localQueue
                                         upTo:totalNumElems];
    }
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
    
    SPPriorityQueueDispose(&localQueue);
}


- (void)testToleratesMixingOfOperations
{
    const int numRuns = 10;
    for (int iter = 0; iter < numRuns; ++iter) {
        // Add even elements.
        for (int numElem = 2; numElem <= kDefaultQueueSize; numElem += 2) {
            test_elem_t oneElem = {
                .key = (double)numElem,
                .data     = (void *)(sizeof(void *) * numElem)
            };
            
            XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                          @"Can't insert element into queue.");
        }
        
        // Then add odd elements and remove from the front.
        for (int numElem = 1; numElem <= kDefaultQueueSize; numElem += 2) {
            test_elem_t oneElem = {
                .key = (double)numElem,
                .data     = (void *)(sizeof(void *) * numElem)
            };
            
            XCTAssertTrue(SPPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data),
                          @"Can't insert element into queue.");
            
            // Retrieve from the front.
            test_elem_t retrieveElem;
            
            retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
            int expectedNum = (numElem + 1) / 2;
            XCTAssertTrue(retrieveElem.data == (void *)(sizeof(void *) * expectedNum) && retrieveElem.key == (double)expectedNum,
                          @"Queue returns wrong element.");
        }
        
        for (int numElem = kDefaultQueueSize / 2 + 1; numElem <= kDefaultQueueSize; ++numElem) {
            // Retrieve from the front.
            test_elem_t retrieveElem;
            
            retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
            XCTAssertTrue(retrieveElem.data == (void *)(sizeof(void *) * numElem) && retrieveElem.key == (double)numElem,
                          @"Queue returns wrong element.");
        }
    }
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&_pqueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
}


- (void)testSameQueueObjectCanBeReused
{
    SPPriorityQueue localQueue;
    for (int runs = 0; runs < 500; ++runs) {
        
        SPPriorityQueueInit(&localQueue, 4096);
        
        const size_t kNumElems = 2, mulFactor = 10;

        for (int iter = 1; iter <= mulFactor; ++iter) {
            [self fillQueueWithOrderedElements:&localQueue
                                  startingFrom:1
                                          upTo:kNumElems * iter];
        }
        
        [self extractOrderedElementsFromQueue:&localQueue upTo:kNumElems * mulFactor];
        
        XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                      @"Queue still holds elements after extracting everything from it.");

        SPPriorityQueueDispose(&localQueue);
    }
}


- (void)performParallelInserts:(SPPriorityQueue *)localQueue
              minNumberOfElems:(const size_t)minNumElems
               numberOfThreads:(const size_t)numThreads
                  numberOfRuns:(const size_t)numRuns
{
    for (int runs = 0; runs < numRuns; ++runs) {
        
        SPPriorityQueueInit(localQueue, minNumElems * numThreads + MAX(minNumElems, numThreads));
        
        dispatch_group_t group = dispatch_group_create();
        dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestQueue", DISPATCH_QUEUE_CONCURRENT);
        
        dispatch_suspend(queue);
        
        for (int iter = 1; iter <= numThreads; ++iter) {
            dispatch_group_async(group, queue, ^{
                [self fillQueueWithOrderedElements:localQueue
                                      startingFrom:1
                                              upTo:minNumElems * iter];
            });
        }
        
        dispatch_resume(queue);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        dispatch_group_async(group, queue, ^{
            [self extractOrderedElementsFromQueue:localQueue upTo:minNumElems * numThreads];
        });
        
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        XCTAssertTrue(SPPriorityQueueExtractMinimumElement(localQueue, 0) == NULL,
                      @"Queue still holds elements after extracting everything from it.");
        
        SPPriorityQueueDispose(localQueue);
    }
}

- (void)testHandlesParallelInsert
{
    SPPriorityQueue localQueue;
    [self performParallelInserts:&localQueue
                minNumberOfElems:256
                 numberOfThreads:16
                    numberOfRuns:2];
}

- (void)testHandlesInsertOnManyThreads
{
    SPPriorityQueue localQueue;
    [self performParallelInserts:&localQueue
                minNumberOfElems:2
                 numberOfThreads:512
                    numberOfRuns:1];
}


- (void)testHandlesParallelDeletions
{
    for (int reps = 0; reps < 2000; ++reps) {
        __block SPPriorityQueue localQueue;
        __block SPPriorityQueue resultQueue;
        
        const size_t totalNumElems = 16;
        const size_t numDelThreads = 32;
        XCTAssertTrue(SPPriorityQueueInit(&localQueue, totalNumElems));
        XCTAssertTrue(SPPriorityQueueInit(&resultQueue, totalNumElems));
        
        
        [self fillQueueWithOrderedElements:&localQueue
                              startingFrom:1
                                      upTo:totalNumElems];
        
        
        dispatch_group_t group = dispatch_group_create();
        dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestQueue", DISPATCH_QUEUE_CONCURRENT);
        
        dispatch_suspend(queue);
        
        // Delete and move to results.
        for (int iter = 1; iter <= numDelThreads; ++iter) {
            
            dispatch_group_async(group, queue, ^{
                
                // Retrieve the elements.
                test_elem_t prevElem = { .key = DBL_MIN, .data = 0 };
                for (int numElem = 0; numElem < totalNumElems * 2; ++numElem) {
                    test_elem_t retrieveElem;
                    
                    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&localQueue, &retrieveElem.key);
                    if (retrieveElem.data) {
                        SPPriorityQueueInsertElement(&resultQueue, retrieveElem.key, retrieveElem.data);
                    
                        XCTAssertTrue(prevElem.key <= retrieveElem.key, @"The queue does not enforce priorities");
                        prevElem.key = retrieveElem.key;
                    }
                }
            });
        }
        
        dispatch_resume(queue);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        [self extractOrderedElementsFromQueue:&resultQueue upTo:totalNumElems];
        
        XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                      @"Queue still holds elements after extracting everything from it.");
        XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&resultQueue, 0) == NULL,
                      @"Queue still holds elements after extracting everything from it.");
        SPPriorityQueueDispose(&localQueue);
        SPPriorityQueueDispose(&resultQueue);
    }
}


- (void)runParallelInsertAndDeleteForNumberOfElems:(size_t)numElems
                                   numberOfThreads:(size_t)numThreads
                                      numberOfRuns:(size_t)numRuns
{
    //__block SPPriorityQueue localQueue;
    SPPriorityQueueDispose(&_pqueue);
    for (int reps = 0; reps < numRuns; ++reps) {
        fprintf(stderr, ".");
        const size_t totalNumElems = numElems;
        XCTAssertTrue(SPPriorityQueueInit(&_pqueue, totalNumElems * numThreads));
        
        
        dispatch_group_t group = dispatch_group_create();
        dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestQueue", DISPATCH_QUEUE_CONCURRENT);
        
        dispatch_suspend(queue);
        
        // Insert and delete.
        for (int iter = 1; iter <= numThreads; ++iter) {
            
            dispatch_group_async(group, queue, ^{
                
                [self fillQueueWithOrderedElements:&_pqueue
                                      startingFrom:1
                                              upTo:totalNumElems];
            });
            dispatch_group_async(group, queue, ^{
                
                // Retrieve the elements.
                for (int numElem = 0; numElem < totalNumElems * 2; ++numElem) {
                    test_elem_t retrieveElem;
                    
                    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&_pqueue, &retrieveElem.key);
                }
            });
        }
        
        dispatch_resume(queue);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        SPPriorityQueueDispose(&_pqueue);
    }
    SPPriorityQueueInit(&_pqueue, 4096);
    fprintf(stderr, "\n");
}


- (void)testParallelInsertAndDelete
{
    [self runParallelInsertAndDeleteForNumberOfElems:5 numberOfThreads:2 numberOfRuns:1000];
    [self runParallelInsertAndDeleteForNumberOfElems:512 numberOfThreads:64 numberOfRuns:10];
}



- (void)testHandlesHeavyParallelInsertionsAndDeletions
{
    int minNumElems = 256;
    const int numThreads = 32;
    const int numBands = 4;
    
    minNumElems = floor(minNumElems / numBands) * numBands;
    
    __block SPPriorityQueue localQueue;
    __block SPPriorityQueue resultQueue;

    SPPriorityQueueInit(&resultQueue, minNumElems * numThreads + MAX(minNumElems, numThreads));
    SPPriorityQueueInit(&localQueue, minNumElems * numThreads + MAX(minNumElems, numThreads));
    
    
    dispatch_group_t group = dispatch_group_create();
    dispatch_queue_t queue = dispatch_queue_create("com.pzhivkov.concurrentTestQueue", DISPATCH_QUEUE_CONCURRENT);
    
    dispatch_suspend(queue);
    
    // Insert.
    const int bandSize = minNumElems * numThreads / numBands;
    for (int band = 0; band < numBands; ++band) {
        
        for (int iter = 1; iter <= numThreads; ++iter) {
            dispatch_group_async(group, queue, ^{
                [self fillQueueWithOrderedElements:&localQueue
                                      startingFrom:band * bandSize + 1
                                              upTo:band * bandSize + (minNumElems / numBands) * iter];
            });
        }
    }
    
    // Delete and move to results.
    for (int band = 0; band < numBands; ++band) {
        for (int iter = 1; iter <= numThreads; ++iter) {
            
            dispatch_group_async(group, queue, ^{
                
                // Retrieve an element and insert it in the result queue.
                for (int numElem = 1; numElem <= (minNumElems / numBands) * iter; ++numElem) {
                    test_elem_t retrieveElem;
                    
                    retrieveElem.data = SPPriorityQueueExtractMinimumElement(&localQueue, &retrieveElem.key);
                    if (retrieveElem.data)
                        SPPriorityQueueInsertElement(&resultQueue, retrieveElem.key, retrieveElem.data);
                }
            });
        }
    }
    
    dispatch_resume(queue);
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    
    dispatch_group_async(group, queue, ^{
        [self extractOrderedElementsFromQueue:&resultQueue upTo:minNumElems * numThreads];
    });
    
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
  
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&localQueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
    XCTAssertTrue(SPPriorityQueueExtractMinimumElement(&resultQueue, 0) == NULL,
                  @"Queue still holds elements after extracting everything from it.");
    
    SPPriorityQueueDispose(&localQueue);
    SPPriorityQueueDispose(&resultQueue);
}


@end
