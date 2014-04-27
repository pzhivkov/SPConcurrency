//
//  SPMessageQueue.m
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 06/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import "SPMessageQueue.h"

#ifndef DEBUG
#define NDEBUG
#endif

#import <assert.h>
#import <mach/mach.h>

#import "SPRingBuffer.h"



#pragma mark - Private interfaces

/**
 *  A message queue execution thread used to take messages from the queue 
 *  and post them for execution to the main thread.
 */
@interface SPMessageQueueExecutionThread : NSThread
{
@public
    semaphore_t _semaphore;
}

- (instancetype)initWithName:(NSString *)name
                       queue:(SPMessageQueue *)messageQueue;

@end



#pragma mark - Message Queue



@interface SPMessageQueue ()

@property (nonatomic)         SPRingBuffer                   messageBuffer;
@property (nonatomic, strong) SPMessageQueueExecutionThread *executionThread;

@property (nonatomic, copy)   NSString *name;

@end



@implementation SPMessageQueue



#pragma mark - Initialization



static const int32_t kMessageBufferSize = 16384;


- (instancetype)initWithName:(NSString *)name
{
    self = [super init];
    if (!self) return nil;
    
    _name = name;
    _executionThread = [[SPMessageQueueExecutionThread alloc] initWithName:_name
                                                                     queue:self];
    
    bool successful = SPRingBufferInit(&_messageBuffer, kMessageBufferSize);
    if (!successful) return nil;
    
    return self;
}


- (void)dealloc
{
    SPRingBufferDispose(&_messageBuffer);
}



#pragma mark - Processing control



static const NSTimeInterval kThreadWaitTimeout = 0.01;


- (void)startQueue
{
    @synchronized (self.executionThread) {
        if (!self.executionThread) {
            self.executionThread = [[SPMessageQueueExecutionThread alloc] initWithName:self.name
                                                                                 queue:self];
            
            assert(self.executionThread);
            
            [self.executionThread start];
        }
    }
}


- (void)stopQueue
{
    @synchronized (self.executionThread) {
        if (self.executionThread) {
            [self.executionThread cancel];
            
            while ([self.executionThread isExecuting]) {
                
                HANDLE_KERN_ERROR("semaphore_signal", semaphore_signal(self.executionThread->_semaphore));
                
                [NSThread sleepForTimeInterval:kThreadWaitTimeout];
            }
            
            self.executionThread = nil;
        }
    }
}


- (BOOL)isRunning
{
    return [self.executionThread isExecuting];
}


/**
 *  The main processing logic of the message queue.
 */
- (void)flushQueue
{
    for (;;) {
        
        message_t *message = NULL;
        @synchronized (self) {
            message = [self copyLastMessage];
            if (!message)
                break;
        }
        
        if (message->handler) {
            
            // Run the handler.
            message->handler(message->userInfoLength > 0 ? message + 1 : NULL, message->userInfoLength);
        }
        
        free(message);
    }
}



#pragma mark - Messaging



struct message_t {
    SPMessageHandler  handler;
    size_t            userInfoLength;
};

typedef struct message_t message_t;



//
// Dispatch a message to a lock-free message queue.
//
void SPMessageQueueDispatch(SPMessageQueue *this, SPMessageHandler handler, void *userInfo, size_t userInfoLength)
{
    //
    // Write a message to the ring buffer.
    //
    size_t availableBytes;
    message_t *message = SPRingBufferGetForWrite(&this->_messageBuffer, &availableBytes);
    assert(availableBytes >= sizeof(message_t) + userInfoLength);
    
    memset(message, 0, sizeof(message_t));
    message->handler        = handler;
    message->userInfoLength = userInfoLength;

    if (userInfoLength > 0)
        memcpy(message + 1, userInfo, userInfoLength);
    
    SPRingBufferMarkWritten(&this->_messageBuffer, sizeof(message_t) + userInfoLength);
    
    //
    // Signal the processing thread.
    //
    if (this->_executionThread)
        HANDLE_KERN_ERROR("semaphore_signal",
                          semaphore_signal(this->_executionThread->_semaphore));
}


/**
 *  Check if there are messages to process.
 */
- (BOOL)hasPendingMessages
{
    size_t ignore;
    return SPRingBufferGetForRead(&_messageBuffer, &ignore) != NULL;
}


/**
 *  Create a copy of the last message in the queue.
 *
 *  @return A copuy of the last message
 */
- (message_t *)copyLastMessage
{
    size_t availableBytes = 0;
    message_t *buffer = SPRingBufferGetForRead(&_messageBuffer, &availableBytes);
    if (!buffer)
        return NULL;
    
    
    size_t messageLength = sizeof(message_t) + buffer->userInfoLength;
    message_t *message = malloc(messageLength);
    memcpy(message, buffer, messageLength);
    SPRingBufferMarkRead(&_messageBuffer, messageLength);
    
    return message;
}



@end



#pragma mark - Message queue execution thread



@interface SPMessageQueueExecutionThread ()

@property (nonatomic, weak) SPMessageQueue *messageQueue;

@end



@implementation SPMessageQueueExecutionThread



#pragma mark - Initialization



- (instancetype)initWithName:(NSString *)name
                       queue:(SPMessageQueue *)messageQueue
{
    self = [super init];
    if (!self) return nil;
    
    _messageQueue = messageQueue;
    
    self.name = name;
    
    HANDLE_KERN_ERROR_AND_RETURN("semaphore_create",
                                 semaphore_create(mach_task_self(), &_semaphore, SYNC_POLICY_FIFO, 0),
                                 nil);
    
    return self;
}


- (void)dealloc
{
    HANDLE_KERN_ERROR("semaphore_destroy",
                      semaphore_destroy(mach_task_self(), _semaphore));
}



#pragma mark - Message processing



- (void)main
{
    while (![self isCancelled]) {
        
        HANDLE_KERN_ERROR("semaphore_wait",
                          semaphore_wait(_semaphore));
        
        if ([_messageQueue hasPendingMessages]) {
            
            [_messageQueue performSelectorOnMainThread:@selector(flushQueue) withObject:nil waitUntilDone:NO];
        }
    }
}



@end
