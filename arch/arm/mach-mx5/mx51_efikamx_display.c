/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
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

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/gpio.h>
#include <mach/mxc_edid.h>

#include "devices.h"
#include "iomux.h"
#include "mx51_pins.h"

#include "mx51_efikamx.h"

#define VIDEO_MODE_HDMI_DEF     4

#define MEGA              1000000

u8 edid[256];

struct fb_videomode __initdata  preferred_mode;

static struct mxc_iomux_pin_cfg __initdata mx51_efikamx_display_iomux_pins[] = {
	/* display reset pin */
	{
	 MX51_PIN_DISPB2_SER_DIN, IOMUX_CONFIG_GPIO,
	 0,
	 MUX_IN_GPIO3_IPP_IND_G_IN_5_SELECT_INPUT,
	 INPUT_CTL_PATH1,
	 },
};

int mxcfb_initialized = 0;

void mx51_efikamx_display_reset(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);
	msleep(50);

	/* do reset */
	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 1);
	msleep(20);		/* tRES >= 50us */

	gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);
}


static struct resource mxcfb_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		},
};

static struct mxc_fb_platform_data mxcfb_data[] = {
	{
		.interface_pix_fmt = IPU_PIX_FMT_RGB24, /* physical pixel format (to transmitter */
		.mode_str = "800x600-16@60", // safe default for HDMI
	},
};

extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};


int mxcfb_read_edid2(struct i2c_adapter *adp, char *edid, u16 len, struct fb_var_screeninfo *einfo, int *dvi)
{
	int i;
	u8 buf0[2] = {0, 0};
	int dat = 0;
	u16 addr = 0x50;
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

	printk("%s \n", __func__ );

	if (adp == NULL || einfo == NULL)
		return -EINVAL;

	buf0[0] = 0x00;
	memset(edid, 0, len);
	memset(einfo, 0, sizeof(struct fb_var_screeninfo));
	msg[1].len = len;
	dat = i2c_transfer(adp, msg, 2);

	/* If 0x50 fails, try 0x37. */
	if (edid[1] == 0x00) {
		msg[0].addr = msg[1].addr = 0x37;
		dat = i2c_transfer(adp, msg, 2);
	}

	if (edid[1] == 0x00)
		return -ENOENT;

	*dvi = 0;
	if ((edid[20] == 0x80) || (edid[20] == 0x88) || (edid[20] == 0))
		*dvi = 1;

	dat = fb_parse_edid(edid, einfo);
	if (dat)
		return -dat;

	/* This is valid for version 1.3 of the EDID */
	if ((edid[18] == 1) && (edid[19] == 3)) {
		einfo->height = edid[21] * 10;
		einfo->width = edid[22] * 10;
	}

	for(i=0; i< len; i+=32) {
	   int j=0;

	   for(j=0; j<32; j++) {
		  printk("%02x", edid[i+j] );
	   }
	   printk("\n");
	}

	return 0;
}

void mxcfb_dump_modeline( struct fb_videomode *modedb, int num)
{
	int i;
	struct fb_videomode *mode;

	for (i = 0; i < num; i++) {

		mode = &modedb[i];

		BUG_ON(mode->pixclock == 0);
		if (mode->pixclock == 0) {
			printk(KERN_ERR "skipping mode entry %u due to bad pclk", i);
			continue;
		}

		printk("   \"%dx%d%s%d\" %lu.%02lu ",
			mode->xres, mode->yres, (mode->vmode & FB_VMODE_INTERLACED) ? "i@" : "@", mode->refresh,
			(PICOS2KHZ(mode->pixclock) * 1000UL)/1000000,
			(PICOS2KHZ(mode->pixclock) ) % 1000);
		printk("%d %d %d %d ",
			mode->xres,
			mode->xres + mode->right_margin,
			mode->xres + mode->right_margin + mode->hsync_len,
			mode->xres + mode->right_margin + mode->hsync_len + mode->left_margin );
		printk("%d %d %d %d ",
			mode->yres,
			mode->yres + mode->lower_margin,
			mode->yres + mode->lower_margin + mode->vsync_len,
			mode->yres + mode->lower_margin + mode->vsync_len + mode->upper_margin );
		printk("%shsync %svsync\n", (mode->sync & FB_SYNC_HOR_HIGH_ACT) ? "+" : "-",
			   (mode->sync & FB_SYNC_VERT_HIGH_ACT) ? "+" : "-" );

	}
}

static int mxcfb_dump_mode( const char *func_char, const struct fb_videomode *mode)
{
	if ( mode == NULL )
		return -1;

	printk(KERN_INFO "%s geometry %u %u %u\n",
	  func_char,  mode->xres, mode->yres, mode->pixclock);
	printk(KERN_INFO "%s timings %u %u %u %u %u %u %u\n",
	  func_char, mode->pixclock, mode->left_margin, mode->right_margin,
	  mode->upper_margin, mode->lower_margin, mode->hsync_len, mode->vsync_len );
	printk(KERN_INFO "%s flag %u sync %u vmode %u %s\n",
	  func_char, mode->flag, mode->sync, mode->vmode, mode->flag & FB_MODE_IS_FIRST ? "preferred" : "" );

	return 0;
}


void mxcfb_videomode_to_modelist(const struct fb_info *info, const struct fb_videomode *modedb, int num,
			      struct list_head *head)
{
	int i, del = 0;
	INIT_LIST_HEAD(head);

	for (i = 0; i < num; i++) {
		struct list_head *pos, *n;
		struct fb_modelist *modelist;

		del = 0;

		if ( modedb[i].pixclock < PIXCLK_LIMIT ) {
			printk(KERN_INFO "%ux%u%s%u pclk=%u removed (exceed limit)\n",
				modedb[i].xres, modedb[i].yres,
				(modedb[i].vmode & FB_VMODE_INTERLACED ) ? "i@" : "@",
				modedb[i].refresh,
				modedb[i].pixclock );
			continue;
		} else if ( (modedb[i].vmode & FB_VMODE_INTERLACED) ) {
			printk(KERN_INFO "%ux%u%s%u pclk=%u removed (interlaced)\n",
				modedb[i].xres, modedb[i].yres,
				(modedb[i].vmode & FB_VMODE_INTERLACED ) ? "i@" : "@",
				modedb[i].refresh,
				modedb[i].pixclock );
			continue;
		}

		list_for_each_safe(pos, n, head) {
			modelist = list_entry(pos, struct fb_modelist, list);

			if ( res_matches_refresh(modelist->mode,
						modedb[i].xres, modedb[i].yres, modedb[i].refresh) ) {

				/* if candidate is a detail timing, delete existing one in modelist !
				  * note: some TV has 1280x720@60 in standard timings but also has 1280x720 in detail timing block 
				  *         in this case, use detail timing !
				  */
				if ( modedb[i].flag & FB_MODE_IS_DETAILED ) {

					printk(KERN_INFO "%ux%u%s%u pclk=%u removed (duplicate)\n",
						modelist->mode.xres, modelist->mode.yres, 
						(modelist->mode.vmode & FB_VMODE_INTERLACED ) ? "i@" : "@",
						modelist->mode.refresh,
						modelist->mode.pixclock );
					list_del(pos);
					kfree(pos);
					del = 1;
				}
			}
		}

		if ( del == 0 )
			fb_add_videomode(&modedb[i], head);

	}

}

void mxcfb_sanitize_modelist(const struct fb_info *info, const struct fb_videomode *modedb, int num,
			      struct list_head *head)
{
	struct fb_videomode *pmode;
	int i;

	pmode = (struct fb_videomode *) fb_find_best_display(&info->monspecs, head);
	if (pmode)
	{
		//fb_dump_mode("PREFERRED", pmode);
		memcpy((void *) &preferred_mode, (const void *) pmode, sizeof(struct fb_videomode));
	}

	for (i = 0; i < num; i++) {

		struct list_head *pos, *n;
		struct fb_modelist *modelist;

		list_for_each_safe(pos, n, head) {
			modelist = list_entry(pos, struct fb_modelist, list);
			BUG_ON(modelist->mode.pixclock == 0);
			if (modelist->mode.pixclock == 0) {
				printk(KERN_ERR "skipping mode %ux%u due to bad pclk",
					modelist->mode.xres, modelist->mode.yres);
				continue;
			}
			if (PICOS2KHZ(modelist->mode.pixclock) > 133000 ) {
				printk(KERN_INFO "%ux%u%s%u pclk=%u removed (pixclk higher than %lu limit)\n",
					modelist->mode.xres, modelist->mode.yres,
					(modelist->mode.vmode & FB_VMODE_INTERLACED ) ? "i@" : "@",
					modelist->mode.refresh,
					modelist->mode.pixclock, KHZ2PICOS(133000) );

				if (0 == memcmp((void *) &preferred_mode, (void *) &modelist->mode, sizeof(struct fb_videomode)))
				{
					printk(KERN_INFO "uh-oh! deleted preferred mode!\n");
				}
				list_del(pos);
				kfree(pos);
			}
		}

	}
}

int mxcfb_handle_edid2(struct i2c_adapter *adp, char *buffer, u16 len)
{
	int err = 0;
	int dvi = 0;
	struct fb_var_screeninfo screeninfo;

	memset(&screeninfo, 0, sizeof(screeninfo));

	err = mxcfb_read_edid2(adp, buffer, len, &screeninfo, &dvi);

	if ( err )
		printk("read_edid error!\n");

	return err;
}

void mxcfb_adjust(struct fb_var_screeninfo *var )
{
	char *di = "ipu_di0_clk", *parent = "pll3";
	struct clk *clk_di, *clk_parent;
	int ret_di = 0, ret_parent = 0;

	u32 rate = 0;
	static u32 pixel_clock_last = 0;
	int pixel_clock = var->pixclock;


	/* avoid uncessary clock change to reduce unknown impact chance */
	if ( pixel_clock && (pixel_clock == pixel_clock_last) ) {
		printk(KERN_INFO "pclk %u unchanged, not adjusting display clocks\n", pixel_clock );
		return;// 0;
	}
	if ( (((u32)PICOS2KHZ(pixel_clock)) < 25000) || (((u32)PICOS2KHZ(pixel_clock)) > 133000) ) {
		printk(KERN_INFO "pclk %u (%uMHz) exceeds clock limitation (25-133MHz)!\n", pixel_clock, ((u32)PICOS2KHZ(pixel_clock))/1000 );
		return;// -1;
	}
	pixel_clock_last = pixel_clock;

	if ( pixel_clock == 0 ) {
		rate = 26000000;
		printk(KERN_INFO "%s invalid pclk, reset rate to %u\n", __func__, rate );
	} else {
		rate =  (((u32)(PICOS2KHZ(pixel_clock)))*1000);
	}

	printk("%s pixelclk=%u rate=%u\n", __func__, pixel_clock, rate );

	clk_parent = clk_get(NULL, parent);
	clk_di = clk_get(NULL, di);

	clk_disable(clk_parent);
	clk_disable(clk_di);

	printk(" current %s rate %lu\n", parent, clk_get_rate(clk_parent) );
	printk(" current %s rate %lu\n", di, clk_get_rate(clk_di));

	ret_parent = clk_set_rate(clk_parent, rate * 2);
	ret_di = clk_set_rate(clk_di, rate);

	printk(" new %s rate %lu (return %d)\n", parent, clk_get_rate(clk_parent), ret_parent);
	printk(" new %s rate %lu (return %d)\n", di, clk_get_rate(clk_di), ret_di);

	clk_enable(clk_parent);
	clk_enable(clk_di);

	clk_put(clk_parent);
	clk_put(clk_di);

	return;// 0;
}

void mxcfb_update_default_var(struct fb_var_screeninfo *var,
				struct fb_info *info,
				const struct fb_videomode *def_mode )
{
	struct fb_monspecs *specs = &info->monspecs;
	const struct fb_videomode *mode = NULL;
	struct fb_var_screeninfo var_tmp;

	printk(KERN_INFO "%s fb_mode_option = \"%s\"\n", __func__ , fb_mode_option );

	if ( specs->modedb ) {

		fb_videomode_to_var( &var_tmp, def_mode);
		mode = fb_find_nearest_mode( def_mode, &info->modelist );

		if ( mode ) {
			fb_videomode_to_var(var, mode);
			printk(KERN_INFO "%ux%u@%u pclk=%u nearest mode is %ux%u@%u pclk=%u\n",
				def_mode->xres, def_mode->yres, def_mode->refresh, def_mode->pixclock,
				mode->xres, mode->yres, mode->refresh, mode->pixclock );
			mxcfb_dump_mode( "nearest mode", mode);
		}
	}

	if ( mode == NULL ) { /* no best monitor support mode timing found, use def_video_mode timing ! */
		fb_videomode_to_var(var, def_mode);
	}
}

void __init mx51_efikamx_init_display(void)
{
	CONFIG_IOMUX(mx51_efikamx_display_iomux_pins);

	/* HDMI Reset - Assert for i2c disabled mode */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), "hdmi:reset");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);

	/* HDMI Interrupt pin (plug detect etc.)  */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO), "hdmi:irq");
	gpio_direction_input(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO));

	mxc_ipu_data.di_clk[0] = clk_get(NULL, "ipu_di0_clk");
	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_ipu_data.csi_clk[0] = clk_get(NULL, "csi_mclk1");
	mxc_ipu_data.csi_clk[1] = clk_get(NULL, "csi_mclk2");

	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, NULL);
}

/* name is not my choice but.... */
int mxc_init_fb(void)
{
	if ( mxcfb_initialized )
		return 0;

	mxcfb_initialized = 1;

	mxc_fb_devices[0].num_resources = ARRAY_SIZE(mxcfb_resources);
	mxc_fb_devices[0].resource = mxcfb_resources;
	printk(KERN_INFO "registering framebuffer for HDMI\n");
	mxc_register_device(&mxc_fb_devices[0], &mxcfb_data[0]);	// HDMI

	printk(KERN_INFO "registering framebuffer for VPU overlay\n");
	mxc_register_device(&mxc_fb_devices[2], NULL);		// Overlay for VPU

	return 0;
}
device_initcall(mxc_init_fb);

void mx51_efikamx_display_adjust_mem(int gpu_start, int gpu_mem, int fb_mem)
{
		/*reserve memory for gpu*/
		gpu_device.resource[5].start = gpu_start;
		gpu_device.resource[5].end =
				gpu_device.resource[5].start + gpu_mem - 1;
		if (fb_mem) {
			mxcfb_resources[0].start =
				gpu_device.resource[5].end + 1;
			mxcfb_resources[0].end =
				mxcfb_resources[0].start + fb_mem - 1;
		} else {
			mxcfb_resources[0].start = 0;
			mxcfb_resources[0].end = 0;
		}
}
