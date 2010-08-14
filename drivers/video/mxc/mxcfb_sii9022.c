/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup Framebuffer Framebuffer Driver for SDC and ADC.
 */

/*!
 * @file mxcfb_sii9022.c
 *
 * @brief MXC Frame buffer driver for SDC
 *
 * @ingroup Framebuffer
 */

/*!
 * Include files
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/mxcfb.h>
#include <linux/ipu.h>
#include <linux/clk.h>
#include "../edid.h"
#include "../arch/arm/mach-mx5/mx51_efikamx.h"

#define DPRINTK	printk

extern int video_output;
extern int video_mode;
extern char vmode[32];
extern int sink_dvi;
extern int sink_monitor;
extern int video_max_res;
extern u8 edid[256];

extern void mxcfb_adjust( struct fb_var_screeninfo *var );
extern void mxcfb_dump_modeline( struct fb_videomode *modedb, int num);
extern void mxcfb_update_default_var(struct fb_var_screeninfo *var, 
									struct fb_info *info, 
									const struct fb_videomode *def_video_mode );
extern int mxcfb_handle_edid2(struct i2c_adapter *adp, char *buffer, u16 len);

struct i2c_client *sii9022_client;

#define HDMI_CTL_VIDEO_ENABLE	0x01
#define HDMI_CTL_AUDIO_ENABLE	0x02
#define HDMI_CTL_AUDIO_MUTE		0x04
#define HDMI_CTL_AUDIO_UMMUTE	0x08
#define HDMI_CTL_DETECT_CHIP	0x10
#define HDMI_CTL_DETECT_SINK	0x20
#define HDMI_CTL_EDID_GET		0x40

/* HDMI EDID Length */
#define HDMI_EDID_MAX_LENGTH 256
#define SI9022_REG_TPI_RQB 0xC7
#define HDMI_SYS_CTRL_DATA_REG 0x1A

/* HDMI Address */
#define SI9022_I2CSLAVEADDRESS 0x39
#define SI9022_EDIDI2CSLAVEADDRESS 0x50

/* HDMI_SYS_CTRL_DATA_REG */
#define TPI_SYS_CTRL_DDC_BUS_REQUEST (1 << 2)
#define TPI_SYS_CTRL_DDC_BUS_GRANTED (1 << 1)

#define TPI_SYS_CTRL_OUTPUT_MODE_DVI	(0 << 0)
#define TPI_SYS_CTRL_OUTPUT_MODE_HDMI	(1 << 0)

#define TPI_SYS_CTRL_POWER_DOWN		(1 << 4)
#define TPI_SYS_CTRL_POWER_ACTIVE	(0 << 4)

/* Stream Header Data */
#define HDMI_SH_PCM (0x1 << 4)
#define HDMI_SH_TWO_CHANNELS (0x1 << 0)
#define HDMI_SH_44KHz (0x2 << 2)
#define HDMI_SH_48KHz (0x3 << 2)
#define HDMI_SH_16BIT (0x1 << 0)

u8 hdmi_sh_value = (HDMI_SH_48KHz|HDMI_SH_16BIT);

#define HDMI_XRES                      1280
#define HDMI_YRES                      720
#define HDMI_PIXCLOCK_MAX              74250

#define EDID_TIMING_DESCRIPTOR_SIZE            0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS         0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS         0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR     4   
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR     4

int hdmi_video_init = 0;

EXPORT_SYMBOL(sii9022_client);

int sii9022_hdmi_video_init(struct fb_var_screeninfo *var);
int sii9022_hdmi_audio_init(void);
int sii9022_reinit(struct fb_var_screeninfo *var);
extern void mxcfb_videomode_to_modelist(const struct fb_info *info, const struct fb_videomode *modedb, int num,
			      struct list_head *head);
extern void mxcfb_sanitize_modelist(const struct fb_info *info, const struct fb_videomode *modedb, int num,
			      struct list_head *head);
static int sii9022_handle_edid(struct i2c_client *client, char *edid_buffer, u16 len );

static void lcd_poweron(struct fb_info *info);
static void lcd_poweroff(void);

//static void (*lcd_reset) (void);
extern int enable_hdmi_spdif;
extern int ce_mode;
extern int video_mode;
extern int clock_auto;
extern int hdmi_audio;
extern int video_output;

static struct fb_videomode video_modes_ce_mode_4 = {
     /* 720p60 TV output */
     .name          = "720P60",
     .refresh       = 60,
     .xres          = 1280,
     .yres          = 720,
     .pixclock      = 13468,
     .left_margin   = 220,
     .right_margin  = 110,
     .upper_margin  = 20,
     .lower_margin  = 5,
     .hsync_len     = 40,
     .vsync_len     = 5,
     .sync          = FB_SYNC_BROADCAST | FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT | FB_SYNC_EXT,
     .vmode         = FB_VMODE_NONINTERLACED,
};

static struct fb_videomode video_modes_ce_mode_19 = {
     /* 720p50 TV output */
     .name          = "720P50",
     .refresh       = 50,
     .xres          = 1280,
     .yres          = 720,
     .pixclock      = 13468,
     .left_margin   = 220,
     .right_margin  = 440,
     .upper_margin  = 20,
     .lower_margin  = 5,
     .hsync_len     = 40,
     .vsync_len     = 5,
	 .sync			= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT | FB_SYNC_EXT,
     .vmode         = FB_VMODE_NONINTERLACED,
};

/* non-standard 720p timing but work for full screen movie playback */
static struct fb_videomode video_modes_1280x720_65 = {
	.name		   = "1280x720@65",
	.refresh	   = 60,
	.xres 			= 1280,
	.yres 			= 720,
	.pixclock 		= 16260,
	.left_margin 	= 32,
	.right_margin 	= 48,
	.upper_margin	= 7,
	.lower_margin 	= 3,
	.hsync_len 		= 32,
	.vsync_len 		= 6,
	.sync 			= FB_SYNC_OE_LOW_ACT,
	.vmode		   = FB_VMODE_NONINTERLACED,
};

//standard vesa 1024x768@60 pixel clock: 65.00Mhz
static struct fb_videomode video_modes_1024x768_60 = {
	.name		   = "1024x768@60-65Mhz",
	.refresh	   = 60,
	.xres		   = 1024,
	.yres		   = 768,
	.pixclock	   = 15384, //pico seconds of 65.0Mhz
	.left_margin   = 160, 
	.right_margin  = 24,
	.upper_margin  = 29,   
	.lower_margin  = 3,
	.hsync_len	   = 136,
	.vsync_len	   = 6,
	.sync		   = FB_SYNC_EXT,
	.vmode		   = FB_VMODE_NONINTERLACED,
	.flag		   = FB_MODE_IS_VESA,
};

static struct fb_videomode video_modes_1980x1080_60 = {
	.name		   = "1980x1080@60-148Mhz",
	.refresh	   = 60,
	.xres		   = 1980,
	.yres		   = 1080,
	.pixclock	   = 6734,
	.left_margin   = 148, 
	.right_margin  = 88,
	.upper_margin  = 36,   
	.lower_margin  = 4,
	.hsync_len	   = 44,
	.vsync_len	   = 5,
	.sync		   = FB_SYNC_EXT,
	.vmode		   = FB_VMODE_NONINTERLACED,
};

#undef dev_dbg
#define dev_dbg(dev, format, arg...)		\
	dev_printk(KERN_INFO , dev , format , ## arg)

int edid_parse( u8 *edid, int *sink_monitor, int *sink_dvi )
{
	if (edid[126] > 0 ) { 

		if (edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS] == 0x02) {
			/* This block is CEA extension */
			DPRINTK("----------------------------------------\n");
			DPRINTK("	Revision number: %d\n", 
						edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS+1] );
			DPRINTK("	DTV underscan: %s\n",
						(edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS+3] & 0x80 ) ?
							"Supported" : "Not supported");
			DPRINTK("	Basic audio: %s\n",
						(edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS+3] & 0x40 ) ?
							"Supported" : "Not supported");
			if ( (edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS+3] & 0x40 ) ) {
				*sink_monitor = 0;
				*sink_dvi = 0;
			}
			else {
				*sink_monitor = 1;
				*sink_dvi = 1;
			}
		}

	}
	else {
		/* no CEA extension, it must be DVI */
		*sink_monitor = 1;
		*sink_dvi = 1;
	}

	printk(KERN_INFO "	Sink device: %s, %s\n", 
					*sink_monitor ? "Monitor" : "TV",
					*sink_dvi ? "DVI" : "HDMI" );

	return 0;
}

static int sii9022_handle_edid(struct i2c_client *client, char *edid, u16 len )
{
	int err = 0;
	u8 val = 0;
	int retries = 0;

	len = (len < HDMI_EDID_MAX_LENGTH) ? len : HDMI_EDID_MAX_LENGTH;
	memset(edid, 0, len);

	/* Hardware reset to Tx subsystem */
	i2c_smbus_write_byte_data(sii9022_client, SI9022_REG_TPI_RQB, 0x00);

	val = i2c_smbus_read_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG);
	val |= TPI_SYS_CTRL_DDC_BUS_REQUEST;

	err = i2c_smbus_write_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_printk( KERN_INFO, &sii9022_client->dev, "write DDC Bus req err!\n");
		return err;
	}

	/* polling to grant ddc bus access */
	val = 0;
	do {
		val = i2c_smbus_read_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG);
		if (retries++ > 20) {
			dev_printk( KERN_INFO, &sii9022_client->dev, "fail to poll ddc bus access!\n");
			return err;
		}
	} while ((val & TPI_SYS_CTRL_DDC_BUS_GRANTED) == 0);

	/* grant ddc bus access */
	val |= TPI_SYS_CTRL_DDC_BUS_REQUEST | TPI_SYS_CTRL_DDC_BUS_GRANTED;
	err = i2c_smbus_write_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_printk( KERN_INFO, &sii9022_client->dev, "fail to grant ddc bus!\n");
		return err;
	}

	/* read EDID info */
	client->addr = SI9022_EDIDI2CSLAVEADDRESS;	//edid addr=0x50,  i2c slave addr=0x39
	err = mxcfb_handle_edid2( client->adapter, edid, len );

	/*
	  * sil9022_blockread_reg() completely fail to read
	  * i2c_smbus_read_i2c_block_data() only read 32 bytes!
	  */

	/* release ddc bus access */
	client->addr = SI9022_I2CSLAVEADDRESS;
	val &= ~(TPI_SYS_CTRL_DDC_BUS_REQUEST | TPI_SYS_CTRL_DDC_BUS_GRANTED);
	i2c_smbus_write_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG, val);

	return err ;

}

static unsigned char audio_info[15] = {
	0xC2,	//
	0x84,	//frame type
	0x01,	//version
	0x0A,	//length
	0x71,	//cksum
	HDMI_SH_PCM | HDMI_SH_TWO_CHANNELS, 
	HDMI_SH_48KHz | HDMI_SH_16BIT, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

int sii9022_hdmi_audio_ctl(int mute)
{
	u8	dat;

	if( mute ) {
	
		//print(" i2s, mute = 0x90, spdif, mute = 0x50, \n");
		if( enable_hdmi_spdif )
		  i2c_smbus_write_byte_data(sii9022_client, 0x26 ,0x50);
		else
		  i2c_smbus_write_byte_data(sii9022_client, 0x26 ,0x90);
		printk(KERN_INFO "hdmi audio mute\n");

	}
	else { /* unmute */

		// I2S, unmute, PCM=0001 = 0x80,  SPDIF = 0x40
		//i2c_smbus_write_byte_data(sii9022_client, 0x26, 0x80);
		if( enable_hdmi_spdif )
		  i2c_smbus_write_byte_data(sii9022_client, 0x26, 0x40);
		else
		  i2c_smbus_write_byte_data(sii9022_client, 0x26, 0x80);

		i2c_smbus_write_i2c_block_data(sii9022_client, 0xBF, 15, audio_info );
		
		//print(" back door reG0x2F=2CH \n");
		i2c_smbus_write_byte_data(sii9022_client, 0xBC, 0x02);
		i2c_smbus_write_byte_data(sii9022_client, 0xBD, 0x2F);
		dat = i2c_smbus_read_byte_data(sii9022_client, 0xBE);
		dat &= 0xFD; /* layout 0 : 2 channel */
		i2c_smbus_write_byte_data(sii9022_client, 0xBE, dat);

		printk(KERN_INFO "hdmi audio unmute\n");
	}

	return 0;
}

int sii9022_hdmi_audio_init(void)
{
	u8	cksum = 0;
	int i;

	sii9022_hdmi_audio_ctl(1);

	i2c_smbus_write_byte_data(sii9022_client, 0x25, 0x00);
	// 24bit, 48kHz, 2 channel  = 0xD9
	// 16bit, 48kHz, 2 channel  = 0x59
	// 16bit, 44.1kHz, 2 channel =0x51
	// refer to stream          = 0x00
	// SPDIF only?
	i2c_smbus_write_byte_data(sii9022_client, 0x27, 0x59);
	i2c_smbus_write_byte_data(sii9022_client, 0x28, 0x00);

	i2c_smbus_write_byte_data(sii9022_client, 0x1F, 0x80);

	//print(" rising,256Fs,ws-low=left,left justify,msb 1st,1 bit shift \n");
	i2c_smbus_write_byte_data(sii9022_client, 0x20, 0x90);

	// PCM format
	i2c_smbus_write_byte_data(sii9022_client, 0x21, 0x00);

	// 48K = 0x02, 44.1K = 0x00
	i2c_smbus_write_byte_data(sii9022_client, 0x24, 0x02); //0x22

	// 16bit=0x02 , 24b = 1011 (default), pass basic audio only = 0x00
	i2c_smbus_write_byte_data(sii9022_client, 0x25, 0x02); //0xd2

	//audio_info[6] = hdmi_sh_value;
	
	for( i=1; i < 15; i++ )
		cksum += audio_info[i];
	audio_info[4] = 0x100 - cksum;

	//print(" back door reG0x24=16 BIT \n");
	i2c_smbus_write_byte_data(sii9022_client, 0xBC, 0x02);
	i2c_smbus_write_byte_data(sii9022_client, 0xBD, 0x24);
	i2c_smbus_write_byte_data(sii9022_client, 0xBE, 0x02);

	sii9022_hdmi_audio_ctl(0);

	return 0;
}

#define ASPECT_RATIO_NONE	0x08
#define ASPECT_RATIO_4_3	0x18
#define ASPECT_RATIO_16_9	0x28

struct video_mode_map {
	u16 xres;
	u16 yres;
	u8	video_code;
	u8	refresh;
	u8	aspect_ratio;
};

#define MAX_XRES	2048
#define MAX_YRES	2048
#define DIFF(a,b) (a>b ? a-b : b-a)

static struct video_mode_map vmode_map[] = {
	{ 1, 1, 0, 60, ASPECT_RATIO_NONE },
	{ 640, 480, 1, 60, ASPECT_RATIO_4_3 },
	{ 720, 480, 3, 50, ASPECT_RATIO_16_9 },
	{ 720, 576, 17, 50, ASPECT_RATIO_4_3 },
	{ 800, 600, 0, 60, ASPECT_RATIO_4_3 },
	{ 1024, 768, 0, 60, ASPECT_RATIO_4_3 },
	{ 1280, 720, 4, 60, ASPECT_RATIO_16_9 },
	{ 1280, 720, 19, 50, ASPECT_RATIO_16_9 },
	{ 1280, 768, 0, 60, ASPECT_RATIO_16_9 },
	{ 1360, 768, 0, 60, ASPECT_RATIO_16_9 },
	{ 1680, 1050, 0, 60, ASPECT_RATIO_16_9 },
	{ 1920, 1080, 33, 25, ASPECT_RATIO_16_9 },
	{ 1920, 1080, 34, 30, ASPECT_RATIO_16_9 },
	{ MAX_XRES+1, MAX_YRES+1, 0, 0, ASPECT_RATIO_NONE},
};

int video_mode_map_get(__u32 x, __u32 y, u32 refresh)
{
	int i=0; 
	
	for( i=0; (vmode_map[i].xres < MAX_YRES); i++ ) {
		if ( x > vmode_map[i].xres )
			continue;
		if ( i && (x <= vmode_map[i].xres) && (y <= vmode_map[i].yres ) ) { /* so that 1124x644 will be become 720p to avoid overscan issue */
			if( DIFF(refresh, vmode_map[i].refresh) < 3 ) /* refresh rate diff < 3Hz */
				return i;
			else
				continue;
		}

		if ( y > vmode_map[i].yres ) /* no similar found */
			return 0;
	}
	return 0;
}

int sii9022_hdmi_video_init(struct fb_var_screeninfo *var)
{
	unsigned char avi_info[14] = { 0x00, 0x12, 0x28, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; 
	char video_mode[8] = { 0x01, 0x1d, 0x88, 0x13, 0xbc, 0x07, 0xee, 0x02 };
	
	u32 htotal = 0, vtotal=0, refresh_rate;
	u8	cksum = 0, dat = 0;
	int i;
	int vmap_idx = 0;

	if ( var == NULL )
		return -1;

	// Turn off TMDS
	dat = (sink_dvi ? TPI_SYS_CTRL_OUTPUT_MODE_DVI : TPI_SYS_CTRL_OUTPUT_MODE_HDMI);
	dat |= TPI_SYS_CTRL_POWER_DOWN;
	i2c_smbus_write_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG, dat);

	msleep(128); /* wait at least 128ms */
	
	// back door reG0x81
	//i2c_smbus_write_byte_data(sii9022_client, 0xBC, 0x01);
	//i2c_smbus_write_byte_data(sii9022_client, 0xBD, 0x82);
	//i2c_smbus_write_byte_data(sii9022_client, 0xBE, 0x35);

	// Video Input Mode
	i2c_smbus_write_byte_data(sii9022_client, 0x09, 0x00); 	// All 8 bit mode, Auto-selected by [1:0]; RGB

	/* Video Output Mode
	 * 0x0A[4], 0 : BT.601  1: BT.709 
	  * 0x0A[3:2], 00: Auto-selected, 01: Full range, 10: limited range, 11: rsvd
	  * 0x0A[1], 00: HDMI RGB, 01: HDMI-YCbCr 4:4:4, 10: HDMI-YCbCr 4:2:2, 11: DVI RGB
	  */
	dat = sink_dvi ? 0x03: 0x00;	
	i2c_smbus_write_byte_data(sii9022_client, 0x0A, dat);	

	i2c_smbus_write_byte_data(sii9022_client, 0x19, 0x00);
	
	//video mode parameter to reg 00-07 \n ");
	htotal = (var->xres+var->left_margin+var->right_margin+var->hsync_len);
	vtotal = (var->yres+var->upper_margin+var->lower_margin+var->vsync_len);
	refresh_rate = 1000000/(htotal*vtotal/(PICOS2KHZ(var->pixclock)/10));

	video_mode[0x00] = (PICOS2KHZ(var->pixclock)/10) & 0xFF ;
	video_mode[0x01] = ((PICOS2KHZ(var->pixclock)/10) >> 8) & 0xFF ;
	video_mode[0x02] = refresh_rate & 0xff;
	video_mode[0x03] = (refresh_rate>>8) & 0xff; // v refresh rate
	video_mode[0x04] = htotal & 0xff; 
	video_mode[0x05] = (htotal>>8) & 0xff; //total pixel
	video_mode[0x06] = vtotal & 0xff;
	video_mode[0x07] = (vtotal>>8) & 0xff; //total line

	i2c_smbus_write_i2c_block_data(sii9022_client, 0x00, 8, video_mode );

	// burst write 720p60 AVI infoframe to Reg 0c-19 
	vmap_idx = video_mode_map_get( var->xres, var->yres, refresh_rate/100 );
	cksum = 0x82 + 0x02 + 0x0d;
	avi_info[2] = vmode_map[vmap_idx].aspect_ratio; //16:9 , 4:3 
	avi_info[4] = vmode_map[vmap_idx].video_code; //video code 720p@60: 4, 720p@50: 19  
	for( i=0; i < 14; i++ )
		cksum += avi_info[i];
	avi_info[0] = 0x100 - cksum;

	i2c_smbus_write_i2c_block_data(sii9022_client, 0x0C, 14, avi_info );

	dev_printk( KERN_INFO, &sii9022_client->dev, "geometry %u %u %u %u %u\n", 
		var->xres_virtual, var->yres_virtual, var->xres, var->yres, var->bits_per_pixel);
	dev_printk( KERN_INFO, &sii9022_client->dev, "timings %u %u %u %u %u %u %u\n",
		var->pixclock, var->left_margin, var->right_margin, 
		var->upper_margin, var->lower_margin, var->hsync_len, var->vsync_len );
	dev_printk( KERN_INFO, &sii9022_client->dev, "accel %u sync %u vmode=%u\n",
		var->accel_flags, var->sync, var->vmode );
	dev_printk( KERN_INFO, &sii9022_client->dev, "pclk %lu refresh %u total x %u y %u\n", 
												((PICOS2KHZ(var->pixclock)/10)), 
												refresh_rate,
												htotal,vtotal );

	dev_printk( KERN_INFO, &sii9022_client->dev, "vmap idx=%d vcode=%d\n",
												vmap_idx, avi_info[4] );

	return 0;
}

int sii9022_hdmi_ctl(int cmd, void *opt)
{
	int dat;
	
	// Set 9022 in hardware TPI mode on and jump out of D3 state
	dat = 0x00;
	i2c_smbus_write_byte_data(sii9022_client, SI9022_REG_TPI_RQB, 0x00);		// enable access to 9022 other regs
	
	// read device ID
	do {
		dat = i2c_smbus_read_byte_data(sii9022_client, 0x1B);
		if (dat < 0) {
			printk("SII9022 not found ! id=%02x\n", dat );
			return -1;
		}
	} while( dat != 0xB0 );

	if (cmd & HDMI_CTL_DETECT_CHIP)
		return 0;

	if ( cmd & HDMI_CTL_DETECT_SINK ) {
		
		/* 0x3D[2], 0: not plug, 1=plugged */
		dat = i2c_smbus_read_byte_data(sii9022_client, 0x3D);

		if ( dat & 0x07 )
			return 0;

		printk("SII9022 sink not found ! id=%02x\n", dat );

		return -2;
	}

	if( cmd & HDMI_CTL_EDID_GET ) {
		int err = 0;
		
		err = sii9022_handle_edid( sii9022_client, edid, sizeof(edid));
		if ( err == 0 ) {
			struct fb_monspecs *monspecs = (struct fb_monspecs *)opt;

			if ( monspecs == NULL )
				return 0;
			
			edid_parse( edid, &sink_monitor, &sink_dvi );
			fb_edid_to_monspecs(edid, monspecs);

			if ( monspecs->modedb_len ) {
				//fb_videomode_to_modelist( monspecs.modedb, monspecs.modedb_len,
				//					 &info->modelist);

				printk("Monitor/TV supported modelines\n");
				mxcfb_dump_modeline( monspecs->modedb, monspecs->modedb_len );
			}
			return 0;
		}
		return -3;
	}

	// Power up, wakeup to D0, otherwise sink will show 'no signal' 
	i2c_smbus_write_byte_data(sii9022_client, 0x1E, 0x00);

	if( cmd & HDMI_CTL_VIDEO_ENABLE ) {
		sii9022_hdmi_video_init( (struct fb_var_screeninfo *)opt);
	}

	if( cmd & HDMI_CTL_AUDIO_ENABLE)
		sii9022_hdmi_audio_init();

	if( cmd & HDMI_CTL_AUDIO_MUTE)
		sii9022_hdmi_audio_ctl(1);

	if( cmd & HDMI_CTL_AUDIO_UMMUTE)
		sii9022_hdmi_audio_ctl(0);

	//TURN on TMDS 
	dat = (sink_dvi ? TPI_SYS_CTRL_OUTPUT_MODE_DVI : TPI_SYS_CTRL_OUTPUT_MODE_HDMI);
	dat |= TPI_SYS_CTRL_POWER_ACTIVE;
	i2c_smbus_write_byte_data(sii9022_client, HDMI_SYS_CTRL_DATA_REG, dat );

	// Power up, wakeup to D0 again to take effect of 0x1A[0] sink type!
	i2c_smbus_write_byte_data(sii9022_client, 0x1E, 0x00);

	return 0;
	
}

int sii9022_hdmi_ctl_parse( const char *procfs_buffer )
{
	int status = 0;
	struct fb_monspecs monspecs;
	
	if ( strncmp( procfs_buffer, "hdmi_audio_off", 14)==0 )
		status = sii9022_hdmi_ctl(HDMI_CTL_AUDIO_MUTE, 0 ); //mute 
	else if ( strncmp( procfs_buffer, "hdmi_audio_on", 13)==0 )
		status = sii9022_hdmi_ctl(HDMI_CTL_AUDIO_UMMUTE, 0); //unmute 
	else if ( strncmp( procfs_buffer, "hdmi_edid_get", 13)==0 ) {
		status = sii9022_hdmi_ctl(HDMI_CTL_EDID_GET, (void *)&monspecs);
		fb_destroy_modedb(monspecs.modedb);
	}

	return status;		
}

int sii9022_reinit(struct fb_var_screeninfo *var)
{
	static u32 pclk = 0;
	static u32 xres = 0, yres= 0;

	if ( hdmi_video_init == 0 ) {
		printk(KERN_INFO "skip %s!\n", __func__ );
		return -1;
	}
	
	if ( var->xres < 640 || var->yres < 480 || (var->pixclock < 6000 || var->pixclock > 40000) ) {
		printk(KERN_INFO "reinit %ux%u %u fail!\n", 
			var->xres, var->yres, var->pixclock );
		return -2;
	}

	if ( var->xres != xres || var->yres != yres || var->pixclock != pclk ) {

		printk(KERN_INFO "%s %ux%u %u\n", __func__, var->xres, var->yres, var->pixclock );

		xres = var->xres;
		yres = var->yres;
		pclk = var->pixclock;

		sii9022_hdmi_ctl( HDMI_CTL_VIDEO_ENABLE|HDMI_CTL_AUDIO_ENABLE, var );
	}
	else {
		printk(KERN_INFO "%s %ux%u %u same to previous one, skipping\n", __func__, var->xres, var->yres, var->pixclock );
	}

	return 0;
}

static void lcd_init_fb(struct fb_info *info)
{
	int err = 0;
	static struct fb_var_screeninfo var;

	printk("*** sii9022 %s\n", __func__);

	memset(&var, 0, sizeof(var));

	err = sii9022_handle_edid( sii9022_client, edid, sizeof(edid));

	if ( err == 0 ) {
		edid_parse( edid, &sink_monitor, &sink_dvi );
		fb_edid_to_monspecs(edid, &info->monspecs);

		if ( info->monspecs.modedb_len ) {
			printk("Monitor/TV supported modelines\n");
			mxcfb_dump_modeline( info->monspecs.modedb, info->monspecs.modedb_len );
			mxcfb_videomode_to_modelist(info, info->monspecs.modedb, info->monspecs.modedb_len,
							 &info->modelist);
			//mxcfb_sanitize_modelist(info, info->monspecs.modedb, info->monspecs.modedb_len,
			//				 &info->modelist);

		}
	}

	/*
	  * if customized video_mode == xx use video_mode_xx
	  * else if specfied video mode on booting parameter, find it timing from modelist
	  * else find 720p timing from modelist.
	  * modelist is generated from edid. if no edid, use modedb in modedb.c
	  */
	if( video_mode == 4 )
		fb_videomode_to_var(&var, &video_modes_ce_mode_4);

	else if ( video_mode == 19 )
		fb_videomode_to_var(&var, &video_modes_ce_mode_19);

	else if (video_mode == 2 )
		fb_videomode_to_var(&var, &video_modes_1024x768_60);

	else if ( video_mode == 3 )
		fb_videomode_to_var(&var, &video_modes_1280x720_65);

	else {
		struct fb_videomode *def_mode;

		if ( video_max_res )
			def_mode = &video_modes_1980x1080_60;
 		else
			def_mode = &video_modes_ce_mode_4;

		mxcfb_update_default_var( &var, info, def_mode );
	}

	hdmi_video_init = 1;
	mxcfb_adjust( &var );

	var.activate = FB_ACTIVATE_ALL;

	acquire_console_sem();
	info->flags |= FBINFO_MISC_USEREVENT;
	fb_set_var(info, &var);
	info->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();

//	fb_destroy_modedb(info->monspecs.modedb);
//	info->monspecs.modedb = NULL;

}

static int lcd_fb_event(struct notifier_block *nb, unsigned long val, void *v)
{
	struct fb_event *event = v;

	printk("*** sii9022 %s\n", __func__);

	if (strcmp(event->info->fix.id, "DISP3 BG")) {
		return 0;
	}

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
		lcd_init_fb(event->info);
		lcd_poweron(event->info);
		break;
	case FB_EVENT_BLANK:
		if (*((int *)event->data) == FB_BLANK_UNBLANK) {
			lcd_poweron(event->info);
		} else {
			lcd_poweroff();
		}
		break;
	}
	return 0;
}

static struct notifier_block nb = {
	.notifier_call = lcd_fb_event,
};




static int lcd_on;
/*
 * Send Power On commands to L4F00242T03
 *
 */
static void lcd_poweron(struct fb_info *info)
{

	if (lcd_on)
		return;

	dev_dbg(&sii9022_client->dev, "turning on LCD\n");

	lcd_on = 1;
}

/*
 * Send Power Off commands to L4F00242T03
 *
 */
static void lcd_poweroff(void)
{
	if (!lcd_on)
		return;

	dev_dbg(&sii9022_client->dev, "turning off LCD\n");

	lcd_on = 0;
}

/*!
 * This function is called whenever the SPI slave device is detected.
 *
 * @param	spi	the SPI slave device
 *
 * @return 	Returns 0 on SUCCESS and error on FAILURE.
 */
static int __devinit lcd_probe(struct device *dev)
{
	int i;
//	struct mxc_lcd_platform_data *plat = dev->platform_data;

	printk("*** sii9022 %s\n", __func__);

#if 0 /* this never ever compiled - Neko */
	if (plat) {
		/*
		io_reg = regulator_get(dev, plat->io_reg);
		if (!IS_ERR(io_reg)) {
			regulator_set_voltage(io_reg, 1800000);
			regulator_enable(io_reg);
		}
		core_reg = regulator_get(dev, plat->core_reg);
		if (!IS_ERR(core_reg)) {
			regulator_set_voltage(core_reg, 1200000);
			regulator_enable(core_reg);
		}
		*/
		lcd_reset = plat->reset;
		if (lcd_reset)
			lcd_reset();
	}
#endif

	for (i = 0; i < num_registered_fb; i++) {
		if (strcmp(registered_fb[i]->fix.id, "DISP3 BG") == 0) {
			lcd_init_fb(registered_fb[i]);
			fb_show_logo(registered_fb[i], 0);
			lcd_poweron(registered_fb[i]);
		}
	}

	fb_register_client(&nb);

	return 0;
}

static int __devinit sii9022_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	sii9022_client = client;

	printk("*** %s\n", __func__);

	/* if SII9022 not exist, switch to DSUB output ignoring user configured */
	if ( sii9022_hdmi_ctl( HDMI_CTL_DETECT_CHIP, NULL ) != 0 ) {
		video_output = VIDEO_OUT_STATIC_DSUB;
		return -2;
	}

	/* if HDMI sink is detected and output == AUTO, set to HDMI output */
	if ( sii9022_hdmi_ctl( HDMI_CTL_DETECT_SINK, NULL ) == 0 ) {
		video_output = VIDEO_OUT_STATIC_HDMI;
	}
	/* if EDID retrieve OK but HDMI sink not detected, it imply that 
	    D-SUB is connected */
	else if ( sii9022_hdmi_ctl( HDMI_CTL_EDID_GET, NULL ) == 0 ) {
		video_output = VIDEO_OUT_STATIC_DSUB;
	}

	if ( video_output == VIDEO_OUT_STATIC_DSUB  ) {
		printk("configure video output to D-SUB, skip HDMI\n");
		return -1;
	}

	/* set to HDMI output */
	video_output = VIDEO_OUT_STATIC_HDMI;

	mxc_init_fb();

	return lcd_probe(&client->dev);
}

static int __devexit sii9022_remove(struct i2c_client *client)
{
	fb_unregister_client(&nb);
	lcd_poweroff();
	return 0;
}

static int sii9022_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int sii9022_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id sii9022_id[] = {
	{ "sii9022", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sii9022_id);

static struct i2c_driver sii9022_driver = {
	.driver = {
		   .name = "sii9022",
		   },
	.probe = sii9022_probe,
	.remove = sii9022_remove,
	.suspend = sii9022_suspend,
	.resume = sii9022_resume,
	.id_table = sii9022_id,
};

static int __init sii9022_init(void)
{
	return i2c_add_driver(&sii9022_driver);
}

static void __exit sii9022_exit(void)
{
	i2c_del_driver(&sii9022_driver);
}

module_init(sii9022_init);
module_exit(sii9022_exit);

MODULE_AUTHOR("Pegatron Corporation");
MODULE_DESCRIPTION("SI9022 DVI/HDMI driver");
MODULE_LICENSE("PEGATRON-LICENSE");
