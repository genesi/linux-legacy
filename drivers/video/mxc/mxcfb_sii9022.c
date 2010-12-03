/* vim: set noet ts=8 sts=8 sw=8 : */
/*
 * Copyright © 2010 Saleem Abdulrasool <compnerd@compnerd.org>.
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

/* logging helpers */
#define DEBUG(fmt, ...)		printk(KERN_DEBUG "SII9022: " fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...)		printk(KERN_ERR   "SII9022: " fmt, ## __VA_ARGS__)
#define WARNING(fmt, ...)	printk(KERN_WARN  "SII9022: " fmt, ## __VA_ARGS__)
#define INFO(fmt, ...)		printk(KERN_INFO  "SII9022: " fmt, ## __VA_ARGS__)


/* TPI registers */
#define SII9022_TPI_REG_VIDEO_MODE_DATA_BASE		(0x00)
#define SII9022_TPI_REG_PIXEL_CLOCK_LSB			(0x00)
#define SII9022_TPI_REG_PIXEL_CLOCK_MSB			(0x01)
#define SII9022_TPI_REG_VFREQ_LSB			(0x02)
#define SII9022_TPI_REG_VFREQ_MSB			(0x03)
#define SII9022_TPI_REG_PIXELS_LSB			(0x04)
#define SII9022_TPI_REG_PIXELS_MSB			(0x05)
#define SII9022_TPI_REG_LINES_LSB			(0x06)
#define SII9022_TPI_REG_LINES_MSB			(0x07)
#define SII9022_TPI_REG_INPUT_BUS_PIXEL_REPETITION	(0x08)
#define SII9022_TPI_REG_AVI_INPUT_FORMAT		(0x09)
#define SII9022_TPI_REG_AVI_OUTPUT_FORMAT		(0x0a)
#define SII9022_TPI_REG_YC_INPUT_MODE			(0x0b)
#define SII9022_TPI_REG_AVI_INFO_FRAME_BASE		(0x0c)
#define SII9022_TPI_REG_AVI_DBYTE0			(0x0c)
#define SII9022_TPI_REG_AVI_DBYTE1			(0x0d)
#define SII9022_TPI_REG_AVI_DBYTE2			(0x0e)
#define SII9022_TPI_REG_AVI_DBYTE3			(0x0f)
#define SII9022_TPI_REG_AVI_DBYTE4			(0x10)
#define SII9022_TPI_REG_AVI_DBYTE5			(0x11)
#define SII9022_TPI_REG_AVI_INFO_END_TOP_BAR_LSB	(0x12)
#define SII9022_TPI_REG_AVI_INFO_END_TOP_BAR_MSB	(0x13)
#define SII9022_TPI_REG_AVI_INFO_START_BOTTOM_BAR_LSB	(0x14)
#define SII9022_TPI_REG_AVI_INFO_START_BOTTOM_BAR_MSB	(0x15)
#define SII9022_TPI_REG_AVI_INFO_END_LEFT_BAR_LSB	(0x16)
#define SII9022_TPI_REG_AVI_INFO_END_LEFT_BAR_MSB	(0x17)
#define SII9022_TPI_REG_AVI_INFO_END_RIGHT_BAR_LSB	(0x18)
#define SII9022_TPI_REG_AVI_INFO_END_RIGHT_BAR_MSB	(0x19)
#define SII9022_TPI_REG_SYS_CTRL			(0x1a)
#define SII9022_TPI_REG_DEVICE_ID			(0x1b)
#define SII9022_TPI_REG_DEVICE_REVISION			(0x1c)
#define SII9022_TPI_REG_TPI_REVISION			(0x1d)
#define SII9022_TPI_REG_PWR_STATE			(0x1e)
#define SII9022_TPI_REG_I2S_ENABLE_MAPPING		(0x1f)
#define SII9022_TPI_REG_I2S_IINPUT_CONFIGURATAION	(0x20)
#define SII9022_TPI_REG_I2S_STREAM_HEADER_SETTINGS_BASE	(0x21)
#define SII9022_TPI_REG_I2S_CHANNEL_STATUS		(0x21)
#define SII9022_TPI_REG_I2S_CATEGORY_CODE		(0x22)
#define SII9022_TPI_REG_I2S_SOURCE_CHANNEL		(0x23)
#define SII9022_TPI_REG_I2S_ACCURACY_SAMPLING_FREQUENCY	(0x24)
#define SII9022_TPI_REG_I2S_ORIGINAL_FREQ_SAMPLE_LENGTH	(0x25)
#define SII9022_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL	(0x26)
#define SII9022_TPI_REG_I2S_AUDIO_SAMPLING_HBR		(0x27)
#define SII9022_TPI_REG_I2S_AUDIO_RESERVED		(0x28)
#define SII9022_TPI_REG_HDCP_QUERY_DATA			(0x29)
#define SII9022_TPI_REG_HDCP_CONTROL_DATA		(0x2a)
#define SII9022_TPI_REG_HDCP_CONTROL_DATA		(0x2a)
#define SII9022_TPI_REG_HDCP_BKSV_1			(0x2b)
#define SII9022_TPI_REG_HDCP_BKSV_2			(0x2c)
#define SII9022_TPI_REG_HDCP_BKSV_3			(0x2d)
#define SII9022_TPI_REG_HDCP_BKSV_4			(0x2e)
#define SII9022_TPI_REG_HDCP_BKSV_5			(0x2f)
#define SII9022_TPI_REG_HDCP_REVISION			(0x30)
#define SII9022_TPI_REG_HDCP_KSV_V_STAR_VALUE		(0x31)
#define SII9022_TPI_REG_HDCP_V_STAR_VALUE_1		(0x32)
#define SII9022_TPI_REG_HDCP_V_STAR_VALUE_2		(0x33)
#define SII9022_TPI_REG_HDCP_V_STAR_VALUE_3		(0x34)
#define SII9022_TPI_REG_HDCP_V_STAR_VALUE_4		(0x35)
#define SII9022_TPI_REG_HDCP_AKSV_1			(0x36)
#define SII9022_TPI_REG_HDCP_AKSV_2			(0x37)
#define SII9022_TPI_REG_HDCP_AKSV_3			(0x38)
#define SII9022_TPI_REG_HDCP_AKSV_4			(0x39)
#define SII9022_TPI_REG_HDCP_AKSV_5			(0x3a)

#define SII9022_TPI_REG_IER				(0x3c)
#define SII9022_TPI_REG_ISR				(0x3d)

#define SII9022_TPI_REG_RQB				(0xc7)

/* Input Bus and Pixel Repetition */
#define SII9022_PIXEL_REPETITION_DUAL			(1 << 0)
#define SII9022_PIXEL_REPETITION_QUAD			(3 << 0)
#define SII9022_INPUT_BUS_EDGE_SELECT_RISING		(1 << 4)
#define SII9022_INPUT_BUS_SELECT_FULL_PIXEL_WIDTH	(1 << 5)
#define SII9022_INPUT_BUS_TMDS_CLOCK_RATIO_1X		(1 << 6)
#define SII9022_INPUT_BUS_TMDS_CLOCK_RATIO_2X		(2 << 6)
#define SII9022_INPUT_BUS_TMDS_CLOCK_RATIO_4X		(3 << 6)

/* Input Format */
#define SII9022_INPUT_COLOR_SPACE_RGB			(0 << 0)
#define SII9022_INPUT_COLOR_SPACE_YUV_444		(1 << 0)
#define SII9022_INPUT_COLOR_SPACE_YUV_422		(2 << 0)
#define SII9022_INPUT_COLOR_SPACE_BLACK			(3 << 0)
#define SII9022_INPUT_VIDEO_RANGE_EXPANSION_AUTO	(0 << 2)
#define SII9022_INPUT_VIDEO_RANGE_EXPANSION_ON		(1 << 2)
#define SII9022_INPUT_VIDEO_RANGE_EXPANSION_OFF		(2 << 2)
#define SII9022_INPUT_COLOR_DEPTH_8BIT			(0 << 6)
#define SII9022_INPUT_COLOR_DEPTH_10BIT			(2 << 6)
#define SII9022_INPUT_COLOR_DEPTH_12BIT			(3 << 6)
#define SII9022_INPUT_COLOR_DEPTH_16BIT			(1 << 6)

/* Output Format */
#define SII9022_OUTPUT_FORMAT_HDMI_RGB			(0 << 0)
#define SII9022_OUTPUT_FORMAT_HDMI_YUV_444		(1 << 0)
#define SII9022_OUTPUT_FORMAT_HDMI_YUV_422		(2 << 0)
#define SII9022_OUTPUT_FORMAT_DVI_RGB			(3 << 0)
#define SII9022_OUTPUT_VIDEO_RANGE_COMPRESSION_AUTO	(0 << 2)
#define SII9022_OUTPUT_VIDEO_RANGE_COMPRESSION_ON	(1 << 2)
#define SII9022_OUTPUT_VIDEO_RANGE_COMPRESSION_OFF	(2 << 2)
#define SII9022_OUTPUT_COLOR_STANDARD_BT601		(0 << 4)
#define SII9022_OUTPUT_COLOR_STANDARD_BT709		(1 << 4)
#define SII9022_OUTPUT_DITHERING			(1 << 5)
#define SII9022_OUTPUT_COLOR_DEPTH_8BIT			(0 << 6)
#define SII9022_OUTPUT_COLOR_DEPTH_10BIT		(2 << 6)
#define SII9022_OUTPUT_COLOR_DEPTH_12BIT		(3 << 6)
#define SII9022_OUTPUT_COLOR_DEPTH_16BIT		(1 << 6)

/* System Control */
#define SII9022_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI	(1 << 0)
#define SII9022_SYS_CTRL_DDC_BUS_OWNER_HOST		(1 << 1)
#define SII9022_SYS_CTRL_DDC_BUS_GRANTED		(1 << 1)
#define SII9022_SYS_CTRL_DDC_BUS_REQUEST		(1 << 2)
#define SII9022_SYS_CTRL_AV_MUTE_HDMI			(1 << 3)
#define SII9022_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN		(1 << 4)
#define SII9022_SYS_CTRL_DYNAMIC_LINK_INTEGRITY		(1 << 6)

/* Device Power State Control Data */
#define SII9022_POWER_STATE_D0				(0 << 0)
#define SII9022_POWER_STATE_D1				(1 << 0)
#define SII9022_POWER_STATE_D2				(2 << 0)
#define SII9022_POWER_STATE_D3				(3 << 0)
#define SII9022_WAKEUP_STATE_COLD			(1 << 2)

/* I²S Enable and Mapping */
#define SII9022_I2S_MAPPING_SELECT_SD_CHANNEL_0		(0 << 0)
#define SII9022_I2S_MAPPING_SELECT_SD_CHANNEL_1		(1 << 0)
#define SII9022_I2S_MAPPING_SELECT_SD_CHANNEL_2		(2 << 0)
#define SII9022_I2S_MAPPING_SELECT_SD_CHANNEL_3		(3 << 0)
#define SII9022_I2S_MAPPING_SWAP_CHANNELS		(1 << 3)
#define SII9022_I2S_ENABLE_BASIC_AUDIO_DOWNSAMPLING	(1 << 4)
#define SII9022_I2S_MAPPING_SELECT_FIFO_0		(0 << 5)
#define SII9022_I2S_MAPPING_SELECT_FIFO_1		(1 << 5)
#define SII9022_I2S_MAPPING_SELECT_FIFO_2		(2 << 5)
#define SII9022_I2S_MAPPING_SELECT_FIFO_3		(3 << 5)
#define SII9022_I2S_ENABLE_SELECTED_FIFO		(1 << 7)

/* I²S Input Configuration */
#define SII9022_I2S_WS_TO_SD_FIRST_BIT_SHIFT		(1 << 0)
#define SII9022_I2S_SD_LSB_FIRST			(1 << 1)
#define SII9022_I2S_SD_RIGHT_JUSTIFY_DATA		(1 << 2)
#define SII9022_I2S_WS_POLARITY_HIGH			(1 << 3)
#define SII9022_I2S_MCLK_MULTIPLIER_128			(0 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_256			(1 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_384			(2 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_512			(3 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_768			(4 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_1024		(5 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_1152		(6 << 4)
#define SII9022_I2S_MCLK_MULTIPLIER_192			(7 << 4)
#define SII9022_I2S_SCK_SAMPLE_RISING_EDGE		(1 << 7)

/* Interrupt Enable */
#define SII9022_IER_HOT_PLUG_EVENT			(1 << 0)
#define SII9022_IER_RECEIVER_SENSE_EVENT		(1 << 1)
#define SII9022_IER_CTRL_BUS_EVENT			(1 << 2)
#define SII9022_IER_CPI_EVENT				(1 << 3)
#define SII9022_IER_AUDIO_EVENT				(1 << 4)
#define SII9022_IER_SECURITY_STATUS_CHANGE		(1 << 5)
#define SII9022_IER_HDCP_VALUE_READY			(1 << 6)
#define SII9022_IER_HDCP_AUTHENTICATION_STATUS_CHANGE	(1 << 7)

/* Interrupt Status */
#define SII9022_ISR_HOT_PLUG_EVENT			(1 << 0)
#define SII9022_ISR_RECEIVER_SENSE_EVENT		(1 << 1)
#define SII9022_ISR_CTRL_BUS_EVENT			(1 << 2)
#define SII9022_ISR_CPI_EVENT				(1 << 3)
#define SII9022_ISR_AUDIO_EVENT				(1 << 4)
#define SII9022_ISR_SECURITY_STATUS_CHANGED		(1 << 5)
#define SII9022_ISR_HDCP_VALUE_READY			(1 << 6)
#define SII9022_ISR_HDCP_AUTHENTICATION_STATUS_CHANGED	(1 << 7)

/* Request/Grant/Black Mode */
#define SII9022_RQB_DDC_BUS_REQUEST			(1 << 0)
#define SII9022_RQB_DDC_BUS_GRANTED			(1 << 1)
#define SII9022_RQB_I2C_ACCESS_DDC_BUS			(1 << 2)
#define SII9022_RQB_FORCE_VIDEO_BLANK			(1 << 5)
#define SII9022_RQB_TPI_MODE_DISABLE			(1 << 7)


/* driver constants */
#define SII9022_DEVICE_ID_902x				(0xb0)
#define SII9022_BASE_TPI_REVISION			(0x29)
#define SII9022_AVI_INFO_FRAME_VERSION			(0x02)
#define SII9022_CTRL_INFO_FRAME_DRAIN_TIME		(0x80)


/* EDID constants and Structures */
#define EDID_I2C_DDC_DATA_ADDRESS			(0x50)

#define EEDID_BASE_LENGTH				(0x100)

#define EEDID_EXTENSION_FLAG				(0x7e)

#define EEDID_EXTENSION_TAG				(0x80)
#define EEDID_EXTENSION_DATA_OFFSET			(0x80)

enum edid_extension {
	EDID_EXTENSION_TIMING           = 0x00, // Timing Extension
	EDID_EXTENSION_CEA              = 0x02, // Additional Timing Block Data (CEA EDID Timing Extension)
	EDID_EXTENSION_VTB              = 0x10, // Video Timing Block Extension (VTB-EXT)
	EDID_EXTENSION_EDID_2_0         = 0x20, // EDID 2.0 Extension
	EDID_EXTENSION_DI               = 0x40, // Display Information Extension (DI-EXT)
	EDID_EXTENSION_LS               = 0x50, // Localised String Extension (LS-EXT)
	EDID_EXTENSION_MI               = 0x60, // Microdisplay Interface Extension (MI-EXT)
	EDID_EXTENSION_DTCDB_1          = 0xa7, // Display Transfer Characteristics Data Block (DTCDB)
	EDID_EXTENSION_DTCDB_2          = 0xaf,
	EDID_EXTENSION_DTCDB_3          = 0xbf,
	EDID_EXTENSION_BLOCK_MAP        = 0xf0, // Block Map
	EDID_EXTENSION_DDDB             = 0xff, // Display Device Data Block (DDDB)
};

struct __packed cea_timing_block {
	u8       extension_tag;
	u8       revision_number;
	u8       dtd_start_offset;
	unsigned dtd_block_count       : 4;
	unsigned yuv_422_supported     : 1;
	unsigned yuv_444_supported     : 1;
	unsigned basic_audio_supported : 1;
	unsigned underscan_supported   : 1;
	u8       dbc_start_offset;              // data block collection offset
};


/* HDMI Constants and Structures (CEA-861-D) */
#define HDMI_PACKET_TYPE_INFO_FRAME			(0x80)
#define HDMI_PACKET_CHECKSUM				(0x100)

enum info_frame_type {
	INFO_FRAME_TYPE_RESERVED,
	INFO_FRAME_TYPE_VENDOR_SPECIFIC,
	INFO_FRAME_TYPE_AUXILIARY_VIDEO,
	INFO_FRAME_TYPE_SOURCE_PRODUCT_DESCRIPTION,
	INFO_FRAME_TYPE_AUDIO,
	INFO_FRAME_TYPE_MPEG,
};

struct __packed avi_info_frame {
	u8       chksum;

	unsigned scan_information            : 2;
	unsigned bar_info                    : 2;
	unsigned active_format_info_valid    : 1;
	unsigned pixel_format                : 2;
	unsigned dbyte1_reserved0            : 1;

	unsigned active_format_description   : 4;
	unsigned picture_aspect_ratio        : 2;
	unsigned colorimetry                 : 2;

	unsigned non_uniform_picture_scaling : 2;
	unsigned quantization_range          : 2;
	unsigned extended_colorimetry        : 3;
	unsigned it_content_present          : 1;

	unsigned video_format                : 7;
	unsigned dbyte4_reserved0            : 1;

	unsigned pixel_repetition_factor     : 4;       /* value - 1 */
	unsigned dbyte5_reserved0            : 4;

	u16      end_of_top_bar;
	u16      start_of_bottom_bar;
	u16      end_of_left_bar;
	u16      start_of_right_bar;
};

enum scan_information {
	SCAN_INFORMATION_UNKNOWN,
	SCAN_INFORMATION_OVERSCANNED,
	SCAN_INFORMATION_UNDERSCANNED,
};

enum bar_info {
	BAR_INFO_INVALID,
	BAR_INFO_VERTICAL,
	BAR_INFO_HORIZONTAL,
	BAR_INFO_BOTH,
};

enum pixel_format {
	PIXEL_FORMAT_RGB,       /* default */
	PIXEL_FORMAT_YUV_422,
	PIXEL_FORMAT_YUV_444,
};

enum active_format_description {
	ACTIVE_FORMAT_DESCRIPTION_UNSCALED      = 0x08,
	ACTIVE_FORMAT_DESCRIPTION_4_3_CENTERED  = 0x09,
	ACTIVE_FORMAT_DESCRIPTION_16_9_CENTERED = 0x10,
	ACTIVE_FORMAT_DESCRIPTION_14_9_CENTERED = 0x11,
};

enum picture_aspect_ratio {
	PICTURE_ASPECT_RATIO_UNSCALED,
	PICTURE_ASPECT_RATIO_4_3,
	PICTURE_ASPECT_RATIO_16_9,
};

enum colorimetry {
	COLORIMETRY_UNKNOWN,
	COLORIMETRY_BT601,
	COLORIMETRY_BT709,
	COLORIMETRY_EXTENDED,
};

enum non_uniform_picture_scaling {
	NON_UNIFORM_PICTURE_SCALING_NONE,
	NON_UNIFORM_PICTURE_SCALING_HORIZONTAL,
	NON_UNIFORM_PICTURE_SCALING_VERTICAL,
	NON_UNIFORM_PICTURE_SCALING_BOTH,
};

enum quantization_range {
	QUANTIZATION_RANGE_DEFAULT,
	QUANTIZATION_RANGE_LIMITED,
	QUANTIZATION_RANGE_FULL,
};

enum extended_colorimetry {
	EXTENDED_COLORIMETRY_BT601,
	EXTENDED_COLORIMETRY_BT709,
};

enum video_format {
	VIDEO_FORMAT_UNKNOWN,
};


struct sii9022_tx {
	struct i2c_client *client;
	struct notifier_block fb;

	/* sink information */
	bool enable_audio;
	enum {
		CONNECTION_TYPE_DVI,
		CONNECTION_TYPE_HDMI,
	} connection_type;
	enum {
		PIXEL_MAPPING_EXACT,
		PIXEL_MAPPING_UNDERSCANED,
		PIXEL_MAPPING_OVERSCANNED,
	} pixel_mapping;
};


static struct fb_videomode sii9022_default_video_mode = {
	.name         = "1280x720@60 (720p CEA Mode 4)",
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


static int sii9022_detect_revision(struct sii9022_tx *tx)
{
	u8 data;
	char device[80];
	size_t offset = 0;
	int retries = 5;

	/* TODO convert retries to timeout */

	do {
		data = i2c_smbus_read_byte_data(tx->client,
						SII9022_TPI_REG_DEVICE_ID);
	} while (data != SII9022_DEVICE_ID_902x && --retries > 0);

	if (data != SII9022_DEVICE_ID_902x)
		return -ENODEV;

	offset += snprintf(device + offset, sizeof(device) - offset,
			   "Device ID: %#02x", data);

	data = i2c_smbus_read_byte_data(tx->client,
					SII9022_TPI_REG_DEVICE_REVISION);
	if (data)
		offset += snprintf(device + offset, sizeof(device) - offset,
				   " (rev %01u.%01u)",
				   (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SII9022_TPI_REG_TPI_REVISION);
	offset += snprintf(device + offset, sizeof(device) - offset,
			   " (%s", data & (1 << 7) ? "Virtual" : "");
	data &= ~(1 << 7);
	data = data ? data : SII9022_BASE_TPI_REVISION;
	offset += snprintf(device + offset, sizeof(device) - offset,
			   "TPI revision %01u.%01u)",
			   (data >> 4) & 15, data & 15);

	data = i2c_smbus_read_byte_data(tx->client,
					SII9022_TPI_REG_HDCP_REVISION);
	if (data)
		offset += snprintf(device + offset, sizeof(device) - offset,
				   " (HDCP version %01u.%01u)",
				   (data >> 4) & 15, data & 15);

	device[offset] = '\0';
	INFO("%s\n", device);

	return 0;
}

static int sii9022_initialise(struct sii9022_tx *tx)
{
	int ret;

	/* step 1: reset and initialise */
	ret = i2c_smbus_write_byte_data(tx->client, SII9022_TPI_REG_RQB, 0x00);
	if (ret < 0) {
		DEBUG("unable to initialise device to TPI mode\n");
		return ret;
	}

	/* step 2: detect revision */
	if ((ret = sii9022_detect_revision(tx)) < 0) {
		DEBUG("unable to detect device revision\n");
		return ret;
	}

	/* step 3: power up transmitter */
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_PWR_STATE,
					SII9022_POWER_STATE_D0);
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
					SII9022_TPI_REG_IER,
					SII9022_IER_HOT_PLUG_EVENT |
					SII9022_IER_RECEIVER_SENSE_EVENT);
	if (ret < 0) {
		DEBUG("unable to setup interrupt request\n");
		return ret;
	}

	return 0;
}

static int sii9022_read_edid(struct sii9022_tx *tx, u8 *edid, size_t size)
{
	u8 offset, ctrl;
	int ret, retries = 5;

	/* TODO convert retries to timeout */

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
	ctrl = i2c_smbus_read_byte_data(tx->client, SII9022_TPI_REG_SYS_CTRL);

	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					ctrl | SII9022_SYS_CTRL_DDC_BUS_REQUEST);
	if (ret < 0) {
		DEBUG("unable to request DDC bus\n");
		return ret;
	}

	/* step 3: poll for bus grant */
	do {
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SII9022_TPI_REG_SYS_CTRL);
	} while (! (ctrl & SII9022_SYS_CTRL_DDC_BUS_GRANTED) && --retries > 0);

	/* step 4: take ownership of the DDC bus */
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					SII9022_SYS_CTRL_DDC_BUS_REQUEST |
					SII9022_SYS_CTRL_DDC_BUS_OWNER_HOST);
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
	retries = 5;

	do {
		i2c_smbus_write_byte_data(tx->client,
					  SII9022_TPI_REG_SYS_CTRL,
					  0x00);
		ctrl = i2c_smbus_read_byte_data(tx->client,
						SII9022_TPI_REG_SYS_CTRL);
	} while ((ctrl & SII9022_SYS_CTRL_DDC_BUS_GRANTED) && --retries > 0);

	/* step 7: (potentially) enable HDCP */

	return 0;
}

static void sii9022_detect_sink(struct sii9022_tx *tx, u8 *edid, size_t size)
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
				tx->connection_type = CONNECTION_TYPE_HDMI;
				tx->enable_audio = true;
			}

			if (ctb->underscan_supported)
				tx->pixel_mapping = PIXEL_MAPPING_UNDERSCANED;

		default:
			break;
		}
	}
}

static inline unsigned long sii9022_ps_to_hz(const unsigned long ps)
{
	unsigned long long numerator = 1000000000000ull;

	if (ps == 0)
		return 0;

	/* freq = 1 / time; 10^12 ps = 1s; 10^12/ps = freq */
	do_div(numerator, ps);

	return (unsigned long) numerator;
}

static void sii9022_set_vmode_registers(struct sii9022_tx *tx,
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

	pixclk = sii9022_ps_to_hz(var->pixclock);
	htotal = var->xres + var->left_margin + var->hsync_len + var->right_margin;
	vtotal = var->yres + var->upper_margin + var->vsync_len + var->lower_margin;
	refresh = (htotal * vtotal) / pixclk;

	/* basic video mode data */
	vmode[PIXEL_CLOCK]  = pixclk / 10000;
	vmode[REFRESH_RATE] = refresh;
	vmode[X_RESOLUTION] = htotal;
	vmode[Y_RESOLUTION] = vtotal;

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SII9022_TPI_REG_VIDEO_MODE_DATA_BASE,
					     sizeof(vmode),
					     (u8 *) vmode);
	if (ret < 0)
		DEBUG("unable to write video mode data\n");

	/* input format */
	format = SII9022_INPUT_COLOR_SPACE_RGB            |
		 SII9022_INPUT_VIDEO_RANGE_EXPANSION_AUTO |
		 SII9022_INPUT_COLOR_DEPTH_8BIT;

	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_AVI_INPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set input format\n");

	/* output format */
	format = SII9022_OUTPUT_VIDEO_RANGE_COMPRESSION_AUTO |
		 SII9022_OUTPUT_COLOR_STANDARD_BT601         |
		 SII9022_OUTPUT_COLOR_DEPTH_8BIT;

	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		format |= SII9022_OUTPUT_FORMAT_HDMI_RGB;
	else
		format |= SII9022_OUTPUT_FORMAT_DVI_RGB;

	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_AVI_OUTPUT_FORMAT,
					format);
	if (ret < 0)
		DEBUG("unable to set output format\n");
}

static void sii9022_checksum_hdmi_info_packet(const enum info_frame_type type,
					      const u8 version,
					      const u8 length,
					      u8 * const packet)
{
	u8 crc;
	int i;

	crc = (HDMI_PACKET_TYPE_INFO_FRAME + type) + version + (length - 1);
	for (i = 1; i < length; i++)
		crc += packet[i];

	packet[0] = HDMI_PACKET_CHECKSUM - crc;
}

static void sii9022_set_avi_info_frame(struct sii9022_tx *tx,
				       struct fb_var_screeninfo *var)
{
	struct avi_info_frame packet;
	int ret;

	BUILD_BUG_ON(sizeof(packet) != 14);

	/* TODO improve AVI InfoFrame we send */

	switch (tx->pixel_mapping)
	{
	case PIXEL_MAPPING_UNDERSCANED:
		packet.scan_information = SCAN_INFORMATION_UNDERSCANNED;
		break;
	case PIXEL_MAPPING_OVERSCANNED:
		packet.scan_information = SCAN_INFORMATION_OVERSCANNED;
		break;
	default:
		packet.scan_information = SCAN_INFORMATION_UNKNOWN;
		break;
	}

	packet.active_format_info_valid = true;
	packet.active_format_description = ACTIVE_FORMAT_DESCRIPTION_UNSCALED;

	packet.picture_aspect_ratio = PICTURE_ASPECT_RATIO_UNSCALED;

	packet.video_format = VIDEO_FORMAT_UNKNOWN;

	sii9022_checksum_hdmi_info_packet(INFO_FRAME_TYPE_AUXILIARY_VIDEO,
					  SII9022_AVI_INFO_FRAME_VERSION,
					  sizeof(packet),
					  (u8 *) &packet);

	ret = i2c_smbus_write_i2c_block_data(tx->client,
					     SII9022_TPI_REG_AVI_INFO_FRAME_BASE,
					     sizeof(packet),
					     (u8 *) &packet);
	if (ret < 0)
		DEBUG("unable to write avi info frame\n");
}

static int sii9022_set_resolution(struct sii9022_tx *tx,
				  struct fb_var_screeninfo *var)
{
	u8 ctrl;
	int ret;

	INFO("Setting Resolution: %dx%d\n", var->xres, var->yres);

	ctrl = i2c_smbus_read_byte_data(tx->client, SII9022_TPI_REG_SYS_CTRL);

	/* setup the sink type */
	if (tx->connection_type == CONNECTION_TYPE_DVI)
		ctrl &= ~SII9022_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;
	else
		ctrl |= SII9022_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI;

	/* step 1: (potentially) disable HDCP */

	/* step 2: (optionally) blank the display */
	/*
	 * Note that if we set the AV Mute, switching to DVI could result in a
	 * permanently muted display until a hardware reset.  Thus only do this
	 * if the sink is a HDMI connection
	 */
	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		ctrl |= SII9022_SYS_CTRL_AV_MUTE_HDMI;
	/* optimisation: merge the write into the next one */

	/* step 3: prepare for resolution change */
	ctrl |= SII9022_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to prepare for resolution change\n");

	msleep(SII9022_CTRL_INFO_FRAME_DRAIN_TIME);

	/* step 4: change video resolution */

	/* step 5: set the vmode registers */
	sii9022_set_vmode_registers(tx, var);

	/* step 6: [HDMI] set AVI InfoFrame */
	/* NOTE this is required as it flushes the vmode registers */
	sii9022_set_avi_info_frame(tx, var);

	/* step 7: [HDMI] set new audio information */

	/* step 8: enable display */
	ctrl &= ~SII9022_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	/* optimisation: merge the write into the next one */

	/* step 9: (optionally) un-blank the display */
	ctrl &= ~SII9022_SYS_CTRL_AV_MUTE_HDMI;
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to enable the display\n");

	/* step 10: (potentially) enable HDCP */

	return ret;
}

static bool sii9022_sink_present(struct sii9022_tx *tx)
{
	u8 isr;

	isr = i2c_smbus_read_byte_data(tx->client, SII9022_TPI_REG_ISR);

	return (isr & (SII9022_ISR_HOT_PLUG_EVENT |
		       SII9022_ISR_RECEIVER_SENSE_EVENT));
}

static void sii9022_dump_modelines(const struct fb_monspecs * const monspecs)
{
	u32 i;

	INFO("%u Supported Modelines:\n", monspecs->modedb_len);
	for (i = 0; i < monspecs->modedb_len; i++) {
		const struct fb_videomode * const mode = &monspecs->modedb[i];
		const bool interlaced = (mode->vmode & FB_VMODE_INTERLACED);
		const bool double_scan = (mode->vmode & FB_VMODE_DOUBLE);
		u32 pixel_clock = sii9022_ps_to_hz(mode->pixclock);

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

static int sii9022_init_fb(struct sii9022_tx *tx, struct fb_info *fb)
{
	u8 edid[EEDID_BASE_LENGTH];
	const struct fb_videomode *mode = NULL;
	struct fb_var_screeninfo var = {0};
	int ret;

	/* TODO use platform_data to prune modelist */

	if ((ret = sii9022_read_edid(tx, edid, sizeof(edid))) < 0)
		return ret;

	sii9022_detect_sink(tx, edid, sizeof(edid));

	fb_edid_to_monspecs(edid, &fb->monspecs);
	sii9022_dump_modelines(&fb->monspecs);
	/* TODO mxcfb_videomode_to_modelist did some additional work */
	fb_videomode_to_modelist(fb->monspecs.modedb,
				 fb->monspecs.modedb_len,
				 &fb->modelist);

	/* TODO mxcfb_update_default_var did some additional work */
	mode = fb_find_nearest_mode(&sii9022_default_video_mode, &fb->modelist);
	mode = mode ? mode : &sii9022_default_video_mode;

	/* TODO mxcfb_adjust did some additional work */
	fb_videomode_to_var(&var, mode);
	if ((ret = sii9022_set_resolution(tx, &var)) < 0)
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

static int sii9022_blank(struct sii9022_tx *tx, struct fb_var_screeninfo *var)
{
	u8 ctrl;
	int ret;

	/* cheap blanking: Video Black Mode */
	ret = i2c_smbus_write_byte_data(tx->client, SII9022_TPI_REG_RQB, 0x20);

#if 0
	ctrl = i2c_smbus_read_byte_data(tx->client, SII9022_TPI_REG_SYS_CTRL);

	if (tx->connection_type == CONNECTION_TYPE_HDMI)
		ctrl |= SII9022_SYS_CTRL_AV_MUTE_HDMI;

	/* step 3: prepare for resolution change */
	ctrl |= SII9022_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to prepare blank\n");

	msleep(SII9022_CTRL_INFO_FRAME_DRAIN_TIME);
#endif
	return ret;
}

static int sii9022_unblank(struct sii9022_tx *tx, struct fb_var_screeninfo *var)
{
	u8 ctrl;
	int ret;

	/* cheap blanking: Video Black Mode */
	ret = i2c_smbus_write_byte_data(tx->client, SII9022_TPI_REG_RQB, 0x00);

#if 0
	sii9022_set_vmode_registers(tx, var);
	sii9022_set_avi_info_frame(tx, var);
	ctrl &= ~SII9022_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN;
	ctrl &= ~SII9022_SYS_CTRL_AV_MUTE_HDMI;
	ret = i2c_smbus_write_byte_data(tx->client,
					SII9022_TPI_REG_SYS_CTRL,
					ctrl);
	if (ret < 0)
		DEBUG("unable to enable the display\n");
#endif
	return ret;
}


static int sii9022_fb_event_handler(struct notifier_block *nb,
				    unsigned long val,
				    void *v)
{
	struct fb_event *event = v;
	struct sii9022_tx *tx = container_of(nb, struct sii9022_tx, fb);

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
		return sii9022_init_fb(tx, event->info);
	case FB_EVENT_MODE_CHANGE:
	{
		struct fb_var_screeninfo var = {0};

		fb_videomode_to_var(&var, event->info->mode);
		return sii9022_set_resolution(tx, &var);
	}
	break;
	case FB_EVENT_BLANK:
	{
		struct fb_var_screeninfo var = {0};
		fb_videomode_to_var(&var, event->info->mode);

		if (*((int *)event->data) == FB_BLANK_UNBLANK)
			return sii9022_unblank(tx, &var);
		else
			return sii9022_blank(tx, &var);
	}
	break;
	default:
		DEBUG("unhandled fb event 0x%lx", val);
		break;
	}

	return 0;
}

static int __devinit sii9022_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct sii9022_tx *tx;
	int i, ret;

	tx = kmalloc(sizeof(*tx), GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	memset(tx, 0, sizeof(*tx));
	tx->client = client;

	i2c_set_clientdata(client, tx);

	/* initialise the device */
	if ((ret = sii9022_initialise(tx)) < 0)
		goto error;

	if (sii9022_sink_present(tx)) {
		/* enable any existing framebuffers */
		DEBUG("%d registered framebuffers\n", num_registered_fb);
		for (i = 0; i < num_registered_fb; i++)
			if ((ret = sii9022_init_fb(tx, registered_fb[i])) < 0)
				goto error;
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
						SII9022_TPI_REG_PWR_STATE,
						SII9022_POWER_STATE_D2);
		if (ret < 0)
			DEBUG("unable to change to a low power-state\n");
	}

	/* register a notifier for future fb events */
	tx->fb.notifier_call = sii9022_fb_event_handler;
	fb_register_client(&tx->fb);

	return 0;

error:
	kfree(tx);
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int __devexit sii9022_remove(struct i2c_client *client)
{
	struct sii9022_tx *tx;

	tx = i2c_get_clientdata(client);
	if (tx) {
		fb_unregister_client(&tx->fb);
		kfree(tx);
		i2c_set_clientdata(client, NULL);
	}

	return 0;
}

static const struct i2c_device_id sii9022_device_table[] = {
	{ "sii9022", 0 },
	{ },
};

static struct i2c_driver sii9022_driver = {
	.driver   = { .name = "sii9022" },
	.probe    = sii9022_probe,
	.remove   = sii9022_remove,
	.id_table = sii9022_device_table,
};

static int __init sii9022_init(void)
{
	return i2c_add_driver(&sii9022_driver);
}

static void __exit sii9022_exit(void)
{
	i2c_del_driver(&sii9022_driver);
}

/* Module Information */
MODULE_AUTHOR("Saleem Abdulrasool <compnerd@compnerd.org>");
MODULE_LICENSE("BSD-3");
MODULE_DESCRIPTION("SiI9022 TMDS Driver");
MODULE_DEVICE_TABLE(i2c, sii9022_device_table);

module_init(sii9022_init);
module_exit(sii9022_exit);

