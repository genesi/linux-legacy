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

#define GSL_DRVFLAGS_EXTERNAL       0x10000000
#define GSL_DRVFLAGS_INTERNAL       0x20000000

static unsigned int gsl_driver_initialized = 0;
struct kgsl_driver gsl_driver;

int kgsl_driver_init0(unsigned int flags, unsigned int flags_debug)
{
	int status = GSL_SUCCESS;

	if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0)) {
		memset(&gsl_driver, 0, sizeof(struct kgsl_driver));
		mutex_init(&gsl_driver.lock);
	}

	if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0)) {
		mutex_lock(&gsl_driver.lock);

		/* init hal */
		status = kgsl_hal_init();

		if (status == GSL_SUCCESS) {
			gsl_driver_initialized |= flags;
			gsl_driver_initialized |= KGSL_FLAGS_INITIALIZED0;
		}

		mutex_unlock(&gsl_driver.lock);
	}

	return status;
}

int kgsl_driver_close0(unsigned int flags)
{
	int status = GSL_SUCCESS;

	if ((gsl_driver_initialized & KGSL_FLAGS_INITIALIZED0) && (gsl_driver_initialized & flags)) {
		mutex_lock(&gsl_driver.lock);
		/* close hal */
		status = kgsl_hal_close();
		mutex_unlock(&gsl_driver.lock);

		gsl_driver_initialized &= ~flags;
		gsl_driver_initialized &= ~KGSL_FLAGS_INITIALIZED0;
	}

	return status;
}

int kgsl_driver_init(void)
{
	// only an external (platform specific device driver) component should call this
	return(kgsl_driver_init0(GSL_DRVFLAGS_EXTERNAL, 0));
}

int kgsl_driver_close(void)
{
	// only an external (platform specific device driver) component should call this
	return(kgsl_driver_close0(GSL_DRVFLAGS_EXTERNAL));
}

int kgsl_driver_entry(unsigned int flags)
{
	int status = GSL_FAILURE;
	int i;

	if (kgsl_driver_init0(GSL_DRVFLAGS_INTERNAL, flags) != GSL_SUCCESS) {
		return status;
	}

	KGSL_DRV_VDBG("flags=%x )\n", flags);

	mutex_lock(&gsl_driver.lock);

	gsl_driver.refcnt++;

	if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED)) {
		/* init memory apertures */
		status = kgsl_sharedmem_init(&gsl_driver.shmem);
		if (status == GSL_SUCCESS) {
			pr_info("%s: sharedmem_init passed\n", __func__);
			/* init devices */
			status = GSL_FAILURE;

			for (i = 0; i < KGSL_DEVICE_MAX; i++) {
				pr_info("%s: calling device init dev %d\n", __func__,
					i+1);

				status = kgsl_device_init(&gsl_driver.device[i], (unsigned int)(i + 1));
				if (status == GSL_SUCCESS) {
					pr_info("%s: device init success\n", __func__);
				}
			}
		}

		if (status == GSL_SUCCESS)
        	        gsl_driver_initialized |= KGSL_FLAGS_INITIALIZED;
	}

        /* if something went wrong */
	if (status != GSL_SUCCESS) {
		pr_info("%s: decreasing refcount because something is bad, status %d\n", __func__, status);
		gsl_driver.refcnt--;
	}

	mutex_unlock(&gsl_driver.lock);

	return status;
}

int kgsl_driver_exit(void)
{
	int status = GSL_SUCCESS;
	int i;

	pr_info("%s\n", __func__);

	mutex_lock(&gsl_driver.lock);

	if (gsl_driver_initialized & KGSL_FLAGS_INITIALIZED) {
		if ((gsl_driver.refcnt - 1) == 0) {
			// close devices
			for (i = 0; i < KGSL_DEVICE_MAX; i++) {
				kgsl_device_close(&gsl_driver.device[i]);
			}

			// shutdown memory apertures
			kgsl_sharedmem_close(&gsl_driver.shmem);

			gsl_driver_initialized &= ~KGSL_FLAGS_INITIALIZED;
		}

		gsl_driver.refcnt--;
	}

	mutex_unlock(&gsl_driver.lock);

	if (!(gsl_driver_initialized & KGSL_FLAGS_INITIALIZED)) {
		kgsl_driver_close0(GSL_DRVFLAGS_INTERNAL);
	}

	return status;
}

int kgsl_driver_destroy(void)
{
    return (kgsl_driver_exit());
}
