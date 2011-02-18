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

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"


#define EFIKASB_WWAN_WAKEUP	MX51_PIN_CSI1_HSYNC
#define EFIKASB_WWAN_POWER	MX51_PIN_CSI2_D13
#define EFIKASB_WWAN_SIM	MX51_PIN_EIM_CS1

static struct mxc_iomux_pin_cfg __initdata mx51_efikasb_wwan_iomux_pins[] = {
	{ EFIKASB_WWAN_WAKEUP, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_WWAN_POWER, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_WWAN_SIM, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION, PAD_CTL_PKE_NONE },
};



void mx51_efikasb_wwan_power(int state)
{
	gpio_set_value(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), !state);
}




int mx51_efikasb_wwan_sim_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM));
}

static irqreturn_t mx51_efikasb_sim_handler(int irq, void *dev_id)
{
	if(mx51_efikasb_wwan_sim_status()) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	return IRQ_HANDLED;
}




int mx51_efikasb_wwan_wakeup_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP));
}

static irqreturn_t mx51_efikasb_wwan_wakeup(int irq, void *dev_id)
{
	if(mx51_efikasb_wwan_wakeup_status())
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	return IRQ_HANDLED;
}



void __init mx51_efikasb_init_wwan(void)
{
	int irq, ret;

	CONFIG_IOMUX(mx51_efikasb_wwan_iomux_pins);

	gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP), "wwan:wakeup");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_WWAN_WAKEUP));

	gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), "wwan:power");
	gpio_direction_output(IOMUX_TO_GPIO(EFIKASB_WWAN_POWER), 0);

	gpio_request(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM), "wwan:simcard");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_WWAN_SIM));

	irq = IOMUX_TO_IRQ(EFIKASB_WWAN_WAKEUP);

	if (mx51_efikasb_wwan_wakeup_status())
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	ret = request_irq(irq, mx51_efikasb_wwan_wakeup, 0, "wwan-wakeup", 0);
	if(!ret)
		enable_irq_wake(irq);

	irq = IOMUX_TO_IRQ(EFIKASB_WWAN_SIM);

	if (mx51_efikasb_wwan_sim_status()) /* ron: low active */
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	else
		set_irq_type(irq, IRQF_TRIGGER_FALLING);

	ret = request_irq(irq, mx51_efikasb_sim_handler, 0, "sim-detect", 0);

}
