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

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>

#include <mach/hardware.h>
#include <mach/gpio.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

static struct mtd_partition mx51_efikamx_spi_nor_partitions[] = {
	{
	 .name = "u-boot",
	 .offset = 0x0,
	 .size = SZ_256K,
	},
	{
	  .name = "config",
	  .offset = MTDPART_OFS_APPEND,
	  .size = SZ_64K,
	},
	{
	  .name = "spare",
	  .offset = MTDPART_OFS_APPEND,
	  .size = MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data mx51_efikamx_spi_flash_data = {
	.name		= "spi_nor_flash",
	.parts 		= mx51_efikamx_spi_nor_partitions,
	.nr_parts	= ARRAY_SIZE(mx51_efikamx_spi_nor_partitions),
	.type		= "sst25vf032b", 	/* jedec id: 0xbf254a */
};

static struct spi_board_info mx51_efikamx_spi_board_info[] __initdata = {
	{
	 .modalias = "m25p80",
	 .max_speed_hz = 25000000, /* max spi clock (SCK) speed in HZ */
	 .chip_select = 1,
	 .platform_data = &mx51_efikamx_spi_flash_data,
#if defined(CONFIG_SPI_GPIO)
	 .bus_num = 0,
	 .controller_data = (void *) IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1),
#elif defined(CONFIG_SPI_IMX)
	 .bus_num = 0,
#elif defined(CONFIG_SPI_MXC)
	 .bus_num = 1,
#endif
	},
};

void mx51_efikamx_init_nor(void)
{
	(void)spi_register_board_info(mx51_efikamx_spi_board_info, ARRAY_SIZE(mx51_efikamx_spi_board_info));
}
