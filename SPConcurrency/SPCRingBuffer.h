//
//  SPCRingBuffer.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 05/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//


#ifndef PZ_SPCRingBuffer_h
#define PZ_SPCRingBuffer_h


#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "SPCPrimitives.h"



/**
 *  A wait-free ring buffer structure. 
 *
 *  Guaranteed to be thread-safe and reentrant in an SPSC (single-producer, single-consumer) model.
 */

struct SPCRingBuffer {
    void            *_buffer;
    size_t           _length;
    size_t           _tailOffset;
    size_t           _headOffset;
    volatile size_t  _fillCount;
};

typedef struct SPCRingBuffer SPCRingBuffer;



/**
 *  Init a ring buffer.
 *
 *  @param buffer A pointer to the buffer.
 *  @param length The buffer length. (More memory may actually be allocated.)
 *
 *  @return true if successful; false, otherwise.
 */
bool SPCRingBufferInit(SPCRingBuffer *buffer, size_t length);


/**
 *  Dispose of a ring buffer.
 *
 *  @param buffer A pointer to a ring buffer.
 */
void SPCRingBufferDispose(SPCRingBuffer *buffer);


/**
 *  Clear a ring buffer.
 *
 *  @param buffer A pointer to a ring buffer.
 */
void SPCRingBufferClear(SPCRingBuffer *buffer);


/**
 *  Access the buffer tail for reading.
 *
 *  @param buffer         A pointer to a ring buffer.
 *  @param availableBytes The number of bytes ready for reading.
 *
 *  @return A pointer to the chunk of data available for reading; NULL if the buffer is empty.
 */
static FORCE_INLINE void *SPCRingBufferGetForRead(SPCRingBuffer *buffer, size_t *availableBytes)
{
    assert(buffer && availableBytes);
    
    *availableBytes = (size_t)(SPC_ATOMIC_LOAD(&buffer->_fillCount));
    if (*availableBytes)
        return (void *)((char *)(buffer->_buffer + buffer->_tailOffset));
    
    return NULL;
}


/**
 *  Mark a chunk of bytes in a buffer as read, making them available for writing.
 *
 *  @param buffer A pointer to a ring buffer.
 *  @param amount The number of bytes read.
 */
static FORCE_INLINE void SPCRingBufferMarkRead(SPCRingBuffer *buffer, size_t bytesRead)
{
    assert(buffer);
    
    buffer->_tailOffset = (buffer->_tailOffset + bytesRead) % buffer->_length;
    SPC_MEMORY_BARRIER_STORE();
    SPC_ATOMIC_FETCH_AND_ADD(&buffer->_fillCount, -bytesRead);
    
    assert((size_t)(SPC_ATOMIC_LOAD(&buffer->_fillCount)) >= 0);
}


/**
 *  Access the head of the buffer for writing.
 *
 *  @param buffer         A pointer to a ring buffer.
 *  @param availableBytes The number of bytes available for writing.
 *
 *  @return A pointer to the chunk of data available for writing; NULL if the buffer is full.
 */
static FORCE_INLINE void *SPCRingBufferGetForWrite(SPCRingBuffer *buffer, size_t *availableBytes)
{
    assert(buffer && availableBytes);
    
    *availableBytes = (buffer->_length - (size_t)(SPC_ATOMIC_LOAD(&buffer->_fillCount)));
    if (*availableBytes)
        return (void *)((char *)(buffer->_buffer + buffer->_headOffset));
    
    return NULL;
}


/**
 *  Mark a chunk of bytes in a buffer as written, making them available for reading.
 *
 *  @param buffer       A pointer to a ring buffer.
 *  @param bytesWritten The number of bytes written.
 */
static FORCE_INLINE void SPCRingBufferMarkWritten(SPCRingBuffer *buffer, size_t bytesWritten)
{
    assert(buffer);
    
    buffer->_headOffset = (buffer->_headOffset + bytesWritten) % buffer->_length;
    SPC_MEMORY_BARRIER_STORE();
    SPC_ATOMIC_FETCH_AND_ADD(&buffer->_fillCount, bytesWritten);
    
    assert((size_t)(SPC_ATOMIC_LOAD(&buffer->_fillCount)) <= buffer->_length);
}



#endif
