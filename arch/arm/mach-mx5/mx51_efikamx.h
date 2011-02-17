
#include "devices.h"
#include "mx51_pins.h"
#include "iomux.h"

#ifndef __MX51_EFIKAMX_H__
#define __MX51_EFIKAMX_H__

#define VIDEO_OUT_STATIC_AUTO	0
#define VIDEO_OUT_STATIC_HDMI	1
#define VIDEO_OUT_STATIC_DSUB	2

#define res_matches_refresh(v, x, y, r) \
			((v).xres == (x) && (v).yres == (y) && (v).refresh == (r))

extern int mx51_efikamx_revision(void);

/* move to mx51_efikamx.h */
extern void __init mx51_efikamx_io_init(void);
extern int __init mx51_efikamx_init_mc13892(void);
extern void __init mx51_efikamx_timer_init(void);

/* peripheral inits */
extern void __init mx51_efikamx_init_uart(void);
extern void __init mx51_efikamx_init_mmc(void);
extern void __init mx51_efikamx_init_leds(void);
extern void __init mx51_efikamx_init_power_key(void);
extern void __init mx51_efikamx_init_spi(void);
extern void __init mx51_efikamx_init_i2c(void);
extern void __init mx51_efikamx_init_i2c2(void);
extern void __init mx51_efikamx_init_nor(void);
extern void __init mx51_efikamx_init_display(void);
extern void __init mx51_efikamx_init_audio(void);
extern void __init mx51_efikamx_init_usb(void);
extern void __init mx51_efikamx_init_pata(void);
extern void __init mx51_efikamx_init_usb(void);
extern int __init mx51_efikamx_init_pmic(void);
extern void __init mx51_efikamx_init_soc(void);

/* io */
extern void mx51_efikamx_board_id(void);
extern int mx51_efikamx_revision(void);

/* cpu */
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
extern struct cpu_wp *mx51_efikamx_get_cpu_wp(int *wp);
extern void mx51_efikamx_set_num_cpu_wp(int num);

/* hmm? */
extern void mx51_efikamx_power_off(void);

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

extern int mx51_efikamx_reboot(void);
extern void mx51_efikamx_power_off(void);

extern void mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem);
#define DBG(x) { printk(KERN_INFO "Efika MX: "); printk x ; }

// also valid values: 125000 (leaves some bandwdith), 133333 (IPU max on MX51)
// arbitrarily chosen to be 117MHz just because I heard someone at Freescale
// mention this as the best value to leave enough for the VPU and so on..
#define PIXCLK_LIMIT KHZ2PICOS(117000)
#define MXCFB_DEFAULT_BPP 16

#endif /* __MX51_EFIKAMX_H__ */
