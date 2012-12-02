/* Copyright (c) 2002,2007-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>

#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_cmdstream.h"
#include "kgsl_sharedmem.h"
#include "kgsl_driver.h"
#include "kgsl_ioctl.h"
#include "kgsl_g12_drawctxt.h"

#include "g12_reg.h"
#include "kgsl_g12_vgv3types.h"

#ifdef CONFIG_ARCH_MX35
#define V3_SYNC
#endif

static void beginpacket(struct kgsl_g12_z1xx* z1xx, uint32_t cmd, unsigned int nextcount)
{
	unsigned int *p = z1xx->cmdbuf[z1xx->curr];

	p[z1xx->offs++] = KGSL_G12_STREAM_PACKET;
	p[z1xx->offs++] = 5;
	p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
	p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
	p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
	p[z1xx->offs++] = KGSL_G12_STREAM_PACKET_CALL;
	p[z1xx->offs++] = cmd;
	p[z1xx->offs++] = KGSL_G12_CALL_CMD | nextcount;
	p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
	p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
}


int kgsl_g12_issueibcmds(struct kgsl_device* device, int drawctxt_index, uint32_t ibaddr,
			 int sizedwords, unsigned int *timestamp, unsigned int flags)
{
	unsigned int ofs = PACKETSIZE_STATESTREAM * sizeof(unsigned int);
	unsigned int cnt = 5;
	unsigned int cmd = ibaddr;
	unsigned int nextbuf  = (g_z1xx.curr+1) % GSL_HAL_NUMCMDBUFFERS;
	unsigned int nextaddr = g_z1xx.cmdbufdesc[nextbuf].gpuaddr;
	unsigned int nextcnt  = KGSL_G12_STREAM_END_CMD | 5;
	struct kgsl_memdesc tmp = {0};
	unsigned int processed_timestamp;

	(void) flags;

	/* read what is the latest timestamp device have processed */
	/* note: diverge from qcom */
	KGSL_CMDSTREAM_GET_EOP_TIMESTAMP(device, (int *)&processed_timestamp);

	/* wait for the next buffer's timestamp to occur - note: diverge */
	while(processed_timestamp < g_z1xx.timestamp[nextbuf]) {
		kgsl_cmdstream_waittimestamp(device->id, g_z1xx.timestamp[nextbuf], 1000);
		KGSL_CMDSTREAM_GET_EOP_TIMESTAMP(device, (int *)&processed_timestamp);
	}

	// QCOM
	// tmp.hostptr = (void *) *timestamp;
	// device->current_timestamp++;
	// *timestamp = device->current_timestamp;

	*timestamp = g_z1xx.timestamp[nextbuf] = device->current_timestamp + 1;

	/* context switch */
	if (drawctxt_index != (int) g_z1xx.prevctx) {
		cnt = PACKETSIZE_STATESTREAM;
		ofs = 0;
	}

	g_z1xx.prevctx = drawctxt_index;

	g_z1xx.offs = 10;
	beginpacket(&g_z1xx, cmd + ofs, cnt);

	// tmp.hostptr = (void*)(tmp.hostptr + (sizedwords * sizeof(unsigned int)));
	// tmp.size = 12;
	tmp.gpuaddr = ibaddr + (sizedwords * sizeof(unsigned int));

	kgsl_sharedmem_write0(&tmp, 4, &nextaddr, 4, false);
	kgsl_sharedmem_write0(&tmp, 8, &nextcnt,  4, false);

	/* sync mem */
	kgsl_sharedmem_write0((const struct kgsl_memdesc *)
				&g_z1xx.cmdbufdesc[g_z1xx.curr], 0,
				g_z1xx.cmdbuf[g_z1xx.curr],
				(512 + 13) * sizeof(unsigned int), false);

	g_z1xx.offs = 0;
	g_z1xx.curr = nextbuf;

	/* this is Freescale code but it matches a .35 msm kernel too */
	/* increment mark counter */
#ifdef V3_SYNC
	if (device->timestamp == device->current_timestamp) {
		kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, flags);
		kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 0);
	}
#else
	kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, flags);
	kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, ADDR_VGV3_CONTROL, 0);
#endif

	/* increment consumed timestamp */
	device->current_timestamp++;
	kgsl_sharedmem_write0(&device->memstore, KGSL_DEVICE_MEMSTORE_OFFSET(soptimestamp), &device->current_timestamp, 4, 0);
	/* end FSL code */

	return GSL_SUCCESS;
}

int kgsl_g12_addtimestamp(struct kgsl_device* device, unsigned int *timestamp)
{
	device->current_timestamp++;
	*timestamp = device->current_timestamp;

	return GSL_SUCCESS;
}
