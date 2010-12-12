/* vim: set noet ts=8 sts=8 sw=8 : */
/*
 * Copyright © 2010 Saleem Abdulrasool <compnerd@compnerd.org>.
 * Copyright © 2010 Genesi USA, Inc. <matt@genesi-usa.com>.
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
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#include <linux/edid.h>
#include <linux/cea861.h>
#include <linux/i2c/sii_hdmi.h>

#include <mach/hardware.h>

/* logging helpers */
#define DEBUG(fmt, ...)		printk(KERN_DEBUG "SIIHDMI: " fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...)		printk(KERN_ERR   "SIIHDMI: " fmt, ## __VA_ARGS__)
#define WARNING(fmt, ...)	printk(KERN_WARN  "SIIHDMI: " fmt, ## __VA_ARGS__)
#define INFO(fmt, ...)		printk(KERN_INFO  "SIIHDMI: " fmt, ## __VA_ARGS__)


/*
 * Interesting note:
 * CEA spec describes several 1080p "low" field rates at 24, 25 and 30 fields
 * per second (mode 32, 33, 34).  They all run at a 74.250MHz pixel clock just
 * like 720p@60. A decent HDMI TV should be able to display these as it
 * corresponds to "film mode". The question is, how do we detect these? They do
 * not seem to be in any EDID data we've ever seen...
 */

static struct fb_videomode siihdmi_default_video_mode = {
	.name         = "1280x720@60", // 720p CEA Mode 4
	.refresh      = 60,
	.xres         = 1280,
	.yres         = 720,
	.pixclock     = 13468,
	.left_margin  = 220,
	.right_margin = 110,
	.upper_margin = 20,
	.lower_margin = 5,
	.hsync_len    = 40,
	.vsync_len    = 5,
	.sync         = FB_SYNC_BROADCAST     |
	                FB_SYNC_HOR_HIGH_ACT  |
	                FB_SYNC_VERT_HIGH_ACT |
	                FB_SYNC_EXT,
	.vmode        = FB_VMODE_NONINTERLACED,
};


static int siihdmi_detect_revision(struct siihdmi_tx *tx)
{
	u8 data;
	char device[80];
	size_t offset = 0;
	unsigned long timeout = msecs_to_jiffies(50), start, finish;

	start = jiffies;
	do {
		data = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_DEVICE_ID);
	} while (data != SIIHDMI_DEVICE_ID_902x &&
		 !time_after(jiffies, start+timeout) );
	finish = jiffies;

	INFO("detect_revision: took %u ms to read device id\n", jiffies_to_msecs(finish-start));

	if (data != SIIHDMI_DEVICE_ID_902x)
		return -ENODEV;

	offset += snprintf(device + offset, sizeof(device) - offset,
			   "Device ID: %#02x", data);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_DEVICE_REVISION);
	if (data)
		offset += snprintf(device + offset, sizeof(device) - offset,
				   " (rev %01u.%01u)",
				   (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_TPI_REVISION);
	offset += snprintf(device + offset, sizeof(device) - offset,
			   " (%s", data & (1 << 7) ? "Virtual" : "");
	data &= ~(1 << 7);
	data = data ? data : SIIHDMI_BASE_TPI_REVISION;
	offset += snprintf(device + offset, sizeof(device) - offset,
			   "TPI revision %01u.%01u)",
			   (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_HDCP_REVISION);
	if (data)
		offset += snprintf(device + offset, sizeof(device) - offset,
				   " (HDCP version %01u.%01u)",
				   (data >> 4) & 15, data & 15);

	device[offset] = '\0';
	INFO("%s\n", device);

	return 0;
}

static int siihdmi_initialise(struct siihdmi_tx *tx)
{
	int ret;

	/* step 1: reset and initialise */
	ret = i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_RQB, 0x00);
	if (ret < 0) {
		DEBUG("unable to initialise device to TPI mode\n");
		return ret;
	}

	/* step 2: detect revision */
	if ((ret = siihdmi_detect_revision(tx)) < 0) {
		DEBUG("unable to detect device revision\n");
		return ret;
	}

	/* step 3: power up transmitter */
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_PWR_STATE,
					SIIHDMI_POWER_STATE_D0);
	if (ret < 0) {
		DEBUG("unable to power up transmitter\n");
		return ret;
	}

	/* step 4: configure input bus and pixel repetition */

	/* step 5: select YC input mode */

	/* step 6: configure sync methods */

	/* step 7: configure explicit sync DE generation */

	/* step 8: configure embedded sync extraction */

	/* step 9: setup interrupt service */
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_IER,
					SIIHDMI_IER_HOT_PLUG_EVENT |
					SIIHDMI_IER_RECEIVER_SENSE_EVENT);
	if (ret < 0) {
		DEBUG("unable to setup interrupt request\n");
		return ret;
	}

	return 0;
}

static int siihdmi_read_edid(struct siihdmi_tx *tx, u8 *edid, size_t size)
{
	u8 offset, ctrl;
	int ret;
	unsigned long timeout = msecs_to_jiffies(50), start, finish;

	struct i2c_msg request[] = {
		{ .addr  = EDID_I2C_DDC_DATA_ADDRESS,
		  .len   = sizeof(offset),
		  .buf   = &offset, },
		{ .addr  = EDID_I2C_DDC_DATA_ADDRESS,
		  .flags = I2C_M_RD,
		  .len   = size,
		  .buf   = edid, },
	};

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
	} while (! (ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED) &&
		 ! time_after(jiffies, start+timeout) );
	finish = jiffies;

	INFO("read_edid: took %u ms to poll for bus grant\n", jiffies_to_msecs(finish-start));

	/* step 4: take ownership of the DDC bus */
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					SIIHDMI_SYS_CTRL_DDC_BUS_REQUEST |
					SIIHDMI_SYS_CTRL_DDC_BUS_OWNER_HOST);
	if (ret < 0) {
		DEBUG("unable to take ownership of the DDC bus\n");
		return ret;
	}

	/* step 5: read edid */
	offset = 0;
	ret = i2c_transfer(tx->client->adapter, request, ARRAY_SIZE(request));
	if (ret != ARRAY_SIZE(request))
		DEBUG("unable to read EDID block\n");

	/* step 6: relinquish ownership of the DDC bus */
	start = jiffies;
	do {
		i2c_smbus_write_byte_data(tx->client,
					  SIIHDMI_TPI_REG_SYS_CTRL,
					  0x00);
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_SYS_CTRL);
	} while ((ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED) &&
		 ! time_after(jiffies, start+timeout) );
	finish = jiffies;

	INFO("read_edid: took %u ms to relinquish bus grant\n", jiffies_to_msecs(finish-start));

	/* step 7: (potentially) enable HDCP */

	return 0;
}

#if 0
static void siihdmi_parse_audio(struct siihdmi_tx *tx, struct cea_dbc_audio *audio)
{
}

static void siihdmi_parse_video(struct siihdmi_tx *tx, struct cea_dbc_video *video)
{
}

static void siihdmi_parse_speaker(struct siihdmi_tx *tx, struct cea_dbc_speaker *speaker)
{
}

static void siihdmi_parse_vendor(struct siihdmi_tx *tx, struct cea_dbc_vendor *vendor)
{
}
#endif

static void siihdmi_detect_sink(struct siihdmi_tx *tx, u8 *edid, size_t size)
{
	/*
	 * Sink detection is a fairly simple matter.  Assume that we are
	 * connected to a DVI sink (video only, no audio support).  Check the
	 * EDID for the presence of the CEA extension (which is guaranteed to be
	 * the first extension blob, if present).  If it is present, and the CEA
	 * timing block data reports support for audio, then the sink is HDMI.
	 */

	struct cea_timing_block *ctb;

	tx->connection_type = CONNECTION_TYPE_DVI;
	tx->pixel_mapping = PIXEL_MAPPING_EXACT;
	tx->enable_audio = false;

	if (edid[EEDID_EXTENSION_FLAG]) {
		switch (edid[EEDID_EXTENSION_TAG]) {
		case EDID_EXTENSION_CEA:
			ctb = (struct cea_timing_block *) &edid[EEDID_EXTENSION_DATA_OFFSET];

			if (ctb->basic_audio_supported) {
				tx->connection_type = CONNECTION_TYPE_HDMI; // EEEEE. this should be in the vendor block below
				tx->enable_audio = true;
			}

			if (ctb->underscan_supported)
				tx->pixel_mapping = PIXEL_MAPPING_UNDERSCANNED;
#if 0
			if (ctb->dtd_start_offset != 0x4) {
				/* okay DTD data is off in the wild reaches so the next blocks
				 * will be audio, video, vendor and speaker configuration
				 */
				u8 length, offset = 0x5;
				struct cea_dbc_header *dbc_header = (struct cea_dbc_header *) &ctb->dbc_start_offset;

				while (offset <= 127) {
					switch(dbc_header->block_type_tag) {
					case CEA_DATA_BLOCK_TAG_AUDIO:
						length = dbc_header->length;
						siihdmi_parse_audio(tx, (struct cea_dbc_audio *) &edid[EEDID_EXTENSION_DATA_OFFSET + offset]);
						offset += length;
						break;
					case CEA_DATA_BLOCK_TAG_VIDEO:
						length = dbc_header->length;
						siihdmi_parse_video(tx, (struct cea_dbc_video *) &edid[EEDID_EXTENSION_DATA_OFFSET + offset]);
						offset += length;
						break;
					case CEA_DATA_BLOCK_TAG_SPEAKER:
						length = dbc_header->length;
						siihdmi_parse_speaker(tx, (struct cea_dbc_speaker *) &edid[EEDID_EXTENSION_DATA_OFFSET + offset]);
						offset += length;
						break;
					case CEA_DATA_BLOCK_TAG_VENDOR:
						length = dbc_header->length;
						siihdmi_parse_vendor(tx, (struct cea_dbc_vendor *) &edid[EEDID_EXTENSION_DATA_OFFSET + offset]);
						offset += length;
						break;
					}
				}
			}
#endif
		default:
			break;
		}
	}
}

static inline unsigned long siihdmi_ps_to_hz(const unsigned long ps)
{
	unsigned long long numerator = 1000000000000ull;

	if (ps == 0)
		return 0;

	/* freq = 1 / time; 10^12 ps = 1s; 10^12/ps = freq */
	do_div(numerator, ps);

	return (unsigned long) numerator;
}

static void siihdmi_set_vmode_registers(struct siihdmi_tx *tx,
					struct fb_var_screeninfo *var)
{
	enum basic_video_mode_fields {
		PIXEL_CLOCK,
		REFRESH_RATE,
		X_RESOLUTION,
		Y_RESOLUTION,
		FIELDS,
	};

	u16 vmode[FIELDS];
	u16 pixclk, htotal, vtotal, refresh;
	u8 format;
	int ret;

	BUILD_BUG_ON(sizeof(vmode) != 8);

	pixclk = siihdmi_ps_to_hz(var->pixclock);
	htotal = var->xres + var->left_margin + var->hsync_len + var->right_margin;
	vtotal = var->yres + var->upper_margin + var->vsync_len + var->lower_margin;
	refresh = (htotal * vtotal) / pixclk;

	/* basic video mode data */
	vmode[PIXEL_CLOCK]  = pixclk / 10000;
	vmode[REFRESH_RATE] = refresh;
	vmode[X_RESOLUTION] = htotal;
	vmode[Y_RESOLUTION] = vtotal;

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_VIDEO_MODE_DATA_BASE,
					     sizeof(vmode),
					     (u8 *) vmode);
	if (ret < 0)
		DEBUG("unable to write video mode data\n");

	/* input format */
	format = SIIHDMI_INPUT_COLOR_SPACE_RGB            |
		 SIIHDMI_INPUT_VIDEO_RANGE_EXPANSION_AUTO |
		 SIIHDMI_INPUT_COLOR_DEPTH_8BIT;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_AVI_INPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set input format\n");

	/* output format */
	format = SIIHDMI_OUTPUT_VIDEO_RANGE_COMPRESSION_AUTO |
		 SIIHDMI_OUTPUT_COLOR_STANDARD_BT601         |
		 SIIHDMI_OUTPUT_COLOR_DEPTH_8BIT;

	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		format |= SIIHDMI_OUTPUT_FORMAT_HDMI_RGB;
	else
		format |= SIIHDMI_OUTPUT_FORMAT_DVI_RGB;

	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_AVI_OUTPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set output format\n");
}

static void siihdmi_checksum_info_frame(u8 * const info_frame)
{
	u8 crc;
	int i;
	struct info_frame_header *header = (struct info_frame_header *) info_frame;

	u8 type = header->infoframe_type;
	u8 version = header->infoframe_version;
	u8 length = header->infoframe_length;

	crc = (HDMI_PACKET_TYPE_INFO_FRAME + type) + version + (length - 1);
	for (i = 1; i < length; i++)
		crc += info_frame[i];

	header->chksum = HDMI_PACKET_CHECKSUM - crc;
}

static int siihdmi_set_avi_info_frame(struct siihdmi_tx *tx,
				       struct fb_var_screeninfo *var)
{
	int ret;
	struct avi_info_frame *avi = kzalloc(sizeof(struct avi_info_frame), GFP_KERNEL);
	struct info_frame_header *header = (struct info_frame_header *) &avi->header;

	//BUILD_BUG_ON(sizeof(struct avi_info_frame) != 14);

	if (!avi)
		return -ENOMEM;

	/* TODO improve AVI InfoFrame we send */

	header->infoframe_type = INFO_FRAME_TYPE_AUXILIARY_VIDEO_INFO;
	header->infoframe_version = CEA861_AVI_INFO_FRAME_VERSION;
	header->infoframe_length = 12;

	switch (tx->pixel_mapping) {
	case PIXEL_MAPPING_UNDERSCANNED:
		avi->scan_information = SCAN_INFORMATION_UNDERSCANNED;
		break;
	case PIXEL_MAPPING_OVERSCANNED:
		avi->scan_information = SCAN_INFORMATION_OVERSCANNED;
		break;
	default:
		avi->scan_information = SCAN_INFORMATION_UNKNOWN;
		break;
	}

	avi->active_format_info_valid = true;
	avi->active_format_description = ACTIVE_FORMAT_DESCRIPTION_UNSCALED;

	avi->picture_aspect_ratio = PICTURE_ASPECT_RATIO_UNSCALED;

	avi->video_format = VIDEO_FORMAT_UNKNOWN;

	siihdmi_checksum_info_frame((u8 *) avi);

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_AVI_INFO_FRAME_BASE,
					     sizeof(struct avi_info_frame),
					     ((u8 *) avi) + SIIHDMI_AVI_INFO_FRAME_OFFSET);
	if (ret < 0)
		DEBUG("unable to write avi info frame id\n");

	kfree(avi);

	return ret;
}

static int siihdmi_set_spd_info_frame(struct siihdmi_tx *tx,
				       struct fb_var_screeninfo *var)
{
	int ret;
	struct siihdmi_spd_info_frame *packet = kzalloc(sizeof(struct siihdmi_spd_info_frame), GFP_KERNEL);
	struct spd_info_frame *spd = (struct spd_info_frame *) &packet->spd;
	struct info_frame_header *header = (struct info_frame_header *) &spd->header;

	//BUILD_BUG_ON(sizeof(struct siihdmi_spd_info_frame) != SIIHDMI_INFO_FRAME_BUFFER_LENGTH);

	/* this should be in platform data */
	#define SPD_VENDOR "Genesi"
	#define SPD_DEVICE "Efika MX"

	if (!packet)
		return -ENOMEM;

	header->infoframe_type = INFO_FRAME_TYPE_SOURCE_PRODUCT_DESCRIPTION;
	header->infoframe_version = CEA861_SPD_INFO_FRAME_VERSION;
	header->infoframe_length = 25;

	packet->config.buffer_type = INFO_FRAME_BUFFER_SPD;
	packet->config.enable = 1;
	packet->config.repeat = 0;

	/* yuuuck */
	memcpy(&spd->vendor[0], SPD_VENDOR, 6);
	memcpy(&spd->description[0], SPD_DEVICE, 8);
	spd->source_device_info = SPD_SOURCE_PC_GENERAL;

	siihdmi_checksum_info_frame((u8 *) spd);

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_MISC_INFO_FRAME_BASE,
					     sizeof(packet),
					     (u8 *) &packet);
	if (ret < 0)
		DEBUG("unable to write spd info frame id\n");

	kfree(packet);

	return ret;
}

static int siihdmi_set_audio_info_frame(struct siihdmi_tx *tx,
				       struct fb_var_screeninfo *var)
{
	int ret;

	struct siihdmi_audio_info_frame *packet = kzalloc(sizeof(struct siihdmi_audio_info_frame), GFP_KERNEL);
	struct audio_info_frame *audio = (struct audio_info_frame *) &packet->audio;
	struct info_frame_header *header = (struct info_frame_header *) &audio->header;

	//BUILD_BUG_ON(sizeof(siihdmi_audio_info_frame) != SIIHDMI_AUDIO_INFO_FRAME_BUFFER_LENGTH);

	if (!packet)
		return -ENOMEM;

	if (tx->enable_audio)
	{
		packet->config.buffer_type = INFO_FRAME_BUFFER_AUDIO;
		packet->config.enable = 1;
		packet->config.repeat = 0;

		header->infoframe_type = INFO_FRAME_TYPE_AUDIO;
		header->infoframe_version = CEA861_AUDIO_INFO_FRAME_VERSION;
		header->infoframe_length = 10;

		/* lovely autodetecty defaults! */
		audio->coding_type = CODING_TYPE_REFER_STREAM_HEADER;
		audio->channel_count = CHANNEL_COUNT_REFER_STREAM_HEADER; /* (channels - 1) */
		audio->sample_frequency = FREQUENCY_REFER_STREAM_HEADER;
		audio->sample_size =  SAMPLE_SIZE_REFER_STREAM_HEADER;
		audio->channel_allocation = CHANNEL_ALLOCATION_STEREO; /* complex :( */
		audio->extension = CODING_TYPE_REFER_STREAM_HEADER; /* actually 0 = disabled */

		siihdmi_checksum_info_frame((u8 *) audio);

		ret = i2c_smbus_write_i2c_block_data(tx->client,
						     SIIHDMI_TPI_REG_MISC_INFO_FRAME_BASE,
						     sizeof(packet),
						     (u8 *) &packet);
	}

	if (ret < 0)
		DEBUG("unable to write audio info frame id\n");

	kfree(packet);
	return ret;
}

static int siihdmi_set_resolution(struct siihdmi_tx *tx,
				  struct fb_var_screeninfo *var)
{
	u8 ctrl;
	int ret;

	INFO("Setting Resolution: %dx%d\n", var->xres, var->yres);

	ctrl = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_SYS_CTRL);

	/* setup the sink type */
	if (tx->connection_type == CONNECTION_TYPE_DVI)
		ctrl &= ~SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;
	else
		ctrl |= SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;

	/* step 1: (potentially) disable HDCP */

	/* step 2: (optionally) blank the display */
	/*
	 * Note that if we set the AV Mute, switching to DVI could result in a
	 * permanently muted display until a hardware reset.  Thus only do this
	 * if the sink is a HDMI connection
	 */
	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		ctrl |= SIIHDMI_SYS_CTRL_AV_MUTE_HDMI;
	/* optimisation: merge the write into the next one */

	/* step 3: prepare for resolution change */
	ctrl |= SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to prepare for resolution change\n");

	tx->tmds_state = 0;

	msleep(SIIHDMI_CTRL_INFO_FRAME_DRAIN_TIME);

	/* step 4: change video resolution */

	/* step 5: set the vmode registers */
	siihdmi_set_vmode_registers(tx, var);

	/* step 6: [HDMI] set AVI InfoFrame */
	/* NOTE this is required as it flushes the vmode registers */
	if (tx->connection_type == CONNECTION_TYPE_HDMI)
	{
		siihdmi_set_avi_info_frame(tx, var);

		/* step 7: [HDMI] set new audio information */
		siihdmi_set_audio_info_frame(tx, var);
		siihdmi_set_spd_info_frame(tx, var);
	}
	else if (tx->connection_type == CONNECTION_TYPE_DVI) // save time and effort
	{
		u8 enable_dvi = 0;
		ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_AVI_INFO_END_RIGHT_BAR_MSB,
					enable_dvi);
		if (ret < 0)
			DEBUG("unable to enable DVI\n");
	}

	/* step 8: enable display */
	ctrl &= ~SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;

	/* optimisation: merge the write into the next one */

	/* step 9: (optionally) un-blank the display */
	ctrl &= ~SIIHDMI_SYS_CTRL_AV_MUTE_HDMI;
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
	tx->tmds_state = 1;

	if (ret < 0)
		DEBUG("unable to enable the display\n");


	/* step 10: (potentially) enable HDCP */

	return ret;
}

static bool siihdmi_sink_present(struct siihdmi_tx *tx)
{
	u8 isr;

	isr = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);

	return (isr & (SIIHDMI_ISR_HOT_PLUG_EVENT |
		       SIIHDMI_ISR_RECEIVER_SENSE_EVENT));
}

static void siihdmi_dump_modelines(const struct fb_monspecs * const monspecs)
{
	u32 i;

	INFO("%u Supported Modelines:\n", monspecs->modedb_len);
	for (i = 0; i < monspecs->modedb_len; i++) {
		const struct fb_videomode * const mode = &monspecs->modedb[i];
		const bool interlaced = (mode->vmode & FB_VMODE_INTERLACED);
		const bool double_scan = (mode->vmode & FB_VMODE_DOUBLE);
		u32 pixel_clock = siihdmi_ps_to_hz(mode->pixclock);

		pixel_clock >>= (double_scan ? 1 : 0);

		INFO("    \"%dx%d@%d%s\" %lu.%.2lu %u %u %u %u %u %u %u %u %chsync %cvsync\n",
		     /* mode name */
		     mode->xres, mode->yres,
		     mode->refresh << (interlaced ? 1 : 0),
		     interlaced ? "i" : (double_scan ? "d" : ""),

		     /* dot clock frequency (MHz) */
		     pixel_clock / 1000000ul,
		     pixel_clock % 1000000ul,

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
	}
}

static int siihdmi_init_fb(struct siihdmi_tx *tx, struct fb_info *fb)
{
	u8 edid[EEDID_BASE_LENGTH];
	const struct fb_videomode *mode = NULL;
	struct fb_var_screeninfo var = {0};
	int ret;

	/* TODO use platform_data to prune modelist */

	if ((ret = siihdmi_read_edid(tx, edid, sizeof(edid))) < 0)
		return ret;

	siihdmi_detect_sink(tx, edid, sizeof(edid));

	fb_edid_to_monspecs(edid, &fb->monspecs);
	siihdmi_dump_modelines(&fb->monspecs);
	/* TODO mxcfb_videomode_to_modelist did some additional work */
	fb_videomode_to_modelist(fb->monspecs.modedb,
				 fb->monspecs.modedb_len,
				 &fb->modelist);

	/* TODO mxcfb_update_default_var did some additional work */
	mode = fb_find_nearest_mode(&siihdmi_default_video_mode, &fb->modelist);
	mode = mode ? mode : &siihdmi_default_video_mode;

	/* TODO mxcfb_adjust did some additional work */
	fb_videomode_to_var(&var, mode);
	msleep(10); // pause to let IPU settle
	if ((ret = siihdmi_set_resolution(tx, &var)) < 0)
		return ret;

	/* activate the framebuffer */
	var.activate = FB_ACTIVATE_ALL;

	acquire_console_sem();
	fb->flags |= FBINFO_MISC_USEREVENT;
	fb_set_var(fb, &var);
	fb->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();

	return 0;
}

static int siihdmi_blank(struct siihdmi_tx *tx, struct fb_var_screeninfo *var, int powerdown)
{
	u8 data;

	/* TODO Power down TMDS if the flag is set */

	data = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_RQB);

	return i2c_smbus_write_byte_data(tx->client,
					 SIIHDMI_TPI_REG_RQB,
					 data | SIIHDMI_RQB_FORCE_VIDEO_BLANK);
}

static int siihdmi_unblank(struct siihdmi_tx *tx, struct fb_var_screeninfo *var)
{
	u8 data;

	/* TODO Power up TMDS if the tx->tmds_state is down */

	data = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_RQB);

	return i2c_smbus_write_byte_data(tx->client,
					 SIIHDMI_TPI_REG_RQB,
					 data & ~SIIHDMI_RQB_FORCE_VIDEO_BLANK);
}

static int siihdmi_fb_event_handler(struct notifier_block *nb,
				    unsigned long val,
				    void *v)
{
	struct fb_event *event = v;
	struct siihdmi_tx *tx = container_of(nb, struct siihdmi_tx, nb);

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
	{
		/*
		 * sleep just a little while to let the IPU settle
		 * before we force it to change again to an EDID mode
		 */
		msleep(100);
		return siihdmi_init_fb(tx, event->info);
	}
	case FB_EVENT_MODE_CHANGE:
	{
		struct fb_var_screeninfo var = {0};

		fb_videomode_to_var(&var, event->info->mode);
		msleep(100);
		return siihdmi_set_resolution(tx, &var);
	}
	break;
	case FB_EVENT_BLANK:
	{
		struct fb_var_screeninfo var = {0};
		int event_type = *((int *)event->data);
		fb_videomode_to_var(&var, event->info->mode);

		switch (event_type) {
		case FB_BLANK_POWERDOWN:
			return siihdmi_blank(tx, &var, 1);
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			return siihdmi_blank(tx, &var, 0);
		case FB_BLANK_UNBLANK:
			return siihdmi_unblank(tx, &var);
		}
	}
	break;
	default:
		DEBUG("unhandled fb event 0x%lx", val);
		break;
	}

	return 0;
}

#ifdef CONFIG_SIIHDMI_HOTPLUG
static irqreturn_t siihdmi_irq_handler(int irq, void *dev_id)
{
	struct siihdmi_tx *tx = ((struct siihdmi_tx *) dev_id);

	DEBUG("hotplug event irq handled, scheduling hotplug work\n");
	schedule_delayed_work(&tx->hotplug, msecs_to_jiffies(50));
	return IRQ_HANDLED;
}

static void siihdmi_hotplug_event(struct work_struct *work)
{
	struct siihdmi_tx *tx = container_of(work, struct siihdmi_tx, hotplug.work);
	u8 data;

	DEBUG("hotplug event work called\n");

	data = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);

	if (data & SIIHDMI_IER_HOT_PLUG_EVENT)
		DEBUG("interrupt hot plug event\n");

	if (data & SIIHDMI_IER_RECEIVER_SENSE_EVENT)
		DEBUG("interrupt receiver sense event or CTRL bus error\n");

	if (data & SIIHDMI_IER_CTRL_BUS_EVENT)
		DEBUG("interrupt CTRL bus event pending or hotplug state high\n");

	if (data & SIIHDMI_IER_CPI_EVENT)
		DEBUG("interrupt CPI event or RxSense state high\n");

	if (data & SIIHDMI_IER_AUDIO_EVENT)
		DEBUG("interrupt audio event\n");

	if (data & SIIHDMI_IER_SECURITY_STATUS_CHANGE)
		DEBUG("interrupt security status change\n");

	if (data & SIIHDMI_IER_HDCP_VALUE_READY)
		DEBUG("interrupt HDCP value ready\n");

	if (data & SIIHDMI_IER_HDCP_AUTHENTICATION_STATUS_CHANGE)
		DEBUG("interrupt HDCP authentication status change\n");

	// write 1 to every fired event, to clear it
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_ISR, data);
}
#endif

static int __devinit siihdmi_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mxc_lcd_platform_data *plat = client->dev.platform_data;
	struct siihdmi_tx *tx;
	int i, ret;

	tx = kzalloc(sizeof(struct siihdmi_tx), GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	tx->client = client;

	i2c_set_clientdata(client, tx);

#ifdef CONFIG_SIIHDMI_HOTPLUG
	tx->irq = plat->hotplug_irq;
	PREPARE_DELAYED_WORK(&tx->hotplug, siihdmi_hotplug_event);
	ret = request_irq(tx->irq, siihdmi_irq_handler, IRQF_TRIGGER_RISING, "hdmi_hotplug", (void *) tx);
	if (ret) {
		DEBUG("could not register display hotplug irq\n");
	}
#endif
	msleep(100); // let things settle, for some reason this improves compatibility

	/* initialise the device */
	if ((ret = siihdmi_initialise(tx)) < 0)
		goto error;

	if (siihdmi_sink_present(tx)) {
		/* enable any existing framebuffers */
		DEBUG("%d registered framebuffers\n", num_registered_fb);
		for (i = 0; i < num_registered_fb; i++)
		{
			/* hack: only init the background framebuffer */
			if (!strncmp("DISP3 BG", registered_fb[i]->fix.id, 8))
				if ((ret = siihdmi_init_fb(tx, registered_fb[i])) < 0)
					goto error;
		}
	} else {
		/*
		 * A sink is not currently present.  However, the device has
		 * been initialised.  This includes setting up the crucial IER.
		 * As a result, we can power down the transmitter and be
		 * signalled when the sink state changes.
		 */

		/* TODO In theory, we should be able to switch to D3 since we
		 * have RSEN and Hot Plug event bits set in the IER.  Because
		 * there is no sink present, we need not access the registers
		 * on the tx.  When the IRQ is triggered, we need will assert
		 * the RESET# pin to return from D3.
		 */

		ret = i2c_smbus_write_byte_data(tx->client,
						SIIHDMI_TPI_REG_PWR_STATE,
						SIIHDMI_POWER_STATE_D2);
		if (ret < 0)
			DEBUG("unable to change to a low power-state\n");
	}

	/* register a notifier for future fb events */
	tx->nb.notifier_call = siihdmi_fb_event_handler;
	fb_register_client(&tx->nb);

	return 0;

error:
	kfree(tx);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit siihdmi_remove(struct i2c_client *client)
{
	struct siihdmi_tx *tx;

	tx = i2c_get_clientdata(client);
	if (tx) {
		fb_unregister_client(&tx->nb);
		kfree(tx);
		i2c_set_clientdata(client, NULL);
	}

	return 0;
}

static const struct i2c_device_id siihdmi_device_table[] = {
	{ "sii9022", 0 },
	{ },
};

static struct i2c_driver siihdmi_driver = {
	.driver   = { .name = "sii9022" },
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

