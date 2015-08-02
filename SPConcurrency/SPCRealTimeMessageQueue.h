//
<<<<<<< HEAD:SPConcurrency/SPRealTimeMessageQueue.h
//  SPRealTimeMessageQueue.h
//  Peter Zhivkov.
=======
//  SPCRealTimeMessageQueue.h
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCRealTimeMessageQueue.h
//
//  Created by Peter Zhivkov on 08/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <Foundation/Foundation.h>


@class SPCMessageQueue;


/**
 *  A lock-less message queue intended for real-time processing.
 */
@interface SPCRealTimeMessageQueue : NSObject


+ (instancetype)new   __attribute__((unavailable("new not available, call designated initializer instead")));
- (instancetype)init  __attribute__((unavailable("init not available, call designated initializer instead")));


/**
*  Initialize a new real-time queue.
*
*  @param responseQueue A corresponding message queue on the main thread to do memory handling and execute response callbacks.
*
*  @return The queue if successful.
*/
- (instancetype)initWithResponseQueue:(SPCMessageQueue *)responseQueue NS_DESIGNATED_INITIALIZER;


/**
 *  Dispatch a block for asynchronous execution on the real-time thread, with a corresponding response block for the main thread.
 *
 *  @param block         The block to be executed on the real-time thread.
 *  @param responseBlock The response to be executed on the main thread (it will be sent to the response queue).
 */
- (void)dispatchAsynchronously:(void (^)(void))block
                 responseBlock:(void (^)(void))responseBlock;


/**
 *  Dispatch a block for synchronous execution on the real-time thread and wait for it to finish.
 *
 *  @param block The block to be executed on the real-time thread.
 */
- (void)dispatchSynchronously:(void (^)(void))block;


/**
 *  Proccess messages posted on the queue.
 *
 *  @param realTimeMessageQueue A real-time message queue.
 */
void SPCRealTimeMessageQueueProccessMessages(SPCRealTimeMessageQueue *realTimeMessageQueue);



@end
