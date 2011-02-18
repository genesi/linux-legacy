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
#include <asm/mach-types.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"
#include "usb.h"

#include "mx51_efikamx.h"

#define POWER_ON	1
#define POWER_OFF	0

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
};

#define EFIKAMX_USB_HUB_RESET	MX51_PIN_GPIO1_5
#define EFIKAMX_USB_PHY_RESET	MX51_PIN_EIM_D27

struct mxc_iomux_pin_cfg mx51_efikamx_usb_control_pins[] = {
	{
	 EFIKAMX_USB_HUB_RESET, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST),
	},
	{
	 EFIKAMX_USB_PHY_RESET, IOMUX_CONFIG_ALT1,
	 (PAD_CTL_DRV_HIGH | PAD_CTL_HYS_NONE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST),
	},
};

void mx51_efikamx_usb_hub_reset(void)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_USB_HUB_RESET), 1);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_USB_HUB_RESET), 0);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_USB_HUB_RESET), 1);
}

void mx51_efikamx_usb_phy_reset(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_USB_PHY_RESET), 1);
}

void __init mx51_efikamx_init_usb(void)
{
	CONFIG_IOMUX(mx51_efikamx_usb_iomux_pins);
	CONFIG_IOMUX(mx51_efikamx_usb_control_pins);

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_USB_HUB_RESET), "usb:hub_reset");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_USB_HUB_RESET), 1);

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_USB_PHY_RESET), "usb:phy_reset");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_USB_PHY_RESET), 0);

	mx51_efikamx_usb_hub_reset();

	mx51_efikamx_bluetooth_power(POWER_ON);
	mx51_efikamx_wifi_power(POWER_ON);
	mx51_efikamx_wifi_reset();

	mx51_efikamx_usb_phy_reset();

	if (machine_is_mx51_efikasb()) {
		mx51_efikamx_wwan_power(POWER_ON);
		mx51_efikamx_camera_power(POWER_ON);
	}

	mx5_usb_dr_init();
	mx5_usbh1_init();

	if (machine_is_mx51_efikasb()) {
		mx51_usbh2_init();
	}
}

