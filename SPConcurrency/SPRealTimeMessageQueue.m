//
//  SPRealTimeMessageQueue.m
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 08/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import "SPRealTimeMessageQueue.h"

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <mach/mach.h>
#include <string.h>

#import "SPMessageQueue.h"
#import "SPRingBuffer.h"



#pragma mark - Real-time message queue


@interface SPRealTimeMessageQueue ()

@property (nonatomic, weak) SPMessageQueue *responseQueue;
@property (nonatomic)       SPRingBuffer    messageBuffer;

@end




@implementation SPRealTimeMessageQueue



#pragma mark - Initialization



static const int32_t kMessageBufferSize = 16384;



- (instancetype)initWithResponseQueue:(SPMessageQueue *)responseQueue
{
    self = [super init];
    if (!self) return nil;
    
    _responseQueue = responseQueue;
    
    bool successful = SPRingBufferInit(&_messageBuffer, kMessageBufferSize);
    if (!successful) return nil;
    
    return self;
}


- (void)dealloc
{
    SPRingBufferDispose(&_messageBuffer);
}



#pragma mark - Messaging



static void SPReleaseBlockHandler(void *refCon, size_t refConSize)
{
    assert(refCon && refConSize == sizeof(void *));
    
    void *blockPtr = *(void **)refCon;
    if (blockPtr) {
        CFBridgingRelease(blockPtr);
    }
}


//
// Proccess messages posted on the queue.
//
void SPRealTimeMessageQueueProccessMessages(SPRealTimeMessageQueue *this)
{
    
    size_t availableBytes;
    void **messagePtr = SPRingBufferGetForRead(&this->_messageBuffer, &availableBytes);
    void *end = (char *)messagePtr + availableBytes;
    
    while ((void *)messagePtr < (void *)end) {
        
        __unsafe_unretained void (^executionBlock)(void) = (__bridge void (^)(void))(*messagePtr);
        SPRingBufferMarkRead(&this->_messageBuffer, sizeof(void *));
    
        if (executionBlock) {
            
            // Run the block.
            //
            executionBlock();
            
            // Release the block on the main thread.
            //
            void *executionBlockToRelease = (__bridge void *)(executionBlock);
            SPMessageQueueDispatch(this->_responseQueue, SPReleaseBlockHandler, &executionBlockToRelease, sizeof(void *));
        }
        
        messagePtr++;
    }
}



static void SPExecuteAndReleaseBlockHandler(void *refCon, size_t refConSize)
{
    assert(refCon && refConSize == sizeof(void *));
    
    void *responseBlockPtr = *(void **)refCon;
    if (responseBlockPtr) {
        void (^responseBlock)(void) = CFBridgingRelease(responseBlockPtr);
        if (responseBlock)
            responseBlock();
    }
}


- (void)dispatchAsynchronously:(void (^)(void))block
                 responseBlock:(void (^)(void))responseBlock
{
    if (!block) return;
    
    @synchronized (self) {
        
        //
        // Retain the response block if available.
        //
        void *responseBlockPtr = NULL;
        if (responseBlock) {
            responseBlockPtr = (void *)CFBridgingRetain(responseBlock);
        }
        
        //
        // Construct the block to send to the real-time thread.
        //
        void (^executionBlock)(void) = ^{
        
            // Execute the main block.
            block();
            
            // Dispatch the response block to the main thread.
            void *responseBlockPtrHolder = responseBlockPtr;
            SPMessageQueueDispatch(_responseQueue, SPExecuteAndReleaseBlockHandler, &responseBlockPtrHolder, sizeof(void *));
            
        };
        void *executionBlockPtr = (void *)CFBridgingRetain(executionBlock);

        //
        // Write the block to the real-time thread.
        //
        size_t availableBytes;
        void **message = SPRingBufferGetForWrite(&_messageBuffer, &availableBytes);
        assert(availableBytes >= sizeof(void *));
        *message = executionBlockPtr;
        
        SPRingBufferMarkWritten(&_messageBuffer, sizeof(void *));
        
        //
        // If the response queue is not running, just do the exchange offline.
        //
        if (![self.responseQueue isRunning]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                SPRealTimeMessageQueueProccessMessages(self);
                [self.responseQueue flushQueue];
            });
        }
    }
}



static const NSTimeInterval kSynchronousResponseWaitTimeout = 1.00;
static const NSTimeInterval kThreadWaitTimeout              = 0.01;



- (void)dispatchSynchronously:(void (^)(void))block
{
    if (!block) return;
    
    //
    // Send asynchronous message with a completion notification response block.
    //
    __block BOOL finished = NO;
    [self dispatchAsynchronously:block responseBlock:^{
        finished = YES;
    }];

    
    //
    // Wait for the response.
    //
    uint64_t giveUpTime = mach_absolute_time() + (kSynchronousResponseWaitTimeout / SPUMachHostTicksToSeconds());
    while (!finished && mach_absolute_time() < giveUpTime) {
        if ([NSThread isMainThread]) {
            [self.responseQueue flushQueue];
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self.responseQueue flushQueue];
            });
        }
        if (finished) break;
        [NSThread sleepForTimeInterval:kThreadWaitTimeout];
    }
    
    if (!finished) {
        DLog(@"Timed out while waiting for response from the real-time thread.");
        @synchronized (self) {
            SPRealTimeMessageQueueProccessMessages(self);
            [self.responseQueue flushQueue];
        }
    }
}



@end
