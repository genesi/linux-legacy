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

#ifndef LINUX_CEA861_H
#define LINUX_CEA861_H

/* Header file for EIA CEA 861-E structures and definitions */

#if defined(__LITTLE_ENDIAN_BITFIELD)

/* EDID timing extension block */
struct __packed cea_timing_block {
	u8       extension_tag;
	u8       revision_number;
	u8       dtd_start_offset;
	unsigned dtd_block_count       : 4;
	unsigned yuv_422_supported     : 1;
	unsigned yuv_444_supported     : 1;
	unsigned basic_audio_supported : 1;
	unsigned underscan_supported   : 1;
	u8       dbc_start_offset;               // data block collection offset
};

struct __packed cea_dbc_header {
	unsigned length			: 5;
	unsigned block_type_tag		: 3;
};

#define CEA_DATA_BLOCK_TAG_AUDIO	(0x1)

enum cea_dbc_audio_format {
	CEA_AUDIO_FORMAT_RESERVED0,
	CEA_AUDIO_FORMAT_LPCM,
	CEA_AUDIO_FORMAT_AC3,
	CEA_AUDIO_FORMAT_MPEG1,
	CEA_AUDIO_FORMAT_MP3,
	CEA_AUDIO_FORMAT_MPEG2,
	CEA_AUDIO_FORMAT_AAC,
	CEA_AUDIO_FORMAT_DTS,
	CEA_AUDIO_FORMAT_ATRAC,
	CEA_AUDIO_FORMAT_SACD_1BIT,
	CEA_AUDIO_FORMAT_DOLBYDIGITALPLUS,
	CEA_AUDIO_FORMAT_DTSHD,
	CEA_AUDIO_FORMAT_MLP_DOLBYTRUEHD,
	CEA_AUDIO_FORMAT_DST_AUDIO,
	CEA_AUDIO_FORMAT_WMA_PRO,
	CEA_AUDIO_FORMAT_RESERVED15,
};

struct __packed cea_dbc_audio {
	unsigned channels		: 3;
	unsigned audio_format		: 4;
	unsigned audio_reserved		: 1;

	unsigned freq_32kHz		: 1;
	unsigned freq_44kHz		: 1;
	unsigned freq_48kHz		: 1;
	unsigned freq_88kHz		: 1;
	unsigned freq_96kHz		: 1;
	unsigned freq_176kHz		: 1;
	unsigned freq_192kHz		: 1;
	unsigned freq_reserved		: 1;

	union {
		struct {
			unsigned bits_16		: 1;
			unsigned bits_20		: 1;
			unsigned bits_24		: 1;
			unsigned bits_reserved		: 5;
		} lpcm_bits;
		u8 rate_div_8kbps;
	} bitrate;
};

#define CEA_DATA_BLOCK_TAG_VIDEO	(0x2)

struct __packed cea_dbc_video {
	unsigned cea_mode_index		: 7;
	unsigned native			: 1;
};

#define CEA_DATA_BLOCK_TAG_VENDOR	(0x3)

struct __packed cea_dbc_vendor {
	u8 ieee_identifier[3]; // 0x03 0x0C 0x00  == HDMI Licensing, LLC.
	u8 cec_address[2];

	unsigned dvi_dual_link		: 1;
	unsigned reserved		: 2;
	unsigned dc_y444		: 1;
	unsigned dc_30bit		: 1;
	unsigned dc_36bit		: 1;
	unsigned dc_48bit		: 1;
	unsigned supports_ai		: 1;

	u8 max_tmds_freqency; // divided by 5Mhz

	unsigned latency_reserved	: 6;
	unsigned latency_fields_il	: 1;
	unsigned latency_fields		: 1;

	u8 video_latency;
	u8 audio_latency;
	u8 video_latency_il;
	u8 audio_latency_il;
};

#define CEA_DATA_BLOCK_TAG_SPEAKER	(0x4)

struct __packed cea_dbc_speaker {

	unsigned front_lr_present	: 1; /* 2 channels */
	unsigned lfe_present		: 1; /* X.1 channels */
	unsigned front_center_present	: 1; /* standard center speaker */
	unsigned rear_lr_present	: 1; /* surround */
	unsigned rear_center_present	: 1; /* for 6.1 */
	unsigned fcenter_lr_present	: 1; /* for 7.1 */
	unsigned rcenter_lr_present	: 1; /* for 7.1 */
	unsigned speaker_reserved	: 1;

	u8 reserved[2];
};

/* HDMI Constants and Structures */
#define HDMI_PACKET_TYPE_INFO_FRAME			(0x80)
#define HDMI_PACKET_CHECKSUM				(0x100)

/* InfoFrame type constants */
enum info_frame_type {
	INFO_FRAME_TYPE_RESERVED,
	INFO_FRAME_TYPE_VENDOR_SPECIFIC,
	INFO_FRAME_TYPE_AUXILIARY_VIDEO_INFO,
	INFO_FRAME_TYPE_SOURCE_PRODUCT_DESCRIPTION,
	INFO_FRAME_TYPE_AUDIO,
	INFO_FRAME_TYPE_MPEG,
};

/* Common InfoFrame header information */
struct __packed info_frame_header {
	u8 type;
	u8 version;
	u8 length;
	u8 chksum;
};

/* AVI InfoFrame (v2) */
#define CEA861_AVI_INFO_FRAME_VERSION			(0x02)

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

/* quantization range are the same flag values for RGB and YCC */
enum quantization_range {
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

struct __packed avi_info_frame {
	struct info_frame_header header;

	unsigned scan_information            : 2;
	unsigned bar_info                    : 2;
	unsigned active_format_info_valid    : 1;
	unsigned pixel_format                : 2;
	unsigned dbyte1_reserved0            : 1;

	unsigned active_format_description   : 4;
	unsigned picture_aspect_ratio        : 2;
	unsigned colorimetry                 : 2;

	unsigned non_uniform_picture_scaling : 2;
	unsigned rgb_quantization_range      : 2;
	unsigned extended_colorimetry        : 3;
	unsigned it_content_present          : 1;

	unsigned video_format                : 7;
	unsigned dbyte4_reserved0            : 1;

	unsigned pixel_repetition_factor     : 4;       /* value - 1 */
	unsigned content_type                : 2;
	unsigned ycc_quantizaton_range       : 2;

	u16      end_of_top_bar;
	u16      start_of_bottom_bar;
	u16      end_of_left_bar;
	u16      start_of_right_bar;
};


/* SPD InfoFrame */
#define CEA861_SPD_INFO_FRAME_VERSION			(0x01)

enum spd_source_information {
	SPD_SOURCE_UNKNOWN,
	SPD_SOURCE_DIGITAL_STB,
	SPD_SOURCE_DVD_PLAYER,
	SPD_SOURCE_D_VHS,
	SPD_SOURCE_HDD_VIDEORECORDER,
	SPD_SOURCE_DVC,
	SPD_SOURCE_DSC,
	SPD_SOURCE_VIDEOCD,
	SPD_SOURCE_GAME,
	SPD_SOURCE_PC_GENERAL,
	SPD_SOURCE_BLURAY_SACD,
	SPD_SOURCE_HD_DVD,
	SPD_SOURCE_PMP,
};

struct __packed spd_info_frame {
	struct info_frame_header header;

	u8     vendor[8];
	u8     description[16];
	u8     source_device_info;
};


/* Audio InfoFrame */
#define CEA861_AUDIO_INFO_FRAME_VERSION			(0x01)

enum audio_coding_type {
	CODING_TYPE_REFER_STREAM_HEADER,
	CODING_TYPE_PCM,
	CODING_TYPE_AC3,
	CODING_TYPE_MPEG1_LAYER12,
	CODING_TYPE_MP3,
	CODING_TYPE_MPEG2,
	CODING_TYPE_AAC_LC,
	CODING_TYPE_DTS,
	CODING_TYPE_ATRAC,
	CODING_TYPE_DSD,
	CODING_TYPE_E_AC3,
	CODING_TYPE_DTS_HD,
	CODING_TYPE_MLP,
	CODING_TYPE_DST,
	CODING_TYPE_WMA_PRO,
	CODING_TYPE_EXTENDED,
};

enum audio_sample_frequency {
	FREQUENCY_REFER_STREAM_HEADER,
	FREQUENCY_32_KHZ,
	FREQUENCY_44_1_KHZ,
	FREQUENCY_CD = FREQUENCY_44_1_KHZ,
	FREQUENCY_48_KHZ,
	FREQUENCY_88_2_KHZ,
	FREQUENCY_96_KHZ,
	FREQUENCY_176_4_KHZ,
	FREQUENCY_192_KHZ,
};

enum audio_sample_size {
	SAMPLE_SIZE_REFER_STREAM_HEADER,
	SAMPLE_SIZE_16_BIT,
	SAMPLE_SIZE_20_BIT,
	SAMPLE_SIZE_24_BIT,
};

enum audio_coding_extended_type {
/*	CODING TYPE_REFER_STREAM_HEADER */
	CODING_TYPE_HE_AAC = 1,
	CODING_TYPE_HE_AACv2,
	CODING_TYPE_MPEG_SURROUND,
};

/* TODO define speaker allocation bits */

#define CHANNEL_COUNT_REFER_STREAM_HEADER		(0x00)
#define CHANNEL_ALLOCATION_STEREO			(0x00)

enum audio_downmix {
	DOWNMIX_PERMITTED,
	DOWNMIX_PROHIBITED,
};

enum audio_lfe_level {
	LFE_LEVEL_UNKNOWN,
	LFE_LEVEL_0dB,
	LFE_LEVEL_PLUS10dB,
	LFE_LEVEL_RESERVED,
};

struct __packed audio_info_frame {
	struct {
		u8 type;
		u8 version;
		u8 length;
	} header;

	unsigned channel_count          : 3;
	unsigned future13               : 1;
	unsigned coding_type            : 4;

	unsigned sample_size            : 2;
	unsigned sample_frequency       : 3;
	unsigned future25               : 1;
	unsigned future26               : 1;
	unsigned future27               : 1;

	unsigned format_code_extension  : 5;
	unsigned future35               : 1;
	unsigned future36               : 1;
	unsigned future37               : 1;

	u8       channel_allocation;

	unsigned lfe_playback_level     : 2;
	unsigned future52               : 1;
	unsigned level_shift            : 4; /* 0-15dB */
	unsigned down_mix_inhibit       : 1;

	u8       future_byte_6_10[5];
};

#else /* == !__LITTLE_ENDIAN_BITFIELD */
#warning "structures defined and packed for little-endian byte order"
#endif

static inline void cea861_checksum_info_frame(u8 * const info_frame)
{
	struct info_frame_header * const header =
		(struct info_frame_header *) info_frame;

	int i;
	u8 crc;

	crc = (HDMI_PACKET_TYPE_INFO_FRAME + header->type) +
		header->version + (header->length - 1);

	for (i = 1; i < header->length; i++)
		crc += info_frame[i];

	header->chksum = HDMI_PACKET_CHECKSUM - crc;
}

#endif /* LINUX_CEA861_H */

