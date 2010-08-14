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

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#define EFIKAMX_BLUE_LED	MX51_PIN_CSI1_D9
#define EFIKAMX_GREEN_LED	MX51_PIN_CSI1_VSYNC
#define EFIKAMX_RED_LED		MX51_PIN_CSI1_HSYNC

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_led_iomux_pins[] = {
	{ EFIKAMX_BLUE_LED, IOMUX_CONFIG_ALT3, },
	{ EFIKAMX_GREEN_LED, IOMUX_CONFIG_ALT3, },
	{ EFIKAMX_RED_LED, IOMUX_CONFIG_ALT3, },

};

#define EFIKAMX_POWER_KEY	MX51_PIN_EIM_DTACK

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_powerkey_iomux_pins[] = {
	{
	 EFIKAMX_POWER_KEY, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU),
	},
};


static struct gpio_led mx51_efikamx_leds[] = {
	{
		.name = "efikamx:green",
		.default_trigger = "default-on",
		.gpio = IOMUX_TO_GPIO(EFIKAMX_GREEN_LED),
	},
	{
		.name = "efikamx:red",
		.default_trigger = "ide-disk",
		.gpio = IOMUX_TO_GPIO(EFIKAMX_RED_LED),
	},
	{
		.name = "efikamx:blue",
		.default_trigger = "mmc1",
		.gpio = IOMUX_TO_GPIO(EFIKAMX_BLUE_LED),
	},
};

static struct gpio_led_platform_data mx51_efikamx_leds_data = {
	.leds = mx51_efikamx_leds,
	.num_leds = ARRAY_SIZE(mx51_efikamx_leds),
};

static struct platform_device mx51_efikamx_leds_device = {
	.name = "leds-gpio",
	.id = -1,
	.dev = {
		.platform_data = &mx51_efikamx_leds_data,
	},
};




static struct gpio_keys_button mx51_efikamx_powerkey[] = {
	{
		.code	= KEY_POWER,
		.gpio	= IOMUX_TO_GPIO(EFIKAMX_POWER_KEY),
		.type	= EV_PWR,
		.desc	= "Power Button (CM)",
		.wakeup = 1,
		.debounce_interval = 10, /* ms */
	},
};

static struct gpio_keys_platform_data mx51_efikamx_powerkey_data = {
	.buttons = mx51_efikamx_powerkey,
	.nbuttons = ARRAY_SIZE(mx51_efikamx_powerkey),
};


static struct platform_device mx51_efikamx_powerkey_device = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.platform_data = &mx51_efikamx_powerkey_data,
		},
};



void mx51_efikamx_init_leds(void)
{
	DBG(("IOMUX for LED (%d pins)\n", ARRAY_SIZE(mx51_efikamx_led_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_led_iomux_pins);

	/*
		Request each color and set a default output.
		Since Green is our power light by default, turn it on.
		Since Red & Blue are disk activity, turn them off (the trigger will activate it)
	*/
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_BLUE_LED), "blue_led");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_BLUE_LED), 0);
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_BLUE_LED));

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_GREEN_LED), "green_led");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_GREEN_LED), 1);
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_GREEN_LED));

	gpio_request(IOMUX_TO_GPIO(EFIKAMX_RED_LED), "red_led");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_RED_LED), 0);
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_RED_LED));

	if (mx51_efikamx_revision() >= 2)
	{
		/* on 1.2 and above there is no second SD controller,
		 * so the previous external "mmc1" is not valid anymore.
		 * change the trigger to mmc0 to activate the LED for
		 * the external slot for the newer boards
		 */
		mx51_efikamx_leds[2].default_trigger = "mmc0";
	}

	(void)platform_device_register(&mx51_efikamx_leds_device);

	/*
		Register power key (since it's got the LEDs in it :)
	*/

	DBG(("IOMUX for Power Key (%d pins)\n", ARRAY_SIZE(mx51_efikamx_powerkey_iomux_pins)));
	CONFIG_IOMUX(mx51_efikamx_powerkey_iomux_pins);

	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));
	(void)platform_device_register(&mx51_efikamx_powerkey_device);
}
