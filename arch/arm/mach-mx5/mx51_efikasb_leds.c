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
#include <linux/leds_pwm.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/common.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#define EFIKASB_CAPSLOCK_LED		MX51_PIN_EIM_CS0
#define EFIKASB_ALARM_LED		MX51_PIN_GPIO1_3
#define EFIKASB_PWM_BACKLIGHT		MX51_PIN_GPIO1_2
#define EFIKASB_PWM_BACKLIGHT_EN	MX51_PIN_CSI2_D19


static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_led_iomux_pins[] = {
	{ EFIKASB_CAPSLOCK_LED, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_ALARM_LED, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_PWM_BACKLIGHT, IOMUX_CONFIG_ALT1 },
	{ EFIKASB_PWM_BACKLIGHT_EN, IOMUX_CONFIG_GPIO },
};

static struct gpio_led mx51_efikasb_leds[] = {
	{
		.name = "led:capslock",
		.default_trigger = "capslock",
		.gpio = IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED),
	},
	{
		.name = "led:alarm", /* remind me to update the installer script */
		.default_trigger = "ide-disk",
		.active_low = 1,
		.gpio = IOMUX_TO_GPIO(EFIKASB_ALARM_LED),
	},
};

static struct led_pwm mx51_efikasb_pwm_backlight[] = {
	{
	.name = "backlight",
	.default_trigger = "backlight",
	.pwm_id = 0,
	.max_brightness = 64,
	.default_brightness = 32,
	.pwm_period_ns = 78770,
	},
};

static struct led_pwm_platform_data mx51_efikasb_backlight_data = {
	.num_leds = ARRAY_SIZE(mx51_efikasb_pwm_backlight),
	.leds = mx51_efikasb_pwm_backlight,
};

static struct platform_device mx51_efikasb_backlight_device = {
	.name = "leds_pwm",
	.id = -1,
	.dev = {
		.platform_data = &mx51_efikasb_backlight_data,
	},
};

static struct gpio_led_platform_data mx51_efikasb_leds_data = {
	.leds = mx51_efikasb_leds,
	.num_leds = ARRAY_SIZE(mx51_efikasb_leds),
};

static struct platform_device mx51_efikasb_leds_device = {
	.name = "leds-gpio",
	.id = -1,
	.dev = {
		.platform_data = &mx51_efikasb_leds_data,
	},
};

void __init mx51_efikasb_init_leds(void)
{
	CONFIG_IOMUX(mx51_efikasb_led_iomux_pins);

	/*
		Request each color and set a default output.
		Since Green is our power light by default, turn it on.
		Since Red & Blue are disk activity, turn them off (the trigger will activate it)
	*/
	gpio_request(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED), "led:capslock");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED), 0);
	gpio_free(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED));

	gpio_request(IOMUX_TO_GPIO(EFIKASB_ALARM_LED), "led:alarm");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_ALARM_LED), 1);
	gpio_free(IOMUX_TO_GPIO(EFIKASB_ALARM_LED));

	gpio_request(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), "backlight:en#");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), 0);

	mxc_register_device(&mxc_pwm1_device, NULL);
	(void)platform_device_register(&mx51_efikasb_backlight_device);
	(void)platform_device_register(&mx51_efikasb_leds_device);
}
