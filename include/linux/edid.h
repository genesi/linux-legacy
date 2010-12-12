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

#ifndef LINUX_EDID_H
#define LINUX_EDID_H

/* EDID constants and Structures */
#define EDID_I2C_DDC_DATA_ADDRESS			(0x50)

#define EEDID_BASE_LENGTH				(0x100)

#define EEDID_EXTENSION_FLAG				(0x7e)

#define EEDID_EXTENSION_TAG				(0x80)
#define EEDID_EXTENSION_DATA_OFFSET			(0x80)

#define EEDID_CEA_VENDOR_SPECIFIC_TAG			(0x65)
#define EEDID_CEA_VENDOR_SPECIFIC_OFFSET		(0x94)

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

#endif

