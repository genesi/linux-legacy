/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include "mx51_efikasb.h"
#include "iomux.h"
#include "mx51_pins.h"

/*!
 * @file mach-mx51/mx51_efikasb_gpio.c
 *
 * @brief This file contains all the GPIO setup functions for the board.
 *
 * @ingroup GPIO
 */

void mxc_power_on_wlan(int);
void mxc_power_on_wwan(int);
void mxc_power_on_bt(int);
void mxc_power_on_camera(int);
void mxc_reset_wlan(void);
void mxc_reset_usb_hub(void);
void mxc_reset_usb_phy(void);


static struct mxc_iomux_pin_cfg __initdata mxc_iomux_pins[] = {
	/* USBH2_DATA0 */
	{
	 MX51_PIN_EIM_D16, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	  PAD_CTL_100K_PU | PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE |
	  PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA1 */
	{
	 MX51_PIN_EIM_D17, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA2 */
	{
	 MX51_PIN_EIM_D18, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA3 */
	{
	 MX51_PIN_EIM_D19, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA4 */
	{
	 MX51_PIN_EIM_D20, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA5 */
	{
	 MX51_PIN_EIM_D21, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA6 */
	{
	 MX51_PIN_EIM_D22, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DATA7 */
	{
	 MX51_PIN_EIM_D23, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{                       /* ron: GPIO2_10 WLAN Reset*/
	 MX51_PIN_EIM_A16, IOMUX_CONFIG_GPIO,
        },
	{                       /* ron: GPIO2_11 BT Power On */
	 MX51_PIN_EIM_A17, IOMUX_CONFIG_GPIO,
        },
	{
	 MX51_PIN_EIM_A18, IOMUX_CONFIG_GPIO,
        },
	{
	 MX51_PIN_EIM_A19, IOMUX_CONFIG_GPIO,
        },
	{
	 MX51_PIN_EIM_A20, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE),
        },
	{
	 MX51_PIN_EIM_A21, IOMUX_CONFIG_GPIO,
        },
	{                       /* ron: GPIO2_16 WLAN Power On */
	 MX51_PIN_EIM_A22, IOMUX_CONFIG_GPIO,
        },
	/* USBH2_CLK */
	{
	 MX51_PIN_EIM_A24, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_DIR */
	{
	 MX51_PIN_EIM_A25, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_STP */
	{
	 MX51_PIN_EIM_A26, IOMUX_CONFIG_ALT2,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	/* USBH2_NXT */
	{
	  MX51_PIN_EIM_A27, IOMUX_CONFIG_ALT2,
	  (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	   PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
	},
        {			/* ron: GPIO2_24 CPU 22.5792M OSC enable */
	 MX51_PIN_EIM_OE, IOMUX_CONFIG_ALT1,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE),
 	},
	{			/*ron: GPIO2_31 Power Button*/
	 MX51_PIN_EIM_DTACK, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU),
        },
	{
	 MX51_PIN_DI_GP4, IOMUX_CONFIG_ALT4,
        },
	{                       /* ron: I2C1 */
	 MX51_PIN_I2C1_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 0x1E4,
        },
	{
	 MX51_PIN_I2C1_DAT, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 0x1E4,
        },
	{                       /* USBH1_STP */
	 MX51_PIN_USBH1_STP, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_CLK */
	 MX51_PIN_USBH1_CLK, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
        },
	{			/* USBH1_DIR */
	 MX51_PIN_USBH1_DIR, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
        },
	{			/* USBH1_NXT */
	 MX51_PIN_USBH1_NXT, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE | PAD_CTL_DDR_INPUT_CMOS),
        },
	{			/* USBH1_DATA0 */
	 MX51_PIN_USBH1_DATA0, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA1 */
	 MX51_PIN_USBH1_DATA1, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA2 */
	 MX51_PIN_USBH1_DATA2, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA3 */
	 MX51_PIN_USBH1_DATA3, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA4 */
	 MX51_PIN_USBH1_DATA4, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA5 */
	 MX51_PIN_USBH1_DATA5, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA6 */
	 MX51_PIN_USBH1_DATA6, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{			/* USBH1_DATA7 */
	 MX51_PIN_USBH1_DATA7, IOMUX_CONFIG_ALT0,
	 (PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
	  PAD_CTL_PUE_KEEPER | PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_ENABLE),
        },
	{                       /* ron: GPIO1_6 PMIC Interrupt */
	 MX51_PIN_GPIO1_6, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION,
	  (PAD_CTL_SRE_SLOW | PAD_CTL_DRV_MEDIUM | PAD_CTL_100K_PU |
	  PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_VOT_HIGH),
        },
	{
	 MX51_PIN_EIM_D25, IOMUX_CONFIG_ALT4,
	 (PAD_CTL_HYS_NONE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	  PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST),
        },
        {                       /* ron: GPIO3_0 Battery Low */
         MX51_PIN_DI1_PIN11, IOMUX_CONFIG_ALT4,
        },
        {                       /* ron: GPIO3_1 Wireless SW */
         MX51_PIN_DI1_PIN12, IOMUX_CONFIG_ALT4,
        },
        {                       /* ron: GPIO3_2 Watchdog ? GPIO conflict with EIM_CRE*/
         MX51_PIN_DI1_PIN13, IOMUX_CONFIG_ALT4,
        },
        {                       /* ron: GPIO4_10 WWAN Power On */
         MX51_PIN_CSI2_D13, IOMUX_CONFIG_ALT3,
        },
        {                       /* ron: GPIO4_15 Power Good */
         MX51_PIN_CSI2_PIXCLK, IOMUX_CONFIG_ALT3,
        },
};

static void power_on_bt_handler(struct work_struct *work)
{
        mxc_power_on_bt(1);
}

static DECLARE_DELAYED_WORK(power_on_bt_work, power_on_bt_handler);

void __init mx51_efikasb_io_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mxc_iomux_pins); i++) {
		mxc_request_iomux(mxc_iomux_pins[i].pin,
				  mxc_iomux_pins[i].mux_mode);
		if (mxc_iomux_pins[i].pad_cfg)
			mxc_iomux_set_pad(mxc_iomux_pins[i].pin,
					  mxc_iomux_pins[i].pad_cfg);
		if (mxc_iomux_pins[i].in_select)
			mxc_iomux_set_input(mxc_iomux_pins[i].in_select,
					    mxc_iomux_pins[i].in_mode);
	}

	/* ron: USB Hub Reset*/
/* 	mxc_request_iomux(HUB_RESET_PIN, IOMUX_CONFIG_ALT0); */
/* 	mxc_iomux_set_pad(HUB_RESET_PIN, PAD_CTL_DRV_HIGH | */
/* 			  PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST); */
/* 	gpio_request(IOMUX_TO_GPIO(HUB_RESET_PIN), "hub_reset"); */
/* 	gpio_direction_output(IOMUX_TO_GPIO(HUB_RESET_PIN), 0); */
/* 	mxc_reset_usb_hub(); */

	/* ron: USB Phy Reset */
	mxc_request_iomux(USB_PHY_RESET_PIN, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(USB_PHY_RESET_PIN, PAD_CTL_DRV_HIGH |
			  PAD_CTL_HYS_NONE | PAD_CTL_PUE_KEEPER |
			  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE |
			  PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST);
	gpio_request(IOMUX_TO_GPIO(USB_PHY_RESET_PIN), "usb_phy_reset");
        mxc_reset_usb_phy();

	/* ron: PMIC interrupt*/
	gpio_request(IOMUX_TO_GPIO(PMIC_INT_PIN), "pmic_int");
	gpio_direction_input(IOMUX_TO_GPIO(PMIC_INT_PIN));

	/* i2c2 SDA */
	mxc_request_iomux(MX51_PIN_KEY_COL5,
			  IOMUX_CONFIG_ALT3 | IOMUX_CONFIG_SION);
	mxc_iomux_set_input(MUX_IN_I2C2_IPP_SDA_IN_SELECT_INPUT,
			    INPUT_CTL_PATH1);
	mxc_iomux_set_pad(MX51_PIN_KEY_COL5,
			  PAD_CTL_SRE_FAST |
			  PAD_CTL_ODE_OPENDRAIN_ENABLE |
			  PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
			  PAD_CTL_HYS_ENABLE);

	/* i2c2 SCL */
	mxc_request_iomux(MX51_PIN_KEY_COL4,
			  IOMUX_CONFIG_ALT3 | IOMUX_CONFIG_SION);
	mxc_iomux_set_input(MUX_IN_I2C2_IPP_SCL_IN_SELECT_INPUT,
			    INPUT_CTL_PATH1);
	mxc_iomux_set_pad(MX51_PIN_KEY_COL4,
			  PAD_CTL_SRE_FAST |
			  PAD_CTL_ODE_OPENDRAIN_ENABLE |
			  PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU |
			  PAD_CTL_HYS_ENABLE);

	mxc_request_iomux(MX51_PIN_CSPI1_RDY, IOMUX_CONFIG_ALT3);
	mxc_iomux_set_pad(MX51_PIN_CSPI1_RDY, PAD_CTL_DRV_HIGH |
			  PAD_CTL_HYS_NONE | PAD_CTL_PUE_KEEPER |
			  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE |
			  PAD_CTL_PKE_ENABLE | PAD_CTL_SRE_FAST);
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_CSPI1_RDY), "cspi1_rdy");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_RDY), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_RDY), 0);

        /* Lid Switch */
	mxc_request_iomux(LID_SW_PIN, IOMUX_CONFIG_GPIO /* | IOMUX_CONFIG_SION */);
	gpio_request(IOMUX_TO_GPIO(LID_SW_PIN), "lid_sw");
	gpio_direction_input(IOMUX_TO_GPIO(LID_SW_PIN));

        /* Wireless*/
	mxc_power_on_wlan(1);
	mxc_reset_wlan();
/* 	mxc_power_on_bt(1); */
        /* ron: workaround the bluetooth detect fail */
        schedule_delayed_work(&power_on_bt_work, msecs_to_jiffies(500));
	mxc_power_on_wwan(1);

        /* Camera */
	mxc_request_iomux(CAM_PWRON_PIN, IOMUX_CONFIG_GPIO);
	mxc_power_on_camera(1);

	/* LVDS & LCD Panel */
	mxc_iomux_set_pad(MX51_PIN_DI2_DISP_CLK, /* ron: set driver strength to low to avoid EMI */
			  PAD_CTL_PKE_ENABLE |
			  PAD_CTL_DRV_LOW |
			  PAD_CTL_SRE_FAST);

	mxc_request_iomux(LVDS_RESET_PIN, IOMUX_CONFIG_GPIO);
        mxc_request_iomux(LVDS_PWRCTL_PIN, IOMUX_CONFIG_GPIO);
	mxc_request_iomux(LCD_PWRON_PIN, IOMUX_CONFIG_GPIO); /* ron: LCD Power On */
	mxc_request_iomux(LCD_BL_PWRON_PIN, IOMUX_CONFIG_GPIO); /* ron: LCD Backlight Power On */
	gpio_request(IOMUX_TO_GPIO(LVDS_RESET_PIN), "lvds_reset");
        gpio_request(IOMUX_TO_GPIO(LVDS_PWRCTL_PIN), "lvds_pwrctl");
	gpio_request(IOMUX_TO_GPIO(LCD_PWRON_PIN), "lcd_pwron");
	gpio_request(IOMUX_TO_GPIO(LCD_BL_PWRON_PIN), "lcd_bl_pwron");
        gpio_direction_output(IOMUX_TO_GPIO(LVDS_RESET_PIN), 0);
        gpio_direction_output(IOMUX_TO_GPIO(LVDS_PWRCTL_PIN), 0);
        gpio_direction_output(IOMUX_TO_GPIO(LCD_PWRON_PIN), 0);
        gpio_direction_output(IOMUX_TO_GPIO(LCD_BL_PWRON_PIN), 1); /* low active */

        mxc_request_iomux(LCD_BL_PWM_PIN, IOMUX_CONFIG_GPIO);
        gpio_request(IOMUX_TO_GPIO(LCD_BL_PWM_PIN), "lcd_bl_pwm");
        gpio_direction_output(IOMUX_TO_GPIO(LCD_BL_PWM_PIN), 0);

        mxc_request_iomux(LCD_LVDS_EN_PIN, IOMUX_CONFIG_GPIO);
        gpio_request(IOMUX_TO_GPIO(LCD_LVDS_EN_PIN), "lcd_en");
        gpio_direction_output(IOMUX_TO_GPIO(LCD_LVDS_EN_PIN), 0);

	/* Battery & AC Adapter Insertion */
	mxc_request_iomux(BATT_INS_PIN, IOMUX_CONFIG_GPIO);
	gpio_request(IOMUX_TO_GPIO(BATT_INS_PIN), "batt_ins");
	gpio_direction_input(IOMUX_TO_GPIO(BATT_INS_PIN));
        mxc_request_iomux(BATT_LOW_PIN, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION);
        gpio_request(IOMUX_TO_GPIO(BATT_LOW_PIN), "batt_low");
        gpio_direction_input(IOMUX_TO_GPIO(BATT_LOW_PIN));

	mxc_request_iomux(AC_ADAP_INS_PIN, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION);
        /* ron: IOMUXC_GPIO3_IPP_IND_G_IN_3_SELECT_INPUT: 1: Selecting Pad DI1_D0_CS for Mode:ALT4 */
        __raw_writel(0x01, IO_ADDRESS(IOMUXC_BASE_ADDR) + 0x980);
	gpio_request(IOMUX_TO_GPIO(AC_ADAP_INS_PIN), "ac_adap_ins");
	gpio_direction_input(IOMUX_TO_GPIO(AC_ADAP_INS_PIN));

	/* ron: for R12 borad gpio */
	/* Memory ID pin */
	mxc_request_iomux(MEM_ID0_PIN, IOMUX_CONFIG_GPIO);
	gpio_request(IOMUX_TO_GPIO(MEM_ID0_PIN), "mem_id");
	gpio_direction_input(IOMUX_TO_GPIO(MEM_ID0_PIN));
	mxc_request_iomux(MEM_ID1_PIN, IOMUX_CONFIG_GPIO);
	gpio_request(IOMUX_TO_GPIO(MEM_ID1_PIN), "mem_id");
	gpio_direction_input(IOMUX_TO_GPIO(MEM_ID1_PIN));

	/* SIM CD pin */
	mxc_request_iomux(SIM_CD_PIN, IOMUX_CONFIG_GPIO | IOMUX_CONFIG_SION);
        mxc_iomux_set_pad(SIM_CD_PIN, PAD_CTL_PKE_NONE);
	gpio_request(IOMUX_TO_GPIO(SIM_CD_PIN), "sim_cd");
	gpio_direction_input(IOMUX_TO_GPIO(SIM_CD_PIN));

	/* WWAN Wakeup event pin */
	mxc_request_iomux(WWAN_WAKEUP_PIN, IOMUX_CONFIG_GPIO);
	gpio_request(IOMUX_TO_GPIO(WWAN_WAKEUP_PIN), "wwan_wakeup");
	gpio_direction_input(IOMUX_TO_GPIO(WWAN_WAKEUP_PIN));

        /* Watchdog */
        mxc_request_iomux(WDOG_PIN, IOMUX_CONFIG_ALT2);

        /* PCB ID */
        mxc_request_iomux(PCB_ID0_PIN, IOMUX_CONFIG_GPIO);
        mxc_request_iomux(PCB_ID1_PIN, IOMUX_CONFIG_GPIO);
        gpio_request(IOMUX_TO_GPIO(PCB_ID0_PIN), "pcb_id");
        gpio_request(IOMUX_TO_GPIO(PCB_ID1_PIN), "pcb_id");
        gpio_direction_input(IOMUX_TO_GPIO(PCB_ID0_PIN));
        gpio_direction_input(IOMUX_TO_GPIO(PCB_ID1_PIN));

}

struct mxc_power_switch_status {
	int wlan_pwr_status;
	int wwan_pwr_status;
	int bt_pwr_status;
	int camera_pwr_status;
};

static struct mxc_power_switch_status pwr_sw_status = {
	.wlan_pwr_status = 0,
	.wwan_pwr_status = 0,
	.bt_pwr_status = 0,
	.camera_pwr_status = 0,
};

int mxc_get_power_status(iomux_pin_name_t pin)
{
	switch(pin) {
	case WLAN_PWRON_PIN:
		return pwr_sw_status.wlan_pwr_status;
	case WWAN_PWRON_PIN:
		return pwr_sw_status.wwan_pwr_status;
	case BT_PWRON_PIN:
		return pwr_sw_status.bt_pwr_status;
	case CAM_PWRON_PIN:
		return pwr_sw_status.camera_pwr_status;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(mxc_get_power_status);

void mxc_power_on_wlan(int on)
{
	gpio_direction_output(IOMUX_TO_GPIO(WLAN_PWRON_PIN), 0);
	if (on)
		gpio_set_value(IOMUX_TO_GPIO(WLAN_PWRON_PIN), 1);
	else
		gpio_set_value(IOMUX_TO_GPIO(WLAN_PWRON_PIN), 0);

	pwr_sw_status.wlan_pwr_status = on;

}
EXPORT_SYMBOL(mxc_power_on_wlan);

void mxc_power_on_wwan(int on)
{
	gpio_direction_output(IOMUX_TO_GPIO(WWAN_PWRON_PIN), 0);
	if (on)
		gpio_set_value(IOMUX_TO_GPIO(WWAN_PWRON_PIN), 0); /* low active */
	else
		gpio_set_value(IOMUX_TO_GPIO(WWAN_PWRON_PIN), 1);

	pwr_sw_status.wwan_pwr_status = on;
}
EXPORT_SYMBOL(mxc_power_on_wwan);

void mxc_power_on_bt(int on)
{
	gpio_direction_output(IOMUX_TO_GPIO(BT_PWRON_PIN), 0);
	if (on)
		gpio_set_value(IOMUX_TO_GPIO(BT_PWRON_PIN), 1);
	else
		gpio_set_value(IOMUX_TO_GPIO(BT_PWRON_PIN), 0);

	pwr_sw_status.bt_pwr_status = on;

}
EXPORT_SYMBOL(mxc_power_on_bt);

void mxc_power_on_camera(int on)
{
	gpio_direction_output(IOMUX_TO_GPIO(CAM_PWRON_PIN), 0);
	if(on)
		gpio_set_value(IOMUX_TO_GPIO(CAM_PWRON_PIN), 0); /* low active */
	else
		gpio_set_value(IOMUX_TO_GPIO(CAM_PWRON_PIN), 1);

	pwr_sw_status.camera_pwr_status = on;
}
EXPORT_SYMBOL(mxc_power_on_camera);

void mxc_reset_wlan(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(WLAN_RESET_PIN), 1);
        msleep(1);
        gpio_set_value(IOMUX_TO_GPIO(WLAN_RESET_PIN), 0);
        msleep(1);
        gpio_set_value(IOMUX_TO_GPIO(WLAN_RESET_PIN), 1);
}
EXPORT_SYMBOL(mxc_reset_wlan);

void mxc_reset_usb_hub(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(HUB_RESET_PIN), 1);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(HUB_RESET_PIN), 0);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(HUB_RESET_PIN), 1);
}
EXPORT_SYMBOL(mxc_reset_usb_hub);


void mxc_reset_usb_phy(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(USB_PHY_RESET_PIN), 0);
	msleep(1);
	gpio_set_value(IOMUX_TO_GPIO(USB_PHY_RESET_PIN), 1);

}
EXPORT_SYMBOL(mxc_reset_usb_phy);

int mxc_get_battery_insertion_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(BATT_INS_PIN));
}
EXPORT_SYMBOL(mxc_get_battery_insertion_status);

int mxc_get_batt_low_status(void)
{
        return !gpio_get_value(IOMUX_TO_GPIO(BATT_LOW_PIN));
}
EXPORT_SYMBOL(mxc_get_batt_low_status);

int mxc_get_ac_adapter_insertion_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(AC_ADAP_INS_PIN));
}
EXPORT_SYMBOL(mxc_get_ac_adapter_insertion_status);

int mxc_get_sim_card_status(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(SIM_CD_PIN));
}
EXPORT_SYMBOL(mxc_get_sim_card_status);


/* ron: 00    Nanya DDR2
        10    Micron DDR2
        01    Hynix DDR2  */
int mxc_get_memory_id(void)
{
        int id;

	id = gpio_get_value(IOMUX_TO_GPIO(MEM_ID0_PIN));
        id |= gpio_get_value(IOMUX_TO_GPIO(MEM_ID1_PIN)) << 1;

        return id;
}
EXPORT_SYMBOL(mxc_get_memory_id);

int mxc_get_wireless_sw_status(void)
{
        return !gpio_get_value(IOMUX_TO_GPIO(WIRELESS_SW_PIN));
}
EXPORT_SYMBOL(mxc_get_wireless_sw_status);

int mxc_get_lid_sw_status(void)
{
        /* ron: 0:open 1:close */
        return !gpio_get_value(IOMUX_TO_GPIO(LID_SW_PIN));
}
EXPORT_SYMBOL(mxc_get_lid_sw_status);

/* ron: 01     R1.3 board
        10     R2.0 board */
int mxc_get_pcb_id(void)
{
        int id;

        id = gpio_get_value(IOMUX_TO_GPIO(PCB_ID0_PIN));
        id |= gpio_get_value(IOMUX_TO_GPIO(PCB_ID1_PIN)) << 1;

        return id;
}
EXPORT_SYMBOL(mxc_get_pcb_id);

/* workaround for ecspi chipselect pin may not keep correct level when idle */
void mx51_efikasb_gpio_spi_chipselect_active(int cspi_mode, int status,
					    int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			mxc_request_iomux(MX51_PIN_CSPI1_SS0,
					  IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX51_PIN_CSPI1_SS0,
					  PAD_CTL_HYS_ENABLE |
					  PAD_CTL_PKE_ENABLE |
					  PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST);
			mxc_request_iomux(MX51_PIN_CSPI1_SS1,
					  IOMUX_CONFIG_GPIO);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), 0);
/* 			gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), 2 & (~status)); */
                        gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS1), !(status & 0x02));
			break;
		case 0x2:
			mxc_request_iomux(MX51_PIN_CSPI1_SS1,
					  IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX51_PIN_CSPI1_SS1,
					  PAD_CTL_HYS_ENABLE |
					  PAD_CTL_PKE_ENABLE |
					  PAD_CTL_DRV_HIGH | PAD_CTL_SRE_FAST);
			mxc_request_iomux(MX51_PIN_CSPI1_SS0,
					  IOMUX_CONFIG_GPIO);
			gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), 0);
/* 			gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), 1 & (~status)); */
                        gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSPI1_SS0), !(status & 0x01));
			break;
		default:
			break;
		}
		break;
	case 2:
		break;
	case 3:
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(mx51_efikasb_gpio_spi_chipselect_active);

void mx51_efikasb_gpio_spi_chipselect_inactive(int cspi_mode, int status,
					      int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_ALT0);
			mxc_free_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_GPIO);
			mxc_request_iomux(MX51_PIN_CSPI1_SS0,
					  IOMUX_CONFIG_GPIO);
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);

			break;
		case 0x2:
			mxc_free_iomux(MX51_PIN_CSPI1_SS0, IOMUX_CONFIG_GPIO);
			mxc_free_iomux(MX51_PIN_CSPI1_SS1, IOMUX_CONFIG_ALT0);
			break;
		default:
			break;
		}
		break;
	case 2:
		break;
	case 3:
		break;
	default:
		break;
	}

}
EXPORT_SYMBOL(mx51_efikasb_gpio_spi_chipselect_inactive);

