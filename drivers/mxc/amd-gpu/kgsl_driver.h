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

#include <linux/platform_device.h>
#include <linux/mutex.h>

#include "kgsl_types.h" // KGSL_DEVICE_MAX
#include "kgsl_device.h" // kgsl_device etc.
#include "kgsl_sharedmem.h" // struct kgsl_sharedmem
#include "kgsl_buildconfig.h" // GSL_CALLER_PROCESS_MAX

struct kgsl_driver {
	unsigned int  flags_debug;
	int refcnt;
	struct mutex lock;
	void *hal;
	struct kgsl_sharedmem shmem;
	struct kgsl_device device[KGSL_DEVICE_MAX];
	int enable_mmu;
	struct platform_device *pdev;
};

extern struct kgsl_driver gsl_driver;

int                kgsl_driver_init(void);
int                kgsl_driver_close(void);
int                kgsl_driver_entry(unsigned int flags);
int                kgsl_driver_exit(void);
int                kgsl_driver_destroy(void);

#endif // __GSL_DRIVER_H
