/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
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

#include "kgsl_types.h"
#include "kgsl_mmu.h"
#include "kgsl_buildconfig.h"
#include "kgsl_hal.h"
#include "kgsl_halconfig.h"
#include "kgsl_memmgr.h"
#include "kgsl_sharedmem.h"
#include "kgsl_properties.h"
#include "kgsl_intrmgr.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_drawctxt.h"
#include "kgsl_cmdwindow.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"
#include "kgsl_linux_map.h"
#include "kgsl_hwaccess.h"

//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

int
kgsl_context_create(unsigned int device_id, gsl_context_type_t type, unsigned int *drawctxt_id, gsl_flags_t flags)
{
    struct kgsl_device* device  = &gsl_driver.device[device_id-1];
    int status;

    mutex_lock(&gsl_driver.lock);

    if (device->ftbl.context_create)
    {
        status = device->ftbl.context_create(device, type, drawctxt_id, flags);
    }
    else
    {
        status = GSL_FAILURE;
    }

    mutex_unlock(&gsl_driver.lock);

    return status;
}

//----------------------------------------------------------------------------

int
kgsl_context_destroy(unsigned int device_id, unsigned int drawctxt_id)
{
    struct kgsl_device* device  = &gsl_driver.device[device_id-1];
    int status;

    mutex_lock(&gsl_driver.lock);

    if (device->ftbl.context_destroy)
    {
        status = device->ftbl.context_destroy(device, drawctxt_id);
    }
    else
    {
        status = GSL_FAILURE;
    }

    mutex_unlock(&gsl_driver.lock);

    return status;
}

//----------------------------------------------------------------------------

