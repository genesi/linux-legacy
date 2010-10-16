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
#include <linux/leds.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
#include <mach/common.h>

#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"


#define EFIKAMX_SDHC1_CD MX51_PIN_GPIO1_0
#define EFIKAMX_SDHC1_WP MX51_PIN_GPIO1_1
#define EFIKAMX_SDHC2_CD MX51_PIN_GPIO1_8
#define EFIKAMX_SDHC2_WP MX51_PIN_GPIO1_7


static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_internal_sdhc1_iomux_pins[] = {
	{
	 MX51_PIN_SD1_CMD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA0, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA1, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA2, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA3, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH |
	  PAD_CTL_47K_PU | PAD_CTL_SRE_FAST),
	 },
	/* SDHC1 CD */
	{
	 EFIKAMX_SDHC1_CD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_PKE_ENABLE |
	  PAD_CTL_SRE_FAST),
	 },
	/* SDHC1 WP */
	{
	 EFIKAMX_SDHC1_WP, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU |
	  PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_SRE_FAST),
	 },
};

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_external_sdhc1_iomux_pins[] = {
	{
	 MX51_PIN_SD1_CMD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA0, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA1, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA2, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD1_DATA3, IOMUX_CONFIG_ALT0,
	  (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	/* SDHC1 CD */
	{
	 EFIKAMX_SDHC1_CD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_PKE_ENABLE |
	  PAD_CTL_SRE_FAST),
	 },
	/* SDHC1 WP */
	{
	 EFIKAMX_SDHC1_WP, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU |
	  PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_SRE_FAST),
	 },
};
/* only relevant for board revisions 1.1 and below */
static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_sdhc2_iomux_pins[] = {
	{
	 MX51_PIN_SD2_CMD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD2_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD2_DATA0, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD2_DATA1, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD2_DATA2, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_SD2_DATA3, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_MAX | PAD_CTL_22K_PU | PAD_CTL_SRE_FAST),
	 },
	/* SDHC2 CD */
	{
	 EFIKAMX_SDHC2_WP, IOMUX_CONFIG_ALT6 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_PKE_ENABLE |
	  PAD_CTL_SRE_FAST),
	 },
	/* SDHC2 WP */
	{
	 EFIKAMX_SDHC2_CD, IOMUX_CONFIG_ALT6 | IOMUX_CONFIG_SION,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU |
	  PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_SRE_FAST),
	 },
};




static int mx51_efikamx_sdhc_wp(struct device *dev)
{
	int rc = 0;

	if (mx51_efikamx_revision() >= 2)
	{
		/* only one SD card slot on 1.2 and above */
		rc = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_SDHC1_WP));
	}
	else
	{
		/* handle MicroSDHC carrier internal (0) and external card slot (1) */
		if (to_platform_device(dev)->id == 0)
			rc = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_SDHC1_WP));
		else
			rc = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_SDHC2_WP));
	}

	return rc;
}

static unsigned int mx51_efikamx_sdhc_cd(struct device *dev)
{
	// SDHC CD is active low, by the way, so default is "card inserted"
	int rc = 0;

	if (mx51_efikamx_revision() >= 2)
	{
		/* only one SD card slot on board 1.2 and above (SDHC1) */
		rc = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_SDHC1_CD));
	}
	else
	{
		/* no card detect on SDHC1 (0) on 1.1 there as internal
		   MicroSDHC carrier has no detect pin */
		/* SDHC2 is fine though */
		if (to_platform_device(dev)->id == 1)
			rc = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_SDHC2_CD));
	}

	return rc;
}

static struct mxc_mmc_platform_data mx51_efikamx_sdhc_data = {
	.ocr_mask = MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 52000000,
	.card_inserted_state = 1,
	.status = mx51_efikamx_sdhc_cd,
	.wp_status = mx51_efikamx_sdhc_wp,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

void __init mx51_efikamx_init_sdhc(void)
{
	/*
		On Revision 1.1 down: Internal MicroSD Carrier
	*/
	if (mx51_efikamx_revision() < 2)
	{
		DBG(("Initializing SD card IOMUX (internal, mmc0)\n"));
		CONFIG_IOMUX(mx51_efikamx_internal_sdhc1_iomux_pins);
	}
	/*
		On Revision 1.2 up: External SDHC slot has different IOMUX requirements
	*/
	else
	{
		DBG(("Initializing SD card IOMUX (external, mmc0)\n"));
		CONFIG_IOMUX(mx51_efikamx_external_sdhc1_iomux_pins);
	}

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_SDHC1_CD), "sdhc1_cd");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_SDHC1_CD));		/* SD1 CD */
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_SDHC1_WP), "sdhc1_wp");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_SDHC1_WP));		/* SD1 WP */

	/* include CD flag bit (insertion detection) in the resources */
	mxcsdhc1_device.resource[2].start = IOMUX_TO_IRQ(EFIKAMX_SDHC1_CD);
	mxcsdhc1_device.resource[2].end = IOMUX_TO_IRQ(EFIKAMX_SDHC1_CD);
	mxc_register_device(&mxcsdhc1_device, &mx51_efikamx_sdhc_data);

	/*
		On Revision 1.1 down: External SDHC slot
		Not used on newer revisions.
	*/
	if (mx51_efikamx_revision() < 2)
	{
		DBG(("Initializing SD card IOMUX (external, mmc1)\n"));
		CONFIG_IOMUX(mx51_efikamx_sdhc2_iomux_pins);

		gpio_request(IOMUX_TO_GPIO(EFIKAMX_SDHC2_CD), "sdhc2_wp");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_SDHC2_CD));		/*ron: SDHC2 WP*/
		gpio_request(IOMUX_TO_GPIO(EFIKAMX_SDHC2_WP), "sdhc2_cd");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_SDHC2_WP));		/*ron: SDHC2 CD*/

		/* include CD flag bit (insertion detection) in the resources */
		mxcsdhc2_device.resource[2].start = IOMUX_TO_IRQ(EFIKAMX_SDHC2_CD);
		mxcsdhc2_device.resource[2].end = IOMUX_TO_IRQ(EFIKAMX_SDHC2_CD);
		DBG(("registering mxcsdhc2_device\n"));
		mxc_register_device(&mxcsdhc2_device, &mx51_efikamx_sdhc_data);
	}
}
