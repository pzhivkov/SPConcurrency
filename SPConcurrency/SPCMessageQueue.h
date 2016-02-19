//
//  SPCMessageQueue.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 06/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <stddef.h>



/**
 *  A lock-less message queue for transmitting and running execution blocks on the main thread.
 */
@interface SPCMessageQueue : NSObject


+ (instancetype)new   __attribute__((unavailable("new not available, call designated initializer instead")));
- (instancetype)init  __attribute__((unavailable("init not available, call designated initializer instead")));


/**
 *  Determine if the processing loop of the queue is running.
 */
@property (nonatomic, readonly, getter = isRunning) BOOL running;


/**
 *  Initialize a new queue with name.
 *
 *  @param name Reverse-DNS style name.
 *
 *  @return The queue if succesful.
 */
- (instancetype)initWithName:(NSString *)name NS_DESIGNATED_INITIALIZER;


/**
 *  Start the processing loop on the queue.
 */
- (void)startQueue;


/**
 *  Stop the processing of messages on the queue.
 */
- (void)stopQueue;


/**
 *  Flush the queue by processing any remaining messages offline (useful when the queue is not running).
 */
- (void)flushQueue;



/**
 *  A message handler function to be excuted on the main thread.
 */
typedef void (*SPCMessageHandler)(void *refCon, size_t refConSize);

/**
 *  Dispatch a message to a lock-free message queue.
 *
 *  @param messageQueue   The message queue.
 *  @param handler        The message handler.
 *  @param userInfo       A user info.
 *  @param userInfoLength The user info length.
 */
void SPCMessageQueueDispatch(SPCMessageQueue *messageQueue, SPCMessageHandler handler, void *userInfo, size_t userInfoLength);



@end
