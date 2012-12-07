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

#include <linux/slab.h>
#include <linux/sched.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_sharedmem.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_log.h"
#include "kgsl_config.h"
#include "kgsl_driver.h"
#include "kgsl_debug.h"

struct kgsl_pte_debug {
	unsigned int write     :1;
	unsigned int read      :1;
	unsigned int dirty     :1;
	unsigned int reserved  :9;
	unsigned int phyaddr   :20;
};

#define GSL_PT_ENTRY_SIZEBYTES              4
#define GSL_PT_EXTRA_ENTRIES                16

#define GSL_PT_PAGE_WRITE                   0x00000001
#define GSL_PT_PAGE_READ                    0x00000002

#define GSL_PT_PAGE_AP_MASK                 0x00000003
#define GSL_PT_PAGE_ADDR_MASK               ~(PAGE_SIZE-1)



#define GSL_MMUFLAGS_TLBFLUSH               0x80000000

#define GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS   (sizeof(unsigned char) * 8)


const unsigned int GSL_PT_PAGE_AP[4] = {(GSL_PT_PAGE_READ | GSL_PT_PAGE_WRITE), GSL_PT_PAGE_READ, GSL_PT_PAGE_WRITE, 0};


#define GSL_PT_ENTRY_GET(va)                ((va - pagetable->va_base) >> PAGE_SHIFT)
#define GSL_PT_VIRT_GET(pte)                (pagetable->va_base + (pte * PAGE_SIZE))

#define GSL_PT_MAP_APDEFAULT                GSL_PT_PAGE_AP[0]

#define GSL_PT_MAP_GET(pte)                 *((unsigned int *)(((unsigned int)pagetable->base.hostptr) + ((pte) * GSL_PT_ENTRY_SIZEBYTES)))
#define GSL_PT_MAP_GETADDR(pte)             (GSL_PT_MAP_GET(pte) & GSL_PT_PAGE_ADDR_MASK)

#define GSL_PT_MAP_DEBUG(pte)               ((struct kgsl_pte_debug *) &GSL_PT_MAP_GET(pte))

#define GSL_PT_MAP_SETBITS(pte, bits)       (GSL_PT_MAP_GET(pte) |= (((unsigned int) bits) & GSL_PT_PAGE_AP_MASK))
#define GSL_PT_MAP_SETADDR(pte, pageaddr)   (GSL_PT_MAP_GET(pte)  = (GSL_PT_MAP_GET(pte) & ~GSL_PT_PAGE_ADDR_MASK) | (((unsigned int) pageaddr) & GSL_PT_PAGE_ADDR_MASK))

/* reserve RV and WV bits to work around READ_PROTECTION_ERROR in some cases */
#define GSL_PT_MAP_RESET(pte)               (GSL_PT_MAP_GET(pte) &= ~GSL_PT_PAGE_ADDR_MASK)
#define GSL_PT_MAP_RESETBITS(pte, bits)     (GSL_PT_MAP_GET(pte) &= ~(((unsigned int) bits) & GSL_PT_PAGE_AP_MASK))

#define GSL_MMU_VIRT_TO_PAGE(va)            *((unsigned int *)(pagetable->base.gpuaddr + (GSL_PT_ENTRY_GET(va) * GSL_PT_ENTRY_SIZEBYTES)))
#define GSL_MMU_VIRT_TO_PHYS(va)            ((GSL_MMU_VIRT_TO_PAGE(va) & GSL_PT_PAGE_ADDR_MASK) + (va & (PAGE_SIZE-1)))

#define GSL_TLBFLUSH_FILTER_GET(superpte)       *((unsigned char *)(((unsigned int)mmu->tlbflushfilter.base) + (superpte / GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS)))
#define GSL_TLBFLUSH_FILTER_SETDIRTY(superpte)  (GSL_TLBFLUSH_FILTER_GET((superpte)) |= 1 << (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS))
#define GSL_TLBFLUSH_FILTER_ISDIRTY(superpte)   (GSL_TLBFLUSH_FILTER_GET((superpte)) & (1 << (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS)))
#define GSL_TLBFLUSH_FILTER_RESET()             memset(mmu->tlbflushfilter.base, 0, mmu->tlbflushfilter.size)


#define GSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR)

static const struct kgsl_mmu_reg mmu_reg[KGSL_DEVICE_MAX] = {
	{
		.config = REG_MH_MMU_CONFIG,
		.mpu_base = REG_MH_MMU_MPU_BASE,
		.mpu_end = REG_MH_MMU_MPU_END,
		.va_range = REG_MH_MMU_VA_RANGE,
		.pt_page = REG_MH_MMU_PT_BASE,
		.page_fault = REG_MH_MMU_PAGE_FAULT,
		.trans_error = REG_MH_MMU_TRAN_ERROR,
		.invalidate = REG_MH_MMU_INVALIDATE,
		.interrupt_mask = REG_MH_INTERRUPT_MASK,
		.interrupt_status = REG_MH_INTERRUPT_STATUS,
		.interrupt_clear = REG_MH_INTERRUPT_CLEAR
	},
	{
		.config = ADDR_MH_MMU_CONFIG,
		.mpu_base = ADDR_MH_MMU_MPU_BASE,
		.mpu_end = ADDR_MH_MMU_MPU_END,
		.va_range = ADDR_MH_MMU_VA_RANGE,
		.pt_page = ADDR_MH_MMU_PT_BASE,
		.page_fault = ADDR_MH_MMU_PAGE_FAULT,
		.trans_error = ADDR_MH_MMU_TRAN_ERROR,
		.invalidate = ADDR_MH_MMU_INVALIDATE,
		.interrupt_mask = ADDR_MH_INTERRUPT_MASK,
		.interrupt_status = ADDR_MH_INTERRUPT_STATUS,
		.interrupt_clear = ADDR_MH_INTERRUPT_CLEAR
	}
};

static __inline struct kgsl_pagetable* kgsl_mmu_getpagetableobject(struct kgsl_mmu *mmu)
{
	return (struct kgsl_pagetable *) mmu->pagetable;
}

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
//	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int devindex = device->id-1;
	unsigned int status = 0, reg;

	device->ftbl.regread(device, mmu_reg[devindex].interrupt_status, &status);

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR) {
		pr_err("axi read error interrupt\n");
	} else if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR) {
		pr_err("axi write error interrupt\n");
	} else if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT) {
		device->ftbl.regread(device, mmu_reg[devindex].page_fault, &reg);
		pr_err("mmu page fault interrupt: %08x\n", reg);
	} else {
		pr_err("bad bits in REG_MH_INTERRUPT_STATUS %08x\n", status);
	}

	device->ftbl.regwrite(device, mmu_reg[devindex].interrupt_clear, status);

	/* TODO: figure out how to handle error interrupts.
	 * specifically page faults should probably nuke the client that
	 * caused them, but we don't have enough info to figure that out yet.
	 *
	 * fsl code just destroyed the device, booooo
	 */
}

int kgsl_mmu_checkconsistency(struct kgsl_pagetable *pagetable)
{
	unsigned int pte;
	unsigned int data;
	struct kgsl_pte_debug *pte_debug;

	if (pagetable->last_superpte % GSL_PT_SUPER_PTE != 0)
		return GSL_FAILURE;

	/* go through page table and make sure there are no detectable errors */
	pte = 0;
	while (pte < pagetable->max_entries) {
		pte_debug = GSL_PT_MAP_DEBUG(pte);

		if (GSL_PT_MAP_GETADDR(pte) != 0) {
			/* pte is in use */
			/* access first couple bytes of a page */
			data = *((unsigned int *)GSL_PT_VIRT_GET(pte));
		}
		pte++;
	}
	return GSL_SUCCESS;
}

int kgsl_mmu_destroypagetableobject(struct kgsl_mmu *mmu)
{
	unsigned int tmp_id;
	struct kgsl_device *tmp_device;
	struct kgsl_pagetable *pagetable;

	pagetable = kgsl_mmu_getpagetableobject(mmu);

	/* if pagetable object exists for current "current device mmu"/"current caller process" combination */
	if (pagetable) {
		/* no more "device mmu"/"caller process" combinations attached to current pagetable object */
		if (pagetable->refcnt == 0) {
#ifdef DEBUG
			/* memory leak check */
			if (pagetable->last_superpte != 0 || GSL_PT_MAP_GETADDR(pagetable->last_superpte))
				return GSL_FAILURE;
#endif
		}

		if (pagetable->base.gpuaddr) {
			kgsl_sharedmem_free(&pagetable->base);
		}

		kfree(pagetable);

		/* clear pagetable object reference for all "device mmu"/"current caller process" combinations */
		for (tmp_id = KGSL_DEVICE_ANY + 1; tmp_id <= KGSL_DEVICE_MAX; tmp_id++) {
			tmp_device = &gsl_driver.device[tmp_id-1];

			if (tmp_device->mmu.flags & KGSL_FLAGS_STARTED)
				tmp_device->mmu.pagetable = NULL;
		}
	}

	return (GSL_SUCCESS);
}

/* create pagetable object for "current device mmu"/"current caller
 * process" combination. If none exists, setup a new pagetable object. */
struct kgsl_pagetable *kgsl_mmu_createpagetableobject(struct kgsl_mmu *mmu)
{
	int status = GSL_SUCCESS;
	struct kgsl_pagetable *tmp_pagetable = NULL;
	unsigned int tmp_id;
	struct kgsl_device *tmp_device;
	unsigned int flags;

	if (!mmu->pagetable) {
		/* then, check if pagetable object already exists for any "other device mmu" */
		for (tmp_id = KGSL_DEVICE_ANY + 1; tmp_id <= KGSL_DEVICE_MAX; tmp_id++) {
			tmp_device = &gsl_driver.device[tmp_id-1];

			if (tmp_device->mmu.flags & KGSL_FLAGS_STARTED) {
				if (tmp_device->mmu.pagetable) {
					tmp_pagetable = tmp_device->mmu.pagetable;
					break;
				}
			}
		}

		/* pagetable object exists */
		if (tmp_pagetable) {
			/* set pagetable object reference */
			mmu->pagetable = tmp_pagetable;
		} else {
			/* create new pagetable object */
			mmu->pagetable = (void *)kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
			if (!mmu->pagetable)
				return NULL;

			mmu->pagetable->refcnt        = 0;
			mmu->pagetable->va_base       = mmu->va_base;
			mmu->pagetable->va_range      = mmu->va_range;
			mmu->pagetable->last_superpte = 0;
			mmu->pagetable->max_entries   = (mmu->va_range >> PAGE_SHIFT) + GSL_PT_EXTRA_ENTRIES;

			/* allocate page table memory */
			flags  = (GSL_MEMFLAGS_ALIGN4K | GSL_MEMFLAGS_CONPHYS | GSL_MEMFLAGS_STRICTREQUEST);
			status = kgsl_sharedmem_alloc(flags, mmu->pagetable->max_entries * GSL_PT_ENTRY_SIZEBYTES, &mmu->pagetable->base);

			if (status == GSL_SUCCESS) {
				kgsl_sharedmem_set(&mmu->pagetable->base, 0, 0, mmu->pagetable->base.size);
			} else {
				kgsl_mmu_destroypagetableobject(mmu);
			}
		}
	}

	return mmu->pagetable;
}

/* set device mmu to use current caller process's page table */
int kgsl_mmu_setpagetable(struct kgsl_device *device)
{
	int status = GSL_SUCCESS;
	unsigned int devindex = device->id-1; /* device_id is 1 based */
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		struct kgsl_pagetable *pagetable = kgsl_mmu_getpagetableobject(mmu);

		mmu->hwpagetable = pagetable;

		GSL_TLBFLUSH_FILTER_RESET();

		status = mmu->device->ftbl.mmu_tlbinvalidate(mmu->device, mmu_reg[devindex].invalidate);
	}

	return status;
}

/*
 * initialize device mmu
 * call this with the global lock held
 */
int kgsl_mmu_init(struct kgsl_device *device)
{
	int status;
	unsigned int flags;
	struct kgsl_pagetable  *pagetable;
	unsigned int devindex = device->id-1;
	struct kgsl_mmu *mmu = &device->mmu;

	if (device->ftbl.mmu_tlbinvalidate == NULL || device->ftbl.mmu_setpagetable == NULL ||
			!(device->flags & KGSL_FLAGS_INITIALIZED))
		return GSL_FAILURE;

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0)
		return GSL_SUCCESS;

	/* setup backward reference */
	mmu->device = device;

	/* disable MMU when running in safe mode */
	if (device->flags & KGSL_FLAGS_SAFEMODE)
		mmu->config = 0x00000000;

	/* setup MMU and sub-client behavior */
	device->ftbl.regwrite(device, mmu_reg[devindex].config, mmu->config);

	device->ftbl.regwrite(device, mmu_reg[devindex].interrupt_mask, GSL_MMU_INT_MASK);

	mmu->refcnt  = 0;
	mmu->flags  |= KGSL_FLAGS_INITIALIZED0;

	/* MMU enabled */
	if (mmu->config & 0x1) {
		/* idle device */
		pr_info("%s: calling idle on device %d\n", __func__, device->id);
		device->ftbl.idle(device, GSL_TIMEOUT_DEFAULT);

		// define physical memory range accessible by the core
		device->ftbl.regwrite(device, mmu_reg[devindex].mpu_base, mmu->mpu_base);
		device->ftbl.regwrite(device, mmu_reg[devindex].mpu_end,  mmu->mpu_base + mmu->mpu_range);

		device->ftbl.regwrite(device, mmu_reg[devindex].interrupt_mask,
				GSL_MMU_INT_MASK | MH_INTERRUPT_MASK__MMU_PAGE_FAULT);

		mmu->flags |= KGSL_FLAGS_INITIALIZED;

		/* sub-client MMU lookups require address translation */
		if ((mmu->config & ~0x1) > 0) {
			/* make sure virtual address range is a multiple of 64Kb ? */
			/* setup pagetable object */
			pagetable = kgsl_mmu_createpagetableobject(mmu);
			if (!pagetable) {
				kgsl_mmu_close(device);
				return GSL_FAILURE;
			}

			mmu->hwpagetable = pagetable;

			/* create tlb flush filter to track dirty superPTE's -- one bit per superPTE */
			mmu->tlbflushfilter.size = (mmu->va_range / (PAGE_SIZE * GSL_PT_SUPER_PTE * 8)) + 1;
			mmu->tlbflushfilter.base = (unsigned int *)kmalloc(mmu->tlbflushfilter.size, GFP_KERNEL);
			if (!mmu->tlbflushfilter.base) {
				kgsl_mmu_close(device);
				return GSL_FAILURE;
			}

			GSL_TLBFLUSH_FILTER_RESET();

			/* set page table base */
			device->ftbl.regwrite(device, mmu_reg[devindex].pt_page, mmu->hwpagetable->base.gpuaddr);

			/* define virtual address range */
			device->ftbl.regwrite(device, mmu_reg[devindex].va_range, (mmu->hwpagetable->va_base | (mmu->hwpagetable->va_range >> 16)));

			/* allocate memory used for completing r/w operations that cannot be mapped by the MMU */
			flags  = (GSL_MEMFLAGS_ALIGN32 | GSL_MEMFLAGS_CONPHYS | GSL_MEMFLAGS_STRICTREQUEST);
			status = kgsl_sharedmem_alloc(flags, 32, &mmu->dummyspace);
			if (status != GSL_SUCCESS) {
				kgsl_mmu_close(device);
				return status;
			}

			device->ftbl.regwrite(device, mmu_reg[devindex].trans_error, mmu->dummyspace.gpuaddr);

			device->ftbl.mmu_tlbinvalidate(device, mmu_reg[devindex].invalidate);

			mmu->flags |= KGSL_FLAGS_STARTED;
		}
	}
	return GSL_SUCCESS;
}

/* map physical pages into the gpu page table */
int kgsl_mmu_map(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, const gsl_scatterlist_t *scatterlist, unsigned int flags)
{
	int status = GSL_SUCCESS;
	unsigned int i, phyaddr, ap;
	unsigned int pte, ptefirst, ptelast, superpte;
	int flushtlb;
	struct kgsl_pagetable *pagetable;

	if (scatterlist->num <= 0)
		return GSL_FAILURE;

	/* get gpu access permissions */
	ap = GSL_PT_PAGE_AP[((flags & GSL_MEMFLAGS_GPUAP_MASK) >> GSL_MEMFLAGS_GPUAP_SHIFT)];

	pagetable = kgsl_mmu_getpagetableobject(mmu);
	if (!pagetable)
		return GSL_FAILURE;

	/* debug only but no harm.. */
	kgsl_mmu_checkconsistency(pagetable);

	ptefirst = GSL_PT_ENTRY_GET(gpubaseaddr);
	ptelast  = GSL_PT_ENTRY_GET(gpubaseaddr + (PAGE_SIZE * (scatterlist->num-1)));
	flushtlb = 0;

	if (!GSL_PT_MAP_GETADDR(ptefirst)) {
		/* tlb needs to be flushed when the first and last pte are not at superpte boundaries */
		if ((ptefirst & (GSL_PT_SUPER_PTE-1)) != 0 ||
			((ptelast+1) & (GSL_PT_SUPER_PTE-1)) != 0)
			flushtlb = 1;

		/* create page table entries */
		for (pte = ptefirst; pte <= ptelast; pte++) {
			if (scatterlist->contiguous)
				phyaddr = scatterlist->pages[0] + ((pte-ptefirst) * PAGE_SIZE);
			else
				phyaddr = scatterlist->pages[pte-ptefirst];

			GSL_PT_MAP_SETADDR(pte, phyaddr);
			GSL_PT_MAP_SETBITS(pte, ap);

			/* tlb needs to be flushed when a dirty superPTE gets backed */
			if ((pte & (GSL_PT_SUPER_PTE-1)) == 0) {
				if (GSL_TLBFLUSH_FILTER_ISDIRTY(pte / GSL_PT_SUPER_PTE))
					flushtlb = 1;
			}
		}


		if (flushtlb) {
			/* every device's tlb needs to be flushed because the current page table is shared among all devices */
			for (i = 0; i < KGSL_DEVICE_MAX; i++) {
				if (gsl_driver.device[i].flags & KGSL_FLAGS_INITIALIZED) {
					gsl_driver.device[i].mmu.flags |= GSL_MMUFLAGS_TLBFLUSH;
				}
			}
		}

		/* determine new last mapped superPTE */
		superpte = ptelast - (ptelast & (GSL_PT_SUPER_PTE-1));
		if (superpte > pagetable->last_superpte)
			pagetable->last_superpte = superpte;
	} else {
		/* this should never happen */
		status = GSL_FAILURE;
	}
	return status;
}

static bool is_superpte_empty(struct kgsl_pagetable  *pagetable, unsigned int superpte)
{
	int i;
	for (i = 0; i < GSL_PT_SUPER_PTE; i++) {
		if (GSL_PT_MAP_GET(superpte+i))
			return false;
	}

	return true;
}

/* remove mappings in the specified address range from the gpu page table */
int kgsl_mmu_unmap(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, int range)
{
	int status = GSL_SUCCESS;
	struct kgsl_pagetable *pagetable;
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast, superpte;

	if (range <= 0)
		return GSL_FAILURE;

	numpages = (range >> PAGE_SHIFT);
	if (range & (PAGE_SIZE-1))
		numpages++;

	pagetable = kgsl_mmu_getpagetableobject(mmu);
	if (!pagetable)
		return GSL_FAILURE;

	/* check consistency here? */

	ptefirst = GSL_PT_ENTRY_GET(gpubaseaddr);
	ptelast  = GSL_PT_ENTRY_GET(gpubaseaddr + (PAGE_SIZE * (numpages-1)));

	if (GSL_PT_MAP_GETADDR(ptefirst)) {
		superpte = ptefirst - (ptefirst & (GSL_PT_SUPER_PTE-1));
		GSL_TLBFLUSH_FILTER_SETDIRTY(superpte / GSL_PT_SUPER_PTE);
		/* remove page table entries */

		for (pte = ptefirst; pte <= ptelast; pte++) {
			GSL_PT_MAP_RESET(pte);
			superpte = pte - (pte & (GSL_PT_SUPER_PTE-1));
			if (pte == superpte) {
				GSL_TLBFLUSH_FILTER_SETDIRTY(superpte / GSL_PT_SUPER_PTE);
			}
		}

		/* determine new last mapped superPTE */
		superpte = ptelast - (ptelast & (GSL_PT_SUPER_PTE-1));
		if (superpte == pagetable->last_superpte && pagetable->last_superpte >= GSL_PT_SUPER_PTE) {
			do {
				if (is_superpte_empty(pagetable, superpte))
					pagetable->last_superpte -= GSL_PT_SUPER_PTE;
				else
					break;
			} while (!GSL_PT_MAP_GETADDR(pagetable->last_superpte) && pagetable->last_superpte >= GSL_PT_SUPER_PTE);
		}
        } else {
		/* this should never happen */
		status = GSL_FAILURE;
	}

	/* invalidate tlb, debug only */
	//mmu->device->ftbl.mmu_tlbinvalidate(mmu->device, gsl_cfg_mmu_reg[mmu->device->id-1].INVALIDATE);

	return status;
}

/* obtain scatter list of physical pages for the given gpu address range.
 * if all pages are physically contiguous they are coalesced into a single
 * scatterlist entry. */
int kgsl_mmu_getmap(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, int range, gsl_scatterlist_t *scatterlist)
{
	struct kgsl_pagetable  *pagetable;
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast;
	unsigned int contiguous = 1;

	numpages = (range >> PAGE_SHIFT);
	if (range & (PAGE_SIZE-1))
		numpages++;

	if (range <= 0 || scatterlist->num != numpages)
		return GSL_FAILURE;

	pagetable = kgsl_mmu_getpagetableobject(mmu);
	if (!pagetable)
		return GSL_FAILURE;

	ptefirst = GSL_PT_ENTRY_GET(gpubaseaddr);
	ptelast  = GSL_PT_ENTRY_GET(gpubaseaddr + (PAGE_SIZE * (numpages-1)));

	/* determine whether pages are physically contiguous */
	if (numpages > 1) {
		for (pte = ptefirst; pte <= (ptelast - 1); pte++) {
			if ((GSL_PT_MAP_GETADDR(pte) + PAGE_SIZE) != GSL_PT_MAP_GETADDR(pte+1)) {
				contiguous = 0;
				break;
			}
		}
	}

	if (!contiguous) {
		/* populate scatter list */
		for (pte = ptefirst; pte <= ptelast; pte++) {
			scatterlist->pages[pte-ptefirst] = GSL_PT_MAP_GETADDR(pte);
		}
	} else {
		/* coalesce physically contiguous pages into a single scatter list entry */
		scatterlist->pages[0] = GSL_PT_MAP_GETADDR(ptefirst);
	}

	scatterlist->contiguous = contiguous;
	return GSL_SUCCESS;
}

/* close device mmu. call this with the global lock held */
int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int  devindex = mmu->device->id-1;

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0) {
		if (mmu->flags & KGSL_FLAGS_STARTED) {
			/* terminate pagetable object */
			kgsl_mmu_destroypagetableobject(mmu);
		}

		/* no more processes attached to current device mmu */
		if (mmu->refcnt == 0) {
#ifdef DEBUG
			int i;
			/* check if there are any orphaned pagetable objects lingering around */
			for (i = 0; i < GSL_MMU_PAGETABLE_MAX; i++) {
				if (mmu->pagetable[i])
					return GSL_FAILURE;
			}
#endif

			/* disable mh interrupts */
			device->ftbl.regwrite(device, mmu_reg[devindex].interrupt_mask, 0);

			/* disable MMU */
			device->ftbl.regwrite(device, mmu_reg[devindex].config, 0x00000000);

			if (mmu->tlbflushfilter.base)
				kfree(mmu->tlbflushfilter.base);

			if (mmu->dummyspace.gpuaddr)
				kgsl_sharedmem_free(&mmu->dummyspace);

			mmu->flags &= ~KGSL_FLAGS_STARTED;
			mmu->flags &= ~KGSL_FLAGS_INITIALIZED;
			mmu->flags &= ~KGSL_FLAGS_INITIALIZED0;
		}
	}

	return GSL_SUCCESS;
}

/* attach process. call with global lock held */
int kgsl_mmu_attachcallback(struct kgsl_mmu *mmu)
{
	int status = GSL_SUCCESS;
	struct kgsl_pagetable  *pagetable;

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0) {
		/* attach to current device mmu */
		mmu->refcnt++;

		if (mmu->flags & KGSL_FLAGS_STARTED) {
			/* attach to pagetable object */
			pagetable = kgsl_mmu_createpagetableobject(mmu);

			if (pagetable)
				pagetable->refcnt++;
			else
				status = GSL_FAILURE;
		}
	}
	return status;
}

/* detach process */
int kgsl_mmu_detachcallback(struct kgsl_mmu *mmu)
{
	int status = GSL_SUCCESS;
	struct kgsl_pagetable  *pagetable;

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0) {
		/* detach from current device mmu */
		mmu->refcnt--;

		if (mmu->flags & KGSL_FLAGS_STARTED) {
			/* detach from pagetable object */
			pagetable = kgsl_mmu_getpagetableobject(mmu);

			if (pagetable)
				pagetable->refcnt--;
			else
				status = GSL_FAILURE;
		}
	}
	return status;
}
