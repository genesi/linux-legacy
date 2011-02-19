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
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/common.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#define POWER_ON	1
#define POWER_OFF	0

#define EFIKASB_LID_SWITCH	MX51_PIN_CSI1_VSYNC
#define EFIKASB_RFKILL_SWITCH	MX51_PIN_DI1_PIN12

struct mxc_iomux_pin_cfg __initdata mx51_efikasb_input_iomux_pins[] = {
	{ EFIKASB_LID_SWITCH, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_RFKILL_SWITCH, IOMUX_CONFIG_GPIO, },
};


#define EFIKAMX_POWER_KEY	MX51_PIN_EIM_DTACK

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_input_iomux_pins[] = {
	{
	 EFIKAMX_POWER_KEY, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU),
	},
};

static struct gpio_keys_button mx51_efikamx_powerkey[] = {
	{
		.code	= KEY_POWER,
		.gpio	= IOMUX_TO_GPIO(EFIKAMX_POWER_KEY),
		.type	= EV_KEY,
		.desc	= "Power Button",
		.wakeup = 1,
		.debounce_interval = 10, /* ms */
	},
};

static struct gpio_keys_platform_data mx51_efikamx_powerkey_data = {
	.buttons = mx51_efikamx_powerkey,
	.nbuttons = ARRAY_SIZE(mx51_efikamx_powerkey),
};


static struct platform_device mx51_efikamx_input_device = {
	.name = "gpio-keys",
	.id = 0,
	.dev = {
		.platform_data = &mx51_efikamx_powerkey_data,
		},
};


int mx51_efikasb_rfkill_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));
}

static struct gpio_keys_button mx51_efikasb_input[] = {
	{
		.code = SW_LID,
		.gpio = IOMUX_TO_GPIO(EFIKASB_LID_SWITCH),
		.type = EV_SW,
		.desc = "Lid Switch",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 10, /* ms */
	},
	{
		.code = SW_RFKILL_ALL,
		.gpio = IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH),
		.type = EV_SW,
		.desc = "Wifi Switch",
		.active_low = 1,
		.debounce_interval = 10,
	},
};

static struct gpio_keys_platform_data mx51_efikasb_input_data = {
	.buttons = mx51_efikasb_input,
	.nbuttons = ARRAY_SIZE(mx51_efikasb_input),
};


static struct platform_device mx51_efikasb_input_device = {
	.name = "gpio-keys",
	.id = 1,
	.dev = {
		.platform_data = &mx51_efikasb_input_data,
		},
};


void __init mx51_efikamx_init_input(void)
{
	CONFIG_IOMUX(mx51_efikamx_input_iomux_pins);

	gpio_free(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY), "key:power");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));

	(void)platform_device_register(&mx51_efikamx_input_device);

	if (machine_is_mx51_efikasb()) {

		CONFIG_IOMUX(mx51_efikasb_input_iomux_pins);

		gpio_free(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH), "switch:lid");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH));

		gpio_free(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH), "switch:rfkill");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));

		(void)platform_device_register(&mx51_efikasb_input_device);
	}
}
