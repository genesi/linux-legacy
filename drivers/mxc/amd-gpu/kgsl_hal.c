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

/* compile string for debug */
#include <linux/compile.h>

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <asm/atomic.h>
#include <linux/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_hal.h"
#include "kgsl_halconfig.h"
#include "kgsl_linux_map.h"
#include "kgsl_driver.h"

#include "yamato_reg.h"
#include "g12_reg.h"

#define DRVNAME "amd-gpu"

#define GSL_HAL_DEBUG

extern phys_addr_t gpu_2d_regbase;
extern int gpu_2d_regsize;
extern phys_addr_t gpu_3d_regbase;
extern int gpu_3d_regsize;
extern int gmem_size;
extern phys_addr_t gpu_reserved_mem;
extern int gpu_reserved_mem_size;
extern int gpu_2d_irq, gpu_3d_irq;
extern int enable_mmu;


KGSLHAL_API int
kgsl_hal_init(void)
{
    gsl_hal_t *hal;
    unsigned long res_size;
    unsigned int va, pa;

	pr_info("amd-gpu: built for kernel %s\n", UTS_VERSION);

    if (!gpu_reserved_mem || !gpu_reserved_mem_size) {
	printk(KERN_ERR "%s: no GPU reserved memory! Cannot continue!\n", DRVNAME);
	return GSL_FAILURE_SYSTEMERROR;
    }

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
	pr_info("%s: hal->z430_regspace.mmio_phys_base = 0x%p\n", DRVNAME, (void *)hal->z430_regspace.mmio_phys_base);
	pr_info("%s: hal->z430_regspace.mmio_virt_base = 0x%p\n", DRVNAME, (void *)hal->z430_regspace.mmio_virt_base);
	pr_info("%s: hal->z430_regspace.sizebytes      = 0x%08x\n", DRVNAME, hal->z430_regspace.sizebytes);
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
	pr_info("%s: hal->z160_regspace.mmio_phys_base = 0x%p\n", DRVNAME, (void *)hal->z160_regspace.mmio_phys_base);
	pr_info("%s: hal->z160_regspace.mmio_virt_base = 0x%p\n", DRVNAME, (void *)hal->z160_regspace.mmio_virt_base);
	pr_info("%s: hal->z160_regspace.sizebytes      = 0x%08x\n", DRVNAME, hal->z160_regspace.sizebytes);
#endif
    }

	pa = gpu_reserved_mem;
	va = (unsigned int)ioremap(gpu_reserved_mem, gpu_reserved_mem_size);
	if (!va)
		pr_err("%s: ioremap failed!\n", DRVNAME);

    if (gsl_driver.enable_mmu) {
	pr_info("%s: GPU MMU enabled\n", DRVNAME);
	res_size = gpu_reserved_mem_size;
   } else {
	pr_info("%s: GPU MMU disabled\n", DRVNAME);
	res_size = gpu_reserved_mem_size - GSL_HAL_SHMEM_SIZE_EMEM_NOMMU;
    }

    if (va) {
	/* it would be awesome if we didn't have to do this on module init.. */
	memset((void *)va, 0, res_size);

	/* set up a "memchunk" so we know what we can iounmap on exit */
	hal->memchunk.mmio_virt_base = (void *)va;
	hal->memchunk.mmio_phys_base = pa;
	hal->memchunk.sizebytes      = gpu_reserved_mem_size;

#ifdef GSL_HAL_DEBUG
	pr_info("%s: Reserved memory: pa = 0x%p va = 0x%p size = 0x%08x\n", DRVNAME,
				(void *)hal->memchunk.mmio_phys_base,
				(void *)hal->memchunk.mmio_virt_base,
					hal->memchunk.sizebytes);
#endif

	hal->memspace[GSL_HAL_MEM2].mmio_virt_base = (void *) va;
	hal->memspace[GSL_HAL_MEM2].gpu_base       = pa;
	hal->memspace[GSL_HAL_MEM2].sizebytes      = res_size;
	va += res_size;
	pa += res_size;

#ifdef GSL_HAL_DEBUG
	pr_info("%s: GSL_HAL_MEM2 aperture (PHYS) pa = 0x%p va = 0x%p size = 0x%08x\n", DRVNAME,
				(void *)hal->memspace[GSL_HAL_MEM2].gpu_base,
				(void *)hal->memspace[GSL_HAL_MEM2].mmio_virt_base,
				hal->memspace[GSL_HAL_MEM2].sizebytes);
#endif

	if (gsl_driver.enable_mmu) {
	    kgsl_mem_entry_init();
	    hal->memspace[GSL_HAL_MEM1].mmio_virt_base = (void *)GSL_LINUX_MAP_RANGE_START;
	    hal->memspace[GSL_HAL_MEM1].gpu_base       = GSL_LINUX_MAP_RANGE_START;
	    hal->memspace[GSL_HAL_MEM1].sizebytes      = GSL_HAL_SHMEM_SIZE_EMEM_MMU;
	} else {
	    hal->memspace[GSL_HAL_MEM1].mmio_virt_base = (void *) va;
	    hal->memspace[GSL_HAL_MEM1].gpu_base       = pa;
	    hal->memspace[GSL_HAL_MEM1].sizebytes	= GSL_HAL_SHMEM_SIZE_EMEM_NOMMU;
	}

#ifdef GSL_HAL_DEBUG
	pr_info("%s: GSL_HAL_MEM1 aperture (%s) pa = 0x%p va = 0x%p size = 0x%08x\n", DRVNAME,
				gsl_driver.enable_mmu ? "MMU" : "EMEM",
				(void *)hal->memspace[GSL_HAL_MEM1].gpu_base,
				(void *)hal->memspace[GSL_HAL_MEM1].mmio_virt_base,
				hal->memspace[GSL_HAL_MEM1].sizebytes);
#endif
    } else {
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

	/* free physical block */
	if (hal->memchunk.mmio_virt_base && gpu_reserved_mem) {
	    iounmap(hal->memchunk.mmio_virt_base);
	} else {
	    dma_free_coherent(0, hal->memchunk.sizebytes, hal->memchunk.mmio_virt_base, hal->memchunk.mmio_phys_base);
	}

	if (gsl_driver.enable_mmu) {
	    kgsl_mem_entry_destroy();
	}

	/* release hal struct */
	memset(hal, 0, sizeof(gsl_hal_t));
	kfree(gsl_driver.hal);
	gsl_driver.hal = NULL;
    }

    return GSL_SUCCESS;
}

#ifdef CONFIG_KGSL_MMU_PAGE_FAULT
/* page fault when a mapping fails. Not ideal! */
#define MMU_CONFIG 2
#else
/* when a mapping fails, assume PA=VA and continue */
#define MMU_CONFIG 1
#endif

KGSLHAL_API int
kgsl_hal_getdevconfig(unsigned int device_id, struct kgsl_devconfig *config)
{
    int        status = GSL_FAILURE_DEVICEERROR;
    gsl_hal_t  *hal   = (gsl_hal_t *) gsl_driver.hal;

    memset(config, 0, sizeof(struct kgsl_devconfig));

    if (hal) {
	switch (device_id) {
	case KGSL_DEVICE_YAMATO:
	{
	    if (hal->has_z430) {
		unsigned int mmu_config;

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

		// we enable the MMU (actually the MH part) regardless of configuration
		mmu_config          = 1;

		if (gsl_driver.enable_mmu) {
        	    mmu_config = mmu_config
                    | (MMU_CONFIG << MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT);
		}

		config->mmu_config                = mmu_config;

		if (gsl_driver.enable_mmu) {
		    config->va_base               = hal->memspace[GSL_HAL_MEM1].gpu_base;
		    config->va_range              = hal->memspace[GSL_HAL_MEM1].sizebytes;
		} else {
		    config->va_base               = 0x00000000;
		    config->va_range              = 0x00000000;
		}

		/* turn off memory protection unit by setting acceptable physical address range to include all pages */
		config->mpu_base                  = 0x00000000; /* hal->memchunk.mmio_virt_base; */
		config->mpu_range                 = 0xFFFFF000; /* hal->memchunk.sizebytes; */
		status = GSL_SUCCESS;
	    }
	    break;
	}

	case KGSL_DEVICE_G12:
	{
		unsigned int mmu_config;

		config->regspace.gpu_base       = 0;
		config->regspace.mmio_virt_base = (unsigned char *)hal->z160_regspace.mmio_virt_base;
		config->regspace.mmio_phys_base = (unsigned int) hal->z160_regspace.mmio_phys_base;
		config->regspace.sizebytes      = GSL_HAL_SIZE_REG_G12;

		// we enable the MMU (actually the MH part) regardless of configuration
        	mmu_config = 1;

/*		and then qualcomm does this if the MMU is enabled??
                    | (MMU_CONFIG << MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT)
                    | (MMU_CONFIG << MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT);
*/

		if (gsl_driver.enable_mmu) {
		    config->mmu_config              = 0x00555551;
		    config->va_base                 = hal->memspace[GSL_HAL_MEM1].gpu_base;
		    config->va_range                = hal->memspace[GSL_HAL_MEM1].sizebytes;
		} else {
		    config->mmu_config              = mmu_config;
		    config->va_base                 = 0x00000000;
		    config->va_range                = 0x00000000;
		}

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
