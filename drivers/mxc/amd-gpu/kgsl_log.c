/* Copyright (c) 2002,2008-2009, Code Aurora Forum. All rights reserved.
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

#ifdef CONFIG_KGSL_LOGGING

#define _CRT_SECURE_NO_WARNINGS

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>

#include "gsl.h"

#define REG_OUTPUT( X ) case X: b += sprintf( b, "%s", #X ); break;
#define INTRID_OUTPUT( X ) case X: b += sprintf( b, "%s", #X ); break;

typedef struct log_output
{
    unsigned int        flags;
    struct log_output*  next;
} log_output_t;

static log_output_t* outputs = NULL;

static struct mutex   log_mutex;
static char         buffer[1024];
static char         buffer2[1024];
static int          log_initialized = 0;

//----------------------------------------------------------------------------

int kgsl_log_start( unsigned int log_flags )
{
    log_output_t* output;

    if( log_initialized ) return GSL_SUCCESS;

    mutex_init(&log_mutex);
    log_initialized = 1;

    output = kmalloc( sizeof( log_output_t ), GFP_KERNEL );
    output->flags = log_flags;

    // Add to the list
    if( outputs == NULL )
    {
        // First node in the list.
        outputs = output;
        output->next = NULL;
    }
    else
    {
        // Add to the start of the list
        output->next = outputs;
        outputs = output;
    }

    return GSL_SUCCESS;
}

//----------------------------------------------------------------------------

int kgsl_log_finish()
{
    if( !log_initialized ) return GSL_SUCCESS;

    // Go throught output list and free every node
    while( outputs != NULL )
    {
        log_output_t* temp = outputs->next;

        kfree( outputs );
        outputs = temp;
    }

    log_initialized = 0;

    return GSL_SUCCESS;
}

//----------------------------------------------------------------------------

int kgsl_log_write( unsigned int log_flags, char* format, ... )
{
    char            *c = format;
    char            *b = buffer;
    char            *p1, *p2;
    log_output_t*   output;
    va_list         arguments;

    if( !log_initialized ) return GSL_SUCCESS;

    // Acquire mutex lock as we are using shared buffer for the string parsing
    mutex_lock(&log_mutex);

    // Add separator
    *(b++) = '|'; *(b++) = ' ';

    va_start( arguments, format );

    while( 1 )
    {
        // Find the first occurence of %
        p1 = strchr( c, '%' );
        if( !p1 )
        {
            // No more % characters -> copy rest of the string
            strcpy( b, c );

            break;
        }

        // Find the second occurence of % and handle the string until that point
        p2 = strchr( p1+1, '%' );

        // If not found, just use the end of the buffer
        if( !p2 ) p2 = strchr( p1+1, '\0' );

        // Break the string to this point
        memcpy( buffer2, c, p2-c );
        *(buffer2+(unsigned int)(p2-c)) = '\0';

        switch( *(p1+1) )
        {
            // struct kgsl_memdesc
            case 'M':
            {
                struct kgsl_memdesc *val = va_arg( arguments, struct kgsl_memdesc *);
                // Handle string before %M
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %M
                b += sprintf( b, "[hostptr=0x%08x,gpuaddr=0x%08x,size=%u,flags=%x]", val->hostptr, val->gpuaddr, val->size, (unsigned int) val->priv );
                // Handle string after %M
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
            }
            break;

            // GSL_SUCCESS/GSL_FAILURE
            case 'B':
            {
                int val = va_arg( arguments, int );
		char *s;
                // Handle string before %B
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %B
		switch (val) {
			case GSL_SUCCESS:
				s = "GSL_SUCCESS";
				break;
			case GSL_FAILURE:
			default:
				s = "GSL_FAILURE";
				break;
		}
		b += sprintf( b, "%s", s );
                // Handle string after %B
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
            }
            break;

            // unsigned int
            case 'D':
            {
                unsigned int val = va_arg( arguments, unsigned int );
                // Handle string before %D
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %D
                switch( val )
                {
                    case GSL_DEVICE_ANY:
                        b += sprintf( b, "%s", "GSL_DEVICE_ANY" );
                    break;
                    case GSL_DEVICE_YAMATO:
                        b += sprintf( b, "%s", "GSL_DEVICE_YAMATO" );
                    break;
                    case GSL_DEVICE_G12:
                        b += sprintf( b, "%s", "GSL_DEVICE_G12" );
                    break;
                    default:
                        b += sprintf( b, "%s", "UNKNOWN DEVICE" );
                    break;
                }
                // Handle string after %D
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
            }
            break;

            // gsl_intrid_t
            case 'I':
            {
                unsigned int val = va_arg( arguments, unsigned int );
                // Handle string before %I
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %I
                switch( val )
                {
                    INTRID_OUTPUT( GSL_INTR_YDX_MH_AXI_READ_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_MH_AXI_WRITE_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_MH_MMU_PAGE_FAULT );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_SW_INT );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_T0_PACKET_IN_IB );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_OPCODE_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_RESERVED_BIT_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_IB_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_IB2_INT );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_IB1_INT );
                    INTRID_OUTPUT( GSL_INTR_YDX_CP_RING_BUFFER );
                    INTRID_OUTPUT( GSL_INTR_YDX_RBBM_READ_ERROR );
                    INTRID_OUTPUT( GSL_INTR_YDX_RBBM_DISPLAY_UPDATE );
                    INTRID_OUTPUT( GSL_INTR_YDX_RBBM_GUI_IDLE );
                    INTRID_OUTPUT( GSL_INTR_YDX_SQ_PS_WATCHDOG );
                    INTRID_OUTPUT( GSL_INTR_YDX_SQ_VS_WATCHDOG );
                    INTRID_OUTPUT( GSL_INTR_G12_MH );
                    INTRID_OUTPUT( GSL_INTR_G12_G2D );
                    INTRID_OUTPUT( GSL_INTR_G12_FIFO );
#ifndef _Z180
                    INTRID_OUTPUT( GSL_INTR_G12_FBC );
#endif // _Z180
                    INTRID_OUTPUT( GSL_INTR_G12_MH_AXI_READ_ERROR );
                    INTRID_OUTPUT( GSL_INTR_G12_MH_AXI_WRITE_ERROR );
                    INTRID_OUTPUT( GSL_INTR_G12_MH_MMU_PAGE_FAULT );
                    INTRID_OUTPUT( GSL_INTR_COUNT );
                    INTRID_OUTPUT( GSL_INTR_FOOBAR );

                    default:
                        b += sprintf( b, "%s", "UNKNOWN INTERRUPT ID" );
                    break;
                }
                // Handle string after %I
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
            }
            break;

            // Register offset
            case 'R':
            {
                unsigned int val = va_arg( arguments, unsigned int );

                // Handle string before %R
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %R
                switch( val )
                {
                    REG_OUTPUT( REG_PA_CL_VPORT_XSCALE ); REG_OUTPUT( REG_PA_CL_VPORT_XOFFSET ); REG_OUTPUT( REG_PA_CL_VPORT_YSCALE );
                    REG_OUTPUT( REG_PA_CL_VPORT_YOFFSET ); REG_OUTPUT( REG_PA_CL_VPORT_ZSCALE ); REG_OUTPUT( REG_PA_CL_VPORT_ZOFFSET );
                    REG_OUTPUT( REG_PA_CL_VTE_CNTL ); REG_OUTPUT( REG_PA_CL_CLIP_CNTL ); REG_OUTPUT( REG_PA_CL_GB_VERT_CLIP_ADJ );
                    REG_OUTPUT( REG_PA_CL_GB_VERT_DISC_ADJ ); REG_OUTPUT( REG_PA_CL_GB_HORZ_CLIP_ADJ ); REG_OUTPUT( REG_PA_CL_GB_HORZ_DISC_ADJ );
                    REG_OUTPUT( REG_PA_CL_ENHANCE ); REG_OUTPUT( REG_PA_SC_ENHANCE ); REG_OUTPUT( REG_PA_SU_VTX_CNTL );
                    REG_OUTPUT( REG_PA_SU_POINT_SIZE ); REG_OUTPUT( REG_PA_SU_POINT_MINMAX ); REG_OUTPUT( REG_PA_SU_LINE_CNTL );
                    REG_OUTPUT( REG_PA_SU_FACE_DATA ); REG_OUTPUT( REG_PA_SU_SC_MODE_CNTL ); REG_OUTPUT( REG_PA_SU_POLY_OFFSET_FRONT_SCALE );
                    REG_OUTPUT( REG_PA_SU_POLY_OFFSET_FRONT_OFFSET ); REG_OUTPUT( REG_PA_SU_POLY_OFFSET_BACK_SCALE ); REG_OUTPUT( REG_PA_SU_POLY_OFFSET_BACK_OFFSET );
                    REG_OUTPUT( REG_PA_SU_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER1_SELECT ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER2_SELECT );
                    REG_OUTPUT( REG_PA_SU_PERFCOUNTER3_SELECT ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER0_HI );
                    REG_OUTPUT( REG_PA_SU_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER1_HI ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER2_LOW );
                    REG_OUTPUT( REG_PA_SU_PERFCOUNTER2_HI ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER3_LOW ); REG_OUTPUT( REG_PA_SU_PERFCOUNTER3_HI );
                    REG_OUTPUT( REG_PA_SC_WINDOW_OFFSET ); REG_OUTPUT( REG_PA_SC_AA_CONFIG ); REG_OUTPUT( REG_PA_SC_AA_MASK );
                    REG_OUTPUT( REG_PA_SC_LINE_STIPPLE ); REG_OUTPUT( REG_PA_SC_LINE_CNTL ); REG_OUTPUT( REG_PA_SC_WINDOW_SCISSOR_TL );
                    REG_OUTPUT( REG_PA_SC_WINDOW_SCISSOR_BR ); REG_OUTPUT( REG_PA_SC_SCREEN_SCISSOR_TL ); REG_OUTPUT( REG_PA_SC_SCREEN_SCISSOR_BR );
                    REG_OUTPUT( REG_PA_SC_VIZ_QUERY ); REG_OUTPUT( REG_PA_SC_VIZ_QUERY_STATUS ); REG_OUTPUT( REG_PA_SC_LINE_STIPPLE_STATE );
                    REG_OUTPUT( REG_PA_SC_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_PA_SC_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_PA_SC_PERFCOUNTER0_HI );
                    REG_OUTPUT( REG_PA_CL_CNTL_STATUS ); REG_OUTPUT( REG_PA_SU_CNTL_STATUS ); REG_OUTPUT( REG_PA_SC_CNTL_STATUS );
                    REG_OUTPUT( REG_PA_SU_DEBUG_CNTL ); REG_OUTPUT( REG_PA_SU_DEBUG_DATA ); REG_OUTPUT( REG_PA_SC_DEBUG_CNTL );
                    REG_OUTPUT( REG_PA_SC_DEBUG_DATA ); REG_OUTPUT( REG_GFX_COPY_STATE ); REG_OUTPUT( REG_VGT_DRAW_INITIATOR );
                    REG_OUTPUT( REG_VGT_EVENT_INITIATOR ); REG_OUTPUT( REG_VGT_DMA_BASE ); REG_OUTPUT( REG_VGT_DMA_SIZE );
                    REG_OUTPUT( REG_VGT_BIN_BASE ); REG_OUTPUT( REG_VGT_BIN_SIZE ); REG_OUTPUT( REG_VGT_CURRENT_BIN_ID_MIN );
                    REG_OUTPUT( REG_VGT_CURRENT_BIN_ID_MAX ); REG_OUTPUT( REG_VGT_IMMED_DATA ); REG_OUTPUT( REG_VGT_MAX_VTX_INDX );
                    REG_OUTPUT( REG_VGT_MIN_VTX_INDX ); REG_OUTPUT( REG_VGT_INDX_OFFSET ); REG_OUTPUT( REG_VGT_VERTEX_REUSE_BLOCK_CNTL );
                    REG_OUTPUT( REG_VGT_OUT_DEALLOC_CNTL ); REG_OUTPUT( REG_VGT_MULTI_PRIM_IB_RESET_INDX ); REG_OUTPUT( REG_VGT_ENHANCE );
                    REG_OUTPUT( REG_VGT_VTX_VECT_EJECT_REG ); REG_OUTPUT( REG_VGT_LAST_COPY_STATE ); REG_OUTPUT( REG_VGT_DEBUG_CNTL );
                    REG_OUTPUT( REG_VGT_DEBUG_DATA ); REG_OUTPUT( REG_VGT_CNTL_STATUS ); REG_OUTPUT( REG_VGT_CRC_SQ_DATA );
                    REG_OUTPUT( REG_VGT_CRC_SQ_CTRL ); REG_OUTPUT( REG_VGT_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_VGT_PERFCOUNTER1_SELECT );
                    REG_OUTPUT( REG_VGT_PERFCOUNTER2_SELECT ); REG_OUTPUT( REG_VGT_PERFCOUNTER3_SELECT ); REG_OUTPUT( REG_VGT_PERFCOUNTER0_LOW );
                    REG_OUTPUT( REG_VGT_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_VGT_PERFCOUNTER2_LOW ); REG_OUTPUT( REG_VGT_PERFCOUNTER3_LOW );
                    REG_OUTPUT( REG_VGT_PERFCOUNTER0_HI ); REG_OUTPUT( REG_VGT_PERFCOUNTER1_HI ); REG_OUTPUT( REG_VGT_PERFCOUNTER2_HI );
                    REG_OUTPUT( REG_VGT_PERFCOUNTER3_HI ); REG_OUTPUT( REG_TC_CNTL_STATUS ); REG_OUTPUT( REG_TCR_CHICKEN );
                    REG_OUTPUT( REG_TCF_CHICKEN ); REG_OUTPUT( REG_TCM_CHICKEN ); REG_OUTPUT( REG_TCR_PERFCOUNTER0_SELECT );
                    REG_OUTPUT( REG_TCR_PERFCOUNTER1_SELECT ); REG_OUTPUT( REG_TCR_PERFCOUNTER0_HI ); REG_OUTPUT( REG_TCR_PERFCOUNTER1_HI );
                    REG_OUTPUT( REG_TCR_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_TCR_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_TP_TC_CLKGATE_CNTL );
                    REG_OUTPUT( REG_TPC_CNTL_STATUS ); REG_OUTPUT( REG_TPC_DEBUG0 ); REG_OUTPUT( REG_TPC_DEBUG1 );
                    REG_OUTPUT( REG_TPC_CHICKEN ); REG_OUTPUT( REG_TP0_CNTL_STATUS ); REG_OUTPUT( REG_TP0_DEBUG );
                    REG_OUTPUT( REG_TP0_CHICKEN ); REG_OUTPUT( REG_TP0_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_TP0_PERFCOUNTER0_HI );
                    REG_OUTPUT( REG_TP0_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_TP0_PERFCOUNTER1_SELECT ); REG_OUTPUT( REG_TP0_PERFCOUNTER1_HI );
                    REG_OUTPUT( REG_TP0_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_TCM_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_TCM_PERFCOUNTER1_SELECT );
                    REG_OUTPUT( REG_TCM_PERFCOUNTER0_HI ); REG_OUTPUT( REG_TCM_PERFCOUNTER1_HI ); REG_OUTPUT( REG_TCM_PERFCOUNTER0_LOW );
                    REG_OUTPUT( REG_TCM_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER1_SELECT );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER2_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER3_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER4_SELECT );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER5_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER6_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER7_SELECT );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER8_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER9_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER10_SELECT );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER11_SELECT ); REG_OUTPUT( REG_TCF_PERFCOUNTER0_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER1_HI );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER2_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER3_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER4_HI );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER5_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER6_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER7_HI );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER8_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER9_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER10_HI );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER11_HI ); REG_OUTPUT( REG_TCF_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER1_LOW );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER2_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER3_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER4_LOW );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER5_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER6_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER7_LOW );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER8_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER9_LOW ); REG_OUTPUT( REG_TCF_PERFCOUNTER10_LOW );
                    REG_OUTPUT( REG_TCF_PERFCOUNTER11_LOW ); REG_OUTPUT( REG_TCF_DEBUG ); REG_OUTPUT( REG_TCA_FIFO_DEBUG );
                    REG_OUTPUT( REG_TCA_PROBE_DEBUG ); REG_OUTPUT( REG_TCA_TPC_DEBUG ); REG_OUTPUT( REG_TCB_CORE_DEBUG );
                    REG_OUTPUT( REG_TCB_TAG0_DEBUG ); REG_OUTPUT( REG_TCB_TAG1_DEBUG ); REG_OUTPUT( REG_TCB_TAG2_DEBUG );
                    REG_OUTPUT( REG_TCB_TAG3_DEBUG ); REG_OUTPUT( REG_TCB_FETCH_GEN_SECTOR_WALKER0_DEBUG ); REG_OUTPUT( REG_TCB_FETCH_GEN_WALKER_DEBUG );
                    REG_OUTPUT( REG_TCB_FETCH_GEN_PIPE0_DEBUG ); REG_OUTPUT( REG_TCD_INPUT0_DEBUG ); REG_OUTPUT( REG_TCD_DEGAMMA_DEBUG );
                    REG_OUTPUT( REG_TCD_DXTMUX_SCTARB_DEBUG ); REG_OUTPUT( REG_TCD_DXTC_ARB_DEBUG ); REG_OUTPUT( REG_TCD_STALLS_DEBUG );
                    REG_OUTPUT( REG_TCO_STALLS_DEBUG ); REG_OUTPUT( REG_TCO_QUAD0_DEBUG0 ); REG_OUTPUT( REG_TCO_QUAD0_DEBUG1 );
                    REG_OUTPUT( REG_SQ_GPR_MANAGEMENT ); REG_OUTPUT( REG_SQ_FLOW_CONTROL ); REG_OUTPUT( REG_SQ_INST_STORE_MANAGMENT );
                    REG_OUTPUT( REG_SQ_RESOURCE_MANAGMENT ); REG_OUTPUT( REG_SQ_EO_RT ); REG_OUTPUT( REG_SQ_DEBUG_MISC );
                    REG_OUTPUT( REG_SQ_ACTIVITY_METER_CNTL ); REG_OUTPUT( REG_SQ_ACTIVITY_METER_STATUS ); REG_OUTPUT( REG_SQ_INPUT_ARB_PRIORITY );
                    REG_OUTPUT( REG_SQ_THREAD_ARB_PRIORITY ); REG_OUTPUT( REG_SQ_VS_WATCHDOG_TIMER ); REG_OUTPUT( REG_SQ_PS_WATCHDOG_TIMER );
                    REG_OUTPUT( REG_SQ_INT_CNTL ); REG_OUTPUT( REG_SQ_INT_STATUS ); REG_OUTPUT( REG_SQ_INT_ACK );
                    REG_OUTPUT( REG_SQ_DEBUG_INPUT_FSM ); REG_OUTPUT( REG_SQ_DEBUG_CONST_MGR_FSM ); REG_OUTPUT( REG_SQ_DEBUG_TP_FSM );
                    REG_OUTPUT( REG_SQ_DEBUG_FSM_ALU_0 ); REG_OUTPUT( REG_SQ_DEBUG_FSM_ALU_1 ); REG_OUTPUT( REG_SQ_DEBUG_EXP_ALLOC );
                    REG_OUTPUT( REG_SQ_DEBUG_PTR_BUFF ); REG_OUTPUT( REG_SQ_DEBUG_GPR_VTX ); REG_OUTPUT( REG_SQ_DEBUG_GPR_PIX );
                    REG_OUTPUT( REG_SQ_DEBUG_TB_STATUS_SEL ); REG_OUTPUT( REG_SQ_DEBUG_VTX_TB_0 ); REG_OUTPUT( REG_SQ_DEBUG_VTX_TB_1 );
                    REG_OUTPUT( REG_SQ_DEBUG_VTX_TB_STATUS_REG ); REG_OUTPUT( REG_SQ_DEBUG_VTX_TB_STATE_MEM ); REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_0 );
                    REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_STATUS_REG_0 ); REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_STATUS_REG_1 ); REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_STATUS_REG_2 );
                    REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_STATUS_REG_3 ); REG_OUTPUT( REG_SQ_DEBUG_PIX_TB_STATE_MEM ); REG_OUTPUT( REG_SQ_PERFCOUNTER0_SELECT );
                    REG_OUTPUT( REG_SQ_PERFCOUNTER1_SELECT ); REG_OUTPUT( REG_SQ_PERFCOUNTER2_SELECT ); REG_OUTPUT( REG_SQ_PERFCOUNTER3_SELECT );
                    REG_OUTPUT( REG_SQ_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_SQ_PERFCOUNTER0_HI ); REG_OUTPUT( REG_SQ_PERFCOUNTER1_LOW );
                    REG_OUTPUT( REG_SQ_PERFCOUNTER1_HI ); REG_OUTPUT( REG_SQ_PERFCOUNTER2_LOW ); REG_OUTPUT( REG_SQ_PERFCOUNTER2_HI );
                    REG_OUTPUT( REG_SQ_PERFCOUNTER3_LOW ); REG_OUTPUT( REG_SQ_PERFCOUNTER3_HI ); REG_OUTPUT( REG_SX_PERFCOUNTER0_SELECT );
                    REG_OUTPUT( REG_SX_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_SX_PERFCOUNTER0_HI ); REG_OUTPUT( REG_SQ_INSTRUCTION_ALU_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_ALU_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_ALU_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_EXEC_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_CF_EXEC_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_EXEC_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_LOOP_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_CF_LOOP_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_LOOP_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_JMP_CALL_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_CF_JMP_CALL_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_JMP_CALL_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_ALLOC_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_CF_ALLOC_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_CF_ALLOC_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_TFETCH_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_TFETCH_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_TFETCH_2 ); REG_OUTPUT( REG_SQ_INSTRUCTION_VFETCH_0 );
                    REG_OUTPUT( REG_SQ_INSTRUCTION_VFETCH_1 ); REG_OUTPUT( REG_SQ_INSTRUCTION_VFETCH_2 ); REG_OUTPUT( REG_SQ_CONSTANT_0 );
                    REG_OUTPUT( REG_SQ_CONSTANT_1 ); REG_OUTPUT( REG_SQ_CONSTANT_2 ); REG_OUTPUT( REG_SQ_CONSTANT_3 );
                    REG_OUTPUT( REG_SQ_FETCH_0 ); REG_OUTPUT( REG_SQ_FETCH_1 ); REG_OUTPUT( REG_SQ_FETCH_2 );
                    REG_OUTPUT( REG_SQ_FETCH_3 ); REG_OUTPUT( REG_SQ_FETCH_4 ); REG_OUTPUT( REG_SQ_FETCH_5 );
                    REG_OUTPUT( REG_SQ_CONSTANT_VFETCH_0 ); REG_OUTPUT( REG_SQ_CONSTANT_VFETCH_1 ); REG_OUTPUT( REG_SQ_CONSTANT_T2 );
                    REG_OUTPUT( REG_SQ_CONSTANT_T3 ); REG_OUTPUT( REG_SQ_CF_BOOLEANS ); REG_OUTPUT( REG_SQ_CF_LOOP );
                    REG_OUTPUT( REG_SQ_CONSTANT_RT_0 ); REG_OUTPUT( REG_SQ_CONSTANT_RT_1 ); REG_OUTPUT( REG_SQ_CONSTANT_RT_2 );
                    REG_OUTPUT( REG_SQ_CONSTANT_RT_3 ); REG_OUTPUT( REG_SQ_FETCH_RT_0 ); REG_OUTPUT( REG_SQ_FETCH_RT_1 );
                    REG_OUTPUT( REG_SQ_FETCH_RT_2 ); REG_OUTPUT( REG_SQ_FETCH_RT_3 ); REG_OUTPUT( REG_SQ_FETCH_RT_4 );
                    REG_OUTPUT( REG_SQ_FETCH_RT_5 ); REG_OUTPUT( REG_SQ_CF_RT_BOOLEANS ); REG_OUTPUT( REG_SQ_CF_RT_LOOP );
                    REG_OUTPUT( REG_SQ_VS_PROGRAM ); REG_OUTPUT( REG_SQ_PS_PROGRAM ); REG_OUTPUT( REG_SQ_CF_PROGRAM_SIZE );
                    REG_OUTPUT( REG_SQ_INTERPOLATOR_CNTL ); REG_OUTPUT( REG_SQ_PROGRAM_CNTL ); REG_OUTPUT( REG_SQ_WRAPPING_0 );
                    REG_OUTPUT( REG_SQ_WRAPPING_1 ); REG_OUTPUT( REG_SQ_VS_CONST ); REG_OUTPUT( REG_SQ_PS_CONST );
                    REG_OUTPUT( REG_SQ_CONTEXT_MISC ); REG_OUTPUT( REG_SQ_CF_RD_BASE ); REG_OUTPUT( REG_SQ_DEBUG_MISC_0 );
                    REG_OUTPUT( REG_SQ_DEBUG_MISC_1 ); REG_OUTPUT( REG_MH_ARBITER_CONFIG ); REG_OUTPUT( REG_MH_CLNT_AXI_ID_REUSE );
                    REG_OUTPUT( REG_MH_INTERRUPT_MASK ); REG_OUTPUT( REG_MH_INTERRUPT_STATUS ); REG_OUTPUT( REG_MH_INTERRUPT_CLEAR );
                    REG_OUTPUT( REG_MH_AXI_ERROR ); REG_OUTPUT( REG_MH_PERFCOUNTER0_SELECT ); REG_OUTPUT( REG_MH_PERFCOUNTER1_SELECT );
                    REG_OUTPUT( REG_MH_PERFCOUNTER0_CONFIG ); REG_OUTPUT( REG_MH_PERFCOUNTER1_CONFIG ); REG_OUTPUT( REG_MH_PERFCOUNTER0_LOW );
                    REG_OUTPUT( REG_MH_PERFCOUNTER1_LOW ); REG_OUTPUT( REG_MH_PERFCOUNTER0_HI ); REG_OUTPUT( REG_MH_PERFCOUNTER1_HI );
                    REG_OUTPUT( REG_MH_DEBUG_CTRL ); REG_OUTPUT( REG_MH_DEBUG_DATA ); REG_OUTPUT( REG_MH_AXI_HALT_CONTROL );
                    REG_OUTPUT( REG_MH_MMU_CONFIG ); REG_OUTPUT( REG_MH_MMU_VA_RANGE ); REG_OUTPUT( REG_MH_MMU_PT_BASE );
                    REG_OUTPUT( REG_MH_MMU_PAGE_FAULT ); REG_OUTPUT( REG_MH_MMU_TRAN_ERROR ); REG_OUTPUT( REG_MH_MMU_INVALIDATE );
                    REG_OUTPUT( REG_MH_MMU_MPU_BASE ); REG_OUTPUT( REG_MH_MMU_MPU_END ); REG_OUTPUT( REG_WAIT_UNTIL );
                    REG_OUTPUT( REG_RBBM_ISYNC_CNTL ); REG_OUTPUT( REG_RBBM_STATUS ); REG_OUTPUT( REG_RBBM_DSPLY );
                    REG_OUTPUT( REG_RBBM_RENDER_LATEST ); REG_OUTPUT( REG_RBBM_RTL_RELEASE ); REG_OUTPUT( REG_RBBM_PATCH_RELEASE );
                    REG_OUTPUT( REG_RBBM_AUXILIARY_CONFIG ); REG_OUTPUT( REG_RBBM_PERIPHID0 ); REG_OUTPUT( REG_RBBM_PERIPHID1 );
                    REG_OUTPUT( REG_RBBM_PERIPHID2 ); REG_OUTPUT( REG_RBBM_PERIPHID3 ); REG_OUTPUT( REG_RBBM_CNTL );
                    REG_OUTPUT( REG_RBBM_SKEW_CNTL ); REG_OUTPUT( REG_RBBM_SOFT_RESET ); REG_OUTPUT( REG_RBBM_PM_OVERRIDE1 );
                    REG_OUTPUT( REG_RBBM_PM_OVERRIDE2 ); REG_OUTPUT( REG_GC_SYS_IDLE ); REG_OUTPUT( REG_NQWAIT_UNTIL );
                    REG_OUTPUT( REG_RBBM_DEBUG_OUT ); REG_OUTPUT( REG_RBBM_DEBUG_CNTL ); REG_OUTPUT( REG_RBBM_DEBUG );
                    REG_OUTPUT( REG_RBBM_READ_ERROR ); REG_OUTPUT( REG_RBBM_WAIT_IDLE_CLOCKS ); REG_OUTPUT( REG_RBBM_INT_CNTL );
                    REG_OUTPUT( REG_RBBM_INT_STATUS ); REG_OUTPUT( REG_RBBM_INT_ACK ); REG_OUTPUT( REG_MASTER_INT_SIGNAL );
                    REG_OUTPUT( REG_RBBM_PERFCOUNTER1_SELECT ); REG_OUTPUT( REG_RBBM_PERFCOUNTER1_LO ); REG_OUTPUT( REG_RBBM_PERFCOUNTER1_HI );
                    REG_OUTPUT( REG_CP_RB_BASE ); REG_OUTPUT( REG_CP_RB_CNTL ); REG_OUTPUT( REG_CP_RB_RPTR_ADDR );
                    REG_OUTPUT( REG_CP_RB_RPTR ); REG_OUTPUT( REG_CP_RB_RPTR_WR ); REG_OUTPUT( REG_CP_RB_WPTR );
                    REG_OUTPUT( REG_CP_RB_WPTR_DELAY ); REG_OUTPUT( REG_CP_RB_WPTR_BASE ); REG_OUTPUT( REG_CP_IB1_BASE );
                    REG_OUTPUT( REG_CP_IB1_BUFSZ ); REG_OUTPUT( REG_CP_IB2_BASE ); REG_OUTPUT( REG_CP_IB2_BUFSZ );
                    REG_OUTPUT( REG_CP_ST_BASE ); REG_OUTPUT( REG_CP_ST_BUFSZ ); REG_OUTPUT( REG_CP_QUEUE_THRESHOLDS );
                    REG_OUTPUT( REG_CP_MEQ_THRESHOLDS ); REG_OUTPUT( REG_CP_CSQ_AVAIL ); REG_OUTPUT( REG_CP_STQ_AVAIL );
                    REG_OUTPUT( REG_CP_MEQ_AVAIL ); REG_OUTPUT( REG_CP_CSQ_RB_STAT ); REG_OUTPUT( REG_CP_CSQ_IB1_STAT );
                    REG_OUTPUT( REG_CP_CSQ_IB2_STAT ); REG_OUTPUT( REG_CP_NON_PREFETCH_CNTRS ); REG_OUTPUT( REG_CP_STQ_ST_STAT );
                    REG_OUTPUT( REG_CP_MEQ_STAT ); REG_OUTPUT( REG_CP_MIU_TAG_STAT ); REG_OUTPUT( REG_CP_CMD_INDEX );
                    REG_OUTPUT( REG_CP_CMD_DATA ); REG_OUTPUT( REG_CP_ME_CNTL ); REG_OUTPUT( REG_CP_ME_STATUS );
                    REG_OUTPUT( REG_CP_ME_RAM_WADDR ); REG_OUTPUT( REG_CP_ME_RAM_RADDR ); REG_OUTPUT( REG_CP_ME_RAM_DATA );
                    REG_OUTPUT( REG_CP_ME_RDADDR ); REG_OUTPUT( REG_CP_DEBUG ); REG_OUTPUT( REG_SCRATCH_REG0 );
                    REG_OUTPUT( REG_SCRATCH_REG1 ); REG_OUTPUT( REG_SCRATCH_REG2 ); REG_OUTPUT( REG_SCRATCH_REG3 );
                    REG_OUTPUT( REG_SCRATCH_REG4 ); REG_OUTPUT( REG_SCRATCH_REG5 ); REG_OUTPUT( REG_SCRATCH_REG6 );
                    REG_OUTPUT( REG_SCRATCH_REG7 );
                    REG_OUTPUT( REG_SCRATCH_UMSK ); REG_OUTPUT( REG_SCRATCH_ADDR ); REG_OUTPUT( REG_CP_ME_VS_EVENT_SRC );
                    REG_OUTPUT( REG_CP_ME_VS_EVENT_ADDR ); REG_OUTPUT( REG_CP_ME_VS_EVENT_DATA ); REG_OUTPUT( REG_CP_ME_VS_EVENT_ADDR_SWM );
                    REG_OUTPUT( REG_CP_ME_VS_EVENT_DATA_SWM ); REG_OUTPUT( REG_CP_ME_PS_EVENT_SRC ); REG_OUTPUT( REG_CP_ME_PS_EVENT_ADDR );
                    REG_OUTPUT( REG_CP_ME_PS_EVENT_DATA ); REG_OUTPUT( REG_CP_ME_PS_EVENT_ADDR_SWM ); REG_OUTPUT( REG_CP_ME_PS_EVENT_DATA_SWM );
                    REG_OUTPUT( REG_CP_ME_CF_EVENT_SRC ); REG_OUTPUT( REG_CP_ME_CF_EVENT_ADDR ); REG_OUTPUT( REG_CP_ME_CF_EVENT_DATA );
                    REG_OUTPUT( REG_CP_ME_NRT_ADDR ); REG_OUTPUT( REG_CP_ME_NRT_DATA ); REG_OUTPUT( REG_CP_ME_VS_FETCH_DONE_SRC );
                    REG_OUTPUT( REG_CP_ME_VS_FETCH_DONE_ADDR ); REG_OUTPUT( REG_CP_ME_VS_FETCH_DONE_DATA ); REG_OUTPUT( REG_CP_INT_CNTL );
                    REG_OUTPUT( REG_CP_INT_STATUS ); REG_OUTPUT( REG_CP_INT_ACK ); REG_OUTPUT( REG_CP_PFP_UCODE_ADDR );
                    REG_OUTPUT( REG_CP_PFP_UCODE_DATA ); REG_OUTPUT( REG_CP_PERFMON_CNTL ); REG_OUTPUT( REG_CP_PERFCOUNTER_SELECT );
                    REG_OUTPUT( REG_CP_PERFCOUNTER_LO ); REG_OUTPUT( REG_CP_PERFCOUNTER_HI ); REG_OUTPUT( REG_CP_BIN_MASK_LO );
                    REG_OUTPUT( REG_CP_BIN_MASK_HI ); REG_OUTPUT( REG_CP_BIN_SELECT_LO ); REG_OUTPUT( REG_CP_BIN_SELECT_HI );
                    REG_OUTPUT( REG_CP_NV_FLAGS_0 ); REG_OUTPUT( REG_CP_NV_FLAGS_1 ); REG_OUTPUT( REG_CP_NV_FLAGS_2 );
                    REG_OUTPUT( REG_CP_NV_FLAGS_3 ); REG_OUTPUT( REG_CP_STATE_DEBUG_INDEX ); REG_OUTPUT( REG_CP_STATE_DEBUG_DATA );
                    REG_OUTPUT( REG_CP_PROG_COUNTER ); REG_OUTPUT( REG_CP_STAT ); REG_OUTPUT( REG_BIOS_0_SCRATCH );
                    REG_OUTPUT( REG_BIOS_1_SCRATCH ); REG_OUTPUT( REG_BIOS_2_SCRATCH ); REG_OUTPUT( REG_BIOS_3_SCRATCH );
                    REG_OUTPUT( REG_BIOS_4_SCRATCH ); REG_OUTPUT( REG_BIOS_5_SCRATCH ); REG_OUTPUT( REG_BIOS_6_SCRATCH );
                    REG_OUTPUT( REG_BIOS_7_SCRATCH ); REG_OUTPUT( REG_BIOS_8_SCRATCH ); REG_OUTPUT( REG_BIOS_9_SCRATCH );
                    REG_OUTPUT( REG_BIOS_10_SCRATCH ); REG_OUTPUT( REG_BIOS_11_SCRATCH ); REG_OUTPUT( REG_BIOS_12_SCRATCH );
                    REG_OUTPUT( REG_BIOS_13_SCRATCH ); REG_OUTPUT( REG_BIOS_14_SCRATCH ); REG_OUTPUT( REG_BIOS_15_SCRATCH );
                    REG_OUTPUT( REG_COHER_SIZE_PM4 ); REG_OUTPUT( REG_COHER_BASE_PM4 ); REG_OUTPUT( REG_COHER_STATUS_PM4 );
                    REG_OUTPUT( REG_COHER_SIZE_HOST ); REG_OUTPUT( REG_COHER_BASE_HOST ); REG_OUTPUT( REG_COHER_STATUS_HOST );
                    REG_OUTPUT( REG_COHER_DEST_BASE_0 ); REG_OUTPUT( REG_COHER_DEST_BASE_1 ); REG_OUTPUT( REG_COHER_DEST_BASE_2 );
                    REG_OUTPUT( REG_COHER_DEST_BASE_3 ); REG_OUTPUT( REG_COHER_DEST_BASE_4 ); REG_OUTPUT( REG_COHER_DEST_BASE_5 );
                    REG_OUTPUT( REG_COHER_DEST_BASE_6 ); REG_OUTPUT( REG_COHER_DEST_BASE_7 ); REG_OUTPUT( REG_RB_SURFACE_INFO );
                    REG_OUTPUT( REG_RB_COLOR_INFO ); REG_OUTPUT( REG_RB_DEPTH_INFO ); REG_OUTPUT( REG_RB_STENCILREFMASK );
                    REG_OUTPUT( REG_RB_ALPHA_REF ); REG_OUTPUT( REG_RB_COLOR_MASK ); REG_OUTPUT( REG_RB_BLEND_RED );
                    REG_OUTPUT( REG_RB_BLEND_GREEN ); REG_OUTPUT( REG_RB_BLEND_BLUE ); REG_OUTPUT( REG_RB_BLEND_ALPHA );
                    REG_OUTPUT( REG_RB_FOG_COLOR ); REG_OUTPUT( REG_RB_STENCILREFMASK_BF ); REG_OUTPUT( REG_RB_DEPTHCONTROL );
                    REG_OUTPUT( REG_RB_BLENDCONTROL ); REG_OUTPUT( REG_RB_COLORCONTROL ); REG_OUTPUT( REG_RB_MODECONTROL );
                    REG_OUTPUT( REG_RB_COLOR_DEST_MASK ); REG_OUTPUT( REG_RB_COPY_CONTROL ); REG_OUTPUT( REG_RB_COPY_DEST_BASE );
                    REG_OUTPUT( REG_RB_COPY_DEST_PITCH ); REG_OUTPUT( REG_RB_COPY_DEST_INFO ); REG_OUTPUT( REG_RB_COPY_DEST_PIXEL_OFFSET );
                    REG_OUTPUT( REG_RB_DEPTH_CLEAR ); REG_OUTPUT( REG_RB_SAMPLE_COUNT_CTL ); REG_OUTPUT( REG_RB_SAMPLE_COUNT_ADDR );
                    REG_OUTPUT( REG_RB_BC_CONTROL ); REG_OUTPUT( REG_RB_EDRAM_INFO ); REG_OUTPUT( REG_RB_CRC_RD_PORT );
                    REG_OUTPUT( REG_RB_CRC_CONTROL ); REG_OUTPUT( REG_RB_CRC_MASK ); REG_OUTPUT( REG_RB_PERFCOUNTER0_SELECT );
                    REG_OUTPUT( REG_RB_PERFCOUNTER0_LOW ); REG_OUTPUT( REG_RB_PERFCOUNTER0_HI ); REG_OUTPUT( REG_RB_TOTAL_SAMPLES );
                    REG_OUTPUT( REG_RB_ZPASS_SAMPLES ); REG_OUTPUT( REG_RB_ZFAIL_SAMPLES ); REG_OUTPUT( REG_RB_SFAIL_SAMPLES );
                    REG_OUTPUT( REG_RB_DEBUG_0 ); REG_OUTPUT( REG_RB_DEBUG_1 ); REG_OUTPUT( REG_RB_DEBUG_2 );
                    REG_OUTPUT( REG_RB_DEBUG_3 ); REG_OUTPUT( REG_RB_DEBUG_4 ); REG_OUTPUT( REG_RB_FLAG_CONTROL );
                    REG_OUTPUT( REG_RB_BC_SPARES ); REG_OUTPUT( REG_BC_DUMMY_CRAYRB_ENUMS ); REG_OUTPUT( REG_BC_DUMMY_CRAYRB_MOREENUMS );

                    default:
                        b += sprintf( b, "%s", "UNKNOWN REGISTER OFFSET" );
                    break;
                }
                // Handle string after %R
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
            }
            break;

	    // gsl_scatterlist_t
	    case 'S':
	    {
		gsl_scatterlist_t *val = va_arg(arguments, gsl_scatterlist_t *);
		// handle string before %S
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %S
                b += sprintf( b, "[contiguous=%d,num=%u]",val->contiguous,val->num);
                // Handle string after %S
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
	    }
	    break;

	    // gsl_property_type_t
	    case 'T':
	    {
		char *prop;
		gsl_property_type_t val = va_arg(arguments, gsl_property_type_t);
		// handle string before %T
                memcpy( b, c, p1-c );
                b += (unsigned int)p1-(unsigned int)c;
                // Replace %T
		switch (val) {
			case GSL_PROP_DEVICE_INFO:
				prop = "GSL_PROP_DEVICE_INFO";
			break;
			case GSL_PROP_DEVICE_SHADOW:
				prop = "GSL_PROP_DEVICE_SHADOW";
			break;
			case GSL_PROP_DEVICE_POWER:
				prop = "GSL_PROP_DEVICE_POWER";
			break;
			case GSL_PROP_SHMEM:
				prop = "GSL_PROP_SHMEM";
			break;
			case GSL_PROP_SHMEM_APERTURES:
				prop = "GSL_PROP_SHMEM_APERTURES";
			break;
			case GSL_PROP_DEVICE_DMI:
				prop = "GSL_PROP_DEVICE_DMI";
			break;
			default:
				prop = "????????";
			break;
		}
                b += sprintf( b, "%s",prop);
                // Handle string after %T
                memcpy( b, p1+2, p2-(p1+2) );
                b += (unsigned int)p2-(unsigned int)(p1+2);
                *b = '\0';
	    }
	    break;

            default:
            {
                int val = va_arg( arguments, int );
                // Standard format. Use vsprintf.
                b += sprintf( b, buffer2, val );
            }
            break;
        }

        c = p2;
    }

    // Add this string to all outputs
    output = outputs;

    while( output != NULL )
    {
        // Filter according to the flags
        if( ( output->flags & log_flags ) == log_flags )
        {
		/*
                    // Write timestamp if enabled
                    if( output->flags & KGSL_LOG_TIMESTAMP )
                        printf( "[Timestamp: %d] ", kos_timestamp() );
                    // Write process id if enabled
                    if( output->flags & KGSL_LOG_PROCESS_ID )
                        printf( "[Process ID: %d] ", kos_process_getid() );
                    // Write thread id if enabled
                    if( output->flags & KGSL_LOG_THREAD_ID )
                        printf( "[Thread ID: %d] ", kos_thread_getid() );
		*/
		// Write the message
		printk( buffer );
        }

        output = output->next;
    }

    va_end( arguments );

    mutex_unlock(&log_mutex );

    return GSL_SUCCESS;
}

//----------------------------------------------------------------------------
#endif
