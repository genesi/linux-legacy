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
#include "../../../drivers/video/mxc/siihdmi.h"



extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2, /* IPUv3EX (MX51) is 2, IPUv3M (MX53) is 3 */
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};




#define EFIKAMX_DISPLAY_RESET	MX51_PIN_DISPB2_SER_DIN
#define EFIKAMX_HDMI_EN		MX51_PIN_DI1_D1_CS	/* active low */
#define EFIKAMX_VGA_EN		MX51_PIN_DISPB2_SER_CLK /* active low */
#define EFIKAMX_HDMI_IRQ	MX51_PIN_DISPB2_SER_DIO

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_display_iomux_pins[] = {
	{EFIKAMX_DISPLAY_RESET, IOMUX_CONFIG_GPIO,},
	{EFIKAMX_HDMI_EN, IOMUX_CONFIG_GPIO, },
	{EFIKAMX_HDMI_IRQ, IOMUX_CONFIG_GPIO, },
	{EFIKAMX_VGA_EN, IOMUX_CONFIG_GPIO, },
};

void mx51_efikamx_display_reset(void)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), 1);
	msleep(1);		/* SII9022 Treset >= 100us */

	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_DISPLAY_RESET), 0);
}




#define EFIKASB_LCD_POWER	MX51_PIN_CSI1_D9
#define EFIKASB_LVDS_RESET	MX51_PIN_DISPB2_SER_DIN
#define EFIKASB_LVDS_POR	MX51_PIN_DISPB2_SER_CLK /* active low */
#define EFIKASB_LVDS_EN		MX51_PIN_CSI1_D8
#define EFIKASB_NODISPLAY_EN	MX51_PIN_DI1_D1_CS	/* active low */

static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_display_iomux_pins[] = {
	{EFIKASB_LCD_POWER, IOMUX_CONFIG_GPIO, },
	{EFIKASB_LVDS_RESET, IOMUX_CONFIG_GPIO, },
	{EFIKASB_LVDS_POR, IOMUX_CONFIG_GPIO, },
	{EFIKASB_NODISPLAY_EN, IOMUX_CONFIG_GPIO, },

        {MX51_PIN_DI2_DISP_CLK, IOMUX_CONFIG_ALT0, /* drive strength low to avoid EMI */
		(PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_LOW | PAD_CTL_SRE_FAST)
	},
};

static void mx51_efikasb_lcd_power(int state)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LCD_POWER), state);
}

static void mx51_efikasb_lvds_power(int state)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LVDS_POR), state);
}

static void mx51_efikasb_lvds_enable(int state)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LVDS_EN), state);
}

static void mx51_efikasb_lvds_reset(void)
{
        gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET), 0);
        msleep(50);
        gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET), 1);
        msleep(10);
        gpio_set_value(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET), 0);

}

static struct siihdmi_platform_data mx51_efikamx_siihdmi_data = {
	.reset       = mx51_efikamx_display_reset,

	.vendor      = "Genesi",
	.description = "Efika MX",

	.framebuffer = "DISP3 BG",

	.hotplug     = {
		.start = IOMUX_TO_IRQ(MX51_PIN_DISPB2_SER_DIO),
		.end   = IOMUX_TO_IRQ(MX51_PIN_DISPB2_SER_DIO),
		.name  = "video-hotplug",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},

	.pixclock    = KHZ2PICOS(133000L),
};

static struct mxc_lcd_platform_data mx51_efikasb_mtl017_data = {
	.core_reg = "VCAM",
	.io_reg = "VGEN3",
	.analog_reg = "VAUDIO",
	.lcd_power = mx51_efikasb_lcd_power,
	.lvds_power = mx51_efikasb_lvds_power,
	.lvds_enable = mx51_efikasb_lvds_enable,
	.reset = mx51_efikasb_lvds_reset,
};

#define EFIKAMX_HDMI_DISPLAY_ID	0
#define EFIKASB_LVDS_DISPLAY_ID	1


static struct i2c_board_info mx51_efikamx_i2c_display[] __initdata = {
	[EFIKAMX_HDMI_DISPLAY_ID] = {
	.type = "siihdmi",
	.addr = 0x39,
	.platform_data = &mx51_efikamx_siihdmi_data,
	},
	[EFIKASB_LVDS_DISPLAY_ID] = {
	.type = "mtl017",
	.addr = 0x3a,
	.platform_data = &mx51_efikasb_mtl017_data,
	},
};

static struct mxc_fb_platform_data mx51_efikamx_display_data[] = {
	[EFIKAMX_HDMI_DISPLAY_ID] = {
		.interface_pix_fmt = IPU_PIX_FMT_RGB24,
		.external_clk = true,
	},
	[EFIKASB_LVDS_DISPLAY_ID] = {
		.interface_pix_fmt = IPU_PIX_FMT_RGB565,
	},
};

static char *mxcfb_clocks[] = {
	[EFIKAMX_HDMI_DISPLAY_ID] = "ipu_di0_clk",
	[EFIKASB_LVDS_DISPLAY_ID] = "ipu_di1_clk",
};


static struct resource mx51_efikamx_fb_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		},
};

void __init mx51_efikamx_display_adjust_mem(unsigned int start, unsigned int size)
{
	mx51_efikamx_fb_resources[0].start = start;
	mx51_efikamx_fb_resources[0].end = start + size - 1;
}

void __init mx51_efikamx_gpu_adjust_mem(unsigned int start, unsigned int size)
{
	gpu_device.resource[5].start = start;
	gpu_device.resource[5].end = start + size - 1;
}

void __init mx51_efikamx_init_display(void)
{
	int display_id = 0;

	if (machine_is_mx51_efikamx()) {
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

		display_id = EFIKAMX_HDMI_DISPLAY_ID;
	}
	else if (machine_is_mx51_efikasb()) {
		CONFIG_IOMUX(mx51_efikasb_display_iomux_pins);

		/* empty display controller, deactivate */
		gpio_free(IOMUX_TO_GPIO(EFIKASB_NODISPLAY_EN));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_NODISPLAY_EN), "di0:enable#");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_NODISPLAY_EN), 1);

		/* connected to MTL017 "power on reset" pin */
		gpio_free(IOMUX_TO_GPIO(EFIKASB_LVDS_POR));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_LVDS_POR), "lvds:enable#");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_LVDS_POR), 1);

		/* LVDS Reset - Assert for i2c disabled mode */
		gpio_free(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET), "lvds:reset");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_LVDS_RESET), 0);

		gpio_free(IOMUX_TO_GPIO(EFIKASB_LCD_POWER));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_LCD_POWER), "lcd:power");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_LCD_POWER), 1);

		display_id = EFIKASB_LVDS_DISPLAY_ID;
	}

	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_gpu_data.enable_mmu = 0;
	mxc_register_device(&gpu_device, &mxc_gpu_data);
	mxc_register_device(&mxc_v4l2out_device, NULL);

	/* display_id is specific to the board, and configures the primary DI for each board
	 * (DI0 on MX, DI1 on SB) first to make it the first framebuffer device.
	 */
	mxc_ipu_data.di_clk[display_id] = clk_get(NULL, mxcfb_clocks[display_id]);

	mxc_fb_devices[display_id].num_resources = ARRAY_SIZE(mx51_efikamx_fb_resources);
	mxc_fb_devices[display_id].resource = mx51_efikamx_fb_resources;
	mxc_register_device(&mxc_fb_devices[display_id], &mx51_efikamx_display_data[display_id]);

	/* register /dev/fb1 even though it's not used. We just register the other DI with the LVDS platform
	 * data, since this is all it really needs to create the framebuffer, even though it just won't be
	 * used for anything.
	 */
	mxc_register_device(&mxc_fb_devices[!display_id], &mx51_efikamx_display_data[EFIKASB_LVDS_DISPLAY_ID]);

	/* video overlay, absolutely must be /dev/fb2 and therefore registered after TWO framebuffers otherwise
	 * the v4l2sink doesn't work right. This is probably actually a major bug in userspace somewhere..
	 */
	mxc_register_device(&mxc_fb_devices[2], NULL);

	/* make siihdmi and mtl017 appear */
	i2c_register_board_info(1, &mx51_efikamx_i2c_display[display_id], 1);
}
