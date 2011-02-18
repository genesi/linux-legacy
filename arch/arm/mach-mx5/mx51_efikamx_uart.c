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
#include "mx51_efikamx.h"
#include "mx51_pins.h"
#include "iomux.h"

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_uart_iomux_pins[] = {
	{
	 MX51_PIN_UART1_RXD, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	  PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST),
	 MUX_IN_UART1_IPP_UART_RXD_MUX_SELECT_INPUT,
	 INPUT_CTL_PATH0,
	 },
	{
	 MX51_PIN_UART1_TXD, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	  PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST),
	 },
	{
	 MX51_PIN_UART1_RTS, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	  PAD_CTL_DRV_HIGH),
	 MUX_IN_UART1_IPP_UART_RTS_B_SELECT_INPUT,
	 INPUT_CTL_PATH0,
	 },
	{
	 MX51_PIN_UART1_CTS, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	  PAD_CTL_DRV_HIGH),
	 },
};

void __init mx51_efikamx_init_uart(void)
{
	CONFIG_IOMUX(mx51_efikamx_uart_iomux_pins);
}
