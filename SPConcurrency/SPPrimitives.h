//
//  SPPrimitives.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 14/02/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//

#ifndef PZ_SPPrimitives_h
#define PZ_SPPrimitives_h

#include <unistd.h>


#define ARCH_PRIMITIVES



#define FORCE_INLINE inline __attribute__((always_inline))




#pragma mark - Concurrency primitives - Architecture



#ifdef ARCH_PRIMITIVES
#    if defined _ARM_ARCH_7 || defined __ARM_ARCH_7A__ || defined __ARM_ARCH_7S__
#        define ARM_PRIMITIVES
#    elif defined __x86_64__
#        define x86_64_PRIMITIVES
#    elif defined __i386__ && __POINTER_WIDTH__ == 32
#        define x86_PRIMITIVES
#    else
#        define GCC_PRIMITIVES
#    endif
#endif



#pragma mark - Concurrency primitives - ARM



#ifdef ARM_PRIMITIVES


static FORCE_INLINE uintptr_t SPC__ARM_atomic_load(const void *ptr)
{
    uintptr_t result;
    __asm__ __volatile__("ldr %0, [%1];"
                         : "=r" (result)
                         : "r"  (ptr)
                         : "memory");
    return result;
}

#define SPC_ATOMIC_LOAD(ptr) SPC__ARM_atomic_load((const void *)(ptr))



static FORCE_INLINE void SPC__ARM_atomic_store(void *ptr, const uintptr_t value)
{
    __asm__ __volatile__("str %1, [%0];"
                         :
                         : "r" (ptr),
                           "r" (value)
                         : "memory");
    return;
}

#define SPC_ATOMIC_STORE(ptr, value) SPC__ARM_atomic_store((void *)(ptr), (const uintptr_t)(value))




static FORCE_INLINE uintptr_t SPC__ARM_atomic_fetch_and_add(void *ptr, uintptr_t value)
{
    uintptr_t prev, r, tmp;
    
    __asm__ __volatile__("1:"
                         "ldrex %0, [%3];"
                         "add   %1, %4, %0;"
                         "strex %2, %1, [%3];"
                         "cmp   %2, #0;"
                         "bne   1b;"
                         : "=&r" (prev),
                           "=&r" (r),
                           "=&r" (tmp)
                         : "r"   (ptr),
                           "r"   (value)
                         : "memory", "cc");
    
    return prev;
}

#define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) SPC__ARM_atomic_fetch_and_add((void *)(ptr), (uintptr_t)(value))




static FORCE_INLINE bool SPC__ARM_atomic_compare_and_swap(void *ptr, uintptr_t oldValue, uintptr_t newValue)
{
    uintptr_t prev, tmp;
    __asm__ __volatile__("1:"
                         "ldrex   %0, [%2];"
                         "cmp     %0, %4;"
                         "itt     eq;"
                         "strexeq %1, %3, [%2];"
                         "cmpeq   %1, #1;"
                         "beq     1b;"
                         : "=&r" (prev),
                           "=&r" (tmp)
                         : "r"   (ptr),
                           "r"   (newValue),
                           "r"   (oldValue)
                         : "memory", "cc");
    return prev == oldValue;
}

#define SPC_ATOMIC_COMPARE_AND_SWAP(ptr, oldValue, newValue) \
    SPC__ARM_atomic_compare_and_swap((void *)(ptr), (uintptr_t)(oldValue), (uintptr_t)(newValue))



#define SPC_COMPILER_BARRIER()     __asm__ __volatile__(""          : :         : "memory")

#define SPC_MEMORY_BARRIER_FULL()  __asm__ __volatile__("dmb ish"   : : "r" (0) : "memory")
#define SPC_MEMORY_BARRIER_LOAD()  __asm__ __volatile__("dmb ish"   : : "r" (0) : "memory")
#define SPC_MEMORY_BARRIER_STORE() __asm__ __volatile__("dmb ishst" : : "r" (0) : "memory")

#define SPC_STALL()                __asm__ __volatile__(""          : :         : "memory")


#endif



#pragma mark - Concurrency primitives - x86



#ifdef x86_PRIMITIVES


static FORCE_INLINE int32_t SPC__x86_atomic_load(const int32_t *ptr)
{
    const int32_t result;
    __asm__ __volatile__("movl %1, %0"
                         : "=q" (result)
                         : "m"  (*(const int64_t *)ptr)
                         : "memory");
    return result;
}

#define SPC_ATOMIC_LOAD(ptr) SPC__x86_atomic_load((const int32_t *)(ptr))



static FORCE_INLINE void SPC__x86_atomic_store(int32_t *ptr, int32_t value)
{
    __asm__ __volatile__("movl %1, %0"
                         : "=m" (*(int32_t *)ptr)
                         : "q"  (value)
                         : "memory");
}

#define SPC_ATOMIC_STORE(ptr, value) SPC__x86_atomic_store((int32_t *)(ptr), (int32_t)(value))



static FORCE_INLINE int32_t SPC__x86_atomic_fetch_and_add(int32_t *ptr, int32_t value)
{
    __asm__ __volatile__("lock xaddl %1, %0"
                         : "+m" (*(char *)ptr),
                           "+q" (value)
                         :
                         : "memory", "cc");
    return value;
}

#define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) SPC__x86_atomic_fetch_and_add((int32_t *)(ptr), (int32_t)(value))



static FORCE_INLINE bool SPC__x86_atomic_compare_and_swap(int32_t *ptr, int32_t oldValue, int32_t newValue)
{
    bool result;
    __asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
                         : "+m"  (*(char *)ptr),
                           "=a"  (result)
                         : "q"   (newValue),
                           "a"   (oldValue)
                         : "memory", "cc");
    return result;
}

#define SPC_ATOMIC_COMPARE_AND_SWAP(ptr, oldValue, newValue) \
    SPC__x86_atomic_compare_and_swap((int32_t *)(ptr), (int32_t)(oldValue), (int32_t)(newValue))



#define SPC_COMPILER_BARRIER()     __asm__ __volatile__(""       : : : "memory")

#define SPC_MEMORY_BARRIER_FULL()  __asm__ __volatile__("mfence" : : : "memory")
#define SPC_MEMORY_BARRIER_LOAD()  __asm__ __volatile__("lfence" : : : "memory")
#define SPC_MEMORY_BARRIER_STORE() __asm__ __volatile__("sfence" : : : "memory")

#define SPC_STALL()                __asm__ __volatile__("pause"  : : : "memory")


#endif



#pragma mark - Concurrency primitives - x86_64



#ifdef x86_64_PRIMITIVES


static FORCE_INLINE int64_t SPC__x86_64_atomic_load(const int64_t *ptr)
{
    const int64_t result;
    __asm__ __volatile__("movq %1, %0"
                         : "=q" (result)
                         : "m"  (*(const int64_t *)ptr)
                         : "memory");
    return result;
}

#define SPC_ATOMIC_LOAD(ptr) SPC__x86_64_atomic_load((const int64_t *)(ptr))



static FORCE_INLINE void SPC__x86_64_atomic_store(int64_t *ptr, int64_t value)
{
    __asm__ __volatile__("movq %1, %0"
                         : "=m" (*(int64_t *)ptr)
                         : "q"  (value)
                         : "memory");
}

#define SPC_ATOMIC_STORE(ptr, value) SPC__x86_64_atomic_store((int64_t *)(ptr), (int64_t)(value))



static FORCE_INLINE int64_t SPC__x86_64_atomic_fetch_and_add(int64_t *ptr, int64_t value)
{
    __asm__ __volatile__("lock xaddq %1, %0"
                         : "+m" (*(char *)ptr),
                           "+q" (value)
                         :
                         : "memory", "cc");
    return value;
}

#define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) SPC__x86_64_atomic_fetch_and_add((int64_t *)(ptr), (int64_t)(value))




static FORCE_INLINE bool SPC__x86_64_atomic_compare_and_swap(int64_t *ptr, int64_t oldValue, int64_t newValue)
{
    bool result;
    __asm__ __volatile__("lock cmpxchgq %2, %0; setz %1"
                         : "+m"  (*(char *)ptr),
                           "=a"  (result)
                         : "q"   (newValue),
                           "a"   (oldValue)
                         : "memory", "cc");
    return result;
}

#define SPC_ATOMIC_COMPARE_AND_SWAP(ptr, oldValue, newValue) \
    SPC__x86_64_atomic_compare_and_swap((int64_t *)(ptr), (int64_t)(oldValue), (int64_t)(newValue))



#define SPC_COMPILER_BARRIER()     __asm__ __volatile__(""       : : : "memory")

#define SPC_MEMORY_BARRIER_FULL()  __asm__ __volatile__("mfence" : : : "memory")
#define SPC_MEMORY_BARRIER_LOAD()  __asm__ __volatile__("lfence" : : : "memory")
#define SPC_MEMORY_BARRIER_STORE() __asm__ __volatile__("sfence" : : : "memory")

#define SPC_STALL()                __asm__ __volatile__("pause"  : : : "memory")


#endif



#pragma mark - Concurrency primitives - GCC



#ifdef GCC_PRIMITIVES


#define SPC_ATOMIC_LOAD(ptr)         ((void *)(*(volatile typeof(ptr) *)&(*(typeof(ptr) *)(ptr))))
#define SPC_ATOMIC_STORE(ptr, value) ((*(volatile typeof(ptr) *)&(*(typeof(ptr) *)(ptr))) = (typeof(ptr))(value))


#if defined __LP64__
#    define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) __sync_fetch_and_add((int64_t *)(ptr), (int64_t)(value))
#elif __POINTER_WIDTH__ == 32
#    define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) __sync_fetch_and_add((int32_t *)(ptr), (int32_t)(value))
#else
#    error "Unsupported data model."
#endif


#define SPC_ATOMIC_COMPARE_AND_SWAP(ptr, oldValue, newValue) \
    __sync_bool_compare_and_swap((uintptr_t *)(ptr), (uintptr_t)(oldValue), (uintptr_t)(newValue))


#define SPC_COMPILER_BARRIER()     __asm__ __volatile__("" : : : "memory")

#define SPC_MEMORY_BARRIER_FULL()  __sync_synchronize()
#define SPC_MEMORY_BARRIER_LOAD()  __sync_synchronize()
#define SPC_MEMORY_BARRIER_STORE() __sync_synchronize()

#define SPC_STALL()                __asm__ __volatile__("" : : : "memory")


#endif



#pragma mark - Concurrency primitives - DARWIN



#ifdef DARWIN_PRIMITIVES


#include <libkern/OSAtomic.h>


#define SPC_ATOMIC_LOAD(ptr)         ((void *)(*(volatile typeof(ptr) *)&(*(typeof(ptr) *)(ptr))))
#define SPC_ATOMIC_STORE(ptr, value) ((*(volatile typeof(ptr) *)&(*(typeof(ptr) *)(ptr))) = (typeof(ptr))(value))


#if defined __LP64__
#    define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) (OSAtomicAdd64((int64_t)(value), (volatile int64_t *)(ptr)) - (int64_t)(value))
#elif __POINTER_WIDTH__ == 32
#    define SPC_ATOMIC_FETCH_AND_ADD(ptr, value) (OSAtomicAdd32((int32_t)(value), (volatile int32_t *)(ptr)) - (int32_t)(value))
#else
#    error "Unsupported data model."
#endif


#define SPC_ATOMIC_COMPARE_AND_SWAP(ptr, oldValue, newValue) \
    OSAtomicCompareAndSwapPtr((void *)(oldValue), (void *)(newValue), (void *volatile *)(ptr))


#define SPC_COMPILER_BARRIER()     __asm__ __volatile__("" : : : "memory")

#define SPC_MEMORY_BARRIER_FULL()  OSMemoryBarrier()
#define SPC_MEMORY_BARRIER_LOAD()  OSMemoryBarrier()
#define SPC_MEMORY_BARRIER_STORE() OSMemoryBarrier()

#define SPC_STALL()                __asm__ __volatile__("" : : : "memory")


#endif




#pragma mark - Atomic markable pointer



typedef uintptr_t markable_ptr_t;



/**
 *  Check if the mark on a markable pointer is set.
 *
 *  @param ptr_m A markable pointer.
 *
 *  @return true if set; false; otherwise.
 */
static FORCE_INLINE bool isMarked_m(markable_ptr_t ptr_m)
{
    return (uintptr_t)(ptr_m) & 1;
}


/**
 *  Get a markable pointer.
 *
 *  @param ptr_m  A markable pointer.
 *  @param markOn A flag determining whether to set the mark to 0 or 1.
 *
 *  @return true if set; false; otherwise.
 */
static FORCE_INLINE markable_ptr_t toMarkable_m(markable_ptr_t ptr_m, bool markOn)
{
    return (markable_ptr_t)(markOn ? (uintptr_t)(ptr_m) | (uintptr_t)(1) : (uintptr_t)(ptr_m) & (uintptr_t)(~1));
}


/**
 *  Get a markable pointer.
 *
 *  @param ptr    A pointer.
 *  @param markOn A flag determining whether to set the mark to 0 or 1.
 *
 *  @return true if set; false; otherwise.
 */
static FORCE_INLINE markable_ptr_t toMarkable(void *ptr, bool markOn)
{
    return toMarkable_m((markable_ptr_t)ptr, markOn);
}


/**
 *  Convert a markable pointer to a normal dereferenceable pointer.
 *
 *  @param ptr_m A markable pointer.
 *
 *  @return An unmarked pointer.
 */
static FORCE_INLINE void *toPtr_m(markable_ptr_t ptr_m)
{
    return (void *)(toMarkable_m(ptr_m, false));
}


#define NULL_D toMarkable(NULL, true)



#pragma mark - Exponential backoff



#ifndef SPC_BACKOFF_CEILING
#define SPC_BACKOFF_CEILING ((1 << 20) - 1)
#endif


#define SPC_BACKOFF_INIT (1 << 6)


typedef volatile unsigned int spc_backoff_t;


/**
 *  Perform exponential backoff.
 *
 *  @param counter A back-off state counter.
 */
static FORCE_INLINE void SPC_BackoffExponential(spc_backoff_t *backOffCounter)
{
    unsigned int ceiling = *backOffCounter;

    usleep(ceiling);
    
    *backOffCounter = (ceiling <<= (ceiling < SPC_BACKOFF_CEILING));
    
    return;
}



#endif
