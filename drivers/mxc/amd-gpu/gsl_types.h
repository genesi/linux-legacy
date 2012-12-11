/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
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

#ifndef __GSL_TYPES_H
#define __GSL_TYPES_H

#include <linux/types.h>
#include "stddef.h"

typedef enum {
    OS_PROTECTION_GLOBAL,   // inter process
    OS_PROTECTION_LOCAL,    // process local
    OS_PROTECTION_NONE,     // none
} os_protection_t;

typedef struct _os_cputimer_t {
    int refcount;                   // Reference count
    int enabled;                    // Counter is enabled
    int size;                       // Number of counters
    __s64  start_time;            // start time in cpu ticks
    __s64  end_time;              // end time in cpu ticks
    __s64  timer_frequency;       // cpu ticks per second
    __s64  *counter_array;        // number of ticks for each counter
} os_cputimer_t;

//////////////////////////////////////////////////////////////////////////////
// DMI flags
//////////////////////////////////////////////////////////////////////////////
#define GSL_DMIFLAGS_ENABLE_SINGLE      0x00000001  //  Single buffered DMI
#define GSL_DMIFLAGS_ENABLE_DOUBLE      0x00000002  //  Double buffered DMI
#define GSL_DMIFLAGS_ENABLE_TRIPLE      0x00000004  //  Triple buffered DMI
#define GSL_DMIFLAGS_DISABLE            0x00000008
#define GSL_DMIFLAGS_NEXT_BUFFER        0x00000010

#define GSL_TIMEOUT_NONE                        0
#define GSL_TIMEOUT_DEFAULT                     0xFFFFFFFF

#define GSL_PAGESIZE                            0x1000
#define GSL_PAGESIZE_SHIFT                      12

#define GSL_TIMESTAMP_EPSILON           20000

typedef struct _gsl_devmemstore_t {
    volatile unsigned int  soptimestamp;
    unsigned int           sbz;
    volatile unsigned int  eoptimestamp;
    unsigned int           sbz2;
} gsl_devmemstore_t;

#define GSL_DEVICE_MEMSTORE_OFFSET(field)       offsetof(gsl_devmemstore_t, field)

typedef enum _gsl_apertureid_t
{
    GSL_APERTURE_EMEM   = (GSL_MEMFLAGS_EMEM),
    GSL_APERTURE_PHYS   = (GSL_MEMFLAGS_CONPHYS >> GSL_MEMFLAGS_APERTURE_SHIFT),
    GSL_APERTURE_MMU    = (GSL_APERTURE_EMEM | 0x10000000),
    GSL_APERTURE_MAX    = 2,

    GSL_APERTURE_FOOBAR = 0x7FFFFFFF
} gsl_apertureid_t;

typedef enum _gsl_channelid_t
{
    GSL_CHANNEL_1      = (GSL_MEMFLAGS_CHANNEL1 >> GSL_MEMFLAGS_CHANNEL_SHIFT),
    GSL_CHANNEL_2      = (GSL_MEMFLAGS_CHANNEL2 >> GSL_MEMFLAGS_CHANNEL_SHIFT),
    GSL_CHANNEL_3      = (GSL_MEMFLAGS_CHANNEL3 >> GSL_MEMFLAGS_CHANNEL_SHIFT),
    GSL_CHANNEL_4      = (GSL_MEMFLAGS_CHANNEL4 >> GSL_MEMFLAGS_CHANNEL_SHIFT),
    GSL_CHANNEL_MAX    = 4,

    GSL_CHANNEL_FOOBAR = 0x7FFFFFFF
} gsl_channelid_t;

typedef enum _gsl_ap_t
{
    GSL_AP_NULL   = 0x0,
    GSL_AP_R      = 0x1,
    GSL_AP_W      = 0x2,
    GSL_AP_RW     = 0x3,
    GSL_AP_X      = 0x4,
    GSL_AP_RWX    = 0x5,
    GSL_AP_MAX    = 0x6,

    GSL_AP_FOOBAR = 0x7FFFFFFF
} gsl_ap_t;

// -------------
// memory region
// -------------
typedef struct _gsl_memregion_t {
    unsigned char  *mmio_virt_base;
    unsigned int   mmio_phys_base;
    gpuaddr_t      gpu_base;
    unsigned int   sizebytes;
} gsl_memregion_t;

// ---------------------------------
// physical page scatter/gatter list
// ---------------------------------
typedef struct _gsl_scatterlist_t {
    int           contiguous;       // flag whether pages on the list are physically contiguous
    unsigned int  num;
    unsigned int  *pages;
} gsl_scatterlist_t;

// --------------
// mem free queue
// --------------
//
// this could be compressed down into the just the memdesc for the node
//
typedef struct _gsl_memnode_t {
    gsl_timestamp_t       timestamp;
    gsl_memdesc_t         memdesc;
    unsigned int          pid;
    struct _gsl_memnode_t *next;
} gsl_memnode_t;

typedef struct _gsl_memqueue_t {
    gsl_memnode_t   *head;
    gsl_memnode_t   *tail;
} gsl_memqueue_t;

// ---------
// rectangle
// ---------
typedef struct _gsl_rect_t {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
	unsigned int pitch;
} gsl_rect_t;

// -----------------------
// pixel buffer descriptor
// -----------------------
typedef struct _gsl_buffer_desc_t {
    gsl_memdesc_t data;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int format;
    unsigned int enabled;
} gsl_buffer_desc_t;

// ------------
// interrupt id
// ------------
typedef enum _gsl_intrid_t
{
  GSL_INTR_YDX_MH_AXI_READ_ERROR = 0,
  GSL_INTR_YDX_MH_AXI_WRITE_ERROR,
  GSL_INTR_YDX_MH_MMU_PAGE_FAULT,

  GSL_INTR_YDX_CP_SW_INT,
  GSL_INTR_YDX_CP_T0_PACKET_IN_IB,
  GSL_INTR_YDX_CP_OPCODE_ERROR,
  GSL_INTR_YDX_CP_PROTECTED_MODE_ERROR,
  GSL_INTR_YDX_CP_RESERVED_BIT_ERROR,
  GSL_INTR_YDX_CP_IB_ERROR,
  GSL_INTR_YDX_CP_IB2_INT,
  GSL_INTR_YDX_CP_IB1_INT,
  GSL_INTR_YDX_CP_RING_BUFFER,

  GSL_INTR_YDX_RBBM_READ_ERROR,
  GSL_INTR_YDX_RBBM_DISPLAY_UPDATE,
  GSL_INTR_YDX_RBBM_GUI_IDLE,

  GSL_INTR_YDX_SQ_PS_WATCHDOG,
  GSL_INTR_YDX_SQ_VS_WATCHDOG,

  GSL_INTR_G12_MH,
  GSL_INTR_G12_G2D,
  GSL_INTR_G12_FIFO,
#ifndef _Z180
  GSL_INTR_G12_FBC,
#endif // _Z180

  GSL_INTR_G12_MH_AXI_READ_ERROR,
  GSL_INTR_G12_MH_AXI_WRITE_ERROR,
  GSL_INTR_G12_MH_MMU_PAGE_FAULT,

  GSL_INTR_COUNT
} gsl_intrid_t;

#endif  // __GSL_TYPES_H
