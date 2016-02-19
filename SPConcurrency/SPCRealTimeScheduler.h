//
//  SPCRealTimeScheduler.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 27/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import <Foundation/Foundation.h>


@class SPCMessageQueue;



/**
 *  Scheduler callback.
 *
 *  This is called to execute time-stamp based events that fall within the start and end points of a given time interval.
 *  As the callback expected to be called from a real-time thread, it should not wait on locks, allocate memory, 
 *  or call any Objective-C or BSD code.
 *
 *  @param inRTSchedulerPtr The real-time schedule.
 *  @param inIntervalStart  A starting point of an interval (given in host ticks).
 *  @param inIntervalEnd    An ending point of an interval (given in host ticks).
 *
 *  @return A status code
 */
OSStatus SPCRealTimeSchedulerInvokeCallback(void   *inRTSchedulerPtr,
                                            UInt64  inIntervalStart,
                                            UInt64  inIntervalEnd);

/**
 *  Scheduler block.
 *
 *  This will be executed in a real-time thread context, so it should not wait on locks,
 *  allocate memory, or call any Objective-C or BSD code.
 *
 *  @param inIntervalStart  The timestamp in host ticks corresponding to the start of this time interval.
 *  @param inTimeOffset     The offset, in ticks, of this schedule's fire timestamp into the current time interval.
 *  @param isLastRepetition A flag that notifies the callee that this will be the last call,
 *                          giving them the chance to release any resources held on the heap.
 */
typedef void (^SPCRealTimeSchedulerBlock)(UInt64 inIntervalStart, UInt64 inTimeOffset, BOOL isLastRepetition);


/**
 *  A real-time scheduler providing time-based event scheduling.
 */
@interface SPCRealTimeScheduler : NSObject


/**
 *  The host base time in host ticks.
 */
@property (nonatomic) UInt64 hostBaseTime;


+ (instancetype)new   __attribute__((unavailable("new not available, call designated initializer instead")));
- (instancetype)init  __attribute__((unavailable("init not available, call designated initializer instead")));


/**
 *  Initialize a scheduler with a response queue.
 *
 *  @param responseQueue A corresponding message queue on the main thread to do memory handling and execute response callbacks.
 *
 *  @return The scheduler if successful.
 */
- (instancetype)initWithResponseQueue:(SPCMessageQueue *)responseQueue;


/**
 *  Initialize a scheduler with a scheduling queue size and a response queue.
 *
 *  @param queueSize Determines the maximum number of blocks that can be scheduled.
 *  @param responseQueue A corresponding message queue on the main thread to do memory handling and execute response callbacks.
 *
 *  @return The scheduler if successful.
 */
- (instancetype)initWithSize:(size_t)queueSize responseQueue:(SPCMessageQueue *)responseQueue NS_DESIGNATED_INITIALIZER;


/**
 *  Reset the scheduler queue.
 */
- (void)reset;


/**
 *  Schedule a block for execution at a later time.
 *
 *  @param relativeTime  The time interval after which the block should be executed.
 *  @param block         A block.
 *  @param responseBlock A response block (optional).
 *
 *  @return YES if successful.
 */
- (BOOL)scheduleBlockAfterTime:(NSTimeInterval)relativeTime
                         block:(SPCRealTimeSchedulerBlock)block
                 responseBlock:(void (^)(void))responseBlock;

/**
 *  Schedule a block for execution at a later time.
 *
 *  @param relativeTime       The time interval after which the block should be executed.
 *  @param repetitions        Number of times the block is to be repeated.
 *  @param repetitionWaitTime Time to wait between repetitions.
 *  @param block              A block.
 *  @param responseBlock      A response block (optional). Will be executed after every repetition.
 *
 *  @return YES if successful.
 */
- (BOOL)scheduleBlockAfterTime:(NSTimeInterval)relativeTime
                   repetitions:(unsigned long)repetitions
             delayBeforeRepeat:(NSTimeInterval)repetitionWaitTime
                         block:(SPCRealTimeSchedulerBlock)block
                 responseBlock:(void (^)(void))responseBlock;



@end


