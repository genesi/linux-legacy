#ifndef _LINUX_CPAD_H
#define _LINUX_CPAD_H

#include <linux/input.h>


/* Supported ioctls for /dev/usb/cpad*
 * Because the backlight has a finite lifespan (ca. 1000 - 3000 hours), the
 * CPAD_FLASH ioctl should be used instead of CPAD_WLIGHT.
 * To get the result of CPAD_RLIGHT and CPAD_RLCD, read some bytes from
 * the character device. The third byte is the result.
 */
#define CPAD_IOCTL_BASE		0x71

/* reported by CPAD_VERSION */
#define CPAD_DRIVER_NUM 	8

/* get driver version */
#define CPAD_VERSION		_IOR('U', CPAD_IOCTL_BASE, int)

/* get device ID */
#define CPAD_CGID		_IOR('U', CPAD_IOCTL_BASE+1, struct input_id)

/* reset cPad device (DEPRECATED) */
#define CPAD_RESET		_IO('U', CPAD_IOCTL_BASE+2)

/* set backlight state */
#define CPAD_WLIGHT		_IOW('U', CPAD_IOCTL_BASE+3, u8)

/* set LCD state */
#define CPAD_WLCD		_IOW('U', CPAD_IOCTL_BASE+5, u8)

/* read backlight state */
#define CPAD_RLIGHT		_IO('U', CPAD_IOCTL_BASE+6)

/* read LCD state */
#define CPAD_RLCD		_IO('U', CPAD_IOCTL_BASE+7)

/* flash backlight */
#define CPAD_FLASH		_IOW('U', CPAD_IOCTL_BASE+8, int)

/* read eeprom */
#define CPAD_REEPROM		_IO('U', CPAD_IOCTL_BASE+4)


/*
 * writing to /dev/usb/cpad*
 *	The cPad display is controlled by a Seiko/Epson 1335 LCD Controller IC.
 *	A write to the device consists of a command followed by data:
 *		<1335 command> [<data> ...]
 *	For the MRWITE_1335 command, data may be up to 274*30 bytes long.
 *	Possible commands are listed below.
 *
 * reading /dev/usb/cpad*
 *	Reads answer of the sed1335 to a command, can be 2-32 bytes long.
 *	The information is only accessible until the next write access.
 */

/* Possible sed1335 commands as reported in its data sheet are: */

/* system control */
#define SYSSET_1335		0x40
#define SLEEP_1335		0x53

/* display control */
#define DISPOFF_1335		0x58
#define DISP_1335		0x59
#define SCROLL_1335		0x44
#define CSRF_1335		0x5d
#define CGRAMADR_1335		0x5c
#define CSRDIR_RIGHT_1335	0x4c
#define CSRDIR_LEFT_1335	0x4d
#define CSRDIR_UP_1335		0x4e
#define CSRDIR_DOWN_1335	0x4f
#define HDOTSCR_1335		0x5a
#define OVLAY_1335		0x5b

/* drawing control */
#define CSRW_1335		0x46
#define CSRR_1335		0x47

/* memory control */
#define MWRITE_1335		0x42
#define MREAD_1335		0x43


#endif /* _LINUX_CPAD_H */
