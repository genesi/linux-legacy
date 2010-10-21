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

int sii9022_reinit(struct fb_var_screeninfo *var);
int cs8556_reinit(struct fb_var_screeninfo *var);

#define VIDEO_MODE_HDMI_DEF     4
#define VIDEO_MODE_VGA_DEF      2

#define MEGA              1000000

int __initdata video_output = { VIDEO_OUT_STATIC_HDMI };
int __initdata video_mode = { VIDEO_OUT_STATIC_HDMI };
int __initdata hdmi_audio_auto_sw = { 0 };
int __initdata hdmi_audio = { 1 };
int __initdata enable_hdmi_spdif = { 0 };
int __initdata clock_auto = { 1 };
char __initdata vmode[32] = { 0 };
int __initdata mxc_debug = { 1 };
int __initdata extsync = { 1 };
int __initdata sink_dvi = { 0 };                /* default is HDMI */
int __initdata sink_monitor = { 0 };    /* default is TV */
int __initdata video_max_res = { 0 };   /* use supported max resolution in edid modelist */

u8 edid[256];

EXPORT_SYMBOL(hdmi_audio_auto_sw);
EXPORT_SYMBOL(hdmi_audio);
EXPORT_SYMBOL(enable_hdmi_spdif);
EXPORT_SYMBOL(clock_auto);
EXPORT_SYMBOL(vmode);
EXPORT_SYMBOL(mxc_debug);
EXPORT_SYMBOL(extsync);
EXPORT_SYMBOL(video_output);
EXPORT_SYMBOL(video_mode);

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

extern int sii9022_reinit(struct fb_var_screeninfo *var);
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
		// THIS SEEMS TO REPRESENT THE *PHYSICAL* WIRED PIXEL FORMAT
		// 8 lines per gun on HDMI
		.interface_pix_fmt = IPU_PIX_FMT_RGB24,
		.mode_str = "800x600-16@60", // safe default for HDMI
	},
	{
		// 5/6/5 per gun on VGA
		.interface_pix_fmt = IPU_PIX_FMT_RGB565,
		.mode_str = "800x600-16@60", // safe default for VGA
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
		} else if ( (modedb[i].lower_margin < 2) ) {
			printk(KERN_INFO "%ux%u%s%u pclk=%u removed (lower margin does not meet IPU restrictions)\n",
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

	if (cpu_is_mx51_rev(CHIP_REV_3_0) > 0) {
		gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSI2_HSYNC), 1);
		msleep(1);
	}

	err = mxcfb_read_edid2(adp, buffer, len, &screeninfo, &dvi);

	if (cpu_is_mx51_rev(CHIP_REV_3_0) > 0)
		gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSI2_HSYNC), 0);


	if ( err )
		printk("read_edid error!\n");

	return err;
}

void mxcfb_clock_update(const char *clk_name_parent, unsigned long parent_rate,
					const char *clk_name_this, unsigned long divider)
{
	struct clk *clk, *di_clk;
	int ret=0, ret1=0;

	clk = clk_get(NULL, clk_name_parent);
	di_clk = clk_get(NULL, clk_name_this);

	clk_disable(clk);
	clk_disable(di_clk);

	printk("orig: parent=%s clk=%lu ", clk_name_parent, clk_get_rate(clk) );
	printk("this=%s di_clk=%lu\n", clk_name_this, clk_get_rate(di_clk));
	ret = clk_set_rate(clk, parent_rate);
	ret1 = clk_set_rate(di_clk, parent_rate / divider);

	printk("adjust: parent=%s clk=%lu ", clk_name_parent, clk_get_rate(clk));
	printk("this=%s di_clk=%lu ret=%d ret=%d\n", clk_name_this, clk_get_rate(di_clk), ret, ret1);
	clk_enable(clk);
	clk_enable(di_clk);

	clk_put(di_clk);
	clk_put(clk);
}

int mxcfb_di_clock_adjust(int video_output, u32 pixel_clock)
{
	char *clk_di = "ipu_di0_clk";
	u32 rate = 0;
	static u32 pixel_clock_last = 0;
	static int video_output_last = 0;

	if( clock_auto == 0 )
		return 0;

	/* avoid uncessary clock change to reduce unknown impact chance */
	if ( pixel_clock && (pixel_clock == pixel_clock_last) && video_output == video_output_last ) {
		printk(KERN_INFO "pclk %u same to previous one, skipping!\n", pixel_clock );
		return 0;
	}
	if ( pixel_clock < 6000 || pixel_clock > 40000 ) { /* 25Mhz~148Mhz */
		printk(KERN_INFO "pclk %u exceed limitation (6000~40000)!\n", pixel_clock );
		return -1;
	}
	pixel_clock_last = pixel_clock;
	video_output_last = video_output;

	if( video_output == VIDEO_OUT_STATIC_HDMI ) {
		clk_di = "ipu_di0_clk"; //hdmi
	}
#if defined(CONFIG_FB_MXC_CS8556)
	else {
		clk_di = "ipu_di1_clk";	//vga
	}
#endif

#define MEGA 1000000
	if ( pixel_clock == 0 ) {
		printk(KERN_INFO "%s invalid pclk, reset rate to %u\n",
			__func__, 260*MEGA );
		rate = 260*MEGA;
	}
	else {
		/* workaround for CVBS connector */
		if (pixel_clock == 37000 ) /* NTSC 480i */
			rate = (u32) ((((PICOS2KHZ(pixel_clock)))/1000)*1000000)*2;
		else
			rate =  (((u32)(PICOS2KHZ(pixel_clock) ))*1000 * 2);
	}

	printk("%s pixelclk=%u rate=%u\n", __func__, pixel_clock, rate );

	mxcfb_clock_update("pll3", rate, clk_di, 2);

	return 0;

}

void mxcfb_adjust(struct fb_var_screeninfo *var )
{
	if( clock_auto )
		mxcfb_di_clock_adjust( video_output, var->pixclock );

	if ( (video_output == VIDEO_OUT_STATIC_HDMI)) {
		sii9022_reinit( var );
	}
#if defined(CONFIG_FB_MXC_CS8556)
	else if ( video_output == VIDEO_OUT_STATIC_DSUB ) { /* VGA */
		cs8556_reinit( var );
	}
#endif
}

void mxcfb_update_default_var(struct fb_var_screeninfo *var,
				struct fb_info *info,
				const struct fb_videomode *def_mode )
{
	struct fb_monspecs *specs = &info->monspecs;
	const struct fb_videomode *mode = NULL;
	struct fb_var_screeninfo var_tmp;
	int modeidx = 0;

	printk(KERN_INFO "%s mode_opt=%s vmode=%s\n", __func__ , fb_mode_option, vmode );

	/* user specified vmode,  ex: support reduce blanking, such as 1280x768MR-16@60 */
	if ( vmode[0] ) {
		/* use edid support modedb or modedb in modedb.c */
		modeidx = fb_find_mode(var, info, vmode, specs->modedb, specs->modedb_len, def_mode, MXCFB_DEFAULT_BPP);
	}
	else if ( specs->modedb ) {

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

	if ( modeidx == 0 && mode == NULL ) { /* no best monitor support mode timing found, use def_video_mode timing ! */

		fb_videomode_to_var(var, def_mode);
	}
}

void __init mx51_efikamx_init_display(void)
{
	CONFIG_IOMUX(mx51_efikamx_display_iomux_pins);

	/* Deassert VGA reset to free i2c bus */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_EIM_A19), "vga_reset");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_EIM_A19), 1);

	/* LCD related gpio */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DI1_D1_CS), "lcd_gpio");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DI1_D1_CS), 0);

	/* DVI Reset - Assert for i2c disabled mode */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), "dvi_reset");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIN), 0);

	/* DVI Power-down */
	gpio_request(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO), "dvi_power");
	gpio_direction_output(IOMUX_TO_GPIO(MX51_PIN_DISPB2_SER_DIO), 1);

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

	printk("*** %s vmode=%s video-mode=%d clock_auto=%d\n", 
		  __func__, vmode, video_mode, clock_auto);

	if( video_output == VIDEO_OUT_STATIC_HDMI )
	{
		mxc_fb_devices[0].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[0].resource = mxcfb_resources;
		printk(KERN_INFO "registering framebuffer for HDMI\n");
		mxc_register_device(&mxc_fb_devices[0], &mxcfb_data[0]);	// HDMI
	}
#if defined(CONFIG_FB_MXC_CS8556)
	else
	{
		mxc_fb_devices[1].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[1].resource = mxcfb_resources;
		mxc_register_device(&mxc_fb_devices[1], &mxcfb_data[1]);	// VGA
	}
#endif

	printk(KERN_INFO "registering framebuffer for VPU overlay\n");
	mxc_register_device(&mxc_fb_devices[2], NULL);		// Overlay for VPU

	return 0;
}
//device_initcall(mxcfb_init_fb);

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



static int __init vga_setup(char *__unused)
{
	video_output = VIDEO_OUT_STATIC_DSUB;
	return 1;
}

static int __init hdmi_setup(char *__unused)
{
	video_output = VIDEO_OUT_STATIC_HDMI;
	return 1;
}

static int __init hdmi_spdif_setup(char *__unused)
{
	enable_hdmi_spdif = 1;
	return 1;
}

static int __init hdmi_audio_auto_sw_setup(char *options)
{
	if (!options || !*options)
		return 1;

	hdmi_audio_auto_sw = simple_strtol(options, NULL, 10);

	printk("hdmi audio auto switch=%d\n", hdmi_audio_auto_sw);

	return 1;
}

static int __init video_mode_setup(char *options)
{
	if (!options || !*options)
		return 1;

	video_mode = simple_strtol(options, NULL, 10);

	printk("video mode=%d\n", video_mode);

	return 1;
}

static int __init clock_setup(char *options)
{
	if (!options || !*options)
		return 1;

	clock_auto = simple_strtol(options, NULL, 10);
	printk("clock_auto=%d\n", clock_auto);

	return 1;
}

static int __init vmode_setup(char *options)
{
	if (!options || !*options)
		return 1;

	memset( vmode, 0, sizeof(vmode));
	strncpy( vmode, options, sizeof(vmode)-1 );
	printk("vmode=%s\n", vmode );

	return 1;
}

static int __init video_max_res_setup(char *options)
{
	if (!options || !*options)
		return 1;

	video_max_res = simple_strtol(options, NULL, 10);
	printk("video_max_res=%d\n", video_max_res);

	return 1;
}

__setup("vga", vga_setup);
__setup("hdmi", hdmi_setup);
__setup("spdif", hdmi_spdif_setup);
__setup("hdmi_audio_auto_sw=", hdmi_audio_auto_sw_setup);
__setup("video_mode=", video_mode_setup);
__setup("clock_auto=", clock_setup);
__setup("vmode=", vmode_setup);
__setup("video_max_res=", video_max_res_setup);
