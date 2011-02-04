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
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#include <linux/edid.h>
#include <linux/cea861.h>
#include <linux/i2c/siihdmi.h>

#if defined(CONFIG_MACH_MX51_EFIKAMX)
/*
 * IPU flings out DI0_SYNC_DISP_ERR and something similar to
 * IDMAC_NFB4EOF_ERR_21 if we change modes aggressively and will be panicking
 * while we're trying to set the sync config and we are not synchronized to the
 * blanking interval of the IPU.  This is described in the MX51 manual
 * (rev 1 42.3.6.5.1)
 */
#define MX51_IPU_SETTLE_TIME_MS				(100)
#endif


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


/*
 * Interesting note:
 * CEA spec describes several 1080p "low" field rates at 24, 25 and 30 fields
 * per second (mode 32, 33, 34).  They all run at a 74.250MHz pixel clock just
 * like 720p@60. A decent HDMI TV should be able to display these as it
 * corresponds to "film mode". These are in CEA Short Video Descriptors.
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
		CONTINUE(" (rev %01u.%01u)", (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_TPI_REVISION);
	CONTINUE(" (%s", data & (1 << 7) ? "Virtual " : "");
	data &= ~(1 << 7);
	data = data ? data : SIIHDMI_BASE_TPI_REVISION;
	CONTINUE("TPI revision %01u.%01u)", (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SIIHDMI_TPI_REG_HDCP_REVISION);
	if (data)
		CONTINUE(" (HDCP version %01u.%01u)",
			 (data >> 4) & 15, data & 15);

	CONTINUE("\n");

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
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_PWR_STATE,
					SIIHDMI_POWER_STATE_D0);
	if (ret < 0) {
		ERROR("unable to power up transmitter\n");
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
		WARNING("unable to setup interrupt request\n");
		return ret;
	}

	return 0;
}

static bool siihdmi_sink_present(struct siihdmi_tx *tx)
{
	u8 isr;
	bool present;

	isr = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);

	present = isr & (SIIHDMI_ISR_HOT_PLUG_EVENT |
			 SIIHDMI_ISR_RECEIVER_SENSE_EVENT);

	DEBUG("%ssink detected%s%s%s%s\n",
	      present ? "" : "no ",
	      present ? " (" : "",
	      (isr & SIIHDMI_ISR_HOT_PLUG_EVENT) ? "hotplug, " : ", ",
	      (isr & SIIHDMI_ISR_RECEIVER_SENSE_EVENT) ? "receiver sense, " : ", ",
	      present ? "\b\b)" : "\b\b");

	return present;
}

static int siihdmi_read_edid(struct siihdmi_tx *tx, u8 *edid, size_t size)
{
	u8 offset, ctrl;
	int ret;
	unsigned long start;

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

	ctrl = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_SYS_CTRL);

	/* step 2: request the DDC bus */
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
	offset = 0;
	ret = i2c_transfer(tx->client->adapter, request, ARRAY_SIZE(request));
	if (ret != ARRAY_SIZE(request))
		DEBUG("unable to read EDID block\n");

relinquish:
	/* step 6: relinquish ownership of the DDC bus */
	start = jiffies;
	do {
		i2c_smbus_write_byte_data(tx->client,
					  SIIHDMI_TPI_REG_SYS_CTRL,
					  0x00);
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SIIHDMI_TPI_REG_SYS_CTRL);
	} while ((ctrl & SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED) &&
		 !time_after(jiffies, start + bus_timeout));

	/* step 7: (potentially) enable HDCP */

	return 0;
}

static void siihdmi_parse_cea861_timing_block(struct siihdmi_tx *tx,
					      const struct edid_extension *ext)
{
	const struct cea861_timing_block * const cea =
		(struct cea861_timing_block *) ext;
	const u8 offset = offsetof(struct cea861_timing_block, data);
	u8 index = 0;

	BUILD_BUG_ON(sizeof(*cea) != sizeof(*ext));

	tx->enable_audio = cea->basic_audio_supported;

	if (cea->underscan_supported)
		tx->pixel_mapping = PIXEL_MAPPING_UNDERSCANNED;

	if (cea->dtd_offset == CEA81_NO_DTDS_PRESENT)
		return;

	do {
		const struct cea861_data_block_header * const header =
			(struct cea861_data_block_header *) &cea->data[index];

		switch (header->tag) {
		case CEA861_DATA_BLOCK_TYPE_VENDOR_SPECIFIC: {
				const struct cea861_vendor_specific_data_block * const vsdb =
					(struct cea861_vendor_specific_data_block *) header;

				if (!memcmp(vsdb->ieee_registration,
					    CEA861_OUI_REGISTRATION_ID_HDMI_LSB,
					    sizeof(vsdb->ieee_registration)))
					tx->connection_type = CONNECTION_TYPE_HDMI;
			}
			break;
		default:
			break;
		}

		index = index + header->length + sizeof(header);
	} while (index < cea->dtd_offset - offset);
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

	pixclk = var->pixclock ? PICOS2KHZ(var->pixclock) : 0;
	htotal = var->xres + var->left_margin + var->hsync_len + var->right_margin;
	vtotal = var->yres + var->upper_margin + var->vsync_len + var->lower_margin;
	refresh = (htotal * vtotal) / (pixclk * 1000ul);

	BUG_ON(pixclk == 0);

	/* basic video mode data */
	vmode[PIXEL_CLOCK]  = pixclk / 10;
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

static int siihdmi_clear_avi_info_frame(struct siihdmi_tx *tx)
{
	const u8 buffer[SIIHDMI_TPI_REG_AVI_INFO_FRAME_LENGTH] = {0};
	int ret;

	BUG_ON(tx->connection_type != CONNECTION_TYPE_DVI);

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SIIHDMI_TPI_REG_AVI_INFO_FRAME_BASE,
					     sizeof(buffer), buffer);
	if (ret < 0)
		DEBUG("unable to clear avi info frame\n");

	return ret;

}

static int siihdmi_set_avi_info_frame(struct siihdmi_tx *tx)
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

		.video_format              = VIDEO_FORMAT_UNKNOWN,
	};

	BUG_ON(tx->connection_type != CONNECTION_TYPE_HDMI);

	switch (tx->pixel_mapping) {
	case PIXEL_MAPPING_UNDERSCANNED:
		avi.scan_information = SCAN_INFORMATION_UNDERSCANNED;
		break;
	case PIXEL_MAPPING_OVERSCANNED:
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
			.enable     = tx->enable_audio,
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

	BUG_ON(tx->connection_type != CONNECTION_TYPE_HDMI);

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

	BUG_ON(tx->connection_type != CONNECTION_TYPE_HDMI);

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

	tx->tmds_enabled = false;

	msleep(SIIHDMI_CTRL_INFO_FRAME_DRAIN_TIME);

	/* step 4: change video resolution */

	/* step 5: set the vmode registers */
	siihdmi_set_vmode_registers(tx, var);

	/*
	 * step 6:
	 *      [DVI]  clear AVI InfoFrame
	 *      [HDMI] set AVI InfoFrame
	 */
	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		siihdmi_set_avi_info_frame(tx);
	else
		siihdmi_clear_avi_info_frame(tx);

	/* step 7: [HDMI] set new audio information */
	if (tx->connection_type == CONNECTION_TYPE_HDMI) {
		siihdmi_set_audio_info_frame(tx);
		siihdmi_set_spd_info_frame(tx);
	}

	/* step 8: enable display */
	ctrl &= ~SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	/* optimisation: merge the write into the next one */

	/* step 9: (optionally) un-blank the display */
	ctrl &= ~SIIHDMI_SYS_CTRL_AV_MUTE_HDMI;
	ret = i2c_smbus_write_byte_data(tx->client,
					SIIHDMI_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to enable the display\n");

	/* step 10: (potentially) enable HDCP */

	tx->tmds_enabled = true;

	return ret;
}

static const struct fb_videomode *
_fb_match_resolution(const struct fb_videomode * const mode,
		     struct list_head *head)
{
	const struct fb_modelist *entry, *next;

	list_for_each_entry_safe(entry, next, head, list) {
		if (fb_res_is_equal(mode, &entry->mode))
			return &entry->mode;
	}

	return NULL;
}

static void siihdmi_sanitize_modelist(struct siihdmi_tx * const tx)
{
	struct list_head *modelist = &tx->info->modelist;
	const struct fb_modelist *entry, *next;
	const struct fb_videomode *mode;

	if ((mode = fb_find_best_display(&tx->info->monspecs, modelist)))
		memcpy(&tx->preferred, mode, sizeof(tx->preferred));

	/*
	 * Prefer detailed timings found in EDID.  Certain sinks support slight
	 * variations of VESA/CEA timings, and using those allows us to support
	 * a wider variety of devieces.
	 */
	/* TODO: build a list of detailed timing modes and match against it */
	list_for_each_entry_safe(entry, next, modelist, list) {
		bool remove = false;

		mode = &entry->mode;
		if (mode->vmode & FB_VMODE_INTERLACED) {
//			DEBUG("Removing mode %ux%u@%u (interlaced)\n", mode->xres, mode->yres, mode->refresh);
			remove = true;
		} else if (mode->pixclock < tx->platform->pixclock) {
//			DEBUG("Removing mode %ux%u@%u (exceeds pixclk limit)\n", mode->xres, mode->yres, mode->refresh);
			remove = true;
		} else if (mode->lower_margin < 2) {
			/*
			 * HDMI specification requires at least 2 lines of
			 * vertical sync (sect. 5.1.2).
			 */
//			DEBUG("Removing mode %ux%u@%u (vertical sync period too short)\n", mode->xres, mode->yres, mode->refresh);
			remove = true;
		} else {
			const struct fb_videomode *match =
				_fb_match_resolution(mode, modelist);

			if ((match = _fb_match_resolution(mode, modelist)) &&
			    (~mode->flag & FB_MODE_IS_DETAILED)            &&
			    (match->flag & FB_MODE_IS_DETAILED)) {
//				DEBUG("Removing mode %ux%u@%u (redundant mode, detailed match)\n", mode->xres, mode->yres, mode->refresh);
				remove = true;
			}

			if ((match = _fb_match_resolution(mode, modelist)) &&
			    (~mode->flag & FB_MODE_IS_CEA)            &&
			    (match->flag & FB_MODE_IS_CEA)) {
//				DEBUG("Removing mode %ux%u@%u (redundant mode, cea match)\n", mode->xres, mode->yres, mode->refresh);
				remove = true;
			}
		}

		if (remove) {
			struct fb_modelist *modelist =
				container_of(mode, struct fb_modelist, mode);

			/*
			 * We could use flag & FB_MODE_IS_FIRST but it may not
			 * be the mode fb_find_best_display actually returned
			 */

//			if (fb_mode_is_equal(&tx->preferred, mode)) DEBUG("deleted preferred video mode!\n");

			list_del(&modelist->list);
			kfree(&modelist->list);
		}
	}
}

static void siihdmi_dump_modelines(struct siihdmi_tx *tx)
{
	const struct list_head * const modelines = &tx->info->modelist;
	const struct fb_modelist *entry;

	INFO("Supported modelines:\n");
	list_for_each_entry(entry, modelines, list) {
		const struct fb_videomode * const mode = &entry->mode;
		const bool interlaced = (mode->vmode & FB_VMODE_INTERLACED);
		const bool double_scan = (mode->vmode & FB_VMODE_DOUBLE);
		u32 pixclk = mode->pixclock ? PICOS2KHZ(mode->pixclock) : 0;
		char *flag = " ";

		if (fb_mode_is_equal(&tx->preferred, mode)) {
			flag = "*";
		}
		if (mode->flag & FB_MODE_IS_CEA) {
			flag = "C";
		}

		pixclk >>= (double_scan ? 1 : 0);

		INFO("  %s \"%dx%d@%d%s\" %lu.%.2lu %u %u %u %u %u %u %u %u %chsync %cvsync\n",
		     /* list CEA or preferred status of modeline */
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
	}
}

static const struct fb_videomode *
siihdmi_select_video_mode(const struct siihdmi_tx * const tx)
{
	const struct fb_videomode *mode = NULL;
	const struct fb_videomode * const pref = &siihdmi_default_video_mode;

	mode = fb_find_nearest_mode(pref, &tx->info->modelist);

	if (mode && (mode->xres == pref->xres) && (mode->yres == pref->yres)) {
		/* prefer 1280x720 if the monitor supports that mode exactly */
		return mode;
	} else if (tx->preferred.xres && tx->preferred.yres) {
		/* otherwise, use the closest to the monitor preferred mode */
		mode = fb_find_nearest_mode(&tx->preferred, &tx->info->modelist);
	}

	/* if no mode was found push 1280x720 anyway */
	mode = mode ? mode : pref;

#if defined(CONFIG_MACH_MX51_EFIKAMX)
	/*
	 * At modes somewhat greater than 1280x720 (1280x768, 1280x800 may be
	 * fine) doing heavy video work like playing a 720p movie may cause the
	 * IPU to black the screen intermittently due to lack of IPU bandwidth.
	 */
	if ((mode->xres > 1024) && (mode->yres > 720))
		WARNING("available video bandwidth may be very low at the "
			"selected resolution (%ux%u@%u) and may cause IPU errors\n",
			mode->xres, mode->yres, mode->refresh);
#endif

	return mode;
}

static int siihdmi_setup_display(struct siihdmi_tx *tx)
{
	struct fb_var_screeninfo var = {0};
	struct edid_block0 block0;
	int ret;

	BUILD_BUG_ON(sizeof(struct edid_block0) != EDID_BLOCK_SIZE);
	BUILD_BUG_ON(sizeof(struct edid_extension) != EDID_BLOCK_SIZE);

	/* defaults */
	tx->connection_type = CONNECTION_TYPE_DVI;
	tx->pixel_mapping = PIXEL_MAPPING_EXACT;
	tx->enable_audio = false;

	/* use EDID to detect sink characteristics */
	if ((ret = siihdmi_read_edid(tx, (u8 *) &block0, sizeof(block0))) < 0)
		return ret;

	if (!edid_verify_checksum((u8 *) &block0))
		WARNING("EDID block 0 CRC mismatch\n");

	/* need to allocate space for block 0 as well as the extensions */
	tx->edid_length = (block0.extensions + 1) * EDID_BLOCK_SIZE;

	tx->edid = kzalloc(tx->edid_length, GFP_KERNEL);
	if (!tx->edid)
		return -ENOMEM;

	if ((ret = siihdmi_read_edid(tx, tx->edid, tx->edid_length)) < 0)
		return ret;

	if (sysfs_create_bin_file(&tx->info->dev->kobj, &tx->edid_attr) < 0)
		WARNING("unable to populate edid sysfs attribute\n");

	/* create monspecs from EDID for the basic stuff */
	fb_edid_to_monspecs(tx->edid, &tx->info->monspecs);

	if (block0.extensions) {
		const struct edid_extension * const extensions =
			(struct edid_extension *) (tx->edid + sizeof(block0));
		u8 i;

		for (i = 0; i < block0.extensions; i++) {
			const struct edid_extension * const extension =
				&extensions[i];

			if (!edid_verify_checksum((u8 *) extension))
				WARNING("EDID block %u CRC mismatch\n", i);

			/* if there's an extension, add them to the monspecs */
			fb_edid_add_monspecs((unsigned char *) extension, &tx->info->monspecs);

			switch (extension->tag) {
			case EDID_EXTENSION_CEA:
				siihdmi_parse_cea861_timing_block(tx,
								  extension);
				break;
			default:
				break;
			}
		}
	}

	fb_videomode_to_modelist(tx->info->monspecs.modedb,
				 tx->info->monspecs.modedb_len,
				 &tx->info->modelist);

	siihdmi_sanitize_modelist(tx);
	siihdmi_dump_modelines(tx);

	fb_videomode_to_var(&var, siihdmi_select_video_mode(tx));

#if defined(CONFIG_MACH_MX51_EFIKAMX)
	msleep(MX51_IPU_SETTLE_TIME_MS);
#endif

	if ((ret = siihdmi_set_resolution(tx, &var)) < 0)
		return ret;

	/* activate the framebuffer */
	var.activate = FB_ACTIVATE_ALL;

	acquire_console_sem();
	tx->info->flags |= FBINFO_MISC_USEREVENT;
	fb_set_var(tx->info, &var);
	tx->info->flags &= ~FBINFO_MISC_USEREVENT;
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

	/* TODO Power up TMDS if the tx->tmds_enabled is false */

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
	struct fb_var_screeninfo var = {0};

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
#if defined(CONFIG_MACH_MX51_EFIKAMX)
		msleep(MX51_IPU_SETTLE_TIME_MS);
#endif
		if (!strcmp(event->info->fix.id, tx->platform->framebuffer)) {
			tx->info = event->info;
			return siihdmi_setup_display(tx);
		}

		break;
	case FB_EVENT_MODE_CHANGE:
		fb_videomode_to_var(&var, event->info->mode);
#if defined(CONFIG_MACH_MX51_EFIKAMX)
		msleep(MX51_IPU_SETTLE_TIME_MS);
#endif
		return siihdmi_set_resolution(tx, &var);
	case FB_EVENT_BLANK:
		fb_videomode_to_var(&var, event->info->mode);

		switch (*((int *) event->data)) {
		case FB_BLANK_POWERDOWN:
			return siihdmi_blank(tx, &var, 1);
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			return siihdmi_blank(tx, &var, 0);
		case FB_BLANK_UNBLANK:
			return siihdmi_unblank(tx, &var);
		}

		break;
	default:
		DEBUG("unhandled fb event 0x%lx", val);
		break;
	}

	return 0;
}

#if defined(CONFIG_FB_SIIHDMI_HOTPLUG)
static irqreturn_t siihdmi_hotplug_handler(int irq, void *dev_id)
{
	struct siihdmi_tx *tx = ((struct siihdmi_tx *) dev_id);

	schedule_delayed_work(&tx->hotplug,
			      msecs_to_jiffies(SIIHDMI_HOTPLUG_HANDLER_TIMEOUT));

	return IRQ_HANDLED;
}

static void siihdmi_hotplug_event(struct work_struct *work)
{
	struct siihdmi_tx *tx =
		container_of(work, struct siihdmi_tx, hotplug.work);
	u8 isr, ier;

	/* clear the interrupt */
	ier = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_IER);
	i2c_smbus_write_byte_data(tx->client, SIIHDMI_TPI_REG_IER, ier);

	DEBUG("hotplug event(s) received: %s%s%s%s%s%s\b\b\n",
	      (ier & SIIHDMI_IER_HOT_PLUG_EVENT) ? "hotplug, " : "",
	      (ier & SIIHDMI_IER_RECEIVER_SENSE_EVENT) ? "receiver, " : "",
	      (ier & SIIHDMI_IER_CTRL_BUS_EVENT) ? "control bus, " : "",
	      (ier & SIIHDMI_IER_CPI_EVENT) ? "CPI, " : "",
	      (ier & SIIHDMI_IER_AUDIO_EVENT) ? "audio error, " : "",
	      (ier & (SIIHDMI_IER_SECURITY_STATUS_CHANGE |
	              SIIHDMI_IER_HDCP_VALUE_READY       |
	              SIIHDMI_IER_HDCP_AUTHENTICATION_STATUS_CHANGE)) ? "HDCP, " : "");

	/* TODO Handle Events */

	isr = i2c_smbus_read_byte_data(tx->client, SIIHDMI_TPI_REG_ISR);
	DEBUG("polled event(s) received: %s%s%s%s%s%s\b\b\n",
	      (isr & SIIHDMI_ISR_HOT_PLUG_EVENT) ? "hotplug, " : "",
	      (isr & SIIHDMI_ISR_RECEIVER_SENSE_EVENT) ? "receiver, " : "",
	      (isr & SIIHDMI_ISR_CTRL_BUS_EVENT) ? "control bus, " : "",
	      (isr & SIIHDMI_ISR_CPI_EVENT) ? "CPI, " : "",
	      (isr & SIIHDMI_ISR_AUDIO_EVENT) ? "audio error, " : "",
	      (isr & (SIIHDMI_ISR_SECURITY_STATUS_CHANGED |
	              SIIHDMI_ISR_HDCP_VALUE_READY        |
	              SIIHDMI_ISR_HDCP_AUTHENTICATION_STATUS_CHANGED)) ? "HDCP, " : "");
}
#endif

static ssize_t siihdmi_sysfs_read_edid(struct kobject *kobj,
				       struct bin_attribute *bin_attr,
				       char *buf, loff_t off, size_t count)
{
	const struct siihdmi_tx * const tx =
		container_of(bin_attr, struct siihdmi_tx, edid_attr);

	return memory_read_from_buffer(buf, count, &off,
				       tx->edid, tx->edid_length);
}

static inline unsigned long __irq_flags(const struct resource * const res)
{
	return (res->flags & IRQF_TRIGGER_MASK);
}

static int __devinit siihdmi_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct siihdmi_tx *tx;
	int i, ret;

	tx = kzalloc(sizeof(struct siihdmi_tx), GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	tx->client = client;
	tx->platform = client->dev.platform_data;

	tx->edid_attr.attr.name  = "edid";
	tx->edid_attr.attr.owner = THIS_MODULE;
	tx->edid_attr.attr.mode  = 0444;
	/* maximum size of EDID, not necessarily the size of our data */
	tx->edid_attr.size       = SZ_32K;
	tx->edid_attr.read       = siihdmi_sysfs_read_edid;

#if defined(CONFIG_FB_SIIHDMI_HOTPLUG)
	INIT_DELAYED_WORK(&tx->hotplug, siihdmi_hotplug_event);

	ret = request_irq(tx->platform->hotplug.start, siihdmi_hotplug_handler,
			  __irq_flags(&tx->platform->hotplug),
			  tx->platform->hotplug.name, tx);
	if (ret < 0)
		WARNING("failed to setup hotplug interrupt: %d\n", ret);
#endif

	i2c_set_clientdata(client, tx);

#if defined(CONFIG_MACH_MX51_EFIKAMX)
	msleep(MX51_IPU_SETTLE_TIME_MS);
#endif

	/* initialise the device */
	if ((ret = siihdmi_initialise(tx)) < 0)
		goto error;

	if (siihdmi_sink_present(tx)) {
		for (i = 0; i < num_registered_fb; i++) {
			struct fb_info * const info = registered_fb[i];

			if (!strcmp(info->fix.id, tx->platform->framebuffer)) {
				tx->info = info;

				if (siihdmi_setup_display(tx) < 0)
					goto error;

				break;
			}
		}
	} else {
		/*
		 * A sink is not currently present.  However, the device has
		 * been initialised.  This includes setting up the crucial IER.
		 * As a result, we can power down the transmitter and be
		 * signalled when the sink state changes.
		 */
		ret = i2c_smbus_write_byte_data(tx->client,
						SIIHDMI_TPI_REG_PWR_STATE,
						SIIHDMI_POWER_STATE_D3);
		if (ret < 0)
			WARNING("unable to change to a low power-state\n");
	}

	/* register a notifier for future fb events */
	tx->nb.notifier_call = siihdmi_fb_event_handler;
	fb_register_client(&tx->nb);

	return 0;

error:
#if defined(CONFIG_FB_SIIHDMI_HOTPLUG)
	if (tx->platform->hotplug.start)
		free_irq(tx->platform->hotplug.start, NULL);
#endif

	kfree(tx);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit siihdmi_remove(struct i2c_client *client)
{
	struct siihdmi_tx *tx;

	tx = i2c_get_clientdata(client);
	if (tx) {
#if defined(CONFIG_FB_SIIHDMI_HOTPLUG)
		if (tx->platform->hotplug.start)
			free_irq(tx->platform->hotplug.start, NULL);
#endif

		sysfs_remove_bin_file(&tx->info->dev->kobj, &tx->edid_attr);

		if (tx->edid)
			kfree(tx->edid);

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

