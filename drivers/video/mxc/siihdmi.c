/* vim: set noet ts=8 sts=8 sw=8 : */
/*
 * Copyright © 2010 Saleem Abdulrasool <compnerd@compnerd.org>.
 * Copyright © 2010-2011 Genesi USA, Inc. <matt@genesi-usa.com>.
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

#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/console.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#include <linux/edid.h>
#include <linux/cea861.h>

#include "../cea861_modedb.h"

#include "siihdmi.h"


/* logging helpers */
#define CONTINUE(fmt, ...)	printk(KERN_CONT    fmt, ## __VA_ARGS__)
#define DEBUG(fmt, ...)		printk(KERN_DEBUG   "SIIHDMI: " fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...)		printk(KERN_ERR     "SIIHDMI: " fmt, ## __VA_ARGS__)
#define WARNING(fmt, ...)	printk(KERN_WARNING "SIIHDMI: " fmt, ## __VA_ARGS__)
#define INFO(fmt, ...)		printk(KERN_INFO    "SIIHDMI: " fmt, ## __VA_ARGS__)


/* module parameters */
static unsigned int bus_timeout = 50;
module_param(bus_timeout, uint, 0644);
MODULE_PARM_DESC(bus_timeout, "bus timeout in milliseconds");

static unsigned int seventwenty	= 1;
module_param(seventwenty, uint, 0644);
MODULE_PARM_DESC(seventwenty, "attempt to use 720p mode");

static unsigned int teneighty = 0;
module_param(teneighty, uint, 0644);
MODULE_PARM_DESC(teneighty, "attempt to use 1080p mode");

static unsigned int useitmodes = 1;
module_param(useitmodes, uint, 0644);
MODULE_PARM_DESC(useitmodes, "prefer IT modes over CEA modes when sanitizing the modelist");

static unsigned int modevic = 0;
module_param_named(vic, modevic, uint, 0644);
MODULE_PARM_DESC(modevic, "CEA VIC to try and match before autodetection");

static unsigned int forcedvi = 0;
module_param_named(dvi, forcedvi, uint, 0644);
MODULE_PARM_DESC(forcedvi, "Force DVI sink mode");

static unsigned int useavmute = 0;
module_param(useavmute, uint, 0644);
MODULE_PARM_DESC(useavmute, "perform HDMI AV Mute when blanking screen");

static int siihdmi_read_internal(struct siihdmi_tx *tx, u8 page, u8 offset)
{
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_INTERNAL_REG_SET_PAGE, page);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_INTERNAL_REG_SET_OFFSET, offset);
	return i2c_smbus_read_byte_data(tx->client, SIIHDMI_INTERNAL_REG_ACCESS);
}

static void siihdmi_write_internal(struct siihdmi_tx *tx, u8 page, u8 offset, u8 value)
{
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_INTERNAL_REG_SET_PAGE, page);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_INTERNAL_REG_SET_OFFSET, offset);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_INTERNAL_REG_ACCESS, value);
}


static int siihdmi_detect_revision(struct siihdmi_tx *tx)
{
	u8 data;
	unsigned long start;

	start = jiffies;
	do {
		data = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_DEVICE_ID);
	} while (data != SIIHDMI_DEVICE_ID_902x &&
		 !time_after(jiffies, start + bus_timeout));

	if (data != SIIHDMI_DEVICE_ID_902x)
		return -ENODEV;

	INFO("Device ID: %#02x", data);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_DEVICE_REVISION);
	if (data)
		CONTINUE(" (rev %01u.%01u)",
			 (data >> 4) & 0xf, (data >> 0) & 0xf);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_TPI_REVISION);
	CONTINUE(" (%s",
		 (data & SIIHDMI_VERSION_FLAG_VIRTUAL) ? "Virtual " : "");
	data &= ~SIIHDMI_VERSION_FLAG_VIRTUAL;
	data = data ? data : SIIHDMI_BASE_TPI_REVISION;
	CONTINUE("TPI revision %01u.%01u)",
		 (data >> 4) & 0xf, (data >> 0) & 0xf);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_HDCP_REVISION);
	if (data)
		CONTINUE(" (HDCP version %01u.%01u)",
			 (data >> 4) & 0xf, (data >> 0) & 0xf);

	CONTINUE("\n");

	return 0;
}

static inline int siihdmi_power_up(struct siihdmi_tx *tx)
{
	int ret;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_PWR_STATE,
					SIIHDMI_POWER_STATE_D0);
	if (ret < 0)
		ERROR("unable to power up transmitter\n");

	return ret;
}

static inline int siihdmi_power_down(struct siihdmi_tx *tx)
{
	int ret;
	u8 ctrl;

	memset((void *) &tx->sink.current_mode, 0, sizeof(struct fb_videomode));

	ctrl = SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	/* this seems redundant since D2 will wipe the sink state, but just
	 * in case we actually want to keep our display if D2 doesn't work
	 */
	if (tx->sink.type == SINK_TYPE_HDMI)
		ctrl |= SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL, ctrl);
	if (ret < 0) {
		ERROR("unable to power down transmitter\n");
		return ret;
	}

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_PWR_STATE,
					SIIHDMI_POWER_STATE_D2);
	if (ret < 0) {
		ERROR("unable to set transmitter into D2\n");
		return ret;
	}

	return 0;
}

static int siihdmi_initialise(struct siihdmi_tx *tx)
{
	int ret;

	/* step 1: reset and initialise */
	if (tx->platform->reset)
		tx->platform->reset();

	ret = i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_RQB, 0x00);
	if (ret < 0) {
		WARNING("unable to initialise device to TPI mode\n");
		return ret;
	}

	/* step 2: detect revision */
	if ((ret = siihdmi_detect_revision(tx)) < 0) {
		DEBUG("unable to detect device revision\n");
		return ret;
	}

	/* step 3: power up transmitter */
	if ((ret = siihdmi_power_up(tx)) < 0)
		return ret;

	/* step 4: configure input bus and pixel repetition */

	/* step 5: select YC input mode */

	/* step 6: configure sync methods */

	/* step 7: configure explicit sync DE generation */

	/* step 8: configure embedded sync extraction */

	/* step 9: setup interrupt service */
	if (tx->hotplug.enabled) {
		ret = i2c_smbus_write_byte_data(tx->client,
						SIIHDMI_TPI_REG_IER,
						SIIHDMI_IER_HOT_PLUG_EVENT |
						SIIHDMI_IER_RECEIVER_SENSE_EVENT);
		if (ret < 0)
			WARNING("unable to setup interrupt request\n");
	}

	return ret;
}

static inline void _process_cea861_vsdb(struct siihdmi_tx *tx,
					const struct hdmi_vsdb * const vsdb)
{
	unsigned int max_tmds;

	if (memcmp(vsdb->ieee_registration, CEA861_OUI_REGISTRATION_ID_HDMI_LSB,
		   sizeof(vsdb->ieee_registration)))
		return;

	max_tmds = KHZ2PICOS(vsdb->max_tmds_clock * 200);

	DEBUG("HDMI VSDB detected (basic audio %ssupported)\n",
	      tx->audio.available ? "" : "not ");

	if (!forcedvi) {
		tx->sink.type = SINK_TYPE_HDMI;
	} else {
		DEBUG("Sink type forced to DVI despite VSDB\n");
		tx->sink.type = SINK_TYPE_DVI;
	}

	INFO("HDMI port configuration: %u.%u.%u.%u\n",
	     vsdb->port_configuration_a, vsdb->port_configuration_b,
	     vsdb->port_configuration_c, vsdb->port_configuration_d);

	if (max_tmds && max_tmds < tx->platform->pixclock) {
		INFO("maximum TMDS clock limited to %u by device\n", max_tmds);
		tx->platform->pixclock = max_tmds;
	}
}

static inline void _process_cea861_video(struct siihdmi_tx *tx,
					 const struct cea861_video_data_block * const video)
{
	const struct cea861_data_block_header * const header =
		(struct cea861_data_block_header *) video;
	u8 i, count;

	for (i = 0, count = 0; i < header->length; i++) {
		const int vic = video->svd[i] & ~CEA861_SVD_NATIVE_FLAG;

		if (vic && vic <= ARRAY_SIZE(cea_modes)) {
			fb_add_videomode(&cea_modes[vic], &tx->info->modelist);
			count++;
		}
	}

	DEBUG("%u modes parsed from CEA video data block\n", count);
}

static inline void _process_cea861_extended(struct siihdmi_tx *tx,
					    const struct cea861_data_block_extended *ext)
{
	static const char * const scannings[] = {
		[SCAN_INFORMATION_UNKNOWN]      = "unknown",
		[SCAN_INFORMATION_OVERSCANNED]  = "overscanned",
		[SCAN_INFORMATION_UNDERSCANNED] = "underscanned",
		[SCAN_INFORMATION_RESERVED]     = "reserved",
	};

	switch (ext->extension_tag) {
	case CEA861_DATA_BLOCK_EXTENSION_VIDEO_CAPABILITY: {
		const struct cea861_video_capability_block * const vcb =
			(struct cea861_video_capability_block *) ext;
		INFO("CEA video capability (scanning behaviour):\n"
		     "    Preferred Mode: %s\n"
		     "    VESA/PC Mode: %s\n"
		     "    CEA/TV Mode: %s\n",
		     scannings[vcb->pt_overunder_behavior],
		     scannings[vcb->it_overunder_behavior],
		     scannings[vcb->ce_overunder_behavior]);
		}
		break;
	default:
		break;
	}
}

static void siihdmi_parse_cea861_timing_block(struct siihdmi_tx *tx,
					      const struct edid_extension *ext)
{
	const struct cea861_timing_block * const cea = (struct cea861_timing_block *) ext;
	const u8 size = cea->dtd_offset - offsetof(struct cea861_timing_block, data);
	u8 index;

	BUILD_BUG_ON(sizeof(*cea) != sizeof(*ext));

	tx->audio.available = cea->basic_audio_supported;
	if (cea->underscan_supported)
		tx->sink.scanning = SCANNING_UNDERSCANNED;

	if (cea->dtd_offset == CEA861_NO_DTDS_PRESENT)
		return;

	index = 0;
	while (index < size) {
		const struct cea861_data_block_header * const header =
			(struct cea861_data_block_header *) &cea->data[index];

		switch (header->tag) {
		case CEA861_DATA_BLOCK_TYPE_VENDOR_SPECIFIC:
			_process_cea861_vsdb(tx, (struct hdmi_vsdb *) header);
			break;
		case CEA861_DATA_BLOCK_TYPE_VIDEO:
			_process_cea861_video(tx, (struct cea861_video_data_block *) header);
			break;
		case CEA861_DATA_BLOCK_TYPE_EXTENDED:
			_process_cea861_extended(tx, (struct cea861_data_block_extended *) header);
			break;
		}

		index = index + header->length + sizeof(*header);
	}
}

static void siihdmi_set_vmode_registers(struct siihdmi_tx *tx,
					const struct fb_videomode *mode)
{
	enum basic_video_mode_fields {
		PIXEL_CLOCK,
		REFRESH_RATE,
		X_RESOLUTION,
		Y_RESOLUTION,
		FIELDS,
	};

	u16 vmode[FIELDS];
	u32 pixclk, htotal, vtotal, refresh;
	u8 format;
	int ret;

	BUILD_BUG_ON(sizeof(vmode) != 8);

	pixclk = mode->pixclock ? PICOS2KHZ(mode->pixclock) : 0;
	BUG_ON(pixclk == 0);

	htotal = mode->xres + mode->left_margin + mode->hsync_len + mode->right_margin;
	vtotal = mode->yres + mode->upper_margin + mode->vsync_len + mode->lower_margin;

	/* explicitly use 64-bit division to avoid overflow truncation */
	refresh = (u32) div_u64(pixclk * 1000ull, htotal * vtotal);

	/* basic video mode data */
	vmode[PIXEL_CLOCK]  = (u16) (pixclk / 10);
	/*
	  Silicon Image example code implies refresh to be 6000 for 60Hz?
	  This may work simply because we only test it on little-endian :(
	*/
	vmode[REFRESH_RATE] = (u16) refresh;
	vmode[X_RESOLUTION] = (u16) htotal;
	vmode[Y_RESOLUTION] = (u16) vtotal;

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_VIDEO_MODE_DATA_BASE,
					     sizeof(vmode),
					     (u8 *) vmode);
	if (ret < 0)
		DEBUG("unable to write video mode data\n");

	/* input format */
	format = SIIHDMI_INPUT_COLOR_SPACE_RGB
	       | SIIHDMI_INPUT_VIDEO_RANGE_EXPANSION_AUTO
	       | SIIHDMI_INPUT_COLOR_DEPTH_8BIT;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_AVI_INPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set input format\n");

	/* output format */
	format = SIIHDMI_OUTPUT_VIDEO_RANGE_COMPRESSION_AUTO
	       | SIIHDMI_OUTPUT_COLOR_STANDARD_BT601
	       | SIIHDMI_OUTPUT_COLOR_DEPTH_8BIT;

	if (tx->sink.type == SINK_TYPE_HDMI)
		format |= SIIHDMI_OUTPUT_FORMAT_HDMI_RGB;
	else
		format |= SIIHDMI_OUTPUT_FORMAT_DVI_RGB;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_AVI_OUTPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set output format\n");
}

static int siihdmi_clear_avi_info_frame(struct siihdmi_tx *tx)
{
	const u8 buffer[SIIHDMI_TPI_REG_AVI_INFO_FRAME_LENGTH] = {0};
	int ret;

	BUG_ON(tx->sink.type != SINK_TYPE_DVI);

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_AVI_INFO_FRAME_BASE,
					     sizeof(buffer), buffer);
	if (ret < 0)
		DEBUG("unable to clear avi info frame\n");

	return ret;
}

static int siihdmi_set_avi_info_frame(struct siihdmi_tx *tx, int vic)
{
	int ret;

	struct avi_info_frame avi = {
		.header = {
			.type    = INFO_FRAME_TYPE_AUXILIARY_VIDEO_INFO,
			.version = CEA861_AVI_INFO_FRAME_VERSION,
			.length  = sizeof(avi) - sizeof(avi.header),
		},

		.active_format_info_valid  = true,
		.active_format_description = ACTIVE_FORMAT_DESCRIPTION_UNSCALED,

		.picture_aspect_ratio      = PICTURE_ASPECT_RATIO_UNSCALED,

		.video_format              = vic,
	};

	BUG_ON(tx->sink.type != SINK_TYPE_HDMI);

	DEBUG("AVI InfoFrame sending Video Format %d\n", vic);

	switch (tx->sink.scanning) {
	case SCANNING_UNDERSCANNED:
		avi.scan_information = SCAN_INFORMATION_UNDERSCANNED;
		break;
	case SCANNING_OVERSCANNED:
		avi.scan_information = SCAN_INFORMATION_OVERSCANNED;
		break;
	default:
		avi.scan_information = SCAN_INFORMATION_UNKNOWN;
		break;
	}

	cea861_checksum_hdmi_info_frame((u8 *) &avi);

	BUILD_BUG_ON(sizeof(avi) - SIIHDMI_AVI_INFO_FRAME_OFFSET != SIIHDMI_TPI_REG_AVI_INFO_FRAME_LENGTH);
	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_AVI_INFO_FRAME_BASE,
					     sizeof(avi) - SIIHDMI_AVI_INFO_FRAME_OFFSET,
					     ((u8 *) &avi) + SIIHDMI_AVI_INFO_FRAME_OFFSET);
	if (ret < 0)
		DEBUG("unable to write avi info frame\n");

	return ret;
}

static int siihdmi_set_audio_info_frame(struct siihdmi_tx *tx)
{
	int ret;
	struct siihdmi_audio_info_frame packet = {
		.header = {
			.info_frame = SIIHDMI_INFO_FRAME_AUDIO,
			.repeat     = false,
			.enable     = tx->audio.available,
		},

		.info_frame = {
			.header = {
				.type    = INFO_FRAME_TYPE_AUDIO,
				.version = CEA861_AUDIO_INFO_FRAME_VERSION,
				.length  = sizeof(packet.info_frame) - sizeof(packet.info_frame.header),
			},

			.channel_count         = CHANNEL_COUNT_REFER_STREAM_HEADER,
			.channel_allocation    = CHANNEL_ALLOCATION_STEREO,

			.format_code_extension = CODING_TYPE_REFER_STREAM_HEADER,

			/* required to Refer to Stream Header by CEA861-D */
			.coding_type           = CODING_TYPE_REFER_STREAM_HEADER,

			.sample_size           = SAMPLE_SIZE_REFER_STREAM_HEADER,
			.sample_frequency      = FREQUENCY_REFER_STREAM_HEADER,
		},
	};

	BUG_ON(tx->sink.type != SINK_TYPE_HDMI);

	cea861_checksum_hdmi_info_frame((u8 *) &packet.info_frame);

	BUILD_BUG_ON(sizeof(packet) != SIIHDMI_TPI_REG_AUDIO_INFO_FRAME_LENGTH);
	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_MISC_INFO_FRAME_BASE,
					     sizeof(packet),
					     (u8 *) &packet);
	if (ret < 0)
		DEBUG("unable to write audio info frame\n");

	return ret;
}

static int siihdmi_set_spd_info_frame(struct siihdmi_tx *tx)
{
	int ret;
	struct siihdmi_spd_info_frame packet = {
		.header = {
			.info_frame = SIIHDMI_INFO_FRAME_SPD_ACP,
			.repeat     = false,
			.enable     = true,
		},

		.info_frame = {
			.header = {
				.type    = INFO_FRAME_TYPE_SOURCE_PRODUCT_DESCRIPTION,
				.version = CEA861_SPD_INFO_FRAME_VERSION,
				.length  = sizeof(packet.info_frame) - sizeof(packet.info_frame.header),
			},

			.source_device_info = SPD_SOURCE_PC_GENERAL,
		},
	};

	BUG_ON(tx->sink.type != SINK_TYPE_HDMI);

	strncpy(packet.info_frame.vendor, tx->platform->vendor,
		sizeof(packet.info_frame.vendor));

	strncpy(packet.info_frame.description, tx->platform->description,
		sizeof(packet.info_frame.description));

	cea861_checksum_hdmi_info_frame((u8 *) &packet.info_frame);

	BUILD_BUG_ON(sizeof(packet) != SIIHDMI_TPI_REG_MISC_INFO_FRAME_LENGTH);
	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_MISC_INFO_FRAME_BASE,
					     sizeof(packet),
					     (u8 *) &packet);
	if (ret < 0)
		DEBUG("unable to write SPD info frame\n");

	return ret;
}

static inline void siihdmi_audio_mute(struct siihdmi_tx *tx)
{
	u8 data;

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL);

	i2c_smbus_write_byte_data(tx->client,
				  SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL,
				  data | SIIHDMI_AUDIO_MUTE);
}

static inline void siihdmi_audio_unmute(struct siihdmi_tx *tx)
{
	u8 data;

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL);

	i2c_smbus_write_byte_data(tx->client,
				  SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL,
				  data & ~SIIHDMI_AUDIO_MUTE);
}

static inline void siihdmi_configure_audio(struct siihdmi_tx *tx)
{
	siihdmi_audio_mute(tx);

	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL, SIIHDMI_AUDIO_SPDIF_ENABLE | SIIHDMI_AUDIO_MUTE);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_I2S_AUDIO_SAMPLING_HBR, 0);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_I2S_ORIGINAL_FREQ_SAMPLE_LENGTH, SIIHDMI_AUDIO_HANDLING_DOWN_SAMPLE);

	siihdmi_audio_unmute(tx);
	siihdmi_set_audio_info_frame(tx);
}

static void siihdmi_print_modeline(const struct siihdmi_tx *tx,
				   const struct fb_videomode *mode,
				   const char * const message)
{
	const bool interlaced = (mode->vmode & FB_VMODE_INTERLACED);
	const bool double_scan = (mode->vmode & FB_VMODE_DOUBLE);

	u32 pixclk = mode->pixclock ? PICOS2KHZ(mode->pixclock) : 0;
	char flag = ' ';

	pixclk >>= (double_scan ? 1 : 0);

	if (fb_mode_is_equal(&tx->sink.preferred_mode, mode))
		flag = '*';

	if (mode->flag & FB_MODE_IS_CEA)
		flag = 'C';

	INFO("  %c \"%dx%d@%d%s\" %lu.%.2lu %u %u %u %u %u %u %u %u %chsync %cvsync",
	     /* CEA or preferred status of modeline */
	     flag,

	     /* mode name */
	     mode->xres, mode->yres,
	     mode->refresh << (interlaced ? 1 : 0),
	     interlaced ? "i" : (double_scan ? "d" : ""),

	     /* dot clock frequency (MHz) */
	     pixclk / 1000ul,
	     pixclk % 1000ul,

	     /* horizontal timings */
	     mode->xres,
	     mode->xres + mode->right_margin,
	     mode->xres + mode->right_margin + mode->hsync_len,
	     mode->xres + mode->right_margin + mode->hsync_len + mode->left_margin,

	     /* vertical timings */
	     mode->yres,
	     mode->yres + mode->lower_margin,
	     mode->yres + mode->lower_margin + mode->vsync_len,
	     mode->yres + mode->lower_margin + mode->vsync_len + mode->upper_margin,

	     /* sync direction */
	     (mode->sync & FB_SYNC_HOR_HIGH_ACT) ? '+' : '-',
	     (mode->sync & FB_SYNC_VERT_HIGH_ACT) ? '+' : '-');

	if (message)
		CONTINUE(" (%s)", message);

	CONTINUE("\n");
}

static int siihdmi_find_vic_from_modedb(const struct fb_videomode *mode)
{
	int vic;

	for (vic = 1; vic <= 64; vic++)
	{
		if (!memcmp((void *)&cea_modes[vic], (void *)mode, sizeof(struct fb_videomode)))
			return vic;
	}
	return 0;
}



static int siihdmi_set_resolution(struct siihdmi_tx *tx,
				  const struct fb_videomode *mode)
{
	u8 ctrl;
	int ret;

	if (0 == memcmp((void *) &tx->sink.current_mode, (void *) mode, sizeof(struct fb_videomode)))
	{
		return 0;
	}

	memset((void *) &tx->sink.current_mode, 0, sizeof(struct fb_videomode));

	INFO("selected configuration: \n");
	siihdmi_print_modeline(tx, mode, NULL);

	ctrl = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_SYS_CTRL);

	/* make sure we keep writing the sink type */
	if (tx->sink.type == SINK_TYPE_DVI)
		ctrl &= ~SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;
	else
		ctrl |= SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;

	/* step 0: (potentially) disable HDCP */

	/* step 1: (optionally) blank the display */
	/*
	 * Note that if we set the AV Mute, switching to DVI could result in a
	 * permanently muted display until a hardware reset.  Thus only do this
	 * if the sink is a HDMI connection
	 */
	if (tx->sink.type == SINK_TYPE_HDMI) {
		if (useavmute) {
			ctrl |= SIIHDMI_SYS_CTRL_AV_MUTE_HDMI;
				ret = i2c_smbus_write_byte_data(tx->client,
						SIIHDMI_TPI_REG_SYS_CTRL,
						ctrl);
			if (ret < 0)
				DEBUG("unable to AV Mute!\n");
		}
		msleep(SIIHDMI_CTRL_INFO_FRAME_DRAIN_TIME);
	}

	/* step 2: prepare for resolution change */
	ctrl |= SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to prepare for resolution change\n");

	/*
	 * step 3: change video resolution
	 * and wait for it to stabilize (50-500ms);
	 */
	msleep(SIIHDMI_RESOLUTION_STABILIZE_TIME);

	/* step 4: set the vmode registers */
	siihdmi_set_vmode_registers(tx, mode);

	/*
	 * step 5:
	 *      [DVI]  clear AVI InfoFrame
	 *      [HDMI] set AVI InfoFrame
	 */
	if (tx->sink.type == SINK_TYPE_HDMI) {
		int vic = siihdmi_find_vic_from_modedb(mode);
		siihdmi_set_avi_info_frame(tx, vic);
	} else {
		siihdmi_clear_avi_info_frame(tx);
	}

	/* step 6: [HDMI] set new audio information */
	if (tx->sink.type == SINK_TYPE_HDMI) {
		if (tx->audio.available)
			siihdmi_configure_audio(tx);
		siihdmi_set_spd_info_frame(tx);
	}

	/* step 7: enable display */
	ctrl &= ~SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to enable the display\n");

	/* step 8: (optionally) un-blank the display */
	if (tx->sink.type == SINK_TYPE_HDMI && useavmute) {
		ctrl &= ~SIIHDMI_SYS_CTRL_AV_MUTE_HDMI;
		ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
		if (ret < 0)
			DEBUG("unable to unmute the display\n");
	}

	/* step 9: (potentially) enable HDCP */

	memcpy((void *) &tx->sink.current_mode, mode, sizeof(struct fb_videomode));

	return ret;
}

static void siihdmi_dump_modelines(struct siihdmi_tx *tx)
{
	const struct fb_modelist *entry;
	const struct list_head * const modelines = &tx->info->modelist;

	INFO("supported modelines:\n");
	list_for_each_entry(entry, modelines, list)
		siihdmi_print_modeline(tx, &entry->mode, NULL);
}

static inline void siihdmi_process_extensions(struct siihdmi_tx *tx)
{
	const struct edid_block0 * const block0 =
		(struct edid_block0 *) tx->edid.data;
	const struct edid_extension * const extensions =
		(struct edid_extension *) (tx->edid.data + sizeof(*block0));
	u8 i;

	for (i = 0; i < block0->extensions; i++) {
		const struct edid_extension * const extension = &extensions[i];

		if (!edid_verify_checksum((u8 *) extension))
			WARNING("EDID block %u CRC mismatch\n", i);

		switch (extension->tag) {
		case EDID_EXTENSION_CEA:
			siihdmi_parse_cea861_timing_block(tx, extension);
			break;
		default:
			break;
		}
	}
}

static const struct fb_videomode *
_find_similar_mode(const struct fb_videomode * const mode, struct list_head *head)
{
	const struct fb_modelist *entry, *next;

	list_for_each_entry_safe(entry, next, head, list) {
		if (fb_res_is_equal(mode, &entry->mode) && (mode != &entry->mode))
			return &entry->mode;
	}

	return NULL;
}

static void siihdmi_sanitize_modelist(struct siihdmi_tx * const tx)
{
	struct list_head *modelist = &tx->info->modelist;
	const struct fb_modelist *entry, *next;
	const struct fb_videomode *mode;
	int num_removed = 0;

	if ((mode = fb_find_best_display(&tx->info->monspecs, modelist)))
		tx->sink.preferred_mode = *mode;


	list_for_each_entry_safe(entry, next, modelist, list) {
		const char *reason = NULL;
		mode = &entry->mode;

		if (mode->vmode & FB_VMODE_INTERLACED) {
			reason = "interlaced";
		} else if (mode->vmode & FB_VMODE_DOUBLE) {
			reason = "doublescan";
		} else if (mode->pixclock < tx->platform->pixclock) {
			reason = "pixel clock exceeded";
		} else if ((tx->sink.type == SINK_TYPE_HDMI) && mode->lower_margin < 2) {
			/*
			 * HDMI spec (§ 5.1.2) stipulates ≥2 lines of vsync
			 *
			 * We do not care so much on DVI, although it may be that the SII9022 cannot
			 * actually display this mode. Requires testing!!
			 */
			reason = "insufficient margin";
		} else {
			const struct fb_videomode *match = _find_similar_mode(mode, modelist);

			if (match) {
				/*
				 * Prefer detailed timings found in EDID.  Certain sinks support slight
				 * variations of VESA/CEA timings, and using those allows us to support
				 * a wider variety of monitors.
				 */
				if ((~(mode->flag) & FB_MODE_IS_DETAILED) &&
					(match->flag & FB_MODE_IS_DETAILED)) {
					reason = "detailed match present";
				} else if ((~(mode->flag) & FB_MODE_IS_CEA) &&
						   (match->flag & FB_MODE_IS_CEA)) {
					if ((tx->sink.type == SINK_TYPE_HDMI) && !useitmodes) {
						/*
						 * for HDMI connections we want to remove any detailed timings
						 * and leave in CEA mode timings. This is on the basis that you
						 * would expect HDMI monitors to do better with CEA (TV) modes
						 * than you would PC modes. No data is truly lost: these modes
						 * are duplicated in terms of size and refresh but may have
						 * subtle differences insofaras more compatible timings.
						 *
						 * That is, unless we want to prefer IT modes, since most TVs
						 * will overscan CEA modes (720p, 1080p) by default, but display
						 * IT (PC) modes to the edge of the screen.
						 */
						reason = "CEA match present";
					} else {
						/*
						 * DVI connections are the opposite to the above; remove CEA
						 * modes which duplicate normal modes, on the basis that a
						 * DVI sink will better display a standard EDID mode but may
						 * not be fully compatible with CEA timings. This is the
						 * behavior on HDMI sinks if we want to prefer IT modes.
						 *
						 * All we do is copy the matched mode into the mode value
						 * such that we remove the correct mode below.
						 */
						mode = match;
						reason = "IT match present";
					}
				}
			}
		}

		if (reason) {
			struct fb_modelist *modelist =
				container_of(mode, struct fb_modelist, mode);

			if (num_removed == 0) { // first time only
				INFO("Unsupported modelines:\n");
			}

			siihdmi_print_modeline(tx, mode, reason);

			list_del(&modelist->list);
			kfree(&modelist->list);
			num_removed++;
		}
	}

	if (num_removed > 0) {
		INFO("discarded %u incompatible modes\n", num_removed);
	}
}

static inline const struct fb_videomode *_match(const struct fb_videomode * const mode,
						struct list_head *modelist)
{
	const struct fb_videomode *match;

	if ((match = fb_find_best_mode_at_most(mode, modelist)))
		return match;

	return fb_find_nearest_mode(mode, modelist);
}

static const struct fb_videomode *siihdmi_select_video_mode(const struct siihdmi_tx * const tx)
{
	const struct fb_videomode *mode = NULL;

	/*
	 * match a mode against a specific CEA VIC - closest mode wins, we don't bother
	 * to check if the specified mode refresh, interlace or clocking is valid since
	 * it can only return similar but working modes from the sanitized modelist
	 * anyway
	 */

	if (modevic && modevic <= ARRAY_SIZE(cea_modes)) {
		mode = _match(&cea_modes[modevic], &tx->info->modelist);
		if (mode && (mode->xres == cea_modes[modevic].xres)
			 && (mode->yres == cea_modes[modevic].yres))
				return mode;
	}

	/*
	 * The whole point of these two options (siihdmi.seventwenty & siihdmi.teneighty)
	 * is that we want to find a mode which most displays will have optimized scalers for.

	 * Even if the native panel resolution is not precisely 1080p or 720p, most TVs will
	 * use high quality scaler for the standard HD display resolutions, but use more
	 * crude algorithms for other more odd modes.
	 *
	 * That said..
	 *
	 * We specifically handle plasma screens and cheap TVs where the preferred mode is
	 * so close to 1080p or 720p that we may as well try it by default as part of the
	 * search, therefore we get better native panel resolution and similar performance
	 * which should make people all the happier. There are a lot of HDMI monitors with
	 * 1680x1050 or 1440x900 panels and most plasma screens come in at 1360x768 or even
	 * 1024x768. The search is actually therefore "get as close to 1080p as my monitor
	 * will manage" or "get as close to 720p without being blurry on my 62in monster".
	 */

	if (teneighty) {
		if ((tx->sink.preferred_mode.xres == 1680 && tx->sink.preferred_mode.yres == 1050) ||
		    (tx->sink.preferred_mode.xres == 1440 && tx->sink.preferred_mode.yres == 900)) {
			mode = _match(&tx->sink.preferred_mode, &tx->info->modelist);
			if (mode && (mode->xres == tx->sink.preferred_mode.xres)
				 && (mode->yres == tx->sink.preferred_mode.yres))
				return mode;
		}

		mode = _match(&cea_modes[34], &tx->info->modelist);
		if (mode && (mode->xres == 1920) && (mode->yres == 1080))
			return mode;
	}

	if (seventwenty) {
		if ((tx->sink.preferred_mode.xres == 1360 ||
		     tx->sink.preferred_mode.xres == 1366 ||
		     tx->sink.preferred_mode.xres == 1024 ||
		     tx->sink.preferred_mode.xres == 1280) &&
		    (tx->sink.preferred_mode.yres == 768 ||
		     tx->sink.preferred_mode.yres == 800) ) {
			mode = _match(&tx->sink.preferred_mode, &tx->info->modelist);
			if (mode && (mode->xres == tx->sink.preferred_mode.xres)
				 && (mode->yres == tx->sink.preferred_mode.yres))
				return mode;
		}

		mode = _match(&cea_modes[4], &tx->info->modelist);
		if (mode && (mode->xres == 1280) && (mode->yres == 720))
			return mode;
	}

	/* If we disabled or couldn't find a reasonable mode above, just look for and use the
	 * closest to the monitor preferred mode - we don't care if it is not exact */
	if (tx->sink.preferred_mode.xres && tx->sink.preferred_mode.yres)
		if ((mode = _match(&tx->sink.preferred_mode, &tx->info->modelist)))
			return mode;

	/* if no matching mode was found, push 640x480@60 */
	INFO("unable to select a suitable video mode, using CEA Mode 1 (640x480@60)\n");
	return &cea_modes[1];
}

static inline int siihdmi_read_edid(struct siihdmi_tx *tx, u8 *edid, int size)
{
	u8 offset = 0;
	int ret;

	struct i2c_msg request[] = {
		{ .addr  = EDID_I2C_DDC_DATA_ADDRESS,
		  .len   = sizeof(offset),
		  .buf   = &offset, },
		{ .addr  = EDID_I2C_DDC_DATA_ADDRESS,
		  .flags = I2C_M_RD,
		  .len = size,
		  .buf = edid, },
	};

	ret = i2c_transfer(tx->client->adapter, request, ARRAY_SIZE(request));
	if (ret != ARRAY_SIZE(request))
		DEBUG("unable to read EDID block\n");
	return ret;
}

static int siihdmi_detect_monitor(struct siihdmi_tx *tx)
{
	u8 ctrl;
	int ret, length;
	unsigned long start;
	struct edid_block0 *block0;

	BUILD_BUG_ON(sizeof(struct edid_block0) != EDID_BLOCK_SIZE);
	BUILD_BUG_ON(sizeof(struct edid_extension) != EDID_BLOCK_SIZE);

	/* step 1: (potentially) disable HDCP */

	/* step 2: request the DDC bus */
	ctrl = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_SYS_CTRL);
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl | SIIHDMI_SYS_CTRL_DDC_BUS_REQUEST);
	if (ret < 0) {
		DEBUG("unable to request DDC bus\n");
		return ret;
	}

	/* step 3: poll for bus grant */
	start = jiffies;
	do {
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_SYS_CTRL);
	} while ((~ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED) &&
		 !time_after(jiffies, start + bus_timeout));

	if (~ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED)
		goto relinquish;

	/* step 4: take ownership of the DDC bus */
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					SIIHDMI_SYS_CTRL_DDC_BUS_REQUEST |
					SIIHDMI_SYS_CTRL_DDC_BUS_OWNER_HOST);
	if (ret < 0) {
		DEBUG("unable to take ownership of the DDC bus\n");
		goto relinquish;
	}

	/* step 5: read edid */
	if (tx->edid.length < EDID_BLOCK_SIZE) {
		if (tx->edid.data)
				kfree(tx->edid.data);

		tx->edid.data = kzalloc(EDID_BLOCK_SIZE, GFP_KERNEL);
		if (!tx->edid.data) {
			ret = -ENOMEM;
			goto relinquish;
		}
		tx->edid.length = EDID_BLOCK_SIZE;
	} else {
		memset(tx->edid.data, tx->edid.length, 0);
	}

	ret = siihdmi_read_edid(tx, tx->edid.data, EDID_BLOCK_SIZE);
	if (ret < 0) {
		kfree(tx->edid.data);
		tx->edid.length = 0;

		goto relinquish;
	}

	if (!edid_verify_checksum((u8 *) tx->edid.data))
		WARNING("EDID block 0 CRC mismatch\n");

	block0 = (struct edid_block0 *) tx->edid.data;

	/* need to allocate space for block 0 as well as the extensions */
	length = (block0->extensions + 1) * EDID_BLOCK_SIZE;

	if (length > tx->edid.length)
		kfree(tx->edid.data);

	tx->edid.data = kzalloc(length, GFP_KERNEL);
	if (!tx->edid.data) {
			ret = -ENOMEM;
			goto relinquish;
	}
	tx->edid.length = length;

	ret = siihdmi_read_edid(tx, tx->edid.data, tx->edid.length);
	if (ret < 0) {
		WARNING("failed to read extended EDID data\n");
		/* cleanup */
		kfree(tx->edid.data);
		tx->edid.data = NULL;
		tx->edid.length = 0;

		goto relinquish;
	}

	/* create monspecs from EDID for the basic stuff */
	fb_edid_to_monspecs(tx->edid.data, &tx->info->monspecs);
	fb_videomode_to_modelist(tx->info->monspecs.modedb,
			 tx->info->monspecs.modedb_len,
			 &tx->info->modelist);

	block0 = (struct edid_block0 *) tx->edid.data;
	if (block0->extensions)
		siihdmi_process_extensions(tx);

	siihdmi_sanitize_modelist(tx);
	siihdmi_dump_modelines(tx);

relinquish:
	/* step 6: relinquish ownership of the DDC bus while also setting
	 * the correct sink type (as per manual)
	 */
	start = jiffies;
	do {
		i2c_smbus_write_byte_data(tx->client,
					  SIIHDMI_TPI_REG_SYS_CTRL, 0x00);
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_SYS_CTRL);
	} while ((ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED) &&
		 !time_after(jiffies, start + bus_timeout));

	/* now, force the operational mode (HDMI or DVI) based on sink
	 * type and make it stick with a power up request (pg 27)
	 */
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_SYS_CTRL,
							(tx->sink.type == SINK_TYPE_HDMI) ?
								SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI : 0x00
							);

	ret = siihdmi_power_up(tx);

	/* step 7: (potentially) enable HDCP */

	return ret;
}

static int siihdmi_setup_display(struct siihdmi_tx *tx)
{
	const struct fb_videomode *mode;
	struct fb_var_screeninfo var = {0};

	int i, ret;
	u8 isr;

	/* defaults */
	tx->sink.scanning   = SCANNING_EXACT;
	tx->sink.type       = SINK_TYPE_DVI;
	tx->audio.available = false;

	isr = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);
	DEBUG("hotplug: display %s, powered %s\n",
	      (isr & SIIHDMI_ISR_DISPLAY_ATTACHED) ? "attached" : "detached",
	      (isr & SIIHDMI_ISR_RECEIVER_SENSE) ? "on" : "off");

	if (~isr & SIIHDMI_ISR_DISPLAY_ATTACHED)
		return siihdmi_power_down(tx);

	if (tx->info)
		sysfs_remove_link(&tx->info->dev->kobj, "phys-link");

	for (i = 0, tx->info = NULL; i < num_registered_fb; i++) {
		struct fb_info * const info = registered_fb[i];
		if (!strcmp(info->fix.id, tx->platform->framebuffer)) {
			tx->info = info;

			if (sysfs_create_link_nowarn(&tx->info->dev->kobj,
					      &tx->client->dev.kobj,
					      "phys-link") < 0)
				ERROR("failed to create device symlink");

			break;
		}
	}

	if (tx->info == NULL) {
		ERROR("unable to find video framebuffer\n");
		return -1;
	}

	/* use EDID to detect sink characteristics */
	ret = siihdmi_detect_monitor(tx);
	if (ret < 0)
		return ret;

	mode = siihdmi_select_video_mode(tx);
	if ((ret = siihdmi_set_resolution(tx, mode)) < 0)
		return ret;

	/* activate the framebuffer */
	fb_videomode_to_var(&var, mode);
	var.activate = FB_ACTIVATE_ALL;

	acquire_console_sem();
	tx->info->flags |= FBINFO_MISC_USEREVENT;
	fb_set_var(tx->info, &var);
	tx->info->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();

	return 0;
}

static int siihdmi_blank(struct siihdmi_tx *tx)
{
	u8 data;

	data = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_RQB);

	return i2c_smbus_write_byte_data(tx->client,
					 SIIHDMI_TPI_REG_RQB,
					 data | SIIHDMI_RQB_FORCE_VIDEO_BLANK);
}

static int siihdmi_unblank(struct siihdmi_tx *tx)
{
	u8 data;

	data = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_RQB);

	return i2c_smbus_write_byte_data(tx->client,
					 SIIHDMI_TPI_REG_RQB,
					 data & ~SIIHDMI_RQB_FORCE_VIDEO_BLANK);
}

static int siihdmi_fb_event_handler(struct notifier_block *nb,
				    unsigned long val,
				    void *v)
{
	const struct fb_event * const event = v;
	struct siihdmi_tx * const tx = container_of(nb, struct siihdmi_tx, nb);

	if (strcmp(event->info->fix.id, tx->platform->framebuffer))
		return 0;

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
	case FB_EVENT_FB_UNREGISTERED:
		return siihdmi_setup_display(tx);

	case FB_EVENT_MODE_CHANGE:
		if (event->info->mode)
			return siihdmi_set_resolution(tx, event->info->mode);
	case FB_EVENT_MODE_CHANGE_ALL:
		/* is handled above, removes a "unhandled event" warning in dmesg */
		break;
	case FB_EVENT_BLANK:
		switch (*((int *) event->data)) {
			case FB_BLANK_POWERDOWN:
				/* do NOT siihdmi_power_down() here */
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_NORMAL:
				return siihdmi_blank(tx);
			case FB_BLANK_UNBLANK:
				return siihdmi_unblank(tx);
		}
		break;
	default:
		DEBUG("unhandled fb event 0x%lx", val);
		break;
	}

	return 0;
}

static irqreturn_t siihdmi_hotplug_handler(int irq, void *dev_id)
{
	struct siihdmi_tx *tx = ((struct siihdmi_tx *) dev_id);

	schedule_work(&tx->hotplug.handler);

	return IRQ_HANDLED;
}

static void siihdmi_hotplug_event(struct work_struct *work)
{
	char *connected[]    = { "DISPLAY_CONNECTED=1", NULL };
	char *disconnected[] = { "DISPLAY_CONNECTED=0", NULL };
	char *power_on[]     = { "DISPLAY_POWERED_ON=1", NULL };
	char *power_off[]    = { "DISPLAY_POWERED_ON=0", NULL };

	struct siihdmi_tx *tx =
		container_of(work, struct siihdmi_tx, hotplug.handler);
	u8 isr;

	isr = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);
	if (~isr & SIIHDMI_ISR_HOT_PLUG_EVENT)
		goto complete;

	DEBUG("hotplug: display %s, powered %s\n",
	      (isr & SIIHDMI_ISR_DISPLAY_ATTACHED) ? "attached" : "detached",
	      (isr & SIIHDMI_ISR_RECEIVER_SENSE) ? "on" : "off");

	if (isr & SIIHDMI_ISR_HOT_PLUG_EVENT) {
		if (isr & SIIHDMI_ISR_DISPLAY_ATTACHED)
			kobject_uevent_env(&tx->client->dev.kobj, KOBJ_CHANGE,
					   connected);
		else
			kobject_uevent_env(&tx->client->dev.kobj, KOBJ_CHANGE,
					   disconnected);
	}

	if (isr & SIIHDMI_ISR_RECEIVER_SENSE_EVENT) {
		if (isr & SIIHDMI_ISR_RECEIVER_SENSE)
			kobject_uevent_env(&tx->client->dev.kobj, KOBJ_CHANGE,
					   power_on);
		else
			kobject_uevent_env(&tx->client->dev.kobj, KOBJ_CHANGE,
					   power_off);
	}

	if (isr & (SIIHDMI_ISR_DISPLAY_ATTACHED)) {
		siihdmi_initialise(tx);
		siihdmi_setup_display(tx);
	} else {
		siihdmi_power_down(tx);
	}

complete:
	/* clear the interrupt */
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_ISR, isr);
}

#if defined(CONFIG_SYSFS)
static ssize_t siihdmi_sysfs_read_edid(struct kobject *kobj,
				       struct bin_attribute *bin_attr,
				       char *buf, loff_t offset, size_t count)
{
	const struct siihdmi_tx * const tx =
		container_of(bin_attr, struct siihdmi_tx, edid.attributes);

	return memory_read_from_buffer(buf, count, &offset,
				       tx->edid.data, tx->edid.length);
}

static ssize_t siihdmi_sysfs_read_audio(struct kobject *kobj,
					struct bin_attribute *bin_attr,
					char *buf, loff_t off, size_t count)
{
	static const char * const sources[] = { "none\n", "hdmi\n" };
	const struct siihdmi_tx * const tx =
		container_of(bin_attr, struct siihdmi_tx, audio.attributes);

	return memory_read_from_buffer(buf, count, &off,
				       sources[tx->audio.available],
				       strlen(sources[tx->audio.available]));
}
#endif

static int __devinit siihdmi_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct siihdmi_tx *tx;
	int ret;

	tx = kzalloc(sizeof(struct siihdmi_tx), GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	tx->client = client;
	tx->platform = client->dev.platform_data;

	i2c_set_clientdata(client, tx);

	INIT_WORK(&tx->hotplug.handler, siihdmi_hotplug_event);

	BUG_ON(~tx->platform->hotplug.flags & IORESOURCE_IRQ);

	ret = request_irq(tx->platform->hotplug.start, siihdmi_hotplug_handler,
			  tx->platform->hotplug.flags & IRQF_TRIGGER_MASK,
			  tx->platform->hotplug.name, tx);
	if (ret < 0)
		WARNING("failed to setup hotplug interrupt: %d\n", ret);
	else
		tx->hotplug.enabled = true;

	/* initialise the device */
	if ((ret = siihdmi_initialise(tx)) < 0)
		goto error;

	ret = siihdmi_setup_display(tx);

#if defined(CONFIG_SYSFS)
	/* /sys/<>/edid */
	tx->edid.attributes.attr.name  = "edid";
	tx->edid.attributes.attr.owner = THIS_MODULE;
	tx->edid.attributes.attr.mode  = 0444;

	/* maximum size of EDID, not necessarily the size of our data */
	tx->edid.attributes.size       = SZ_32K;
	tx->edid.attributes.read       = siihdmi_sysfs_read_edid;

	if (sysfs_create_bin_file(&tx->client->dev.kobj, &tx->edid.attributes) < 0)
		WARNING("unable to construct attribute for sysfs exported EDID\n");

	/* /sys/<>/audio */
	tx->audio.attributes.attr.name  = "audio";
	tx->audio.attributes.attr.owner = THIS_MODULE;
	tx->audio.attributes.attr.mode  = 0444;

	/* we only want to return the value "hdmi" or "none" */
	tx->audio.attributes.size       = 5;
	tx->audio.attributes.read       = siihdmi_sysfs_read_audio;

	if (sysfs_create_bin_file(&tx->client->dev.kobj, &tx->audio.attributes) < 0)
		WARNING("unable to construct attribute for sysfs exported audio\n");
#endif

	/* register a notifier for future fb events */
	tx->nb.notifier_call = siihdmi_fb_event_handler;
	fb_register_client(&tx->nb);

	return 0;

error:
	i2c_set_clientdata(client, NULL);
	kfree(tx);
	return ret;
}

static int __devexit siihdmi_remove(struct i2c_client *client)
{
	struct siihdmi_tx *tx;

	tx = i2c_get_clientdata(client);
	if (tx) {
		if (tx->platform->hotplug.start)
			free_irq(tx->platform->hotplug.start, NULL);

#if defined(CONFIG_SYSFS)
		sysfs_remove_bin_file(&tx->client->dev.kobj, &tx->edid.attributes);
		sysfs_remove_bin_file(&tx->client->dev.kobj, &tx->audio.attributes);
#endif

		if (tx->edid.data)
			kfree(tx->edid.data);

		fb_unregister_client(&tx->nb);
		siihdmi_power_down(tx);
		i2c_set_clientdata(client, NULL);
		kfree(tx);
	}

	return 0;
}

static const struct i2c_device_id siihdmi_device_table[] = {
	{ "siihdmi", 0 },
	{ },
};

static struct i2c_driver siihdmi_driver = {
	.driver   = { .name = "siihdmi" },
	.probe    = siihdmi_probe,
	.remove   = siihdmi_remove,
	.id_table = siihdmi_device_table,
};

static int __init siihdmi_init(void)
{
	return i2c_add_driver(&siihdmi_driver);
}

static void __exit siihdmi_exit(void)
{
	i2c_del_driver(&siihdmi_driver);
}

/* Module Information */
MODULE_AUTHOR("Saleem Abdulrasool <compnerd@compnerd.org>");
MODULE_LICENSE("BSD-3");
MODULE_DESCRIPTION("Silicon Image SiI9xxx TMDS Driver");
MODULE_DEVICE_TABLE(i2c, siihdmi_device_table);

module_init(siihdmi_init);
module_exit(siihdmi_exit);

