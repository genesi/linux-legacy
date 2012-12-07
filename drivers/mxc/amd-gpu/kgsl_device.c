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

/* move me to a header please */
unsigned int kgsl_yamato_getchipid(struct kgsl_device *device);

int kgsl_device_init(struct kgsl_device *device, unsigned int device_id)
{
	int status = GSL_SUCCESS;
	struct kgsl_devconfig  config;
	gsl_hal_t *hal = (gsl_hal_t *)gsl_driver.hal;

	pr_info("%s: device 0x%08x device_id %d\n", __func__,
		(unsigned int) device, device_id );

	switch (device_id) {
	case KGSL_DEVICE_YAMATO:
		if (!hal->has_z430)
			return GSL_FAILURE_NOTSUPPORTED;
		break;
	case KGSL_DEVICE_G12:
		if (!hal->has_z160)
			return GSL_FAILURE_NOTSUPPORTED;
		break;
	default:
		return GSL_FAILURE_NOTSUPPORTED;
		break;
	}

	if (device->flags & KGSL_FLAGS_INITIALIZED) {
		return status;
	}

	memset(device, 0, sizeof(struct kgsl_device));

	// if device configuration is present
	if (kgsl_hal_getdevconfig(device_id, &config) == GSL_SUCCESS) {

		switch (device_id) {
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

		if (device->ftbl.init) {
			status = device->ftbl.init(device);
		} else {
			status = GSL_FAILURE_NOTINITIALIZED;
		}

		/* init memqueue */
		device->memqueue.head = NULL;
		device->memqueue.tail = NULL;

		/* init cmdstream - surely if device init didn't pass we shouldn't get here */
		status = kgsl_cmdstream_init(device);
		if (status != GSL_SUCCESS) {
			kgsl_device_stop(device);
			return status;
		}

		/* Create timestamp wait queue */
		/* qcom: g12_init or yamato_init */
		init_waitqueue_head(&device->wait_timestamp_wq);

		/* allocate memory store */
		status = kgsl_sharedmem_alloc(GSL_MEMFLAGS_ALIGNPAGE | GSL_MEMFLAGS_CONPHYS, sizeof(struct kgsl_devmemstore), &device->memstore);

		if (status != GSL_SUCCESS) {
			kgsl_device_stop(device);
			return status;
		}

		kgsl_sharedmem_set(&device->memstore, 0, 0, device->memstore.size);

		if (device->id == KGSL_DEVICE_YAMATO)
			device->chip_id = kgsl_yamato_getchipid(device);
		else
			device->chip_id = 0;
	}

	return status;
}

int kgsl_device_close(struct kgsl_device *device)
{
	int status = GSL_FAILURE_NOTINITIALIZED;

	pr_info("%s: device 0x%08x id %d\n", __func__, (unsigned int) device, device->id);

	if (!(device->flags & KGSL_FLAGS_INITIALIZED)) {
		return status;
	}

	/* make sure the device is stopped before close
	 * kgsl_device_close is only called for last running caller process
	 */
	while (device->refcnt > 0) {
		/* are we sure the lock is held here? */
		mutex_unlock(&gsl_driver.lock);
		kgsl_device_stop(device);
		mutex_lock(&gsl_driver.lock);
	}

	/* close cmdstream */
	status = kgsl_cmdstream_close(device);
	if (status != GSL_SUCCESS)
		return status;

	if (device->ftbl.close) {
		status = device->ftbl.close(device);
	}

	wake_up_interruptible_all(&(device->wait_timestamp_wq));

	return status;
}

int kgsl_device_destroy(struct kgsl_device *device)
{
	int status = GSL_FAILURE_NOTINITIALIZED;

	KGSL_DRV_VDBG("device=0x%08x\n", (unsigned int) device );

	if (device->flags & KGSL_FLAGS_INITIALIZED) {
		if (device->ftbl.destroy) {
			status = device->ftbl.destroy(device);
		}
	}

	return status;
}

/* qcom: ? */
int kgsl_device_getproperty(struct kgsl_device *device, enum kgsl_property_type type, void *value, unsigned int sizebytes)
{
	int  status  = GSL_SUCCESS;

	KGSL_DRV_VDBG("device_id=%d, type=%d, value=0x%08x, sizebytes=%u\n", device->id, (int) type, (unsigned int) value, sizebytes);

	(void) sizebytes;

	switch (type) {
	case KGSL_PROP_DEVICE_SHADOW:
	{
		struct kgsl_shadowprop *shadowprop = (struct kgsl_shadowprop *) value;
		DEBUG_ASSERT(sizebytes == sizeof(struct kgsl_shadowprop));

		memset(shadowprop, 0, sizeof(struct kgsl_shadowprop));

#ifdef GSL_DEVICE_SHADOW_MEMSTORE_TO_USER
		if (device->memstore.hostptr) {
			shadowprop->hostaddr = (unsigned int) device->memstore.hostptr;
			shadowprop->size = device->memstore.size;
			shadowprop->flags = KGSL_FLAGS_INITIALIZED;
		}
#endif
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

int kgsl_device_start(struct kgsl_device *device, unsigned int flags)
{
	int           status = GSL_FAILURE_NOTINITIALIZED;
	gsl_hal_t     *hal = (gsl_hal_t *)gsl_driver.hal;

	pr_info("%s: device %d flags 0x%08x\n", __func__, device->id, flags);

	mutex_lock(&gsl_driver.lock);

	switch (device->id) {
	case KGSL_DEVICE_YAMATO:
		if (!hal->has_z430) {
			mutex_unlock(&gsl_driver.lock);
			return GSL_FAILURE_NOTSUPPORTED;
		}
		break;
	case KGSL_DEVICE_G12:
		if (!hal->has_z160) {
			mutex_unlock(&gsl_driver.lock);
			return GSL_FAILURE_NOTSUPPORTED;
		}
		break;
	default:
		mutex_unlock(&gsl_driver.lock);
		return GSL_FAILURE_NOTSUPPORTED;
		break;
	}

	pr_info("%s: calling device active\n", __func__);
	kgsl_device_active(device);

	if (!(device->flags & KGSL_FLAGS_INITIALIZED)) {
		pr_info("%s: trying to start an uninitialized device! failing out\n", __func__);
		mutex_unlock(&gsl_driver.lock);
		return GSL_FAILURE;
	}

	device->refcnt++;

	if (device->flags & KGSL_FLAGS_STARTED) {
		pr_info("%s: device already started, unlocking and exiting\n", __func__);
		mutex_unlock(&gsl_driver.lock);
		return GSL_SUCCESS;
	}

	/* start device in safe mode */
	if (flags & KGSL_FLAGS_SAFEMODE) {
		KGSL_DRV_VDBG("Running the device in safe mode.\n");
		device->flags |= KGSL_FLAGS_SAFEMODE;
	}

	if (device->ftbl.start) {
		pr_info("%s: calling device start from function table\n", __func__);
		status = device->ftbl.start(device, flags);
	}

	mutex_unlock(&gsl_driver.lock);

	return status;
}

int kgsl_device_stop(struct kgsl_device *device)
{
	int status = GSL_FAILURE_NOTINITIALIZED;

	KGSL_DRV_VDBG("device id=%d\n", device->id );

	mutex_lock(&gsl_driver.lock);

	if (device->flags & KGSL_FLAGS_STARTED) {
		device->refcnt--;

		if (device->refcnt == 0) {
			if (device->ftbl.stop)
				status = device->ftbl.stop(device);
		} else {
			status = GSL_SUCCESS;
		}
	}

	mutex_unlock(&gsl_driver.lock);

	return status;
}

int kgsl_device_idle(struct kgsl_device *device, unsigned int timeout)
{
	int           status = GSL_FAILURE_NOTINITIALIZED;

	pr_info("%s: device %d, timeout %d\n", __func__, device->id, timeout );

	mutex_lock(&gsl_driver.lock);

	kgsl_device_active(device);

	if (device->ftbl.idle)
		status = device->ftbl.idle(device, timeout);

	mutex_unlock(&gsl_driver.lock);

	return (status);
}

int kgsl_device_isidle(struct kgsl_device *device)
{
	unsigned int retired = kgsl_cmdstream_readtimestamp(device, KGSL_TIMESTAMP_RETIRED);
	unsigned int consumed = kgsl_cmdstream_readtimestamp(device, KGSL_TIMESTAMP_CONSUMED);
	unsigned int ts_diff = retired - consumed;
	return (ts_diff >= 0) || (ts_diff < -KGSL_TIMESTAMP_EPSILON) ? GSL_SUCCESS : GSL_FAILURE;
}

int kgsl_device_regread(struct kgsl_device *device, unsigned int offsetwords, unsigned int *value)
{
	int status = GSL_FAILURE_NOTINITIALIZED;

	mutex_lock(&gsl_driver.lock);

	/* bug on here for invalid offsets greater than memspace */
	if (device->ftbl.regread) {
		status = device->ftbl.regread(device, offsetwords, value);
	}

	mutex_unlock(&gsl_driver.lock);

	return status;
}

int kgsl_device_runpending(struct kgsl_device *device)
{
	int status = GSL_FAILURE_NOTINITIALIZED;

	KGSL_DRV_VDBG("device=%d\n", (unsigned int) device->id);

	if (device->flags & KGSL_FLAGS_INITIALIZED) {
		if (device->ftbl.runpending)
			status = device->ftbl.runpending(device);

		/* free any pending freeontimestamps */
		kgsl_cmdstream_memqueue_drain(device);
	}

	KGSL_DRV_VDBG("done %d\n", status);

	return status;
}

