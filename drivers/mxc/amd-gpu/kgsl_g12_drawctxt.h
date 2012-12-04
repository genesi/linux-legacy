/* Copyright (c) 2002,2007-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __GSL_DRAWCTXT_G12_H
#define __GSL_DRAWCTXT_G12_H

#include "kgsl_sharedmem.h"

struct kgsl_device;

#define GSL_HAL_NUMCMDBUFFERS       5
#define GSL_HAL_CMDBUFFERSIZE       ((1024 + 13) * sizeof(unsigned int))

#define ALIGN_IN_BYTES(dim, alignment) (((dim) + (alignment - 1)) & \
		~(alignment - 1))



#define PACKETSIZE_BEGIN        3
#define PACKETSIZE_G2DCOLOR     2
#define PACKETSIZE_TEXUNIT      (TEXUNITREGCOUNT * 2)
#define PACKETSIZE_REG          (VG_REGCOUNT * 2)
#define PACKETSIZE_STATE        (PACKETSIZE_TEXUNIT * NUMTEXUNITS + \
				 PACKETSIZE_REG + PACKETSIZE_BEGIN + \
				 PACKETSIZE_G2DCOLOR)
#define PACKETSIZE_STATESTREAM  (ALIGN_IN_BYTES((PACKETSIZE_STATE * \
				 sizeof(unsigned int)), 32) / \
				 sizeof(unsigned int))


#define KGSL_G12_PACKET_SIZE		15
#define KGSL_G12_MARKER_SIZE		10
#define KGSL_G12_CALL_CMD		0x1000
#define KGSL_G12_MARKER_CMD		0x8000
#define KGSL_G12_STREAM_END_CMD		0x9000
#define KGSL_G12_STREAM_PACKET		0x7C000176
#define KGSL_G12_STREAM_PACKET_CALL	0x7C000275

struct kgsl_g12_hal_z1xxdrawctx_t {
	unsigned int id;
};

struct kgsl_g12_z1xx {
	unsigned int offs;
	unsigned int curr;
	unsigned int prevctx;

	struct kgsl_memdesc e0;
	struct kgsl_memdesc e1;
#ifdef _Z180
	struct kgsl_memdesc e2;
#endif
	unsigned int            *cmdbuf[GSL_HAL_NUMCMDBUFFERS];
	struct kgsl_memdesc      cmdbufdesc[GSL_HAL_NUMCMDBUFFERS];
	unsigned int timestamp[GSL_HAL_NUMCMDBUFFERS];

	unsigned int numcontext;
	unsigned int nextUniqueContextID;
};

extern struct kgsl_g12_z1xx g_z1xx;
extern int z160_version;

int
kgsl_g12_drawctxt_create(struct kgsl_device *device,
			unsigned int type,
			unsigned int *drawctxt_id,
			unsigned int flags);

int
kgsl_g12_drawctxt_destroy(struct kgsl_device *device,
			unsigned int drawctxt_id);


#ifdef _Z180
#define NUMTEXUNITS                         4
#define TEXUNITREGCOUNT                     25
#define VG_REGCOUNT                         0x39
#define GSL_HAL_EDGE0BUFSIZE                0x3E8 + 64
#define GSL_HAL_EDGE1BUFSIZE                0x8000 + 64
#define GSL_HAL_EDGE2BUFSIZE                0x80020 + 64
#define GSL_HAL_EDGE0REG                    ADDR_VGV1_CBUF
#define GSL_HAL_EDGE1REG                    ADDR_VGV1_BBUF
#define GSL_HAL_EDGE2REG                    ADDR_VGV1_EBUF
#else
#define NUMTEXUNITS             2
#define TEXUNITREGCOUNT         24
#define	VG_REGCOUNT             0x3A
#define L1TILESIZE                           64
#define GSL_HAL_EDGE0BUFSIZE                 L1TILESIZE * L1TILESIZE * 4 + 64
#define GSL_HAL_EDGE1BUFSIZE                 L1TILESIZE * L1TILESIZE * 16 + 64
#define GSL_HAL_EDGE0REG                     ADDR_VGV1_CBASE1
#define GSL_HAL_EDGE1REG                     ADDR_VGV1_UBASE2
#endif

/* should be in kgsl_g12_drawctxt.h? */
int kgsl_g12_issueibcmds(struct kgsl_device* device, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags);
int kgsl_g12_drawctxt_create(struct kgsl_device* device, unsigned int type, unsigned int *drawctxt_id, unsigned int flags);
int kgsl_g12_drawctxt_destroy(struct kgsl_device* device, unsigned int drawctxt_id);



#endif  /* __GSL_DRAWCTXT_H */
