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

#include "gsl.h"
#include "gsl_hal.h"

//////////////////////////////////////////////////////////////////////////////
//  inline functions
//////////////////////////////////////////////////////////////////////////////
static __inline void
kgsl_device_getfunctable(gsl_deviceid_t device_id, gsl_functable_t *ftbl)
{
    switch (device_id)
    {
#ifdef GSL_BLD_YAMATO
    case GSL_DEVICE_YAMATO:
        kgsl_yamato_getfunctable(ftbl);
        break;
#endif // GSL_BLD_YAMATO
#ifdef GSL_BLD_G12
    case GSL_DEVICE_G12:
        kgsl_g12_getfunctable(ftbl);
        break;
#endif // GSL_BLD_G12
    default:
        break;
    }
}


//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

int
kgsl_device_init(gsl_device_t *device, gsl_deviceid_t device_id)
{
    int              status = GSL_SUCCESS;
    gsl_devconfig_t  config;
    gsl_hal_t        *hal = (gsl_hal_t *)gsl_driver.hal;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_init(gsl_device_t *device=0x%08x, gsl_deviceid_t device_id=%D )\n", device, device_id );

    if ((GSL_DEVICE_YAMATO == device_id) && !(hal->has_z430)) {
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if ((GSL_DEVICE_G12 == device_id) && !(hal->has_z160)) {
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_init. Return value %B\n", GSL_SUCCESS );
        return (GSL_SUCCESS);
    }

    memset(device, 0, sizeof(gsl_device_t));

    // if device configuration is present
    if (kgsl_hal_getdevconfig(device_id, &config) == GSL_SUCCESS)
    {
        kgsl_device_getfunctable(device_id, &device->ftbl);

        memcpy(&device->regspace,  &config.regspace,  sizeof(gsl_memregion_t));
#ifdef GSL_BLD_YAMATO
        memcpy(&device->gmemspace, &config.gmemspace, sizeof(gsl_memregion_t));
#endif // GSL_BLD_YAMATO

        device->refcnt        = 0;
        device->id            = device_id;

#ifndef GSL_NO_MMU
        device->mmu.config    = config.mmu_config;
        device->mmu.mpu_base  = config.mpu_base;
        device->mmu.mpu_range = config.mpu_range;
        device->mmu.va_base   = config.va_base;
        device->mmu.va_range  = config.va_range;
#endif

        if (device->ftbl.device_init)
        {
            status = device->ftbl.device_init(device);
        }
        else
        {
            status = GSL_FAILURE_NOTINITIALIZED;
        }

        // allocate memory store
        status = kgsl_sharedmem_alloc0(device->id, GSL_MEMFLAGS_ALIGNPAGE | GSL_MEMFLAGS_CONPHYS, sizeof(gsl_devmemstore_t), &device->memstore);

        KGSL_DEBUG(GSL_DBGFLAGS_DUMPX,
        {
            // dumpx needs this to be in EMEM0 aperture
            kgsl_sharedmem_free0(&device->memstore, current->tgid);
            status = kgsl_sharedmem_alloc0(device->id, GSL_MEMFLAGS_ALIGNPAGE, sizeof(gsl_devmemstore_t), &device->memstore);
        });

        if (status != GSL_SUCCESS)
        {
            kgsl_device_stop(device->id);
            return (status);
        }
        kgsl_sharedmem_set0(&device->memstore, 0, 0, device->memstore.size);

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

        //
        //  Read the chip ID after the device has been initialized.
        //
        device->chip_id       = kgsl_hal_getchipid(device->id);
    }


    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_init. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_close(gsl_device_t *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_close(gsl_device_t *device=0x%08x )\n", device );

    if (!(device->flags & GSL_FLAGS_INITIALIZED)) {
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

    if (device->ftbl.device_close) {
	status = device->ftbl.device_close(device);
    }

    // DumpX allocates memstore from MMU aperture
    if ((device->refcnt == 0) && device->memstore.hostptr
	&& !(gsl_driver.flags_debug & GSL_DBGFLAGS_DUMPX))
    {
	kgsl_sharedmem_free0(&device->memstore, current->tgid);
    }

    wake_up_interruptible_all(&(device->timestamp_waitq));

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_close. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_destroy(gsl_device_t *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_destroy(gsl_device_t *device=0x%08x )\n", device );

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        if (device->ftbl.device_destroy)
        {
            status = device->ftbl.device_destroy(device);
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_destroy. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_attachcallback(gsl_device_t *device, unsigned int pid)
{
    int  status = GSL_SUCCESS;
    int  pindex;

#ifndef GSL_NO_MMU

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "--> int kgsl_device_attachcallback(gsl_device_t *device=0x%08x, unsigned int pid=0x%08x)\n", device, pid );

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        if (kgsl_driver_getcallerprocessindex(pid, &pindex) == GSL_SUCCESS)
        {
            device->callerprocess[pindex] = pid;

            status = kgsl_mmu_attachcallback(&device->mmu, pid);
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_attachcallback. Return value: %B\n", status );

#else
    (void)pid;
    (void)device;
#endif

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_detachcallback(gsl_device_t *device, unsigned int pid)
{
    int  status = GSL_SUCCESS;
    int  pindex;

#ifndef GSL_NO_MMU

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "--> int kgsl_device_detachcallback(gsl_device_t *device=0x%08x, unsigned int pid=0x%08x)\n", device, pid );

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        if (kgsl_driver_getcallerprocessindex(pid, &pindex) == GSL_SUCCESS)
        {
            status |= kgsl_mmu_detachcallback(&device->mmu, pid);

            device->callerprocess[pindex] = 0;
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_detachcallback. Return value: %B\n", status );

#else
    (void)pid;
    (void)device;
#endif

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_getproperty(gsl_deviceid_t device_id, gsl_property_type_t type, void *value, unsigned int sizebytes)
{
    int           status  = GSL_SUCCESS;
    gsl_device_t  *device = &gsl_driver.device[device_id-1];        // device_id is 1 based

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_getproperty(gsl_deviceid_t device_id=%D, gsl_property_type_t type=%T, void *value=0x%08x, uint sizebytes=%u)\n", device_id, type, value, sizebytes );

    DEBUG_ASSERT(value);

#ifndef _DEBUG
    (void) sizebytes;       // unreferenced formal parameter
#endif

    switch (type)
    {
    case GSL_PROP_SHMEM:
        {
        gsl_shmemprop_t  *shem = (gsl_shmemprop_t *) value;

        DEBUG_ASSERT(sizebytes == sizeof(gsl_shmemprop_t));

        shem->numapertures   = 1;
        shem->aperture_mask  = GSL_APERTURE_MASK;
        shem->aperture_shift = GSL_APERTURE_SHIFT;

        break;
        }

    case GSL_PROP_SHMEM_APERTURES:
        {
        int i;
        gsl_apertureprop_t  *aperture = (gsl_apertureprop_t *) value;

        DEBUG_ASSERT(sizebytes == (sizeof(gsl_apertureprop_t) * gsl_driver.shmem.numapertures));

        if (gsl_driver.shmem.memarena)
        {
            aperture->gpuaddr  = GSL_SHMEM_MEMARENA_GETGPUADDR(gsl_driver.shmem);
            aperture->hostaddr = GSL_SHMEM_MEMARENA_GETHOSTADDR(gsl_driver.shmem);
        }
         else
        {
            aperture->gpuaddr  = 0x0;
            aperture->hostaddr = 0x0;
        }

        break;
        }

    case GSL_PROP_DEVICE_SHADOW:
        {
        gsl_shadowprop_t  *shadowprop = (gsl_shadowprop_t *) value;

        DEBUG_ASSERT(sizebytes == sizeof(gsl_shadowprop_t));

        memset(shadowprop, 0, sizeof(gsl_shadowprop_t));

#ifdef  GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
        if (device->memstore.hostptr)
        {
            shadowprop->hostaddr = (unsigned int) device->memstore.hostptr;
            shadowprop->size     = device->memstore.size;
            shadowprop->flags    = GSL_FLAGS_INITIALIZED;
        }
#endif // GSL_DEVICE_SHADOW_MEMSTORE_TO_USER

        break;
        }

    default:
        {
        if (device->ftbl.device_getproperty)
        {
            status = device->ftbl.device_getproperty(device, type, value, sizebytes);
        }

        break;
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_getproperty. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_setproperty(gsl_deviceid_t device_id, gsl_property_type_t type, void *value, unsigned int sizebytes)
{
    int           status = GSL_SUCCESS;
    gsl_device_t  *device;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_setproperty(gsl_deviceid_t device_id=%D, gsl_property_type_t type=%T, void *value=0x%08x, uint sizebytes=%u)\n", device_id, type, value, sizebytes );

    DEBUG_ASSERT(value);

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        if (device->ftbl.device_setproperty)
        {
            status = device->ftbl.device_setproperty(device, type, value, sizebytes);
        }
    }

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_setproperty. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_start(gsl_deviceid_t device_id, gsl_flags_t flags)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    gsl_device_t  *device;
    gsl_hal_t     *hal = (gsl_hal_t *)gsl_driver.hal;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_start(gsl_deviceid_t device_id=%D, gsl_flags_t flags=%d)\n", device_id, flags );

    mutex_lock(&gsl_driver.lock);

    if ((GSL_DEVICE_G12 == device_id) && !(hal->has_z160)) {
	mutex_unlock(&gsl_driver.lock);
	return GSL_FAILURE_NOTSUPPORTED;
    }

    if ((GSL_DEVICE_YAMATO == device_id) && !(hal->has_z430)) {
	mutex_unlock(&gsl_driver.lock);
	return GSL_FAILURE_NOTSUPPORTED;
    }

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based
    
    kgsl_device_active(device);
    
    if (!(device->flags & GSL_FLAGS_INITIALIZED))
    {
        mutex_unlock(&gsl_driver.lock);

        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_ERROR, "ERROR: Trying to start uninitialized device.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_start. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    device->refcnt++;

    if (device->flags & GSL_FLAGS_STARTED)
    {
        mutex_unlock(&gsl_driver.lock);
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_start. Return value %B\n", GSL_SUCCESS );
        return (GSL_SUCCESS);
    }

    // start device in safe mode
    if (flags & GSL_FLAGS_SAFEMODE)
    {
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_INFO, "Running the device in safe mode.\n" );
        device->flags |= GSL_FLAGS_SAFEMODE;
    }

    if (device->ftbl.device_start)
    {
        status = device->ftbl.device_start(device, flags);
    }

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_start. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_stop(gsl_deviceid_t device_id)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    gsl_device_t  *device;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_stop(gsl_deviceid_t device_id=%D)\n", device_id );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    if (device->flags & GSL_FLAGS_STARTED)
    {
        DEBUG_ASSERT(device->refcnt);

        device->refcnt--;

        if (device->refcnt == 0)
        {
            if (device->ftbl.device_stop)
            {
                status = device->ftbl.device_stop(device);
            }
        }
        else
        {
            status = GSL_SUCCESS;
        }
    }

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_stop. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_idle(gsl_deviceid_t device_id, unsigned int timeout)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    gsl_device_t  *device;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_idle(gsl_deviceid_t device_id=%D, unsigned int timeout=%d)\n", device_id, timeout );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    kgsl_device_active(device);
    
    if (device->ftbl.device_idle)
    {
        status = device->ftbl.device_idle(device, timeout);
    }

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_idle. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_isidle(gsl_deviceid_t device_id)
{
	gsl_timestamp_t retired = kgsl_cmdstream_readtimestamp0(device_id, GSL_TIMESTAMP_RETIRED);
	gsl_timestamp_t consumed = kgsl_cmdstream_readtimestamp0(device_id, GSL_TIMESTAMP_CONSUMED);
	gsl_timestamp_t ts_diff = retired - consumed;
	return (ts_diff >= 0) || (ts_diff < -GSL_TIMESTAMP_EPSILON) ? GSL_SUCCESS : GSL_FAILURE;
}

//----------------------------------------------------------------------------

int
kgsl_device_regread(gsl_deviceid_t device_id, unsigned int offsetwords, unsigned int *value)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    gsl_device_t  *device;


#ifdef GSL_LOG
    if( offsetwords != mmRBBM_STATUS && offsetwords != mmCP_RB_RPTR ) // Would otherwise flood the log
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                        "--> int kgsl_device_regread(gsl_deviceid_t device_id=%D, unsigned int offsetwords=%R, unsigned int *value=0x%08x)\n", device_id, offsetwords, value );
#endif

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    DEBUG_ASSERT(value);
    DEBUG_ASSERT(offsetwords < device->regspace.sizebytes);

    if (device->ftbl.device_regread)
    {
        status = device->ftbl.device_regread(device, offsetwords, value);
    }

    mutex_unlock(&gsl_driver.lock);

#ifdef GSL_LOG
    if( offsetwords != mmRBBM_STATUS && offsetwords != mmCP_RB_RPTR )
        kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_regread. Return value %B\n", status );
#endif

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_device_regwrite(gsl_deviceid_t device_id, unsigned int offsetwords, unsigned int value)
{
    int           status = GSL_FAILURE_NOTINITIALIZED;
    gsl_device_t  *device;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_regwrite(gsl_deviceid_t device_id=%D, unsigned int offsetwords=%R, uint value=0x%08x)\n", device_id, offsetwords, value );

    mutex_lock(&gsl_driver.lock);

    device = &gsl_driver.device[device_id-1];       // device_id is 1 based

    DEBUG_ASSERT(offsetwords < device->regspace.sizebytes);

    if (device->ftbl.device_regwrite)
    {
        status = device->ftbl.device_regwrite(device, offsetwords, value);
    }

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_regwrite. Return value %B\n", status );

    return (status);
}

int
kgsl_device_runpending(gsl_device_t *device)
{
    int  status = GSL_FAILURE_NOTINITIALIZED;

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_device_runpending(gsl_device_t *device=0x%08x )\n", device );

    if (device->flags & GSL_FLAGS_INITIALIZED)
    {
        if (device->ftbl.device_runpending)
        {
            status = device->ftbl.device_runpending(device);
        }
    }

    // free any pending freeontimestamps
    kgsl_cmdstream_memqueue_drain(device);

    kgsl_log_write( KGSL_LOG_GROUP_DEVICE | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_device_runpending. Return value %B\n", status );

    return (status);
}

