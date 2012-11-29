/* Copyright (c) 2002,2008-2009, Code Aurora Forum. All rights reserved.
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
#include <linux/debugfs.h>
#include "kgsl_log.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_DEFAULT 3
#define KGSL_LOG_LEVEL_MAX     7
unsigned int kgsl_drv_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_cmd_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_ctxt_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_mem_log = KGSL_LOG_LEVEL_DEFAULT;

struct device *kgsl_driver_getdevnode(void)
{
	return &gsl_driver.pdev->dev;
}
