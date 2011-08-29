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

#ifndef __GSL_LIBAPI_H
#define __GSL_LIBAPI_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "gsl_types.h"

//  defines
#define GSLLIB_NAME            "AMD GSL User Library"
#define GSLLIB_VERSION         "0.1"


//  libary API
int                     gsl_library_open(gsl_flags_t flags);
int                     gsl_library_close(void);


//  device API
gsl_devhandle_t         gsl_device_open(gsl_deviceid_t device_id, gsl_flags_t flags);
int                     gsl_device_close(gsl_devhandle_t devhandle);
int                     gsl_device_idle(gsl_devhandle_t devhandle, unsigned int timeout);
int                     gsl_device_isidle(gsl_devhandle_t devhandle);
int                     gsl_device_getcount(void);
int                     gsl_device_getinfo(gsl_devhandle_t devhandle, gsl_devinfo_t *devinfo);
int                     gsl_device_setpowerstate(gsl_devhandle_t devhandle, gsl_flags_t flags);
int                     gsl_device_setdmistate(gsl_devhandle_t devhandle,  gsl_flags_t flags);
int                     gsl_device_waitirq(gsl_devhandle_t devhandle, gsl_intrid_t intr_id, unsigned int *count, unsigned int timeout);
int                     gsl_device_waittimestamp(gsl_devhandle_t devhandle, gsl_timestamp_t timestamp, unsigned int timeout);
int                     gsl_device_addtimestamp(gsl_devhandle_t devhandle, gsl_timestamp_t *timestamp);

//  direct register API
int                     gsl_register_read(gsl_devhandle_t devhandle, unsigned int offsetwords, unsigned int *data);


//  command API
int                     gsl_cp_issueibcommands(gsl_devhandle_t devhandle, gsl_ctxthandle_t ctxthandle, gpuaddr_t ibaddr, unsigned int sizewords, gsl_timestamp_t *timestamp, gsl_flags_t flags);
gsl_timestamp_t         gsl_cp_readtimestamp(gsl_devhandle_t devhandle, gsl_timestamp_type_t type);
int                     gsl_cp_checktimestamp(gsl_devhandle_t devhandle, gsl_timestamp_t timestamp, gsl_timestamp_type_t type);
int                     gsl_cp_freememontimestamp(gsl_devhandle_t devhandle, gsl_memdesc_t *memdesc, gsl_timestamp_t timestamp, gsl_timestamp_type_t type);
int                     gsl_v3_issuecommand(gsl_devhandle_t devhandle, gsl_cmdwindow_t target, unsigned int addr, unsigned int data);

//  context API
gsl_ctxthandle_t        gsl_context_create(gsl_devhandle_t devhandle, gsl_context_type_t type, gsl_flags_t flags);
int                     gsl_context_destroy(gsl_devhandle_t devhandle, gsl_ctxthandle_t ctxthandle);
int                     gsl_context_bind_gmem_shadow(gsl_devhandle_t devhandle, gsl_ctxthandle_t ctxthandle, const gsl_rect_t* gmem_rect, unsigned int shadow_x, unsigned int shadow_y, const gsl_buffer_desc_t* shadow_buffer, unsigned int buffer_id);


//  sharedmem API
int                     gsl_memory_alloc(gsl_deviceid_t device_id, unsigned int sizebytes, gsl_flags_t flags, gsl_memdesc_t *memdesc);
int                     gsl_memory_free(gsl_memdesc_t *memdesc);
int                     gsl_memory_read(const gsl_memdesc_t *memdesc, void *dst, unsigned int sizebytes, unsigned int offsetbytes);
int                     gsl_memory_write(const gsl_memdesc_t *memdesc, void *src, unsigned int sizebytes, unsigned int offsetbytes);
int                     gsl_memory_write_multiple(const gsl_memdesc_t *memdesc, void *src, unsigned int srcstridebytes, unsigned int dststridebytes, unsigned int blocksizebytes, unsigned int numblocks, unsigned int offsetbytes);
unsigned int            gsl_memory_getlargestfreeblock(gsl_deviceid_t device_id, gsl_flags_t flags);
int                     gsl_memory_set(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes);
int                     gsl_memory_cacheoperation(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int sizebytes, unsigned int operation);
int                     gsl_memory_fromhostpointer(gsl_deviceid_t device_id, gsl_memdesc_t *memdesc, void* hostptr);

#ifdef _DIRECT_MAPPED
unsigned int            gsl_sharedmem_gethostaddr(const gsl_memdesc_t *memdesc);
#endif // _DIRECT_MAPPED

//  address translation API
int                     gsl_translate_physaddr(void* virtAddr, unsigned int* physAddr);

//  TB dump API
int                     gsl_tbdump_waitirq();
int                     gsl_tbdump_exportbmp(const void* addr, unsigned int format, unsigned int stride, unsigned int width, unsigned int height);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  // __GSL_LIBAPI_H
