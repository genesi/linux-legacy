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
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/mxc.h>
#include "iomux.h"
#include "mx51_pins.h"
#include "mx51_efikamx.h"
#include "devices.h"

#define SPI_PAD_CONFIG (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH | \
						PAD_CTL_SRE_FAST)

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_spi_iomux_pins[] = {
	{ MX51_PIN_CSPI1_MISO, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_MOSI, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_RDY, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_SCLK, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
};

/* ecspi chipselect pin for pmic and nor flash */
void mx51_efikamx_spi_chipselect_active(int cspi_mode, int status,
					    int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			mxc_request_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX51_PIN_CSPI1_SS0, SPI_PAD_CONFIG);
			mxc_request_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_GPIO);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), 0);
			gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), !(status & 0x02));
			break;
		case 0x2:
			mxc_request_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX51_PIN_CSPI1_SS1, SPI_PAD_CONFIG);
			mxc_request_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), 0);
			gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), !(status & 0x01));
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void mx51_efikamx_spi_chipselect_inactive(int cspi_mode, int status,
					      int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_ALT0);
			mxc_free_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_GPIO);
			mxc_request_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			break;
		case 0x2:
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			mxc_free_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_ALT0);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

}

struct mxc_spi_master mx51_efikamx_spi_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx51_efikamx_spi_chipselect_active,
	.chipselect_inactive = mx51_efikamx_spi_chipselect_inactive,
};

void mx51_efikamx_init_spi(void)
{
	DBG(("IOMUX for SPI (%u pins)\n", ARRAY_SIZE(mx51_efikamx_spi_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_spi_iomux_pins);

	/* SPI peripheral init is in flash.c and pmic.c */
	/* BABBAGE: where does mxcspi1_device come from */
	mxc_register_device(&mxcspi1_device, &mx51_efikamx_spi_data);
};
