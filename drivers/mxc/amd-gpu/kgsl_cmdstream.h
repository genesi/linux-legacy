/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __GSL_CMDSTREAM_H
#define __GSL_CMDSTREAM_H

#include "kgsl_device.h"

#ifdef KGSL_DEVICE_SHADOW_MEMSTORE_TO_USER
#define KGSL_CMDSTREAM_USE_MEM_TIMESTAMP
#endif /* KGSL_DEVICE_SHADOW_MEMSTORE_TO_USER */

/* old
-#define GSL_CMDSTREAM_GET_SOP_TIMESTAMP(device, data)   kgsl_sharedmem_read(&device->memstore, (data), GSL_DEVICE_MEMSTORE_OFFSET(soptimestamp), 4, false)
-#define GSL_CMDSTREAM_GET_EOP_TIMESTAMP(device, data)   kgsl_sharedmem_read(&device->memstore, (data), GSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp), 4, false)
*/

/* new
#ifdef KGSL_CMDSTREAM_USE_MEM_TIMESTAMP
#define KGSL_CMDSTREAM_GET_SOP_TIMESTAMP(device, data) \
	kgsl_sharedmem_read(&device->memstore, (data), \
		KGSL_DEVICE_MEMSTORE_OFFSET(soptimestamp), 4, false)
	// the false above is not in qualcomm's driver
#else
#define KGSL_CMDSTREAM_GET_SOP_TIMESTAMP(device, data)  \
        do { \
                device->fbtl.device_regread(device, REG_CP_TIMESTAMP,(data)); \
        } while (0)
	// qcom: use yamato directly!
#endif // KGSL_CMDSTREAM_USE_MEM_TIMESTAMP
*/

#define KGSL_CMDSTREAM_GET_SOP_TIMESTAMP(device, data) \
	kgsl_sharedmem_read(&device->memstore, (data), \
		KGSL_DEVICE_MEMSTORE_OFFSET(soptimestamp), 4, false)

#define KGSL_CMDSTREAM_GET_EOP_TIMESTAMP(device, data) \
	kgsl_sharedmem_read(&device->memstore, (data), \
		KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp), 4, false)

void kgsl_cmdstream_memqueue_drain(struct kgsl_device *device);
int kgsl_cmdstream_init(struct kgsl_device *device);
int kgsl_cmdstream_close(struct kgsl_device *device);

// these all seem to be in the wrong place..?
int kgsl_cmdstream_issueibcmds(struct kgsl_device *device, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags);
unsigned int kgsl_cmdstream_readtimestamp(struct kgsl_device *device, enum kgsl_timestamp_type type);
int kgsl_cmdstream_freememontimestamp(struct kgsl_device *device, struct kgsl_memdesc *memdesc, unsigned int timestamp, enum kgsl_timestamp_type type);
int kgsl_cmdstream_waittimestamp(struct kgsl_device *device, unsigned int timestamp, unsigned int timeout);
int kgsl_g12_cmdwindow_write(struct kgsl_device *device, enum kgsl_cmdwindow_type target, unsigned int addr, unsigned int data);
int kgsl_add_timestamp(struct kgsl_device *device, unsigned int *timestamp);
int kgsl_cmdstream_check_timestamp(struct kgsl_device *device, unsigned int timestamp);

#endif  // __GSL_CMDSTREAM_H
