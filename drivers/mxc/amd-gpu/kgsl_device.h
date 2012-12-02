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

#ifndef __GSL_DEVICE_H
#define __GSL_DEVICE_H

#include <linux/wait.h>
#include <linux/workqueue.h>

//////////////////////////////////////////////////////////////////////////////
//  types
//////////////////////////////////////////////////////////////////////////////


// forward declaration
struct kgsl_device;

#include "kgsl_types.h"
#include "kgsl_intrmgr.h" // for gsl_intr_t
#include "kgsl_ringbuffer.h" // for struct kgsl_ringbuffer
#include "kgsl_drawctxt.h" // struct kgsl_drawctxt
#include "kgsl_g12_cmdwindow.h" // for GSL_G12_INTR_COUNT
#include "kgsl_properties.h" // for enum kgsl_property_type
#include "kgsl_mmu.h" // for kgsl_mmu

// --------------
// function table
// --------------
struct kgsl_functable {
	int (*init)            (struct kgsl_device *device);
	int (*close)           (struct kgsl_device *device);
	int (*destroy)         (struct kgsl_device *device);
	int (*start)           (struct kgsl_device *device, unsigned int flags);
	int (*stop)            (struct kgsl_device *device);
	int (*getproperty)     (struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes);
	int (*setproperty)     (struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes);
	int (*idle)            (struct kgsl_device *device, unsigned int timeout);
	int (*regread)         (struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
	int (*regwrite)        (struct kgsl_device *device, unsigned int offsetwords, unsigned int value);
	int (*waitirq)         (struct kgsl_device *device, gsl_intrid_t intr_id, unsigned int *count, unsigned int timeout);
	int (*waittimestamp)   (struct kgsl_device *device, unsigned int timestamp, unsigned int timeout);
	int (*runpending)      (struct kgsl_device *device);
	int (*addtimestamp)    (struct kgsl_device *device_id, unsigned int *timestamp);
	int (*intr_isr)               (struct kgsl_device *device);
	int (*mmu_tlbinvalidate)      (struct kgsl_device *device, unsigned int reg_invalidate, unsigned int pid);
	int (*mmu_setpagetable)       (struct kgsl_device *device, unsigned int reg_ptbase, uint32_t ptbase, unsigned int pid);
	int (*cmdstream_issueibcmds)  (struct kgsl_device *device, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags);
	int (*device_drawctxt_create)         (struct kgsl_device *device, unsigned int type, unsigned int *drawctxt_id, unsigned int flags);
	int (*device_drawctxt_destroy)        (struct kgsl_device *device_id, unsigned int drawctxt_id);
};

#define GSL_CALLER_PROCESS_MAX		64

// device object
struct kgsl_device {
	unsigned int      refcnt;
	unsigned int      callerprocess[GSL_CALLER_PROCESS_MAX];    // caller process table
	struct kgsl_functable   ftbl;
	unsigned int       flags;
	unsigned int    id;
	unsigned int      chip_id;
	struct kgsl_memregion   regspace;
	gsl_intr_t        intr;
	struct kgsl_memdesc     memstore;
	gsl_memqueue_t    memqueue; // queue of memfrees pending timestamp elapse

#ifdef  GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
	unsigned int      memstoreshadow[GSL_CALLER_PROCESS_MAX];
#endif // GSL_DEVICE_SHADOW_MEMSTORE_TO_USER

#ifdef CONFIG_KGSL_MMU_ENABLE
	struct kgsl_mmu         mmu;
#endif

	struct kgsl_memregion   gmemspace;
	struct kgsl_ringbuffer  ringbuffer;
	struct mutex 	  *drawctxt_mutex;
	unsigned int      drawctxt_count;
	struct kgsl_drawctxt    *drawctxt_active;
	struct kgsl_drawctxt    drawctxt[GSL_CONTEXT_MAX];

	unsigned int		intrcnt[GSL_G12_INTR_COUNT];
	unsigned int		current_timestamp;
	unsigned int		timestamp;
	struct mutex 	  *cmdwindow_mutex;

	struct mutex 	*cmdstream_mutex;
	wait_queue_head_t timestamp_waitq;
	struct workqueue_struct	*irq_workq;
	struct work_struct irq_work;
	struct work_struct irq_err_work;

	void              *autogate;
};


//  prototypes
int kgsl_device_init(struct kgsl_device *device, unsigned int device_id);
int kgsl_device_close(struct kgsl_device *device);
int kgsl_device_destroy(struct kgsl_device *device);
int kgsl_device_attachcallback(struct kgsl_device *device, unsigned int pid);
int kgsl_device_detachcallback(struct kgsl_device *device, unsigned int pid);
int kgsl_device_runpending(struct kgsl_device *device);

int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl);
int kgsl_g12_getfunctable(struct kgsl_functable *ftbl);

int kgsl_device_start(unsigned int device_id, unsigned int flags);
int kgsl_device_stop(unsigned int device_id);
int kgsl_device_idle(unsigned int device_id, unsigned int timeout);
int kgsl_device_isidle(unsigned int device_id);
int kgsl_device_getproperty(unsigned int device_id, enum kgsl_property_type type, void *value, unsigned int sizebytes);
int kgsl_device_setproperty(unsigned int device_id, enum kgsl_property_type type, void *value, unsigned int sizebytes);
int kgsl_device_regread(unsigned int device_id, unsigned int offsetwords, unsigned int *value);
int kgsl_device_regwrite(unsigned int device_id, unsigned int offsetwords, unsigned int value);
int kgsl_device_waitirq(unsigned int device_id, gsl_intrid_t intr_id, unsigned int *count, unsigned int timeout);

int kgsl_clock(unsigned int dev, int enable);
int kgsl_device_active(struct kgsl_device *dev);
int kgsl_device_clock(unsigned int id, int enable);
int kgsl_device_autogate_init(struct kgsl_device *dev);
void kgsl_device_autogate_exit(struct kgsl_device *dev);

#endif  // __GSL_DEVICE_H
