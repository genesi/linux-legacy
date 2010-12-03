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
#include <mach/spi.h>

#include "iomux.h"
#include "mx51_pins.h"
#include "mx51_efikamx.h"
#include "devices.h"

#undef SPI_CONFIG_ERROR
#if defined(CONFIG_SPI_MXC)
	#if defined(CONFIG_SPI_IMX) || defined(CONFIG_SPI_GPIO)
		#define SPI_CONFIG_ERROR 1
	#endif
#elif defined(CONFIG_SPI_IMX)
	#if defined(CONFIG_SPI_MXC) || defined(CONFIG_SPI_GPIO)
		#define SPI_CONFIG_ERROR 1
	#endif
#elif defined(CONFIG_SPI_GPIO)
	#if defined(CONFIG_SPI_IMX) || defined(CONFIG_SPI_MXC)
		#define SPI_CONFIG_ERROR 1
	#endif
#endif

#if defined(SPI_CONFIG_ERROR)
		#error pick ONE of CONFIG_SPI_IMX or CONFIG_SPI_MXC or CONFIG_SPI_GPIO please..
#endif


#if defined(CONFIG_SPI_MXC) || defined(CONFIG_SPI_IMX)

/* IOMUX pin definitions valid for mxc_spi and spi_imx drivers */

#define SPI_PAD_CONFIG (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST)

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_spi_iomux_pins[] = {
	{ MX51_PIN_CSPI1_MISO, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_MOSI, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_RDY, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_SCLK, IOMUX_CONFIG_ALT0, SPI_PAD_CONFIG, },
	{ MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_GPIO, },
};

#endif

#if defined(CONFIG_SPI_MXC)

#if defined (PURE_GPIO_CHIPSELECTS)

/* mxc_spi chipselect pin twiddlers for pmic and nor flash */
void mx51_efikamx_spi_chipselect_active(int cspi_mode, int status, int chipselect)
{
	/* we ignore status since we know what the physical chips need */
	int out = -1;
	if (cspi_mode == 1) {
		if (chipselect == 1) {
			out = 1; // NOR (active low) disabled, PMIC (active high) enabled
		} else if (chipselect == 2) {
			out = 0; // NOR (active low) enabled, PMIC (active high) disabled
		}
		if (out != -1) {
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), out);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), out);
		}
	}
}

void mx51_efikamx_spi_chipselect_inactive(int cspi_mode, int status, int chipselect)
{
	/* disable everything, gpio twiddlers will enable the CS on the next transfer anyway */
	if (cspi_mode == 1) {
		gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), 0); // PMIC disable
		gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), 1); // NOR# disable
	}
}

#elif

}/* ecspi chipselect pin for pmic and nor flash */
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
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), !(status & 0x02));
			break;
		case 0x2:
			mxc_request_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX51_PIN_CSPI1_SS1, SPI_PAD_CONFIG);
			mxc_request_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), !(status & 0x01));
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

#endif

struct mxc_spi_master mx51_efikamx_spi_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx51_efikamx_spi_chipselect_active,
	.chipselect_inactive = mx51_efikamx_spi_chipselect_inactive,
};

#elif defined(CONFIG_SPI_IMX)

static struct resource spi_imx_resources[] = {
	{
		.start = CSPI1_BASE_ADDR,
		.end = CSPI1_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_CSPI1,
		.end = MXC_INT_CSPI1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device spi_imx_device = {
	.name = "spi_imx",
	.id = 0,
	.num_resources = ARRAY_SIZE(spi_imx_resources),
	.resource = spi_imx_resources,
};

static int mx51_efikamx_spi_cs[] = {
	IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0),
	IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1),
};

static struct spi_imx_master mx51_efikamx_spi_data = {
	.chipselect = mx51_efikamx_spi_cs,
	.num_chipselect = ARRAY_SIZE(mx51_efikamx_spi_cs),
};

#elif defined(CONFIG_SPI_GPIO) /* above is for SPI_IMX, SPI_MXC */

#include <linux/spi/spi_gpio.h>

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_spi_iomux_pins[] = {
	{ MX51_PIN_CSPI1_MISO, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_CSPI1_MOSI, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_CSPI1_RDY, IOMUX_CONFIG_GPIO, }, // actually not connected??
	{ MX51_PIN_CSPI1_SCLK, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_GPIO, },
};

static struct platform_device gpiospi_device = {
	.name = "spi_gpio",
	.id = 0,
};

static struct spi_gpio_platform_data mx51_efikamx_spi_data = {
	.sck = IOMUX_TO_GPIO(MX51_PIN_CSPI1_SCLK),
	.miso = IOMUX_TO_GPIO(MX51_PIN_CSPI1_MISO),
	.mosi = IOMUX_TO_GPIO(MX51_PIN_CSPI1_MOSI),
	.num_chipselect = 2,
};

// chipselects are defined in the nor.c and pmic.c files (yergh...)

#endif

#if defined(CONFIG_SPI_IMX)

#endif

void mx51_efikamx_init_spi(void)
{
	DBG(("IOMUX for SPI (%u pins)\n", ARRAY_SIZE(mx51_efikamx_spi_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_spi_iomux_pins);

	/* SPI peripheral init is in flash.c and pmic.c */
#if defined(CONFIG_SPI_MXC) || defined(CONFIG_SPI_IMX)
	/* default to PMIC access */
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), 1);
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), 1);
#endif

#if defined(CONFIG_SPI_MXC)
	mxc_register_device(&mxcspi1_device,
#elif defined(CONFIG_SPI_IMX)
	mxc_register_device(&spi_imx_device,
#elif defined(CONFIG_SPI_GPIO)
	/* mxc_request_iomux also requests gpio for us so free them before
	   we register the device or it won't probe properly */

	gpio_free(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SCLK));
	gpio_free(IOMUX_TO_GPIO(MX51_PIN_CSPI1_MISO));
	gpio_free(IOMUX_TO_GPIO(MX51_PIN_CSPI1_MOSI));
	gpio_free(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0));
	gpio_free(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1));

	mxc_register_device(&gpiospi_device,
#endif
		&mx51_efikamx_spi_data);
};
