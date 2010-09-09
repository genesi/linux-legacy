/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/pci_ids.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <asm/setup.h>

#include "mx51_efikasb.h"
#include "mx51_pins.h"
#include "iomux.h"

int lid_wake_enable = 0;
extern int mxc_get_lid_sw_status(void);
extern void mxc_reset_idle_timer(void);

static struct input_dev *efikasb_lid_inputdev;

static struct platform_device efikasb_lid_dev = {
        .name = "efikasb_lid",
};

static irqreturn_t lid_sw_int(int irq, void *dev_id)
{
	int lid_close;

        mxc_reset_idle_timer();
	lid_close = mxc_get_lid_sw_status();
	if(lid_close) {
		pr_info("Lid Switch Close\n");
		set_irq_type(irq, IRQF_TRIGGER_RISING);

                if(lid_wake_enable)
                        enable_irq_wake(irq);

		input_report_switch(efikasb_lid_inputdev, SW_LID, lid_close);
		input_sync(efikasb_lid_inputdev);

	} else {
		pr_info("Lid Switch Open\n");
		set_irq_type(irq, IRQF_TRIGGER_FALLING);

		input_report_switch(efikasb_lid_inputdev, SW_LID, lid_close);
		input_sync(efikasb_lid_inputdev);

                if(lid_wake_enable)
                        disable_irq_wake(irq);
	}

	return IRQ_HANDLED;
}

static int mxc_init_lid_sw(void)
{
	int irq, ret;

	gpio_request(IOMUX_TO_GPIO(LID_SW_PIN), "lid_sw");
	gpio_direction_input(IOMUX_TO_GPIO(LID_SW_PIN));
	irq = IOMUX_TO_IRQ(LID_SW_PIN);

	if(mxc_get_lid_sw_status()) {
		pr_info("Lid Switch Close\n");
		set_irq_type(irq, IRQF_TRIGGER_RISING);
	} else {
		pr_info("Lid Switch Open\n");
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	ret = request_irq(irq, lid_sw_int, 0, "lid-sw", 0);
	if(ret) 
		pr_info("register lid switch interrupt failed\n");

	return ret;
}

static ssize_t lid_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", mxc_get_lid_sw_status());
}

static ssize_t lid_wake_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%s\n", lid_wake_enable ? "on": "off");
}

static ssize_t lid_wake_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
        if(strncmp(buf, "on", 2) == 0)
                lid_wake_enable = 1;
        else if (strncmp(buf, "off", 3) == 0)
                lid_wake_enable = 0;
        else
                return -EINVAL;

        return count;
}

static struct kobj_attribute lid_status_attribute =
        __ATTR(lid, S_IFREG | S_IRUGO, lid_status_show, NULL);
static struct kobj_attribute lid_wake_attribute =
        __ATTR(lid_wake, 0666, lid_wake_show, lid_wake_store);

static struct attribute *status_attrs[] = {
        &lid_status_attribute.attr,
        &lid_wake_attribute.attr,
        NULL,
};

static struct attribute_group status_attr_group = {
        .attrs = status_attrs,
};

static void __init lid_wake_setup(char **p)
{
        if(memcmp(*p, "on", 2) == 0) {
                lid_wake_enable = 1;
                *p += 2;
        } else if(memcmp(*p, "off", 3) == 0) {
                lid_wake_enable = 0;
                *p += 3;
        }
}

__early_param("lid_wake=", lid_wake_setup);

static int __init mxc_init_efikasb_lid(void)
{
        int ret ;
        struct kobject *lid_kobj;

        platform_device_register(&efikasb_lid_dev);

        lid_kobj = kobject_create_and_add("status", &efikasb_lid_dev.dev.kobj);
        if(!lid_kobj) {
                ret = -ENOMEM;
                goto err3;
        }

        ret = sysfs_create_group(lid_kobj, &status_attr_group);
        if(ret) {
                goto err2;
        }

        efikasb_lid_inputdev = input_allocate_device();
        if(!efikasb_lid_inputdev) {
                pr_err("Failed to allocate lid input device\n");
                ret = -ENOMEM;
                goto err2;
        }

        efikasb_lid_inputdev->name = "Efika MX Smartbook Lid Switch";
        efikasb_lid_inputdev->phys = "efikasb/input1";
        efikasb_lid_inputdev->uniq = "efikasb";
        efikasb_lid_inputdev->id.bustype = BUS_HOST;
        efikasb_lid_inputdev->id.vendor = PCI_VENDOR_ID_FREESCALE;

        set_bit(EV_SW, efikasb_lid_inputdev->evbit);
        set_bit(SW_LID, efikasb_lid_inputdev->swbit);

        /* ron: 0:open 1:close */
        if(mxc_get_lid_sw_status())
                set_bit(SW_LID, efikasb_lid_inputdev->sw);

        ret = input_register_device(efikasb_lid_inputdev);
        if(ret) {
                pr_err("Failed to register Efika MX Smartbook lid input device\n");
                ret = -ENODEV;
                goto err1;
        }

        mxc_init_lid_sw();

        return ret;

 err1:
        input_free_device(efikasb_lid_inputdev);
 err2:
        kobject_put(lid_kobj);
 err3:
        platform_device_unregister(&efikasb_lid_dev);

        return ret;
}

late_initcall(mxc_init_efikasb_lid);
