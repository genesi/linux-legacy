/* Copyright (c) 2002,2007-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_types.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_drawctxt.h"
#include "kgsl_cmdstream.h"

#include "yamato_reg.h"
#include "kgsl_pm4types.h"

#include "kgsl_log.h"
#include "kgsl_debug.h"

/* move me to a header please */
int kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value);
int kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
int kgsl_yamato_stop(struct kgsl_device *device);
int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout);

void kgsl_mh_intrcallback(struct kgsl_device *device);
void kgsl_cp_intrcallback(struct kgsl_device *device);


#define GSL_RBBM_INT_MASK \
	 (RBBM_INT_CNTL__RDERR_INT_MASK |  \
	  RBBM_INT_CNTL__DISPLAY_UPDATE_INT_MASK)

#define GSL_SQ_INT_MASK \
	(SQ_INT_CNTL__PS_WATCHDOG_MASK | \
	 SQ_INT_CNTL__VS_WATCHDOG_MASK)


/* Yamato MH arbiter config*/
#define KGSL_CFG_YAMATO_MHARB \
	(0x10 \
		| (0 << MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT) \
		| (0x8 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT))


/* qcom: pass yamato_device, derive kgsl_device from it */
static int kgsl_yamato_gmeminit(struct kgsl_device *device)
{
	union reg_rb_edram_info rb_edram_info;
	unsigned int    gmem_size;
	unsigned int    edram_value = 0;


	/* make sure edram range is aligned to size */
	BUG_ON(device->gmemspace.gpu_base &
		(device->gmemspace.sizebytes - 1));

	/* get edram_size value equivalent */
	gmem_size = (device->gmemspace.sizebytes >> 14);
	while (gmem_size >>= 1)
		edram_value++;

	rb_edram_info.f.edram_size = edram_value;
	rb_edram_info.f.edram_mapping_mode = 0; /* EDRAM_MAP_UPPER */

	/* must be aligned to size */
	rb_edram_info.f.edram_range = (device->gmemspace.gpu_base >> 14);

	kgsl_yamato_regwrite(device, REG_RB_EDRAM_INFO,
			(unsigned int)rb_edram_info.val);

	return GSL_SUCCESS;
}

static int kgsl_yamato_gmemclose(struct kgsl_device *device)
{
	kgsl_yamato_regwrite(device, REG_RB_EDRAM_INFO, 0x00000000);

	return GSL_SUCCESS;
}

void kgsl_yamato_rbbm_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int rderr = 0;

	KGSL_DRV_VDBG("enter (device=%p)\n", device);

	kgsl_yamato_regread(device, REG_RBBM_INT_STATUS, &status);

	if (status & RBBM_INT_CNTL__RDERR_INT_MASK) {
		kgsl_yamato_regread(device, REG_RBBM_READ_ERROR, &rderr);
		KGSL_DRV_FATAL("rbbm read error interrupt: %08x\n", rderr);
	} else if (status & RBBM_INT_CNTL__DISPLAY_UPDATE_INT_MASK) {
		KGSL_DRV_DBG("rbbm display update interrupt\n");
	} else if (status & RBBM_INT_CNTL__GUI_IDLE_INT_MASK) {
		KGSL_DRV_DBG("rbbm gui idle interrupt\n");
	} else {
		KGSL_CMD_DBG("bad bits in REG_CP_INT_STATUS %08x\n", status);
	}

	status &= GSL_RBBM_INT_MASK;
	kgsl_yamato_regwrite(device, REG_RBBM_INT_ACK, status);

	KGSL_DRV_VDBG("return\n");
}

void kgsl_yamato_sq_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;

	KGSL_DRV_VDBG("enter (device=%p)\n", device);

	kgsl_yamato_regread(device, REG_SQ_INT_STATUS, &status);

	if (status & SQ_INT_CNTL__PS_WATCHDOG_MASK)
		KGSL_DRV_DBG("sq ps watchdog interrupt\n");
	else if (status & SQ_INT_CNTL__VS_WATCHDOG_MASK)
		KGSL_DRV_DBG("sq vs watchdog interrupt\n");
	else
		KGSL_DRV_DBG("bad bits in REG_SQ_INT_STATUS %08x\n", status);

	status &= GSL_SQ_INT_MASK;
	kgsl_yamato_regwrite(device, REG_SQ_INT_ACK, status);

	KGSL_DRV_VDBG("return\n");
}

irqreturn_t kgsl_yamato_isr(int irq, void *data)
{
	irqreturn_t result = IRQ_NONE;
	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_YAMATO-1];
	unsigned int status;

	/* determine if yamato is interrupting, and if so, which block */
	kgsl_yamato_regread(device, REG_MASTER_INT_SIGNAL, &status);

	if (status & MASTER_INT_SIGNAL__MH_INT_STAT) {
		kgsl_mh_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__CP_INT_STAT) {
		kgsl_cp_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__RBBM_INT_STAT) {
		kgsl_yamato_rbbm_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__SQ_INT_STAT) {
		kgsl_yamato_sq_intrcallback(device);
		result = IRQ_HANDLED;
	}

	/* qcom: reset timeout in idle timer mod_timer(device->idle_timer, jiffies + device->interval_timeout */
	return GSL_SUCCESS;
}

/* qcom: part of kgsl_yamato_setstate flags & KGSL_MMUFLAGS_TLBFLUSH */
int kgsl_yamato_tlbinvalidate(struct kgsl_device *device, unsigned int reg_invalidate)
{
	unsigned int link[2];
	unsigned int *cmds = &link[0];
	unsigned int sizedwords = 0;
	unsigned int mh_mmu_invalidate = 0x00000003L; /* invalidate all and tc */

	/* if possible, invalidate via command stream, otherwise via direct register writes */
	if (device->flags & KGSL_FLAGS_STARTED)
	{
		/* there's a wait for idle packet up front here in qcom code */
		*cmds++ = pm4_type0_packet(reg_invalidate, 1);
		*cmds++ = mh_mmu_invalidate;
		sizedwords += 2;

		kgsl_ringbuffer_issuecmds(device, 1, &link[0], sizedwords);
	}
	else
		kgsl_yamato_regwrite(device, reg_invalidate, mh_mmu_invalidate);

	return GSL_SUCCESS;
}


/* qcom: part of kgsl_yamato_setstate flags & KGSL_MMUFLAGS_PTUPDATE */
int kgsl_yamato_setpagetable(struct kgsl_device *device, unsigned int reg_ptbase, uint32_t ptbase)
{
	unsigned int link[25];
	unsigned int *cmds = &link[0];
	unsigned int sizedwords = 0;

	/* if there is an active draw context, set via command stream,
	 * otherwise set via direct register writes
	 */
	if (device->flags & KGSL_FLAGS_STARTED) {
		/* wait for graphics pipe to be idle */
		*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
		*cmds++ = 0x00000000;

		/* set page table base */
		*cmds++ = pm4_type0_packet(reg_ptbase, 1);
		*cmds++ = ptbase;
		sizedwords += 4;

		/* HW workaround: to resolve MMU page fault interrupts
		 * caused by the VGT. It prevents the CP PFP from filling
		 * the VGT DMA request fifo too early, thereby ensuring that
		 * the VGT will not fetch vertex/bin data until after the page
		 * table base register has been updated.
		 *
		 * Two null DRAW_INDX_BIN packets are inserted right after the
		 * page table base update, followed by a wait for idle. The
		 * null packets will fill up the VGT DMA request fifo and
		 * prevent any further vertex/bin updates from occurring until
		 * the wait has finished. */
		*cmds++ = pm4_type3_packet(PM4_SET_CONSTANT, 2);
		*cmds++ = (0x4 << 16) |
				(REG_PA_SU_SC_MODE_CNTL - 0x2000);
		*cmds++ = 0;		/* disable faceness generation */
		*cmds++ = pm4_type3_packet(PM4_SET_BIN_BASE_OFFSET, 1);
		*cmds++ = device->mmu.dummyspace.gpuaddr;
		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX_BIN, 6);
		*cmds++ = 0;		/* viz query info */
		*cmds++ = 0x0003C004;	/* draw indicator */
		*cmds++ = 0;		/* bin base */
		*cmds++ = 3;		/* bin size */
		*cmds++ = device->mmu.dummyspace.gpuaddr; /* dma base */
		*cmds++ = 6;		/* dma size */
		*cmds++ = pm4_type3_packet(PM4_DRAW_INDX_BIN, 6);
		*cmds++ = 0;		/* viz query info */
		*cmds++ = 0x0003C004;	/* draw indicator */
		*cmds++ = 0;		/* bin base */
		*cmds++ = 3;		/* bin size */
		/* dma base */
		*cmds++ = device->mmu.dummyspace.gpuaddr;
		*cmds++ = 6;		/* dma size */
		*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
		*cmds++ = 0x00000000;
		sizedwords += 21;

		/* qcom: invalidate state packet */

		kgsl_ringbuffer_issuecmds(device, 1, &link[0], sizedwords);
	}
	else
	{
		kgsl_yamato_idle(device, GSL_TIMEOUT_DEFAULT);
		kgsl_yamato_regwrite(device, reg_ptbase, ptbase);
	}

	return GSL_SUCCESS;
}

unsigned int kgsl_yamato_getchipid(struct kgsl_device *device)
{
	unsigned int chipid = 0;
	unsigned int coreid, majorid, minorid, patchid, revid;

	kgsl_yamato_regread(device, REG_RBBM_PERIPHID1, &coreid);
	coreid &= 0xF;

	kgsl_yamato_regread(device, REG_RBBM_PERIPHID2, &majorid);
	majorid = (majorid >> 4) & 0xF;

	kgsl_yamato_regread(device, REG_RBBM_PATCH_RELEASE, &revid);

	/* this is a 16bit field, but extremely unlikely it would ever get
	 * this high
	 */
	minorid = ((revid >> 0) & 0xFF);


	patchid = ((revid >> 16) & 0xFF);

	chipid = ((coreid << 24) | (majorid << 16) |
			(minorid << 8) | (patchid << 0));
	return chipid;
}

/* qcom: does a lot of probe style stuff here */
int kgsl_yamato_init(struct kgsl_device *device)
{
	int status = GSL_FAILURE;

	device->flags |= KGSL_FLAGS_INITIALIZED;

	kgsl_pwrctrl(device, GSL_PWRFLAGS_POWER_ON, 100);

	/* We need to make sure all blocks are powered up and clocked
	 * before issuing a soft reset. The overrides will be turned off
	 * (set to 0) later in kgsl_yamato_start */

	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE1, 0xfffffffe);
	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE2, 0xffffffff);

	/* soft reset */
	kgsl_yamato_regwrite(device, REG_RBBM_SOFT_RESET, 0xFFFFFFFF);
	msleep(50);
	kgsl_yamato_regwrite(device, REG_RBBM_SOFT_RESET, 0x00000000);

	/* RBBM control */
	kgsl_yamato_regwrite(device, REG_RBBM_CNTL, 0x00004442);

	/* setup MH arbiter */
	kgsl_yamato_regwrite(device, REG_MH_ARBITER_CONFIG, KGSL_CFG_YAMATO_MHARB);

	/* SQ_*_PROGRAM */
	kgsl_yamato_regwrite(device, REG_SQ_VS_PROGRAM, 0x00000000);
	kgsl_yamato_regwrite(device, REG_SQ_PS_PROGRAM, 0x00000000);

	/* init mmu */
	status = kgsl_mmu_init(device);
	if (status != GSL_SUCCESS) {
		kgsl_yamato_stop(device);
		return status;
	}

	return status;
}

/* qcom: clean up gmem, regspace, irq maps etc. */
int kgsl_yamato_close(struct kgsl_device *device)
{
	if (device->refcnt == 0) {
		/* shutdown mmu */
		kgsl_mmu_close(device);

		/* shutdown interrupt */
		kgsl_pwrctrl(device, GSL_PWRFLAGS_POWER_OFF, 0);

		device->flags &= ~KGSL_FLAGS_INITIALIZED;
	}

	return GSL_SUCCESS;
}

int kgsl_yamato_destroy(struct kgsl_device *device)
{
	/* todo: - hard reset core? */
	kgsl_drawctxt_destroyall(device);

	kgsl_yamato_stop(device);
	kgsl_driver_destroy();
	/* todo: terminate client process? */
	return GSL_SUCCESS;
}

int kgsl_yamato_start(struct kgsl_device *device, unsigned int flags)
{
	int           status = GSL_FAILURE;
	unsigned int  pm1, pm2;

	kgsl_pwrctrl(device, GSL_PWRFLAGS_CLK_ON, 100);

	/* default power management override when running in safe mode */
	pm1 = (device->flags & KGSL_FLAGS_SAFEMODE) ? 0xFFFFFFFE : 0x00000000;
	pm2 = (device->flags & KGSL_FLAGS_SAFEMODE) ? 0x000000FF : 0x00000000;
	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE1, pm1);
	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE2, pm2);

	/* enable RBBM interrupts */
	kgsl_yamato_regwrite(device, REG_RBBM_INT_CNTL, GSL_RBBM_INT_MASK);

	/* make sure SQ interrupts are disabled */
	kgsl_yamato_regwrite(device, REG_SQ_INT_CNTL, 0);

	/* init gmem */
	kgsl_yamato_gmeminit(device);

	/* init ring buffer */
	status = kgsl_ringbuffer_init(device);
	if (status != GSL_SUCCESS) {
		kgsl_yamato_stop(device);
		return status;
	}

	/* init draw context */
	status = kgsl_drawctxt_init(device);
	if (status != GSL_SUCCESS) {
		kgsl_yamato_stop(device);
		return status;
	}

	device->flags |= KGSL_FLAGS_STARTED;

	/* bist here? */
	return (status);
}

int kgsl_yamato_stop(struct kgsl_device *device)
{
	/* HW WORKAROUND: Ringbuffer hangs during next start if it is
	 * stopped without any commands ever being submitted. To avoid this,
	 * submit a dummy wait packet. */
	unsigned int cmds[2];

	cmds[0] = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
	cmds[0] = 0;
	kgsl_ringbuffer_issuecmds(device, 0, cmds, 2);

	kgsl_yamato_regwrite(device, REG_RBBM_INT_CNTL, 0);
	kgsl_yamato_regwrite(device, REG_SQ_INT_CNTL, 0);

	kgsl_drawctxt_close(device);

	/* shutdown ringbuffer */
	kgsl_ringbuffer_close(&device->ringbuffer);

	/* shutdown gmem */
	kgsl_yamato_gmemclose(device);

	if(device->refcnt == 0) {
		kgsl_pwrctrl(device, GSL_PWRFLAGS_CLK_OFF, 0);
	}

	device->flags &= ~KGSL_FLAGS_STARTED;

	return GSL_SUCCESS;
}


int kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value)
{
	unsigned int *reg;

	if (offsetwords * sizeof(unsigned int) >= device->regspace.sizebytes) {
		return GSL_FAILURE; // -ERANGE, print some debug about invalid offset
	}

	reg = (unsigned int *)(device->regspace.mmio_virt_base + (offsetwords << 2));

	*value = readl(reg);

	return GSL_SUCCESS;
}

int kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value)
{
	unsigned int *reg;

	if (offsetwords * sizeof(unsigned int) >= device->regspace.sizebytes) {
		return GSL_FAILURE; // -ERANGE, invalid offset
	}

	reg = (unsigned int *)(device->regspace.mmio_virt_base + (offsetwords << 2));

	writel(value, reg);

	return GSL_SUCCESS;
}

int kgsl_yamato_getproperty(struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes)
{
	int status = GSL_FAILURE;
	(void) sizebytes;

	if (type == KGSL_PROP_DEVICE_INFO) {
		struct kgsl_devinfo  *devinfo = (struct kgsl_devinfo *) value;

		DEBUG_ASSERT(sizebytes == sizeof(struct kgsl_devinfo));

		devinfo->device_id         = device->id;
		devinfo->chip_id           = (unsigned int)device->chip_id;
		devinfo->mmu_enabled       = kgsl_mmu_isenabled(&device->mmu);
		devinfo->gmem_hostbaseaddr = device->gmemspace.mmio_virt_base;
		devinfo->gmem_gpubaseaddr  = device->gmemspace.gpu_base;
		devinfo->gmem_sizebytes    = device->gmemspace.sizebytes;
		devinfo->high_precision    = 0;

		status = GSL_SUCCESS;
	}

	return status;
}

/* qcom comment: Caller must hold the driver mutex */
int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout)
{
	int status = GSL_FAILURE;
	struct kgsl_ringbuffer *rb = &device->ringbuffer;
	unsigned int rbbm_status;
	int idle_count = 0;
#define IDLE_COUNT_MAX 1500000

	(void) timeout;

	// first, wait until the CP has consumed all the commands in the ring buffer
	if (rb->flags & KGSL_FLAGS_STARTED) {
		do {
			idle_count++;
			GSL_RB_GET_READPTR(rb, &rb->rptr);
		} while ((rb->rptr != rb->wptr) && (idle_count < IDLE_COUNT_MAX));

		if (idle_count == IDLE_COUNT_MAX) {
			pr_err("spun too long waiting for RB to idle\n");
			// -EINVAL, now do a ringbuffer dump, mmu dump
			goto done;
		}
	}

	// now, wait for the GPU to finish its operations
	for (idle_count = 0; idle_count < IDLE_COUNT_MAX; idle_count++) {
		kgsl_yamato_regread(device, REG_RBBM_STATUS, &rbbm_status);

		//if (!(rbbm_status & 0x80000000)) {
		if (rbbm_status == 0x110) {
			status = GSL_SUCCESS;
			break;
		}
	}

	if (idle_count == IDLE_COUNT_MAX) {
		pr_err("spun too long waiting for rbbm status to idle\n");
		// -EINVAL, now do a ringbuffer dump, mmu dump
		goto done;
	}
done:
	return status;
}

int kgsl_yamato_check_timestamp(struct kgsl_device *device, unsigned int timestamp)
{
	int i;

	/* Reason to use a wait loop:
	 * When bus is busy, for example vpu is working too, the timestamp is
	 * possibly not yet refreshed to memory by yamato. For most cases, it
	 * will hit on first loop cycle. So it don't effect performance.
	 */
	for (i = 0; i < 10; i++) {
		if (kgsl_cmdstream_check_timestamp(device, timestamp))
			return 1;
		udelay(10);
	}

	return 0;
}

/* qcom comment: MUST be called with the gsl_driver.lock held */
int kgsl_yamato_waittimestamp(struct kgsl_device *device, unsigned int timestamp, unsigned int msecs)
{
	int status = GSL_SUCCESS;
	long timeout;

	timeout = wait_event_interruptible_timeout(device->wait_timestamp_wq,
			kgsl_yamato_check_timestamp(device, timestamp),
			msecs_to_jiffies(msecs));
	if (timeout > 0)
		status = GSL_SUCCESS; // 0
	else if (timeout == 0) {
		// check timestamp?
		status = GSL_FAILURE; // -ETIMEDOUT
		// register dump?
	}
	return status;
}

int kgsl_yamato_runpending(struct kgsl_device *device)
{
	(void) device;
	return GSL_SUCCESS;
}

int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl)
{
	if (ftbl == NULL)
		return GSL_FAILURE;

	ftbl->regread = kgsl_yamato_regread;
	ftbl->regwrite = kgsl_yamato_regwrite;
	ftbl->idle = kgsl_yamato_idle;
	ftbl->start = kgsl_yamato_start;
	ftbl->stop = kgsl_yamato_stop;
	ftbl->getproperty = kgsl_yamato_getproperty;
	ftbl->waittimestamp = kgsl_yamato_waittimestamp;
	ftbl->cmdstream_issueibcmds = kgsl_ringbuffer_issueibcmds;
	ftbl->device_drawctxt_create = kgsl_drawctxt_create;
	ftbl->device_drawctxt_destroy = kgsl_drawctxt_destroy;
	ftbl->mmu_tlbinvalidate = kgsl_yamato_tlbinvalidate;
	ftbl->mmu_setpagetable = kgsl_yamato_setpagetable;
	ftbl->init = kgsl_yamato_init;
	ftbl->close = kgsl_yamato_close;
	ftbl->destroy = kgsl_yamato_destroy;
	ftbl->runpending = kgsl_yamato_runpending;

	return GSL_SUCCESS;
}

/* qcom: missing yamato_isidle? */
