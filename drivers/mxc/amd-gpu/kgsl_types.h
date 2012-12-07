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


//////////////////////////////////////////////////////////////////////////////
// Operating System values
//////////////////////////////////////////////////////////////////////////////
#define OS_SUCCESS                       0
#define OS_FAILURE                      -1
#define OS_FAILURE_SYSTEMERROR          -2
#define OS_FAILURE_DEVICEERROR          -3
#define OS_FAILURE_OUTOFMEM             -4
#define OS_FAILURE_BADPARAM             -5
#define OS_FAILURE_NOTSUPPORTED         -6
#define OS_FAILURE_NOMOREAVAILABLE      -7
#define OS_FAILURE_NOTINITIALIZED       -8
#define OS_FAILURE_ALREADYINITIALIZED   -9
#define OS_FAILURE_TIMEOUT              -10


#define OS_INFINITE             0xFFFFFFFF
#define OS_TLS_OUTOFINDEXES     0xFFFFFFFF
#define OS_TRUE                         1
#define OS_FALSE                        0



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
// status
//////////////////////////////////////////////////////////////////////////////
#define GSL_SUCCESS                     OS_SUCCESS
#define GSL_FAILURE                     OS_FAILURE
#define GSL_FAILURE_SYSTEMERROR         OS_FAILURE_SYSTEMERROR
#define GSL_FAILURE_DEVICEERROR         OS_FAILURE_DEVICEERROR
#define GSL_FAILURE_OUTOFMEM            OS_FAILURE_OUTOFMEM
#define GSL_FAILURE_BADPARAM            OS_FAILURE_BADPARAM
#define GSL_FAILURE_OFFSETINVALID       OS_FAILURE_OFFSETINVALID
#define GSL_FAILURE_NOTSUPPORTED        OS_FAILURE_NOTSUPPORTED
#define GSL_FAILURE_NOMOREAVAILABLE     OS_FAILURE_NOMOREAVAILABLE
#define GSL_FAILURE_NOTINITIALIZED      OS_FAILURE_NOTINITIALIZED
#define GSL_FAILURE_ALREADYINITIALIZED  OS_FAILURE_ALREADYINITIALIZED
#define GSL_FAILURE_TIMEOUT             OS_FAILURE_TIMEOUT


//////////////////////////////////////////////////////////////////////////////
// memory allocation flags
//////////////////////////////////////////////////////////////////////////////
#define GSL_MEMFLAGS_ANY                0x00000000      // dont care

#define GSL_MEMFLAGS_CHANNELANY         0x00000000
#define GSL_MEMFLAGS_CHANNEL1           0x00000000
#define GSL_MEMFLAGS_CHANNEL2           0x00000001
#define GSL_MEMFLAGS_CHANNEL3           0x00000002
#define GSL_MEMFLAGS_CHANNEL4           0x00000003

#define GSL_MEMFLAGS_BANKANY            0x00000000
#define GSL_MEMFLAGS_BANK1              0x00000010
#define GSL_MEMFLAGS_BANK2              0x00000020
#define GSL_MEMFLAGS_BANK3              0x00000040
#define GSL_MEMFLAGS_BANK4              0x00000080

#define GSL_MEMFLAGS_DIRANY             0x00000000
#define GSL_MEMFLAGS_DIRTOP             0x00000100
#define GSL_MEMFLAGS_DIRBOT             0x00000200

#define GSL_MEMFLAGS_APERTUREANY        0x00000000
#define GSL_MEMFLAGS_EMEM               0x00000000
#define GSL_MEMFLAGS_CONPHYS            0x00001000

#define GSL_MEMFLAGS_ALIGNANY           0x00000000      // minimum alignment is 32 bytes
#define GSL_MEMFLAGS_ALIGN32            0x00000000
#define GSL_MEMFLAGS_ALIGN64            0x00060000
#define GSL_MEMFLAGS_ALIGN128           0x00070000
#define GSL_MEMFLAGS_ALIGN256           0x00080000
#define GSL_MEMFLAGS_ALIGN512           0x00090000
#define GSL_MEMFLAGS_ALIGN1K            0x000A0000
#define GSL_MEMFLAGS_ALIGN2K            0x000B0000
#define GSL_MEMFLAGS_ALIGN4K            0x000C0000
#define GSL_MEMFLAGS_ALIGN8K            0x000D0000
#define GSL_MEMFLAGS_ALIGN16K           0x000E0000
#define GSL_MEMFLAGS_ALIGN32K           0x000F0000
#define GSL_MEMFLAGS_ALIGN64K           0x00100000
#define GSL_MEMFLAGS_ALIGNPAGE          GSL_MEMFLAGS_ALIGN4K

#define GSL_MEMFLAGS_GPUREADWRITE       0x00000000
#define GSL_MEMFLAGS_GPUREADONLY        0x01000000
#define GSL_MEMFLAGS_GPUWRITEONLY       0x02000000
#define GSL_MEMFLAGS_GPUNOACCESS        0x04000000

#define GSL_MEMFLAGS_FORCEPAGESIZE      0x40000000	// round allocation up to page size
#define GSL_MEMFLAGS_STRICTREQUEST      0x80000000      // fail the alloc if the flags cannot be honored

#define GSL_MEMFLAGS_CHANNEL_MASK       0x0000000F
#define GSL_MEMFLAGS_BANK_MASK          0x000000F0
#define GSL_MEMFLAGS_DIR_MASK           0x00000F00
#define GSL_MEMFLAGS_APERTURE_MASK      0x0000F000
#define GSL_MEMFLAGS_ALIGN_MASK         0x00FF0000
#define GSL_MEMFLAGS_GPUAP_MASK         0x0F000000

#define GSL_MEMFLAGS_CHANNEL_SHIFT      0
#define GSL_MEMFLAGS_BANK_SHIFT         4
#define GSL_MEMFLAGS_DIR_SHIFT          8
#define GSL_MEMFLAGS_APERTURE_SHIFT     12
#define GSL_MEMFLAGS_ALIGN_SHIFT        16
#define GSL_MEMFLAGS_GPUAP_SHIFT        24


//////////////////////////////////////////////////////////////////////////////
// debug flags
//////////////////////////////////////////////////////////////////////////////
#define GSL_DBGFLAGS_ALL                0xFFFFFFFF
#define GSL_DBGFLAGS_DEVICE             0x00000001
#define GSL_DBGFLAGS_CTXT               0x00000002
#define GSL_DBGFLAGS_MEMMGR             0x00000004
#define GSL_DBGFLAGS_MMU                0x00000008
#define GSL_DBGFLAGS_POWER              0x00000010
#define GSL_DBGFLAGS_IRQ                0x00000020
#define GSL_DBGFLAGS_BIST               0x00000040
#define GSL_DBGFLAGS_PM4                0x00000080
#define GSL_DBGFLAGS_PM4MEM             0x00000100
#define GSL_DBGFLAGS_PM4CHECK           0x00000200
#define GSL_DBGFLAGS_IFH                0x00001000
#define GSL_DBGFLAGS_NULL               0x00002000


#define GSL_PWRFLAGS_POWER_OFF          0x00000001
#define GSL_PWRFLAGS_POWER_ON           0x00000002
#define GSL_PWRFLAGS_CLK_ON             0x00000004
#define GSL_PWRFLAGS_CLK_OFF            0x00000008
#define GSL_PWRFLAGS_OVERRIDE_ON        0x00000010
#define GSL_PWRFLAGS_OVERRIDE_OFF       0x00000020

#define GSL_CACHEFLAGS_CLEAN            0x00000001  /* flush cache          */
#define GSL_CACHEFLAGS_INVALIDATE       0x00000002  /* invalidate cache     */
#define GSL_CACHEFLAGS_WRITECLEAN       0x00000004  /* flush write cache    */

#define GSL_TIMEOUT_NONE                        0
#define GSL_TIMEOUT_DEFAULT                     0xFFFFFFFF


typedef unsigned int        uint32_t;

// ----------------
// chip revision id
// ----------------
//
// coreid:8 majorrev:8 minorrev:8 patch:8
//
// coreid = 0x00 = YAMATO_DX
// coreid = 0x80 = G12
//

#define COREID(x)   ((((unsigned int)x & 0xFF) << 24))
#define MAJORID(x)  ((((unsigned int)x & 0xFF) << 16))
#define MINORID(x)  ((((unsigned int)x & 0xFF) <<  8))
#define PATCHID(x)  ((((unsigned int)x & 0xFF) <<  0))

typedef enum _gsl_chipid_t
{
    GSL_CHIPID_YAMATODX_REV13   = (COREID(0x00) | MAJORID(0x01) | MINORID(0x03) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV14   = (COREID(0x00) | MAJORID(0x01) | MINORID(0x04) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV20   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x00) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV21   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x01) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV211  = (COREID(0x00) | MAJORID(0x02) | MINORID(0x01) | PATCHID(0x01)),
    GSL_CHIPID_YAMATODX_REV22   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x02) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV23   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x03) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV231  = (COREID(0x00) | MAJORID(0x02) | MINORID(0x03) | PATCHID(0x01)),
    GSL_CHIPID_YAMATODX_REV24   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x04) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV25   = (COREID(0x00) | MAJORID(0x02) | MINORID(0x05) | PATCHID(0x00)),
    GSL_CHIPID_YAMATODX_REV251  = (COREID(0x00) | MAJORID(0x02) | MINORID(0x05) | PATCHID(0x01)),
    GSL_CHIPID_G12_REV00        = (int)(COREID(0x80) | MAJORID(0x00) | MINORID(0x00) | PATCHID(0x00)),
    GSL_CHIPID_ERROR            = (int)0xFFFFFFFF

} gsl_chipid_t;

#undef COREID
#undef MAJORID
#undef MINORID
#undef PATCHID

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

/* perhaps these should be in kgsl_sharedmem.h */
struct kgsl_memregion {
    unsigned char  *mmio_virt_base;
    unsigned int   mmio_phys_base;
    uint32_t      gpu_base;
    unsigned int   sizebytes;
};

typedef struct _gsl_scatterlist_t {
    int           contiguous;       // flag whether pages on the list are physically contiguous
    unsigned int  num;
    unsigned int  *pages;
} gsl_scatterlist_t;

typedef struct _gsl_memnode_t {
    unsigned int       timestamp;
    struct kgsl_memdesc         memdesc;
    struct _gsl_memnode_t *next;
} gsl_memnode_t;

typedef struct _gsl_memqueue_t {
    gsl_memnode_t   *head;
    gsl_memnode_t   *tail;
} gsl_memqueue_t;

#define DEBUG_ASSERT(x)

#endif  // __GSL_TYPES_H
