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

#include "gsl.h"
#include "gsl_hal.h"

//  defines
#define GSL_CMDWINDOW_TARGET_MASK       0x000000FF
#define GSL_CMDWINDOW_ADDR_MASK         0x00FFFF00
#define GSL_CMDWINDOW_TARGET_SHIFT      0
#define GSL_CMDWINDOW_ADDR_SHIFT        8

int kgsl_cmdwindow_init(struct kgsl_device *device)
{
	device->cmdwindow_mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (!device->cmdwindow_mutex)
		return GSL_FAILURE;
	mutex_init(device->cmdwindow_mutex);
	return GSL_SUCCESS;
}

int kgsl_cmdwindow_close(struct kgsl_device *device)
{
	if (!device->cmdwindow_mutex)
		return GSL_FAILURE;
	kfree(device->cmdwindow_mutex);
	device->cmdwindow_mutex = NULL;
	return GSL_SUCCESS;
}

//----------------------------------------------------------------------------

int
kgsl_cmdwindow_write0(unsigned int device_id, enum kgsl_cmdwindow_type target, unsigned int addr, unsigned int data)
{
    struct kgsl_device  *device;
    unsigned int  cmdwinaddr;
    unsigned int  cmdstream;

    kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_cmdwindow_write( gsl_device_id_t device_id=%D, enum kgsl_cmdwindow_type target=%d, uint addr=0x%08x, uint data=0x%08x)\n", device_id, target, addr, data );

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    if (target < GSL_CMDWINDOW_MIN || target > GSL_CMDWINDOW_MAX)
    {
        kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_ERROR, "ERROR: Invalid target.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_cmdwindow_write. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    if ((!(device->flags & GSL_FLAGS_INITIALIZED) && target == GSL_CMDWINDOW_MMU) ||
        (!(device->flags & GSL_FLAGS_STARTED)     && target != GSL_CMDWINDOW_MMU))
    {
        kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_ERROR, "ERROR: Invalid device state to write to selected targer.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_cmdwindow_write. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    // set command stream
    if (target == GSL_CMDWINDOW_MMU)
    {
#ifdef GSL_NO_MMU
        return (GSL_SUCCESS);
#endif
        cmdstream = ADDR_VGC_MMUCOMMANDSTREAM;
    }
    else
    {
        cmdstream = ADDR_VGC_COMMANDSTREAM;
    }


    // set command window address
    cmdwinaddr  = ((target << GSL_CMDWINDOW_TARGET_SHIFT) & GSL_CMDWINDOW_TARGET_MASK);
    cmdwinaddr |= ((addr   << GSL_CMDWINDOW_ADDR_SHIFT)   & GSL_CMDWINDOW_ADDR_MASK);

    mutex_lock(device->cmdwindow_mutex);

#ifndef GSL_NO_MMU
    // set mmu pagetable
	kgsl_mmu_setpagetable(device, current->tgid);
#endif

    // write command window address
    device->ftbl.regwrite(device, (cmdstream)>>2, cmdwinaddr);

    // write data
    device->ftbl.regwrite(device, (cmdstream)>>2, data);

    mutex_unlock(device->cmdwindow_mutex);

    kgsl_log_write( KGSL_LOG_GROUP_COMMAND | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_cmdwindow_write. Return value %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_cmdwindow_write(unsigned int device_id, enum kgsl_cmdwindow_type target, unsigned int addr, unsigned int data)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_cmdwindow_write0(device_id, target, addr, data);
	mutex_unlock(&gsl_driver.lock);
	return status;
}
