/*
 * Efika MX Smart Battery driver
 *
 * Copyright (C) 2009 Ron Lee <ron1_lee@pegatroncorp.com>
 *
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/unaligned.h>
#include <mach/hardware.h>

#define SBS_MFG_ACCESS                  0x00
#define SBS_REMAIN_CAPACITY_ALARM       0x01
#define SBS_REMNAIN_TIME_ALARM          0x02
#define SBS_BATTERY_MODE                0x03
#define SBS_AT_RATE                     0x04
#define SBS_AT_RATE_TIME_TO_FULL        0x05
#define SBS_AT_RATE_TIME_TO_EMPTY       0x06
#define SBS_AT_RATE_OK                  0x07
#define SBS_TEMP                        0x08
#define SBS_VOLTAGE                     0x09
#define SBS_CURRENT                     0x0A
#define SBS_AVG_CURRENT                 0x0B
#define SBS_MAX_ERROR                   0x0C
#define SBS_REL_STATE_OF_CHARGE         0x0D
#define SBS_ABS_STATE_OF_CHARGE         0x0E
#define SBS_REMAIN_CAPABILITY           0x0F
#define SBS_FULL_CHARGE_CAPACITY        0x10
#define SBS_RUN_TIME_TO_EMPTY           0x11
#define SBS_AVG_TIME_TO_EMPTY           0x12
#define SBS_AVG_TIME_TO_FULL            0x13
#define SBS_CHARGE_CURRENT              0x14
#define SBS_CHARGE_VOLTAGE              0x15
#define SBS_BATTERY_STATUS              0x16
#define SBS_CYCLE_COUNT                 0x17
#define SBS_DESIGN_CAPACITY             0x18
#define SBS_DESIGN_VOLTAGE              0x19
#define SBS_SPEC_INFO                   0x1A
#define SBS_MFG_DATE                    0x1B
#define SBS_SERIAL_NO                   0x1C
#define SBS_MFG_NAME                    0x20
#define SBS_DEV_NAME                    0x21
#define SBS_DEV_CHEMISTRY               0x22
#define SBS_MFG_DATA                    0x23
#define SBS_BATTERY_USAGE               0x30
#define SBS_PERMANENT_FAILURE           0x31
#define SBS_BATTERY_LOG1                0x32
#define SBS_BATTERY_LOG2                0x33
#define SBS_FET_TEMP                    0x3B
#define SBS_OPTION_MFG_FUNC5            0x2F
#define SBS_OPTION_MFG_FUNC4            0x3C
#define SBS_OPTION_MFG_FUNC3            0x3D
#define SBS_OPTION_MFG_FUNC2            0x3E
#define SBS_OPTION_MFG_FUNC1            0x3F

/* SBS_BATTERY_STATUS Register Bit Mapping */
#define SBS_STATUS_OVER_CHARGE_ALARM    0x8000
#define SBS_STATUS_TERM_CHARGE_ALARM    0x4000
#define SBS_STATUS_OVER_TEMP_ALARM      0x1000
#define SBS_STATUS_TERM_DISCHARGE_ALARM 0x0800
#define SBS_STATUS_REMAIN_CAPACITY_ALARM  0x0200
#define SBS_STATUS_REMAIN_TIME_ALARM    0x0100

#define SBS_STATUS_INITIALIZED          0x0080
#define SBS_STATUS_DISCHARGING          0x0040
#define SBS_STATUS_FULLY_CHARGED        0x0020
#define SBS_STATUS_FULLY_DISCHARGED     0x0010

static void update_status_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(update_status_work, update_status_worker);

static void batt_capacity_worker(struct work_struct *work);
static DECLARE_WORK(batt_capacity_work, batt_capacity_worker);

static void power_off_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(power_off_work, power_off_worker);

extern void kernel_power_off(void);

struct sbs_reg_tbl {
	char *name;
	unsigned char reg_offset;
};

#ifdef EFIKASB_BATTERY_DEBUG
static struct sbs_reg_tbl reg_tbl[] = {
	{"Manufacture Access", SBS_MFG_ACCESS},
	{"Remain Capacity Alarm", SBS_REMAIN_CAPACITY_ALARM },
	{"Remain Time Alarm", SBS_REMNAIN_TIME_ALARM},
	{"Battery Mode", SBS_BATTERY_MODE},
	{"AT Rate", SBS_AT_RATE},
	{"AT Rate Time To Full", SBS_AT_RATE_TIME_TO_FULL},
	{"AT Rate Time To Empty", SBS_AT_RATE_TIME_TO_EMPTY},
	{"AT Rate OK", SBS_AT_RATE_OK},
	{"Temperature", SBS_TEMP},
	{"Voltage", SBS_VOLTAGE},
	{"Current", SBS_CURRENT},
	{"Average Current", SBS_AVG_CURRENT},
	{"Maximum Error", SBS_MAX_ERROR},
	{"Relative State of Charge", SBS_REL_STATE_OF_CHARGE},
	{"Absolute State of Charge", SBS_ABS_STATE_OF_CHARGE},
	{"Remain Capacity", SBS_REMAIN_CAPABILITY},
	{"Full Charge Capacity", SBS_FULL_CHARGE_CAPACITY},
	{"Run Time To Empty", SBS_RUN_TIME_TO_EMPTY},
	{"Average Time To Empty", SBS_AVG_TIME_TO_EMPTY},
	{"Average Time To Full", SBS_AVG_TIME_TO_FULL},
	{"Charge Current", SBS_CHARGE_CURRENT},
	{"Charge Voltage", SBS_CHARGE_VOLTAGE},
	{"Battery Status", SBS_BATTERY_STATUS},
	{"Cycle Count", SBS_CYCLE_COUNT},
	{"Design Capacity", SBS_DESIGN_CAPACITY},
	{"Design Voltage", SBS_DESIGN_VOLTAGE},
	{"Spec Info", SBS_SPEC_INFO},
	{"Manufacture Date", SBS_MFG_DATE},
	{"Serial Number", SBS_SERIAL_NO},
	{"Manufacture Name", SBS_MFG_NAME},
	{"Device Name", SBS_DEV_NAME},
	{"Device Chemistry", SBS_DEV_CHEMISTRY},
	{"Manufacture Date", SBS_MFG_DATA},
	{"Battery Usage", SBS_BATTERY_USAGE},
	{"Permanent Failure", SBS_PERMANENT_FAILURE},
	{"Battery Log1", SBS_BATTERY_LOG1},
	{"Battery Log2", SBS_BATTERY_LOG2},
	{"FET Temperature", SBS_FET_TEMP},
	{"Optional Mfg Func5", SBS_OPTION_MFG_FUNC5},
	{"Optional Mfg Func4", SBS_OPTION_MFG_FUNC4},
	{"Optional Mfg Func3", SBS_OPTION_MFG_FUNC3},
	{"Optional Mfg Func2", SBS_OPTION_MFG_FUNC2},
	{"Optional Mfg Func1", SBS_OPTION_MFG_FUNC1},

};

void dump_sbs_reg(struct i2c_client *client)
{
	int i;
	unsigned int value;

	printk("Dump Smart Battery Register\n");
	for (i = 0; i < ARRAY_SIZE(reg_tbl); i ++) {
		value = i2c_smbus_read_word_data(client, reg_tbl[i].reg_offset);
		printk("%s: 0x%04x\n", reg_tbl[i].name, value);
	}
}
#else
void dump_sbs_reg(struct i2c_client *client) {}
#endif

struct efikasb_batt_dev_info;

struct efikasb_batt_dev_info {
	struct device *dev;
	struct i2c_client *client;
	struct power_supply *bat;
	struct power_supply *ac_charger;
	char mfg_name[32];
	char model_name[32];
	char serial[32];
	char chemistry[32];

        int batt_in_irq;
        int batt_low_irq;
        int ac_in_irq;

        int batt_in;
        int ac_in;
        int batt_low;
        u32 capacity;

	int (*get_batt_in_status) (void);
        int (*get_batt_low_status) (void);
	int (*get_ac_in_status) (void);
        void (*set_batt_low_led) (int);

        struct timer_list batt_low_timer;
};

static struct efikasb_batt_dev_info *batt = NULL;

static enum power_supply_property efikasb_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
/* 	POWER_SUPPLY_PROP_HEALTH, */
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
 	POWER_SUPPLY_PROP_MODEL_NAME,
 	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,

};

static enum power_supply_property efikasb_ac_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int efikasb_batt_read(struct i2c_client *client, u8 reg, u32 *value)
{
	int ret;
        int retry = 5;

 retry:
	ret = i2c_smbus_read_word_data(client, reg);
	if(ret < 0) {           /* ron: retry i2c again to avoid conflict with PIC */
                if(retry -- > 0)
                        goto retry;
                return ret;
        }

	*value = ret;
	return 0;
}

/* Smart Battery Helper Function */
static int efikasb_batt_get_status(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_BATTERY_STATUS, value);
}

static int efikasb_batt_get_current(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_CURRENT, value);
}

static int efikasb_batt_get_voltage(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_VOLTAGE, value);
}

static int efikasb_batt_get_average_current(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_AVG_CURRENT, value);
}

static int efikasb_batt_get_capacity(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_REL_STATE_OF_CHARGE, value);
}

static int efikasb_batt_get_temperature(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_TEMP, value);
}

static int efikasb_batt_run_time_to_empty(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_RUN_TIME_TO_EMPTY, value);
}

static int efikasb_batt_avg_time_to_empty(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_AVG_TIME_TO_EMPTY, value);
}

static int efikasb_batt_avg_time_to_full(struct efikasb_batt_dev_info *di, u32 *value)
{
	return efikasb_batt_read(di->client, SBS_AVG_TIME_TO_FULL, value);
}

static int efikasb_batt_get_serial_no(struct efikasb_batt_dev_info *di)
{
	int ret;
	u32 sn;

	ret = efikasb_batt_read(di->client, SBS_SERIAL_NO, &sn);
	if(ret != 0)
		return ret;

	sprintf(di->serial, "%d", sn);

	return ret;
}

static int efikasb_batt_get_mfg_name(struct efikasb_batt_dev_info *di)
{
	int ret;
	char mfg[33];

	ret = i2c_smbus_read_i2c_block_data(di->client, SBS_MFG_NAME, 32, mfg);
	if(ret < 0)
		return ret;

        if(mfg[0] > sizeof(di->mfg_name))
                return -EINVAL;

	strncpy(di->mfg_name, &mfg[1], mfg[0]);
	return 0;
}

static int efikasb_batt_get_model_name(struct efikasb_batt_dev_info *di)
{
	int ret;
	char model[33];

	ret = i2c_smbus_read_i2c_block_data(di->client, SBS_DEV_NAME, 32, model);
	if(ret < 0)
		return ret;

        if(model[0] > sizeof(di->model_name))
                return -EINVAL;

	strncpy(di->model_name, &model[1], model[0]);
	return 0;
}

static int efikasb_batt_get_technology(struct efikasb_batt_dev_info *di)
{
	int ret;
	char chem[33];

	ret = i2c_smbus_read_i2c_block_data(di->client, SBS_DEV_CHEMISTRY, 32, chem);
	if(ret < 0)
		return ret;

        if(chem[0] > sizeof(di->chemistry))
                return -EINVAL;

	strncpy(di->chemistry, &chem[1], chem[0]);
/* 	printk("Technology: %s\n", di->chemistry); */

	if (!strcasecmp("NiCd", di->chemistry))
		return POWER_SUPPLY_TECHNOLOGY_NiCd;
	if (!strcasecmp("NiMH", di->chemistry))
		return POWER_SUPPLY_TECHNOLOGY_NiMH;
	if (!strcasecmp("LION", di->chemistry))
		return POWER_SUPPLY_TECHNOLOGY_LION;
	if (!strncasecmp("LI-ION", di->chemistry, 6))
		return POWER_SUPPLY_TECHNOLOGY_LION;
	if (!strcasecmp("LiP", di->chemistry))
		return POWER_SUPPLY_TECHNOLOGY_LIPO;
	return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

}

#define to_efikasb_batt_device_info(x) container_of((x), \
				struct efikasb_batt_dev_info, bat);

static int efikasb_batt_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct efikasb_batt_dev_info *di = batt;
	u32 value;
	int ret;
        int batt_in, ac_in;

        batt_in = di->get_batt_in_status();

	if (!batt_in) {
                val->intval = 0;
		return -ENODEV;
        }

        ac_in = di->get_ac_in_status();

	switch(psp) {
        case POWER_SUPPLY_PROP_PRESENT:
                val->intval = di->get_batt_in_status();
                break;
	case POWER_SUPPLY_PROP_STATUS:
                if (ac_in) {
			ret = efikasb_batt_get_status(di, &value);
			if (ret != 0) {
				val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
				return 0;
			}

			if (value & SBS_STATUS_FULLY_CHARGED)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}

		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = efikasb_batt_get_technology(di);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW: /* mV */
		ret = efikasb_batt_get_voltage(di, &value);
		if(ret != 0) {
                        val->intval = 0;
			break;
                }

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW: /* mA */
		ret = efikasb_batt_get_current(di, &value);
		if(ret != 0) {
                        val->intval = 0;
			return ret;
                }

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG: /* mA in 1 minute rolling avg */
		ret = efikasb_batt_get_average_current(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CAPACITY: /* % percent */
		ret = efikasb_batt_get_capacity(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
                di->capacity = value; /* ron: cache the battery capacity */
		break;

	case POWER_SUPPLY_PROP_TEMP: /* K degree */
		ret = efikasb_batt_get_temperature(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW: /* minutes */
		ret = efikasb_batt_run_time_to_empty(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG: /* minutes */
		ret = efikasb_batt_avg_time_to_empty(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG: /* minutes */
		ret = efikasb_batt_avg_time_to_full(di, &value);
		if(ret != 0)
			return ret;
		val->intval = value;
		break;

 	case POWER_SUPPLY_PROP_MODEL_NAME:
		ret = efikasb_batt_get_model_name(di);
		if(ret < 0)
			return ret;
		val->strval = di->model_name;
		break;

 	case POWER_SUPPLY_PROP_MANUFACTURER:
		ret = efikasb_batt_get_mfg_name(di);
		if(ret < 0)
			return ret;
		val->strval = di->mfg_name;
		break;
 	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		ret = efikasb_batt_get_serial_no(di);
		if(ret < 0)
			return ret;
		val->strval = di->serial;
 		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int efikasb_ac_charger_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct efikasb_batt_dev_info *di = batt;

	switch(psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->get_ac_in_status();
		break;
	default:
                return -EINVAL;
		break;
	}

	return 0;
}

static struct power_supply efikasb_batt = {
	.name = "efikasb_battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = efikasb_batt_props,
	.num_properties = ARRAY_SIZE(efikasb_batt_props),
	.get_property = efikasb_batt_get_property,
	.external_power_changed = NULL,

};

static struct power_supply efikasb_ac_charger = {
	.name = "efikasb_ac_charger",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = efikasb_ac_charger_props,
	.num_properties = ARRAY_SIZE(efikasb_ac_charger_props),
	.get_property = efikasb_ac_charger_get_property,
	.external_power_changed = NULL,
};

static void update_status_worker(struct work_struct *work)
{
        power_supply_changed(batt->ac_charger);
        power_supply_changed(batt->bat);
}


static irqreturn_t efikasb_ac_detect_handler(int irq, void *data)
{
        struct efikasb_batt_dev_info *di = data;

	di->ac_in = di->get_ac_in_status();
	printk("efikasb_ac_charger: AC %s\n",
	       di->ac_in ? "Inserted" : "Removed");

        schedule_delayed_work(&update_status_work, msecs_to_jiffies(1500));

	if (di->ac_in) {
                if(di->set_batt_low_led)
                        di->set_batt_low_led(0);
		set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
                mod_timer(&di->batt_low_timer, jiffies + 10);
	} else {
                mod_timer(&di->batt_low_timer, jiffies + 10);
		set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
        }

	return IRQ_HANDLED;
}

static irqreturn_t efikasb_batt_detect_handler(int irq, void *data)
{
        struct efikasb_batt_dev_info *di = data;

	di->batt_in = di->get_batt_in_status();
	printk("efikasb_battery: Battery %s\n",
	       di->batt_in ? "Inserted" : "Removed");

        schedule_delayed_work(&update_status_work, msecs_to_jiffies(1500));

	if (di->batt_in) {
                mod_timer(&di->batt_low_timer, jiffies + 10);
		set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
	} else {
                if(di->set_batt_low_led)
                        di->set_batt_low_led(0);
		set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
        }

	return IRQ_HANDLED;
}

static void power_off_worker(struct work_struct *work)
{
        if(!batt->ac_in && batt->batt_low && batt->capacity == 0) {
                sys_sync();
                kernel_power_off();
        }
}

static irqreturn_t efikasb_batt_low_handler(int irq, void *data)
{
        struct efikasb_batt_dev_info *di = data;

        di->batt_low = batt->get_batt_low_status();
        printk("efikasb_batter: Battery %s\n",
               di->batt_low ? "Low" : "Normal");

        if (di->batt_low) {
                set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
        } else {
                set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
        }

        return IRQ_HANDLED;
}

static void batt_capacity_worker(struct work_struct *work)
{
        int ret;
        u32 capacity;

        if(batt->batt_in) {
                ret = efikasb_batt_get_capacity(batt, &capacity);
                if(ret == 0)
                        batt->capacity = capacity;

                if(!batt->ac_in && batt->capacity == 0) {
                        sys_sync();
                        kernel_power_off();
                }
        }

}

static void batt_low_fn(unsigned long data)
{
        struct efikasb_batt_dev_info *di = (struct efikasb_batt_dev_info *)data;

        if(!di->batt_in)
                return;

        if(!di->ac_in && di->batt_in) { /* battery is discharging */
                schedule_work(&batt_capacity_work);
                if(di->set_batt_low_led) {
                        if(di->capacity <= 10)
                                di->set_batt_low_led(1);
                        else
                                di->set_batt_low_led(0);

                        if(di->capacity <= 11 && di->capacity > 9) {
                                mod_timer(&di->batt_low_timer, jiffies + 4 * HZ);
                        }

                        if(di->capacity <= 9 || di->capacity > 11)  {
                                mod_timer(&di->batt_low_timer, jiffies + 60 * HZ);
                        }
                } else {
                        mod_timer(&di->batt_low_timer, jiffies + 60 * HZ);
                }
        } else if(di->batt_in) { /* battery is charging */
                mod_timer(&di->batt_low_timer, jiffies + 60 * HZ);
        }

        /* battery is not charging and capacity is critical low, power off immediately */
        if(!di->ac_in && di->batt_in && di->capacity == 0)
                schedule_delayed_work(&power_off_work, msecs_to_jiffies(1000));

}

static int efikasb_batt_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct efikasb_batt_dev_info *di;
	struct mxc_battery_platform_data *platform_data;
	int retval = 0;

	platform_data = (struct mxc_battery_platform_data *)client->dev.platform_data;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if(!di) {
		dev_err(&client->dev, "failed to allocate device info\n");
		retval = -ENOMEM;
		return retval;
	}

	batt = di;

	i2c_set_clientdata(client, di);
	di->dev = &client->dev;
	di->client = client;
	di->bat = &efikasb_batt;
	di->ac_charger = &efikasb_ac_charger;

	di->get_batt_in_status = platform_data->get_batt_in_status;
        di->get_batt_low_status = platform_data->get_batt_low_status;
	di->get_ac_in_status = platform_data->get_ac_in_status;
        di->set_batt_low_led = platform_data->set_batt_low_led;

        if(di->get_batt_in_status)
                di->batt_in = di->get_batt_in_status();
        if(di->get_ac_in_status)
                di->ac_in = di->get_ac_in_status();
        if(di->get_batt_low_status)
                di->batt_low = di->get_batt_low_status();

        di->batt_in_irq = platform_data->batt_in_irq;
        di->batt_low_irq = platform_data->batt_low_irq;
        di->ac_in_irq = platform_data->ac_in_irq;

#ifndef CONFIG_EFIKASB_EXPERIMENTAL_OS
        if(di->batt_in) {
                retval = request_irq(di->batt_in_irq,
                                     efikasb_batt_detect_handler, \
                                     IRQ_TYPE_EDGE_RISING,
                                     "efikasb_battery", di);
        } else {
                retval = request_irq(di->batt_in_irq,
                                     efikasb_batt_detect_handler,
                                     IRQ_TYPE_EDGE_FALLING,
                                     "efikasb_battery", di);
        }
        if(retval)
                goto batt_irq_failed;

        if(di->ac_in) {
                retval = request_irq(di->ac_in_irq,
                                     efikasb_ac_detect_handler,
                                     IRQ_TYPE_EDGE_RISING,
                                     "efikasb_ac_charger", di);
        } else {
                retval = request_irq(di->ac_in_irq,
                                     efikasb_ac_detect_handler,
                                     IRQ_TYPE_EDGE_FALLING,
                                     "efikasb_ac_charger", di);
        }
        if(retval)
                goto ac_irq_failed;

        if(di->batt_low) {
                retval = request_irq(di->batt_low_irq,
                                  efikasb_batt_low_handler,
                                  IRQ_TYPE_LEVEL_HIGH, "efikasb_batt_low", di);
        } else {
                retval = request_irq(di->batt_low_irq,
                                  efikasb_batt_low_handler,
                                  IRQ_TYPE_LEVEL_LOW, "efikasb_batt_low", di);
        }
        if(retval)
                goto batt_low_irq_failed;

        /* ron: the battery low pin is triggered at 10% battery capacity,
           battery critical low won't generate wake up event,
           the battery power may be exhauseted */
        /* enable_irq_wake(di->batt_low_irq); */
#endif

	retval = power_supply_register(&client->dev, &efikasb_batt);
	if(retval) {
		dev_err(&client->dev, "failed to register battery\n");
		goto batt_failed;
	}

	retval =power_supply_register(&client->dev, &efikasb_ac_charger);
	if(retval) {
		dev_err(&client->dev, "failed to register ac charger\n");
		goto ac_charger_failed;
	}
	if(di->batt_in) {
                efikasb_batt_get_mfg_name(di);
                efikasb_batt_get_model_name(di);
                efikasb_batt_get_capacity(di, &di->capacity);
        }


	printk("Probe Efika MX Smartbook Battery: %s %s %s: %d%%, AC %s\n",
	       di->mfg_name, di->model_name,
	       di->batt_in ? "Inserted" : "Removed", di->capacity,
	       di->ac_in ? "Inserted" : "Removed");

	dump_sbs_reg(client);

        init_timer(&di->batt_low_timer);
        di->batt_low_timer.data = (unsigned long)di;
        di->batt_low_timer.function = batt_low_fn;
        di->batt_low_timer.expires = jiffies + 10;
        if(!di->ac_in && di->batt_in)
                add_timer(&di->batt_low_timer);

        /* ron: if battery critical low, shutdown immediately */
        if(!di->ac_in && di->batt_low && di->capacity == 0) {
                printk("Battery critical low, shutdown now....\n");
                schedule_delayed_work(&power_off_work, msecs_to_jiffies(1000));
        }

	return 0;

 ac_charger_failed:
	power_supply_unregister(&efikasb_batt);
 batt_failed:
        free_irq(di->batt_low_irq, NULL);
 batt_low_irq_failed:
        free_irq(di->ac_in_irq, NULL);
 ac_irq_failed:
        free_irq(di->batt_in_irq, NULL);
 batt_irq_failed:
        i2c_set_clientdata(client, NULL);
	kfree(di);
	return retval;
}

static int efikasb_batt_i2c_remove(struct i2c_client *client)
{
	struct efikasb_batt_dev_info *di = i2c_get_clientdata(client);

	power_supply_unregister(&efikasb_batt);
        power_supply_unregister(&efikasb_ac_charger);
        free_irq(di->batt_in_irq, NULL);
        free_irq(di->ac_in_irq, NULL);
	kfree(di);
	batt = NULL;

	return 0;
}

static const struct i2c_device_id efikasb_batt_id[] = {
	{ "efikasb-battery", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, efikasb_batt_id);

static struct i2c_driver efikasb_batt_i2c_driver = {
	.driver = {
		.name = "efikasb-battery",
	},
	.probe = efikasb_batt_i2c_probe,
	.remove = efikasb_batt_i2c_remove,
	.id_table = efikasb_batt_id,
};

static int __init efikasb_batt_init(void)
{
	int ret;

	ret = i2c_add_driver(&efikasb_batt_i2c_driver);
	if(ret)
		printk(KERN_ERR "Unable to register Efika MX Smartbook Battery Driver\n");

	return ret;
}
module_init(efikasb_batt_init);

static void __exit efikasb_batt_exit(void)
{
	i2c_del_driver(&efikasb_batt_i2c_driver);

}
module_exit(efikasb_batt_exit);

MODULE_AUTHOR("Ron Lee <ron1_lee@pegatroncorp.com>");
MODULE_DESCRIPTION("Efika MX Smart Battery Driver");
MODULE_LICENSE("GPL");


