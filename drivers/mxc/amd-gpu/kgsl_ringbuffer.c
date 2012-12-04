/* Copyright (c) 2002,2007-2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/io.h>
#include <linux/sched.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_cmdstream.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_driver.h"
#include "kgsl_log.h"
#include "kgsl_debug.h"

#include "kgsl_config.h"

#include "yamato_reg.h"
#include "kgsl_pm4types.h"


#define uint32 unsigned int
#include "pm4_microcode.inl"
#include "pfp_microcode_nrt.inl"
#undef uint32
#define YAMATO_PFP_FW "yamato_pfp.fw"
#define YAMATO_PM4_FW "yamato_pm4.fw"


/* move me to a header please */
int kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
int kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value);
int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout);


#define GSL_RB_NOP_SIZEDWORDS               2

/* protected mode error checking below register address 0x800
 * note: if CP_INTERRUPT packet is used then checking needs
 * to change to below register address 0x7C8
 *
 * note: qcom: 0x200001F2
 */
#define GSL_RB_PROTECTED_MODE_CONTROL       0x00000000

/* ringbuffer size log2 quadwords equivalent */
inline unsigned int kgsl_ringbuffer_sizelog2quadwords(unsigned int sizedwords)
{
	unsigned int sizelog2quadwords = 0;
	int i = sizedwords >> 1;

	while (i >>= 1)
		sizelog2quadwords++;

	return sizelog2quadwords;
}

/* qcom: kgsl_cp_intrcallback(struct kgsl_device *device) and different layout */
void kgsl_cp_intrcallback(gsl_intrid_t id, void *cookie)
{
	struct kgsl_ringbuffer  *rb = (struct kgsl_ringbuffer *) cookie;

	/* qualcomm's driver decodes the interrupt here so the gsl_intrid_t
	 * is not required to be passed. ringbuffer is pulled from device->ringbuffer */

	switch(id) {
	/* error condition interrupt */
	case GSL_INTR_YDX_CP_T0_PACKET_IN_IB:
	case GSL_INTR_YDX_CP_OPCODE_ERROR:
	case GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR:
	case GSL_INTR_YDX_CP_RESERVED_BIT_ERROR:
	case GSL_INTR_YDX_CP_IB_ERROR:
		pr_err("GPU: CP Error\n");
		schedule_work(&rb->device->irq_err_work);
		break;

	/* non-error condition interrupt */
	case GSL_INTR_YDX_CP_SW_INT:
	case GSL_INTR_YDX_CP_IB2_INT:
	case GSL_INTR_YDX_CP_IB1_INT:
	case GSL_INTR_YDX_CP_RING_BUFFER:
		/* signal intr completion event */
		complete_all(&rb->device->intr.evnt[id]);
		break;
	default:
		break;
	}
}

static void kgsl_ringbuffer_submit(struct kgsl_ringbuffer *rb)
{
	/* qcom: not there */
	kgsl_device_active(rb->device);

	GSL_RB_UPDATE_WPTR_POLLING(rb);
	/* Drain write buffer and data memory barrier */
	dsb();
	wmb();

	/* Memory fence to ensure all data has posted. On some
	 * systems, like 7x27, the register block is not allocated
	 * as strongly ordered memory. Adding a memory fence ensures
	 * ordering during ringbuffer submits. */
	mb();

	kgsl_yamato_regwrite(rb->device, REG_CP_RB_WPTR, rb->wptr);

	rb->flags |= KGSL_FLAGS_ACTIVE;
}

static int kgsl_ringbuffer_waitspace(struct kgsl_ringbuffer *rb, unsigned int numcmds, int wptr_ahead)
{
	int           nopcount;
	unsigned int  freecmds;
	unsigned int  *cmds;

	/* if wptr ahead, fill the remaining with NOPs */
	if (wptr_ahead) {
		/* -1 for header */
		nopcount = rb->sizedwords - rb->wptr - 1;

		cmds = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		GSL_RB_WRITE(cmds, pm4_nop_packet(nopcount));

		/* Make sure that rptr is not 0 before submitting
		 * commands at the end of the ringbuffer. We do
		 * not want the rptr and wptr to become equal when
		 * the ringbuffer is not empty */
		do {
			GSL_RB_GET_READPTR(rb, &rb->rptr);
		} while (!rb->rptr);

		rb->wptr++;

		kgsl_ringbuffer_submit(rb);

		rb->wptr = 0;
	}

	/* wait for space in ringbuffer */
	do {
		GSL_RB_GET_READPTR(rb, &rb->rptr);
		freecmds = rb->rptr - rb->wptr;
	} while ((freecmds != 0) && (freecmds <= numcmds));

	return GSL_SUCCESS;
}

static unsigned int *kgsl_ringbuffer_allocspace(struct kgsl_ringbuffer *rb,
							unsigned int numcmds)
{
	unsigned int *ptr = NULL;
	int status = GSL_SUCCESS;

	/* check for available space */
	if (rb->wptr >= rb->rptr) {
		/* wptr ahead or equal to rptr */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS)) {
			status = kgsl_ringbuffer_waitspace(rb, numcmds, 1);
		}
	} else {
		/* wptr behind rptr */
		if ((rb->wptr + numcmds) >= rb->rptr) {
			status  = kgsl_ringbuffer_waitspace(rb, numcmds, 0);
		}

		/* check for remaining space */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS)) {
			status = kgsl_ringbuffer_waitspace(rb, numcmds, 1);
		}
	}

	if (status == GSL_SUCCESS) {
		ptr = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		rb->wptr += numcmds;
	}

	return ptr;
}

/* qcom: this is significantly different, flags and sizedwords passed and does more than the above */
static unsigned int *kgsl_ringbuffer_addcmds(struct kgsl_ringbuffer *rb, unsigned int numcmds)
{
	unsigned int *ptr;

	/* NQ: update host copy of read pointer when running in safe mode */
	if (rb->device->flags & KGSL_FLAGS_SAFEMODE) {
		GSL_RB_GET_READPTR(rb, &rb->rptr);
	}

	ptr = kgsl_ringbuffer_allocspace(rb, numcmds);

	return ptr;
}

static int kgsl_ringbuffer_load_pm4_ucode(struct kgsl_device *device)
{
	int i;

	kgsl_yamato_regwrite(device, REG_CP_DEBUG, 0x02000000);
	kgsl_yamato_regwrite(device, REG_CP_ME_RAM_WADDR, 0);

	for (i = 0; i < PM4_MICROCODE_SIZE; i++) {
		kgsl_yamato_regwrite(device, REG_CP_ME_RAM_DATA, aPM4_Microcode[i][0]);
		kgsl_yamato_regwrite(device, REG_CP_ME_RAM_DATA, aPM4_Microcode[i][1]);
		kgsl_yamato_regwrite(device, REG_CP_ME_RAM_DATA, aPM4_Microcode[i][2]);
	}

	return GSL_SUCCESS;
}

static int kgsl_ringbuffer_load_pfp_ucode(struct kgsl_device *device)
{
	int i;

	kgsl_yamato_regwrite(device, REG_CP_PFP_UCODE_ADDR, 0);

	for (i = 0; i < PFP_MICROCODE_SIZE_NRT; i++) {
		kgsl_yamato_regwrite(device, REG_CP_PFP_UCODE_DATA, aPFP_Microcode_nrt[i]);
	}

	return GSL_SUCCESS;
}

int kgsl_ringbuffer_start(struct kgsl_ringbuffer *rb)
{
	int status;
	union reg_cp_rb_cntl cp_rb_cntl;
	unsigned int *cmds; /*, rb_cntl */
	struct kgsl_device *device = rb->device;

	if (rb->flags & KGSL_FLAGS_STARTED) {
		return GSL_SUCCESS;
	}

	kgsl_sharedmem_set0(&rb->memptrs_desc, 0, 0,
				sizeof(struct kgsl_rbmemptrs));
	kgsl_sharedmem_set0(&rb->buffer_desc, 0, 0x12341234, /* qcom: 0xAA */
				(rb->sizedwords << 2));

	kgsl_yamato_regwrite(device, REG_CP_RB_WPTR_BASE,
				(rb->memptrs_desc.gpuaddr
				+ GSL_RB_MEMPTRS_WPTRPOLL_OFFSET));

	/* setup WPTR delay */
	kgsl_yamato_regwrite(device, REG_CP_RB_WPTR_DELAY, 0/*0x70000010*/);

	/* setup RB_CNTL */
	kgsl_yamato_regread(device, REG_CP_RB_CNTL, (unsigned int *)&cp_rb_cntl);

	/* size of ringbuffer */
	cp_rb_cntl.f.rb_bufsz =
		kgsl_ringbuffer_sizelog2quadwords(rb->sizedwords);
	/* quadwords to read before updating mem RPTR */
	cp_rb_cntl.f.rb_blksz = rb->blksizequadwords;
	/* WPTR polling */
	cp_rb_cntl.f.rb_poll_en = GSL_RB_CNTL_POLL_EN;
	/* mem RPTR writebacks */
	cp_rb_cntl.f.rb_no_update = GSL_RB_CNTL_NO_UPDATE;

	kgsl_yamato_regwrite(device, REG_CP_RB_CNTL, cp_rb_cntl.val);

	kgsl_yamato_regwrite(device, REG_CP_RB_BASE, rb->buffer_desc.gpuaddr);

	kgsl_yamato_regwrite(device, REG_CP_RB_RPTR_ADDR,
				rb->memptrs_desc.gpuaddr +
				GSL_RB_MEMPTRS_RPTR_OFFSET);

	/* explicitly clear all cp interrupts */
	kgsl_yamato_regwrite(device, REG_CP_INT_ACK, 0xFFFFFFFF);

	/* setup scratch/timestamp addr */
	kgsl_yamato_regwrite(device, REG_SCRATCH_ADDR,
				device->memstore.gpuaddr +
				KGSL_DEVICE_MEMSTORE_OFFSET(soptimestamp));

	kgsl_yamato_regwrite(device, REG_SCRATCH_UMSK,
				GSL_RB_MEMPTRS_SCRATCH_MASK);


	/* load the CP ucode */
	status = kgsl_ringbuffer_load_pm4_ucode(device);

	/* load the prefetch parser ucode */
	status = kgsl_ringbuffer_load_pfp_ucode(device);

	kgsl_yamato_regwrite(device, REG_CP_QUEUE_THRESHOLDS, 0x000C0804);

	rb->rptr = 0;
	rb->wptr = 0;

	rb->timestamp = 0;
	GSL_RB_INIT_TIMESTAMP(rb);

	/* qcom: init list head memqueue here */

	/* NQ: clear ME_HALT to start micro engine */
	kgsl_yamato_regwrite(device, REG_CP_ME_CNTL, 0);

	/* ME_INIT */
	/* qcom: ME_INIT, do kgsl_ringbuffer_allocspace(rb, 19) here */
	cmds  = kgsl_ringbuffer_addcmds(rb, 19);

	GSL_RB_WRITE(cmds, PM4_HDR_ME_INIT);
	/* All fields present (bits 9:0) */
	GSL_RB_WRITE(cmds, 0x000003ff);
	/* Disable/Enable Real-Time Stream processing (present but ignored) */
	GSL_RB_WRITE(cmds, 0x00000000);
	/* Enable (2D to 3D) and (3D to 2D) implicit synchronization (present but ignored) */
	GSL_RB_WRITE(cmds, 0x00000000);

	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_RB_SURFACE_INFO));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_PA_SC_WINDOW_OFFSET));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_VGT_MAX_VTX_INDX));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_SQ_PROGRAM_CNTL));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_RB_DEPTHCONTROL));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_PA_SU_POINT_SIZE));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_PA_SC_LINE_CNTL));
	GSL_RB_WRITE(cmds, GSL_HAL_SUBBLOCK_OFFSET(REG_PA_SU_POLY_OFFSET_FRONT_SCALE));

	/* Vertex and Pixel Shader Start Addresses in instructions
	 * (3 DWORDS per instruction) */
	GSL_RB_WRITE(cmds, 0x80000180);
	/* Maximum Contexts */
	GSL_RB_WRITE(cmds, 0x00000001);
	/* Write Confirm Interval and The CP will wait the
	 * wait_interval * 16 clocks between polling */
	GSL_RB_WRITE(cmds, 0x00000000);
	/* NQ and External Memory Swap */
	GSL_RB_WRITE(cmds, 0x00000000);
	/* Protected mode error checking */
	GSL_RB_WRITE(cmds, GSL_RB_PROTECTED_MODE_CONTROL);
	/* Disable header dumping and Header dump address */
	GSL_RB_WRITE(cmds, 0x00000000);
	/* Header dump size */
	GSL_RB_WRITE(cmds, 0x00000000);

	kgsl_ringbuffer_submit(rb);

	/* idle device to validate ME INIT */
	status = kgsl_yamato_idle(device, GSL_TIMEOUT_DEFAULT);

	/* enable cp interrupts, this is done as one regwrite in qcom */
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_SW_INT, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_T0_PACKET_IN_IB, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_OPCODE_ERROR, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_RESERVED_BIT_ERROR, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_IB_ERROR, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_IB2_INT, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_IB1_INT, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_attach(&device->intr, GSL_INTR_YDX_CP_RING_BUFFER, kgsl_cp_intrcallback, (void *) rb);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_SW_INT);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_T0_PACKET_IN_IB);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_OPCODE_ERROR);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_RESERVED_BIT_ERROR);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_IB_ERROR);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_IB2_INT);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_IB1_INT);
	kgsl_intr_enable(&device->intr, GSL_INTR_YDX_CP_RING_BUFFER);

	if (status == GSL_SUCCESS)
		rb->flags |= KGSL_FLAGS_STARTED;

	return status;
}

int kgsl_ringbuffer_stop(struct kgsl_ringbuffer *rb)
{
	if (rb->flags & KGSL_FLAGS_STARTED) {
		/* disable cp interrupts */
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_SW_INT);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_T0_PACKET_IN_IB);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_OPCODE_ERROR);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_RESERVED_BIT_ERROR);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_IB_ERROR);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_IB2_INT);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_IB1_INT);
		kgsl_intr_detach(&rb->device->intr, GSL_INTR_YDX_CP_RING_BUFFER);

		/* ME_HALT */
		kgsl_yamato_regwrite(rb->device, REG_CP_ME_CNTL, 0x10000000);

		/* qcom: kgsl_cmdstream_memqueue_drain(rb->device); */

		rb->flags &= ~KGSL_FLAGS_STARTED;
	}
	return GSL_SUCCESS;
}

int kgsl_ringbuffer_init(struct kgsl_device *device)
{
	int status;
	unsigned int flags;
	struct kgsl_ringbuffer *rb = &device->ringbuffer;

	rb->device = device;
	rb->sizedwords = (2 << kgsl_cfg_rb_sizelog2quadwords);
	rb->blksizequadwords = kgsl_cfg_rb_blksizequadwords;

	/* allocate memory for ringbuffer, needs to be double octword aligned
	 * align on page from contiguous physical memory */
	flags = (GSL_MEMFLAGS_ALIGNPAGE | GSL_MEMFLAGS_CONPHYS |
		GSL_MEMFLAGS_STRICTREQUEST);

	status = kgsl_sharedmem_alloc0(device->id, flags, (rb->sizedwords << 2),
					&rb->buffer_desc);

	if (status != GSL_SUCCESS) {
		kgsl_ringbuffer_close(rb);
		return status;
	}

	/* allocate memory for polling and timestamps */
	/* This really can be at a 4 byte alignment boundary but for using MMU
	 * we need to make it at page boundary */
	flags = (GSL_MEMFLAGS_ALIGNPAGE | GSL_MEMFLAGS_CONPHYS);

	status = kgsl_sharedmem_alloc0(device->id, flags, sizeof(struct kgsl_rbmemptrs),
					&rb->memptrs_desc);

	if (status != GSL_SUCCESS) {
		kgsl_ringbuffer_close(rb);
		return status;
	}

	/* overlay structure on memptrs memory */
	rb->memptrs = (struct kgsl_rbmemptrs *)rb->memptrs_desc.hostptr;

	/* qcom: stop and return here! */
	rb->flags |= KGSL_FLAGS_INITIALIZED;

	/* validate command stream data when running in safe mode */
	if (device->flags & KGSL_FLAGS_SAFEMODE)
	{
		gsl_driver.flags_debug |= GSL_DBGFLAGS_PM4CHECK;
	}

	/* start ringbuffer */
	status = kgsl_ringbuffer_start(rb);

	if (status != GSL_SUCCESS) {
		kgsl_ringbuffer_close(rb);
		return status;
	}

	return GSL_SUCCESS;
}

int kgsl_ringbuffer_close(struct kgsl_ringbuffer *rb)
{
	/* NQ: stop ringbuffer */
	kgsl_ringbuffer_stop(rb);

	if (rb->buffer_desc.hostptr)
		kgsl_sharedmem_free0(&rb->buffer_desc, current->tgid);

	if (rb->memptrs_desc.hostptr)
		kgsl_sharedmem_free0(&rb->memptrs_desc, current->tgid);

	rb->flags &= ~KGSL_FLAGS_INITIALIZED;

	memset(rb, 0, sizeof(struct kgsl_ringbuffer));

	return GSL_SUCCESS;
}

/* qcom: just calls qcom addcmds */
/* neko: we assume pmodeoff = 0 */
unsigned int kgsl_ringbuffer_issuecmds(struct kgsl_device *device,
					unsigned int flags, unsigned int *cmds,
					int sizedwords, unsigned int pid)
{
	struct kgsl_ringbuffer *rb = &device->ringbuffer;
	unsigned int pmodesizedwords = 0;
	unsigned int *ringcmds;
	unsigned int timestamp;

	if (!(device->ringbuffer.flags & KGSL_FLAGS_STARTED))
		return GSL_FAILURE;

	/* set mmu pagetable */
	kgsl_mmu_setpagetable(device, pid);

#ifdef GSL_RB_TIMESTAMP_INTERUPT
	pmodesizedwords += 2;
#endif

	/* this is all in qcom addcmds, replace addcmds here with allocspace */
	/* allocate space in ringbuffer */
	ringcmds = kgsl_ringbuffer_addcmds(rb, pmodesizedwords + sizedwords + 6);

	/* copy the cmds to the ringbuffer */
	memcpy(ringcmds, cmds, (sizedwords << 2));

	ringcmds += sizedwords;

	rb->timestamp++;
	timestamp = rb->timestamp;

	/* start-of-pipeline and end-of-pipeline timestamps */
	*ringcmds++ = pm4_type0_packet(REG_CP_TIMESTAMP, 1);
	*ringcmds++ = rb->timestamp;
	*ringcmds++ = pm4_type3_packet(PM4_EVENT_WRITE, 3);
	*ringcmds++ = CACHE_FLUSH_TS;
	*ringcmds++ = device->memstore.gpuaddr + KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp);
	*ringcmds++ = rb->timestamp;
#ifdef GSL_RB_TIMESTAMP_INTERUPT
	*ringcmds++ = pm4_type3_packet(PM4_INTERRUPT, 1);
	*ringcmds++ = 0x80000000;
#endif

	/* issue the commands */
	kgsl_ringbuffer_submit(rb);

	return timestamp;
}

/* neko: to get this closer to qualcomm we allocate the ib stuff here, but hack it so the loops only go once */
int kgsl_ringbuffer_issueibcmds(struct kgsl_device *device, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags)
{
	unsigned int *link;
	unsigned int *cmds;
	int i, numibs = 1; /* hack */

	struct kgsl_ringbuffer  *rb = &device->ringbuffer;

	if (!(rb->flags & KGSL_FLAGS_STARTED) ||
		(drawctxt_index >= KGSL_CONTEXT_MAX)) {
		return GSL_FAILURE; // -EINVAL
	}

	link = kzalloc(sizeof(unsigned int) * numibs * 3, GFP_KERNEL);
	cmds = link;

	if (!link) {
		pr_err("Failed to allocate memory for command submission, size %x\n", numibs * 3);
		return GSL_FAILURE; // -ENOMEM
	}

	for (i = 0; i < numibs; i++) {
		*cmds++ = PM4_HDR_INDIRECT_BUFFER_PFD;
		*cmds++ = ibaddr; // qcom: ibdesc[i].gpuaddr
		*cmds++ = sizedwords; // qcom: ibdesc[i].sizedwords;
	}

	/* qcom: setstate here (mmu flush? */
	/* context switch if needed */
	kgsl_drawctxt_switch(device, &device->drawctxt[drawctxt_index], flags);

	/* qcom: calls qcom addcmds */
	*timestamp = kgsl_ringbuffer_issuecmds(device,
				0, &link[0], (cmds - link), current->tgid);

	/* NQ: idle device when running in safe mode */
	if (device->flags & KGSL_FLAGS_SAFEMODE)
		kgsl_yamato_idle(device, GSL_TIMEOUT_DEFAULT);

	kfree(link);

	return GSL_SUCCESS;
}

/* NQ?? */
int kgsl_ringbuffer_bist(struct kgsl_ringbuffer *rb)
{
	unsigned int *cmds;
	unsigned int temp, k, j;
	int status, i;

	if (!(rb->flags & KGSL_FLAGS_STARTED))
		return GSL_FAILURE;

	/* simple nop submit */
	cmds = kgsl_ringbuffer_addcmds(rb, 2);
	if (!cmds)
		return GSL_FAILURE;

	GSL_RB_WRITE(cmds, pm4_nop_packet(1));
	GSL_RB_WRITE(cmds, 0xDEADBEEF);

	kgsl_ringbuffer_submit(rb);

	status = kgsl_yamato_idle(rb->device, GSL_TIMEOUT_DEFAULT);

	if (status != GSL_SUCCESS)
		return status;

	/* simple scratch submit */
	cmds = kgsl_ringbuffer_addcmds(rb, 2);
	if (!cmds)
		return GSL_FAILURE;

	GSL_RB_WRITE(cmds, pm4_type0_packet(REG_SCRATCH_REG7, 1));
	GSL_RB_WRITE(cmds, 0xFEEDF00D);

	kgsl_ringbuffer_submit(rb);

	status = kgsl_yamato_idle(rb->device, GSL_TIMEOUT_DEFAULT);

	if (status != GSL_SUCCESS)
		return status;

	kgsl_yamato_regread(rb->device, REG_SCRATCH_REG7, &temp);

	if (temp != 0xFEEDF00D)
		return GSL_FAILURE;

	/* simple wraps */
	for (i = 0; i < 256; i+=2) {
		j = ((rb->sizedwords >> 2) - 256) + i;

		cmds = kgsl_ringbuffer_addcmds(rb, j);
		if (!cmds)
			return GSL_FAILURE;

		k = 0;
		while (k < j) {
			k+=2;
			GSL_RB_WRITE(cmds, pm4_type0_packet(REG_SCRATCH_REG7, 1));
			GSL_RB_WRITE(cmds, k);
		}

		kgsl_ringbuffer_submit(rb);

		status = kgsl_yamato_idle(rb->device, GSL_TIMEOUT_DEFAULT);

		if (status != GSL_SUCCESS)
			return status;

		kgsl_yamato_regread(rb->device, REG_SCRATCH_REG7, &temp);

		if (temp != k)
			return GSL_FAILURE;
	}

	/* max size submits, TODO do this at least with regreads */
	for (i = 0; i < 256; i++) {
		cmds = kgsl_ringbuffer_addcmds(rb, (rb->sizedwords >> 2));

		if (!cmds)
			return GSL_FAILURE;

		GSL_RB_WRITE(cmds, pm4_nop_packet((rb->sizedwords >> 2) - 1));

		kgsl_ringbuffer_submit(rb);

		status = kgsl_yamato_idle(rb->device, GSL_TIMEOUT_DEFAULT);

		if (status != GSL_SUCCESS)
			return status;
	}
	/* submit load with randomness */

#ifdef GSL_RB_USE_MEM_TIMESTAMP
	/* scratch memptr validate */
#endif

#ifdef GSL_RB_USE_MEM_RPTR
	/* rptr memptr validate */
#endif

#ifdef GSL_RB_USE_WPTR_POLLING
	/* wptr memptr validate */
#endif
	return status;
}
