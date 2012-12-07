/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

#ifndef __GSL__CONFIG_H
#define __GSL__CONFIG_H

#include "kgsl_types.h" // GSL_DEVICE_MAX
#include "kgsl_mmu.h" // kgsl_mmu_stuff

#include "kgsl_ringbuffer.h" // GSL_RB_SIZE_*

#include "g12_reg.h" // MH stuff
#include "yamato_reg.h" // bleh

static const struct kgsl_mmu_reg kgsl_cfg_mmu_reg[KGSL_DEVICE_MAX] = {
    {   /* Yamato */
	REG_MH_MMU_CONFIG,
	REG_MH_MMU_MPU_BASE,
	REG_MH_MMU_MPU_END,
	REG_MH_MMU_VA_RANGE,
	REG_MH_MMU_PT_BASE,
	REG_MH_MMU_PAGE_FAULT,
	REG_MH_MMU_TRAN_ERROR,
	REG_MH_MMU_INVALIDATE,
    },
    {   /* G12 - MH offsets are considered to be dword based, therefore no down shift */
	ADDR_MH_MMU_CONFIG,
	ADDR_MH_MMU_MPU_BASE,
	ADDR_MH_MMU_MPU_END,
	ADDR_MH_MMU_VA_RANGE,
	ADDR_MH_MMU_PT_BASE,
	ADDR_MH_MMU_PAGE_FAULT,
	ADDR_MH_MMU_TRAN_ERROR,
	ADDR_MH_MMU_INVALIDATE,
    },
};

#endif /* __GSL__CONFIG_H */
