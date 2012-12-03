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

#include <linux/sched.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_driver.h"
#include "kgsl_debug.h"
#include "kgsl_device.h"
#include "kgsl_cmdstream.h"
#include "kgsl_log.h"

//////////////////////////////////////////////////////////////////////////////
//  defines
//////////////////////////////////////////////////////////////////////////////
#define GSL_PROCESSID_NONE          0x00000000

#define GSL_DRVFLAGS_EXTERNAL       0x10000000
#define GSL_DRVFLAGS_INTERNAL       0x20000000


//////////////////////////////////////////////////////////////////////////////
// globals
//////////////////////////////////////////////////////////////////////////////
static unsigned int  gsl_driver_initialized = 0;
struct kgsl_driver        gsl_driver;


//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

int
kgsl_driver_init0(unsigned int flags, unsigned int flags_debug)
{
    int  status = GSL_SUCCESS;

    if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0))
    {
        memset(&gsl_driver, 0, sizeof(struct kgsl_driver));
	mutex_init(&gsl_driver.lock);
    }

    (void) flags_debug;     // unref formal parameter

    if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0))
    {
	mutex_lock(&gsl_driver.lock);

        // init hal
        status = kgsl_hal_init();

        if (status == GSL_SUCCESS)
        {
            gsl_driver_initialized |= flags;
            gsl_driver_initialized |= KGSL_FLAGS_INITIALIZED0;
        }

	mutex_unlock(&gsl_driver.lock);
    }

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_driver_close0(unsigned int flags)
{
    int  status = GSL_SUCCESS;

    if ((gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0) && (gsl_driver_initialized & flags))
    {
	mutex_lock(&gsl_driver.lock);
        // close hal
        status = kgsl_hal_close();
	mutex_unlock(&gsl_driver.lock);

        gsl_driver_initialized &= ~flags;
        gsl_driver_initialized &= ~KGSL_FLAGS_INITIALIZED0;
    }

    return (status);
}

//----------------------------------------------------------------------------

int kgsl_driver_init(void)
{
    // only an external (platform specific device driver) component should call this

    return(kgsl_driver_init0(GSL_DRVFLAGS_EXTERNAL, 0));
}

//----------------------------------------------------------------------------

int kgsl_driver_close(void)
{
    // only an external (platform specific device driver) component should call this

    return(kgsl_driver_close0(GSL_DRVFLAGS_EXTERNAL));
}

//----------------------------------------------------------------------------

int
kgsl_driver_entry(unsigned int flags)
{
    int           status = GSL_FAILURE;
    int           index, i;
    unsigned int  pid;

    if (kgsl_driver_init0(GSL_DRVFLAGS_INTERNAL, flags) != GSL_SUCCESS)
    {
        return (GSL_FAILURE);
    }

	KGSL_DRV_VDBG("flags=%x )\n", flags);

    mutex_lock(&gsl_driver.lock);

    pid = current->tgid;

    // if caller process has not already opened access
    status = kgsl_driver_getcallerprocessindex(pid, &index);
    if (status != GSL_SUCCESS)
    {
        // then, add caller pid to process table
        status = kgsl_driver_getcallerprocessindex(GSL_PROCESSID_NONE, &index);
        if (status == GSL_SUCCESS)
        {
            gsl_driver.callerprocess[index] = pid;
            gsl_driver.refcnt++;
        }
    }

    if (status == GSL_SUCCESS)
    {
        if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED))
        {
            // init memory apertures
            status = kgsl_sharedmem_init(&gsl_driver.shmem);
            if (status == GSL_SUCCESS)
            {
                // init devices
		status = GSL_FAILURE;
                for (i = 0; i < KGSL_DEVICE_MAX; i++)
                {
		    if (kgsl_device_init(&gsl_driver.device[i], (unsigned int)(i + 1)) == GSL_SUCCESS) {
			status = GSL_SUCCESS;
		    }
                }
            }

            if (status == GSL_SUCCESS)
            {
                gsl_driver_initialized |= KGSL_FLAGS_INITIALIZED;
            }
        }

        // walk through process attach callbacks
        if (status == GSL_SUCCESS)
        {
            for (i = 0; i < KGSL_DEVICE_MAX; i++)
            {
                status = kgsl_device_attachcallback(&gsl_driver.device[i], pid);
                if (status != GSL_SUCCESS)
                {
                    break;
                }
            }
        }

        // if something went wrong
        if (status != GSL_SUCCESS)
        {
            // then, remove caller pid from process table
            if (kgsl_driver_getcallerprocessindex(pid, &index) == GSL_SUCCESS)
            {
                gsl_driver.callerprocess[index] = GSL_PROCESSID_NONE;
                gsl_driver.refcnt--;
            }
        }
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_driver_exit0(unsigned int pid)
{
    int  status = GSL_SUCCESS;
    int  index, i;

    mutex_lock(&gsl_driver.lock);

    if (gsl_driver_initialized & KGSL_FLAGS_INITIALIZED)
    {
        if (kgsl_driver_getcallerprocessindex(pid, &index) == GSL_SUCCESS)
        {
            // walk through process detach callbacks
            for (i = 0; i < KGSL_DEVICE_MAX; i++)
            {
                // Empty the freememqueue of this device
                kgsl_cmdstream_memqueue_drain(&gsl_driver.device[i]);

                // Detach callback
                status = kgsl_device_detachcallback(&gsl_driver.device[i], pid);
                if (status != GSL_SUCCESS)
                {
                    break;
                }
            }

            // last running caller process
            if (gsl_driver.refcnt - 1 == 0)
            {
                // close devices
                for (i = 0; i < KGSL_DEVICE_MAX; i++)
                {
                    kgsl_device_close(&gsl_driver.device[i]);
                }

                // shutdown memory apertures
                kgsl_sharedmem_close(&gsl_driver.shmem);

                gsl_driver_initialized &= ~KGSL_FLAGS_INITIALIZED;
            }

            // remove caller pid from process table
            gsl_driver.callerprocess[index] = GSL_PROCESSID_NONE;
            gsl_driver.refcnt--;
        }
    }

    mutex_unlock(&gsl_driver.lock);

    if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED))
    {
        kgsl_driver_close0(GSL_DRVFLAGS_INTERNAL);
    }

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_driver_exit(void)
{
    int status;

	KGSL_DRV_VDBG("exit\n");

    status = kgsl_driver_exit0(current->tgid);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_driver_destroy(unsigned int pid)
{
    return (kgsl_driver_exit0(pid));
}
