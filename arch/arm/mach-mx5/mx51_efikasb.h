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

/* here today, gone tomorrow */
/* cpu */
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
extern struct cpu_wp *mx51_efikamx_get_cpu_wp(int *wp);
extern void mx51_efikamx_set_num_cpu_wp(int num);

extern void __init mx51_efikamx_timer_init(void);

extern void __init mx51_efikasb_io_init(void);
extern void __init mx51_efikasb_init_battery(void);

extern void __init mx51_efikamx_init_uart(void);
extern void __init mx51_efikamx_init_soc(void);
extern void __init mx51_efikamx_init_mmc(void);
extern void __init mx51_efikamx_init_audio(void);
extern void __init mx51_efikamx_init_pata(void);
extern void __init mx51_efikamx_init_leds(void);
extern void __init mx51_efikamx_init_spi(void);
extern void __init mx51_efikamx_init_nor(void);
extern void __init mx51_efikamx_init_display(void);
extern void __init mx51_efikamx_init_i2c(void);
extern void __init mx51_efikamx_init_wwan(void);
extern void __init mx51_efikamx_init_input(void);
extern int  __init mx51_efikamx_init_pmic(void);
extern int  __init mx51_efikamx_init_usb(void);

/* io */
extern void mx51_efikamx_board_id(void);
extern int mx51_efikamx_revision(void);
extern char *mx51_efikamx_memory(void);

extern void mx51_efikamx_power_off(void);


extern void mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem);
#define DBG(x) { printk(KERN_INFO "Efika MX: "); printk x ; }

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

extern void mx51_efikamx_wifi_power(int state);
extern void mx51_efikamx_wifi_reset(void);
extern void mx51_efikamx_bluetooth_power(int state);
extern void mx51_efikamx_camera_power(int state);
extern void mx51_efikamx_wwan_power(int state);

#endif
