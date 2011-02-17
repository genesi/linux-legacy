/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */




#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/sbs.h>
#include <mach/gpio.h>

#include "mx51_efikasb.h"
#include "mx51_pins.h"
#include "iomux.h"


#define EFIKASB_BATTERY_LOW	MX51_PIN_DI1_PIN11
#define EFIKASB_BATTERY_INSERT	MX51_PIN_DISPB2_SER_DIO
#define EFIKASB_AC_INSERT	MX51_PIN_DI1_D0_CS

static struct mxc_iomux_pin_cfg mx51_efikasb_battery_pins[] = {
	{ EFIKASB_BATTERY_INSERT, IOMUX_CONFIG_GPIO, },
	{ EFIKASB_BATTERY_LOW, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION, },
	{ EFIKASB_AC_INSERT, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION,
/*		IOMUXC_GPIO3_IPP_IND_G_3_SELECT_INPUT, INPUT_CTL_PATH1, */
	},
};



enum mxc_power_resource {
	MAINS_INSERTION_STATUS,
	BATTERY_INSERTION_STATUS,
	BATTERY_ALERT,
};

static struct resource mxc_power_resources[] = {
	[MAINS_INSERTION_STATUS] = {
		.start = IOMUX_TO_IRQ(EFIKASB_AC_INSERT),
		.end   = IOMUX_TO_IRQ(EFIKASB_AC_INSERT),
		.name  = "mains-insertion-status",
		.flags =  IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE,
	},
	[BATTERY_INSERTION_STATUS] = {
		.start = IOMUX_TO_IRQ(EFIKASB_BATTERY_INSERT),
		.end   = IOMUX_TO_IRQ(EFIKASB_BATTERY_INSERT),
		.name  = "battery-insertion-status",
		.flags =  IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE,
	},
	[BATTERY_ALERT] = {
		.start = IOMUX_TO_IRQ(EFIKASB_BATTERY_LOW),
		.end   = IOMUX_TO_IRQ(EFIKASB_BATTERY_LOW),
		.name  = "battery-alert",
		.flags =  IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE,
	},
};

int mx51_efikasb_battery_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT));
}

int mx51_efikasb_battery_alarm(void)
{
        return !gpio_get_value(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW));
}

int mx51_efikasb_ac_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(EFIKASB_AC_INSERT));
}

static struct sbs_platform_data mx51_efikasb_sbs_platform_data = {
	.mains_insertion_status   = mx51_efikasb_ac_status,
	.battery_insertion_status = mx51_efikasb_battery_status,

	.battery_alert            = &mxc_power_resources[BATTERY_ALERT],

	.mains_presence_changed   = &mxc_power_resources[MAINS_INSERTION_STATUS],
	.battery_presence_changed = &mxc_power_resources[BATTERY_INSERTION_STATUS],
};

static struct i2c_board_info mx51_efikasb_i2c_battery __initdata = {
	.type          = "smart-battery",
	.addr          = 0x0b,
	.platform_data = &mx51_efikasb_sbs_platform_data,
};

void __init mx51_efikasb_init_battery(void)
{
	CONFIG_IOMUX(mx51_efikasb_battery_pins);

	gpio_request(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT), "battery:insert");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_BATTERY_INSERT));

        gpio_request(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW), "battery:alarm");
        gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_BATTERY_LOW));

        /* ron: IOMUXC_GPIO3_IPP_IND_G_IN_3_SELECT_INPUT: 1: Selecting Pad DI1_D0_CS for Mode:ALT4 */
        __raw_writel(0x01, IO_ADDRESS(IOMUXC_BASE_ADDR) + 0x980);
	gpio_request(IOMUX_TO_GPIO(EFIKASB_AC_INSERT), "battery:ac");
	gpio_direction_input(IOMUX_TO_GPIO(EFIKASB_AC_INSERT));

	i2c_register_board_info(1, &mx51_efikasb_i2c_battery, 1);
}
