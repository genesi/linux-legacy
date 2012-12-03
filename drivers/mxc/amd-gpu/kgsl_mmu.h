/* Copyright (c) 2002,2007-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __GSL_MMU_H
#define __GSL_MMU_H

#include "kgsl_types.h"
#include "kgsl_buildconfig.h" // ugh stats

//////////////////////////////////////////////////////////////////////////////
// defines
//////////////////////////////////////////////////////////////////////////////
#ifdef GSL_STATS_MMU
#define GSL_MMU_STATS(x) x
#else
#define GSL_MMU_STATS(x)
#endif // GSL_STATS_MMU

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
#define GSL_MMU_PAGETABLE_MAX               GSL_CALLER_PROCESS_MAX      // all device mmu's share a single page table per process
#else
#define GSL_MMU_PAGETABLE_MAX               1                           // all device mmu's share a single global page table
#endif

#define GSL_PT_SUPER_PTE                    8


//////////////////////////////////////////////////////////////////////////////
//  types
//////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
// ---------
// mmu debug
// ---------
typedef struct _gsl_mmu_debug_t {
    unsigned int  config;
    unsigned int  mpu_base;
    unsigned int  mpu_end;
    unsigned int  va_range;
    unsigned int  pt_base;
    unsigned int  page_fault;
    unsigned int  trans_error;
    unsigned int  invalidate;
} gsl_mmu_debug_t;
#endif // _DEBUG

// ------------
// mmu register
// ------------
struct kgsl_mmu_reg {
    unsigned int  CONFIG;
    unsigned int  MPU_BASE;
    unsigned int  MPU_END;
    unsigned int  VA_RANGE;
    unsigned int  PT_BASE;
    unsigned int  PAGE_FAULT;
    unsigned int  TRAN_ERROR;
    unsigned int  INVALIDATE;
/*
    unsigned int interrupt_mask;
    unsigned int interrupt_status;
    unsigned int interrupt_clear;
*/
};

// ------------
// mh interrupt
// ------------
typedef struct _gsl_mh_intr_t
{
    gsl_intrid_t  AXI_READ_ERROR;
    gsl_intrid_t  AXI_WRITE_ERROR;
    gsl_intrid_t  MMU_PAGE_FAULT;
} gsl_mh_intr_t;

// ----------------
// page table stats
// ----------------
struct kgsl_ptstats {
    __s64  maps;
    __s64  unmaps;
    __s64  switches;
};

// ---------
// mmu stats
// ---------
typedef struct _gsl_mmustats_t {
	struct kgsl_ptstats pt;
	__s64        tlbflushes;
} gsl_mmustats_t;

// -----------------
// page table object
// -----------------
struct kgsl_pagetable {
	unsigned int   pid;
    unsigned int   refcnt;
    struct kgsl_memdesc  base;
    uint32_t      va_base;
    unsigned int   va_range;
    unsigned int   last_superpte;
    unsigned int   max_entries;
};

// -------------------------
// tlb flush filter object
// -------------------------
struct kgsl_tlbflushfilter {
    unsigned int  *base;
    unsigned int  size;
};

// ----------
// mmu object
// ----------
struct kgsl_mmu {
    unsigned int          refcnt;
    unsigned int           flags;
    struct kgsl_device          *device;
    unsigned int          config;
    uint32_t             mpu_base;
    int                   mpu_range;
    uint32_t             va_base;
    unsigned int          va_range;
    struct kgsl_memdesc         dummyspace;
    struct kgsl_tlbflushfilter  tlbflushfilter;
    struct kgsl_pagetable       *hwpagetable;                     // current page table object being used by device mmu
    struct kgsl_pagetable       *pagetable[GSL_MMU_PAGETABLE_MAX];    // page table object table
    struct mutex		*mutex;
#ifdef GSL_STATS_MMU
	gsl_mmustats_t        stats;
#endif  // GSL_STATS_MMU
};


//////////////////////////////////////////////////////////////////////////////
//  inline functions
//////////////////////////////////////////////////////////////////////////////
static __inline int
kgsl_mmu_isenabled(struct kgsl_mmu *mmu)
{
    // address translation enabled
    int enabled = ((mmu)->flags & KGSL_FLAGS_STARTED) ? 1 : 0;

    return (enabled);
}


//////////////////////////////////////////////////////////////////////////////
//  prototypes
//////////////////////////////////////////////////////////////////////////////
int    kgsl_mmu_init(struct kgsl_device *device);
int    kgsl_mmu_close(struct kgsl_device *device);
int    kgsl_mmu_attachcallback(struct kgsl_mmu *mmu, unsigned int pid);
int    kgsl_mmu_detachcallback(struct kgsl_mmu *mmu, unsigned int pid);
int    kgsl_mmu_setpagetable(struct kgsl_device *device, unsigned int pid);
int    kgsl_mmu_map(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, const gsl_scatterlist_t *scatterlist, unsigned int flags, unsigned int pid);
int    kgsl_mmu_unmap(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, int range, unsigned int pid);
int    kgsl_mmu_getmap(struct kgsl_mmu *mmu, uint32_t gpubaseaddr, int range, gsl_scatterlist_t *scatterlist, unsigned int pid);
int    kgsl_mmu_querystats(struct kgsl_mmu *mmu, gsl_mmustats_t *stats);
int    kgsl_mmu_bist(struct kgsl_mmu *mmu);

#endif // __GSL_MMU_H
