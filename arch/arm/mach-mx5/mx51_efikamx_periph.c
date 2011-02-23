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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/common.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"


#define EFIKAMX_WIFI_POWER	MX51_PIN_EIM_A22
#define EFIKAMX_WIFI_RESET	MX51_PIN_EIM_A16
#define EFIKAMX_BT_POWER	MX51_PIN_EIM_A17

/* schematic confusion, these are defined twice */
#define EFIKASB_BT_POWER	MX51_PIN_EIM_EB2
#define EFIKASB_WIFI_RESET	MX51_PIN_EIM_D24
#define EFIKASB_WIFI_POWER	MX51_PIN_EIM_EB3

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_periph_iomux_pins[] = {
	{ EFIKAMX_WIFI_POWER, IOMUX_CONFIG_GPIO, },
	{ EFIKAMX_WIFI_RESET, IOMUX_CONFIG_GPIO, },
	{ EFIKAMX_BT_POWER, IOMUX_CONFIG_GPIO, },
};

#define EFIKASB_WWAN_WAKEUP	MX51_PIN_CSI1_HSYNC
#define EFIKASB_WWAN_POWER	MX51_PIN_CSI2_D13
#define EFIKASB_WWAN_SIM	MX51_PIN_EIM_CS1
#define EFIKASB_CAMERA_POWER	MX51_PIN_NANDF_CS0

#define SIM_INSERTED	0
#define SIM_MISSING	1

#define WWAN_WAKE	0
#define WWAN_SLEEP	1

static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_periph_iomux_pins[] = {
	{ EFIKASB_WWAN_WAKEUP, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_WWAN_POWER, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_WWAN_SIM, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION, PAD_CTL_PKE_NONE },
	{ EFIKASB_CAMERA_POWER, IOMUX_CONFIG_GPIO, },
};


void mx51_efikamx_wifi_power(int state)
{
	/* active high on smartbook, active low on smarttop */
	if (machine_is_mx51_efikasb())
		state = !state;
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_WIFI_POWER), state);
}

void mx51_efikamx_wifi_reset(void)
{
        msleep(1);
        gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_WIFI_RESET), 0);
        msleep(1);
        gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_WIFI_RESET), 1);
}

void mx51_efikamx_bluetooth_power(int state)
{
	/* BT_PWRON is active high in the schematic (CARD_GPO_OUT on Smarttop) */
	gpio_set_value(IOMUX_TO_GPIO(EFIKAMX_BT_POWER), !state);
	msleep(250);
}

void mx51_efikamx_camera_power(int state)
{
	/* CAM_POWER_ON is active low */
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_CAMERA_POWER), state);
}

void mx51_efikamx_wwan_power(int state)
{
	/* WWAN_PWRON active low */
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), state);
}

int mx51_efikamx_wwan_sim_status(void)
{
	/* SIM_CD active low */
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM));
}

static irqreturn_t mx51_efikamx_sim_handler(int irq, void *dev_id)
{
	int sim = mx51_efikamx_wwan_sim_status();
	if(sim == SIM_INSERTED) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		mx51_efikamx_wwan_power(POWER_ON)
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		mx51_efikamx_wwan_power(POWER_OFF)
	}

	DBG(("SIM card %s\n", (sim == SIM_INSERTED) ? "inserted" : "removed");

	return IRQ_HANDLED;
}

int mx51_efikamx_wwan_wakeup_status(void)
{
	/* active low */
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP));
}

static irqreturn_t mx51_efikamx_wwan_wakeup(int irq, void *dev_id)
{
	if(mx51_efikamx_wwan_wakeup_status())
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	return IRQ_HANDLED;
}



void __init mx51_efikamx_init_periph(void)
{
	int irq, ret;

	CONFIG_IOMUX(mx51_efikamx_periph_iomux_pins);

	/* active high on smartbook, low on smarttop. power off to start */
	gpio_free(IOMUX_TO_GPIO(EFIKAMX_WIFI_POWER));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_WIFI_POWER), "wifi:power");
	if (machine_is_mx51_efikasb())
		gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_WIFI_POWER), 1);
	else
		gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_WIFI_POWER), 0);

	gpio_free(IOMUX_TO_GPIO(EFIKAMX_WIFI_RESET));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_WIFI_RESET), "wifi:reset");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_WIFI_RESET), 0);

	gpio_free(IOMUX_TO_GPIO(EFIKAMX_BT_POWER));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_BT_POWER), "bluetooth:power");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKAMX_BT_POWER), 1);

	if (machine_is_mx51_efikasb()) {

		CONFIG_IOMUX(mx51_efikasb_periph_iomux_pins);

		gpio_free(IOMUX_TO_GPIO(EFIKASB_CAMERA_POWER));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_CAMERA_POWER), "camera:power");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_CAMERA_POWER), 1);

		gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP), "wwan:wakeup");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), "wwan:power");
		gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), 1);

		gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM), "wwan:simcard");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM));

		irq = IOMUX_TO_IRQ(EFIKASB_WWAN_WAKEUP);

		if (mx51_efikamx_wwan_wakeup_status())
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		else
			set_irq_type(irq, IRQF_TRIGGER_RISING);

		ret = request_irq(irq, mx51_efikamx_wwan_wakeup, 0, "wwan:wakeup", 0);
		if(!ret)
			enable_irq_wake(irq);

		irq = IOMUX_TO_IRQ(EFIKASB_WWAN_SIM);

		if (mx51_efikamx_wwan_sim_status()) /* ron: low active */
			set_irq_type(irq, IRQF_TRIGGER_RISING);
		else
			set_irq_type(irq, IRQF_TRIGGER_FALLING);

		ret = request_irq(irq, mx51_efikamx_sim_handler, 0, "wwan:simcard", 0);
	}
}
