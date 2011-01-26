/* vim: set noet ts=8 sts=8 sw=8 : */
/*
 * Copyright © 2010 Saleem Abdulrasool <compnerd@compnerd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/i2c.h>
#include <linux/sbs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/power_supply.h>

/* logging helpers */
#define ERROR(fmt, ...)		printk(KERN_ERR     "SBS: " fmt, __VA_ARGS__)
#define WARNING(fmt, ...)	printk(KERN_WARNING "SBS: " fmt, __VA_ARGS__)
#define INFO(fmt, ...)		printk(KERN_INFO    "SBS: " fmt, __VA_ARGS__)
#define DEBUG(fmt, ...)		printk(KERN_DEBUG   "SBS: " fmt, __VA_ARGS__)
#define CONTINUE(fmt, ...)	printk(KERN_CONT fmt, __VA_ARGS__)

/* Smart Battery Messages */
#define SBS_MANUFACTURER_ACCESS		(0x00)
#define SBS_REMAINING_CAPACITY_ALARM	(0x01)
#define SBS_REMAINING_TIME_ALARM	(0x02)
#define SBS_BATTERY_MODE		(0x03)
#define SBS_AT_RATE			(0x04)
#define SBS_AT_RATE_TIME_TO_FULL	(0x05)
#define SBS_AT_RATE_TIME_TO_EMPTY	(0x06)
#define SBS_AT_RATE_OK			(0x07)
#define SBS_TEMPERATURE			(0x08)
#define SBS_VOLTAGE			(0x09)
#define SBS_CURRENT			(0x0a)
#define SBS_AVERAGE_CURRENT		(0x0b)
#define SBS_MAX_ERROR			(0x0c)
#define SBS_RELATIVE_STATE_OF_CHARGE	(0x0d)
#define SBS_ABSOLUTE_STATE_OF_CHARGE	(0x0e)
#define SBS_REMAINING_CAPACITY		(0x0f)
#define SBS_FULL_CHARGE_CAPACITY	(0x10)
#define SBS_RUN_TIME_TO_EMPTY		(0x11)
#define SBS_AVERAGE_TIME_TO_EMPTY	(0x12)
#define SBS_AVERAGE_TIME_TO_FULL	(0x13)
#define SBS_CHARGING_CURRENT		(0x14)
#define SBS_CHARGING_VOLTAGE		(0x15)
#define SBS_BATTERY_STATUS		(0x16)
#define SBS_ALARM_WARNING		(0x16)
#define SBS_BATTERY_CYCLE_COUNT		(0x17)
#define SBS_DESIGN_CAPACITY		(0x18)
#define SBS_DESIGN_VOLTAGE		(0x19)
#define SBS_SPECIFICATION_INFO		(0x1a)
#define SBS_MANUFACTURE_DATE		(0x1b)
#define SBS_SERIAL_NUMBER		(0x1c)
#define SBS_MANUFACTURER_NAME		(0x20)
#define SBS_DEVICE_NAME			(0x21)
#define SBS_DEVICE_CHEMISTRY		(0x22)
#define SBS_MANUFACTURER_DATA		(0x23)

/* Battery Mode Flags */
#define INTERNAL_CHARGE_CONTROLLER_CAPABILITY	(1 << 0)
#define BATTERY_ROLE_CAPABILITY			(1 << 1)
#define CAPACITY_RELEARN			(1 << 7)
#define INTERNAL_CHARGE_CONTROLLER_ENABLED	(1 << 8)
#define PRIMARY_BATTERY				(1 << 9)
#define ALARM_MODE				(1 << 13)
#define CHARGER_MODE				(1 << 14)
#define CAPACITY_MODE				(1 << 15)


/* module parameters */
static unsigned int cache_time = 1000;
module_param(cache_time, uint, 0644);
MODULE_PARM_DESC(cache_time, "cache time in milliseconds");


struct sbs_string {
	u8 length;
	u8 data[];
};

struct sbs_battery {
	struct i2c_client        *client;
	struct sbs_platform_data *platform;

	struct {
		u32 timestamp;

		/* dynamic information */
		u16 battery_mode;
		u16 temperature;
		u16 voltage;
		u16 current_now;
		u16 average_current;
		u16 absolute_state_of_charge;
		u16 remaining_capacity;

		/* affected by battery_mode */
		u16 full_charge_capacity;
		u16 design_capacity;

		/* static information */
		u16 battery_cycle_count;
		u16 design_voltage;
		u16 specification_info;
		u16 serial_number;
		 u8 manufacturer_name[I2C_SMBUS_BLOCK_MAX];
		 u8 device_name[I2C_SMBUS_BLOCK_MAX];
		 u8 device_chemistry[I2C_SMBUS_BLOCK_MAX];
	} cache;

	unsigned int vscale;
	unsigned int ipscale;

	struct power_supply battery;
	struct power_supply mains;
};

struct sbs_battery_field {
	u8     command;
	enum {
		SBS_READ_WORD,
		SBS_READ_BLOCK,
	}      size;
	size_t offset;
};


/* Battery State */
static enum power_supply_property sbs_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
	POWER_SUPPLY_PROP_CYCLE_COUNT,
#endif
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,

	/* Capacity Mode */
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,

	/* Energy Mode */
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static inline int __chem_to_tech(const u8 * const chem)
{
	const struct sbs_string * const str = (struct sbs_string *) chem;

	if (!strcasecmp(str->data, "PbAc")) /* Lead Acid */
		return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	if (!strcasecmp(str->data, "LION")) /* Lithium Ion */
		return POWER_SUPPLY_TECHNOLOGY_LION;
	if (!strcasecmp(str->data, "NiCd")) /* Nickel Cadmium */
		return POWER_SUPPLY_TECHNOLOGY_NiCd;
	if (!strcasecmp(str->data, "NiMH")) /* Nickel Metal Hydride */
		return POWER_SUPPLY_TECHNOLOGY_NiMH;
	if (!strcasecmp(str->data, "NiZn")) /* Nickel Zinc */
		return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	if (!strcasecmp(str->data, "RAM"))  /* Rechargable Alkaline-Manganese */
		return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	if (!strcasecmp(str->data, "ZnAr")) /* Zinc Air */
		return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	if (!strcasecmp(str->data, "LiP"))  /* Lithium Polymer */
		return POWER_SUPPLY_TECHNOLOGY_LIPO;

	DEBUG("Unknown Device Chemistry: %.*s", str->length, str->data);
	return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
}

static inline const int __mV_2_uV(const struct sbs_battery * const batt,
				  const int mv)
{
	return batt->vscale * mv * 1000;
}

static inline const int __mA_2_uA(const struct sbs_battery * const batt,
				  const int ma)
{
	return batt->ipscale * ma * 1000;
}

static inline const int __dK_2_dC(const int dk)
{
	return dk - 2730;
}

static inline const int __mW_2_uW(const struct sbs_battery * const batt,
				  const int mw)
{
	/* SBS reports mWh in 10 mWh units */
	return batt->vscale * batt->ipscale * mw * 1000 * 10;
}

static inline void read_battery_field(struct sbs_battery * const batt,
				      const struct sbs_battery_field * const field)
{
	u8 * const cache = (u8 *) batt;

	switch (field->size) {
	case SBS_READ_WORD:
		*((u16 *) &cache[field->offset]) =
			i2c_smbus_read_word_data(batt->client,
					         field->command);
		break;
	case SBS_READ_BLOCK:
		BUILD_BUG_ON(sizeof(*batt) - field->offset >= I2C_SMBUS_BLOCK_MAX);
		i2c_smbus_read_i2c_block_data(batt->client,
					      field->command,
					      I2C_SMBUS_BLOCK_MAX,
					      &cache[field->offset]);
		break;
	}
}

static const struct sbs_battery_field sbs_state_fields[] = {
	{ SBS_BATTERY_MODE,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.battery_mode), },
	{ SBS_TEMPERATURE,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.temperature), },
	{ SBS_VOLTAGE,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.voltage), },
	{ SBS_CURRENT,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.current_now), },
	{ SBS_AVERAGE_CURRENT,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.average_current), },
	{ SBS_ABSOLUTE_STATE_OF_CHARGE,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.absolute_state_of_charge), },
	{ SBS_REMAINING_CAPACITY,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.remaining_capacity), },
	{ SBS_FULL_CHARGE_CAPACITY,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.full_charge_capacity), },
	{ SBS_DESIGN_CAPACITY,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.design_capacity), },
};

static void sbs_get_battery_state(struct sbs_battery *batt)
{
	unsigned int i;

	if (likely(batt->cache.timestamp))
		if (time_before(jiffies,
				batt->cache.timestamp + msecs_to_jiffies(cache_time)))
			return;

	for (i = 0; i < ARRAY_SIZE(sbs_state_fields); i++)
		read_battery_field(batt, &sbs_state_fields[i]);

	batt->cache.timestamp = jiffies;
}

static int sbs_get_battery_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct sbs_battery *batt =
		container_of(psy, struct sbs_battery, battery);
	const struct sbs_string *str;

	/* don't bother updating the cache for static data */

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = __chem_to_tech(batt->cache.device_chemistry);
		return 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = batt->cache.battery_cycle_count;
		return 0;
#endif
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:                   /* uV */
		val->intval = __mV_2_uV(batt, batt->cache.design_voltage);
		return 0;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		str = (struct sbs_string *) batt->cache.device_name;
		val->strval = kstrndup(str->data, str->length, GFP_KERNEL);
		return 0;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		str = (struct sbs_string *) batt->cache.manufacturer_name;
		val->strval = kstrndup(str->data, str->length, GFP_KERNEL);
		return 0;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = kasprintf(GFP_KERNEL,
					"%u", batt->cache.serial_number);
		return 0;
	default:
		break;
	}

	sbs_get_battery_state(batt);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if ((s16) batt->cache.current_now < 0)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if ((s16) batt->cache.current_now > 0)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = batt->platform->get_battery_status();
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:                          /* uV */
		val->intval = __mV_2_uV(batt, batt->cache.voltage);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:                          /* µA */
		val->intval = __mA_2_uA(batt, abs(batt->cache.current_now));
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:                          /* µA */
		val->intval = __mA_2_uA(batt, abs(batt->cache.average_current));
		break;
	case POWER_SUPPLY_PROP_CAPACITY:                             /* % */
		val->intval = batt->cache.absolute_state_of_charge;
		break;
	case POWER_SUPPLY_PROP_TEMP:                                 /* .1 °C */
		val->intval = __dK_2_dC(batt->cache.temperature);
		break;

	/* Capacity Mode */
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:                   /* µAh */
		if (batt->cache.battery_mode & CAPACITY_MODE)
			return -EINVAL;
		val->intval = __mA_2_uA(batt, batt->cache.design_capacity);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:                          /* µAh */
		if (batt->cache.battery_mode & CAPACITY_MODE)
			return -EINVAL;
		val->intval = __mA_2_uA(batt, batt->cache.full_charge_capacity);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:                           /* µAh */
		if (batt->cache.battery_mode & CAPACITY_MODE)
			return -EINVAL;
		val->intval = __mA_2_uA(batt, batt->cache.remaining_capacity);
		break;

	/* Energy Mode */
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:                   /* µWh */
		if (!(batt->cache.battery_mode & CAPACITY_MODE))
			return -EINVAL;
		val->intval = __mW_2_uW(batt, batt->cache.design_capacity);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:                          /* µWh */
		if (!(batt->cache.battery_mode & CAPACITY_MODE))
			return -EINVAL;
		val->intval = __mW_2_uW(batt, batt->cache.full_charge_capacity);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:                           /* µWh */
		if (!(batt->cache.battery_mode & CAPACITY_MODE))
			return -EINVAL;
		val->intval = __mW_2_uW(batt, batt->cache.remaining_capacity);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* Battery Information */
static const struct sbs_battery_field sbs_info_fields[] = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
	{ SBS_BATTERY_CYCLE_COUNT,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.battery_cycle_count), },
#endif
	{ SBS_DESIGN_VOLTAGE,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.design_voltage), },
	{ SBS_SPECIFICATION_INFO,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.specification_info), },
	{ SBS_SERIAL_NUMBER,
	  SBS_READ_WORD,
	  offsetof(struct sbs_battery, cache.serial_number), },
	{ SBS_MANUFACTURER_NAME,
	  SBS_READ_BLOCK,
	  offsetof(struct sbs_battery, cache.manufacturer_name), },
	{ SBS_DEVICE_NAME,
	  SBS_READ_BLOCK,
	  offsetof(struct sbs_battery, cache.device_name), },
	{ SBS_DEVICE_CHEMISTRY,
	  SBS_READ_BLOCK,
	  offsetof(struct sbs_battery, cache.device_chemistry), },
};

static inline unsigned int ipow(const int base, int exp)
{
	unsigned int value = base;

	if (unlikely(!exp))
		return 1;

	while (--exp)
		value *= base;

	return value;
}

static void sbs_get_battery_info(struct sbs_battery *batt)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sbs_info_fields); i++)
		read_battery_field(batt, &sbs_info_fields[i]);

	batt->vscale = ipow(10, (batt->cache.specification_info >> 8) & 0xf);
	batt->ipscale = ipow(10, (batt->cache.specification_info >> 12) & 0xf);
}

static struct power_supply sbs_battery = {
	.name           = "sbs-battery",
	.type           = POWER_SUPPLY_TYPE_BATTERY,
	.properties     = sbs_battery_properties,
	.num_properties = ARRAY_SIZE(sbs_battery_properties),
	.get_property   = sbs_get_battery_property,
};


/* Mains Information */
static enum power_supply_property sbs_mains_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int sbs_get_mains_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct sbs_battery *batt =
		container_of(psy, struct sbs_battery, mains);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = batt->platform->get_mains_status();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct power_supply sbs_mains = {
	.name           = "sbs-mains",
	.type           = POWER_SUPPLY_TYPE_MAINS,
	.properties     = sbs_mains_properties,
	.num_properties = ARRAY_SIZE(sbs_mains_properties),
	.get_property   = sbs_get_mains_property,
};


static int __devinit sbs_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	struct sbs_battery *batt;
	int ret;

	batt = kzalloc(sizeof(*batt), GFP_KERNEL);
	if (!batt)
		return -ENOMEM;

	batt->client = client;
	batt->platform = client->dev.platform_data;

	batt->mains = sbs_mains;
	batt->battery = sbs_battery;

	i2c_set_clientdata(client, batt);

	if ((ret = power_supply_register(&client->dev, &batt->battery)) < 0)
		goto error;

	if ((ret = power_supply_register(&client->dev, &batt->mains)) < 0) {
		power_supply_unregister(&batt->battery);
		goto error;
	}

	sbs_get_battery_info(batt);

	return 0;

error:
	kfree(batt);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit sbs_remove(struct i2c_client *client)
{
	struct sbs_battery *batt;

	batt = i2c_get_clientdata(client);
	if (batt) {
		power_supply_unregister(&batt->mains);
		power_supply_unregister(&batt->battery);

		kfree(batt);
		i2c_set_clientdata(client, NULL);
	}

	return 0;
}

static const struct i2c_device_id sbs_device_table[] = {
	{ "smart-battery", 0 },
	{ },
};

static struct i2c_driver sbs_driver = {
	.driver   = { .name = "sbs", },
	.probe    = sbs_probe,
	.remove   = sbs_remove,
	.id_table = sbs_device_table,
};

static int __init sbs_init(void)
{
	return i2c_add_driver(&sbs_driver);
}

static void __exit sbs_exit(void)
{
	i2c_del_driver(&sbs_driver);
}

module_init(sbs_init);
module_exit(sbs_exit);

MODULE_AUTHOR("Saleem Abdulrasool <compnerd@compnerd.org>");
MODULE_LICENSE("BSD-3");
MODULE_DESCRIPTION("Smart Battery");
MODULE_DEVICE_TABLE(i2c, sbs_device_table);

