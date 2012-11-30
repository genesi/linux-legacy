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

#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_cmdstream.h"
#include "kgsl_sharedmem.h"
#include "kgsl_driver.h"
#include "kgsl_ioctl.h"
#include "kgsl_g12_drawctxt.h"
#include "kgsl_g12_cmdstream.h"

#include "g12_reg.h"
#include "kgsl_g12_vgv3types.h"

#ifdef CONFIG_ARCH_MX35
#define V3_SYNC
#endif

#define GSL_IRQ_TIMEOUT         200

int kgsl_g12_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value);
int kgsl_g12_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value);

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

static unsigned int drawctx_id  = 0;
static int kgsl_g12_idle(struct kgsl_device *device, unsigned int timeout);

void
kgsl_g12_intrcallback(gsl_intrid_t id, void *cookie)
{
    struct kgsl_device  *device = (struct kgsl_device *) cookie;

    switch(id)
    {
        // non-error condition interrupt
        case GSL_INTR_G12_G2D:
			queue_work(device->irq_workq, &(device->irq_work));
			break;
#ifndef _Z180
        case GSL_INTR_G12_FBC:
            // signal intr completion event
            complete_all(&device->intr.evnt[id]);
            break;
#endif //_Z180

        // error condition interrupt
        case GSL_INTR_G12_FIFO:
		printk(KERN_ERR "GPU: Z160 FIFO Error\n");
		schedule_work(&device->irq_err_work);
		break;

        case GSL_INTR_G12_MH:
            // don't do anything. this is handled by the MMU manager
            break;

        default:
            break;
    }
}

int
kgsl_g12_isr(struct kgsl_device *device)
{
    unsigned int           status;
#ifdef _DEBUG
    REG_MH_MMU_PAGE_FAULT  page_fault = {0};
    REG_MH_AXI_ERROR       axi_error  = {0};
#endif // DEBUG

    // determine if G12 is interrupting
    kgsl_g12_regread(device, (ADDR_VGC_IRQSTATUS >> 2), &status);

    if (status)
    {
        // if G12 MH is interrupting, clear MH block interrupt first, then master G12 MH interrupt
        if (status & REG_VGC_IRQSTATUS__MH_MASK)
        {
#ifdef _DEBUG
            // obtain mh error information
            kgsl_g12_regread(device, ADDR_MH_MMU_PAGE_FAULT, (unsigned int *)&page_fault);
            kgsl_g12_regread(device, ADDR_MH_AXI_ERROR, (unsigned int *)&axi_error);
#endif // DEBUG

            kgsl_intr_decode(device, GSL_INTR_BLOCK_G12_MH);
        }

        kgsl_intr_decode(device, GSL_INTR_BLOCK_G12);
    }

    return (GSL_SUCCESS);
}

int
kgsl_g12_tlbinvalidate(struct kgsl_device *device, unsigned int reg_invalidate, unsigned int pid)
{
#ifdef CONFIG_KGSL_MMU_ENABLE
    unsigned int mh_mmu_invalidate = 0x00000003L; // invalidate all and tc

    // unreferenced formal parameter
    (void) pid;

    kgsl_g12_regwrite(device, reg_invalidate, *(unsigned int *) &mh_mmu_invalidate);
#else
    (void)device;
    (void)reg_invalidate;
#endif
    return (GSL_SUCCESS);
}

int
kgsl_g12_setpagetable(struct kgsl_device *device, unsigned int reg_ptbase, uint32_t ptbase, unsigned int pid)
{
	// unreferenced formal parameter
	(void) pid;
#ifdef CONFIG_KGSL_MMU_ENABLE
    device->ftbl.idle(device, GSL_TIMEOUT_DEFAULT);
	kgsl_g12_regwrite(device, reg_ptbase, ptbase);
#else
    (void)device;
    (void)reg_ptbase;
    (void)reg_varange;
#endif
    return (GSL_SUCCESS);
}

static void kgsl_g12_updatetimestamp(struct kgsl_device *device)
{
	unsigned int count = 0;
	kgsl_g12_regread(device, (ADDR_VGC_IRQ_ACTIVE_CNT >> 2), &count);
	count >>= 8;
	count &= 255;
	device->timestamp += count;
#ifdef V3_SYNC
	if (device->current_timestamp > device->timestamp)
	{
	    kgsl_cmdwindow_write0(2, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 2);
	    kgsl_cmdwindow_write0(2, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 0);
	}
#endif
	kgsl_sharedmem_write0(&device->memstore, KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp), &device->timestamp, 4, 0);
}

static void kgsl_g12_irqtask(struct work_struct *work)
{
	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_G12-1];
	kgsl_g12_updatetimestamp(device);
	wake_up_interruptible_all(&device->timestamp_waitq);
}

static void kgsl_g12_irqerr(struct work_struct *work)
{
	struct kgsl_device *device = &gsl_driver.device[KGSL_DEVICE_G12-1];
	device->ftbl.destroy(device);
}


int
kgsl_g12_init(struct kgsl_device *device)
{
    int  status = GSL_FAILURE;

    device->flags |= GSL_FLAGS_INITIALIZED;

    kgsl_hal_setpowerstate(device->id, GSL_PWRFLAGS_POWER_ON, 100);

    // setup MH arbiter - MH offsets are considered to be dword based, therefore no down shift
    kgsl_g12_regwrite(device, ADDR_MH_ARBITER_CONFIG, KGSL_G12_CFG_G12_MHARB);

    // init interrupt
    status = kgsl_intr_init(device);
    if (status != GSL_SUCCESS)
    {
        device->ftbl.stop(device);
        return (status);
    }

    // enable irq
    kgsl_g12_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 0x3);

#ifdef CONFIG_KGSL_MMU_ENABLE
    // enable master interrupt for G12 MH
    kgsl_intr_attach(&device->intr, GSL_INTR_G12_MH, kgsl_g12_intrcallback, (void *) device);
    kgsl_intr_enable(&device->intr, GSL_INTR_G12_MH);

    // init mmu
    status = kgsl_mmu_init(device);
    if (status != GSL_SUCCESS)
    {
        device->ftbl.stop(device);
        return (status);
    }
#endif

    // enable interrupts
    kgsl_intr_attach(&device->intr, GSL_INTR_G12_G2D,  kgsl_g12_intrcallback, (void *) device);
    kgsl_intr_attach(&device->intr, GSL_INTR_G12_FIFO, kgsl_g12_intrcallback, (void *) device);
    kgsl_intr_enable(&device->intr, GSL_INTR_G12_G2D);
    kgsl_intr_enable(&device->intr, GSL_INTR_G12_FIFO);

#ifndef _Z180
    kgsl_intr_attach(&device->intr, GSL_INTR_G12_FBC,  kgsl_g12_intrcallback, (void *) device);
  //kgsl_intr_enable(&device->intr, GSL_INTR_G12_FBC);
#endif //_Z180

    // create thread for IRQ handling
    device->irq_workq = create_singlethread_workqueue("z160_workqueue");
    INIT_WORK(&device->irq_work, kgsl_g12_irqtask);
    INIT_WORK(&device->irq_err_work, kgsl_g12_irqerr);

    return (status);
}

int
kgsl_g12_close(struct kgsl_device *device)
{
    int status = GSL_FAILURE; 

    if (device->refcnt == 0)
    {
        // wait pending interrupts before shutting down G12 intr thread to
        // empty irq counters. Otherwise there's a possibility to have them in
        // registers next time systems starts up and this results in a hang.
        status = device->ftbl.idle(device, 1000);
        DEBUG_ASSERT(status == GSL_SUCCESS);

	destroy_workqueue(device->irq_workq);

        // shutdown command window
        kgsl_cmdwindow_close(device);

#ifdef CONFIG_KGSL_MMU_ENABLE
        // shutdown mmu
        kgsl_mmu_close(device);
#endif
        // disable interrupts
        kgsl_intr_detach(&device->intr, GSL_INTR_G12_MH);
        kgsl_intr_detach(&device->intr, GSL_INTR_G12_G2D);
        kgsl_intr_detach(&device->intr, GSL_INTR_G12_FIFO);
#ifndef _Z180
        kgsl_intr_detach(&device->intr, GSL_INTR_G12_FBC);
#endif //_Z180

        // shutdown interrupt
        kgsl_intr_close(device);

        kgsl_hal_setpowerstate(device->id, GSL_PWRFLAGS_POWER_OFF, 0);

        device->flags &= ~GSL_FLAGS_INITIALIZED;

        drawctx_id = 0;

        DEBUG_ASSERT(g_z1xx.numcontext == 0);

	memset(&g_z1xx, 0, sizeof(struct kgsl_g12_z1xx));
    }

    return (GSL_SUCCESS);
}

int
kgsl_g12_destroy(struct kgsl_device *device)
{
    int           i;
    unsigned int  pid;

#ifdef _DEBUG
    // for now, signal catastrophic failure in a brute force way
    DEBUG_ASSERT(0);
#endif // _DEBUG

    //todo: hard reset core?

    for (i = 0; i < GSL_CALLER_PROCESS_MAX; i++)
    {
        pid = device->callerprocess[i];
        if (pid)
        {
            device->ftbl.stop(device);
            kgsl_driver_destroy(pid);

            // todo: terminate client process?
        }
    }

    return (GSL_SUCCESS);
}

int
kgsl_g12_start(struct kgsl_device *device, unsigned int flags)
{
    int  status = GSL_SUCCESS;

    (void) flags;       // unreferenced formal parameter

    kgsl_hal_setpowerstate(device->id, GSL_PWRFLAGS_CLK_ON, 100);

    // init command window
    status = kgsl_cmdwindow_init(device);
    if (status != GSL_SUCCESS)
    {
        device->ftbl.stop(device);
        return (status);
    }

    DEBUG_ASSERT(g_z1xx.numcontext == 0);

    device->flags |= GSL_FLAGS_STARTED;

    return (status);
}

int
kgsl_g12_stop(struct kgsl_device *device)
{
    int status;

    DEBUG_ASSERT(device->refcnt == 0);

    /* wait for device to idle before setting it's clock off */
    status = device->ftbl.idle(device, 1000);
    DEBUG_ASSERT(status == GSL_SUCCESS);

    status = kgsl_hal_setpowerstate(device->id, GSL_PWRFLAGS_CLK_OFF, 0);
    device->flags &= ~GSL_FLAGS_STARTED;

    return (status);
}

int
kgsl_g12_getproperty(struct kgsl_device *device, gsl_property_type_t type, void *value, unsigned int sizebytes)
{
    int  status = GSL_FAILURE;
    // unreferenced formal parameter
    (void) sizebytes;

    if (type == GSL_PROP_DEVICE_INFO)
    {
        struct kgsl_devinfo  *devinfo = (struct kgsl_devinfo *) value;

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

    return (status);
}

int
kgsl_g12_setproperty(struct kgsl_device *device, gsl_property_type_t type, void *value, unsigned int sizebytes)
{
    int  status = GSL_FAILURE;

    // unreferenced formal parameters
    (void) device;

    if (type == GSL_PROP_DEVICE_POWER)
    {
        gsl_powerprop_t  *power = (gsl_powerprop_t *) value;

        DEBUG_ASSERT(sizebytes == sizeof(gsl_powerprop_t));

        if (!(device->flags & GSL_FLAGS_SAFEMODE))
        {
            kgsl_hal_setpowerstate(device->id, power->flags, power->value);
        }

        status = GSL_SUCCESS;
    }
    return (status);
}

int
kgsl_g12_idle(struct kgsl_device *device, unsigned int timeout)
{
	if ( device->flags & GSL_FLAGS_STARTED )
	{
		for ( ; ; )
		{
			unsigned int retired = kgsl_cmdstream_readtimestamp0( device->id, GSL_TIMESTAMP_RETIRED );
			unsigned int ts_diff = retired - device->current_timestamp;
			if ( ts_diff >= 0 || ts_diff < -GSL_TIMESTAMP_EPSILON )
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
		kgsl_cmdwindow_write0(device->id, GSL_CMDWINDOW_MMU, offsetwords, value);
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

int
kgsl_g12_waitirq(struct kgsl_device *device, gsl_intrid_t intr_id, unsigned int *count, unsigned int timeout)
{
    int  status = GSL_FAILURE_NOTSUPPORTED;
    int complete = 0;

#ifndef _Z180
    if (intr_id == GSL_INTR_G12_G2D || intr_id == GSL_INTR_G12_FBC)
#else
        if (intr_id == GSL_INTR_G12_G2D)
#endif //_Z180
        {
            if (kgsl_intr_isenabled(&device->intr, intr_id) == GSL_SUCCESS)
            {
                // wait until intr completion event is received and check that
                // the interrupt is still enabled. If event is received, but
                // interrupt is not enabled any more, the driver is shutting
                // down and event structure is not valid anymore.

		if (timeout != OS_INFINITE)
			complete = wait_for_completion_timeout(&device->intr.evnt[intr_id], msecs_to_jiffies(timeout));
		else
			complete = wait_for_completion_killable(&device->intr.evnt[intr_id]);

                if (complete && kgsl_intr_isenabled(&device->intr, intr_id) == GSL_SUCCESS)
                {
                    unsigned int cntrs;
                    int          i;
		    struct completion *comp = &device->intr.evnt[intr_id];

                    kgsl_device_active(device);

                    INIT_COMPLETION(*comp);
                    kgsl_g12_regread(device, (ADDR_VGC_IRQ_ACTIVE_CNT >> 2), &cntrs);

                    for (i = 0; i < GSL_G12_INTR_COUNT; i++)
                    {
                        int intrcnt = cntrs >> ((8 * i)) & 255;

                        // maximum allowed counter value is 254. if set to 255 then something has gone wrong
                        if (intrcnt && (intrcnt < 0xFF))
                        {
                            device->intrcnt[i] += intrcnt;
                        }
                    }

                    *count = device->intrcnt[intr_id - GSL_INTR_G12_MH];
                    device->intrcnt[intr_id - GSL_INTR_G12_MH] = 0;
                    status = GSL_SUCCESS;
                }
                else
                {
                    status = GSL_FAILURE_TIMEOUT;
                }
            }
        }
    else if(intr_id == GSL_INTR_FOOBAR)
    {
        if (kgsl_intr_isenabled(&device->intr, GSL_INTR_G12_G2D) == GSL_SUCCESS)
        {
            complete_all(&device->intr.evnt[GSL_INTR_G12_G2D]);
        }
    }

    return (status);
}

int
kgsl_g12_waittimestamp(struct kgsl_device *device, unsigned int timestamp, unsigned int timeout)
{
	int status = wait_event_interruptible_timeout(device->timestamp_waitq,
	                                              kgsl_cmdstream_check_timestamp(device->id, timestamp),
												  msecs_to_jiffies(timeout));
	if (status > 0)
		return GSL_SUCCESS;
	else
		return GSL_FAILURE;
}

int
kgsl_g12_getfunctable(struct kgsl_functable *ftbl)
{
	ftbl->init		= kgsl_g12_init;
	ftbl->close		= kgsl_g12_close;
	ftbl->destroy		= kgsl_g12_destroy;
	ftbl->start		= kgsl_g12_start;
	ftbl->stop		= kgsl_g12_stop;
	ftbl->getproperty	= kgsl_g12_getproperty;
	ftbl->setproperty	= kgsl_g12_setproperty;
	ftbl->idle		= kgsl_g12_idle;
	ftbl->regread		= kgsl_g12_regread;
	ftbl->regwrite		= kgsl_g12_regwrite;
	ftbl->waitirq		= kgsl_g12_waitirq;
	ftbl->waittimestamp	= kgsl_g12_waittimestamp;
	ftbl->runpending	= NULL;
	ftbl->addtimestamp	= kgsl_g12_addtimestamp;
	ftbl->intr_isr		= kgsl_g12_isr;
	ftbl->mmu_tlbinvalidate	= kgsl_g12_tlbinvalidate;
	ftbl->mmu_setpagetable	= kgsl_g12_setpagetable;
	ftbl->cmdstream_issueibcmds	= kgsl_g12_issueibcmds;
	ftbl->device_drawctxt_create	= kgsl_g12_drawctxt_create;
	ftbl->device_drawctxt_destroy	= kgsl_g12_drawctxt_destroy;

	return GSL_SUCCESS;
}
