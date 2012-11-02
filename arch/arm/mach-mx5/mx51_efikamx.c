/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2011 Genesi USA, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/leds.h>
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
#include <linux/android_pmem.h>

#include "devices.h"
#include "iomux.h"
#include "mx51_pins.h"
#include "crm_regs.h"
#include "usb.h"

#include "mx51_efikamx.h"


/* android */
static struct android_pmem_platform_data android_pmem_data = {
	.name = "pmem_adsp",
	.size = SZ_16M,
};

static struct android_pmem_platform_data android_pmem_gpu_data = {
	.name = "pmem_gpu",
	.size = SZ_32M,
	.cached = 1,
};

int mx51_efikamx_id = 0; /* can't get less than 1.0 */
int mx51_efikamx_mem = 0;

char *mx51_efikasb_memory_types[3] = {
	"Nanya DDR2",
	"Hynix DDR2",
	"Micron DDR2",
};

#define EFIKAMX_PCBID0	MX51_PIN_NANDF_CS0
#define EFIKAMX_PCBID1	MX51_PIN_NANDF_CS1
#define EFIKAMX_PCBID2	MX51_PIN_NANDF_RB3

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_id_iomux_pins[] = {
	/* Note the 100K pullups. These are being driven low by the different revisions */
	{	EFIKAMX_PCBID0, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
	{	EFIKAMX_PCBID1, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
	{	EFIKAMX_PCBID2, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
};

#define EFIKASB_MEMID0	MX51_PIN_EIM_LBA
#define EFIKASB_MEMID1	MX51_PIN_EIM_CRE
#define EFIKASB_PCBID0	MX51_PIN_EIM_CS3
#define EFIKASB_PCBID1	MX51_PIN_EIM_CS4

static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_id_iomux_pins[] = {
	{ EFIKASB_PCBID0, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_PCBID1, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_MEMID0, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_MEMID1, IOMUX_CONFIG_GPIO, },
};


/*
	EIM A16 CKIH_FREQ_SEL[0]
	EIM A17 CKIH_FREQ_SEL[1]
	EIM A18 BT_LPB[0]
	EIM A19 BT_LPB[1]
	EIM A20 BT_UART_SRC[0]
	EIM A21 BT_UART_SRC[1]
	EIM A22 TBD3 ???
	EIM A23 BT_HPN_EN

	EIM LBA (GPIO3_1) DI1_PIN12
	EIM CRE (GPIO3_2) DI1_PIN13 - Test Point (1.2+), WDOG (1.0, 1.1)

	DI_GP4 DISP2_DRDY_CPU
*/

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_general_iomux_pins[] = {
	{ MX51_PIN_EIM_A18, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_A19, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_A20, IOMUX_CONFIG_GPIO, (PAD_CTL_PKE_ENABLE), },
	{ MX51_PIN_EIM_A21, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_LBA, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_CRE, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_DI_GP4, IOMUX_CONFIG_ALT4, },
};

static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_general_iomux_pins[] = {
	/* ron: GPIO2_24 CPU 22.5792M OSC enable */
        { MX51_PIN_EIM_OE, IOMUX_CONFIG_ALT1, (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE), },
};





int mx51_efikamx_revision(void)
{
	if (machine_is_mx51_efikamx()) {
		return(mx51_efikamx_id & 0xf);
	} else if (machine_is_mx51_efikasb()) {
		return(mx51_efikamx_id);
	}
	return 0;
}

char *mx51_efikamx_memory(void)
{
	return(mx51_efikasb_memory_types[mx51_efikamx_mem]);
}


void mx51_efikamx_board_id(void)
{
	if (machine_is_mx51_efikamx()) {
		int pcbid[3];

		mx51_efikamx_id = 0x10; /* can't get less than 1.0 */

		/* NOTE:
			IOMUX settings have to settle so run the iomux setup long before this
			function has to otherwise it will give freakish results.
		*/
		gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID0), "efikamx:pcbid0");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID0));
		pcbid[0] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID0));

		gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID1), "efikamx:pcbid1");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID1));
		pcbid[1] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID1));

		gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID2), "efikamx:pcbid2");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID2));
		pcbid[2] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID2));

		/*
		 *	PCBID2 PCBID1 PCBID0  STATE
		 *	  1       1      1    ER1:rev1.1
		 *	  1       1      0    ER2:rev1.2
		 *	  1       0      1    ER3:rev1.3
		 *	  1       0      0    ER4:rev1.4
		 */
		if (pcbid[2] == 1) {
			if (pcbid[1] == 1) {
				if (pcbid[0] == 1) {
					mx51_efikamx_id = 0x11;
				} else {
					mx51_efikamx_id = 0x12;
				}
			} else {
				if (pcbid[0] == 1) {
					mx51_efikamx_id = 0x13;
				} else {
					mx51_efikamx_id = 0x14;
				}
			}
		}

		if ( (mx51_efikamx_id == 0x10) ||		/* "developer edition" */
			 (mx51_efikamx_id == 0x12) ||		/* unreleased, broken PATA */
			 (mx51_efikamx_id == 0x14) ) {		/* unreleased.. */
			DBG(("PCB Identification Error!\n"));
			DBG(("Unsupported board revision 1.%u - USE AT YOUR OWN RISK!\n",
					mx51_efikamx_revision() ));
		}
	} else if (machine_is_mx51_efikasb()) {
		int pcbid[2], memid[2];

		gpio_request(IOMUX_TO_GPIO(EFIKASB_MEMID0), "efikasb:memid0");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_MEMID0));
		memid[0] = gpio_get_value(IOMUX_TO_GPIO(EFIKASB_MEMID0));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_MEMID1), "efikasb:memid1");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_MEMID1));
		memid[1] = gpio_get_value(IOMUX_TO_GPIO(EFIKASB_MEMID1));

	        gpio_request(IOMUX_TO_GPIO(EFIKASB_PCBID0), "efikasb:pcbid0");
        	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_PCBID0));
		pcbid[0] = gpio_get_value(IOMUX_TO_GPIO(EFIKASB_PCBID0));

	        gpio_request(IOMUX_TO_GPIO(EFIKASB_PCBID1), "efikasb:pcbid1");
        	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_PCBID1));
		pcbid[1] = gpio_get_value(IOMUX_TO_GPIO(EFIKASB_PCBID1));

		/*
		 *	PCBID1 PCBID0  STATE
		 *	  0	1	TO2: Revision 1.3
		 *	  1	0	TO3: Revision 2.0
		 */

		/* gives us 0x1 or 0x2 */
		mx51_efikamx_id = pcbid[0] | (pcbid[1] << 1);
		mx51_efikamx_mem = memid[0] | (memid[1] << 1);
	}
}


static void __init mx51_efikamx_board_init(void)
{
	mxc_cpu_common_init();
	mxc_register_gpios();

	CONFIG_IOMUX(mx51_efikamx_general_iomux_pins);

	/* do ID pins first! */
	if (machine_is_mx51_efikamx()) {
		CONFIG_IOMUX(mx51_efikamx_id_iomux_pins);
	} else if (machine_is_mx51_efikasb()) {
		CONFIG_IOMUX(mx51_efikasb_id_iomux_pins);
		CONFIG_IOMUX(mx51_efikasb_general_iomux_pins);
	}

	/* common platform configuration for all boards */
	mx51_efikamx_init_uart();
	mx51_efikamx_init_soc();
	mx51_efikamx_init_nor();
	mx51_efikamx_init_spi();
	mx51_efikamx_init_i2c();
	mx51_efikamx_init_pata();

	/* we do board id late because it takes time to settle */
	mx51_efikamx_board_id();

	/* these all depend on board id */
	mx51_efikamx_init_display();
	mx51_efikamx_init_audio();
	mx51_efikamx_init_pmic();
	mx51_efikamx_init_mmc();
	mx51_efikamx_init_leds();
	mx51_efikamx_init_periph();
	mx51_efikamx_init_usb();

	pm_power_off = mx51_efikamx_power_off;

	if (machine_is_mx51_efikamx()) {
		mxc_free_iomux(MX51_PIN_GPIO1_2, IOMUX_CONFIG_ALT2);
		mxc_free_iomux(MX51_PIN_GPIO1_3, IOMUX_CONFIG_ALT2);
		mxc_free_iomux(MX51_PIN_EIM_LBA, IOMUX_CONFIG_GPIO);

		DBG(("Smarttop Revision 1.%u", mx51_efikamx_revision() ));
	} else if (machine_is_mx51_efikasb()) {
		mx51_efikamx_init_battery();

		/* dastardly code to give us 1.3 or 2.0 out of "1" or "2" */
		DBG(("Smartbook Revision %u.%u\n",
					mx51_efikamx_revision(),
					((mx51_efikamx_revision() == 1) ? 3 : 0)  ));
		DBG(("Memory type %s\n", mx51_efikamx_memory() ));
	}

	// android
	mxc_register_device(&mxc_android_pmem_device, &android_pmem_data);
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_data);
}

static struct sys_timer mx51_efikamx_timer = {
	.init	= mx51_efikamx_timer_init,
};

static void __init mx51_efikamx_fixup(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int fb_mem = SZ_16M;
	int gpu_mem = SZ_32M;
	int sys_mem;

	mxc_set_cpu_type(MXC_CPU_MX51);

	get_cpu_wp = mx51_efikamx_get_cpu_wp;
	set_num_cpu_wp = mx51_efikamx_set_num_cpu_wp;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			break;
		}
	}

	sys_mem = total_mem - gpu_mem - fb_mem;

	if (mem_tag) {
		unsigned int fb_start = mem_tag->u.mem.start + sys_mem;
		unsigned int gpu_start = fb_start + fb_mem;

		mem_tag->u.mem.size = sys_mem;
		mx51_efikamx_display_adjust_mem(fb_start, fb_mem);
		mx51_efikamx_gpu_adjust_mem(gpu_start, gpu_mem);
	}
}

void __init mx51_efikamx_pmem_adjust_mem(unsigned int start)
{
	android_pmem_data.start = start;
	android_pmem_gpu_data.start = start + android_pmem_data.size;
}

static void __init mx51_efikamx_android_fixup(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int fb_mem = SZ_16M;
	int gpu_mem = SZ_32M;
	int pmem_mem = android_pmem_data.size + android_pmem_gpu_data.size;
	int sys_mem;

	mxc_set_cpu_type(MXC_CPU_MX51);

	get_cpu_wp = mx51_efikamx_get_cpu_wp;
	set_num_cpu_wp = mx51_efikamx_set_num_cpu_wp;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			break;
		}
	}

	sys_mem = total_mem - gpu_mem - fb_mem - pmem_mem;

	if (mem_tag) {
		unsigned int fb_start = mem_tag->u.mem.start + sys_mem;
		unsigned int gpu_start = fb_start + fb_mem;
		unsigned int pmem_start = gpu_start + gpu_mem;

		mem_tag->u.mem.size = sys_mem;
		mx51_efikamx_display_adjust_mem(fb_start, fb_mem);
		mx51_efikamx_gpu_adjust_mem(gpu_start, gpu_mem);
		mx51_efikamx_pmem_adjust_mem(pmem_start);
	}
}

MACHINE_START(MX51_EFIKAMX, "Genesi Efika MX (Smarttop)")
	/* Maintainer: Genesi USA, Inc. */
#ifdef CONFIG_ANDROID_PMEM
	.fixup = mx51_efikamx_android_fixup,
#else
	.fixup = mx51_efikamx_fixup,
#endif
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine =  mx51_efikamx_board_init,
	.timer = &mx51_efikamx_timer,
MACHINE_END

MACHINE_START(MX51_EFIKASB, "Genesi Efika MX (Smartbook)")
	/* Maintainer: Genesi, Inc. */
#ifdef CONFIG_ANDROID_PMEM
	.fixup = mx51_efikamx_android_fixup,
#else
	.fixup = mx51_efikamx_fixup,
#endif
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mx51_efikamx_board_init,
	.timer = &mx51_efikamx_timer,
MACHINE_END
