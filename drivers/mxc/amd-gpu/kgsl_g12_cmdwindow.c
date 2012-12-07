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

#include <linux/sched.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_buildconfig.h"

#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"
#include "kgsl_log.h"

#include "g12_reg.h"

#define KGSL_CMDWINDOW_TARGET_MASK       0x000000FF
#define KGSL_CMDWINDOW_ADDR_MASK         0x00FFFF00
#define KGSL_CMDWINDOW_TARGET_SHIFT      0
#define KGSL_CMDWINDOW_ADDR_SHIFT        8

int kgsl_g12_regwrite(struct kgsl_device *device, unsigned int offsetwords, unsigned int value);

int kgsl_g12_cmdwindow_init(struct kgsl_device *device)
{
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	device->cmdwindow_mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (!device->cmdwindow_mutex)
		return GSL_FAILURE;
	mutex_init(device->cmdwindow_mutex);
#endif
	return GSL_SUCCESS;
}

int kgsl_g12_cmdwindow_close(struct kgsl_device *device)
{
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	if (!device->cmdwindow_mutex)
		return GSL_FAILURE;
	kfree(device->cmdwindow_mutex);
	device->cmdwindow_mutex = NULL;
#endif
	return GSL_SUCCESS;
}

int kgsl_g12_cmdwindow_write(struct kgsl_device *device,
		enum kgsl_cmdwindow_type target, unsigned int addr,
		unsigned int data)
{
	unsigned int  cmdwinaddr;
	unsigned int  cmdstream;

	KGSL_DRV_INFO("enter (device=%p,addr=%08x,data=0x%x)\n", device, addr, data);

	if (target < GSL_CMDWINDOW_MIN || target > GSL_CMDWINDOW_MAX)
	{
		KGSL_DRV_ERR("dev %p invalid target\n", device);
		return GSL_FAILURE;
	}

	/* qcom: no differentiation between init or started? */
	if ((!(device->flags & KGSL_FLAGS_INITIALIZED) && target == GSL_CMDWINDOW_MMU) ||
	    (!(device->flags & KGSL_FLAGS_STARTED)     && target != GSL_CMDWINDOW_MMU)) {
		KGSL_DRV_ERR("Trying to write uninitialized device.\n");
		return GSL_FAILURE;
	}

	if (target == GSL_CMDWINDOW_MMU) {
#ifndef CONFIG_KGSL_MMU_ENABLE
		return GSL_SUCCESS;
#endif
		cmdstream = ADDR_VGC_MMUCOMMANDSTREAM;
	} else {
		cmdstream = ADDR_VGC_COMMANDSTREAM;
	}

	cmdwinaddr = ((target << KGSL_CMDWINDOW_TARGET_SHIFT) &
			KGSL_CMDWINDOW_TARGET_MASK);
	cmdwinaddr |= ((addr   << KGSL_CMDWINDOW_ADDR_SHIFT) &
			KGSL_CMDWINDOW_ADDR_MASK);

#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	mutex_lock(device->cmdwindow_mutex);
#endif
	/* qcom: no mmu mess here */
#ifdef CONFIG_KGSL_MMU_ENABLE
	kgsl_mmu_setpagetable(device);
#endif

	/* newer qualcomm drivers spinlock_irqsave around this
	 * there's also a pre_hwaccess above it
	 */
	kgsl_g12_regwrite(device, cmdstream >> 2, cmdwinaddr);
	kgsl_g12_regwrite(device, cmdstream >> 2, data);

#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	mutex_unlock(device->cmdwindow_mutex);
#endif

	return GSL_SUCCESS;
}
