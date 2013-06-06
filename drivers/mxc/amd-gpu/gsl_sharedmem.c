/* Copyright (c) 2002,2007-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/sched.h>

#include "gsl.h"
#include "gsl_hal.h"

/////////////////////////////////////////////////////////////////////////////
// macros
//////////////////////////////////////////////////////////////////////////////

#define GSL_MEMDESC_DEVICE_SET(memdesc, device_id)  \
    memdesc->priv = (memdesc->priv & ~GSL_DEVICEID_MASK) | ((device_id << GSL_DEVICEID_SHIFT) & GSL_DEVICEID_MASK);

#define GSL_MEMDESC_EXTALLOC_SET(memdesc, flag) \
    memdesc->priv = (memdesc->priv & ~GSL_EXTALLOC_MASK) | ((flag << GSL_EXTALLOC_SHIFT) & GSL_EXTALLOC_MASK);

#define GSL_MEMDESC_DEVICE_GET(memdesc, device_id)                                              \
    DEBUG_ASSERT(memdesc);                                                                        \
    device_id = (gsl_deviceid_t)((memdesc->priv & GSL_DEVICEID_MASK) >> GSL_DEVICEID_SHIFT);    \
    DEBUG_ASSERT(device_id <= GSL_DEVICE_MAX);

#define GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc)  \
    ((memdesc->priv & GSL_EXTALLOC_MASK) >> GSL_EXTALLOC_SHIFT)



//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

int
kgsl_sharedmem_init(gsl_sharedmem_t *shmem)
{
    int                status;
    gsl_shmemconfig_t  config;
    int                mmu_virtualized;
    unsigned int       hostbaseaddr;
    gpuaddr_t          gpubaseaddr;
    int                sizebytes;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "--> int kgsl_sharedmem_init(gsl_sharedmem_t *shmem=0x%08x)\n", shmem );

    if (shmem->flags & GSL_FLAGS_INITIALIZED)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_init. Return value %B\n", GSL_SUCCESS );
        return (GSL_SUCCESS);
    }

    status = kgsl_hal_getshmemconfig(&config);
    if (status != GSL_SUCCESS)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Unable to get sharedmem config.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_init. Return value %B\n", status );
        return (status);
    }

    hostbaseaddr    = config.emem_hostbase;
    gpubaseaddr     = config.emem_gpubase;
    sizebytes       = config.emem_sizebytes;
    mmu_virtualized = 0;

    // make sure aligned to page size
    DEBUG_ASSERT((gpubaseaddr & ((1 << GSL_PAGESIZE_SHIFT) - 1)) == 0);

    // make a multiple of page size
    sizebytes = (sizebytes & ~((1 << GSL_PAGESIZE_SHIFT) - 1));

    if (sizebytes > 0)
    {
        shmem->memarena = kgsl_memarena_create(mmu_virtualized, hostbaseaddr, gpubaseaddr, sizebytes);

            if (!shmem->memarena)
            {
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Unable to allocate memarena.\n" );
                kgsl_sharedmem_close(shmem);
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_init. Return value %B\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
    }

    shmem->flags |= GSL_FLAGS_INITIALIZED;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_init. Return value %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_close(gsl_sharedmem_t *shmem)
{
    int  result = GSL_SUCCESS;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "--> int kgsl_sharedmem_close(gsl_sharedmem_t *shmem=0x%08x)\n", shmem );

    if (shmem->flags & GSL_FLAGS_INITIALIZED)
    {
            if (shmem->memarena)
            {
                result = kgsl_memarena_destroy(shmem->memarena);
            }

        memset(shmem, 0, sizeof(gsl_sharedmem_t));
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_close. Return value %B\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_alloc0(gsl_deviceid_t device_id, gsl_flags_t flags, int sizebytes, gsl_memdesc_t *memdesc)
{
    gsl_deviceid_t    tmp_id;
    int               result  = GSL_FAILURE;
    gsl_sharedmem_t   *shmem  = &gsl_driver.shmem;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_alloc(gsl_deviceid_t device_id=%D, gsl_flags_t flags=%x, int sizebytes=%d, gsl_memdesc_t *memdesc=%M)\n",
                    device_id, flags, sizebytes, memdesc );

    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc);

    memset(memdesc, 0, sizeof(gsl_memdesc_t));

    if (!(shmem->flags & GSL_FLAGS_INITIALIZED))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Shared memory not initialized.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_alloc. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    // execute pending device action
    tmp_id = (device_id != GSL_DEVICE_ANY) ? device_id : device_id+1;
    for ( ; tmp_id <= GSL_DEVICE_MAX; tmp_id++)
    {
        if (gsl_driver.device[tmp_id-1].flags & GSL_FLAGS_INITIALIZED)
        {
            kgsl_device_runpending(&gsl_driver.device[tmp_id-1]);

            if (tmp_id == device_id)
            {
                break;
            }
        }
    }

    // convert any device to an actual existing device
    if (device_id == GSL_DEVICE_ANY)
    {
        for ( ; ; )
        {
            device_id++;

            if (device_id <= GSL_DEVICE_MAX)
            {
                if (gsl_driver.device[device_id-1].flags & GSL_FLAGS_INITIALIZED)
                {
                    break;
                }
            }
            else
            {
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Invalid device.\n" );
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_alloc. Return value %B\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
        }
    }

    DEBUG_ASSERT(device_id > GSL_DEVICE_ANY && device_id <= GSL_DEVICE_MAX);

    result = kgsl_memarena_alloc(shmem->memarena, flags, sizebytes, memdesc);

    KGSL_DEBUG_TBDUMP_SETMEM( memdesc->gpuaddr, 0, memdesc->size );

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_alloc. Return value %B\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_alloc(gsl_deviceid_t device_id, gsl_flags_t flags, int sizebytes, gsl_memdesc_t *memdesc)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_alloc0(device_id, flags, sizebytes, memdesc);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_free0(gsl_memdesc_t *memdesc, unsigned int pid)
{
    int              status = GSL_SUCCESS;
    gsl_deviceid_t   device_id;
    gsl_sharedmem_t  *shmem;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "--> int kgsl_sharedmem_free(gsl_memdesc_t *memdesc=%M)\n", memdesc );

    GSL_MEMDESC_DEVICE_GET(memdesc, device_id);

    shmem = &gsl_driver.shmem;

    if (shmem->flags & GSL_FLAGS_INITIALIZED)
    {
        kgsl_memarena_free(shmem->memarena, memdesc);

        // clear descriptor
        memset(memdesc, 0, sizeof(gsl_memdesc_t));
    }
    else
    {
        status = GSL_FAILURE;
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_free. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_free(gsl_memdesc_t *memdesc)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_free0(memdesc, current->tgid);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_read0(const gsl_memdesc_t *memdesc, void *dst, unsigned int offsetbytes, unsigned int sizebytes, unsigned int touserspace)
{
    gsl_sharedmem_t  *shmem;
    unsigned int     gpuoffsetbytes;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_read(gsl_memdesc_t *memdesc=%M, void *dst=0x%08x, uint offsetbytes=%u, uint sizebytes=%u)\n",
                    memdesc, dst, offsetbytes, sizebytes );

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_read. Return value %B\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & GSL_FLAGS_INITIALIZED))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Shared memory not initialized.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_read. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(dst);
    DEBUG_ASSERT(sizebytes);

    if (memdesc->gpuaddr < shmem->memarena->gpubaseaddr)
    {
        return (GSL_FAILURE_BADPARAM);
    }

    if ((memdesc->gpuaddr + sizebytes) > (shmem->memarena->gpubaseaddr + shmem->memarena->sizebytes))
    {
        return (GSL_FAILURE_BADPARAM);
    }

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->memarena->gpubaseaddr) + offsetbytes;

    GSL_HAL_MEM_READ(dst, shmem->memarena->hostbaseaddr, gpuoffsetbytes, sizebytes, touserspace);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_read. Return value %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_read(const gsl_memdesc_t *memdesc, void *dst, unsigned int offsetbytes, unsigned int sizebytes, unsigned int touserspace)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_read0(memdesc, dst, offsetbytes, sizebytes, touserspace);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_write0(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, void *src, unsigned int sizebytes, unsigned int fromuserspace)
{
    gsl_sharedmem_t  *shmem;
    unsigned int     gpuoffsetbytes;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_write(gsl_memdesc_t *memdesc=%M, uint offsetbytes=%u, void *src=0x%08x, uint sizebytes=%u)\n",
                    memdesc, offsetbytes, src, sizebytes );

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_write. Return value %B\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & GSL_FLAGS_INITIALIZED))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Shared memory not initialized.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_write. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(src);
    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc->gpuaddr >= shmem->memarena->gpubaseaddr);
    DEBUG_ASSERT((memdesc->gpuaddr + sizebytes) <= (shmem->memarena->gpubaseaddr + shmem->memarena->sizebytes));

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->memarena->gpubaseaddr) + offsetbytes;

    GSL_HAL_MEM_WRITE(shmem->memarena->hostbaseaddr, gpuoffsetbytes, src, sizebytes, fromuserspace);

    KGSL_DEBUG(GSL_DBGFLAGS_PM4MEM, KGSL_DEBUG_DUMPMEMWRITE((memdesc->gpuaddr + offsetbytes), sizebytes, src));

    KGSL_DEBUG_TBDUMP_SYNCMEM( (memdesc->gpuaddr + offsetbytes), src, sizebytes );

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_write. Return value %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_write(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, void *src, unsigned int sizebytes, unsigned int fromuserspace)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_write0(memdesc, offsetbytes, src, sizebytes, fromuserspace);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_set0(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes)
{
    gsl_sharedmem_t  *shmem;
    unsigned int     gpuoffsetbytes;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_set(gsl_memdesc_t *memdesc=%M, unsigned int offsetbytes=%d, unsigned int value=0x%08x, unsigned int sizebytes=%d)\n",
                    memdesc, offsetbytes, value, sizebytes );

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_set. Return value %B\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & GSL_FLAGS_INITIALIZED))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Shared memory not initialized.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_set. Return value %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc->gpuaddr >= shmem->memarena->gpubaseaddr);
    DEBUG_ASSERT((memdesc->gpuaddr + sizebytes) <= (shmem->memarena->gpubaseaddr + shmem->memarena->sizebytes));

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->memarena->gpubaseaddr) + offsetbytes;

    GSL_HAL_MEM_SET(shmem->memarena->hostbaseaddr, gpuoffsetbytes, value, sizebytes);

    KGSL_DEBUG(GSL_DBGFLAGS_PM4MEM, KGSL_DEBUG_DUMPMEMSET((memdesc->gpuaddr + offsetbytes), sizebytes, value));

    KGSL_DEBUG_TBDUMP_SETMEM( (memdesc->gpuaddr + offsetbytes), value, sizebytes );

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_set. Return value %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_set(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_set0(memdesc, offsetbytes, value, sizebytes);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

unsigned int
kgsl_sharedmem_largestfreeblock(gsl_deviceid_t device_id, gsl_flags_t flags)
{
    unsigned int      result = 0;
    gsl_sharedmem_t   *shmem;

    // device_id is ignored at this level, it would be used with per-device memarena's

    // unreferenced formal parameter
    (void) device_id;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_largestfreeblock(gsl_deviceid_t device_id=%D, gsl_flags_t flags=%x)\n",
                    device_id, flags );

    mutex_lock(&gsl_driver.lock);

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & GSL_FLAGS_INITIALIZED))
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Shared memory not initialized.\n" );
        mutex_unlock(&gsl_driver.lock);
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_largestfreeblock. Return value %d\n", 0 );
        return (0);
    }

    result = kgsl_memarena_getlargestfreeblock(shmem->memarena, flags);

    mutex_unlock(&gsl_driver.lock);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_largestfreeblock. Return value %d\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_map(gsl_deviceid_t device_id, gsl_flags_t flags, const gsl_scatterlist_t *scatterlist, gsl_memdesc_t *memdesc)
{
    int              status = GSL_FAILURE;
    gsl_sharedmem_t  *shmem = &gsl_driver.shmem;
    gsl_deviceid_t   tmp_id;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_map(gsl_deviceid_t device_id=%D, gsl_flags_t flags=%x, gsl_scatterlist_t scatterlist=%S, gsl_memdesc_t *memdesc=%M)\n",
                    device_id, flags, memdesc, scatterlist );

    // execute pending device action
    tmp_id = (device_id != GSL_DEVICE_ANY) ? device_id : device_id+1;
    for ( ; tmp_id <= GSL_DEVICE_MAX; tmp_id++)
    {
        if (gsl_driver.device[tmp_id-1].flags & GSL_FLAGS_INITIALIZED)
        {
            kgsl_device_runpending(&gsl_driver.device[tmp_id-1]);

            if (tmp_id == device_id)
            {
                break;
            }
        }
    }

    // convert any device to an actual existing device
    if (device_id == GSL_DEVICE_ANY)
    {
        for ( ; ; )
        {
            device_id++;

            if (device_id <= GSL_DEVICE_MAX)
            {
                if (gsl_driver.device[device_id-1].flags & GSL_FLAGS_INITIALIZED)
                {
                    break;
                }
            }
            else
            {
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Invalid device.\n" );
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_map. Return value %B\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
        }
    }

    DEBUG_ASSERT(device_id > GSL_DEVICE_ANY && device_id <= GSL_DEVICE_MAX);

    if (shmem->flags & GSL_FLAGS_INITIALIZED)
    {
        if (kgsl_memarena_isvirtualized(shmem->memarena))
        {
            DEBUG_ASSERT(scatterlist->num);
            DEBUG_ASSERT(scatterlist->pages);

            status = kgsl_memarena_alloc(shmem->memarena, flags, scatterlist->num *GSL_PAGESIZE, memdesc);
            if (status == GSL_SUCCESS)
            {
                GSL_MEMDESC_DEVICE_SET(memdesc, device_id);

                // mark descriptor's memory as externally allocated -- i.e. outside GSL
                GSL_MEMDESC_EXTALLOC_SET(memdesc, 1);

                status = kgsl_mmu_map(&gsl_driver.device[device_id-1].mmu, memdesc->gpuaddr, scatterlist, flags, current->tgid);
                if (status != GSL_SUCCESS)
                {
                    kgsl_memarena_free(shmem->memarena, memdesc);
                }
            }
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_map. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_unmap(gsl_memdesc_t *memdesc)
{
    return (kgsl_sharedmem_free0(memdesc, current->tgid));
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_getmap(const gsl_memdesc_t *memdesc, gsl_scatterlist_t *scatterlist)
{
    int              status = GSL_SUCCESS;
    gsl_deviceid_t   device_id;
    gsl_sharedmem_t  *shmem;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_sharedmem_getmap(gsl_memdesc_t *memdesc=%M, gsl_scatterlist_t scatterlist=%S)\n",
                    memdesc, scatterlist );

    GSL_MEMDESC_DEVICE_GET(memdesc, device_id);

    shmem = &gsl_driver.shmem;

    if (shmem->flags & GSL_FLAGS_INITIALIZED)
    {
        DEBUG_ASSERT(scatterlist->num);
        DEBUG_ASSERT(scatterlist->pages);
        DEBUG_ASSERT(memdesc->gpuaddr >= shmem->memarena->gpubaseaddr);
        DEBUG_ASSERT((memdesc->gpuaddr + memdesc->size) <= (shmem->memarena->gpubaseaddr + shmem->memarena->sizebytes));

        memset(scatterlist->pages, 0, sizeof(unsigned int) * scatterlist->num);

        if (kgsl_memarena_isvirtualized(shmem->memarena))
        {
            status = kgsl_mmu_getmap(&gsl_driver.device[device_id-1].mmu, memdesc->gpuaddr, memdesc->size, scatterlist, current->tgid);
        }
        else
        {
            // coalesce physically contiguous pages into a single scatter list entry
            scatterlist->pages[0]   = memdesc->gpuaddr;
            scatterlist->contiguous = 1;
        }
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_sharedmem_getmap. Return value %B\n", status );

    return (status);
}

//----------------------------------------------------------------------------

unsigned int
kgsl_sharedmem_convertaddr(unsigned int addr, int type)
{
    gsl_sharedmem_t  *shmem  = &gsl_driver.shmem;
    unsigned int     cvtaddr = 0;
    unsigned int     gpubaseaddr, hostbaseaddr, sizebytes;

    if ((shmem->flags & GSL_FLAGS_INITIALIZED))
    {
            hostbaseaddr = shmem->memarena->hostbaseaddr;
            gpubaseaddr  = shmem->memarena->gpubaseaddr;
            sizebytes    = shmem->memarena->sizebytes;

            // convert from gpu to host
            if (type == 0)
            {
                if (addr >= gpubaseaddr && addr < (gpubaseaddr + sizebytes))
                {
                    cvtaddr = hostbaseaddr + (addr - gpubaseaddr);
                }
            }
            // convert from host to gpu
            else if (type == 1)
            {
                if (addr >= hostbaseaddr && addr < (hostbaseaddr + sizebytes))
                {
                    cvtaddr = gpubaseaddr + (addr - hostbaseaddr);
                }
            }
    }

    return (cvtaddr);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_cacheoperation(const gsl_memdesc_t *memdesc, unsigned int offsetbytes, unsigned int sizebytes, unsigned int operation)
{
    int status  = GSL_FAILURE;

    /* unreferenced formal parameter */
    (void)memdesc;
    (void)offsetbytes;
    (void)sizebytes;
    (void)operation;

    /* do cache operation */

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_fromhostpointer(gsl_deviceid_t device_id, gsl_memdesc_t *memdesc, void* hostptr)
{
    int status  = GSL_FAILURE;

    memdesc->gpuaddr = (gpuaddr_t)hostptr;  /* map physical address with hostptr    */
    memdesc->hostptr = hostptr;             /* set virtual address also in memdesc  */

    /* unreferenced formal parameter */
    (void)device_id;

    return (status);
}
