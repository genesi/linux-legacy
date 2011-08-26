 /* Copyright (c) 2008-2010, QUALCOMM Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of QUALCOMM Incorporated nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __OSTYPES_H
#define __OSTYPES_H

#include <linux/types.h>

//////////////////////////////////////////////////////////////////////////////
//   status
//////////////////////////////////////////////////////////////////////////////
#define OS_SUCCESS                       0
#define OS_FAILURE                      -1
#define OS_FAILURE_SYSTEMERROR          -2
#define OS_FAILURE_DEVICEERROR          -3
#define OS_FAILURE_OUTOFMEM             -4
#define OS_FAILURE_BADPARAM             -5
#define OS_FAILURE_NOTSUPPORTED         -6
#define OS_FAILURE_NOMOREAVAILABLE      -7
#define OS_FAILURE_NOTINITIALIZED       -8
#define OS_FAILURE_ALREADYINITIALIZED   -9
#define OS_FAILURE_TIMEOUT              -10


//////////////////////////////////////////////////////////////////////////////
// inline
//////////////////////////////////////////////////////////////////////////////
#ifndef OSINLINE
#define OSINLINE    static __inline
#endif // OSINLINE


//////////////////////////////////////////////////////////////////////////////
//  values
//////////////////////////////////////////////////////////////////////////////
#define OS_INFINITE             0xFFFFFFFF
#define OS_TLS_OUTOFINDEXES     0xFFFFFFFF
#define OS_TRUE                         1
#define OS_FALSE                        0

#ifndef NULL
#define NULL                    (void *)0x0
#endif  // !NULL

//////////////////////////////////////////////////////////////////////////////
// types
//////////////////////////////////////////////////////////////////////////////


//
// oshandle_t
//
typedef void *          oshandle_t;
#define OS_HANDLE_NULL  (oshandle_t)0x0

typedef enum {
    OS_PROTECTION_GLOBAL,   // inter process
    OS_PROTECTION_LOCAL,    // process local
    OS_PROTECTION_NONE,     // none
} os_protection_t;

typedef struct _os_cputimer_t {
    int refcount;                   // Reference count
    int enabled;                    // Counter is enabled
    int size;                       // Number of counters
    __s64  start_time;            // start time in cpu ticks
    __s64  end_time;              // end time in cpu ticks
    __s64  timer_frequency;       // cpu ticks per second
    __s64  *counter_array;        // number of ticks for each counter
} os_cputimer_t;

#endif  // __OSTYPES_H
