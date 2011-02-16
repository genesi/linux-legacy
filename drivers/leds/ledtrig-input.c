/*
 * led trigger for input subsystem led states
 *
 * Peter Korsgaard <jacmet@sunsite.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "leds.h"

struct input_trig {
	int value; /* current value of input LED */
	struct work_struct work;
	struct led_trigger trig;
};

static int trig_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "ledtrig-input";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void trig_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static void trig_activate(struct led_classdev *led_cdev)
{
	struct input_trig *input = container_of(led_cdev->trigger,
						struct input_trig, trig);

	led_set_brightness(led_cdev, input->value ? LED_FULL : LED_OFF);
}

static struct input_trig trig[] = {
	[LED_NUML] = {
		.trig = {
			.name = "numlock",
			.activate = trig_activate,
		},
	},
	[LED_CAPSL] = {
		.trig = {
			.name = "capslock",
			.activate = trig_activate,
		}
	},
	[LED_SCROLLL] = {
		.trig = {
			.name = "scrolllock",
			.activate = trig_activate,
		},
	},
};

static void trig_event(struct input_handle *handle, unsigned int type,
		      unsigned int code, int value)
{
	if (type == EV_LED && code < ARRAY_SIZE(trig)) {
		if (value != trig[code].value) {
			trig[code].value = value;
			schedule_work(&trig[code].work);
		}
	}
}

static const struct input_device_id trig_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_LED) },
	},
	{ },			/* Terminating zero entry */
};
MODULE_DEVICE_TABLE(input, trig_ids);

static struct input_handler trig_handler = {
	.event		= trig_event,
	.connect	= trig_connect,
	.disconnect	= trig_disconnect,
	.name		= "ledtrig-input",
	.id_table	= trig_ids,
};

static void trig_work(struct work_struct *work)
{
	struct input_trig *input = container_of(work, struct input_trig, work);

	led_trigger_event(&input->trig, input->value ? LED_FULL : LED_OFF);
}

static int __init trig_init(void)
{
	int i, ret;

	for (i=0; i<ARRAY_SIZE(trig); i++) {
		INIT_WORK(&trig[i].work, trig_work);
		ret = led_trigger_register(&trig[i].trig);
		if (ret)
			goto trig_reg_fail;
	}

	ret = input_register_handler(&trig_handler);
	if (!ret)
		return 0;

 trig_reg_fail:
	for (; i>0; i--)
		led_trigger_unregister(&trig[i-1].trig);

	return ret;
}
module_init(trig_init);

static void __exit trig_exit(void)
{
	int i;

	input_unregister_handler(&trig_handler);

	for (i=0; i<ARRAY_SIZE(trig); i++) {
		cancel_work_sync(&trig[i].work);
		led_trigger_unregister(&trig[i].trig);
	}
}
module_exit(trig_exit);

MODULE_AUTHOR("Peter Korsgaard <jacmet@sunsite.dk>");
MODULE_DESCRIPTION("LED trigger for input subsystem LEDs");
MODULE_LICENSE("GPL");

