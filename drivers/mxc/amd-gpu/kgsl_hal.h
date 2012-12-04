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

#ifndef __GSL_HALAPI_H
#define __GSL_HALAPI_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "kgsl_types.h"

//////////////////////////////////////////////////////////////////////////////
// linkage
//////////////////////////////////////////////////////////////////////////////
#ifdef __KGSLHAL_EXPORTS
#define KGSLHAL_API                 OS_DLLEXPORT
#else
#define KGSLHAL_API
#endif // __KGSLLIB_EXPORTS


//////////////////////////////////////////////////////////////////////////////
//  version control
//////////////////////////////////////////////////////////////////////////////
#define KGSLHAL_NAME            "AMD GSL Kernel HAL"
#define KGSLHAL_VERSION         "0.1"


//////////////////////////////////////////////////////////////////////////////
//  types
//////////////////////////////////////////////////////////////////////////////

// -------------
// device config
// -------------
struct kgsl_devconfig {

    struct kgsl_memregion  regspace;

    unsigned int     mmu_config;
    uint32_t        mpu_base;
    int              mpu_range;
    uint32_t        va_base;
    unsigned int     va_range;

    struct kgsl_memregion  gmemspace;
};

#define GSL_HAL_MEM1                        0
#define GSL_HAL_MEM2                        1
#define GSL_SHMEM_MAX_APERTURES 2

typedef struct _gsl_hal_t {
     struct kgsl_memregion z160_regspace;
     struct kgsl_memregion z430_regspace;
     struct kgsl_memregion memchunk;
     struct kgsl_memregion memspace[GSL_SHMEM_MAX_APERTURES];
     unsigned int has_z160;
     unsigned int has_z430;
} gsl_hal_t;


//////////////////////////////////////////////////////////////////////////////
//  HAL API
//////////////////////////////////////////////////////////////////////////////
KGSLHAL_API int             kgsl_hal_init(void);
KGSLHAL_API int             kgsl_hal_close(void);
KGSLHAL_API int             kgsl_hal_getdevconfig(unsigned int device_id, struct kgsl_devconfig *config);
KGSLHAL_API unsigned int    kgsl_hal_getchipid(unsigned int device_id);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  // __GSL_HALAPI_H
