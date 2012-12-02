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

#ifndef __GSL_PROPERTIES_H
#define __GSL_PROPERTIES_H

#include "kgsl_types.h"

/* move me and my friends to include/linux/mxc_kgsl.h */
enum kgsl_property_type
{
	KGSL_PROP_DEVICE_INFO      = 0x00000001,
	KGSL_PROP_DEVICE_SHADOW    = 0x00000002,
	KGSL_PROP_DEVICE_POWER     = 0x00000003,
	KGSL_PROP_SHMEM            = 0x00000004,
	KGSL_PROP_SHMEM_APERTURES  = 0x00000005,
	KGSL_PROP_DEVICE_DMI       = 0x00000006  /* qcom: MMU_ENABLE */
};

/* qcom: kgsl_devinfo is the structure returned by PROP_DEVICE_INFO, qcom code ignores anything other than 0x1 or 0x2 */

/* NQ */
struct kgsl_apertureprop {
    unsigned int  gpuaddr;
    unsigned int  hostaddr;
};

/* NQ */
struct kgsl_shmemprop {
	int numapertures;
	unsigned int aperture_mask;
	unsigned int aperture_shift;
	struct kgsl_apertureprop *aperture;
};

struct kgsl_shadowprop {
	unsigned int hostaddr; /* qcom: called gpuaddr */
	unsigned int size;
	unsigned int flags; /* contains KGSL_FLAGS_ values */
};

/* NQ */
struct kgsl_powerprop {
	unsigned int value;
	unsigned int flags;
};

/* NQ */
struct kgsl_dmiprop {
	unsigned int value;
	unsigned int flags;
};

#endif  // __GSL_PROPERTIES_H
