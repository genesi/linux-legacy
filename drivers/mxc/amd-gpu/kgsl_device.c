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

#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_log.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"
#include "kgsl_cmdstream.h"
#include "kgsl_debug.h"

int kgsl_device_init(struct kgsl_device *device, unsigned int device_id)
{
    int              status = GSL_SUCCESS;
    struct kgsl_devconfig  config;
    gsl_hal_t        *hal = (gsl_hal_t *)gsl_driver.hal;

    KGSL_DRV_VDBG("device=0x%08x device_id=%d \n", (unsigned int) device, device_id );

    if ((KGSL_DEVICE_YAMATO == device_id) && !(hal->has_z430)) {
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if ((KGSL_DEVICE_G12 == device_id) && !(hal->has_z160)) {
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if (device->flags & KGSL_FLAGS_INITIALIZED)
    {
        return (GSL_SUCCESS);
    }

    memset(device, 0, sizeof(struct kgsl_device));

    // if device configuration is present
    if (kgsl_hal_getdevconfig(device_id, &config) == GSL_SUCCESS)
    {

    switch (device_id)
    {
    case KGSL_DEVICE_YAMATO:
        kgsl_yamato_getfunctable(&device->ftbl);
        break;
    case KGSL_DEVICE_G12:
        kgsl_g12_getfunctable(&device->ftbl);
        break;
    default:
        break;
    }

        memcpy(&device->regspace,  &config.regspace,  sizeof(struct kgsl_memregion));
	// for Z430
        memcpy(&device->gmemspace, &config.gmemspace, sizeof(struct kgsl_memregion));

        device->refcnt        = 0;
        device->id            = device_id;

#ifdef CONFIG_KGSL_MMU_ENABLE
        device->mmu.config    = config.mmu_config;
        device->mmu.mpu_base  = config.mpu_base;
        device->mmu.mpu_range = config.mpu_range;
        device->mmu.va_base   = config.va_base;
        device->mmu.va_range  = config.va_range;
#endif

        if (device->ftbl.init)
        {
            status = device->ftbl.init(device);
        }
        else
        {
            status = GSL_FAILURE_NOTINITIALIZED;
        }

        // init memqueue
        device->memqueue.head = NULL;
        device->memqueue.tail = NULL;

        // init cmdstream
        status = kgsl_cmdstream_init(device);
        if (status != GSL_SUCCESS)
        {
            kgsl_device_stop(device->id);
            return (status);
        }

	// Create timestamp wait queue
	init_waitqueue_head(&device->timestamp_waitq);

        // allocate memory store
        status = kgsl_sharedmem_alloc0(device->id, GSL_MEMFLAGS_ALIGNPAGE | GSL_MEMFLAGS_CONPHYS, sizeof(struct kgsl_devmemstore), &device->memstore);

        if (status != GSL_SUCCESS)
        {
            kgsl_device_stop(device->id);
            return (status);
        }
        kgsl_sharedmem_set0(&device->memstore, 0, 0, device->memstore.size);

        //
        //  Read the chip ID after the device has been initialized.
        //
        device->chip_id       = kgsl_hal_getchipid(device->id);
    }

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_close(struct kgsl_device *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    KGSL_DRV_VDBG("device=0x%08x \n", (unsigned int) device);

    if (!(device->flags & KGSL_FLAGS_INITIALIZED)) {
	return status;
    }

    /* make sure the device is stopped before close
       kgsl_device_close is only called for last running caller process
    */
    while (device->refcnt > 0) {
	mutex_unlock(&gsl_driver.lock);
	kgsl_device_stop(device->id);
	mutex_lock(&gsl_driver.lock);
    }

    // close cmdstream
    status = kgsl_cmdstream_close(device);
    if( status != GSL_SUCCESS ) return status;

    if (device->ftbl.close) {
	status = device->ftbl.close(device);
    }

    wake_up_interruptible_all(&(device->timestamp_waitq));

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_destroy(struct kgsl_device *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    KGSL_DRV_VDBG("device=0x%08x\n", (unsigned int) device );

    if (device->flags & KGSL_FLAGS_INITIALIZED)
    {
        if (device->ftbl.destroy)
        {
            status = device->ftbl.destroy(device);
        }
    }

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_attachcallback(struct kgsl_device *device, unsigned int pid)
{
    int  status = GSL_SUCCESS;
    int  pindex;

#ifdef CONFIG_KGSL_MMU_ENABLE

    KGSL_DRV_VDBG("device=0x%08x, pid=0x%08x\n", (unsigned int) device, pid );

    if (device->flags & KGSL_FLAGS_INITIALIZED)
    {
        if (kgsl_driver_getcallerprocessindex(pid, &pindex) == GSL_SUCCESS)
        {
            device->callerprocess[pindex] = pid;

            status = kgsl_mmu_attachcallback(&device->mmu, pid);
        }
    }

#else
    (void)pid;
    (void)device;
#endif

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_detachcallback(struct kgsl_device *device, unsigned int pid)
{
    int  status = GSL_SUCCESS;
    int  pindex;

#ifdef CONFIG_KGSL_MMU_ENABLE

    KGSL_DRV_VDBG("device=0x%08x, pid=0x%08x\n", (unsigned int) device, pid);

    if (device->flags & KGSL_FLAGS_INITIALIZED)
    {
        if (kgsl_driver_getcallerprocessindex(pid, &pindex) == GSL_SUCCESS)
        {
            status |= kgsl_mmu_detachcallback(&device->mmu, pid);

            device->callerprocess[pindex] = 0;
        }
    }

#else
    (void)pid;
    (void)device;
#endif

    return (status);
}

/* qcom: ? */
int kgsl_device_getproperty(unsigned int device_id, enum kgsl_property_type type, void *value, unsigned int sizebytes)
{
	int  status  = GSL_SUCCESS;
	struct kgsl_device  *device = &gsl_driver.device[device_id-1]; // device_id is 1 based

	KGSL_DRV_VDBG("device_id=%d, type=%d, value=0x%08x, sizebytes=%u\n", device_id, (int) type, (unsigned int) value, sizebytes);

	(void) sizebytes;

	switch (type) {
	case KGSL_PROP_SHMEM:
	{
		struct kgsl_shmemprop  *shem = (struct kgsl_shmemprop *) value;

		/* BUG_ON? */
		DEBUG_ASSERT(sizebytes == sizeof(gsl_shmemprop_t));

		shem->numapertures   = gsl_driver.shmem.numapertures;
		shem->aperture_mask  = GSL_APERTURE_MASK;
		shem->aperture_shift = GSL_APERTURE_SHIFT;
		break;
	}
	case KGSL_PROP_SHMEM_APERTURES:
	{
		int i;
		struct kgsl_apertureprop  *aperture = (struct kgsl_apertureprop *) value;

		DEBUG_ASSERT(sizebytes == (sizeof(struct kgsl_apertureprop) * gsl_driver.shmem.numapertures));

		for (i = 0; i < gsl_driver.shmem.numapertures; i++) {
			if (gsl_driver.shmem.apertures[i].memarena) {
				aperture->gpuaddr  = GSL_APERTURE_GETGPUADDR(gsl_driver.shmem, i);
				aperture->hostaddr = GSL_APERTURE_GETHOSTADDR(gsl_driver.shmem, i);
			} else {
				aperture->gpuaddr  = 0x0;
				aperture->hostaddr = 0x0;
			}
			aperture++;
		}
		break;
	}
	case KGSL_PROP_DEVICE_SHADOW:
	{
		struct kgsl_shadowprop  *shadowprop = (struct kgsl_shadowprop *) value;
		DEBUG_ASSERT(sizebytes == sizeof(struct kgsl_shadowprop));

		memset(shadowprop, 0, sizeof(struct kgsl_shadowprop));

#ifdef  GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
		if (device->memstore.hostptr) {
			shadowprop->hostaddr = (unsigned int) device->memstore.hostptr;
			shadowprop->size     = device->memstore.size;
			shadowprop->flags    = KGSL_FLAGS_INITIALIZED;
		}
#endif // GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
		break;
	}
	default:
		if (device->ftbl.getproperty) {
			status = device->ftbl.getproperty(device, type, value, sizebytes);
		}
		break;
	}

	return status;
}

int kgsl_device_setproperty(unsigned int device_id, enum kgsl_property_type type, void *value, unsigned int sizebytes)
{
	int status = GSL_SUCCESS;
	struct kgsl_device  *device;

	KGSL_DRV_VDBG("device_id=%d, type=%d, value=0x%08x, sizebytes=%u\n", device_id, type, (unsigned int) value, sizebytes);

	DEBUG_ASSERT(value);

	mutex_lock(&gsl_driver.lock);
	device = &gsl_driver.device[device_id-1]; // device_id is 1 based
	if (device->flags & KGSL_FLAGS_INITIALIZED) {
		if (device->ftbl.setproperty) {
			status = device->ftbl.setproperty(device, type, value, sizebytes);
		}
	}
	mutex_unlock(&gsl_driver.lock);
	return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_start(unsigned int device_id, unsigned int flags)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;
    gsl_hal_t     *hal = (gsl_hal_t *)gsl_driver.hal;

    KGSL_DRV_VDBG("device_id=%d, flags=%d\n", device_id, flags);

    mutex_lock(&gsl_driver.lock);

    if ((KGSL_DEVICE_G12 == device_id) && !(hal->has_z160)) {
	mutex_unlock(&gsl_driver.lock);
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if ((KGSL_DEVICE_YAMATO == device_id) && !(hal->has_z430)) {
	mutex_unlock(&gsl_driver.lock);
	return GSL_FAILURE_NOTSUPPORTED;
    }

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    kgsl_device_active(device);

    if (!(device->flags & KGSL_FLAGS_INITIALIZED))
    {
        mutex_unlock(&gsl_driver.lock);
        KGSL_DRV_VDBG("ERROR: Trying to start uninitialized device.\n");
        return (GSL_FAILURE);
    }

    device->refcnt++;

    if (device->flags & KGSL_FLAGS_STARTED)
    {
        mutex_unlock(&gsl_driver.lock);
        return (GSL_SUCCESS);
    }

    // start device in safe mode
    if (flags & KGSL_FLAGS_SAFEMODE)
    {
        KGSL_DRV_VDBG("Running the device in safe mode.\n");
        device->flags |= KGSL_FLAGS_SAFEMODE;
    }

    if (device->ftbl.start)
    {
        status = device->ftbl.start(device, flags);
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_stop(unsigned int device_id)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;

    KGSL_DRV_VDBG("device_id=%d\n", device_id );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    if (device->flags & KGSL_FLAGS_STARTED)
    {
        DEBUG_ASSERT(device->refcnt);

        device->refcnt--;

        if (device->refcnt == 0)
        {
            if (device->ftbl.stop)
            {
                status = device->ftbl.stop(device);
            }
        }
        else
        {
            status = GSL_SUCCESS;
        }
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_idle(unsigned int device_id, unsigned int timeout)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;

    KGSL_DRV_VDBG("device_id=%d, timeout=%d\n", device_id, timeout );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    kgsl_device_active(device);
    
    if (device->ftbl.idle)
    {
        status = device->ftbl.idle(device, timeout);
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_isidle(unsigned int device_id)
{
	unsigned int retired = kgsl_cmdstream_readtimestamp0(device_id, KGSL_TIMESTAMP_RETIRED);
	unsigned int consumed = kgsl_cmdstream_readtimestamp0(device_id, KGSL_TIMESTAMP_CONSUMED);
	unsigned int ts_diff = retired - consumed;
	return (ts_diff >= 0) || (ts_diff < -KGSL_TIMESTAMP_EPSILON) ? GSL_SUCCESS : GSL_FAILURE;
}

//----------------------------------------------------------------------------

int
kgsl_device_regread(unsigned int device_id, unsigned int offsetwords, unsigned int *value)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    DEBUG_ASSERT(value);
    DEBUG_ASSERT(offsetwords < device->regspace.sizebytes);

    if (device->ftbl.regread)
    {
        status = device->ftbl.regread(device, offsetwords, value);
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_regwrite(unsigned int device_id, unsigned int offsetwords, unsigned int value)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;

    KGSL_DRV_VDBG("device_id=%d, offsetwords=%d, value=0x%08x\n", device_id, offsetwords, value );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    DEBUG_ASSERT(offsetwords < device->regspace.sizebytes);

    if (device->ftbl.regwrite)
    {
        status = device->ftbl.regwrite(device, offsetwords, value);
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_waitirq(unsigned int device_id, gsl_intrid_t intr_id, unsigned int *count, unsigned int timeout)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    struct kgsl_device  *device;

    KGSL_DRV_VDBG("device_id=%d, intr_id=%d, count=0x%08x, timoute=0x%08x\n", device_id, intr_id, (unsigned int) count, timeout);

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    if (device->ftbl.waitirq)
    {
        status = device->ftbl.waitirq(device, intr_id, count, timeout);
    }

    mutex_unlock(&gsl_driver.lock);

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_runpending(struct kgsl_device *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    KGSL_DRV_VDBG("device=0x%08x\n", (unsigned int) device );

    if (device->flags & KGSL_FLAGS_INITIALIZED)
    {
        if (device->ftbl.runpending)
        {
            status = device->ftbl.runpending(device);
        }

	// free any pending freeontimestamps
	kgsl_cmdstream_memqueue_drain(device);
    }

    KGSL_DRV_VDBG("done %d\n", status );

    return (status);
}

