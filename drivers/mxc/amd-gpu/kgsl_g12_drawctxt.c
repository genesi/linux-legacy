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

#include "kgsl_g12_drawctxt.h"
#include "kgsl_types.h"
#include "kgsl_hal.h"
#include "kgsl_cmdstream.h"
#include "kgsl_sharedmem.h"
#include "kgsl_driver.h"
#include "kgsl_ioctl.h"

#include "kgsl_g12_vgv3types.h"
#include "g12_reg.h"

struct kgsl_g12_z1xx g_z1xx = {0};

static void addmarker(struct kgsl_g12_z1xx *z1xx)
{
	if (z1xx) {
		unsigned int *p = z1xx->cmdbuf[z1xx->curr];
		p[z1xx->offs++] = KGSL_G12_STREAM_PACKET;
		p[z1xx->offs++] = (KGSL_G12_MARKER_CMD | 5);
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
		p[z1xx->offs++] = KGSL_G12_STREAM_PACKET;
		p[z1xx->offs++] = 5;
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
		p[z1xx->offs++] = ADDR_VGV3_LAST << 24;
	}
}

int kgsl_g12_drawctxt_create(struct kgsl_device* device, unsigned int type, unsigned int *drawctxt_id, unsigned int flags)
{
	int i;
	int cmd;
	int result;
	unsigned int gslflags = (GSL_MEMFLAGS_CONPHYS | GSL_MEMFLAGS_ALIGNPAGE);

	// unreferenced formal parameters
	(void) device;
	(void) type;
	(void) flags;

	kgsl_device_active(device);

	if (g_z1xx.numcontext == 0) {
		g_z1xx.nextUniqueContextID = 0;

		/* todo: move this to device create or start. Error checking!! */
		for (i = 0; i < GSL_HAL_NUMCMDBUFFERS; i++) {
			if (kgsl_sharedmem_alloc0(KGSL_DEVICE_ANY, gslflags,
				GSL_HAL_CMDBUFFERSIZE, &g_z1xx.cmdbufdesc[i]) != GSL_SUCCESS)
				return GSL_FAILURE;

			g_z1xx.cmdbuf[i] = kzalloc(GSL_HAL_CMDBUFFERSIZE, GFP_KERNEL);

			g_z1xx.curr = i;
			g_z1xx.offs = 0;
			addmarker(&g_z1xx);
			kgsl_sharedmem_write0(&g_z1xx.cmdbufdesc[i], 0, g_z1xx.cmdbuf[i],
						(512 + 13) * sizeof(unsigned int), false);
		}

		g_z1xx.curr = 0;
		cmd = VGV3_NEXTCMD_JUMP << VGV3_NEXTCMD_NEXTCMD_FSHIFT;

		/* set cmd stream buffer to hw */
		result = kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D,
					ADDR_VGV3_MODE, 4);
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D,
					ADDR_VGV3_NEXTADDR,
					g_z1xx.cmdbufdesc[0].gpuaddr );
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D,
					ADDR_VGV3_NEXTCMD,  cmd | 5);
		if (result != GSL_SUCCESS)
			return result;

		/*
		 * NOTE: diverges from upstream Qualcomm code...!!!
		 * there are some cmdwindow writes to VGV3_CONTROL to add markers?
		 */


		/* NOTE: this may be done in Qualcomm's userspace...
		 *
		 * Edge buffer setup todo: move register setup to own function.
		 * This function can be then called, if power managemnet is used and clocks are turned off and then on.
		 */
		result = kgsl_sharedmem_alloc0(KGSL_DEVICE_ANY, gslflags, GSL_HAL_EDGE0BUFSIZE, &g_z1xx.e0);
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_sharedmem_alloc0(KGSL_DEVICE_ANY, gslflags, GSL_HAL_EDGE1BUFSIZE, &g_z1xx.e1);
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_sharedmem_set0(&g_z1xx.e0, 0, 0, GSL_HAL_EDGE0BUFSIZE);
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_sharedmem_set0(&g_z1xx.e1, 0, 0, GSL_HAL_EDGE1BUFSIZE);

		result = kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, GSL_HAL_EDGE0REG, g_z1xx.e0.gpuaddr);
		if (result != GSL_SUCCESS)
			return result;

		result = kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, GSL_HAL_EDGE1REG, g_z1xx.e1.gpuaddr);
		if (result != GSL_SUCCESS)
			return result;

#ifdef _Z180
		kgsl_sharedmem_alloc0(KGSL_DEVICE_ANY, gslflags, GSL_HAL_EDGE2BUFSIZE, &g_z1xx.e2);
		kgsl_sharedmem_set0(&g_z1xx.e2, 0, 0, GSL_HAL_EDGE2BUFSIZE);
		kgsl_g12_cmdwindow_write0(device, GSL_CMDWINDOW_2D, GSL_HAL_EDGE2REG, g_z1xx.e2.gpuaddr);
#endif
	}

	/* diverge from qualcomm - it returns numcontext */
	g_z1xx.numcontext++;
	g_z1xx.nextUniqueContextID++;
	*drawctxt_id = g_z1xx.nextUniqueContextID;

	return GSL_SUCCESS;
}

int kgsl_g12_drawctxt_destroy(struct kgsl_device* device, unsigned int drawctxt_id)
{
	(void) device;
	(void) drawctxt_id;

	g_z1xx.numcontext--;
	if (g_z1xx.numcontext < 0) {
		g_z1xx.numcontext=0;
		return GSL_FAILURE;
	}

	if (g_z1xx.numcontext == 0) {
		int i;
		for (i = 0; i < GSL_HAL_NUMCMDBUFFERS; i++) {
			kgsl_sharedmem_free0(&g_z1xx.cmdbufdesc[i], current->tgid);
			kfree(g_z1xx.cmdbuf[i]);
		}
		/* remember, qualcomm's code doesn't have these edge buffers? */
		kgsl_sharedmem_free0(&g_z1xx.e0, current->tgid);
		kgsl_sharedmem_free0(&g_z1xx.e1, current->tgid);
#ifdef _Z180
		kgsl_sharedmem_free0(&g_z1xx.e2, current->tgid);
#endif
	}
	return GSL_SUCCESS;
}
