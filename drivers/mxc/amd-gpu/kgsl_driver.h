/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices nor
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

#ifndef __GSL_DRIVER_H
#define __GSL_DRIVER_H

#include <linux/mutex.h>

#include "kgsl_types.h" // GSL_DRIVER_MAX
#include "kgsl_device.h" // kgsl_device etc.
#include "kgsl_sharedmem.h" // gsl_sharedmem_t
#include "kgsl_buildconfig.h" // GSL_CALLER_PROCESS_MAX

//////////////////////////////////////////////////////////////////////////////
// types
//////////////////////////////////////////////////////////////////////////////

// -------------
// driver object
// -------------
typedef struct _gsl_driver_t {
    unsigned int      flags_debug;
    int              refcnt;
    unsigned int     callerprocess[GSL_CALLER_PROCESS_MAX]; // caller process table
    struct mutex     lock;                                 // global API mutex
    void             *hal;
    gsl_sharedmem_t  shmem;
    struct kgsl_device     device[GSL_DEVICE_MAX];
    int              dmi_state;     //  OS_TRUE = enabled, OS_FALSE otherwise
    unsigned int      dmi_mode;      //  single, double, or triple buffering
    int              dmi_frame;     //  set to -1 when DMI is enabled
    int              dmi_max_frame; //  indicates the maximum frame # that we will support
    int              enable_mmu;
} gsl_driver_t;


//////////////////////////////////////////////////////////////////////////////
// external variable declarations
//////////////////////////////////////////////////////////////////////////////
extern gsl_driver_t  gsl_driver;


//////////////////////////////////////////////////////////////////////////////
//  inline functions
//////////////////////////////////////////////////////////////////////////////
static __inline int
kgsl_driver_getcallerprocessindex(unsigned int pid, int *index)
{
    int  i;

    // obtain index in caller process table
    for (i = 0; i < GSL_CALLER_PROCESS_MAX; i++)
    {
        if (gsl_driver.callerprocess[i] == pid)
        {
            *index = i;
            return (GSL_SUCCESS);
        }
    }

    return (GSL_FAILURE);
}
int                kgsl_driver_init(void);
int                kgsl_driver_close(void);
int                kgsl_driver_entry(unsigned int flags);
int                kgsl_driver_exit(void);
int                kgsl_driver_destroy(unsigned int pid);

#endif // __GSL_DRIVER_H
