/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 * Copyright (c) 2008-2011, Freescale Semiconductor, Inc.
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

/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

#include "gsl_hal.h"
#include "gsl_halconfig.h"
#include "gsl_linux_map.h"

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <asm/atomic.h>
#include <linux/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#define DRVNAME "amd-gpu"

#define GSL_HAL_MEM1                        0
#define GSL_HAL_MEM2                        1
//#define GSL_HAL_MEM3                        2

#define GSL_HAL_DEBUG

extern phys_addr_t gpu_2d_regbase;
extern int gpu_2d_regsize;
extern phys_addr_t gpu_3d_regbase;
extern int gpu_3d_regsize;
extern int gmem_size;
extern int gpu_2d_irq, gpu_3d_irq;
extern int enable_mmu;
extern phys_addr_t gpu_reserved_mem;
extern int gpu_reserved_mem_size;


KGSLHAL_API int
kgsl_hal_init(void)
{
    gsl_hal_t *hal;
    unsigned int va;

    if (gsl_driver.hal) {
	return GSL_FAILURE_ALREADYINITIALIZED;
    }

    gsl_driver.hal = (void *)kzalloc(sizeof(gsl_hal_t), GFP_KERNEL);

    if (!gsl_driver.hal) {
	return GSL_FAILURE_OUTOFMEM;
    }

    /* overlay structure on hal memory */
    hal = (gsl_hal_t *) gsl_driver.hal;

    if (gpu_3d_regbase && gpu_3d_regsize && gpu_3d_irq) {
	hal->has_z430 = 1;
    } else {
	hal->has_z430 = 0;
    }

    if (gpu_2d_regbase && gpu_2d_regsize && gpu_2d_irq) {
	hal->has_z160 = 1;
    } else {
	hal->has_z160 = 0;
    }

    gsl_driver.enable_mmu = enable_mmu;

    /* setup register space */
    if (hal->has_z430) {
	hal->z430_regspace.mmio_phys_base = gpu_3d_regbase;
	hal->z430_regspace.sizebytes = gpu_3d_regsize;
	hal->z430_regspace.mmio_virt_base = (unsigned char *)ioremap(hal->z430_regspace.mmio_phys_base, hal->z430_regspace.sizebytes);

	if (hal->z430_regspace.mmio_virt_base == NULL) {
	    return GSL_FAILURE_SYSTEMERROR;
	}

#ifdef GSL_HAL_DEBUG
	pr_info("%s: Z430: phys 0x%p virt 0x%p size 0x%08x irq %d\n", DRVNAME, (void *)hal->z430_regspace.mmio_phys_base,
									(void *)hal->z430_regspace.mmio_virt_base,
									hal->z430_regspace.sizebytes, gpu_3d_irq);
#endif
    }

    if (hal->has_z160) {
	hal->z160_regspace.mmio_phys_base = gpu_2d_regbase;
	hal->z160_regspace.sizebytes = gpu_2d_regsize;
	hal->z160_regspace.mmio_virt_base = (unsigned char *)ioremap(hal->z160_regspace.mmio_phys_base, hal->z160_regspace.sizebytes);

	if (hal->z160_regspace.mmio_virt_base == NULL) {
	    return GSL_FAILURE_SYSTEMERROR;
	}

#ifdef GSL_HAL_DEBUG
	pr_info("%s: Z160: phys 0x%p virt 0x%p size 0x%08x irq %d\n", DRVNAME, (void *)hal->z160_regspace.mmio_phys_base,
									(void *)hal->z160_regspace.mmio_virt_base,
									hal->z160_regspace.sizebytes, gpu_2d_irq);
#endif
    }

    if (gsl_driver.enable_mmu) {
	pr_info("%s: GPU MMU enabled\n", DRVNAME);
    }

    va = (unsigned int) ioremap_wc(gpu_reserved_mem, gpu_reserved_mem_size);
    if (va) {
	/* it would be nice if we didn't do this on module init.. */
        memset((void *)va, 0, gpu_reserved_mem_size);

	hal->memchunk.mmio_virt_base = (void *) va;
	hal->memchunk.mmio_phys_base = (void *) gpu_reserved_mem;
	hal->memchunk.sizebytes = (void *) gpu_reserved_mem_size;

#ifdef GSL_HAL_DEBUG
	pr_info("%s: EMEM: phys 0x%p virt 0x%p size %dMiB\n", DRVNAME, (void *)hal->memchunk.mmio_phys_base,
									(void *)hal->memchunk.mmio_virt_base,
									hal->memchunk.sizebytes / SZ_1M);
#endif
	hal->memspace.mmio_virt_base = (void *) va;
	hal->memspace.gpu_base       = gpu_reserved_mem;
	hal->memspace.sizebytes      = gpu_reserved_mem_size;

    } else {
	pr_err("%s: ioremap of reserved memory failed!\n", DRVNAME);
	return GSL_FAILURE_SYSTEMERROR;
    }

    return GSL_SUCCESS;
}

/* ---------------------------------------------------------------------------- */

KGSLHAL_API int
kgsl_hal_close(void)
{
    gsl_hal_t  *hal;
    printk(KERN_INFO "kgsl_hal_close\n");

    if (gsl_driver.hal) {
	/* overlay structure on hal memory */
	hal = (gsl_hal_t *) gsl_driver.hal;

	/* unmap registers */
	if (hal->has_z430 && hal->z430_regspace.mmio_virt_base) {
	    iounmap(hal->z430_regspace.mmio_virt_base);
	}

	if (hal->has_z160 && hal->z160_regspace.mmio_virt_base) {
	    iounmap(hal->z160_regspace.mmio_virt_base);
	}

	if (hal->memchunk.mmio_virt_base) {
	    iounmap(hal->memchunk.mmio_virt_base);
	}

/*
	if (gsl_driver.enable_mmu) {
	    gsl_linux_map_destroy();
	}
*/
	/* release hal struct */
	gsl_driver.hal = NULL;
	memset(hal, 0, sizeof(gsl_hal_t));
	kfree(hal);
    }

    return GSL_SUCCESS;
}

KGSLHAL_API int
kgsl_hal_getshmemconfig(gsl_shmemconfig_t *config)
{
    int        status = GSL_FAILURE_DEVICEERROR;
    gsl_hal_t  *hal   = (gsl_hal_t *) gsl_driver.hal;

    memset(config, 0, sizeof(gsl_shmemconfig_t));

    if (hal) {
       config->emem_hostbase = hal->memchunk.mmio_virt_base;
       config->emem_gpubase = hal->memchunk.mmio_phys_base;
       config->emem_sizebytes = hal->memchunk.sizebytes;
       status = GSL_SUCCESS;
    }

    return status;
}


KGSLHAL_API int
kgsl_hal_getdevconfig(gsl_deviceid_t device_id, gsl_devconfig_t *config)
{
    int        status = GSL_FAILURE_DEVICEERROR;
    gsl_hal_t  *hal   = (gsl_hal_t *) gsl_driver.hal;

    memset(config, 0, sizeof(gsl_devconfig_t));

    if (hal) {
	switch (device_id) {
	case GSL_DEVICE_YAMATO:
	{
	    if (hal->has_z430) {
		mh_mmu_config_u      mmu_config   = {0};

		config->gmemspace.gpu_base        = 0;
		config->gmemspace.mmio_virt_base  = 0;
		config->gmemspace.mmio_phys_base  = 0;

		if (gmem_size) {
		    config->gmemspace.sizebytes = gmem_size;
		} else {
		    config->gmemspace.sizebytes = 0;
		}

		config->regspace.gpu_base         = 0;
		config->regspace.mmio_virt_base   = (unsigned char *)hal->z430_regspace.mmio_virt_base;
		config->regspace.mmio_phys_base   = (unsigned int) hal->z430_regspace.mmio_phys_base;
		config->regspace.sizebytes        = GSL_HAL_SIZE_REG_YDX;

		mmu_config.f.mmu_enable           = 1;

/*
		if (gsl_driver.enable_mmu) {
		    mmu_config.f.split_mode_enable    = 0;
		    mmu_config.f.rb_w_clnt_behavior   = 1;
		    mmu_config.f.cp_w_clnt_behavior   = 1;
		    mmu_config.f.cp_r0_clnt_behavior  = 1;
		    mmu_config.f.cp_r1_clnt_behavior  = 1;
		    mmu_config.f.cp_r2_clnt_behavior  = 1;
		    mmu_config.f.cp_r3_clnt_behavior  = 1;
		    mmu_config.f.cp_r4_clnt_behavior  = 1;
		    mmu_config.f.vgt_r0_clnt_behavior = 1;
		    mmu_config.f.vgt_r1_clnt_behavior = 1;
		    mmu_config.f.tc_r_clnt_behavior   = 1;
		    mmu_config.f.pa_w_clnt_behavior   = 1;
		}
*/
		config->mmu_config                = mmu_config.val;

/*
		if (gsl_driver.enable_mmu) {
		    config->va_base               = hal->memspace.gpu_base;
		    config->va_range              = hal->memspace.sizebytes;
		} else {
*/
		    config->va_base               = 0x00000000;
		    config->va_range              = 0x00000000;
/*
		}
*/
		/* turn off memory protection unit by setting acceptable physical address range to include all pages */
		config->mpu_base                  = 0x00000000; /* hal->memchunk.mmio_virt_base; */
		config->mpu_range                 = 0xFFFFF000; /* hal->memchunk.sizebytes; */
		status = GSL_SUCCESS;
	    }
	    break;
	}

	case GSL_DEVICE_G12:
	{
		mh_mmu_config_u      mmu_config   = {0};

		config->regspace.gpu_base       = 0;
		config->regspace.mmio_virt_base = (unsigned char *)hal->z160_regspace.mmio_virt_base;
		config->regspace.mmio_phys_base = (unsigned int) hal->z160_regspace.mmio_phys_base;
		config->regspace.sizebytes      = GSL_HAL_SIZE_REG_G12;

		mmu_config.f.mmu_enable           = 1;

/*
		if (gsl_driver.enable_mmu) {
		    config->mmu_config              = 0x00555551;
		    config->va_base                 = hal->memspace.gpu_base;
		    config->va_range                = hal->memspace.sizebytes;
		} else {
*/
		    config->mmu_config              = mmu_config.val;
		    config->va_base                 = 0x00000000;
		    config->va_range                = 0x00000000;
/*
		}
*/
		config->mpu_base                = 0x00000000; /* (unsigned int) hal->memchunk.mmio_virt_base; */
		config->mpu_range               = 0xFFFFF000; /* hal->memchunk.sizebytes; */

		status = GSL_SUCCESS;
		break;
	}

	default:
		break;
	}
    }

    return status;
}

/*----------------------------------------------------------------------------
 * kgsl_hal_getchipid
 *
 * The proper platform method, build from RBBM_PERIPHIDx and RBBM_PATCH_RELEASE
 *----------------------------------------------------------------------------
 */
KGSLHAL_API gsl_chipid_t
kgsl_hal_getchipid(gsl_deviceid_t device_id)
{
    gsl_hal_t *hal = (gsl_hal_t *) gsl_driver.hal;
    gsl_device_t *device = &gsl_driver.device[device_id-1];
    gsl_chipid_t chipid = 0;
    unsigned int coreid, majorid, minorid, patchid, revid;

    if (hal->has_z430 && (device_id == GSL_DEVICE_YAMATO)) {
	device->ftbl.device_regread(device, mmRBBM_PERIPHID1, &coreid);
	coreid &= 0xF;

	device->ftbl.device_regread(device, mmRBBM_PERIPHID2, &majorid);
	majorid = (majorid >> 4) & 0xF;

	device->ftbl.device_regread(device, mmRBBM_PATCH_RELEASE, &revid);

	minorid = ((revid >> 0) & 0xFF); /* this is a 16bit field, but extremely unlikely it would ever get this high */

	patchid = ((revid >> 16) & 0xFF);

	chipid = ((coreid << 24) | (majorid << 16) | (minorid << 8) | (patchid << 0));

#ifdef GSL_HAL_DEBUG
	pr_info("Z430 found: core %u major %u minor %u patch %u (chipid 0x%08x)\n", coreid, majorid, minorid, patchid, chipid);
#endif
    }

    return chipid;
}

/* --------------------------------------------------------------------------- */

KGSLHAL_API int
kgsl_hal_setpowerstate(gsl_deviceid_t device_id, int state, unsigned int value)
{
	gsl_device_t *device = &gsl_driver.device[device_id-1];
	struct clk *gpu_clk = NULL;
	struct clk *garb_clk = NULL;
	struct clk *emi_garb_clk = NULL;

	/* unreferenced formal parameters */
	(void) value;

	switch (device_id) {
	case GSL_DEVICE_G12:
		gpu_clk = clk_get(0, "gpu2d_clk");
		break;
	case GSL_DEVICE_YAMATO:
		gpu_clk = clk_get(0, "gpu3d_clk");
		garb_clk = clk_get(0, "garb_clk");
		emi_garb_clk = clk_get(0, "emi_garb_clk");
		break;
	default:
		return GSL_FAILURE_DEVICEERROR;
	}

	if (!gpu_clk) {
		return GSL_FAILURE_DEVICEERROR;
	}

	switch (state) {
	case GSL_PWRFLAGS_CLK_ON:
		break;
	case GSL_PWRFLAGS_POWER_ON:
		clk_enable(gpu_clk);
		if (garb_clk) {
			clk_enable(garb_clk);
		}
		if (emi_garb_clk) {
			clk_enable(emi_garb_clk);
		}
		kgsl_device_autogate_init(&gsl_driver.device[device_id-1]);
		break;
	case GSL_PWRFLAGS_CLK_OFF:
		break;
	case GSL_PWRFLAGS_POWER_OFF:
		if (device->ftbl.device_idle(device, GSL_TIMEOUT_DEFAULT) != GSL_SUCCESS) {
			return GSL_FAILURE_DEVICEERROR;
		}
		kgsl_device_autogate_exit(&gsl_driver.device[device_id-1]);
		clk_disable(gpu_clk);
		if (garb_clk) {
			clk_disable(garb_clk);
		}
		if (emi_garb_clk) {
			clk_disable(emi_garb_clk);
		}
		break;
	default:
		break;
	}

	return GSL_SUCCESS;
}

KGSLHAL_API int kgsl_clock(gsl_deviceid_t dev, int enable)
{
	struct clk *gpu_clk = NULL;
	struct clk *garb_clk = NULL;
	struct clk *emi_garb_clk = NULL;

	switch (dev) {
	case GSL_DEVICE_G12:
		gpu_clk = clk_get(0, "gpu2d_clk");
		break;
	case GSL_DEVICE_YAMATO:
		gpu_clk = clk_get(0, "gpu3d_clk");
		garb_clk = clk_get(0, "garb_clk");
		emi_garb_clk = clk_get(0, "emi_garb_clk");
		break;
	default:
		printk(KERN_ERR "GPU device %d is invalid!\n", dev);
		return GSL_FAILURE_DEVICEERROR;
	}

	if (IS_ERR(gpu_clk)) {
		printk(KERN_ERR "%s: GPU clock get failed!\n", DRVNAME);
		return GSL_FAILURE_DEVICEERROR;
	}

	if (enable) {
		clk_enable(gpu_clk);
		if (garb_clk) {
		    clk_enable(garb_clk);
		}
		if (emi_garb_clk) {
		    clk_enable(emi_garb_clk);
		}
	} else {
		clk_disable(gpu_clk);
		if (garb_clk) {
		    clk_disable(garb_clk);
		}
		if (emi_garb_clk) {
		    clk_disable(emi_garb_clk);
		}
	}

	return GSL_SUCCESS;
}
