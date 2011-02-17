/*
 * Backlight emulation LED trigger
 *
 * Copyright 2008 (C) Rodolfo Giometti <giometti@linux.it>
 * Copyright 2008 (C) Eurotech S.p.A. <info@eurotech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include "leds.h"

struct bl_trig_notifier {
	struct led_classdev *led;
	int brightness;
	int old_status;
	struct notifier_block notifier;
};

static void bl_blank(struct bl_trig_notifier *n, int type)
{
	struct led_classdev *led = n->led;

	switch (type) {
		case FB_BLANK_UNBLANK:
			if (n->old_status != FB_BLANK_UNBLANK) {
				led_set_brightness(led, n->brightness);
				n->old_status = type;
			}
		break;
		case FB_BLANK_NORMAL:
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			if (n->old_status == FB_BLANK_UNBLANK) {
				n->brightness = led->brightness;
				led_set_brightness(led, LED_OFF);
				n->old_status = type;
			}
		break;
	}
}

static int fb_notifier_callback(struct notifier_block *p,
				unsigned long event, void *data)
{
	struct bl_trig_notifier *n = container_of(p,
					struct bl_trig_notifier, notifier);
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;

	switch (event) {
	case FB_EVENT_BLANK:
		bl_blank(n, *blank);
		break;
	case FB_EVENT_FB_REGISTERED:
		bl_blank(n, FB_BLANK_UNBLANK);
		break;
	case FB_EVENT_FB_UNREGISTERED:
		bl_blank(n, FB_BLANK_NORMAL);
		break;
	}

	return 0;
}

static void bl_trig_activate(struct led_classdev *led)
{
	int ret;

	struct bl_trig_notifier *n;

	n = kzalloc(sizeof(struct bl_trig_notifier), GFP_KERNEL);
	led->trigger_data = n;
	if (!n) {
		dev_err(led->dev, "unable to allocate backlight trigger\n");
		return;
	}

	n->led = led;
	n->brightness = led->brightness;
	n->old_status = FB_BLANK_UNBLANK;
	n->notifier.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&n->notifier);
	if (ret)
		dev_err(led->dev, "unable to register backlight trigger\n");
}

static void bl_trig_deactivate(struct led_classdev *led)
{
	struct bl_trig_notifier *n =
		(struct bl_trig_notifier *) led->trigger_data;

	if (n) {
		fb_unregister_client(&n->notifier);
		kfree(n);
	}
}

static struct led_trigger bl_led_trigger = {
	.name		= "backlight",
	.activate	= bl_trig_activate,
	.deactivate	= bl_trig_deactivate
};

static int __init bl_trig_init(void)
{
	return led_trigger_register(&bl_led_trigger);
}

static void __exit bl_trig_exit(void)
{
	led_trigger_unregister(&bl_led_trigger);
}

module_init(bl_trig_init);
module_exit(bl_trig_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("Backlight emulation LED trigger");
MODULE_LICENSE("GPL v2");
