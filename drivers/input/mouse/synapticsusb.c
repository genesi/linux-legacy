/****
 * USB Synaptics device driver
 *
 *  Copyright (c) 2002 Rob Miller (rob@inpharmatica . co . uk)
 *  Copyright (c) 2003 Ron Lee (ron@debian.org)
 *	cPad driver for kernel 2.4
 *
 *  Copyright (c) 2004 Jan Steinhoff (cpad@jan-steinhoff . de)
 *  Copyright (c) 2004 Ron Lee (ron@debian.org)
 *	rewritten for kernel 2.6
 *
 *  cPad dispaly character device part now in cpad.c
 *
 * Bases on: 	usb_skeleton.c v2.2 by Greg Kroah-Hartman
 *		drivers/hid/usbhid/usbmouse.c by Vojtech Pavlik
 *		drivers/input/mouse/synaptics.c by Peter Osterlund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Trademarks are the property of their respective owners.
 */

/****
 * There are three different types of Synaptics USB devices: Touchpads,
 * touchsticks (or trackpoints), and touchscreens. Touchpads are well supported
 * by this driver, touchstick support has not been tested much yet, and
 * touchscreens have not been tested at all. The only difference between pad
 * and stick seems to be that the x and y finger positions are unsigned 13 bit
 * integers in the first case, but are signed ones in the second case.
 *
 * Up to three alternate settings are possible:
 *	setting 0: one int endpoint for relative movement (used by usbhid.ko)
 *	setting 1: one int endpoint for absolute finger position
 *	setting 2 (cPad only): one int endpoint for absolute finger position and
 *		   two bulk endpoints for the display (in/out)
 * This driver uses setting 2 for the cPad and setting 1 for other devices.
 *
 * The cPad is an USB touchpad with background display. The display driver part
 * can be found in cpad.c.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/usb/input.h>
#include <linux/workqueue.h>

#include "synapticsusb.h"
#include "cpad.h"


/*
 * input device code
 */

MODULE_PARM_DESC(xmin, "minimal horizontal finger position for touchpads");
MODULE_PARM_DESC(xmax, "maximal horizontal finger position for touchpads");
MODULE_PARM_DESC(ymin, "minimal vertical finger position for touchpads");
MODULE_PARM_DESC(ymax, "maximal vertical finger position for touchpads");
static int xmin = 1472;
static int xmax = 5472;
static int ymin = 1408;
static int ymax = 4448;
module_param(xmin, int, 0444);
module_param(xmax, int, 0444);
module_param(ymin, int, 0444);
module_param(ymax, int, 0444);

MODULE_PARM_DESC(btn_middle, "if set, cPad menu button is reported "
			     "as a middle button");
static int btn_middle = 1;
module_param(btn_middle, int, 0644);

static const char synusb_pad_name[] = "Synaptics USB TouchPad";
static const char synusb_stick_name[] = "Synaptics USB Styk";
static const char synusb_screen_name[] = "Synaptics USB TouchScreen";

static const char *synusb_get_name(struct synusb *synusb)
{
	switch (synusb->input_type) {
	case SYNUSB_PAD:
		return synusb_pad_name;
	case SYNUSB_STICK:
		return synusb_stick_name;
	case SYNUSB_SCREEN:
		return synusb_screen_name;
	}
	return NULL;
}

/* report tool_width for touchpads */
static void synusb_report_width(struct input_dev *idev, int pressure, int w)
{
	int num_fingers, tool_width;

	if (pressure > 0) {
		num_fingers = 1;
		tool_width = 5;
		switch (w) {
		case 0 ... 1:
			num_fingers = 2 + w;
			break;
		case 2:	                /* pen, pretend its a finger */
			break;
		case 4 ... 15:
			tool_width = w;
			break;
		}
	} else {
		num_fingers = 0;
		tool_width = 0;
	}

	input_report_abs(idev, ABS_TOOL_WIDTH, tool_width);

	input_report_key(idev, BTN_TOOL_FINGER, num_fingers == 1);
	input_report_key(idev, BTN_TOOL_DOUBLETAP, num_fingers == 2);
	input_report_key(idev, BTN_TOOL_TRIPLETAP, num_fingers == 3);
}

/* convert signed or unsigned 13 bit number to int */
static inline int us13_to_int(u8 high, u8 low, int has_sign)
{
	int res;

	res = ((int)(high & 0x1f) << 8) | low;
	if (has_sign && (high & 0x10))
		res -= 0x2000;

	return res;
}

static void synusb_input_callback(struct urb *urb)
{
	struct synusb *synusb = (struct synusb *)urb->context;
	u8 *data = urb->transfer_buffer;
	struct input_dev *idev = synusb->idev;
	int res, x, y, pressure;
	int is_stick = (synusb->input_type == SYNUSB_STICK) ? 1 : 0;

	if (urb->status) {
		if (synusb_urb_status_error(urb)) {
			synusb_err(synusb, "nonzero int in status received: %d",
				   urb->status);
			/* an error occured, try to resubmit */
			goto resubmit;
		}

		/* unlink urb, do not resubmit */
		return;
	}

	pressure = data[6];
	x = us13_to_int(data[2], data[3], is_stick);
	y = us13_to_int(data[4], data[5], is_stick);

	if (is_stick) {
		y = -y;
		if (pressure > 6)
			input_report_key(idev, BTN_TOUCH, 1);
		if (pressure < 5)
			input_report_key(idev, BTN_TOUCH, 0);
		input_report_key(idev, BTN_TOOL_FINGER, pressure ? 1 : 0);
	} else {
		if (pressure > 30)
			input_report_key(idev, BTN_TOUCH, 1);
		if (pressure < 25)
			input_report_key(idev, BTN_TOUCH, 0);
		synusb_report_width(idev, pressure, data[0] & 0x0f);
		y = ymin + ymax - y;
	}

	if (pressure > 0) {
		input_report_abs(idev, ABS_X, x);
		input_report_abs(idev, ABS_Y, y);
	}
	input_report_abs(idev, ABS_PRESSURE, pressure);

	input_report_key(idev, BTN_LEFT, data[1] & 0x04);
	input_report_key(idev, BTN_RIGHT, data[1] & 0x01);
	input_report_key(idev, BTN_MIDDLE, data[1] & 0x02);
	if (synusb->has_display)
		input_report_key(idev, btn_middle ? BTN_MIDDLE : BTN_MISC,
				 data[1] & 0x08);

	input_sync(idev);
resubmit:
	res = usb_submit_urb(urb, GFP_ATOMIC);
	if (res)
		synusb_err(synusb, "submit int in urb failed with result %d",
			   res);
}

/* Data must always be fetched from the int endpoint, otherwise the device
 * would reconnect to force driver reload. So this is always scheduled by probe.
 */
static void synusb_submit_int(struct work_struct *work)
{
	struct synusb *synusb = container_of(work, struct synusb, isubmit.work);
	int res;

	res = usb_submit_urb(synusb->iurb, GFP_KERNEL);
	if (res)
		synusb_err(synusb, "submit int in urb failed with result %d",
			   res);
}

static int synusb_init_input(struct synusb *synusb)
{
	struct input_dev *idev;
	struct usb_device *udev = synusb->udev;
	int is_stick = (synusb->input_type == SYNUSB_STICK) ? 1 : 0;
	int retval = -ENOMEM;

	idev = input_allocate_device();
	if (idev == NULL) {
		synusb_err(synusb, "Can not allocate input device");
		goto error;
	}

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);

	if (is_stick) {
		input_set_abs_params(idev, ABS_X, -512, 512, 0, 0);
		input_set_abs_params(idev, ABS_Y, -512, 512, 0, 0);
		input_set_abs_params(idev, ABS_PRESSURE, 0, 127, 0, 0);
	} else {
		input_set_abs_params(idev, ABS_X, xmin, xmax, 0, 0);
		input_set_abs_params(idev, ABS_Y, ymin, ymax, 0, 0);
		input_set_abs_params(idev, ABS_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(idev, ABS_TOOL_WIDTH, 0, 15, 0, 0);
		__set_bit(BTN_TOOL_DOUBLETAP, idev->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, idev->keybit);
	}

	__set_bit(BTN_TOUCH, idev->keybit);
	__set_bit(BTN_TOOL_FINGER, idev->keybit);
	__set_bit(BTN_LEFT, idev->keybit);
	__set_bit(BTN_RIGHT, idev->keybit);
	__set_bit(BTN_MIDDLE, idev->keybit);
	if (synusb->has_display)
		__set_bit(BTN_MISC, idev->keybit);

	usb_make_path(udev, synusb->iphys, sizeof(synusb->iphys));
	strlcat(synusb->iphys, "/input0", sizeof(synusb->iphys));
	idev->phys = synusb->iphys;
	idev->name = synusb_get_name(synusb);
	usb_to_input_id(udev, &idev->id);
	idev->dev.parent = &synusb->interface->dev;
	input_set_drvdata(idev, synusb);

	retval = input_register_device(idev);
	if (retval) {
		synusb_err(synusb, "Can not register input device");
		goto error;
	}
	synusb->idev = idev;

	synusb->interface->needs_remote_wakeup = 1;

	return 0;
error:
	if (idev)
		input_free_device(idev);

	return retval;
}


/*
 * initialization of usb data structures
 */

static int synusb_setup_iurb(struct synusb *synusb,
			     struct usb_endpoint_descriptor *endpoint)
{
	char *buf;

	if (endpoint->wMaxPacketSize < 8)
		return 0;
	if (synusb->iurb) {
		synusb_warn(synusb, "Found more than one possible "
				    "int in endpoint");
		return 0;
	}
	synusb->iurb = usb_alloc_urb(0, GFP_KERNEL);
	if (synusb->iurb == NULL)
		return -ENOMEM;
	buf = usb_buffer_alloc(synusb->udev, 8, GFP_ATOMIC,
							 &synusb->iurb->transfer_dma);
	if (buf == NULL)
		return -ENOMEM;
	usb_fill_int_urb(synusb->iurb, synusb->udev,
			 usb_rcvintpipe(synusb->udev,
					endpoint->bEndpointAddress),
			 buf, 8, synusb_input_callback,
			 synusb, endpoint->bInterval);
	synusb->iurb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	return 0;
}

static int synusb_check_int_setup(struct synusb *synusb)
{
	if (synusb->iurb)
		return 0;
	return -ENODEV;
}

static struct synusb_endpoint_table {
	__u8 dir;
	__u8 xfer_type;
	int  (*setup)(struct synusb *,
		      struct usb_endpoint_descriptor *);
} synusb_endpoints[] = {
	{ USB_DIR_IN,	USB_ENDPOINT_XFER_BULK,	cpad_setup_in },
	{ USB_DIR_OUT,	USB_ENDPOINT_XFER_BULK,	cpad_setup_out },
	{ USB_DIR_IN,	USB_ENDPOINT_XFER_INT,	synusb_setup_iurb },
	{ }
};

/* return entry index in synusb_endpoint_table that matches ep */
static inline int synusb_match_endpoint(struct usb_endpoint_descriptor *ep)
{
	int i;

	for (i = 0; synusb_endpoints[i].setup; i++)
		if (((ep->bEndpointAddress & USB_DIR_IN)
				== synusb_endpoints[i].dir) &&
		    ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
				== synusb_endpoints[i].xfer_type))
			return i;
	return -ENODEV;
}

static int synusb_setup_endpoints(struct synusb *synusb)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int int_num = synusb->interface->cur_altsetting->desc.bInterfaceNumber;
	unsigned altsetting;
	int i, j, res;

	altsetting = min((unsigned) (synusb->has_display ? 2 : 1),
			 synusb->interface->num_altsetting);
	res = usb_set_interface(synusb->udev, int_num, altsetting);
	if (res) {
		synusb_err(synusb, "Can not set alternate setting to %i, "
				   "error: %i", altsetting, res);
		return res;
	}

	/* allocate synusb->display, if the device has a display */
	res = cpad_alloc(synusb);
	if (res)
		return res;

	/* go through all endpoints and call the setup function
	 * listed in synusb_endpoint_table */
	iface_desc = synusb->interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		j = synusb_match_endpoint(endpoint);
		if (j >= 0) {
			res = synusb_endpoints[j].setup(synusb, endpoint);
			if (res)
				return res;
		}
	}

	/* check whether all possible endpoints have been set up */
	res = synusb_check_int_setup(synusb);
	if (res)
		return res;
	res = cpad_check_setup(synusb);
	if (res)
		return res;

	return 0;
}

/* disable experimental stick support by default */
MODULE_PARM_DESC(enable_stick, "enable trackpoint support");
static int enable_stick;
module_param(enable_stick, int, 0644);

static int synusb_detect_type(struct synusb *synusb,
			      const struct usb_device_id *id)
{
	int int_num = synusb->interface->cur_altsetting->desc.bInterfaceNumber;

	synusb->has_display = 0;
	if (id->idVendor == USB_VID_SYNAPTICS) {
		switch (id->idProduct) {
		case USB_DID_SYN_TS:
			synusb->input_type = SYNUSB_SCREEN;
			break;
		case USB_DID_SYN_STICK:
			synusb->input_type = SYNUSB_STICK;
			break;
		case USB_DID_SYN_COMP_TP:
			if (int_num == 1)
				synusb->input_type = SYNUSB_STICK;
			else
				synusb->input_type = SYNUSB_PAD;
			break;
		case USB_DID_SYN_CPAD:
			synusb->has_display = 1;
		default:
			synusb->input_type = SYNUSB_PAD;
		}
	} else {
		synusb->input_type = SYNUSB_PAD;
	}

	if ((synusb->input_type == SYNUSB_STICK) && !enable_stick)
		return -ENODEV;
	if (synusb->input_type == SYNUSB_SCREEN)
		synusb_warn(synusb, "driver has not been tested "
				    "with touchscreens!");

	return 0;
}

void synusb_free_urb(struct urb *urb)
{
	if (urb == NULL)
		return;
	usb_buffer_free(urb->dev, urb->transfer_buffer_length,
					  urb->transfer_buffer, urb->transfer_dma);
	usb_free_urb(urb);
}

void synusb_delete(struct kref *kref)
{
	struct synusb *synusb = container_of(kref, struct synusb, kref);

	synusb_free_urb(synusb->iurb);
	if (synusb->idev)
		input_unregister_device(synusb->idev);

	cpad_free(synusb);

	usb_put_dev(synusb->udev);
	kfree(synusb);
}

static int synusb_probe(struct usb_interface *interface,
			const struct usb_device_id *id)
{
	struct synusb *synusb = NULL;
	struct usb_device *udev = interface_to_usbdev(interface);
	int retval = -ENOMEM;

	synusb = kzalloc(sizeof(*synusb), GFP_KERNEL);
	if (synusb == NULL) {
		dev_err(&interface->dev, "Out of memory in synusb_probe\n");
		goto error;
	}

	synusb->udev = usb_get_dev(udev);
	synusb->interface = interface;
	kref_init(&synusb->kref);
	usb_set_intfdata(interface, synusb);

	if (synusb_detect_type(synusb, id))
		goto error;

	retval = synusb_setup_endpoints(synusb);
	if (retval) {
		synusb_err(synusb, "Can not set up endpoints, error: %i",
			   retval);
		goto error;
	}

	retval = synusb_init_input(synusb);
	if (retval)
		goto error;

	retval = cpad_init(synusb);
	if (retval)
		goto error;

	/* submit the int in urb */
	INIT_DELAYED_WORK(&synusb->isubmit, synusb_submit_int);
	schedule_delayed_work(&synusb->isubmit, msecs_to_jiffies(10));

	return 0;

error:
	if (synusb) {
		usb_set_intfdata(interface, NULL);
		kref_put(&synusb->kref, synusb_delete);
	}
	return retval;
}

static void synusb_disconnect(struct usb_interface *interface)
{
	struct synusb *synusb;

	synusb = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	cpad_disconnect(synusb);

	cancel_delayed_work_sync(&synusb->isubmit);

	usb_kill_urb(synusb->iurb);
	input_unregister_device(synusb->idev);
	synusb->idev = NULL;

	kref_put(&synusb->kref, synusb_delete);

	dev_info(&interface->dev, "Synaptics device disconnected\n");
}


/*
 * suspend code
 */

static void synusb_draw_down(struct synusb *synusb)
{
	cancel_delayed_work_sync(&synusb->isubmit);
	usb_kill_urb(synusb->iurb);
}

static int synusb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct synusb *synusb = usb_get_intfdata(intf);
	int res;

	if (synusb == NULL)
		return 0;

	res = cpad_suspend(synusb);
	if (res)
		goto error;

	synusb_draw_down(synusb);
error:
	return res;
}

static int synusb_resume(struct usb_interface *intf)
{
	struct synusb *synusb = usb_get_intfdata(intf);
	int res;

	res = usb_submit_urb(synusb->iurb, GFP_ATOMIC);
	if (res) {
		synusb_err(synusb, "submit int in urb failed during resume "
				   "with result %d", res);
		goto error;
	}

	res = cpad_resume(synusb);
	if (res)
		synusb_draw_down(synusb);
error:
	return res;
}

static int synusb_pre_reset(struct usb_interface *intf)
{
	struct synusb *synusb = usb_get_intfdata(intf);
	int res;

	res = cpad_pre_reset(synusb);
	if (res)
		goto error;

	synusb_draw_down(synusb);
error:
	return res;
}

static int synusb_post_reset(struct usb_interface *intf)
{
	struct synusb *synusb = usb_get_intfdata(intf);
	int res;

	res = usb_submit_urb(synusb->iurb, GFP_ATOMIC);
	if (res) {
		synusb_err(synusb, "submit int in urb failed in during "
				   "post_reset with result %d", res);
		goto error;
	}

	res = cpad_post_reset(synusb);
	if (res)
		synusb_draw_down(synusb);
error:
	return res;
}

static int synusb_reset_resume(struct usb_interface *intf)
{
	struct synusb *synusb = usb_get_intfdata(intf);
	int res;

	res = usb_submit_urb(synusb->iurb, GFP_ATOMIC);
	if (res) {
		synusb_err(synusb, "submit int in urb failed during "
				   "reset-resume with result %d", res);
		goto error;
	}

	res = cpad_reset_resume(synusb);
	if (res)
		synusb_draw_down(synusb);
error:
	return res;
}

/* the id table is filled via sysfs, so usbhid is always the default driver */
static struct usb_device_id synusb_idtable[] = { { } };
MODULE_DEVICE_TABLE(usb, synusb_idtable);

struct usb_driver synusb_driver = {
	.name =		"synaptics_usb",
	.probe =	synusb_probe,
	.disconnect =	synusb_disconnect,
	.id_table =	synusb_idtable,
	.suspend =	synusb_suspend,
	.resume =	synusb_resume,
	.pre_reset =	synusb_pre_reset,
	.post_reset =	synusb_post_reset,
	.reset_resume = synusb_reset_resume,
	.supports_autosuspend = 1,
};

static int __init synusb_init(void)
{
	int result;

	result = usb_register(&synusb_driver);
	if (result)
		err("usb_register failed. Error number %d", result);
	else
		pr_info(DRIVER_DESC " " DRIVER_VERSION "\n");

	return result;
}

static void __exit synusb_exit(void)
{
	usb_deregister(&synusb_driver);
}

module_init(synusb_init);
module_exit(synusb_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
