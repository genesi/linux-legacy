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
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci_ids.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

#define EFIKASB_LID_SWITCH	MX51_PIN_CSI1_VSYNC
#define EFIKASB_RFKILL_SWITCH	MX51_PIN_DI1_PIN12
#define EFIKASB_BATTERY_LOW	MX51_PIN_DI1_PIN11
#define EFIKASB_BATTERY_INSERT	MX51_PIN_DISPB2_SER_DIO
#define EFIKASB_AC_INSERT	MX51_PIN_DI1_D0_CS

/* active low just like on the board */
#define LID_CLOSED	0
#define WIFI_ON		0
#define BATTERY_IN	0
#define BATTERY_LOW	0
#define AC_IN		0

struct mxc_iomux_pin_cfg __initdata mx51_efikasb_input_iomux_pins[] = {
	{ EFIKASB_LID_SWITCH, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_RFKILL_SWITCH, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_BATTERY_INSERT, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_BATTERY_LOW, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION, },
	{ EFIKASB_AC_INSERT, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION,
/*		IOMUXC_GPIO3_IPP_IND_G_3_SELECT_INPUT, INPUT_CTL_PATH1, */
	},
};

int mx51_efikasb_battery_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT));
}

int mx51_efikasb_battery_alarm(void)
{
        return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW));
}

int mx51_efikasb_ac_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_AC_INSERT));
}

int mx51_efikasb_rfkill_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));
}

int mx51_efikasb_lid_status(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH));
}



#define EFIKAMX_POWER_KEY	MX51_PIN_EIM_DTACK

struct mxc_iomux_pin_cfg __initdata mx51_efikamx_input_iomux_pins[] = {
	{
	 EFIKAMX_POWER_KEY, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_100K_PU),
	},
};

int mx51_efikamx_powerkey_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));
}

static struct input_dev *input;
int system_is_suspending = 0;

static int mx51_efikamx_pm_notifier(struct notifier_block *nb, unsigned long event, void *dummy)
{
	switch (event) {
		case PM_SUSPEND_PREPARE:
			system_is_suspending = 1;
			return NOTIFY_OK;

		case PM_POST_SUSPEND:
			system_is_suspending = 0;
			input_event(input, EV_PWR, KEY_WAKEUP, 1);
			input_sync(input);
			return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block pm_notifier = {
	.notifier_call = mx51_efikamx_pm_notifier,
};

static irqreturn_t mx51_efikamx_powerkey_handler(int irq, void *dev_id)
{
	int pressed = mx51_efikamx_powerkey_status();
	static unsigned int press_count = 0;
	static unsigned long press_time = 0;

	if (system_is_suspending)
		return IRQ_HANDLED;

	if (pressed) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);

		if ( (jiffies - press_time) < HZ )
			press_count++;
		else
			press_count = 0;

		press_time = jiffies;
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	DBG(("Power Key %s %u\n", pressed ? "pressed" : "released", press_count));

	input_event(input, EV_KEY, KEY_POWER, pressed);
	input_sync(input);

	if (press_count >= 4) {
		/* clever shutdown code here */
	}

	return IRQ_HANDLED;
}

static irqreturn_t mx51_efikasb_rfkill_handler(int irq, void *dev_id)
{
	int wifi = mx51_efikasb_rfkill_status();

	if (wifi == WIFI_ON) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	DBG(("Wireless Switch %s\n", wifi ? "on" : "off"));

	input_event(input, EV_SW, SW_RFKILL_ALL, (wifi == WIFI_ON));
	input_sync(input);

	return IRQ_HANDLED;
}

static irqreturn_t mx51_efikasb_lid_handler(int irq, void *dev_id)
{
	int lid = mx51_efikasb_lid_status();

	if (lid == LID_CLOSED) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		enable_irq_wake(irq);
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		disable_irq_wake(irq);
	}

	DBG(("Lid %s\n", lid ? "closed" : "open"));

	input_event(input, EV_SW, SW_LID, (lid == LID_CLOSED));
	input_sync(input);

	return IRQ_HANDLED;
}

static irqreturn_t mx51_efikasb_battery_handler(int irq, void *dev_id)
{
	int battery = mx51_efikasb_battery_status();

	if (battery == BATTERY_IN) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	DBG(("Battery %s\n", (battery == BATTERY_IN) ? "inserted" : "not present"));

	input_event(input, EV_SW, SW_BATTERY_INSERT, (battery == BATTERY_IN));
	input_sync(input);

	return IRQ_HANDLED;
}

static irqreturn_t mx51_efikasb_alarm_handler(int irq, void *dev_id)
{
	int alarm = mx51_efikasb_battery_alarm();

	if (alarm == BATTERY_LOW) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		DBG(("Battery Low!"));
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	input_event(input, EV_SW, SW_BATTERY_LOW, (alarm == BATTERY_LOW));
	input_sync(input);

	return IRQ_HANDLED;
}

static irqreturn_t mx51_efikasb_ac_handler(int irq, void *dev_id)
{
	int ac = mx51_efikasb_ac_status();

	if (ac == AC_IN) {
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	DBG(("AC Adapter %s\n", (ac == AC_IN) ? "inserted" : "not present"));

	input_event(input, EV_SW, SW_AC_INSERT, (ac == AC_IN));
	input_sync(input);

	return IRQ_HANDLED;
}


/* TODO: split this up, they shouldn't fail all as one big thing */
static int __init mx51_efikamx_init_input(void)
{
	int ret, irq;

	CONFIG_IOMUX(mx51_efikamx_input_iomux_pins);

	gpio_free(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));
	gpio_request(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY), "key:power");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKAMX_POWER_KEY));

	if (machine_is_mx51_efikasb()) {

		CONFIG_IOMUX(mx51_efikasb_input_iomux_pins);

		gpio_free(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH), "switch:lid");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_LID_SWITCH));

		gpio_free(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));
		gpio_request(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH), "switch:rfkill");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_RFKILL_SWITCH));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT), "battery:insert");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT));

		gpio_request(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW), "battery:alarm");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW));

		/* IOMUXC_GPIO3_IPP_IND_G_IN_3_SELECT_INPUT: 1: Selecting Pad DI1_D0_CS for Mode:ALT4 */
		__raw_writel(0x01, IO_ADDRESS(IOMUXC_BASE_ADDR) + 0x980);
		gpio_request(IOMUX_TO_GPIO(EFIKASB_AC_INSERT), "battery:ac");
		gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_AC_INSERT));
	}

	input = input_allocate_device();
	if (!input) {
		DBG(("Failed to allocate Input Device\n"));
		return -ENOMEM;
	}

	input->name = "Efika MX Input";
	input->phys = "efikamx/input0";
	input->uniq = "efikamx";
	input->id.bustype = BUS_HOST;
	input->id.vendor = PCI_VENDOR_ID_FREESCALE;

	set_bit(EV_KEY, input->evbit);
	set_bit(EV_PWR, input->evbit);

	set_bit(KEY_POWER, input->keybit);
	set_bit(KEY_SLEEP, input->keybit);
	set_bit(KEY_WAKEUP, input->keybit);

	if (machine_is_mx51_efikasb()) {
		set_bit(EV_SW, input->evbit);

		set_bit(SW_LID, input->swbit);
		set_bit(SW_RFKILL_ALL, input->swbit);
		set_bit(SW_BATTERY_INSERT, input->swbit);
		set_bit(SW_BATTERY_LOW, input->swbit);
		set_bit(SW_AC_INSERT, input->swbit);

		if (mx51_efikasb_lid_status() == LID_CLOSED)
			set_bit(SW_LID, input->sw);

		if (mx51_efikasb_rfkill_status() == WIFI_ON)
			set_bit(SW_RFKILL_ALL, input->sw);

		if (mx51_efikasb_battery_status() == BATTERY_IN)
			set_bit(SW_BATTERY_INSERT, input->sw);

		if (mx51_efikasb_battery_alarm() == BATTERY_LOW)
			set_bit(SW_BATTERY_LOW, input->sw);

		if (mx51_efikasb_ac_status() == AC_IN)
			set_bit(SW_AC_INSERT, input->sw);
	}

	ret = input_register_device(input);
	if (ret) {
		DBG(("Failed to register Input Device\n"));
		goto fail_input;
	}

	irq = IOMUX_TO_IRQ(EFIKAMX_POWER_KEY);
	set_irq_type(irq, IRQF_TRIGGER_FALLING);
	enable_irq_wake(irq);

	ret = request_irq(irq, mx51_efikamx_powerkey_handler, 0, "key:power", NULL);
	if (ret) {
		DBG(("Failed to request Power Key interrupt\n"));
		goto fail_power_irq;
	}

	if (machine_is_mx51_efikasb()) {
		irq = IOMUX_TO_IRQ(EFIKASB_LID_SWITCH);
		if (mx51_efikasb_lid_status() == LID_CLOSED) {
			set_irq_type(irq, IRQF_TRIGGER_RISING);
			enable_irq_wake(irq);
		} else {
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		}

		ret = request_irq(irq, mx51_efikasb_lid_handler, 0, "switch:lid", NULL);
		if (ret) {
			DBG(("Failed to request Lid Switch interrupt\n"));
			goto fail_lid_irq;
		}

		irq = IOMUX_TO_IRQ(EFIKASB_RFKILL_SWITCH);
		if (mx51_efikasb_rfkill_status() == WIFI_ON) {
			set_irq_type(irq, IRQF_TRIGGER_RISING);
		} else {
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		}

		ret = request_irq(irq, mx51_efikasb_rfkill_handler, 0, "switch:rfkill", NULL);
		if (ret) {
			DBG(("Failed to request Wireless Switch interrupt\n"));
			goto fail_rfkill_irq;
		}

		irq = IOMUX_TO_IRQ(EFIKASB_BATTERY_INSERT);
		if (mx51_efikasb_battery_status() == BATTERY_IN) {
			set_irq_type(irq, IRQF_TRIGGER_RISING);
		} else {
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		}

		ret = request_irq(irq, mx51_efikasb_battery_handler, 0, "battery:insert", NULL);
		if (ret) {
			DBG(("Failed to request Battery Insertion interrupt\n"));
			goto fail_batt_irq;
		}

		irq = IOMUX_TO_IRQ(EFIKASB_AC_INSERT);
		if (mx51_efikasb_ac_status() == AC_IN) {
			set_irq_type(irq, IRQF_TRIGGER_RISING);
		} else {
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		}

		ret = request_irq(irq, mx51_efikasb_ac_handler, 0, "battery:ac", NULL);
		if (ret) {
			DBG(("Failed to request AC Adapter Insertion interrupt\n"));
			goto fail_ac_irq;
		}

		irq = IOMUX_TO_IRQ(EFIKASB_BATTERY_LOW);
		if (mx51_efikasb_battery_alarm() == BATTERY_LOW) {
			set_irq_type(irq, IRQF_TRIGGER_RISING);
		} else {
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
		}

		ret = request_irq(irq, mx51_efikasb_alarm_handler, 0, "battery:alarm", NULL);
		if (ret) {
			DBG(("Failed to request Battery Low interrupt\n"));
			goto fail_alarm_irq;
		}


	}

	register_pm_notifier(&pm_notifier);

	return 0;

fail_alarm_irq:
	free_irq(IOMUX_TO_IRQ(EFIKASB_AC_INSERT), NULL);
fail_ac_irq:
	free_irq(IOMUX_TO_IRQ(EFIKASB_BATTERY_INSERT), NULL);
fail_batt_irq:
	free_irq(IOMUX_TO_IRQ(EFIKASB_RFKILL_SWITCH), NULL);
fail_rfkill_irq:
	free_irq(IOMUX_TO_IRQ(EFIKASB_LID_SWITCH), NULL);
fail_lid_irq:
	free_irq(IOMUX_TO_IRQ(EFIKAMX_POWER_KEY), NULL);
fail_power_irq:
	input_unregister_device(input);
fail_input:
	input_free_device(input);

	return -ENODEV;
}

late_initcall(mx51_efikamx_init_input);
