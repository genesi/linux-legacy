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
#include "kgsl_hal.h"
#include "kgsl_cmdstream.h"
#include "kgsl_sharedmem.h"
#include "kgsl_driver.h"
#include "kgsl_g12_drawctxt.h"
#include "kgsl_g12_cmdstream.h"

#include "g12_reg.h"
#include "kgsl_g12_vgv3types.h"

#define GSL_VGC_INT_MASK \
	(REG_VGC_IRQSTATUS__MH_MASK | \
	 REG_VGC_IRQSTATUS__G2D_MASK | \
	 REG_VGC_IRQSTATUS__FIFO_MASK)

#ifdef CONFIG_ARCH_MX35
#define V3_SYNC
#endif

#define GSL_IRQ_TIMEOUT         200

int kgsl_g12_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
int kgsl_g12_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value);
int kgsl_g12_stop(struct kgsl_device *device);

/* G12 MH arbiter config*/
#define KGSL_G12_CFG_G12_MHARB \
	(0x10 \
		| (0 << MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT) \
		| (0x8 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT))

#define FIRST_TIMEOUT (HZ / 2)
#define INTERVAL_TIMEOUT (HZ / 10)

//#define KGSL_G12_TIMESTAMP_EPSILON 20000
#define KGSL_G12_IDLE_COUNT_MAX 1000000

static struct timer_list idle_timer;
static struct work_struct idle_check;

static unsigned int drawctx_id  = 0;
static int kgsl_g12_idle(struct kgsl_device *device, unsigned int timeout);

static void kgsl_g12_updatetimestamp(struct kgsl_device *device);

static void kgsl_g12_timer(unsigned long data)
{
	/* Have work run in a non-interrupt context */
	schedule_work(&idle_check);
}

static void kgsl_g12_irqtask(struct work_struct *work)
{
	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_G12-1];

	kgsl_g12_updatetimestamp(device);
	//pr_info("%s: waking waitqueue\n", __func__);
	wake_up_interruptible_all(&device->wait_timestamp_wq);
}

irqreturn_t kgsl_g12_isr(int irq, void *data)
{
	irqreturn_t result = IRQ_NONE;

	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_G12-1];
	unsigned int status;
	kgsl_g12_regread(device, ADDR_VGC_IRQSTATUS >> 2, &status);

	if (status & GSL_VGC_INT_MASK) {
		kgsl_g12_regwrite(device,
			ADDR_VGC_IRQSTATUS >> 2, status & GSL_VGC_INT_MASK);

		result = IRQ_HANDLED;

		if (status & REG_VGC_IRQSTATUS__FIFO_MASK)
                        pr_err("g12 fifo interrupt\n");
                else if (status & REG_VGC_IRQSTATUS__MH_MASK)
                        pr_err("g12 mh interrupt\n");
                else if (status & REG_VGC_IRQSTATUS__G2D_MASK) {
			//pr_err("g12 g2d interrupt\n");
			queue_work(device->irq_wq, &(device->irq_work));
		} else {
			pr_err("bad bits in ADDR_VGC_IRQ_STATUS %08x\n", status);
		}
	}

	mod_timer(&idle_timer, jiffies + INTERVAL_TIMEOUT);

	return result;
}

/* qcom: kgsl_g12_setstate flags & GSL_MMUFLAGS_TLBFLUSH */
int kgsl_g12_tlbinvalidate(struct kgsl_device *device, unsigned int reg_invalidate)
{
#ifdef CONFIG_KGSL_MMU_ENABLE
	unsigned int mh_mmu_invalidate = 0x00000003L; /* invalidate all and tc */

	kgsl_g12_regwrite(device, reg_invalidate, *(unsigned int *) &mh_mmu_invalidate);
#endif
	return GSL_SUCCESS;
}

/* qcom: kgsl_g12_setstate flags & GSL_MMUFLAGS_PTUPDATE. don't pass PTBASE! */
int kgsl_g12_setpagetable(struct kgsl_device *device, unsigned int reg_ptbase, uint32_t ptbase)
{
#ifdef CONFIG_KGSL_MMU_ENABLE
	kgsl_g12_idle(device, GSL_TIMEOUT_DEFAULT);
	kgsl_g12_regwrite(device, reg_ptbase, ptbase);

	/* qcom: writes to MMU_VA_RANGE, MMU_INVALIDATE */
#endif
	return GSL_SUCCESS;
}

/* this seems to get run during issueibcmds which is holding a lock.. so it stalls. locks are in qualcomms */
void kgsl_g12_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_G12-1];
	mutex_lock(&gsl_driver.lock);
	if (device->flags & KGSL_FLAGS_STARTED) {
//		if (kgsl_g12_sleep(device, false) == GSL_FAILURE)
			mod_timer(&idle_timer, jiffies + INTERVAL_TIMEOUT);
	}
	mutex_unlock(&gsl_driver.lock);
}

static void kgsl_g12_updatetimestamp(struct kgsl_device *device)
{
	unsigned int count = 0;

	kgsl_g12_regread(device, (ADDR_VGC_IRQ_ACTIVE_CNT >> 2), &count);

	//pr_info("%s: count %d (>>8 %d &255 %d), current timestamp %d\n", __func__,
	//		count, count>>8, (count>>8)&255, device->timestamp);
	count >>= 8;
	count &= 255;
	device->timestamp += count;
#ifdef V3_SYNC
	if (device->current_timestamp > device->timestamp)
	{
	    kgsl_g12_cmdwindow_write(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 2);
	    kgsl_g12_cmdwindow_write(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 0);
	}
#endif
	//pr_info("%s: update memstore\n", __func__);
	kgsl_sharedmem_write(&device->memstore, KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp), &device->timestamp, 4, 0);
}

int kgsl_g12_init(struct kgsl_device *device)
{
	int status = GSL_FAILURE;

	/* qcom: if init already, return */

	device->flags |= KGSL_FLAGS_INITIALIZED;

	/* qcom: init waitqueue head */

	kgsl_pwrctrl(device, GSL_PWRFLAGS_POWER_ON, 100);

	/* create thread for IRQ handling - qcom: z1xx_sync_wq*/
	device->irq_wq = create_singlethread_workqueue("z160_workqueue");
	INIT_WORK(&device->irq_work, kgsl_g12_irqtask);

	/* setup MH arbiter - MH offsets are considered to be dword based, therefore no down shift */
	kgsl_g12_regwrite(device, ADDR_MH_ARBITER_CONFIG, KGSL_G12_CFG_G12_MHARB);

	/* enable irq - qualcomm never turns these off? */
	pr_err("%s: enable irqs mask 0x%08lx\n", __func__, GSL_VGC_INT_MASK);
	kgsl_g12_regwrite(device, (ADDR_VGC_IRQENABLE >> 2),  GSL_VGC_INT_MASK);

#ifdef CONFIG_KGSL_MMU_ENABLE
	/* init mmu */
	status = kgsl_mmu_init(device);
	if (status != GSL_SUCCESS) {
		kgsl_g12_stop(device);
		return status;
	}
#endif

	return status;
}

int kgsl_g12_close(struct kgsl_device *device)
{
	int status = GSL_FAILURE;

	if (device->refcnt == 0) {
		/* wait pending interrupts before shutting down G12 intr thread to
		 * empty irq counters. Otherwise there's a possibility to have them in
		 * registers next time systems starts up and this results in a hang. */

		status = kgsl_g12_idle(device, 1000);

		/* disable irq - qualcomm never turns these off? otherwise the workqueue will crash */
		pr_err("%s: disable irqs\n", __func__);
		kgsl_g12_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 0x0);

		/* qcom: this is in stop */
		if (device->irq_wq) {
			destroy_workqueue(device->irq_wq);
			device->irq_wq = NULL;
		}

		/* shutdown command window */
		kgsl_g12_cmdwindow_close(device);

#ifdef CONFIG_KGSL_MMU_ENABLE
		/* shutdown mmu */
		kgsl_mmu_close(device);
#endif

		kgsl_pwrctrl(device, GSL_PWRFLAGS_POWER_OFF, 0);

		device->flags &= ~KGSL_FLAGS_INITIALIZED;

		drawctx_id = 0;

		BUG_ON(g_z1xx.numcontext == 0);
		memset(&g_z1xx, 0, sizeof(struct kgsl_g12_z1xx));
	}

	return status;
}

int kgsl_g12_destroy(struct kgsl_device *device)
{
	/* todo: hard reset core? */
	kgsl_g12_stop(device);
	kgsl_driver_destroy();
	/* todo: terminate client process? */
	return GSL_SUCCESS;
}

int kgsl_g12_start(struct kgsl_device *device, unsigned int flags)
{
	int  status = GSL_SUCCESS;
	(void) flags;

	kgsl_pwrctrl(device, GSL_PWRFLAGS_CLK_ON, 100);

	/* qcom: bitch if not initialized */

	if (device->flags & KGSL_FLAGS_STARTED) {
		/* already started */
		return GSL_SUCCESS;
	}

	status = kgsl_g12_cmdwindow_init(device);
	if (status != GSL_SUCCESS) {
		kgsl_g12_stop(device);
		return status;
	}

	device->flags |= KGSL_FLAGS_STARTED;

	init_timer(&idle_timer);
	idle_timer.function = kgsl_g12_timer;
	idle_timer.expires = jiffies + FIRST_TIMEOUT;
	add_timer(&idle_timer);

	INIT_WORK(&idle_check, kgsl_g12_idle_check);

	/* qcom: init ringbuffer.memqueue */

	return status;
}

int kgsl_g12_stop(struct kgsl_device *device)
{
	int status;

	del_timer(&idle_timer);

	/* wait for device to idle before setting it's clock off */
	if (device->flags & KGSL_FLAGS_STARTED) {
		status = kgsl_g12_idle(device, 1000);
		device->flags &= ~KGSL_FLAGS_STARTED;
	}

	/* this should probably be in last_release_locked */
	status = kgsl_pwrctrl(device, GSL_PWRFLAGS_CLK_OFF, 0);

	return status;
}

int kgsl_g12_getproperty(struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes)
{
	int status = GSL_FAILURE;
	(void) sizebytes;

	if (type == KGSL_PROP_DEVICE_INFO) {
		struct kgsl_devinfo  *devinfo = (struct kgsl_devinfo *) value;

		/* BUG_ON? */
		DEBUG_ASSERT(sizebytes == sizeof(struct kgsl_devinfo));

		devinfo->device_id   = device->id;
		devinfo->chip_id     = (unsigned int)device->chip_id;
#ifdef CONFIG_KGSL_MMU_ENABLE
		devinfo->mmu_enabled = kgsl_mmu_isenabled(&device->mmu);
#endif
		if (z160_version == 1)
			devinfo->high_precision = 1;
		else
			devinfo->high_precision = 0;

		status = GSL_SUCCESS;
	}
	return status;
}

int kgsl_g12_idle(struct kgsl_device *device, unsigned int timeout)
{
	if ( device->flags & KGSL_FLAGS_STARTED )
	{
		for ( ; ; )
		{
			unsigned int retired = kgsl_cmdstream_readtimestamp( device, KGSL_TIMESTAMP_RETIRED );
			unsigned int ts_diff = retired - device->current_timestamp;
			if ( ts_diff >= 0 || ts_diff < -KGSL_TIMESTAMP_EPSILON )
				break;
			msleep(10);
		}
	}

    return (GSL_SUCCESS);
}

int kgsl_g12_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value)
{
	unsigned int *reg;
	// G12 MH register values can only be retrieved via dedicated read registers
	if ((offsetwords >= ADDR_MH_ARBITER_CONFIG &&
	     offsetwords <= ADDR_MH_AXI_HALT_CONTROL) ||
	    (offsetwords >= ADDR_MH_MMU_CONFIG     &&
	     offsetwords <= ADDR_MH_MMU_MPU_END)) {
		kgsl_g12_regwrite(device, (ADDR_VGC_MH_READ_ADDR >> 2), offsetwords);
#ifdef _Z180
		reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (ADDR_VGC_MH_READ_ADDR << 2));
#else
		reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (ADDR_VGC_MH_DATA_ADDR << 2));
#endif
	} else {
		if (offsetwords * sizeof(unsigned int) >= device->regspace.sizebytes) {
			pr_err("g12 read invalid offset %d\n", offsetwords);
			return GSL_FAILURE;//-ERANGE
		}

		reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (offsetwords << 2));
	}

	*value = readl(reg);
	return GSL_SUCCESS;
}

int kgsl_g12_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value)
{
	unsigned int *reg;
	// G12 MH registers can only be written via the command window
	if ((offsetwords >= ADDR_MH_ARBITER_CONFIG &&
	     offsetwords <= ADDR_MH_AXI_HALT_CONTROL) ||
	    (offsetwords >= ADDR_MH_MMU_CONFIG     &&
	     offsetwords <= ADDR_MH_MMU_MPU_END)) {
		kgsl_g12_cmdwindow_write(device, GSL_CMDWINDOW_MMU, offsetwords, value);
	} else {
		if (offsetwords * sizeof(unsigned int) >= device->regspace.sizebytes) {
			pr_err("g12 write invalid offset %d\n", offsetwords);
			return GSL_FAILURE;//-ERANGE
		}

		reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (offsetwords << 2));
		writel(value, reg);
		/* Drain write buffer */
		dsb();

		/* Memory fence to ensure all data has posted.  On some systems,
		 * like 7x27, the register block is not allocated as strongly
		 * ordered memory.  Adding a memory fence ensures ordering
		 * during ringbuffer submits.*/
                mb();
	}

	return GSL_SUCCESS;
}

int kgsl_g12_waittimestamp(struct kgsl_device *device, unsigned int timestamp, unsigned int timeout)
{
	/* qcom: calls kgsl_g12_cmdstream_check_timestamp */
	int status = wait_event_interruptible_timeout(device->wait_timestamp_wq,
				kgsl_cmdstream_check_timestamp(device, timestamp),
					msecs_to_jiffies(timeout));
	if (status > 0)
		return GSL_SUCCESS;
	else
		return GSL_FAILURE;
}

int kgsl_g12_getfunctable(struct kgsl_functable *ftbl)
{
	if (ftbl == NULL)
		return GSL_FAILURE;

	ftbl->init = kgsl_g12_init;
	ftbl->close = kgsl_g12_close;
	ftbl->destroy = kgsl_g12_destroy;
	ftbl->start = kgsl_g12_start;
	ftbl->stop = kgsl_g12_stop;
	ftbl->getproperty = kgsl_g12_getproperty;
	ftbl->idle = kgsl_g12_idle;
	ftbl->regread = kgsl_g12_regread;
	ftbl->regwrite = kgsl_g12_regwrite;
	ftbl->waittimestamp = kgsl_g12_waittimestamp;
	ftbl->runpending = NULL;
	ftbl->mmu_tlbinvalidate	= kgsl_g12_tlbinvalidate;
	ftbl->mmu_setpagetable = kgsl_g12_setpagetable;
	ftbl->cmdstream_issueibcmds = kgsl_g12_cmdstream_issueibcmds;
	ftbl->device_drawctxt_create = kgsl_g12_drawctxt_create;
	ftbl->device_drawctxt_destroy = kgsl_g12_drawctxt_destroy;

	return GSL_SUCCESS;
}
