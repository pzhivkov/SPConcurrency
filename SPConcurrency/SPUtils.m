//
//  SPUtils.c
//  Peter Zhivkov.
//
//  Created by Peter Zhivkov on 02/01/2014.
//  Copyright (c) 2014 Peter Zhivkov. All rights reserved.
//


#include <stdio.h>
#include <mach/mach_time.h>




double SPUMachHostTicksToSeconds()
{
    static double hostTicksToSeconds = 0.0;
    if (hostTicksToSeconds == 0.0) {
        mach_timebase_info_data_t tinfo;
        mach_timebase_info(&tinfo);
        hostTicksToSeconds = ((double) tinfo.numer / tinfo.denom) * 1.0e-9;
    }
    return hostTicksToSeconds;
}



void SPHandleOSStatus(OSStatus error, const char *operation, const char *func, const char *file, int line)
{
	if (error == noErr) return;
	
    //
    // Implement message supression.
    //
    static uint64_t timestampOfLastMessage = 0;
    static int      messageCount           = 0;
    uint64_t        currentTime            = mach_absolute_time();
    
    if ((currentTime - timestampOfLastMessage) * SPUMachHostTicksToSeconds() > 2)
        messageCount = 0;
    
    timestampOfLastMessage = currentTime;
    if (++messageCount >= 10) {
        if (messageCount == 10) {
            @autoreleasepool {
                NSLog(@"Suppressing some error messages...");
            }
        }
        if (messageCount % 100 != 0)
            return;
    }
    
    //
    // Render 4-character code.
    //
    const size_t maxLength = 20;
	char errorCode[maxLength];
	*(UInt32 *)(errorCode + 1) = CFSwapInt32HostToBig(error);
	if (isprint(errorCode[1]) && isprint(errorCode[2]) && isprint(errorCode[3]) && isprint(errorCode[4])) {
		errorCode[0] = errorCode[5] = '\'';
		errorCode[6] = '\0';
	} else {
		snprintf(errorCode, maxLength, "%d", (int) error);
    }
    
    @autoreleasepool {
        NSLog(@"%s:%d:%s: Error in %s (%s)", file, line, func, operation, errorCode);
    }
}
