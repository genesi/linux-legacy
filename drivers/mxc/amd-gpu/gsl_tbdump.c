/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/string.h>

#include "gsl.h"
#include "gsl_tbdump.h"

#ifdef TBDUMP

typedef struct TBDump_
{
    void* file;
} TBDump;


static TBDump g_tb;
static struct mutex *tbdump_mutex = NULL;

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static void tbdump_printline(const char* format, ...)
{
    if(g_tb.file)
    {
        va_list va;
        va_start(va, format);
        vfprintf((FILE*)g_tb.file, format, va);
        va_end(va);
        fprintf((FILE*)g_tb.file, "\n");
    }
}

static void tbdump_printinfo(const char* message )
{
    tbdump_printline("15 %s", message);
}

static void tbdump_getmemhex(char* buffer, unsigned int addr, unsigned int sizewords)
{
    unsigned int i = 0;
    static const char* hexChars = "0123456789abcdef";
    unsigned char* ptr = (unsigned char*)addr;

    for (i = 0; i < sizewords; i++)
    {
        buffer[(sizewords - i) * 2 - 1] = hexChars[ptr[i] & 0x0f];
        buffer[(sizewords - i) * 2 - 2] = hexChars[ptr[i] >> 4];
    }
    buffer[sizewords * 2] = '\0';
}

/* ------------------------------------------------------------------------ */

void tbdump_open(char* filename)
{
    if( !tbdump_mutex ) {
	tbdump_mutex = (struct mutex *) kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (tbdump_mutex)
		mutex_init(tbdump_mutex);
    }

    memset( &g_tb, 0, sizeof( g_tb ) );

    g_tb.file = kos_fopen( filename, "wt" );

    tbdump_printinfo("reset");
    tbdump_printline("0");
    tbdump_printline("1 00000000 00000eff");

    /* Enable interrupts */
    tbdump_printline("1 00000000 00000003");
}

void tbdump_close()
{
    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    kos_fclose( g_tb.file );
    g_tb.file = 0;

    if( tbdump_mutex ) {
	mutex_unlock(tbdump_mutex);
	kfree( tbdump_mutex );
    }
}

/* ------------------------------------------------------------------------ */

void tbdump_syncmem(unsigned int addr, unsigned int src, unsigned int sizebytes)
{
    /* Align starting address and size */
    unsigned int beg = addr;
    unsigned int end = addr+sizebytes;
    char buffer[65];

    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    beg = (beg+15) & ~15;
    end &= ~15;

    if( sizebytes <= 16 )
    {
        tbdump_getmemhex(buffer, src, 16);

        tbdump_printline("19 %08x %i 1 %s", addr, sizebytes, buffer);

	    if (tbdump_mutex)
		mutex_unlock(tbdump_mutex);
        return;
    }

    /* Handle unaligned start */
    if( beg != addr )
    {
        tbdump_getmemhex(buffer, src, 16);

        tbdump_printline("19 %08x %i 1 %s", addr, beg-addr, buffer);

        src += beg-addr;
    }

    /* Dump the memory writes */
    while( beg < end )
    {
        tbdump_getmemhex(buffer, src, 16);

        tbdump_printline("2 %08x %s", beg, buffer);

        beg += 16;
        src += 16;
    }

    /* Handle unaligned end */
    if( end != addr+sizebytes )
    {
        tbdump_getmemhex(buffer, src, 16);

        tbdump_printline("19 %08x %i 1 %s", end, (addr+sizebytes)-end, buffer);
    }

    if (tbdump_mutex)
	mutex_unlock(tbdump_mutex);
}

/* ------------------------------------------------------------------------ */

void tbdump_setmem(unsigned int addr, unsigned int value, unsigned int sizebytes)
{
    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    tbdump_printline("19 %08x 4 %i %032x", addr, (sizebytes+3)/4, value );

    if (tbdump_mutex)
	mutex_unlock(tbdump_mutex);
}

/* ------------------------------------------------------------------------ */

void tbdump_slavewrite(unsigned int addr, unsigned int value)
{
    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    tbdump_printline("1 %08x %08x", addr, value);

    if (tbdump_mutex)
	mutex_unlock(tbdump_mutex);
}

/* ------------------------------------------------------------------------ */


int
kgsl_tbdump_waitirq()
{
    if(!g_tb.file) return GSL_FAILURE;

    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    tbdump_printinfo("wait irq");
    tbdump_printline("10");

    /* ACK IRQ */
    tbdump_printline("1 00000418 00000003");
    tbdump_printline("18 00000018 00000000 # slave read & assert");

    if (tbdump_mutex)
	mutex_unlock(tbdump_mutex);

    return GSL_SUCCESS;
}

/* ------------------------------------------------------------------------ */

int
kgsl_tbdump_exportbmp(const void* addr, unsigned int format, unsigned int stride, unsigned int width, unsigned int height)
{
    static char filename[20];
    static int numframe = 0;

    if(!g_tb.file) return GSL_FAILURE;

    if (tbdump_mutex)
	mutex_lock(tbdump_mutex);

    #pragma warning(disable:4996)
    sprintf( filename, "tbdump_%08d.bmp", numframe++ );

    tbdump_printline("13 %s %d %08x %d %d %d 0", filename, format, (unsigned int)addr, stride, width, height);

    if (tbdump_mutex)
	mutex_unlock(tbdump_mutex);

    return GSL_SUCCESS;
}

/* ------------------------------------------------------------------------ */

#endif /* TBDUMP */
