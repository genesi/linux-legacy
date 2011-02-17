/*
 * Copyright 2009 Pegatron Corporation. All Rights Reserved.
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

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/i2c.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/irqs.h>

#include <linux/fb.h>
#include <linux/i2c/siihdmi.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#if defined(CONFIG_I2C_MXC) && defined(CONFIG_I2C_IMX)
#error pick CONFIG_I2C_MXC or CONFIG_I2C_IMX but not both, please..
#endif

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_i2c_iomux_pins[] = {
	{
	 MX51_PIN_GPIO1_2, IOMUX_CONFIG_ALT0,
	 },
	{
	 MX51_PIN_GPIO1_3, IOMUX_CONFIG_ALT0,
	 },
	{
	 MX51_PIN_KEY_COL5, (IOMUX_CONFIG_ALT3 | IOMUX_CONFIG_SION),
		(PAD_CTL_SRE_FAST | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		 PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
		MUX_IN_I2C2_IPP_SDA_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	{
	 MX51_PIN_KEY_COL4, (IOMUX_CONFIG_ALT3 | IOMUX_CONFIG_SION),
		(PAD_CTL_SRE_FAST | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		 PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
		MUX_IN_I2C2_IPP_SCL_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
};

/* FYI VGA core_reg was VCAM, does that mean we can power the regulator off ? */
extern void mx51_efikamx_display_reset(void);

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

static struct i2c_board_info mx51_efikamx_i2c_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	 },
	{
	 .type = "sii9022",
	 .addr = 0x39,
	 .platform_data = &mx51_efikamx_sii9022_data,
	 },
};


#if defined(CONFIG_I2C_MXC)
static struct mxc_i2c_platform_data mx51_efikamx_i2c2_data = {
	.i2c_clk = 100000,
};
#endif

#if defined(CONFIG_I2C_IMX)
static struct imxi2c_platform_data mx51_efikamx_imxi2c2_data = {
	.bitrate = 100000,
};

static struct resource imxi2c2_resources[] = {
	{
		.start = I2C2_BASE_ADDR,
		.end = I2C2_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_I2C2,
		.end = MXC_INT_I2C2,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device imxi2c2_device = {
	.name = "imx-i2c",
	.id = 1,
	.resource = imxi2c2_resources,
	.num_resources = ARRAY_SIZE(imxi2c2_resources),
};
#endif



void __init mx51_efikamx_init_i2c(void)
{
	DBG(("IOMUX for I2C (%d pins)", ARRAY_SIZE(mx51_efikamx_i2c_iomux_pins)));

	CONFIG_IOMUX(mx51_efikamx_i2c_iomux_pins);

#if defined(CONFIG_I2C_MXC)
	mxc_register_device(&mxci2c_devices[1], &mx51_efikamx_i2c_data);
#elif defined(CONFIG_I2C_IMX)
	mxc_register_device(&imxi2c2_device, &mx51_efikamx_imxi2c2_data);
#else
	#error Please pick at least one of CONFIG_I2C_MXC or CONFIG_I2C_IMX
#endif

	i2c_register_board_info(1, mx51_efikamx_i2c_board_info,
					ARRAY_SIZE(mx51_efikamx_i2c_board_info));
};
