/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */




#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/pci_ids.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>

#include "mx51_efikasb.h"
#include "mx51_pins.h"
#include "iomux.h"

int wireless_sw_state;

extern int mxc_get_wireless_sw_status(void);
extern int mxc_get_sim_card_status(void);

extern void mxc_power_on_wlan(int on);
extern void efikasb_power_on_wwan(int val);
extern void efikasb_power_on_bt(int val);

extern int lid_wake_enable;
extern int mxc_get_lid_sw_status(void);

extern void kernel_power_off(void);

static struct input_dev *efikasb_inputdev;
static int system_is_resuming = 0;


/*!
 * Power Key interrupt handler.
 */
static irqreturn_t power_key_int(int irq, void *dev_id)
{
	static int key_pressed = 0;
	int pk_pressed;

        /* ron: Don't report power key event if system is resuming */
        if(system_is_resuming)
                return IRQ_HANDLED;

	pk_pressed = !gpio_get_value(IOMUX_TO_GPIO(POWER_BTN_PIN));
	if (pk_pressed) {
		if (key_pressed)
			return 0;
		key_pressed = 1;
		pr_info("PWR key pressed\n");
		/* ron: input_report_key(pk_dev, KEY_POWER, pk_pressed); */
		set_irq_type(irq, IRQF_TRIGGER_RISING); /* ron: detect falling edge */

                if(lid_wake_enable)
                        enable_irq_wake(IOMUX_TO_IRQ(LID_SW_PIN));

		input_report_key(efikasb_inputdev, KEY_POWER, 1);
		input_sync(efikasb_inputdev);
	} else {
		if (!key_pressed)
			return 0;
		key_pressed = 0;
		pr_info("PWR Key released\n");
		/* ron: input_report_key(pk_dev, KEY_POWER, pk_released); */
		set_irq_type(irq, IRQF_TRIGGER_FALLING); /* ron: detect falling edge */
		input_report_key(efikasb_inputdev, KEY_POWER, 0);
		input_sync(efikasb_inputdev);
	}

	return IRQ_HANDLED;
}

static int mxc_init_power_key(void)
{
        int ret;
        int irq;

	/* ron: set power key as wakeup source */
	gpio_request(IOMUX_TO_GPIO(POWER_BTN_PIN), "power_btn");
	gpio_direction_input(IOMUX_TO_GPIO(POWER_BTN_PIN));
	irq = IOMUX_TO_IRQ(POWER_BTN_PIN);
	set_irq_type(irq, IRQF_TRIGGER_FALLING); /* ron: detect rising & falling edge */

	ret = request_irq(irq, power_key_int, 0, "power_key", 0);
	if (ret)
		pr_info("register on-off key interrupt failed\n");
	else
		enable_irq_wake(irq);

        return ret;

}

#define KEY_RESUME    KEY_WAKEUP

static int pm_notifier_call(struct notifier_block *nb, unsigned long event, void *dummy)
{
        switch (event) {
        case PM_SUSPEND_PREPARE:
                printk("System Suspending .....\n");
                system_is_resuming = 1;
                return NOTIFY_OK;

        case PM_POST_SUSPEND:
                printk("System Resumed\n");
                system_is_resuming = 0;
                input_event(efikasb_inputdev, EV_PWR, KEY_RESUME, 1);
                input_sync(efikasb_inputdev);

                return NOTIFY_OK;

        }

        return NOTIFY_DONE;
}


static struct notifier_block pm_nb = {
        .notifier_call = pm_notifier_call,
};


static irqreturn_t wireless_sw_int(int irq, void *dev_id)
{
	wireless_sw_state = gpio_get_value(IOMUX_TO_GPIO(WIRELESS_SW_PIN));

	if (wireless_sw_state) {
		pr_info("Wireless SW Off\n");
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		input_report_switch(efikasb_inputdev, SW_RFKILL_ALL, 0);
		input_sync(efikasb_inputdev);
		// power off WLAN, WWAN and BT by H/W
	} else {
		pr_info("Wireless SW On\n");
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		input_report_switch(efikasb_inputdev, SW_RFKILL_ALL, 1);
		input_sync(efikasb_inputdev);
	}

	return IRQ_HANDLED;
}

static int mxc_init_wireless_sw(void)
{
	int irq, ret;

	gpio_request(IOMUX_TO_GPIO(WIRELESS_SW_PIN), "wireless_sw");
	gpio_direction_input(IOMUX_TO_GPIO(WIRELESS_SW_PIN));
	irq = IOMUX_TO_IRQ(WIRELESS_SW_PIN);

	if (gpio_get_value(IOMUX_TO_GPIO(WIRELESS_SW_PIN)))
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else 
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	ret = request_irq(irq, wireless_sw_int, 0, "wireless-sw", 0);

	if (ret)
		pr_info("register wireless switch interrupt failed\n");

	wireless_sw_state = gpio_get_value(IOMUX_TO_GPIO(WIRELESS_SW_PIN));

	return ret;

}

static irqreturn_t sim_detect_int(int irq, void *dev_id)
{
	if(mxc_get_sim_card_status()) { /* ron: low active */
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		pr_info("SIM card inserted\n");
	} else {
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		pr_info("SIM card removed\n");
	}

	return IRQ_HANDLED;
}

static int mxc_init_sim_detect(void)
{
	int irq, ret;

	irq = IOMUX_TO_IRQ(SIM_CD_PIN);

	if(mxc_get_sim_card_status()) /* ron: low active */
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	else
		set_irq_type(irq, IRQF_TRIGGER_FALLING);

	ret = request_irq(irq, sim_detect_int, 0, "sim-detect", 0);
	if(ret)
		pr_info("register SIM card detect interrupt failed\n");

	return ret;
}

static ssize_t sim_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", mxc_get_sim_card_status());
}

static ssize_t wireless_sw_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", mxc_get_wireless_sw_status());
}

static struct kobj_attribute sim_status_attribute =
        __ATTR(sim, S_IFREG | S_IRUGO, sim_status_show, NULL);
static struct kobj_attribute wireless_sw_status_attribute =
        __ATTR(wireless_sw, S_IFREG | S_IRUGO, wireless_sw_status_show, NULL);

static struct attribute *status_attrs[] = {
        &sim_status_attribute.attr,
        &wireless_sw_status_attribute.attr,
        NULL,
};

static struct attribute_group status_attr_group = {
        .attrs = status_attrs,
};

static struct platform_device mxc_efikasb_input_dev = {
        .name = "efikasb_input",
};

static int __init mxc_init_efikasb_inputdev(void)
{
	int ret;
        struct kobject *efikasb_input_kobj;

        platform_device_register(&mxc_efikasb_input_dev);

        efikasb_input_kobj = kobject_create_and_add("status", &mxc_efikasb_input_dev.dev.kobj);
        if(!efikasb_input_kobj)
                return -ENOMEM;

        ret = sysfs_create_group(efikasb_input_kobj, &status_attr_group);
        if(ret) {
                kobject_put(efikasb_input_kobj);
                return ret;
        }

	efikasb_inputdev = input_allocate_device();
	if (!efikasb_inputdev) {
		pr_err("Failed to allocate hotkey input device\n");
		return -ENOMEM;
	}
	efikasb_inputdev->name = "Genesi Efika MX Smartbook Extra Buttons";
	efikasb_inputdev->phys = "genesi-efikasb/input0";
	efikasb_inputdev->uniq = "genesi-efikasb";
	efikasb_inputdev->id.bustype = BUS_HOST;
	efikasb_inputdev->id.vendor = PCI_VENDOR_ID_FREESCALE;
	set_bit(KEY_POWER, efikasb_inputdev->keybit);
        set_bit(KEY_SLEEP, efikasb_inputdev->keybit);
        set_bit(KEY_WAKEUP, efikasb_inputdev->keybit);
	set_bit(EV_KEY, efikasb_inputdev->evbit);
        set_bit(EV_PWR, efikasb_inputdev->evbit);

	set_bit(SW_RFKILL_ALL, efikasb_inputdev->swbit);
        if(mxc_get_wireless_sw_status())
                set_bit(SW_RFKILL_ALL, efikasb_inputdev->sw);

 	ret = input_register_device(efikasb_inputdev);
	if (ret) {
		input_free_device(efikasb_inputdev);
		pr_err("Failed to register hotkey input device\n");
                sysfs_remove_group(efikasb_input_kobj, &status_attr_group);
                kobject_put(efikasb_input_kobj);
		return -ENODEV;
	}

        mxc_init_power_key();

        mxc_init_sim_detect();
        mxc_init_wireless_sw();

        register_pm_notifier(&pm_nb);

	return ret;
}

late_initcall(mxc_init_efikasb_inputdev);

