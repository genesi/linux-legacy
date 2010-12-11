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

#include <linux/cea861.h>

/* TPI registers */
#define SIIHDMI_TPI_REG_VIDEO_MODE_DATA_BASE		(0x00)
#define SIIHDMI_TPI_REG_PIXEL_CLOCK_LSB			(0x00)
#define SIIHDMI_TPI_REG_PIXEL_CLOCK_MSB			(0x01)
#define SIIHDMI_TPI_REG_VFREQ_LSB			(0x02)
#define SIIHDMI_TPI_REG_VFREQ_MSB			(0x03)
#define SIIHDMI_TPI_REG_PIXELS_LSB			(0x04)
#define SIIHDMI_TPI_REG_PIXELS_MSB			(0x05)
#define SIIHDMI_TPI_REG_LINES_LSB			(0x06)
#define SIIHDMI_TPI_REG_LINES_MSB			(0x07)
#define SIIHDMI_TPI_REG_INPUT_BUS_PIXEL_REPETITION	(0x08)
#define SIIHDMI_TPI_REG_AVI_INPUT_FORMAT		(0x09)
#define SIIHDMI_TPI_REG_AVI_OUTPUT_FORMAT		(0x0a)
#define SIIHDMI_TPI_REG_YC_INPUT_MODE			(0x0b)
#define SIIHDMI_TPI_REG_AVI_INFO_FRAME_BASE		(0x0c)
#define SIIHDMI_TPI_REG_AVI_DBYTE0			(0x0c)
#define SIIHDMI_TPI_REG_AVI_DBYTE1			(0x0d)
#define SIIHDMI_TPI_REG_AVI_DBYTE2			(0x0e)
#define SIIHDMI_TPI_REG_AVI_DBYTE3			(0x0f)
#define SIIHDMI_TPI_REG_AVI_DBYTE4			(0x10)
#define SIIHDMI_TPI_REG_AVI_DBYTE5			(0x11)
#define SIIHDMI_TPI_REG_AVI_INFO_END_TOP_BAR_LSB	(0x12)
#define SIIHDMI_TPI_REG_AVI_INFO_END_TOP_BAR_MSB	(0x13)
#define SIIHDMI_TPI_REG_AVI_INFO_START_BOTTOM_BAR_LSB	(0x14)
#define SIIHDMI_TPI_REG_AVI_INFO_START_BOTTOM_BAR_MSB	(0x15)
#define SIIHDMI_TPI_REG_AVI_INFO_END_LEFT_BAR_LSB	(0x16)
#define SIIHDMI_TPI_REG_AVI_INFO_END_LEFT_BAR_MSB	(0x17)
#define SIIHDMI_TPI_REG_AVI_INFO_END_RIGHT_BAR_LSB	(0x18)
#define SIIHDMI_TPI_REG_AVI_INFO_END_RIGHT_BAR_MSB	(0x19)
#define SIIHDMI_TPI_REG_SYS_CTRL			(0x1a)
#define SIIHDMI_TPI_REG_DEVICE_ID			(0x1b)
#define SIIHDMI_TPI_REG_DEVICE_REVISION			(0x1c)
#define SIIHDMI_TPI_REG_TPI_REVISION			(0x1d)
#define SIIHDMI_TPI_REG_PWR_STATE			(0x1e)
#define SIIHDMI_TPI_REG_I2S_ENABLE_MAPPING		(0x1f)
#define SIIHDMI_TPI_REG_I2S_IINPUT_CONFIGURATAION	(0x20)
#define SIIHDMI_TPI_REG_I2S_STREAM_HEADER_SETTINGS_BASE	(0x21)
#define SIIHDMI_TPI_REG_I2S_CHANNEL_STATUS		(0x21)
#define SIIHDMI_TPI_REG_I2S_CATEGORY_CODE		(0x22)
#define SIIHDMI_TPI_REG_I2S_SOURCE_CHANNEL		(0x23)
#define SIIHDMI_TPI_REG_I2S_ACCURACY_SAMPLING_FREQUENCY	(0x24)
#define SIIHDMI_TPI_REG_I2S_ORIGINAL_FREQ_SAMPLE_LENGTH	(0x25)
#define SIIHDMI_TPI_REG_I2S_AUDIO_PACKET_LAYOUT_CTRL	(0x26)
#define SIIHDMI_TPI_REG_I2S_AUDIO_SAMPLING_HBR		(0x27)
#define SIIHDMI_TPI_REG_I2S_AUDIO_RESERVED		(0x28)
#define SIIHDMI_TPI_REG_HDCP_QUERY_DATA			(0x29)
#define SIIHDMI_TPI_REG_HDCP_CONTROL_DATA		(0x2a)
#define SIIHDMI_TPI_REG_HDCP_CONTROL_DATA		(0x2a)
#define SIIHDMI_TPI_REG_HDCP_BKSV_1			(0x2b)
#define SIIHDMI_TPI_REG_HDCP_BKSV_2			(0x2c)
#define SIIHDMI_TPI_REG_HDCP_BKSV_3			(0x2d)
#define SIIHDMI_TPI_REG_HDCP_BKSV_4			(0x2e)
#define SIIHDMI_TPI_REG_HDCP_BKSV_5			(0x2f)
#define SIIHDMI_TPI_REG_HDCP_REVISION			(0x30)
#define SIIHDMI_TPI_REG_HDCP_KSV_V_STAR_VALUE		(0x31)
#define SIIHDMI_TPI_REG_HDCP_V_STAR_VALUE_1		(0x32)
#define SIIHDMI_TPI_REG_HDCP_V_STAR_VALUE_2		(0x33)
#define SIIHDMI_TPI_REG_HDCP_V_STAR_VALUE_3		(0x34)
#define SIIHDMI_TPI_REG_HDCP_V_STAR_VALUE_4		(0x35)
#define SIIHDMI_TPI_REG_HDCP_AKSV_1			(0x36)
#define SIIHDMI_TPI_REG_HDCP_AKSV_2			(0x37)
#define SIIHDMI_TPI_REG_HDCP_AKSV_3			(0x38)
#define SIIHDMI_TPI_REG_HDCP_AKSV_4			(0x39)
#define SIIHDMI_TPI_REG_HDCP_AKSV_5			(0x3a)

#define SIIHDMI_TPI_REG_IER				(0x3c)
#define SIIHDMI_TPI_REG_ISR				(0x3d)

#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_BASE		(0xbf)
#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_TYPE		(0xc0)
#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_VERSION		(0xc1)
#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_LENGTH		(0xc2)
#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_CHECKSUM	(0xc3)
#define SIIHDMI_TPI_REG_MISC_INFO_FRAME_DATA0		(0xc4)

#define SIIHDMI_TPI_REG_RQB				(0xc7)

/* Input Bus and Pixel Repetition */
#define SIIHDMI_PIXEL_REPETITION_DUAL			(1 << 0)
#define SIIHDMI_PIXEL_REPETITION_QUAD			(3 << 0)
#define SIIHDMI_INPUT_BUS_EDGE_SELECT_RISING		(1 << 4)
#define SIIHDMI_INPUT_BUS_SELECT_FULL_PIXEL_WIDTH	(1 << 5)
#define SIIHDMI_INPUT_BUS_TMDS_CLOCK_RATIO_1X		(1 << 6)
#define SIIHDMI_INPUT_BUS_TMDS_CLOCK_RATIO_2X		(2 << 6)
#define SIIHDMI_INPUT_BUS_TMDS_CLOCK_RATIO_4X		(3 << 6)

/* Input Format */
#define SIIHDMI_INPUT_COLOR_SPACE_RGB			(0 << 0)
#define SIIHDMI_INPUT_COLOR_SPACE_YUV_444		(1 << 0)
#define SIIHDMI_INPUT_COLOR_SPACE_YUV_422		(2 << 0)
#define SIIHDMI_INPUT_COLOR_SPACE_BLACK			(3 << 0)
#define SIIHDMI_INPUT_VIDEO_RANGE_EXPANSION_AUTO	(0 << 2)
#define SIIHDMI_INPUT_VIDEO_RANGE_EXPANSION_ON		(1 << 2)
#define SIIHDMI_INPUT_VIDEO_RANGE_EXPANSION_OFF		(2 << 2)
#define SIIHDMI_INPUT_COLOR_DEPTH_8BIT			(0 << 6)
#define SIIHDMI_INPUT_COLOR_DEPTH_10BIT			(2 << 6)
#define SIIHDMI_INPUT_COLOR_DEPTH_12BIT			(3 << 6)
#define SIIHDMI_INPUT_COLOR_DEPTH_16BIT			(1 << 6)

/* Output Format */
#define SIIHDMI_OUTPUT_FORMAT_HDMI_RGB			(0 << 0)
#define SIIHDMI_OUTPUT_FORMAT_HDMI_YUV_444		(1 << 0)
#define SIIHDMI_OUTPUT_FORMAT_HDMI_YUV_422		(2 << 0)
#define SIIHDMI_OUTPUT_FORMAT_DVI_RGB			(3 << 0)
#define SIIHDMI_OUTPUT_VIDEO_RANGE_CO/* driver constants */
#define SIIHDMI_DEVICE_ID_902x				(0xb0)
#define SIIHDMI_BASE_TPI_REVISION			(0x29)
#define SIIHDMI_CTRL_INFO_FRAME_DRAIN_TIME		(0x80)
#define SIIHDMI_OUTPUT_VIDEO_RANGE_COMPRESSION_AUTO	(0 << 2)
#define SIIHDMI_OUTPUT_VIDEO_RANGE_COMPRESSION_ON	(1 << 2)
#define SIIHDMI_OUTPUT_VIDEO_RANGE_COMPRESSION_OFF	(2 << 2)
#define SIIHDMI_OUTPUT_COLOR_STANDARD_BT601		(0 << 4)
#define SIIHDMI_OUTPUT_COLOR_STANDARD_BT709		(1 << 4)
#define SIIHDMI_OUTPUT_DITHERING			(1 << 5)
#define SIIHDMI_OUTPUT_COLOR_DEPTH_8BIT			(0 << 6)
#define SIIHDMI_OUTPUT_COLOR_DEPTH_10BIT		(2 << 6)
#define SIIHDMI_OUTPUT_COLOR_DEPTH_12BIT		(3 << 6)
#define SIIHDMI_OUTPUT_COLOR_DEPTH_16BIT		(1 << 6)

/* System Control */
#define SIIHDMI_SYS_CTRL_OUTPUT_MODE_SELECT_HDMI	(1 << 0)
#define SIIHDMI_SYS_CTRL_DDC_BUS_OWNER_HOST		(1 << 1)
#define SIIHDMI_SYS_CTRL_DDC_BUS_GRANTED		(1 << 1)
#define SIIHDMI_SYS_CTRL_DDC_BUS_REQUEST		(1 << 2)
#define SIIHDMI_SYS_CTRL_AV_MUTE_HDMI			(1 << 3)
#define SIIHDMI_SYS_CTRL_TMDS_OUTPUT_POWER_DOWN		(1 << 4)
#define SIIHDMI_SYS_CTRL_DYNAMIC_LINK_INTEGRITY		(1 << 6)

/* Device Power State Control Data */
#define SIIHDMI_POWER_STATE_D0				(0 << 0)
#define SIIHDMI_POWER_STATE_D1				(1 << 0)
#define SIIHDMI_POWER_STATE_D2				(2 << 0)
#define SIIHDMI_POWER_STATE_D3				(3 << 0)
#define SIIHDMI_WAKEUP_STATE_COLD			(1 << 2)

/* I²S Enable and Mapping */
#define SIIHDMI_I2S_MAPPING_SELECT_SD_CHANNEL_0		(0 << 0)
#define SIIHDMI_I2S_MAPPING_SELECT_SD_CHANNEL_1		(1 << 0)
#define SIIHDMI_I2S_MAPPING_SELECT_SD_CHANNEL_2		(2 << 0)
#define SIIHDMI_I2S_MAPPING_SELECT_SD_CHANNEL_3		(3 << 0)
#define SIIHDMI_I2S_MAPPING_SWAP_CHANNELS		(1 << 3)
#define SIIHDMI_I2S_ENABLE_BASIC_AUDIO_DOWNSAMPLING	(1 << 4)
#define SIIHDMI_I2S_MAPPING_SELECT_FIFO_0		(0 << 5)
#define SIIHDMI_I2S_MAPPING_SELECT_FIFO_1		(1 << 5)
#define SIIHDMI_I2S_MAPPING_SELECT_FIFO_2		(2 << 5)
#define SIIHDMI_I2S_MAPPING_SELECT_FIFO_3		(3 << 5)
#define SIIHDMI_I2S_ENABLE_SELECTED_FIFO		(1 << 7)

/* I²S Input Configuration */
#define SIIHDMI_I2S_WS_TO_SD_FIRST_BIT_SHIFT		(1 << 0)
#define SIIHDMI_I2S_SD_LSB_FIRST			(1 << 1)
#define SIIHDMI_I2S_SD_RIGHT_JUSTIFY_DATA		(1 << 2)
#define SIIHDMI_I2S_WS_POLARITY_HIGH			(1 << 3)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_128			(0 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_256			(1 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_384			(2 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_512			(3 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_768			(4 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_1024		(5 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_1152		(6 << 4)
#define SIIHDMI_I2S_MCLK_MULTIPLIER_192			(7 << 4)
#define SIIHDMI_I2S_SCK_SAMPLE_RISING_EDGE		(1 << 7)

/* Interrupt Enable */
#define SIIHDMI_IER_HOT_PLUG_EVENT			(1 << 0)
#define SIIHDMI_IER_RECEIVER_SENSE_EVENT		(1 << 1)
#define SIIHDMI_IER_CTRL_BUS_EVENT			(1 << 2)
#define SIIHDMI_IER_CPI_EVENT				(1 << 3)
#define SIIHDMI_IER_AUDIO_EVENT				(1 << 4)
#define SIIHDMI_IER_SECURITY_STATUS_CHANGE		(1 << 5)
#define SIIHDMI_IER_HDCP_VALUE_READY			(1 << 6)
#define SIIHDMI_IER_HDCP_AUTHENTICATION_STATUS_CHANGE	(1 << 7)

/* Interrupt Status */
#define SIIHDMI_ISR_HOT_PLUG_EVENT			(1 << 0)
#define SIIHDMI_ISR_RECEIVER_SENSE_EVENT		(1 << 1)
#define SIIHDMI_ISR_CTRL_BUS_EVENT			(1 << 2)
#define SIIHDMI_ISR_CPI_EVENT				(1 << 3)
#define SIIHDMI_ISR_AUDIO_EVENT				(1 << 4)
#define SIIHDMI_ISR_SECURITY_STATUS_CHANGED		(1 << 5)
#define SIIHDMI_ISR_HDCP_VALUE_READY			(1 << 6)
#define SIIHDMI_ISR_HDCP_AUTHENTICATION_STATUS_CHANGED	(1 << 7)

/* Request/Grant/Black Mode */
#define SIIHDMI_RQB_DDC_BUS_REQUEST			(1 << 0)
#define SIIHDMI_RQB_DDC_BUS_GRANTED			(1 << 1)
#define SIIHDMI_RQB_I2C_ACCESS_DDC_BUS			(1 << 2)
#define SIIHDMI_RQB_FORCE_VIDEO_BLANK			(1 << 5)
#define SIIHDMI_RQB_TPI_MODE_DISABLE			(1 << 7)

/* driver constants */
#define SIIHDMI_DEVICE_ID_902x				(0xb0)
#define SIIHDMI_BASE_TPI_REVISION			(0x29)
#define SIIHDMI_CTRL_INFO_FRAME_DRAIN_TIME		(0x80)

/* SII HDMI chips have two ways to send HDMI InfoFrames - AVI is sent via a dedicated
 * block of registers while the rest are sent using a common set with a small configruation
 * header. However, the buffer types do not exactly correspond to the ones in the CEA spec
 * so they need to be redefined here for the buffer header
 */
enum sii_info_frame_type {
	INFO_FRAME_BUFFER_NONE,
	INFO_FRAME_BUFFER_SPD,
	INFO_FRAME_BUFFER_AUDIO,
	INFO_FRAME_BUFFER_MPEG_GBD_VSIF,
	INFO_FRAME_BUFFER_GENERIC1,
	INFO_FRAME_BUFFER_GENERIC2,
	INFO_FRAME_BUFFER_DEDICATED_GBD, /* SII9136, SII9334 only, for 3D support */
};

#if defined(__LITTLE_ENDIAN_BITFIELD)
struct __packed info_frame_buffer_header {
	unsigned buffer_type		: 3;
	unsigned buffer_rsvd		: 3;
	unsigned repeat			: 1;
	unsigned enable			: 1;
};
#else
struct __packed info_frame_buffer_header {
	unsigned enable			: 1;
	unsigned repeat			: 1;
	unsigned buffer_rsvd		: 3;
	unsigned buffer_type		: 3;
};
#endif

/* AVI InfoFrame is special so we skip type, version and length when we send it to the
 * chip to fill the registers. This is the offset inside the standard infoframe struct
 * that we send (encompassing type, version and length). The sent data starts at checksum
 * which is "byte 3".
 */
#define SIIHDMI_AVI_INFO_FRAME_OFFSET 3

#define SIIHDMI_INFO_FRAME_BUFFER_LENGTH 31
struct siihdmi_spd_info_frame {
	struct info_frame_buffer_header config;
	struct spd_info_frame spd;
	u8	padding[7];
};

#define SIIHDMI_AUDIO_INFO_FRAME_BUFFER_LENGTH 14
struct siihdmi_audio_info_frame {
	struct info_frame_buffer_header config;
	struct audio_info_frame audio;
	u8     padding[4];
};

struct siihdmi_tx {
	struct i2c_client *client;
	struct notifier_block nb;

	/* sink information */
	bool enable_audio;
	enum {
		CONNECTION_TYPE_DVI,
		CONNECTION_TYPE_HDMI,
	} connection_type;
	enum {
		PIXEL_MAPPING_EXACT,
		PIXEL_MAPPING_UNDERSCANNED,
		PIXEL_MAPPING_OVERSCANNED,
	} pixel_mapping;
	bool tmds_state;
};




// audio infoframe padding
//	u8	padding[4]; /* SII9022 needs 14 bytes to send */
// spd infoframe padding
//	u8	padding[7]; 	/* SII9022 needs 31 bytes to trigger sending */
