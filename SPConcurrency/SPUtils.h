//
//  SPUtils.h
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 5/10/13.
//  Copyright (c) 2013 Peter Zhivkov. All rights reserved.
//

#ifndef PZ_SPUtils_h
#define PZ_SPUtils_h


#include <MacTypes.h>
#include <pthread.h>

#ifndef DEBUG
#  ifndef NDEBUG
#    define NDEBUG
#  endif
#  ifndef NS_BLOCK_ASSERTIONS
#    define NS_BLOCK_ASSERTIONS
#  endif
#endif

#include <assert.h>


#ifdef DEBUG
# ifdef __OBJC__
#  define DLog(fmt, ...) NSLog((@"%s:%d:%s: " fmt), strrchr(__FILE__, '/') + 1, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
# else
#  define DLog(fmt, ...)                                                                                                   \
   do {                                                                                                                    \
       mach_port_t tid = pthread_mach_thread_np(pthread_self());                                                           \
       fprintf(stderr, ("%s:%d:[%d]:%s: " fmt "\n"), strrchr(__FILE__, '/') + 1, __LINE__, tid,  __func__, ##__VA_ARGS__); \
   } while (0)
# endif
#else
# define DLog(...)
#endif

#ifdef DEBUG
# define ULog(fmt, ...)                                                                                                            \
  do {                                                                                                                             \
      UIAlertView *alert = [[UIAlertView alloc] initWithTitle:[NSString stringWithFormat:@"%s:%d:%s:", strrchr(__FILE__, '/') + 1, \
                                                                                                      __LINE__,                    \
                                                                                                      __PRETTY_FUNCTION__]         \
                                                      message:[NSString stringWithFormat:fmt, ##__VA_ARGS__]                       \
                                                     delegate:nil                                                                  \
                                            cancelButtonTitle:@"Ok"                                                                \
                                            otherButtonTitles:nil];                                                                \
      [alert show];                                                                                                                \
  } while (0)
#else
# define ULog(...)
#endif


// ALog always displays output regardless of the DEBUG setting.
#define ALog(fmt, ...) NSLog((@"%s:%d:%s: " fmt), strrchr(__FILE__, '/') + 1, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)


#define mustOverride()                                                                                                                      \
    @throw [NSException exceptionWithName:NSInvalidArgumentException                                                                        \
                                   reason:[NSString stringWithFormat:@"%s must be overridden in a subclass/category.", __PRETTY_FUNCTION__] \
                                 userInfo:nil]

#define methodNotImplemented() mustOverride()



// Color from RGB value.
//
#define UIColorFromRGBA(rgbaValue)                                           \
    [UIColor colorWithRed:(float)(((rgbaValue) & 0xFF000000) >> 24) / 255.0  \
                    green:(float)(((rgbaValue) & 0x00FF0000) >> 16) / 255.0  \
                     blue:(float)(((rgbaValue) & 0x0000FF00) >>  8) / 255.0  \
                    alpha:(float)(((rgbaValue) & 0x000000FF)      ) / 255.0]

#define UIColorFromRGB(rgbValue) UIColorFromRGBA(((rgbValue << 8) | 0xFF))



//
// OSStatus handling.
//

#define HANDLE_ERROR_AND_CLEANUP(operation, result, cleanupBlock)                                       \
do {                                                                                                    \
    OSStatus __status = (result);                                                                       \
    SPHandleOSStatus(__status, (operation), __PRETTY_FUNCTION__, strrchr(__FILE__, '/') + 1, __LINE__); \
    if (__status != noErr) { cleanupBlock; }                                                            \
} while (0)

#define HANDLE_ERROR_AND_RETURN(operation, result, returnValue) HANDLE_ERROR_AND_CLEANUP(operation, result, { return (returnValue); })

#define HANDLE_ERROR_AND_RETURN_VOID(operation, result) HANDLE_ERROR_AND_CLEANUP(operation, result, { return; })

#define HANDLE_ERROR(operation, result) HANDLE_ERROR_AND_CLEANUP(operation, result, {})

void SPHandleOSStatus(OSStatus error, const char *operation, const char* func, const char* file, int line);


//
// kern_return_t handling.
//

#define STD_OUTPUT_ERROR(operation, errorString) \
    (fprintf(stderr, "%s:%d:%s: Error in %s (%s)\n", strrchr(__FILE__, '/') + 1, __LINE__, __PRETTY_FUNCTION__, (operation), (errorString)))

#define HANDLE_KERN_ERROR_AND_CLEANUP(operation, result, cleanupBlock)                                          \
do {                                                                                                            \
    kern_return_t __return = (result);                                                                          \
    if (__return != KERN_SUCCESS) { STD_OUTPUT_ERROR((operation), mach_error_string(__return)); cleanupBlock; } \
} while (0)

#define HANDLE_KERN_ERROR_AND_RETURN(operation, result, returnValue) HANDLE_KERN_ERROR_AND_CLEANUP(operation, result, { return (returnValue); })

#define HANDLE_KERN_ERROR(operation, result) HANDLE_KERN_ERROR_AND_CLEANUP(operation, result, {})


//
// Misc.
//

double SPUMachHostTicksToSeconds();


#define IS_PTR_ALIGNED_TO(ptr, byte_count) (((uintptr_t)(const void *)(ptr)) % (byte_count) == 0)

#define IS_PTR_ALIGNED(ptr) IS_PTR_ALIGNED_TO(ptr, sizeof(const void *))

#define COMPILER_BARRIER() asm volatile ("" : : : "memory")


#endif
