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

#define EFIKAMX_DISPLAY_RESET	MX51_PIN_DISPB2_SER_DIN
#define EFIKAMX_HDMI_EN		MX51_PIN_DI1_D1_CS	/*active low*/
#define EFIKAMX_VGA_EN		MX51_PIN_DISPB2_SER_CLK /*active low*/
#define EFIKAMX_HDMI_IRQ	MX51_PIN_DISPB2_SER_DIO

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_display_iomux_pins[] = {
	{EFIKAMX_DISPLAY_RESET, IOMUX_CONFIG_GPIO,},
	{EFIKAMX_HDMI_EN, IOMUX_CONFIG_GPIO, },
	{EFIKAMX_HDMI_IRQ, IOMUX_CONFIG_GPIO, },
	{EFIKAMX_VGA_EN, IOMUX_CONFIG_GPIO, },
};

int mxcfb_initialized = 0;

void mx51_efikamx_display_reset(void)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), 1);
	msleep(1);		/* SII9022 Treset >= 100us */

	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), 0);
	/*
	 * this is a very long time, but we need to wait this long for the
	 * SII9022 to come out of reset *AND* for any hotplug events to have
	 * registered and a sink detection to be absolutely accurate
	 * (SII9022 programmer's reference p42:
	 *		Tplug_dly min. 400 typ. 480 max. 600ms)
	 */
	msleep(600);
}


static struct resource mxcfb_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		},
};

static struct mxc_fb_platform_data mxcfb_data[] = {
	{
		.interface_pix_fmt = IPU_PIX_FMT_RGB24, /* physical pixel format (to transmitter */
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

	/* DISP_EN# and DISP2_EN# go to a level shifter which we need to turn on */
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_HDMI_EN));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_HDMI_EN), "hdmi:enable#");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_HDMI_EN), 0);

	gpio_free(IOMUX_TO_GPIO(EFIKAMX_VGA_EN));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_VGA_EN), "vga:enable#");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_VGA_EN), 1);

	/* HDMI Reset - Assert for i2c disabled mode */
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), "hdmi:reset");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), 0);

	/* HDMI Interrupt pin (plug detect etc.)  */
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_HDMI_IRQ));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_HDMI_IRQ), "hdmi:irq");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_HDMI_IRQ));

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
