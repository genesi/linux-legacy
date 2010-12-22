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
#include <linux/fsl_devices.h>
#include <linux/ata.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/gpio.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#if defined(CONFIG_PATA_PLATFORM_MODULE) || defined(CONFIG_PATA_PLATFORM)
#include <linux/ata_platform.h>
#if defined(CONFIG_PATA_FSL_MODULE) || defined(CONFIG_PATA_FSL)
#warning please select PATA_FSL or PATA_PLATFORM but not both...
#endif
#endif


#define ATA_PAD_CONFIG (PAD_CTL_DRV_HIGH | PAD_CTL_DRV_VOT_HIGH)

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_ata_iomux_pins[] = {
	{ MX51_PIN_NANDF_ALE, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CS2, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CS3, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CS4, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CS5, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CS6, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_RE_B, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_WE_B, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_CLE, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_RB0, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_WP_B, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	/* TO 2.0 */
	{ MX51_PIN_GPIO_NAND, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_RB1, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D0, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D1, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D2, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D3, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D4, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D5, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D6, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D7, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D8, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D9, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D10, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D11, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D12, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D13, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D14, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
	{ MX51_PIN_NANDF_D15, IOMUX_CONFIG_ALT1, ATA_PAD_CONFIG, },
};


#if defined(CONFIG_PATA_FSL) || defined(CONFIG_PATA_FSL_MODULE)
static struct fsl_ata_platform_data mx51_efikamx_ata_data = {
	.udma_mask = ATA_UDMA3,
	.mwdma_mask = ATA_MWDMA2,
	.pio_mask = ATA_PIO4,
	.fifo_alarm = MXC_IDE_DMA_WATERMARK / 2,
	.max_sg = MXC_IDE_DMA_BD_NR,
	.core_reg = NULL,
	.io_reg = NULL,
};
#elif defined(CONFIG_PATA_PLATFORM) || defined(CONFIG_PATA_PLATFORM_MODULE)
/*
 * pata_fsl includes the whole lot in it's register set but pata_platform
 * requires the last register to be it's own whole resouce. Dumb but we will
 * accomodate it..
 */
static struct resource pata_platform_resources[] = {
	{
		.start = ATA_BASE_ADDR,
		.end = ATA_BASE_ADDR + 0x000000C0,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = ATA_BASE_ADDR + 0x000000D8,
		.end = ATA_BASE_ADDR + 0x000000DC,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_ATA,
		.end = MXC_INT_ATA,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device pata_platform_device = {
	.name = "pata_platform",
	.id = 0,
        .num_resources = ARRAY_SIZE(pata_platform_resources),
        .resource = pata_platform_resources,
};

static struct pata_platform_info mx51_efikamx_ata_data = {
	.ioport_shift = 2,
};
#endif


void __init mx51_efikamx_init_pata(void)
{
	DBG(("IOMUX for ATA (%d pins)\n", ARRAY_SIZE(mx51_efikamx_ata_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_ata_iomux_pins);

#if defined(CONFIG_PATA_FSL) || defined(CONFIG_PATA_FSL_MODULE)
	mxc_register_device(&pata_fsl_device,
#elif defined(CONFIG_PATA_PLATFORM) || defined(CONFIG_PATA_PLATFORM_MODULE)
	mxc_register_device(&pata_platform_device,
#endif
						&mx51_efikamx_ata_data);
}
