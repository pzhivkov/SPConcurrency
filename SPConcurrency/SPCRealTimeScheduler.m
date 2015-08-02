//
<<<<<<< HEAD:SPConcurrency/SPRealTimeScheduler.m
//  SPRealTimeScheduler.m
//  Peter Zhivkov.
=======
//  SPCRealTimeScheduler.m
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCRealTimeScheduler.m
//
//  Created by Peter Zhivkov on 27/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#import "SPCRealTimeScheduler.h"

#import "SPCMessageQueue.h"
#import "SPCPriorityQueue.h"



/**
 *  Total number of items that can be scheduled.
 */
static const size_t kDefaultSchedulerQueueSize = 86400;



/**
 *  Scheduler control data. Used to repeat a block.
 */
struct sched_control_data_t {
    UInt64         time_between_reps;
    unsigned long  num_reps;
    void          *execution_block;
};

typedef struct sched_control_data_t sched_control_data_t;



@interface SPCRealTimeScheduler ()

@property (nonatomic)       SPCPriorityQueue  schedulerQueue;
@property (nonatomic, weak) SPCMessageQueue  *responseQueue;
@property (nonatomic)       size_t            queueSize;

@end




@implementation SPCRealTimeScheduler



#pragma mark - Initialization



- (instancetype)initWithResponseQueue:(SPCMessageQueue *)responseQueue
{
    return [self initWithSize:kDefaultSchedulerQueueSize responseQueue:responseQueue];
}


/**
 *  Initialize a scheduler with a scheduling queue size and a response queue.
 *
 *  @param queueSize Determines the maximum number of blocks that can be scheduled.
 *  @param responseQueue A corresponding message queue on the main thread to do memory handling and execute response callbacks.
 */
- (instancetype)initWithSize:(size_t)queueSize responseQueue:(SPCMessageQueue *)responseQueue;
{
    self = [super init];
    if (!self) return nil;
    
    _hostBaseTime  = 0;
    _responseQueue = responseQueue;
    _queueSize     = queueSize;
    
    SPCPriorityQueueInit(&_schedulerQueue, queueSize);
    
    return self;
}


/**
 *  Reset the scheduler queue.
 */
- (void)reset
{
    SPCPriorityQueueDispose(&_schedulerQueue);
    SPCPriorityQueueInit(&_schedulerQueue, _queueSize);
}


- (void)dealloc
{
    SPCPriorityQueueDispose(&_schedulerQueue);
}



#pragma mark - Block scheduling



static void SPExecuteBlockHandler(void *refCon, size_t refConSize)
{
    assert(refCon && refConSize == sizeof(void *));
    
    void *responseBlockPtr = *(void **)refCon;
    if (responseBlockPtr) {
        __unsafe_unretained void (^responseBlock)(void) = (__bridge void (^)(void))responseBlockPtr;
        if (responseBlock)
            responseBlock();
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
                 responseBlock:(void (^)(void))responseBlock
{
    return [self scheduleBlockAfterTime:relativeTime
                            repetitions:1
                      delayBeforeRepeat:0
                                  block:block
                          responseBlock:responseBlock];
}


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
                 responseBlock:(void (^)(void))responseBlock
{
    if (repetitions == 0) {
        DLog(@"Scheduled block for 0 repetitions.");
        return YES;
    }
    
    NSAssert(sizeof(SPCPriorityQueueKey) == sizeof(UInt64),
             @"Scheduler timing information can't be encoded as a priority queue key.");
    
    //
    // Retain the response block if available.
    //
    void *responseBlockPtr = NULL;
    if (responseBlock) {
        responseBlockPtr = (void *)CFBridgingRetain(responseBlock);
    }
    
    //
    // Construct the block to execute on the real-time thread.
    //
    SPCRealTimeSchedulerBlock executionBlock = ^(UInt64 inIntervalStart, UInt64 inTimeOffset, BOOL isLastRepetition) {
        
        // Execute the main block.
        if (block)
            block(inIntervalStart, inTimeOffset, isLastRepetition);
        
        // Dispatch the response block to the main thread.
        void *responseBlockPtrHolder = responseBlockPtr;
        SPCMessageQueueDispatch(_responseQueue,
                               isLastRepetition ? SPExecuteAndReleaseBlockHandler : SPExecuteBlockHandler,
                               &responseBlockPtrHolder,
                               sizeof(void *));
    };
    
    //
    // Create a scheduler control data, and initialize it with the block.
    //
    sched_control_data_t *controlData = calloc(1, sizeof(sched_control_data_t));
    controlData->execution_block      = (void *)CFBridgingRetain(executionBlock);
    controlData->num_reps             = repetitions;
    controlData->time_between_reps    = repetitionWaitTime * (1.0 / SPUMachHostTicksToSeconds());
    
    //
    // Put the block on the scheduler queue.
    //
    SPCPriorityQueueKey  key  = relativeTime * (1.0 / SPUMachHostTicksToSeconds());
    void                *data = controlData;
    
    BOOL scheduled = SPCPriorityQueueInsertElement(&_schedulerQueue, key, data);
    if (!scheduled) {
        DLog(@"Failed to schedule block");
        
        // Release resources.
        //
        CFBridgingRelease(controlData->execution_block);
        CFBridgingRelease(responseBlockPtr);
        free(controlData);
    }
    
    return scheduled;
}



#pragma mark - Scheduler callback



static void SPReleaseSchedulerControlDataHandler(void *refCon, size_t refConSize)
{
    assert(refCon && refConSize == sizeof(sched_control_data_t *));
    
    sched_control_data_t *controlData = *(sched_control_data_t **)refCon;
    if (controlData) {
        if (controlData->execution_block)
            CFBridgingRelease(controlData->execution_block);
        
        free(controlData);
    }
}


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
                                            UInt64  inIntervalEnd)
{
    SPCRealTimeScheduler *this = (__bridge SPCRealTimeScheduler *)inRTSchedulerPtr;
    assert(this);
    
    //
    // Calculate the end time and peek at the top item of the scheduler queue.
    //

    UInt64 relativeEndTime = inIntervalEnd - this->_hostBaseTime;
    
    UInt64 relativeTime = 0;
    void  *data         = SPCPriorityQueuePeek_MPSC(&this->_schedulerQueue, &relativeTime);
    
    while (data && relativeTime <= relativeEndTime) {
        
        //
        // Extract the control data from the scheduler queue.
        //
        data = SPCPriorityQueueExtractMinimumElement(&this->_schedulerQueue, &relativeTime);
        
        //
        // Compute the time offset, and then execute the block.
        //
        UInt64 eventTime  = this->_hostBaseTime + relativeTime;
        UInt64 timeOffset = (eventTime > inIntervalStart) ? (eventTime - inIntervalStart) : 0;

        sched_control_data_t *controlData = (sched_control_data_t *)data;
        __unsafe_unretained SPCRealTimeSchedulerBlock executionBlock = (__bridge SPCRealTimeSchedulerBlock)(controlData->execution_block);
        if (executionBlock) {
            
            // Run the block.
            //
            BOOL isLastRepetition = (--controlData->num_reps <= 0);
            executionBlock(inIntervalStart, timeOffset, isLastRepetition);

            if (!isLastRepetition) {
                // If the block requires repeated executions, schedule the next one.
                //
                SPCPriorityQueueInsertElement(&this->_schedulerQueue, relativeTime + controlData->time_between_reps, controlData);
            } else {
                // Release the block on the main thread.
                //
                SPCMessageQueueDispatch(this->_responseQueue, SPReleaseSchedulerControlDataHandler, &controlData, sizeof(void *));
            }
        }
        
        //
        // Take a peek at the next one.
        //
        data = SPCPriorityQueuePeek_MPSC(&this->_schedulerQueue, &relativeTime);
    }
    
    return noErr;
}




@end
