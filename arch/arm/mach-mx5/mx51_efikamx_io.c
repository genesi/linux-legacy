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
#include "mx51_pins.h"
#include "iomux.h"
#include "mx51_efikamx.h"


#define EFIKAMX_PCBID0	MX51_PIN_NANDF_CS0
#define EFIKAMX_PCBID1	MX51_PIN_NANDF_CS1
#define EFIKAMX_PCBID2	MX51_PIN_NANDF_RB3

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_id_iomux_pins[] = {
	/* Note the 100K pullups. These are being driven low by the different revisions */
	{	EFIKAMX_PCBID0, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
	{	EFIKAMX_PCBID1, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
	{	EFIKAMX_PCBID2, IOMUX_CONFIG_GPIO, PAD_CTL_100K_PU, },
};

int mx51_efikamx_id = 0x10; /* can't get less than 1.0 */



/*
	EIM A16 CKIH_FREQ_SEL[0]
	EIM A17 CKIH_FREQ_SEL[1]
	EIM A18 BT_LPB[0]
	EIM A19 BT_LPB[1]
	EIM A20 BT_UART_SRC[0]
	EIM A21 BT_UART_SRC[1]
	EIM A22 TBD3 ???
	EIM A23 BT_HPN_EN

	EIM LBA (GPIO3_1) DI1_PIN12
	EIM CRE (GPIO3_2) DI1_PIN13 - Test Point (1.2+), WDOG (1.0, 1.1)

	DI_GP4 DISP2_DRDY_CPU
*/


static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_general_iomux_pins[] = {
	{ MX51_PIN_EIM_A18, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_A19, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_A20, IOMUX_CONFIG_GPIO, (PAD_CTL_PKE_ENABLE), },
	{ MX51_PIN_EIM_A21, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_LBA, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_EIM_CRE, IOMUX_CONFIG_GPIO, },
	{ MX51_PIN_DI_GP4, IOMUX_CONFIG_ALT4, },
};

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_wdog2_iomux_pins[] = {
	/* Watchdog (1.2+) */
	{
	 MX51_PIN_GPIO1_4, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU),
	 },
};

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_wdog1_iomux_pins[] = {
	/* Watchdog (1.1) */
	{
	 MX51_PIN_DI1_PIN13, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU),
	 },
};

__init void mx51_efikamx_init_wdog(void)
{
	/* Watchdog */

	if (mx51_efikamx_revision() < 2)
	{
		DBG(("IOMUX for WDOG (1.1) (%u pins)\n", ARRAY_SIZE(mx51_efikamx_wdog1_iomux_pins)));
		CONFIG_IOMUX(mx51_efikamx_wdog1_iomux_pins);

		gpio_request(IOMUX_TO_GPIO(MX51_PIN_DI1_PIN13), "wdog_rst");
		gpio_direction_input(MX51_PIN_DI1_PIN13);
	}
	else
	{
		DBG(("IOMUX for WDOG (1.2) (%u pins)\n", ARRAY_SIZE(mx51_efikamx_wdog2_iomux_pins)));
		CONFIG_IOMUX(mx51_efikamx_wdog2_iomux_pins);

		gpio_request(IOMUX_TO_GPIO(MX51_PIN_GPIO1_4), "wdog_rst");
		gpio_direction_input(MX51_PIN_GPIO1_4);
	}
}

void __init mx51_efikamx_io_init(void)
{
	/* do ID pins first! */
	DBG(("IOMUX for Board ID (%u pins)\n", ARRAY_SIZE(mx51_efikamx_id_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_id_iomux_pins);
	DBG(("IOMUX for General GPIO Stuff (%u pins)\n", ARRAY_SIZE(mx51_efikamx_general_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_general_iomux_pins);

	mx51_efikamx_init_soc();

	mx51_efikamx_init_pmic();
	mx51_efikamx_init_nor();
	mx51_efikamx_init_spi();

	mx51_efikamx_init_i2c();     /* i2c devices like sii9022 need IPU already there */

	mx51_efikamx_init_usb();

	mx51_efikamx_init_display(); /* register IPU and so on */
	mx51_efikamx_init_audio(); /* register audio devices and so on */

	mx51_efikamx_board_id(); /* we do board id late because it takes time to settle */
	mx51_efikamx_init_pata();
	mx51_efikamx_init_sdhc(); /* depends on board id - do it last */
	mx51_efikamx_init_wdog(); /* depends on board id - do it last */
	mx51_efikamx_init_leds(); /* mmc trigger depends on board id */


	mxc_free_iomux(MX51_PIN_GPIO1_2, IOMUX_CONFIG_ALT2);
	mxc_free_iomux(MX51_PIN_GPIO1_3, IOMUX_CONFIG_ALT2);
	mxc_free_iomux(MX51_PIN_EIM_LBA, IOMUX_CONFIG_GPIO);
}

int mx51_efikamx_revision(void)
{
	/* short and sweet */
	return(mx51_efikamx_id & 0xf);
}

void mx51_efikamx_board_id(void)
{
	int pcbid[3];

	/* NOTE:
		IOMUX settings have to settle so run the iomux setup long before this
		function has to otherwise it will give freakish results.
	*/
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID0), "efikamx_pcbid0");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID0));
	pcbid[0] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID0));

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID1), "efikamx_pcbid1");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID1));
	pcbid[1] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID1));

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_PCBID2), "efikamx_pcbid2");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_PCBID2));
	pcbid[2] = gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_PCBID2));

/*
	PCBID2 PCBID1 PCBID0  STATE
	  1       1      1    ER1:rev1.1
	  1       1      0    ER2:rev1.2
	  1       0      1    ER3:rev1.3
	  1       0      0    ER4:rev1.4
*/
	if (pcbid[2] == 1) {
		if (pcbid[1] == 1) {
			if (pcbid[0] == 1) {
				mx51_efikamx_id = 0x11;
			} else {
				mx51_efikamx_id = 0x12;
			}
		} else {
			if (pcbid[0] == 1) {
				mx51_efikamx_id = 0x13;
			} else {
				mx51_efikamx_id = 0x14;
			}
		}
	}

	if ( (mx51_efikamx_id == 0x10) ||		/* "developer edition" */
		 (mx51_efikamx_id == 0x12) ||		/* unreleased, broken PATA */
		 (mx51_efikamx_id == 0x14) ) {		/* unreleased.. */
		printk(KERN_ERR "Efika MX: PCB Identification Error!\n"
				"Efika MX: Unsupported board revision 1.%u - USE AT YOUR OWN RISK!\n", 
				mx51_efikamx_revision());
	}
}

