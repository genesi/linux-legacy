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
#include <linux/clk.h>
#include <linux/i2c.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/mxc.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#define EFIKAMX_AMP_ENABLE		MX51_PIN_EIM_A23
#define EFIKAMX_AUDIO_CLOCK_ENABLE	MX51_PIN_GPIO1_9
#define EFIKAMX_HP_DETECT		MX51_PIN_DISPB2_SER_RS
#define EFIKAMX_SPDIF_OUT		MX51_PIN_OWIRE_LINE

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_audio_iomux_pins[] = {
	{
	 MX51_PIN_AUD3_BB_TXD, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_HYS_NONE | PAD_CTL_DDR_INPUT_CMOS |
	  PAD_CTL_DRV_VOT_LOW),
	 },
	{
	 MX51_PIN_AUD3_BB_RXD, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_HYS_NONE | PAD_CTL_DDR_INPUT_CMOS |
	  PAD_CTL_DRV_VOT_LOW),
	 },
	{
	 MX51_PIN_AUD3_BB_CK, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_HYS_NONE | PAD_CTL_DDR_INPUT_CMOS |
	  PAD_CTL_DRV_VOT_LOW),
	 },
	{
	 MX51_PIN_AUD3_BB_FS, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_HYS_NONE | PAD_CTL_DDR_INPUT_CMOS |
	  PAD_CTL_DRV_VOT_LOW),
	 },
	{ EFIKAMX_AUDIO_CLOCK_ENABLE, IOMUX_CONFIG_GPIO, }, /* ALT4? */
	{ EFIKAMX_AMP_ENABLE, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
	{ EFIKAMX_HP_DETECT, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
};

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_spdif_iomux_pins[] = {
	{ EFIKAMX_SPDIF_OUT, IOMUX_CONFIG_ALT6, },
};

static int mx51_efikamx_audio_amp_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_AMP_ENABLE), enable ? 1 : 0);
	return 0;
}

static int mx51_efikamx_audio_clock_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_AUDIO_CLOCK_ENABLE), enable ? 0 : 1);
	return 0;
}

static int mx51_efikamx_headphone_det_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_HP_DETECT));
}

static struct mxc_audio_platform_data mx51_efikamx_audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.hp_irq = IOMUX_TO_IRQ(EFIKAMX_HP_DETECT),
	.hp_status = mx51_efikamx_headphone_det_status,
	.amp_enable = mx51_efikamx_audio_amp_enable,
	.clock_enable = mx51_efikamx_audio_clock_enable,
	.sysclk = 12288000,
};

static struct platform_device mx51_efikamx_audio_device = {
	.name = "imx-3stack-sgtl5000",
};

static struct i2c_board_info mx51_efikamx_i2c_audio __initdata = {
	.type = "sgtl5000-i2c",
	.addr = 0x0a,
};

static struct mxc_spdif_platform_data mx51_efikamx_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 0,
	.spdif_clk_44100 = 0,	/* spdif_ext_clk source for 44.1KHz */
	.spdif_clk_48000 = 7,	/* audio osc source */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};

void mx51_efikamx_init_audio(void)
{
	struct clk *ssi_ext1;
	int rate;

	CONFIG_IOMUX(mx51_efikamx_audio_iomux_pins);

	/* turn the SGTL5000 off to start */
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_AUDIO_CLOCK_ENABLE), 1);
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_AMP_ENABLE), 0);
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_HP_DETECT));

	ssi_ext1 = clk_get(NULL, "ssi_ext1_clk");
	rate = clk_round_rate(ssi_ext1, 24000000);
	clk_set_rate(ssi_ext1, rate);
	clk_enable(ssi_ext1);
	mx51_efikamx_audio_data.sysclk = rate;

	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);

	mxc_register_device(&mx51_efikamx_audio_device, &mx51_efikamx_audio_data);

	if (machine_is_mx51_efikamx()) {
		CONFIG_IOMUX(mx51_efikamx_spdif_iomux_pins);

		mx51_efikamx_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_xtal_clk");
		clk_put(mx51_efikamx_spdif_data.spdif_core_clk);

		mxc_register_device(&mxc_alsa_spdif_device, &mx51_efikamx_spdif_data);
	}

	i2c_register_board_info(1, &mx51_efikamx_i2c_audio, 1);
};
