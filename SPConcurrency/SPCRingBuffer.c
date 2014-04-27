//
<<<<<<< HEAD:SPConcurrency/SPRingBuffer.c
//  SPRingBuffer.c
//  Peter Zhivkov.
=======
//  SPCRingBuffer.c
//  Peter Zhivkov.
>>>>>>> 6af3d94... Namespace update.:SPConcurrency/SPCRingBuffer.c
//
//  Created by Peter Zhivkov on 05/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#include "SPCRingBuffer.h"

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <mach/mach.h>
#include <stdio.h>

#include "SPUtils.h"



bool SPCRingBufferInit(SPCRingBuffer *buffer, size_t length)
{
    assert(buffer);
    
    // Keep trying until we get the buffer (needed to handle race conditions).
    //
    int retries = 3;
    for (;;) {
        
        buffer->_length = (int32_t) round_page(length);    // Allocate whole page sizes.
        
        //
        // Temporarily allocate twice the length, so we have the contiguous address space to
        // support a second instance of the buffer directly after.
        //
        vm_address_t bufferAddress;
        HANDLE_KERN_ERROR_AND_CLEANUP("attempt buffer allocation",
                                      vm_allocate(mach_task_self(),
                                                  &bufferAddress,
                                                  buffer->_length * 2,
                                                  VM_FLAGS_ANYWHERE),
                                      {
                                          if (!retries--) {
                                              STD_OUTPUT_ERROR("buffer allocation", "FAILURE");
                                              return false;
                                          }
                                          continue; // Try again.
                                      });

        //
        // Now replace the second half of the allocation with a virtual copy of the first half.
        // Then deallocate the second half.
        //
        HANDLE_KERN_ERROR_AND_CLEANUP("buffer deallocation",
                                      vm_deallocate(mach_task_self(),
                                                    bufferAddress + buffer->_length,
                                                    buffer->_length),
                                      {
                                          if (!retries--) {
                                              STD_OUTPUT_ERROR("buffer deallocation", "FAILURE");
                                              return false;
                                          }
                                          
                                          // If this fails somehow, deallocate the whole region and try again.
                                          vm_deallocate(mach_task_self(), bufferAddress, buffer->_length);
                                          continue;
                                      });

        //
        // Re-map the buffer to the address space immediately after the buffer.
        //
        vm_address_t virtualAddress = bufferAddress + buffer->_length;
        vm_prot_t cur_prot, max_prot;
        HANDLE_KERN_ERROR_AND_CLEANUP("remap buffer memory",
                                      vm_remap(mach_task_self(),
                                               &virtualAddress,   // Mirror target.
                                               buffer->_length,   // Size of mirror.
                                               0,                 // Auto alignment
                                               0,                 // Force remapping to virtualAddress.
                                               mach_task_self(),  // Same task.
                                               bufferAddress,     // Mirror source.
                                               0,                 // MAP READ-WRITE, NOT COPY.
                                               &cur_prot,         // Unused protection struct.
                                               &max_prot,         // Unused protection struct.
                                               VM_INHERIT_DEFAULT),
                                      {
                                          if (!retries--) {
                                              STD_OUTPUT_ERROR("remap buffer memory", "FAILURE");
                                              return false;
                                          }
                                          
                                          // If this remap failed, we hit a race condition, so deallocate and try again.
                                          vm_deallocate(mach_task_self(), bufferAddress, buffer->_length);
                                          continue;
                                      });
        
        if (virtualAddress != bufferAddress + buffer->_length) {
            //
            // If the memory is not contiguous, clean up both allocated buffers and try again.
            if (!retries--) {
                STD_OUTPUT_ERROR("contiguous memory check", "FAILURE");
                return false;
            }
            
            vm_deallocate(mach_task_self(), virtualAddress, buffer->_length);
            vm_deallocate(mach_task_self(), bufferAddress,  buffer->_length);
            continue;
        }
        
        buffer->_buffer     = (void*) bufferAddress;
        buffer->_fillCount  = 0;
        buffer->_headOffset = buffer->_tailOffset = 0;
        
        return true;
    }
    
    return false;
}


void SPCRingBufferDispose(SPCRingBuffer *buffer)
{
    assert(buffer);
    
    vm_deallocate(mach_task_self(), (vm_address_t) buffer->_buffer, buffer->_length * 2);
    memset(buffer, 0, sizeof(SPCRingBuffer));
}


void SPCRingBufferClear(SPCRingBuffer *buffer)
{
    assert(buffer);
    
    size_t fillCount;
    if (SPCRingBufferGetForRead(buffer, &fillCount)) {
        SPCRingBufferMarkRead(buffer, fillCount);
    }
}
