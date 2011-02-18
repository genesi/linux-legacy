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
#include <linux/pwm_backlight.h>
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

#if defined(CONFIG_BACKLIGHT_PWM) && defined(CONFIG_LEDS_PWM)
#error pick CONFIG_BACKLIGHT_PWM or CONFIG_LEDS_PWM but not both, please..
#endif

#if defined(CONFIG_LEDS_PWM) && !defined(CONFIG_LEDS_TRIGGER_BACKLIGHT)
#warning using the PWM led driver but not the backlight trigger, backlight control will be manual!!
#endif


#define EFIKAMX_BLUE_LED	MX51_PIN_CSI1_D9
#define EFIKAMX_GREEN_LED	MX51_PIN_CSI1_VSYNC
#define EFIKAMX_RED_LED		MX51_PIN_CSI1_HSYNC

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_led_iomux_pins[] = {
	{ EFIKAMX_BLUE_LED, IOMUX_CONFIG_GPIO, },
	{ EFIKAMX_GREEN_LED, IOMUX_CONFIG_GPIO, },
	{ EFIKAMX_RED_LED, IOMUX_CONFIG_GPIO, },

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

#if defined(CONFIG_BACKLIGHT_PWM)
static void mx51_efikasb_backlight_power(int on)
{
        if (on) {
                mxc_free_iomux(EFIKASB_PWM_BACKLIGHT, IOMUX_CONFIG_GPIO);
                mxc_request_iomux(EFIKASB_PWM_BACKLIGHT, IOMUX_CONFIG_ALT1);

                msleep(10);

                gpio_set_value(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), 0);     /* Backlight Power On */
        } else {
                gpio_set_value(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), 1);     /* Backlight Power Off */

                msleep(10);

                mxc_free_iomux(EFIKASB_PWM_BACKLIGHT, IOMUX_CONFIG_ALT1);
                mxc_request_iomux(EFIKASB_PWM_BACKLIGHT, IOMUX_CONFIG_GPIO);
                gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT), 0);
        }
}

static struct platform_pwm_backlight_data mx51_efikasb_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 64,
	.dft_brightness = 32,
	.pwm_period_ns = 78770,
	.power = mx51_efikasb_backlight_power,
};

#elif defined(CONFIG_LEDS_PWM)
static struct led_pwm mx51_efikasb_pwm_backlight[] = {
	{
	.name = "led:backlight",
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
#endif

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


void __init mx51_efikamx_init_leds(void)
{

	if (machine_is_mx51_efikamx()) {
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
	} else {
		CONFIG_IOMUX(mx51_efikasb_led_iomux_pins);

		gpio_request(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED), "led:capslock");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED), 0);
		gpio_free(IOMUX_TO_GPIO(EFIKASB_CAPSLOCK_LED));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_ALARM_LED), "led:alarm");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_ALARM_LED), 1);
		gpio_free(IOMUX_TO_GPIO(EFIKASB_ALARM_LED));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), "backlight:en#");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_PWM_BACKLIGHT_EN), 0);

		mxc_register_device(&mxc_pwm1_device, NULL);
		#if defined(CONFIG_BACKLIGHT_PWM)
		mxc_register_device(&mxc_pwm_backlight_device, &mx51_efikasb_backlight_data);
		#elif defined(CONFIG_LEDS_PWM)
		(void)platform_device_register(&mx51_efikasb_backlight_device);
		#endif
		(void)platform_device_register(&mx51_efikasb_leds_device);
	}
}







