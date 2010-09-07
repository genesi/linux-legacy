/*
 * Copyright 2009 Pegatron Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

extern void mxc_turn_on_alarm_led(int on);

static void msg_alarm_led_set(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	mxc_turn_on_alarm_led(value ? 1 : 0);
}

static struct led_classdev msg_alarm_led = {
	.name = "msg-alarm-led",
	.brightness_set = msg_alarm_led_set,
};

static int efikasb_led_remove(struct platform_device *dev)
{

	led_classdev_unregister(&msg_alarm_led);

	return 0;
}

static int efikasb_led_probe(struct platform_device *dev)
{
	int ret;
	
	ret = led_classdev_register(&dev->dev, &msg_alarm_led);
	if (ret < 0) {
		dev_err(&dev->dev, "Register Message Alarm LED failed\n");
		return -ENODEV;

	}

	printk(KERN_INFO "Registered Efika MX Smartbook Message Alarm LED\n");

	return 0;

}

#ifdef CONFIG_PM
static int efikasb_led_suspend(struct platform_device *dev, pm_message_t state)
{

	led_classdev_suspend(&msg_alarm_led);
	return 0;
}

static int efikasb_led_resume(struct platform_device *dev)
{

	led_classdev_resume(&msg_alarm_led);
	return 0;
}
#else
#define efikasb_led_suspend NULL
#define efikasb_led_resume NULL
#endif

static struct platform_driver efikasb_led_driver = {
	.probe = efikasb_led_probe,
	.remove = efikasb_led_remove,
	.suspend = efikasb_led_suspend,
	.resume = efikasb_led_resume,
	.driver = {
		   .name = "efikasb_leds",
		   .owner = THIS_MODULE,
		   },
};

static int __init efikasb_led_init(void)
{
	return platform_driver_register(&efikasb_led_driver);
}

static void __exit efikasb_led_exit(void)
{
	platform_driver_unregister(&efikasb_led_driver);
}

module_init(efikasb_led_init);
module_exit(efikasb_led_exit);

MODULE_DESCRIPTION("Led driver for Efika MX Smartbook LEDs");
MODULE_AUTHOR("Ron Lee <ron1_lee@pegatroncorp.com> Pegatron Inc.");
MODULE_LICENSE("GPL");
