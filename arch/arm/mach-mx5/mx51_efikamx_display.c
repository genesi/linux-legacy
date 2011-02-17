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
#include <linux/i2c/siihdmi.h>
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



extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
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
	/*
	 * this is a very long time, but we need to wait this long for the
	 * SII9022 to come out of reset *AND* for any hotplug events to have
	 * registered and a sink detection to be absolutely accurate
	 * (SII9022 programmer's reference p42:
	 *		Tplug_dly min. 400 typ. 480 max. 600ms)
	 */
	msleep(600);
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




static struct siihdmi_platform_data mx51_efikamx_sii9022_data = {
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
	.type = "sii9022",
	.addr = 0x39,
	.platform_data = &mx51_efikamx_sii9022_data,
	},
	[EFIKASB_LVDS_DISPLAY_ID] = {
	.type = "mtl017",
	.addr = 0x3a,
	.platform_data = &mx51_efikasb_mtl017_data,
	},
};


static struct resource mxcfb_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		},
};

static struct mxc_fb_platform_data mx51_efikamx_display_data[] = {
	[EFIKAMX_HDMI_DISPLAY_ID] = {
		.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	},
	[EFIKASB_LVDS_DISPLAY_ID] = {
		.interface_pix_fmt = IPU_PIX_FMT_RGB565,
	},
};

static char *mxcfb_clocks[] = {
	[EFIKAMX_HDMI_DISPLAY_ID] = "ipu_di0_clk",
	[EFIKASB_LVDS_DISPLAY_ID] = "ipu_di1_clk",
};




void __init mx51_efikamx_init_display(void)
{
	int display_id = 0;

	/* what is this in aid of? */
	clk_set_rate(clk_get(&(mxc_fb_devices[0].dev), "axi_b_clk"), 133000000);

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
	mxc_register_device(&gpu_device, NULL);
	mxc_register_device(&mxc_v4l2out_device, NULL);

	/* display_id is specific to the board, and configures the single controller
	 * we have on each board except for errant, weirdo VGA systems (which are not
	 * supported)
	 */
	mxc_ipu_data.di_clk[display_id] = clk_get(NULL, mxcfb_clocks[display_id]);
	mxc_fb_devices[display_id].num_resources = ARRAY_SIZE(mxcfb_resources);
	mxc_fb_devices[display_id].resource = mxcfb_resources;
	mxc_register_device(&mxc_fb_devices[display_id], &mx51_efikamx_display_data[display_id]);

	i2c_register_board_info(1, &mx51_efikamx_i2c_display[display_id], 1);

	/* video overlay */
	mxc_register_device(&mxc_fb_devices[2], NULL);
}

void __init mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem)
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
