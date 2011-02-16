/*
 * Copyright 2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __ASM_ARCH_MXC_BOARD_MX51_EFIKASB_H__
#define __ASM_ARCH_MXC_BOARD_MX51_EFIKASB_H__

/*!
 * @defgroup BRDCFG_MX51 Board Configuration Options
 * @ingroup MSL_MX51
 */

/*!
 * @file mach-mx51/board-mx51_efikasb.h
 *
 * @brief This file contains all the board level configuration options.
 *
 * It currently hold the options defined for Genesi MX51 Efikasb Platform.
 *
 * @ingroup BRDCFG_MX51
 */

/*
 * Include Files
 */
#include <mach/mxc_uart.h>

/*!
 * @name MXC UART board level configurations
 */
/*! @{ */
/*!
 * Specifies if the Irda transmit path is inverting
 */
#define MXC_IRDA_TX_INV	0
/*!
 * Specifies if the Irda receive path is inverting
 */
#define MXC_IRDA_RX_INV	0

/* UART 1 configuration */
/*!
 * This define specifies if the UART port is configured to be in DTE or
 * DCE mode. There exists a define like this for each UART port. Valid
 * values that can be used are \b MODE_DTE or \b MODE_DCE.
 */
#define UART1_MODE		MODE_DCE
/*!
 * This define specifies if the UART is to be used for IRDA. There exists a
 * define like this for each UART port. Valid values that can be used are
 * \b IRDA or \b NO_IRDA.
 */
#define UART1_IR		NO_IRDA
/*!
 * This define is used to enable or disable a particular UART port. If
 * disabled, the UART will not be registered in the file system and the user
 * will not be able to access it. There exists a define like this for each UART
 * port. Specify a value of 1 to enable the UART and 0 to disable it.
 */
#define UART1_ENABLED		1
/*! @} */
/* UART 2 configuration */
#define UART2_MODE		MODE_DCE
#define UART2_IR		NO_IRDA
#define UART2_ENABLED		1
/* UART 3 configuration */
#define UART3_MODE		MODE_DTE
#define UART3_IR		NO_IRDA
#define UART3_ENABLED		1
/* UART 4 configuration */
#define UART4_MODE		MODE_DCE
#define UART4_IR		NO_IRDA
#define UART4_ENABLED		0
/* UART 5 configuration */
#define UART5_MODE		MODE_DCE
#define UART5_IR		NO_IRDA
#define UART5_ENABLED		0

#define MXC_LL_UART_PADDR	UART1_BASE_ADDR
#define MXC_LL_UART_VADDR	AIPS1_IO_ADDRESS(UART1_BASE_ADDR)

/* ron: Efikasb GPIO Pin Definition */
#define SDHC1_CD_PIN           MX51_PIN_EIM_CS2  /* MX51_PIN_GPIO1_0 */
#define SDHC1_WP_PIN           MX51_PIN_GPIO1_1
#define SDHC2_CD_PIN           MX51_PIN_GPIO1_8
#define SDHC2_WP_PIN           MX51_PIN_GPIO1_7

#define HUB_RESET_PIN          MX51_PIN_GPIO1_5
#define PMIC_INT_PIN           MX51_PIN_GPIO1_6
#define USB_PHY_RESET_PIN      MX51_PIN_EIM_D27

#define WIRELESS_SW_PIN        MX51_PIN_DI1_PIN12
#define WLAN_PWRON_PIN         MX51_PIN_EIM_A22
#define WLAN_RESET_PIN         MX51_PIN_EIM_A16
#define BT_PWRON_PIN           MX51_PIN_EIM_A17
#define WWAN_PWRON_PIN         MX51_PIN_CSI2_D13

#define LCD_LVDS_EN_PIN        MX51_PIN_CSI1_D8
#define LCD_PWRON_PIN          MX51_PIN_CSI1_D9
#define LCD_BL_PWM_PIN         MX51_PIN_GPIO1_2
#define LCD_BL_PWRON_PIN       MX51_PIN_CSI2_D19
#define LVDS_RESET_PIN         MX51_PIN_DISPB2_SER_DIN
#define LVDS_PWRCTL_PIN        MX51_PIN_DISPB2_SER_CLK

#define CAM_PWRON_PIN          MX51_PIN_NANDF_CS0

#define BATT_LOW_PIN           MX51_PIN_DI1_PIN11
#define BATT_INS_PIN           MX51_PIN_DISPB2_SER_DIO
#define AC_ADAP_INS_PIN        MX51_PIN_DI1_D0_CS /* MX51_PIN_CSI1_D8 */

#define LID_SW_PIN             MX51_PIN_CSI1_VSYNC /* Low: close, High: open */

#define POWER_BTN_PIN          MX51_PIN_EIM_DTACK
#define SYS_PWROFF_PIN         MX51_PIN_CSI2_VSYNC
#define SYS_PWRGD_PIN          MX51_PIN_CSI2_PIXCLK

/* ron: Efikasb LED Pin Definition */
#define CAPS_LED_PIN           MX51_PIN_EIM_CS0
#define ALARM_LED_PIN          MX51_PIN_GPIO1_3

/* ron: R1.2 borad GPIO definition */
#define MEM_ID0_PIN            MX51_PIN_EIM_LBA /* MX51_PIN_GPIO1_4 */
#define MEM_ID1_PIN            MX51_PIN_EIM_CRE

#define SIM_CD_PIN             MX51_PIN_EIM_CS1
#define WWAN_WAKEUP_PIN        MX51_PIN_CSI1_HSYNC
#define AGPS_PWRON_PIN         MX51_PIN_CSI2_D12
#define AGPS_RESET_PIN         MX51_PIN_CSI2_D18
#define AGPS_PWRSW_PIN         MX51_PIN_NANDF_CS1 /* ron: R1.3 add AGPS_PWRSW */

/* ron: R1.3 board GPIO definition */
#define PCB_ID0_PIN            MX51_PIN_EIM_CS3
#define PCB_ID1_PIN            MX51_PIN_EIM_CS4

#define WDOG_PIN               MX51_PIN_GPIO1_4

/* here today, gone tomorrow */
extern void __init mx51_efikamx_init_audio(void);
extern void __init mx51_efikamx_init_pata(void);

extern int __init mx51_efikasb_init_mc13892(void);

#endif				/* __ASM_ARCH_MXC_BOARD_MX51_LANGE51_H__ */
