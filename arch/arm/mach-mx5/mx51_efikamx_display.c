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
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/gpio.h>

#include "devices.h"
#include "iomux.h"
#include "mx51_pins.h"

#include "mx51_efikamx.h"

#define VIDEO_MODE_HDMI_DEF     4

#define MEGA              1000000

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_display_iomux_pins[] = {
	/* display reset pin */
	{
	 MX51_PIN_DISPB2_SER_DIN, IOMUX_CONFIG_GPIO,
	 0,
	 MUX_IN_GPIO3_IPP_IND_G_IN_5_SELECT_INPUT,
	 INPUT_CTL_PATH1,
	 },
};

int mxcfb_initialized = 0;

void mx51_efikamx_display_reset(void)
{
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 1);
	msleep(50);		/* SII9022 Treset >= 100us */

	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);
	msleep(100);
}


static struct resource mxcfb_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		},
};

static struct mxc_fb_platform_data mxcfb_data[] = {
	{
		.interface_pix_fmt = IPU_PIX_FMT_RGB24, /* physical pixel format (to transmitter */
		.mode_str = "800x600-16@60", // safe default for HDMI
	},
};

extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

void __init mx51_efikamx_init_display(void)
{
	CONFIG_IOMUX(mx51_efikamx_display_iomux_pins);

	/* HDMI Reset - Assert for i2c disabled mode */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), "hdmi:reset");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);

	/* HDMI Interrupt pin (plug detect etc.)  */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO), "hdmi:irq");
	gpio_direction_input(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO));

	mxc_ipu_data.di_clk[0] = clk_get(NULL, "ipu_di0_clk");
	mxc_ipu_data.csi_clk[0] = clk_get(NULL, "csi_mclk1");

	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, NULL);
	mxc_register_device(&mxc_v4l2out_device, NULL);
}

/* name is not my choice but.... */
int mxc_init_fb(void)
{
	if ( mxcfb_initialized )
		return 0;

	mxcfb_initialized = 1;

	mxc_fb_devices[0].num_resources = ARRAY_SIZE(mxcfb_resources);
	mxc_fb_devices[0].resource = mxcfb_resources;
	printk(KERN_INFO "registering framebuffer for HDMI\n");
	mxc_register_device(&mxc_fb_devices[0], &mxcfb_data[0]);	// HDMI

	printk(KERN_INFO "registering framebuffer for VPU overlay\n");
	mxc_register_device(&mxc_fb_devices[2], NULL);		// Overlay for VPU

	return 0;
}
device_initcall(mxc_init_fb);

void mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem)
{
		/*reserve memory for gpu*/
		gpu_device.resource[5].start = gpu_start;
		gpu_device.resource[5].end =
				gpu_device.resource[5].start + gpu_mem - 1;
		if (fb_mem) {
			mxcfb_resources[0].start =
				gpu_device.resource[5].end + 1;
			mxcfb_resources[0].end =
				mxcfb_resources[0].start + fb_mem - 1;
		} else {
			mxcfb_resources[0].start = 0;
			mxcfb_resources[0].end = 0;
		}
}
