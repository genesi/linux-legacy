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
/* ------------------------
 * Yamato ringbuffer config
 * ------------------------ */
static const unsigned int gsl_cfg_rb_sizelog2quadwords = GSL_RB_SIZE_32K;
static const unsigned int gsl_cfg_rb_blksizequadwords  = GSL_RB_SIZE_16;

/* -----------------------------
 * interrupt block register data
 * ----------------------------- */
static const gsl_intrblock_reg_t gsl_cfg_intrblock_reg[GSL_INTR_BLOCK_COUNT] = {
    {   /* Yamato MH */
	GSL_INTR_BLOCK_YDX_MH,
	GSL_INTR_YDX_MH_AXI_READ_ERROR,
	GSL_INTR_YDX_MH_MMU_PAGE_FAULT,
	REG_MH_INTERRUPT_STATUS,
	REG_MH_INTERRUPT_CLEAR,
	REG_MH_INTERRUPT_MASK
    },
    {   /* Yamato CP */
	GSL_INTR_BLOCK_YDX_CP,
	GSL_INTR_YDX_CP_SW_INT,
	GSL_INTR_YDX_CP_RING_BUFFER,
	REG_CP_INT_STATUS,
	REG_CP_INT_ACK,
	REG_CP_INT_CNTL
    },
    {   /* Yamato RBBM */
	GSL_INTR_BLOCK_YDX_RBBM,
	GSL_INTR_YDX_RBBM_READ_ERROR,
	GSL_INTR_YDX_RBBM_GUI_IDLE,
	REG_RBBM_INT_STATUS,
	REG_RBBM_INT_ACK,
	REG_RBBM_INT_CNTL
    },
    {   /* Yamato SQ */
	GSL_INTR_BLOCK_YDX_SQ,
	GSL_INTR_YDX_SQ_PS_WATCHDOG,
	GSL_INTR_YDX_SQ_VS_WATCHDOG,
	REG_SQ_INT_STATUS,
	REG_SQ_INT_ACK,
	REG_SQ_INT_CNTL
    },
    {   /* G12 */
	GSL_INTR_BLOCK_G12,
	GSL_INTR_G12_MH,
#ifndef _Z180
	GSL_INTR_G12_FBC,
#else
	GSL_INTR_G12_FIFO,
#endif /* _Z180 */
	(ADDR_VGC_IRQSTATUS >> 2),
	(ADDR_VGC_IRQSTATUS >> 2),
	(ADDR_VGC_IRQENABLE >> 2)
    },
    {   /* G12 MH */
	GSL_INTR_BLOCK_G12_MH,
	GSL_INTR_G12_MH_AXI_READ_ERROR,
	GSL_INTR_G12_MH_MMU_PAGE_FAULT,
	ADDR_MH_INTERRUPT_STATUS,       /* G12 MH offsets are considered to be dword based, therefore no down shift */
	ADDR_MH_INTERRUPT_CLEAR,
	ADDR_MH_INTERRUPT_MASK
    },
};

/* -----------------------
 * interrupt mask bit data
 * ----------------------- */
static const int gsl_cfg_intr_mask[GSL_INTR_COUNT] = {
    MH_INTERRUPT_MASK__AXI_READ_ERROR,
    MH_INTERRUPT_MASK__AXI_WRITE_ERROR,
    MH_INTERRUPT_MASK__MMU_PAGE_FAULT,

    CP_INT_CNTL__SW_INT_MASK,
    CP_INT_CNTL__T0_PACKET_IN_IB_MASK,
    CP_INT_CNTL__OPCODE_ERROR_MASK,
    CP_INT_CNTL__PROTECTED_MODE_ERROR_MASK,
    CP_INT_CNTL__RESERVED_BIT_ERROR_MASK,
    CP_INT_CNTL__IB_ERROR_MASK,
    CP_INT_CNTL__IB2_INT_MASK,
    CP_INT_CNTL__IB1_INT_MASK,
    CP_INT_CNTL__RB_INT_MASK,

    RBBM_INT_CNTL__RDERR_INT_MASK,
    RBBM_INT_CNTL__DISPLAY_UPDATE_INT_MASK,
    RBBM_INT_CNTL__GUI_IDLE_INT_MASK,

    SQ_INT_CNTL__PS_WATCHDOG_MASK,
    SQ_INT_CNTL__VS_WATCHDOG_MASK,

    REG_VGC_IRQSTATUS__MH_MASK,
    REG_VGC_IRQSTATUS__G2D_MASK,
    REG_VGC_IRQSTATUS__FIFO_MASK,
#ifndef _Z180
    REG_VGC_IRQSTATUS__FBC_MASK,
#endif
    MH_INTERRUPT_MASK__AXI_READ_ERROR,
    MH_INTERRUPT_MASK__AXI_WRITE_ERROR,
    MH_INTERRUPT_MASK__MMU_PAGE_FAULT,
};

/* -----------------
 * mmu register data
 * ----------------- */
static const gsl_mmu_reg_t gsl_cfg_mmu_reg[KGSL_DEVICE_MAX] = {
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

/* -----------------
 * mh interrupt data
 * ----------------- */
static const gsl_mh_intr_t gsl_cfg_mh_intr[KGSL_DEVICE_MAX] =
{
    {   /* Yamato */
	GSL_INTR_YDX_MH_AXI_READ_ERROR,
	GSL_INTR_YDX_MH_AXI_WRITE_ERROR,
	GSL_INTR_YDX_MH_MMU_PAGE_FAULT,
    },
    {   /* G12 */
	GSL_INTR_G12_MH_AXI_READ_ERROR,
	GSL_INTR_G12_MH_AXI_WRITE_ERROR,
	GSL_INTR_G12_MH_MMU_PAGE_FAULT,
    }
};

#endif /* __GSL__CONFIG_H */
