
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <asm/io.h>
#include "mx51_efikasb.h"
#include "mx51_pins.h"
#include "iomux.h"

extern int mxc_get_power_status(iomux_pin_name_t pin);
extern void mxc_power_on_wlan(int on);
extern void mxc_power_on_wwan(int on);
extern void mxc_power_on_bt(int on);
extern void mxc_power_on_camera(int on);
extern void mxc_power_on_agps(int on);
extern void mxc_reset_agps(void);

extern int suspend_device_by_name(char *dev_name, pm_message_t state);
extern int resume_device_by_name(char *dev_name, pm_message_t state);

static void suspend_usbh2_wq_handler(struct work_struct *work);

static DECLARE_MUTEX(usbh2_mx);
static DECLARE_DELAYED_WORK(suspend_usbh2_work, suspend_usbh2_wq_handler);

#define BIT_BT_PWRON       1
#define BIT_WWAN_PWRON     2
#define BIT_CAM_PWRON      3 

extern int wireless_sw_state;

static int usbh2_dev_power = 0;
static int usbh2_is_suspend = 0;

static void suspend_usbh2_wq_handler(struct work_struct *work)
{
	int error;

	down(&usbh2_mx);

	if (usbh2_dev_power == 0) {
		error = suspend_device_by_name("fsl-ehci.1", PMSG_SUSPEND);
		if (error == 0)
			usbh2_is_suspend = 1;
	};

	up(&usbh2_mx);
}

static ssize_t wlan_pwr_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val;
	
	val = mxc_get_power_status(WLAN_PWRON_PIN);
	return sprintf(buf, "%s\n", val ? "on" : "off");
}

static ssize_t wlan_pwr_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int val;

	if (strncmp(buf, "on", 2) == 0) {
		val = 1;
	} else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	mxc_power_on_wlan(val);

	return count;
}

static ssize_t wwan_pwr_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val;
	
	val = mxc_get_power_status(WWAN_PWRON_PIN);
	return sprintf(buf, "%s\n", val ? "on" : "off");
}

static ssize_t wwan_pwr_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
        int old_usbh2_dev_power = usbh2_dev_power;
	int error;
	int val;

	if (strncmp(buf, "on", 2) == 0) {
		val = 1;
	} else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	down(&usbh2_mx);

	if (usbh2_dev_power == 0 && val == 1 && usbh2_is_suspend == 1) {
		error = resume_device_by_name("fsl-ehci.1", PMSG_RESUME);
		if (error == 0)
			usbh2_is_suspend = 0;
	}

	mxc_power_on_wwan(val);

	if (val)
		usbh2_dev_power |= (1 << BIT_WWAN_PWRON);
	else
		usbh2_dev_power &= ~(1 << BIT_WWAN_PWRON);

	up(&usbh2_mx);

	printk("usbh2_dev_power=%x\n", usbh2_dev_power);

	if (old_usbh2_dev_power != 0 && usbh2_dev_power == 0)
		schedule_delayed_work(&suspend_usbh2_work, msecs_to_jiffies(4000));

	return count;
}

static ssize_t bt_pwr_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val;
	
	val = mxc_get_power_status(BT_PWRON_PIN);
	return sprintf(buf, "%s\n", val ? "on" : "off");
}

static ssize_t bt_pwr_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int old_usbh2_dev_power = usbh2_dev_power;
	int error;
	int val;

	if (strncmp(buf, "on", 2) == 0) {
		val = 1;
	} else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	down(&usbh2_mx);

	if (usbh2_dev_power == 0 && val == 1 && usbh2_is_suspend == 1) {
		error = resume_device_by_name("fsl-ehci.1", PMSG_RESUME);
		if (error == 0)
			usbh2_is_suspend = 0;
	}

	mxc_power_on_bt(val);

	if (val)
		usbh2_dev_power |= (1 << BIT_BT_PWRON);
	else
		usbh2_dev_power &= ~(1 << BIT_BT_PWRON);

	up(&usbh2_mx);

	printk("usbh2_dev_power=%x\n", usbh2_dev_power);

	if (old_usbh2_dev_power != 0 && usbh2_dev_power == 0)
		schedule_delayed_work(&suspend_usbh2_work, msecs_to_jiffies(4000));

	return count;
}

static ssize_t camera_pwr_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val;
	
	val = mxc_get_power_status(CAM_PWRON_PIN);
	return sprintf(buf, "%s\n", val ? "on" : "off");
}

static ssize_t camera_pwr_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int val;
        int old_usbh2_dev_power = usbh2_dev_power;
        int error;

	if (strncmp(buf, "on", 2) == 0)
		val = 1;
	else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	down(&usbh2_mx);

	if (usbh2_dev_power == 0 && val == 1 && usbh2_is_suspend == 1) {
		error = resume_device_by_name("fsl-ehci.1", PMSG_RESUME);
		if (error == 0)
			usbh2_is_suspend = 0;
	}

	mxc_power_on_camera(val);

	if (val)
		usbh2_dev_power |= (1 << BIT_CAM_PWRON);
	else
		usbh2_dev_power &= ~(1 << BIT_CAM_PWRON);

	up(&usbh2_mx);

	printk("usbh2_dev_power=%x\n", usbh2_dev_power);

	if (old_usbh2_dev_power != 0 && usbh2_dev_power == 0)
		schedule_delayed_work(&suspend_usbh2_work, msecs_to_jiffies(4000));

	return count;
}

static ssize_t agps_pwr_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val;
	
	val = mxc_get_power_status(AGPS_PWRON_PIN);
	return sprintf(buf, "%s\n", val ? "on" : "off");
}

static ssize_t agps_pwr_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int val;

	if (strncmp(buf, "on", 2) == 0)
		val = 1;
	else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	mxc_power_on_agps(val);
	if (val == 1)
		mxc_reset_agps();

	return count;
}

#define DISABLE_POWER_DOWN        0
#define PRECHARGE_POWER_DOWN      1
#define ACTIVE_POWER_DOWN_64      2
#define ACTIVE_POWER_DOWN_128     3

static void mxc_set_ddr2_power_down(int arg)
{
	u32 value;

	value = __raw_readl(IO_ADDRESS(ESDCTL_BASE_ADDR + 0x08));
	value |= (arg & 0x03) << 12;
	__raw_writel(value, IO_ADDRESS(ESDCTL_BASE_ADDR + 0x08));

	value = __raw_readl(IO_ADDRESS(ESDCTL_BASE_ADDR + 0x00));
	value |= (arg & 0x03) << 12;
	__raw_writel(value, IO_ADDRESS(ESDCTL_BASE_ADDR + 0x00));
}

static ssize_t ddr2_pd_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 value;

	value = __raw_readl(IO_ADDRESS(ESDCTL_BASE_ADDR + 0x08));
	value = (value & 0x3000) >> 12;
	
	return sprintf(buf, "%d\n", value);
}
static ssize_t ddr2_pd_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char *end;
	int arg;
	
	arg = simple_strtoul(buf, &end, 10);
	if (arg >= 4 || arg < 0) {
		return -EINVAL;
	}

	mxc_set_ddr2_power_down(arg);

	return count;
}

static struct kobj_attribute wlan_pwr_attribute =
	__ATTR(wlan_power, 0666, wlan_pwr_show, wlan_pwr_store);
static struct kobj_attribute wwan_pwr_attribute =
	__ATTR(wwan_power, 0666, wwan_pwr_show, wwan_pwr_store);
static struct kobj_attribute bt_pwr_attribute =
	__ATTR(bt_power, 0666, bt_pwr_show, bt_pwr_store);
static struct kobj_attribute camera_pwr_attribute =
	__ATTR(camera_power, 0666, camera_pwr_show, camera_pwr_store);
static struct kobj_attribute agps_pwr_attribute =
	__ATTR(agps_power, 0666, agps_pwr_show, agps_pwr_store);
static struct kobj_attribute ddr2_pd_attribute =
	__ATTR(ddr2_pd, 0666, ddr2_pd_show, ddr2_pd_store);

static struct attribute *pwr_attrs[] = {
	&wlan_pwr_attribute.attr,
	&wwan_pwr_attribute.attr,
	&bt_pwr_attribute.attr,
	&camera_pwr_attribute.attr,
	&agps_pwr_attribute.attr,
	&ddr2_pd_attribute.attr,
	NULL,
};

static struct attribute_group pwr_attr_group = {
	.attrs = pwr_attrs,
};

static struct platform_device mxc_pwr_sw_device = {
	.name = "mxc_pwr_sw",
};

static int __init mxc_init_pwr_sw(void)
{
	int retval;
	static struct kobject *pwr_sw_kobj;

	init_MUTEX(&usbh2_mx);

	usbh2_dev_power |= mxc_get_power_status(BT_PWRON_PIN) << BIT_BT_PWRON;
	usbh2_dev_power |= mxc_get_power_status(WWAN_PWRON_PIN) << BIT_WWAN_PWRON;
	usbh2_dev_power |= mxc_get_power_status(CAM_PWRON_PIN) << BIT_CAM_PWRON;
	if (usbh2_dev_power == 0)
		suspend_device_by_name("fsl-ehci.1", PMSG_SUSPEND);

	mxc_set_ddr2_power_down(ACTIVE_POWER_DOWN_64); /* ron: set DDR2 power down */

	platform_device_register(&mxc_pwr_sw_device);

	pwr_sw_kobj = kobject_create_and_add("power_control",
					     &mxc_pwr_sw_device.dev.kobj);
	if (!pwr_sw_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(pwr_sw_kobj, &pwr_attr_group);
	if (retval) {
		kobject_put(pwr_sw_kobj);
		return retval;
	}
	
	return 0;
}

late_initcall(mxc_init_pwr_sw);
