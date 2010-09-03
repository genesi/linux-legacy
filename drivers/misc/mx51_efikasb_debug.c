#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/i2c.h>
#include <mach/hardware.h>

static ssize_t reg_dump_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t reg_dump_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char *end;
	char *token;
	char *running;
	const char delimiters[] = " ";
	u32 addr, value;
	int len = 1;
	int i;

	running = (char *)buf;

	token = strsep(&running, delimiters);
	addr = simple_strtoul(token, &end, 16);
	token = strsep(&running, delimiters);
	if(token != NULL)
		len = simple_strtoul(token, &end, 10);

	if(len <= 1)
		len = 1;

	for (i = 0; i < len; i ++) {
		value = __raw_readl(IO_ADDRESS(addr));
		printk("[0x%08x] = 0x%08x\n", addr, value);
		addr += 4;
	}

	return count;
}

static ssize_t reg_write_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t reg_write_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char *end;
	char *token;
	char *running;
	const char delimiters[] = "=";
	u32 addr, value;

	running = (char *)buf;

	token = strsep(&running, delimiters);
	addr = simple_strtoul(token, &end, 16);
	token = strsep(&running, delimiters);
	if(token != NULL) {
		value = simple_strtoul(token, &end, 16);
	} else {
		value = __raw_readl(IO_ADDRESS(addr));
		printk("[0x%08x] = 0x%08x\n", addr, value);
		return -EINVAL;
	}

	__raw_writel(value, IO_ADDRESS(addr));
	value = __raw_readl(IO_ADDRESS(addr));

	printk("[0x%08x] = 0x%08x\n", addr, value);

	return count;
}

struct pmic_reg_tbl {
	char *name;
	int reg_addr;
};

struct pmic_reg_tbl pmic_reg_tbl[] = {
	{"INT_STATUS0",REG_INT_STATUS0},
	{"INT_MASK0",REG_INT_MASK0},
	{"INT_SENSE0",REG_INT_SENSE0},
	{"INT_STATUS1",REG_INT_STATUS1},
	{"INT_MASK1",REG_INT_MASK1},
	{"INT_SENSE1",REG_INT_SENSE1},
	{"PU_MODE_S",REG_PU_MODE_S},
	{"ID",REG_IDENTIFICATION},
	{"UNUSED0",REG_UNUSED0},
	{"ACC0",REG_ACC0},
	{"ACC1",REG_ACC1},
	{"UNUSED1",REG_UNUSED1},
	{"UNUSED2",REG_UNUSED2},
	{"POWER_CTL0",REG_POWER_CTL0},
	{"POWER_CTL1",REG_POWER_CTL1},
	{"POWER_CTL2",REG_POWER_CTL2},
	{"REGEN_ASSIGN",REG_REGEN_ASSIGN},
	{"UNUSED3",REG_UNUSED3},
	{"MEM_A",REG_MEM_A},
	{"MEM_B",REG_MEM_B},
	{"RTC_TIME",REG_RTC_TIME},
	{"RTC_ALARM",REG_RTC_ALARM},
	{"RTC_DAY",REG_RTC_DAY},
	{"RTC_DAY_ALARM",REG_RTC_DAY_ALARM},
	{"SW0",REG_SW_0},
	{"SW1",REG_SW_1},
	{"SW2",REG_SW_2},
	{"SW3",REG_SW_3},
	{"SW4",REG_SW_4},
	{"SW5",REG_SW_5},
	{"SETTING0",REG_SETTING_0},
	{"SETTING1",REG_SETTING_1},
	{"MODE_0",REG_MODE_0},
	{"MODE_1",REG_MODE_1},
	{"POWER_MISC",REG_POWER_MISC},
	{"UNUSED4",REG_UNUSED4},
	{"UNUSED5",REG_UNUSED5},
	{"UNUSED6",REG_UNUSED6},
	{"UNUSED7",REG_UNUSED7},
	{"UNUSED8",REG_UNUSED8},
	{"UNUSED9",REG_UNUSED9},
	{"UNUSED10",REG_UNUSED10},
	{"UNUSED11",REG_UNUSED11},
	{"ADC0",REG_ADC0},
	{"ADC1",REG_ADC1},
	{"ADC2",REG_ADC2},
	{"ADC3",REG_ADC3},
	{"ADC4",REG_ADC4},
	{"CHARGE",REG_CHARGE},
	{"USB0",REG_USB0},
	{"USB1",REG_USB1},
	{"LED_CTL0",REG_LED_CTL0},
	{"LED_CTL1",REG_LED_CTL1},
	{"LED_CTL2",REG_LED_CTL2},
	{"LED_CTL3",REG_LED_CTL3},
	{"UNUSED12",REG_UNUSED12},
	{"UNUSED13",REG_UNUSED13},
	{"TRIM0",REG_TRIM0},
	{"TRIM1",REG_TRIM1},
	{"TEST0",REG_TEST0},
	{"TEST1",REG_TEST1},
	{"TEST2",REG_TEST2},
	{"TEST3",REG_TEST3},
	{"TEST4",REG_TEST4},

};

static ssize_t pmic_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 value;
	int i;

	for(i = 0; i < ARRAY_SIZE(pmic_reg_tbl); i++) {
		pmic_read(i, &value);
		printk("%s: [%d] = 0x%08x\n", pmic_reg_tbl[i].name, i, value);
	};

	return 0/* strlen(buf) */;

}

static ssize_t pmic_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char *end;
	char *token;
	char *running;
	const char delimiters[] = "=";
	u32 addr, value;

	running = (char *)buf;

	token = strsep(&running, delimiters);
	addr = simple_strtoul(token, &end, 10);
	if(addr > 63) {
		return -EINVAL;
	}

	token = strsep(&running, delimiters);
	if(token != NULL) {
		value = simple_strtoul(token, &end, 16);
	} else {
		value = pmic_read(addr, &value);
		printk("[%d] = 0x%08x\n", addr, value);
		return -EINVAL;
	}

	pmic_write(addr, value);
	pmic_read(addr, &value);

	printk("[%d] = 0x%08x\n", addr, value);

	return count;
}

static ssize_t edid_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 buf0[2] = {0, 0};
	int data = 0;
	u16 addr = 0x50;
	u8 edid[128];
	int i;
	struct i2c_adapter *adp;
	struct i2c_msg msg[2] = {
		{
		.addr	= addr,
		.flags	= 0,
		.len	= 1,
		.buf	= buf0,
		}, {
		.addr	= addr,
		.flags	= I2C_M_RD,
		.len	= 128,
		.buf	= edid,
		},
	};

	adp = i2c_get_adapter(1);

	data = i2c_transfer(adp, msg, 2);
	if(data <= 0)
		return -ENODEV;

	for (i = 0; i < 128; i ++) {
		printk("[%x]=%x\n", i, edid[i]);
	}

	return 0;
}

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

struct sbs_reg_tbl {
	char *name;
	unsigned char reg_offset;
};

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

static ssize_t battery_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct i2c_adapter *adp;
	union i2c_smbus_data data;
	int status;
	int i;

	adp = i2c_get_adapter(1);

	for (i = 0; i < ARRAY_SIZE(reg_tbl); i++) {
		status = i2c_smbus_xfer(adp, 0x0b, 0,
					I2C_SMBUS_READ, reg_tbl[i].reg_offset,
					I2C_SMBUS_WORD_DATA, &data);
		if(status < 0)
			return -ENODEV;
		printk("%s: 0x%04x\n", reg_tbl[i].name, data.word);
	}

	return 0;
}

static ssize_t mtl017_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct i2c_adapter *adp;
	union i2c_smbus_data data;
	int status;
	int i;

	adp = i2c_get_adapter(1);

	for (i = 0; i < 0xff; i++) {
		status = i2c_smbus_xfer(adp, 0x3a, 0,
					I2C_SMBUS_READ, i,
					I2C_SMBUS_BYTE_DATA, &data);
		if(status < 0)
			return -ENODEV;
		printk("[0x%x]: 0x%02x\n", i, data.byte);
	}
	return 0;

}

static ssize_t mtl017_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	return count;
}

#define SGTL5000_CHIP_ID			0x0000
#define SGTL5000_CHIP_DIG_POWER			0x0002
#define SGTL5000_CHIP_CLK_CTRL			0x0004
#define SGTL5000_CHIP_I2S_CTRL			0x0006
#define SGTL5000_CHIP_SSS_CTRL			0x000a
#define SGTL5000_CHIP_ADCDAC_CTRL		0x000e
#define SGTL5000_CHIP_DAC_VOL			0x0010
#define SGTL5000_CHIP_PAD_STRENGTH		0x0014
#define SGTL5000_CHIP_ANA_ADC_CTRL		0x0020
#define SGTL5000_CHIP_ANA_HP_CTRL		0x0022
#define SGTL5000_CHIP_ANA_CTRL			0x0024
#define SGTL5000_CHIP_LINREG_CTRL		0x0026
#define SGTL5000_CHIP_REF_CTRL			0x0028
#define SGTL5000_CHIP_MIC_CTRL			0x002a
#define SGTL5000_CHIP_LINE_OUT_CTRL		0x002c
#define SGTL5000_CHIP_LINE_OUT_VOL		0x002e
#define SGTL5000_CHIP_ANA_POWER			0x0030
#define SGTL5000_CHIP_PLL_CTRL			0x0032
#define SGTL5000_CHIP_CLK_TOP_CTRL		0x0034
#define SGTL5000_CHIP_ANA_STATUS		0x0036
#define SGTL5000_CHIP_SHORT_CTRL		0x003c
#define SGTL5000_CHIP_ANA_TEST2			0x003a
#define SGTL5000_DAP_CTRL			0x0100
#define SGTL5000_DAP_PEQ			0x0102
#define SGTL5000_DAP_BASS_ENHANCE		0x0104
#define SGTL5000_DAP_BASS_ENHANCE_CTRL		0x0106
#define SGTL5000_DAP_AUDIO_EQ			0x0108
#define SGTL5000_DAP_SURROUND			0x010a
#define SGTL5000_DAP_FLT_COEF_ACCESS		0x010c
#define SGTL5000_DAP_COEF_WR_B0_MSB		0x010e
#define SGTL5000_DAP_COEF_WR_B0_LSB		0x0110
#define SGTL5000_DAP_EQ_BASS_BAND0		0x0116
#define SGTL5000_DAP_EQ_BASS_BAND1		0x0118
#define SGTL5000_DAP_EQ_BASS_BAND2		0x011a
#define SGTL5000_DAP_EQ_BASS_BAND3		0x011c
#define SGTL5000_DAP_EQ_BASS_BAND4		0x011e
#define SGTL5000_DAP_MAIN_CHAN			0x0120
#define SGTL5000_DAP_MIX_CHAN			0x0122
#define SGTL5000_DAP_AVC_CTRL			0x0124
#define SGTL5000_DAP_AVC_THRESHOLD		0x0126
#define SGTL5000_DAP_AVC_ATTACK			0x0128
#define SGTL5000_DAP_AVC_DECAY			0x012a
#define SGTL5000_DAP_COEF_WR_B1_MSB		0x012c
#define SGTL5000_DAP_COEF_WR_B1_LSB		0x012e
#define SGTL5000_DAP_COEF_WR_B2_MSB		0x0130
#define SGTL5000_DAP_COEF_WR_B2_LSB		0x0132
#define SGTL5000_DAP_COEF_WR_A1_MSB		0x0134
#define SGTL5000_DAP_COEF_WR_A1_LSB		0x0136
#define SGTL5000_DAP_COEF_WR_A2_MSB		0x0138
#define SGTL5000_DAP_COEF_WR_A2_LSB		0x013a

struct sgtl_reg_tbl {
	char *name;
	unsigned int reg_addr;
};

struct sgtl_reg_tbl sgtl_tbl[] = {
	{"ID", SGTL5000_CHIP_ID},
	{"POWER", SGTL5000_CHIP_DIG_POWER},
	{"CLK_CTRL", SGTL5000_CHIP_CLK_CTRL},
	{"I2S_CTRL", SGTL5000_CHIP_I2S_CTRL},
	{"SSS_CTRL", SGTL5000_CHIP_SSS_CTRL},
	{"ADCDAC_CTRL", SGTL5000_CHIP_ADCDAC_CTRL},
	{"DAC_VOL", SGTL5000_CHIP_DAC_VOL},
	{"PAD_STRENGTH", SGTL5000_CHIP_PAD_STRENGTH},
	{"ANA_ADC_CTRL", SGTL5000_CHIP_ANA_ADC_CTRL},
	{"ANA_HP_CTRL", SGTL5000_CHIP_ANA_HP_CTRL},
	{"ANA_CTRL", SGTL5000_CHIP_ANA_CTRL},
	{"LINREG_CTRL", SGTL5000_CHIP_LINREG_CTRL},
	{"REF_CTRL", SGTL5000_CHIP_REF_CTRL},
	{"MIC_CTRL", SGTL5000_CHIP_MIC_CTRL},
	{"LINE_OUT_CTRL", SGTL5000_CHIP_LINE_OUT_CTRL},
	{"LINE_OUT_VOL", SGTL5000_CHIP_LINE_OUT_VOL},
	{"ANA_POWER", SGTL5000_CHIP_ANA_POWER},
	{"PLL_CTRL", SGTL5000_CHIP_PLL_CTRL},
	{"CLK_TOP_CTRL", SGTL5000_CHIP_CLK_TOP_CTRL},
	{"ANA_STATUS", SGTL5000_CHIP_ANA_STATUS},
	{"SHORT_CTRL", SGTL5000_CHIP_SHORT_CTRL},
	{"ANA_TEST2", SGTL5000_CHIP_ANA_TEST2},
	{"DAP_CTRL", SGTL5000_DAP_CTRL},
	{"DAP_PEQ", SGTL5000_DAP_PEQ},
	{"DAP_BASS_ENHANCE", SGTL5000_DAP_BASS_ENHANCE},
	{"DAP_BASS_ENHANCE_CTRL", SGTL5000_DAP_BASS_ENHANCE_CTRL},
	{"DAP_AUDIO_EQ", SGTL5000_DAP_AUDIO_EQ},
	{"DAP_SURROUND", SGTL5000_DAP_SURROUND},
	{"DAP_FLT_COEF_ACCESS", SGTL5000_DAP_FLT_COEF_ACCESS},
	{"DAP_COEF_WR_B0_MSB", SGTL5000_DAP_COEF_WR_B0_MSB},
	{"DAP_COEF_WR_B0_LSB", SGTL5000_DAP_COEF_WR_B0_LSB},
	{"DAP_EQ_BASS_BAND0", SGTL5000_DAP_EQ_BASS_BAND0},
	{"DAP_EQ_BASS_BAND1", SGTL5000_DAP_EQ_BASS_BAND1},
	{"DAP_EQ_BASS_BAND2", SGTL5000_DAP_EQ_BASS_BAND2},
	{"DPA_EQ_BASS_BAND3", SGTL5000_DAP_EQ_BASS_BAND3},
	{"DAP_EQ_BASS_BAND4", SGTL5000_DAP_EQ_BASS_BAND4},
	{"DAP_MAIN_CHAIN", SGTL5000_DAP_MAIN_CHAN},
	{"DAP_MIX_CHAN", SGTL5000_DAP_MIX_CHAN},
	{"DAP_AVC_CTRL", SGTL5000_DAP_AVC_CTRL},
	{"DAP_AVC_THRESHOLD", SGTL5000_DAP_AVC_THRESHOLD},
	{"DAP_AVC_ATTACK", SGTL5000_DAP_AVC_ATTACK},
	{"DAP_AVC_DECAY", SGTL5000_DAP_AVC_DECAY},
	{"DAP_COEF_WR_B1_MSB", SGTL5000_DAP_COEF_WR_B1_MSB},
	{"DAP_COEF_WR_B1_LSB", SGTL5000_DAP_COEF_WR_B1_LSB},
	{"DAP_COEF_WR_B2_MSB", SGTL5000_DAP_COEF_WR_B2_MSB},
	{"DAP_COEF_WR_B2_LSB", SGTL5000_DAP_COEF_WR_B2_LSB},
	{"DAP_COEF_WR_A1_MSB", SGTL5000_DAP_COEF_WR_A1_MSB},
	{"DAP_COEF_WR_A1_LSB", SGTL5000_DAP_COEF_WR_A1_LSB},
	{"DAP_COEF_WR_A2_MSB", SGTL5000_DAP_COEF_WR_A2_MSB},
	{"DAP_COEF_WR_A2_LSB", SGTL5000_DAP_COEF_WR_A2_LSB},
};

static unsigned int sgtl5000_hw_read(unsigned int reg)
{
	struct i2c_adapter *adp;
	u8 buf0[2], buf1[2];
	u16 value;
	int ret;
	struct i2c_msg msg[2] = {
		{0x0a, 0, 2, buf0},
		{0x0a, I2C_M_RD, 2, buf1},
	};

	adp = i2c_get_adapter(1);

	buf0[0] = (reg & 0xff00) >> 8;
	buf0[1] = reg & 0xff;
	ret = i2c_transfer(adp, msg, 2);
	if (ret < 0) {
		return -ENODEV;
	}

	value = buf1[0] << 8 | buf1[1];
	return value;

}

static unsigned int sgtl5000_hw_write(unsigned int reg, unsigned int value)
{
	struct i2c_adapter *adp;
	u8 buf[4];
	int i2c_ret;
	struct i2c_msg msg = { 0x0a, 0, 4, buf };

	buf[0] = (reg & 0xff00) >> 8;
	buf[1] = reg & 0xff;
	buf[2] = (value & 0xff00) >> 8;
	buf[3] = value & 0xff;

	adp = i2c_get_adapter(1);
	i2c_ret = i2c_transfer(adp, &msg, 1);
	if (i2c_ret < 0) {
		return -EIO;
	}

	return i2c_ret;
}

static ssize_t sgtl5000_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 value;
	int i;

	for (i = 0; i < ARRAY_SIZE(sgtl_tbl); i++) {
		value = sgtl5000_hw_read(sgtl_tbl[i].reg_addr);
		if(value < 0)
			return -ENODEV;
		printk("%s:[0x%04x]=0x%04x\n", sgtl_tbl[i].name, sgtl_tbl[i].reg_addr, value);
	}

	return 0;
}

static ssize_t sgtl5000_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char *end;
	char *token;
	char *running;
	const char delimiters[] = "=";
	u32 addr, value;

	running = (char *)buf;

	token = strsep(&running, delimiters);
	addr = simple_strtoul(token, &end, 16);
	if(addr > SGTL5000_DAP_COEF_WR_A2_LSB) {
		return -EINVAL;
	}

	token = strsep(&running, delimiters);
	if(token != NULL) {
		value = simple_strtoul(token, &end, 16);
	} else {
		value = sgtl5000_hw_read(addr);
		printk("[%d] = 0x%04x\n", addr, value);
		return -EINVAL;
	}

	sgtl5000_hw_write(addr, value);
	value = sgtl5000_hw_read(addr);

	printk("[%d] = 0x%04x\n", addr, value);

	return count;

}

static struct kobj_attribute reg_read_attribute =
	__ATTR(reg_dump, 0666, reg_dump_show, reg_dump_store);
static struct kobj_attribute reg_write_attribute =
	__ATTR(reg_write, 0666, reg_write_show, reg_write_store);
static struct kobj_attribute pmic_attribute =
	__ATTR(pmic, 0666, pmic_show, pmic_store);
static struct kobj_attribute edid_attribute =
	__ATTR(edid, 0666, edid_show, NULL);
static struct kobj_attribute battery_attribute =
	__ATTR(battery, 0666, battery_show, NULL);
static struct kobj_attribute mtl017_attribute =
	__ATTR(mtl017, 0666, mtl017_show, mtl017_store);
static struct kobj_attribute sgtl5000_attribute =
	__ATTR(sgtl5000, 0666, sgtl5000_show, sgtl5000_store);

static struct attribute *debug_attrs[] = {
	&reg_read_attribute.attr,
	&reg_write_attribute.attr,
	&pmic_attribute.attr,
	&edid_attribute.attr,
	&battery_attribute.attr,
	&mtl017_attribute.attr,
	&sgtl5000_attribute.attr,
	NULL,
};

static struct attribute_group debug_attr_group = {
	.attrs = debug_attrs,
};


static struct platform_device mxc_debug_device = {
	.name = "mxc_debug",
};

static struct kobject *debug_kobj;

static int __init mx51_debug_init(void)
{
	int retval;

	platform_device_register(&mxc_debug_device);

	debug_kobj = kobject_create_and_add("debug", &mxc_debug_device.dev.kobj);
	if(!debug_kobj)
		return -ENOMEM;


	retval = sysfs_create_group(debug_kobj, &debug_attr_group);
	if(retval) {
		kobject_put(debug_kobj);
		return retval;
	}

	return 0;

}

static void __exit mx51_debug_exit(void)
{

	sysfs_remove_group(debug_kobj, &debug_attr_group);
	kobject_put(debug_kobj);

}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ron Lee <ron1_lee@pegatroncorp.com>, Pegatron Corp.");
module_init(mx51_debug_init)
module_exit(mx51_debug_exit)
