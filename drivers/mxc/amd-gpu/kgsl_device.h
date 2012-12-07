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
#include <linux/irq.h>

struct kgsl_device;

#include "kgsl_types.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_drawctxt.h"
#include "kgsl_g12_cmdwindow.h"
#include "kgsl_mmu.h"

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
	int (*idle)            (struct kgsl_device *device, unsigned int timeout);
	int (*regread)         (struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
	int (*regwrite)        (struct kgsl_device *device, unsigned int offsetwords, unsigned int value);
	int (*waittimestamp)   (struct kgsl_device *device, unsigned int timestamp, unsigned int timeout);
	int (*runpending)      (struct kgsl_device *device);
	int (*mmu_tlbinvalidate)      (struct kgsl_device *device, unsigned int reg_invalidate);
	int (*mmu_setpagetable)       (struct kgsl_device *device, unsigned int reg_ptbase, uint32_t ptbase);
	int (*cmdstream_issueibcmds)  (struct kgsl_device *device, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags);
	int (*device_drawctxt_create)         (struct kgsl_device *device, unsigned int type, unsigned int *drawctxt_id, unsigned int flags);
	int (*device_drawctxt_destroy)        (struct kgsl_device *device, unsigned int drawctxt_id);
};

#define GSL_CALLER_PROCESS_MAX		64

struct kgsl_device {
	unsigned int refcnt;
	unsigned int flags;
	unsigned int id;
	unsigned int chip_id;
	struct kgsl_memregion regspace;
	struct kgsl_memdesc memstore;

	gsl_memqueue_t memqueue; // queue of memfrees pending timestamp elapse

#ifdef GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
	unsigned int memstoreshadow;
#endif /* GSL_DEVICE_SHADOW_MEMSTORE_TO_USER */

#ifdef CONFIG_KGSL_MMU_ENABLE
	struct kgsl_mmu mmu;
#endif

	struct kgsl_memregion gmemspace;
	struct kgsl_ringbuffer ringbuffer;
	unsigned int drawctxt_count;
	struct kgsl_drawctxt *drawctxt_active;
	struct kgsl_drawctxt drawctxt[KGSL_CONTEXT_MAX];
	/* qcom: hwaccess gate */
	struct kgsl_functable   ftbl;
	/* qcom: ib1_wq but this gets removed at later versions*/

	unsigned int current_timestamp;
	unsigned int timestamp;

	wait_queue_head_t wait_timestamp_wq;
	/* irq wq gets removed in later qcom versions but we need it */
	struct workqueue_struct	*irq_wq;
	struct work_struct irq_work;

	void *autogate;
	/* later qcom: cmdwindow spinlock */
};


int kgsl_device_init(struct kgsl_device *device, unsigned int device_id); // not sure if device_id needs to be passed
int kgsl_device_close(struct kgsl_device *device);
int kgsl_device_destroy(struct kgsl_device *device);
int kgsl_device_attachcallback(struct kgsl_device *device);
int kgsl_device_detachcallback(struct kgsl_device *device);
int kgsl_device_runpending(struct kgsl_device *device);

int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl);
int kgsl_g12_getfunctable(struct kgsl_functable *ftbl);

int kgsl_device_start(struct kgsl_device *device, unsigned int flags);
int kgsl_device_stop(struct kgsl_device *device);
int kgsl_device_idle(struct kgsl_device *device, unsigned int timeout);
int kgsl_device_isidle(struct kgsl_device *device);
int kgsl_device_getproperty(struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes);
int kgsl_device_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);

int kgsl_pwrctrl(struct kgsl_device *device, int state, unsigned int value);

int kgsl_device_active(struct kgsl_device *dev);

irqreturn_t kgsl_g12_isr(int irq, void *data);
irqreturn_t kgsl_yamato_isr(int irq, void *data);

#endif  // __GSL_DEVICE_H
