/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2011 Genesi USA, Inc. All Rights Reserved.
 */


#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/sbs.h>
#include <mach/gpio.h>
#include <asm/mach-types.h>

#include "mx51_pins.h"
#include "iomux.h"

#include "mx51_efikamx.h"

static struct sbs_platform_data mx51_efikamx_sbs_platform_data = {
	.mains_status = mx51_efikasb_ac_status,
	.battery_status = mx51_efikasb_battery_status,
	.alarm_status = mx51_efikasb_battery_alarm,
};

static struct i2c_board_info mx51_efikamx_i2c_battery __initdata = {
	.type          = "smart-battery",
	.addr          = 0x0b,
	.platform_data = &mx51_efikamx_sbs_platform_data,
};

void __init mx51_efikamx_init_battery(void)
{
	if (machine_is_mx51_efikasb()) {
		i2c_register_board_info(1, &mx51_efikamx_i2c_battery, 1);
	}
}
