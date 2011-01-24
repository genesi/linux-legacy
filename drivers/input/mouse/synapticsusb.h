#ifndef _SYNAPTICS_USB_H
#define _SYNAPTICS_USB_H

#include <linux/usb.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#define DRIVER_VERSION	"v1.5"
#define DRIVER_AUTHOR	"Rob Miller (rob@inpharmatica . co . uk), "\
			"Ron Lee (ron@debian.org), "\
			"Jan Steinhoff (cpad@jan-steinhoff . de)"
#define DRIVER_DESC	"USB Synaptics device driver"

/* vendor and device IDs */
#define USB_VID_SYNAPTICS	0x06cb	/* Synaptics vendor ID */
#define USB_DID_SYN_TP		0x0001	/* Synaptics USB TouchPad */
#define USB_DID_SYN_INT_TP	0x0002	/* Synaptics Integrated USB TouchPad */
#define USB_DID_SYN_CPAD	0x0003	/* Synaptics cPad */
#define USB_DID_SYN_TS		0x0006	/* Synaptics TouchScreen */
#define USB_DID_SYN_STICK	0x0007	/* Synaptics USB Styk */
#define USB_DID_SYN_WP		0x0008	/* Synaptics USB WheelPad */
#define USB_DID_SYN_COMP_TP	0x0009	/* Synaptics Composite USB TouchPad */
#define USB_DID_SYN_WTP		0x0010	/* Synaptics Wireless TouchPad */
#define USB_DID_SYN_DP		0x0013	/* Synaptics DisplayPad */

/* structure to hold all of our device specific stuff */
struct synusb {
	struct usb_device	*udev;
	struct usb_interface	*interface;
	struct kref		kref;

	/* characteristics of the device */
	unsigned int		input_type:8;
	unsigned int		buttons:8;
	unsigned int		has_display:1;

	/* input device related data structures */
	struct input_dev	*idev;
	char			iphys[64];
	struct delayed_work	isubmit;
	struct urb		*iurb;

	/* cPad display character device, see cpad.h */
	struct syndisplay	*display;
};

/* possible values for synusb.input_type */
#define SYNUSB_PAD		0
#define SYNUSB_STICK		1
#define SYNUSB_SCREEN		2

extern struct usb_driver synusb_driver;

/* free urb and its buffer */
void synusb_free_urb(struct urb *urb);
/* cleanup handler for kref_put, kref_put(&synusb->kref, synusb_delete) */
void synusb_delete(struct kref *);

static inline int synusb_urb_status_error(struct urb *urb)
{
	/* sync/async unlink faults aren't errors */
	if (urb->status == -ENOENT ||
	    urb->status == -ECONNRESET ||
	    urb->status == -ESHUTDOWN)
		return 0;
	else
		return urb->status;
}

/* helper functions for printk */
#define synusb_err(_synusb, format, arg...)	\
	dev_err(&(_synusb)->interface->dev, format "\n", ## arg)
#define synusb_warn(_synusb, format, arg...)	\
	dev_warn(&(_synusb)->interface->dev, format "\n", ## arg)

#include "cpad.h"

#endif /* _SYNAPTICS_USB_H */
