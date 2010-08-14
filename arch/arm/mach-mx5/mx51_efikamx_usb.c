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
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/common.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"
#include "usb.h"

#include "mx51_efikamx.h"


#define USB_PAD_CONFIG \
			(PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |	\
			PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE)

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_usb_iomux_pins[] = {
	/* USBH2_DATA0 */
	{
	 MX51_PIN_EIM_D16, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE |
	  PAD_CTL_HYS_ENABLE),
	 },
	/* USBH2_DATA1 */
	{	MX51_PIN_EIM_D17, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA2 */
	{	MX51_PIN_EIM_D18, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA3 */
	{	MX51_PIN_EIM_D19, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA4 */
	{	MX51_PIN_EIM_D20, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA5 */
	{	MX51_PIN_EIM_D21, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA6 */
	{	MX51_PIN_EIM_D22, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DATA7 */
	{	MX51_PIN_EIM_D23, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_CLK */
	{	MX51_PIN_EIM_A24, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_DIR */
	{	MX51_PIN_EIM_A25, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_STP */
	{	MX51_PIN_EIM_A26, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	/* USBH2_NXT */
	{	MX51_PIN_EIM_A27, IOMUX_CONFIG_ALT2, USB_PAD_CONFIG,	},
	{
	 MX51_PIN_USBH1_STP, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
	 },
	{
	 MX51_PIN_USBH1_CLK, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
	 },
	{
	 MX51_PIN_USBH1_DIR, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
	 },
	{
	 MX51_PIN_USBH1_NXT, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
	 },
	{	MX51_PIN_USBH1_DATA0, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA1, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA2, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA3, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA4, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA5, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA6, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },
	{	MX51_PIN_USBH1_DATA7, IOMUX_CONFIG_ALT0, USB_PAD_CONFIG, },

	/* USB HUB RESET  */
	{	MX51_PIN_GPIO1_5, IOMUX_CONFIG_ALT0,
		(PAD_CTL_DRV_HIGH | PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST),
	},

	/* Wireless enable (active low) */
	{ MX51_PIN_EIM_A22, IOMUX_CONFIG_GPIO, },
	/* Wireless reset */
	{ MX51_PIN_EIM_A16, IOMUX_CONFIG_GPIO, },
	/* Bluetooth enable (active low) */
	{ MX51_PIN_EIM_A17, IOMUX_CONFIG_GPIO, },
	/* USB PHY reset */
	{
	 MX51_PIN_EIM_D27, IOMUX_CONFIG_ALT1,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_NONE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST),
	},
};

void __init mx51_efikamx_init_usb(void)
{
	DBG(("IOMUX for USB (%d pins)\n", ARRAY_SIZE(mx51_efikamx_usb_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_usb_iomux_pins);

	gpio_request(IOMUX_TO_GPIO(MX51_PIN_GPIO1_5), "usb_hub_reset");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_GPIO1_5), 1);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_GPIO1_5), 0);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_GPIO1_5), 1);

	/* enable BlueTooth */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_EIM_A17), "bluetooth_enable");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_EIM_A17), 0);
	msleep(10);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_EIM_A17), 1);

	/* pull low wlan_on# */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_EIM_A22), "wlan_on");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_EIM_A22), 0);
	msleep(10);

	/* pull low-high pulse wlan_rst# */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_EIM_A16), "wlan_rst");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_EIM_A16), 0);
	msleep(10);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_EIM_A16), 1);

	/* De-assert USB PHY RESETB */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_EIM_D27), "usb_phy_resetb");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_EIM_D27), 1);

	mx5_usb_dr_init();
	mx5_usbh1_init();
}
