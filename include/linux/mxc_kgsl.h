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

#ifndef _MXC_KGSL_H
#define _MXC_KGSL_H

/* for porting to mxc_kgsl.h include:
 * flag values like KGSL_FLAGS_INITIALIZED and context flags
 *  kgsl_platform_data?
 *  kgsl_cmdwindow_type?
 */

#define KGSL_FLAGS_NORMALMODE            0x00000000
#define KGSL_FLAGS_SAFEMODE              0x00000001
#define KGSL_FLAGS_INITIALIZED0          0x00000002
#define KGSL_FLAGS_INITIALIZED           0x00000004
#define KGSL_FLAGS_STARTED               0x00000008
#define KGSL_FLAGS_ACTIVE                0x00000010
#define KGSL_FLAGS_RESERVED0             0x00000020
#define KGSL_FLAGS_RESERVED1             0x00000040
#define KGSL_FLAGS_RESERVED2             0x00000080

/* context flags */
#define KGSL_CONTEXT_MAX             20
#define KGSL_CONTEXT_NONE            0
#define KGSL_CONTEXT_SAVE_GMEM       1
#define KGSL_CONTEXT_NO_GMEM_ALLOC   2

/* device id */
enum kgsl_deviceid
{
	KGSL_DEVICE_ANY    = 0x00000000,
	KGSL_DEVICE_YAMATO = 0x00000001,
	KGSL_DEVICE_G12    = 0x00000002,
	KGSL_DEVICE_MAX    = 0x00000002
};

struct kgsl_devinfo {
	unsigned int device_id;
	/* chip revision id
	* coreid:8 majorrev:8 minorrev:8 patch:8
	*/
	unsigned int chip_id;
	unsigned int mmu_enabled;        // mmu address translation enabled
	unsigned int gmem_gpubaseaddr;
	/* if gmem_hostbaseaddr is NULL, we would know its not mapped into
	 * mmio space */
	void *gmem_hostbaseaddr;
	unsigned int gmem_sizebytes;
	/* mx50 z160 has higher gradient/texture precision */
	unsigned int high_precision;
};

/* this structure defines the region of memory that can be mmap()ed from this
   driver. The timestamp fields are volatile because they are written by the
   GPU
*/
struct kgsl_devmemstore {
	volatile unsigned int  soptimestamp;
	unsigned int           sbz;
	volatile unsigned int  eoptimestamp;
	unsigned int           sbz2;
};

#define KGSL_DEVICE_MEMSTORE_OFFSET(field) \
	offsetof(struct kgsl_devmemstore, field)

/* timestamp id*/
enum kgsl_timestamp_type {
	KGSL_TIMESTAMP_CONSUMED = 0x00000001, /* start-of-pipeline timestamp */
	KGSL_TIMESTAMP_RETIRED  = 0x00000002, /* end-of-pipeline timestamp */
	KGSL_TIMESTAMP_MAX      = 0x00000002,
	KGSL_TIMESTAMP_FOOBAR   = 0x7FFFFFFF
};

#define KGSL_TIMESTAMP_EPSILON	20000
enum kgsl_property_type
{
	KGSL_PROP_DEVICE_INFO      = 0x00000001,
	KGSL_PROP_DEVICE_SHADOW    = 0x00000002,
	KGSL_PROP_DEVICE_POWER     = 0x00000003,
	KGSL_PROP_SHMEM            = 0x00000004,
	KGSL_PROP_SHMEM_APERTURES  = 0x00000005,
	KGSL_PROP_DEVICE_DMI       = 0x00000006  /* qcom: MMU_ENABLE */
};

/* qcom: not used, passes pointers directly due to simpler memory manager */
struct kgsl_memdesc {
	void *hostptr;
	uint32_t gpuaddr;
	int size;
	unsigned int priv;
	unsigned int unused;
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


/*
 * please check of NQ items are even called from FSL userspace
 */

/* ioctls - note qcom driver is 0x09 */
#define KGSL_IOC_TYPE 0xF9

/* NQ */
struct kgsl_device_start {
	unsigned int device_id;
	unsigned int flags;
};

#define IOCTL_KGSL_DEVICE_START \
	_IOW(KGSL_IOC_TYPE, 0x20, struct kgsl_device_start)

/* NQ */
struct kgsl_device_stop {
	unsigned int device_id;
};

#define IOCTL_KGSL_DEVICE_STOP \
	_IOW(KGSL_IOC_TYPE, 0x21, struct kgsl_device_stop)

/* NQ */
struct kgsl_device_idle {
	unsigned int device_id;
	unsigned int timeout;
};

#define IOCTL_KGSL_DEVICE_IDLE \
	_IOW(KGSL_IOC_TYPE, 0x22, struct kgsl_device_idle)

/* NQ */
struct kgsl_device_isidle {
	unsigned int device_id;
};

#define IOCTL_KGSL_DEVICE_ISIDLE \
	 _IOR(KGSL_IOC_TYPE, 0x23, struct kgsl_device_isidle)


/*
 * get misc info about the GPU
 * type should be a value from enum kgsl_property_type
 * value points to a structure that varies based on type
 * sizebytes is sizeof() that structure
 * for KGSL_PROP_DEVICE_INFO, use struct kgsl_devinfo
 * this structure contaings hardware versioning info.
 * for KGSL_PROP_DEVICE_SHADOW, use struct kgsl_shadowprop
 * this is used to find mmap() offset and sizes for mapping
 * struct kgsl_memstore into userspace.
 *
 * qcom: unsigned int type, void *value, IOCNR=0x2
 * kgsl_property_type defined right here
 */
struct kgsl_device_getproperty {
	unsigned int device_id;
	enum kgsl_property_type type;
	unsigned int *value;
	unsigned int sizebytes;
};

#define IOCTL_KGSL_DEVICE_GETPROPERTY \
	_IOWR(KGSL_IOC_TYPE, 0x24, struct kgsl_device_getproperty)

struct kgsl_device_setproperty {
	unsigned int  device_id;
	enum kgsl_property_type type;
	void *value;
	unsigned int sizebytes;
};

#define IOCTL_KGSL_DEVICE_SETPROPERTY \
	_IOW(KGSL_IOC_TYPE, 0x25, struct kgsl_device_setproperty)

/*
 * read a GPU register
 * offsetwords is the 32-bit word offset from the beginning of
 * the GPU register space.
 *
 * qcom: unsigned int value, IOCNR=0x3
 */
struct kgsl_device_regread {
	unsigned int device_id;
	unsigned int offsetwords;
	unsigned int *value;
};

#define IOCTL_KGSL_DEVICE_REGREAD \
	_IOWR(KGSL_IOC_TYPE, 0x26, struct kgsl_device_regread)

struct kgsl_device_regwrite {
	unsigned int device_id;
	unsigned int offsetwords;
	unsigned int value;
};

#define IOCTL_KGSL_DEVICE_REGWRITE \
	_IOW(KGSL_IOC_TYPE, 0x27, struct kgsl_device_regwrite)

/*
 * qcom: kgsl_ringbuffer_issueibcmds IOCNR=0x10
 * all unsigned int (including timestamp!?)
 */
struct kgsl_cmdstream_issueibcmds {
	unsigned int device_id;
	int drawctxt_index;
	uint32_t ibaddr;
	int sizedwords;
	unsigned int *timestamp; /* output param */
	unsigned int flags;
};

#define IOCTL_KGSL_CMDSTREAM_ISSUEIBCMDS \
	_IOWR(KGSL_IOC_TYPE, 0x29, struct kgsl_cmdstream_issueibcmds)

/*
 * read the most recently executed timestamp value
 * type should be a value from enum kgsl_timestamp_type
 * qcom: type is unsigned int despite comment. IOCNR=0x11, _IOR
 */
struct kgsl_cmdstream_readtimestamp {
	unsigned int device_id;
	enum kgsl_timestamp_type type;
	unsigned int *timestamp;
};

#define IOCTL_KGSL_CMDSTREAM_READTIMESTAMP \
	_IOWR(KGSL_IOC_TYPE, 0x2A, struct kgsl_cmdstream_readtimestamp)

/*
 * free memory when the GPU reaches a given timestamp.
 * gpuaddr specify a memory region created by a
 * IOCTL_KGSL_SHAREDMEM_FROM_PMEM call
 * type should be a value from enum kgsl_timestamp_type
 *
 * qcom: obviously PMEM call, passes gpuaddr instead of memdesc
 * IOCNR=0x12, _IOR (I think this is corrected in a future qcom)
 */
struct kgsl_cmdstream_freememontimestamp {
	unsigned int device_id;
	struct kgsl_memdesc *memdesc;
	unsigned int timestamp;
	enum kgsl_timestamp_type type;
};

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP \
	_IOW(KGSL_IOC_TYPE, 0x2B, struct kgsl_cmdstream_freememontimestamp)

/*
 * block until the GPU has executed past a given timestamp
 * timeout is in milliseconds
 * qcom: IOCNR=0x6, _IOWR
 */
struct kgsl_cmdstream_waittimestamp {
	unsigned int device_id;
	unsigned int timestamp;
	unsigned int timeout;
};

#define IOCTL_KGSL_CMDSTREAM_WAITTIMESTAMP \
	_IOW(KGSL_IOC_TYPE, 0x2C, struct kgsl_cmdstream_waittimestamp)


enum kgsl_cmdwindow_type {
    GSL_CMDWINDOW_MIN     = 0x00000000,
    GSL_CMDWINDOW_2D      = 0x00000000,
    GSL_CMDWINDOW_3D      = 0x00000001,     /* legacy */
    GSL_CMDWINDOW_MMU     = 0x00000002,
    GSL_CMDWINDOW_ARBITER = 0x000000FF,
    GSL_CMDWINDOW_MAX     = 0x000000FF,

    GSL_CMDWINDOW_FOOBAR  = 0x7FFFFFFF,
};

/*
 * write to the commend window
 * qcom: IOCNR=0x2e, kgsl_cmdwindow_type defined just here
 */
struct kgsl_cmdwindow_write {
	unsigned int  device_id;
	enum kgsl_cmdwindow_type target;
	unsigned int addr;
	unsigned int data;
};

#define IOCTL_KGSL_CMDWINDOW_WRITE \
	_IOW(KGSL_IOC_TYPE, 0x2D, struct kgsl_cmdwindow_write)

/*
 * create a draw context, which is used to preserve GPU state.
 * The flags field may contain a mask KGSL_CONTEXT_*  values
 *
 * qcom: kgsl_drawctxt_create, no type field, drawctxt_id is unsigned int IOCNR=0x13
 */
struct kgsl_context_create {
	unsigned int device_id;
	unsigned int type;
	unsigned int *drawctxt_id;
	unsigned int flags;
};

#define IOCTL_KGSL_CONTEXT_CREATE \
	_IOWR(KGSL_IOC_TYPE, 0x2E, struct kgsl_context_create)

/* destroy a draw context qcom: 0x14*/
struct kgsl_context_destroy {
	unsigned int device_id;
	unsigned int drawctxt_id;
};

#define IOCTL_KGSL_CONTEXT_DESTROY \
	_IOW(KGSL_IOC_TYPE, 0x2F, struct kgsl_context_destroy)


struct kgsl_gmem_desc {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
};

struct kgsl_buffer_desc {
	/* qcom: hostptr, gpuaddr, size, format, pitch, enabled */
	struct kgsl_memdesc data;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int format;
	unsigned int enabled;
};

/* qcom: kgsl_bind_gmem_shadow, no device_id, struct is not a pointer, IOCNR=0x22 */
struct kgsl_drawctxt_bind_gmem_shadow {
	unsigned int device_id;
	unsigned int drawctxt_id;
	const struct kgsl_gmem_desc* gmem_rect;
	unsigned int shadow_x;
	unsigned int shadow_y;
	const struct kgsl_buffer_desc* shadow_buffer;
	unsigned int buffer_id;
};

#define IOCTL_KGSL_DRAWCTXT_BIND_GMEM_SHADOW \
	_IOW(KGSL_IOC_TYPE, 0x30, struct kgsl_drawctxt_bind_gmem_shadow)

/* qualcomm's has a different memory allocation strategy but basically doesn't
 * tend to use memdescs
 * IOCNRs: 0x20 FROM_PMEM, 0x23 FROM_VMALLOC
 */
struct kgsl_sharedmem_alloc {
	unsigned int device_id;
	unsigned int flags;
	int sizebytes;
	struct kgsl_memdesc *memdesc;
};

#define IOCTL_KGSL_SHAREDMEM_ALLOC  _IOWR(KGSL_IOC_TYPE, 0x31, struct kgsl_sharedmem_alloc)

/* qcom: pass gpuaddr, IOCNR=0x21 */
struct kgsl_sharedmem_free {
	struct kgsl_memdesc *memdesc;
};

#define IOCTL_KGSL_SHAREDMEM_FREE \
	_IOW(KGSL_IOC_TYPE, 0x32, struct kgsl_sharedmem_free)

/* NQ */
struct kgsl_sharedmem_read {
	const struct kgsl_memdesc *memdesc;
	unsigned int *dst;
	unsigned int offsetbytes;
	unsigned int sizebytes;
};

#define IOCTL_KGSL_SHAREDMEM_READ \
	_IOWR(KGSL_IOC_TYPE, 0x33, struct kgsl_sharedmem_read)

/* NQ */
struct kgsl_sharedmem_write {
	const struct kgsl_memdesc *memdesc;
	unsigned int offsetbytes;
	unsigned int *src;
	unsigned int sizebytes;
};

#define IOCTL_KGSL_SHAREDMEM_WRITE \
	_IOW(KGSL_IOC_TYPE, 0x34, struct kgsl_sharedmem_write)

/* NQ */
struct kgsl_sharedmem_set {
	const struct kgsl_memdesc *memdesc;
	unsigned int offsetbytes;
	unsigned int value;
	unsigned int sizebytes;
};

#define IOCTL_KGSL_SHAREDMEM_SET \
	_IOW(KGSL_IOC_TYPE, 0x35, struct kgsl_sharedmem_set)

/* NQ */
struct kgsl_sharedmem_largestfreeblock {
	unsigned int device_id;
	unsigned int flags;
	unsigned int *largestfreeblock;
};

#define IOCTL_KGSL_SHAREDMEM_LARGESTFREEBLOCK \
	_IOWR(KGSL_IOC_TYPE, 0x36, struct kgsl_sharedmem_largestfreeblock)

/* NQ although they have FLUSH_CACHE (0x24) which takes a gpuaddr */
struct kgsl_sharedmem_cacheoperation {
	const struct kgsl_memdesc *memdesc;
	unsigned int offsetbytes;
	unsigned int sizebytes;
	unsigned int operation;
};

#define IOCTL_KGSL_SHAREDMEM_CACHEOPERATION \
	_IOW(KGSL_IOC_TYPE, 0x37, struct kgsl_sharedmem_cacheoperation)

/* NQ */
struct kgsl_sharedmem_fromhostpointer {
	unsigned int device_id;
	struct kgsl_memdesc *memdesc;
	void *hostptr;
};

#define IOCTL_KGSL_SHAREDMEM_FROMHOSTPOINTER \
	_IOW(KGSL_IOC_TYPE, 0x38, struct kgsl_sharedmem_fromhostpointer)

/* NQ */
//struct kgsl_add_timestamp {
//	unsigned int device_id;
//	unsigned int *timestamp;
//};
//
//#define IOCTL_KGSL_ADD_TIMESTAMP \
//	_IOWR(KGSL_IOC_TYPE, 0x39, struct kgsl_add_timestamp)

/* NQ */
#define IOCTL_KGSL_DRIVER_EXIT \
	_IOWR(KGSL_IOC_TYPE, 0x3A, NULL)

/* NQ */
struct kgsl_device_clock {
	unsigned int device;
	int enable;
};

#define IOCTL_KGSL_DEVICE_CLOCK \
	_IOWR(KGSL_IOC_TYPE, 0x60, struct kgsl_device_clock)

#endif /* _MXC_KGSL_H */
