/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/keypad.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/mxc_edid.h>

#include "devices.h"
#include "iomux.h"
#include "mx51_pins.h"
#include "crm_regs.h"
#include "usb.h"

#include "mx51_efikamx.h"


extern struct resource mxcfb_resources[];


static void __init mx51_efikamx_board_init(void)
{
	mxc_cpu_common_init();
	mxc_register_gpios();

	/* calls io.c */
	mx51_efikamx_io_init();

	DBG(("AIPS1_BASE_ADDR = 0x%x\n", AIPS1_BASE_ADDR));
	DBG(("AIPS1_BASE_ADDR_VIRT = 0x%x, io_pg_offst = 0x%x\n", 
		AIPS1_BASE_ADDR_VIRT, 
		((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc)
		);
	DBG(("PHYS_OFFSET = 0x%lx\n", PHYS_OFFSET));

	pm_power_off = mx51_efikamx_power_off;

	printk(KERN_INFO "Efika MX Revision 1.%u", mx51_efikamx_revision());
}



static struct sys_timer mx51_efikamx_timer = {
	.init	= mx51_efikamx_timer_init,
};


static void __init mx51_efikamx_fixup(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	char *str;
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int left_mem = 0;
	int gpu_mem = SZ_64M;
	int fb_mem = SZ_32M;

	mxc_set_cpu_type(MXC_CPU_MX51);

	get_cpu_wp = mx51_efikamx_get_cpu_wp;
	set_num_cpu_wp = mx51_efikamx_set_num_cpu_wp;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			left_mem = total_mem - gpu_mem - fb_mem;
			break;
		}
	}

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				left_mem = memparse(str, &str);
				if (left_mem == 0 || left_mem > total_mem)
					left_mem = total_mem - gpu_mem - fb_mem;
			}

			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpu_memory=");
			if (str != NULL) {
				str += 11;
				gpu_mem = memparse(str, &str);
			}

			break;
		}
	}

	if (mem_tag) {
		fb_mem = total_mem - left_mem - gpu_mem;
		if (fb_mem < 0) {
			gpu_mem = total_mem - left_mem;
			fb_mem = 0;
		}
		mem_tag->u.mem.size = left_mem;

		mx51_efikamx_display_adjust_mem(mem_tag->u.mem.start + left_mem,
						gpu_mem,
						fb_mem);

	}
}


MACHINE_START(MX51_EFIKAMX, "Genesi Efika MX (Smarttop)")
	/* these next three should be patched into U-Boot and
	 * CONFIG_RUNTIME_PHYS_OFFSET supported properly
	 */
	.phys_io = AIPS1_BASE_ADDR,
	.io_pg_offst = ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.boot_params = PHYS_OFFSET + 0x100,
	/* Maintainer: Genesi USA, Inc., Pegatron Corporation */
	.fixup = mx51_efikamx_fixup,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine =  mx51_efikamx_board_init,
	.timer = &mx51_efikamx_timer,
MACHINE_END







#if 0

MACHINE_START(MX51_EFIKASB, "Genesi Efika MX (Smartbook)")
	/* Maintainer: Genesi USA, Inc., Pegatron Corporation */
	.fixup = mx5_efikasb_fixup,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine =  mx5_efikasb_board_init,
	.timer = &mx5_efikasb_timer,
MACHINE_END

#endif
