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

#ifndef MX51_EFIKASB_H
#define MX51_EFIKASB_H

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

#define MXC_IRDA_TX_INV	0
#define MXC_IRDA_RX_INV	0
#define UART1_MODE		MODE_DCE
#define UART1_IR		NO_IRDA
#define UART1_ENABLED		1

#define MXC_LL_UART_PADDR	UART1_BASE_ADDR
#define MXC_LL_UART_VADDR	AIPS1_IO_ADDRESS(UART1_BASE_ADDR)

#define HUB_RESET_PIN          MX51_PIN_GPIO1_5
#define PMIC_INT_PIN           MX51_PIN_GPIO1_6
#define USB_PHY_RESET_PIN      MX51_PIN_EIM_D27

#define WIRELESS_SW_PIN        MX51_PIN_DI1_PIN12
#define WLAN_PWRON_PIN         MX51_PIN_EIM_A22
#define WLAN_RESET_PIN         MX51_PIN_EIM_A16
#define BT_PWRON_PIN           MX51_PIN_EIM_A17

#define CAM_PWRON_PIN          MX51_PIN_NANDF_CS0

#define LID_SW_PIN             MX51_PIN_CSI1_VSYNC /* Low: close, High: open */

#define POWER_BTN_PIN          MX51_PIN_EIM_DTACK
#define SYS_PWROFF_PIN         MX51_PIN_CSI2_VSYNC
#define SYS_PWRGD_PIN          MX51_PIN_CSI2_PIXCLK

/* ron: R1.2 borad GPIO definition */
#define MEM_ID0_PIN            MX51_PIN_EIM_LBA /* MX51_PIN_GPIO1_4 */
#define MEM_ID1_PIN            MX51_PIN_EIM_CRE

#define SIM_CD_PIN             MX51_PIN_EIM_CS1
#define AGPS_PWRON_PIN         MX51_PIN_CSI2_D12
#define AGPS_RESET_PIN         MX51_PIN_CSI2_D18
#define AGPS_PWRSW_PIN         MX51_PIN_NANDF_CS1 /* ron: R1.3 add AGPS_PWRSW */

/* ron: R1.3 board GPIO definition */
#define PCB_ID0_PIN            MX51_PIN_EIM_CS3
#define PCB_ID1_PIN            MX51_PIN_EIM_CS4

#define WDOG_PIN               MX51_PIN_GPIO1_4

/* here today, gone tomorrow */
/* cpu */
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
extern struct cpu_wp *mx51_efikamx_get_cpu_wp(int *wp);
extern void mx51_efikamx_set_num_cpu_wp(int num);

extern void __init mx51_efikamx_timer_init(void);

extern int mx51_efikamx_revision(void);

extern void __init mx51_efikamx_init_uart(void);
extern void __init mx51_efikamx_init_soc(void);
extern void __init mx51_efikamx_init_mmc(void);
extern void __init mx51_efikamx_init_audio(void);
extern void __init mx51_efikamx_init_pata(void);
extern void __init mx51_efikasb_init_leds(void);
extern void __init mx51_efikamx_init_spi(void);
extern void __init mx51_efikamx_init_nor(void);
extern void __init mx51_efikamx_init_display(void);
extern void __init mx51_efikamx_init_i2c(void);
extern void __init mx51_efikasb_init_battery(void);
extern void __init mx51_efikasb_init_wwan(void);
extern int  __init mx51_efikasb_init_pmic(void);

extern void mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem);

// DBG(("iomux [%u] %u,%u,%u,%u,%u\n", i, pins[i].pin, pins[i].mux_mode, pins[i].pad_cfg, pins[i].in_select, pins[i].in_mode));

#define CONFIG_IOMUX(pins) \
{\
	int i = 0; \
	for (i = 0; i < ARRAY_SIZE(pins); i++) { \
		mxc_request_iomux(pins[i].pin, pins[i].mux_mode); \
		if (pins[i].pad_cfg) \
			mxc_iomux_set_pad(pins[i].pin, pins[i].pad_cfg); \
		if (pins[i].in_select) \
			mxc_iomux_set_input(pins[i].in_select, pins[i].in_mode); \
	} \
}


#endif
