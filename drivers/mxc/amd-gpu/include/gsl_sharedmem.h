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

#ifndef __GSL_SHAREDMEM_H
#define __GSL_SHAREDMEM_H


//////////////////////////////////////////////////////////////////////////////
//  defines
//////////////////////////////////////////////////////////////////////////////

#define GSL_DEVICEID_MASK                   0x0000FF00
#define GSL_EXTALLOC_MASK                   0x000F0000

#define GSL_DEVICEID_SHIFT                  8
#define GSL_EXTALLOC_SHIFT                  16

#define GSL_SHMEM_MEMARENA_GETGPUADDR(shmem) shmem.memarena->gpubaseaddr;
#define GSL_SHMEM_MEMARENA_GETHOSTADDR(shmem) shmem.memarena->hostbaseaddr;

#define GSL_APERTURE_MASK                   0x000000FF
#define GSL_APERTURE_SHIFT                  0

//////////////////////////////////////////////////////////////////////////////
//  types
//////////////////////////////////////////////////////////////////////////////

// --------------------
// shared memory object
// --------------------
typedef struct _gsl_sharedmem_t
{
    gsl_flags_t     flags;
    unsigned int    priv;
    gsl_memarena_t  *memarena;
} gsl_sharedmem_t;


//////////////////////////////////////////////////////////////////////////////
//  prototypes
//////////////////////////////////////////////////////////////////////////////
int             kgsl_sharedmem_init(gsl_sharedmem_t *shmem);
int             kgsl_sharedmem_close(gsl_sharedmem_t *shmem);
int             kgsl_sharedmem_alloc0(gsl_deviceid_t device_id, gsl_flags_t flags, int sizebytes, gsl_memdesc_t *memdesc);
int             kgsl_sharedmem_free0(gsl_memdesc_t *memdesc, unsigned int pid);
int             kgsl_sharedmem_read0(const gsl_memdesc_t *memdesc, void *dst, unsigned int offsetbytes, unsigned int sizebytes, unsigned int touserspace);
int             kgsl_sharedmem_write0(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, void *src, unsigned int sizebytes, unsigned int fromuserspace);
int             kgsl_sharedmem_set0(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes);
unsigned int    kgsl_sharedmem_convertaddr(unsigned int addr, int type);

#endif // __GSL_SHAREDMEM_H
