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
#include <asm/uaccess.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_types.h"
#include "kgsl_driver.h"
#include "kgsl_sharedmem.h"
#include "kgsl_halconfig.h"
#include "kgsl_linux_map.h"
#include "kgsl_debug.h"
#include "kgsl_log.h"

/////////////////////////////////////////////////////////////////////////////
// macros
//////////////////////////////////////////////////////////////////////////////
#define GSL_SHMEM_APERTURE_MARK(aperture_id)    \
    (shmem->priv |= (((aperture_id + 1) << GSL_APERTURE_SHIFT) & GSL_APERTURE_MASK))

#define GSL_SHMEM_APERTURE_ISMARKED(aperture_id)    \
    (((shmem->priv & GSL_APERTURE_MASK) >> GSL_APERTURE_SHIFT) & (aperture_id + 1))

#define GSL_MEMFLAGS_APERTURE_GET(flags, aperture_id)                                                       \
    aperture_id = (gsl_apertureid_t)((flags & GSL_MEMFLAGS_APERTURE_MASK) >> GSL_MEMFLAGS_APERTURE_SHIFT);  \
    DEBUG_ASSERT(aperture_id < GSL_APERTURE_MAX);

#define GSL_MEMFLAGS_CHANNEL_GET(flags, channel_id)                                                     \
    channel_id = (gsl_channelid_t)((flags & GSL_MEMFLAGS_CHANNEL_MASK) >> GSL_MEMFLAGS_CHANNEL_SHIFT);  \
    DEBUG_ASSERT(channel_id < GSL_CHANNEL_MAX);

#define GSL_MEMDESC_APERTURE_SET(memdesc, aperture_index)   \
    memdesc->priv = (memdesc->priv & ~GSL_APERTURE_MASK) | ((aperture_index << GSL_APERTURE_SHIFT) & GSL_APERTURE_MASK);

#define GSL_MEMDESC_DEVICE_SET(memdesc, device_id)  \
    memdesc->priv = (memdesc->priv & ~GSL_DEVICEID_MASK) | ((device_id << GSL_DEVICEID_SHIFT) & GSL_DEVICEID_MASK);

#define GSL_MEMDESC_EXTALLOC_SET(memdesc, flag) \
    memdesc->priv = (memdesc->priv & ~GSL_EXTALLOC_MASK) | ((flag << GSL_EXTALLOC_SHIFT) & GSL_EXTALLOC_MASK);

#define GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index)                           \
    DEBUG_ASSERT(memdesc);                                                            \
    aperture_index = ((memdesc->priv & GSL_APERTURE_MASK) >> GSL_APERTURE_SHIFT);   \
    DEBUG_ASSERT(aperture_index < GSL_SHMEM_MAX_APERTURES);

#define GSL_MEMDESC_DEVICE_GET(memdesc, device_id)                                              \
    DEBUG_ASSERT(memdesc);                                                                        \
    device_id = (unsigned int)((memdesc->priv & GSL_DEVICEID_MASK) >> GSL_DEVICEID_SHIFT);    \
    DEBUG_ASSERT(device_id <= KGSL_DEVICE_MAX);

#define GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc)  \
    ((memdesc->priv & GSL_EXTALLOC_MASK) >> GSL_EXTALLOC_SHIFT)


//////////////////////////////////////////////////////////////////////////////
// aperture index in shared memory object
//////////////////////////////////////////////////////////////////////////////
static __inline int
kgsl_sharedmem_getapertureindex(struct kgsl_sharedmem *shmem, gsl_apertureid_t aperture_id, gsl_channelid_t channel_id)
{
    DEBUG_ASSERT(shmem->aperturelookup[aperture_id][channel_id] < shmem->numapertures);

    return (shmem->aperturelookup[aperture_id][channel_id]);
}


//////////////////////////////////////////////////////////////////////////////
// functions
//////////////////////////////////////////////////////////////////////////////

int
kgsl_sharedmem_init(struct kgsl_sharedmem *shmem)
{
    int                i;
    int                status;
    gsl_shmemconfig_t  config;
    int                mmu_virtualized;
    gsl_apertureid_t   aperture_id;
    gsl_channelid_t    channel_id;
    unsigned int       hostbaseaddr;
    uint32_t          gpubaseaddr;
    int                sizebytes;

    KGSL_MEM_VDBG("--> int kgsl_sharedmem_init(struct kgsl_sharedmem *shmem=0x%08x)\n", (unsigned int) shmem );

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_init. Return value %d\n", GSL_SUCCESS );
        return (GSL_SUCCESS);
    }

    status = kgsl_hal_getshmemconfig(&config);
    if (status != GSL_SUCCESS)
    {
        KGSL_MEM_VDBG("ERROR: Unable to get sharedmem config.\n" );
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_init. Return value %d\n", status );
        return (status);
    }

    shmem->numapertures = config.numapertures;

    for (i = 0; i < shmem->numapertures; i++)
    {
        aperture_id     = config.apertures[i].id;
        channel_id      = config.apertures[i].channel;
        hostbaseaddr    = config.apertures[i].hostbase;
        gpubaseaddr     = config.apertures[i].gpubase;
        sizebytes       = config.apertures[i].sizebytes;
        mmu_virtualized = 0;

        // handle mmu virtualized aperture
        if (aperture_id == GSL_APERTURE_MMU)
        {
            mmu_virtualized = 1;
            aperture_id     = GSL_APERTURE_EMEM;
        }

        // make sure aligned to page size
        DEBUG_ASSERT((gpubaseaddr & ((1 << PAGE_SHIFT) - 1)) == 0);

        // make a multiple of page size
        sizebytes = (sizebytes & ~((1 << PAGE_SHIFT) - 1));

        if (sizebytes > 0)
        {
            shmem->apertures[i].memarena = kgsl_memarena_create(aperture_id, mmu_virtualized, hostbaseaddr, gpubaseaddr, sizebytes);

            if (!shmem->apertures[i].memarena)
            {
                KGSL_MEM_VDBG("ERROR: Unable to allocate memarena.\n" );
                kgsl_sharedmem_close(shmem);
                KGSL_MEM_VDBG("<-- kgsl_sharedmem_init. Return value %d\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }

            shmem->apertures[i].id       = aperture_id;
            shmem->apertures[i].channel  = channel_id;
            shmem->apertures[i].numbanks = 1;

            // create aperture lookup table
            if (GSL_SHMEM_APERTURE_ISMARKED(aperture_id))
            {
                // update "current aperture_id"/"current channel_id" index
                shmem->aperturelookup[aperture_id][channel_id] = i;
            }
            else
            {
                // initialize "current aperture_id"/"channel_id" indexes
                for (channel_id = GSL_CHANNEL_1; channel_id < GSL_CHANNEL_MAX; channel_id++)
                {
                    shmem->aperturelookup[aperture_id][channel_id] = i;
                }

                GSL_SHMEM_APERTURE_MARK(aperture_id);
            }
        }
    }

    shmem->flags |= KGSL_FLAGS_INITIALIZED;

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_init. Return value %d\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_close(struct kgsl_sharedmem *shmem)
{
    int  i;
    int  result = GSL_SUCCESS;

    KGSL_MEM_VDBG("int kgsl_sharedmem_close(struct kgsl_sharedmem *shmem=0x%08x)\n", (unsigned int) shmem );

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        for (i = 0; i < shmem->numapertures; i++)
        {
            if (shmem->apertures[i].memarena)
            {
                result = kgsl_memarena_destroy(shmem->apertures[i].memarena);
            }
        }

        memset(shmem, 0, sizeof(struct kgsl_sharedmem));
    }

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_close. Return value %d\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_alloc0(unsigned int device_id, unsigned int flags, int sizebytes, struct kgsl_memdesc *memdesc)
{
    gsl_apertureid_t  aperture_id;
    gsl_channelid_t   channel_id;
    unsigned int    tmp_id;
    int               aperture_index, org_index;
    int               result  = GSL_FAILURE;
    struct kgsl_mmu         *mmu    = NULL;
    struct kgsl_sharedmem   *shmem  = &gsl_driver.shmem;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_alloc(unsigned int device_id=%d, unsigned int flags=%x, int sizebytes=%d, struct kgsl_memdesc *memdesc=%x)\n",
                    device_id, flags, sizebytes, (unsigned int) memdesc );

    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc);

    GSL_MEMFLAGS_APERTURE_GET(flags, aperture_id);
    GSL_MEMFLAGS_CHANNEL_GET(flags, channel_id);

    memset(memdesc, 0, sizeof(struct kgsl_memdesc));

    if (!(shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        KGSL_MEM_VDBG("ERROR: Shared memory not initialized.\n" );
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_alloc. Return value %d\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    // execute pending device action
    tmp_id = (device_id != KGSL_DEVICE_ANY) ? device_id : device_id+1;
    for ( ; tmp_id <= KGSL_DEVICE_MAX; tmp_id++)
    {
        if (gsl_driver.device[tmp_id-1].flags & KGSL_FLAGS_INITIALIZED)
        {
            kgsl_device_runpending(&gsl_driver.device[tmp_id-1]);

            if (tmp_id == device_id)
            {
                break;
            }
        }
    }

    // convert any device to an actual existing device
    if (device_id == KGSL_DEVICE_ANY)
    {
        for ( ; ; )
        {
            device_id++;

            if (device_id <= KGSL_DEVICE_MAX)
            {
                if (gsl_driver.device[device_id-1].flags & KGSL_FLAGS_INITIALIZED)
                {
                    break;
                }
            }
            else
            {
                KGSL_MEM_VDBG("ERROR: Invalid device.\n" );
                KGSL_MEM_VDBG("<-- kgsl_sharedmem_alloc. Return value %d\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
        }
    }

    DEBUG_ASSERT(device_id > KGSL_DEVICE_ANY && device_id <= KGSL_DEVICE_MAX);

    // get mmu reference
    mmu = &gsl_driver.device[device_id-1].mmu;

    aperture_index = kgsl_sharedmem_getapertureindex(shmem, aperture_id, channel_id);

    //do not proceed if it is a strict request, the aperture requested is not present, and the MMU is enabled
    if (!((flags & GSL_MEMFLAGS_STRICTREQUEST) && aperture_id != shmem->apertures[aperture_index].id && kgsl_mmu_isenabled(mmu)))
    {
        // do allocation
        result = kgsl_memarena_alloc(shmem->apertures[aperture_index].memarena, flags, sizebytes, memdesc);

        // if allocation failed
        if (result != GSL_SUCCESS)
        {
            org_index = aperture_index;

            // then failover to other channels within the current aperture
            for (channel_id = GSL_CHANNEL_1; channel_id < GSL_CHANNEL_MAX; channel_id++)
            {
                aperture_index = kgsl_sharedmem_getapertureindex(shmem, aperture_id, channel_id);

                if (aperture_index != org_index)
                {
                    // do allocation
                    result = kgsl_memarena_alloc(shmem->apertures[aperture_index].memarena, flags, sizebytes, memdesc);

                    if (result == GSL_SUCCESS)
                    {
                        break;
                    }
                }
            }

            // if allocation still has not succeeded, then failover to EMEM/MMU aperture, but
            // not if it's a strict request and the MMU is enabled
            if (result != GSL_SUCCESS && aperture_id != GSL_APERTURE_EMEM
                && !((flags & GSL_MEMFLAGS_STRICTREQUEST) && kgsl_mmu_isenabled(mmu)))
            {
                aperture_id = GSL_APERTURE_EMEM;

                // try every channel
                for (channel_id = GSL_CHANNEL_1; channel_id < GSL_CHANNEL_MAX; channel_id++)
                {
                    aperture_index = kgsl_sharedmem_getapertureindex(shmem, aperture_id, channel_id);

                    if (aperture_index != org_index)
                    {
                        // do allocation
                        result = kgsl_memarena_alloc(shmem->apertures[aperture_index].memarena, flags, sizebytes, memdesc);

                        if (result == GSL_SUCCESS)
                        {
                            break;
                        }
                    }
                }
            }
        }
    }

    if (result == GSL_SUCCESS)
    {
        GSL_MEMDESC_APERTURE_SET(memdesc, aperture_index);
        GSL_MEMDESC_DEVICE_SET(memdesc, device_id);

        if (kgsl_memarena_isvirtualized(shmem->apertures[aperture_index].memarena))
        {
            gsl_scatterlist_t scatterlist;

            scatterlist.contiguous = 0;
            scatterlist.num        = memdesc->size / PAGE_SIZE;

            if (memdesc->size & (PAGE_SIZE-1))
            {
                scatterlist.num++;
            }

            scatterlist.pages = kmalloc(sizeof(unsigned int) * scatterlist.num, GFP_KERNEL);
            if (scatterlist.pages)
            {
                // allocate physical pages
                result = kgsl_hal_allocphysical(memdesc->gpuaddr, scatterlist.num, scatterlist.pages);
                if (result == GSL_SUCCESS)
                {
                    result = kgsl_mmu_map(mmu, memdesc->gpuaddr, &scatterlist, flags, current->tgid);
                    if (result != GSL_SUCCESS)
                    {
                        kgsl_hal_freephysical(memdesc->gpuaddr, scatterlist.num, scatterlist.pages);
                    }
                }

                kfree(scatterlist.pages);
            }
            else
            {
                result = GSL_FAILURE;
            }

            if (result != GSL_SUCCESS)
            {
                kgsl_memarena_free(shmem->apertures[aperture_index].memarena, memdesc);
            }
        }
    }

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_alloc. Return value %d\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_alloc(unsigned int device_id, unsigned int flags, int sizebytes, struct kgsl_memdesc *memdesc)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_alloc0(device_id, flags, sizebytes, memdesc);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_free0(struct kgsl_memdesc *memdesc, unsigned int pid)
{
    int              status = GSL_SUCCESS;
    int              aperture_index;
    unsigned int   device_id;
    struct kgsl_sharedmem  *shmem;

    KGSL_MEM_VDBG("int kgsl_sharedmem_free(struct kgsl_memdesc *memdesc=%x)\n", (unsigned int) memdesc );

    GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index);
    GSL_MEMDESC_DEVICE_GET(memdesc, device_id);

    shmem = &gsl_driver.shmem;

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        if (kgsl_memarena_isvirtualized(shmem->apertures[aperture_index].memarena))
        {
            status |= kgsl_mmu_unmap(&gsl_driver.device[device_id-1].mmu, memdesc->gpuaddr, memdesc->size, pid);

            if (!GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
            {
                status |= kgsl_hal_freephysical(memdesc->gpuaddr, memdesc->size / PAGE_SIZE, NULL);
            }
        }

        kgsl_memarena_free(shmem->apertures[aperture_index].memarena, memdesc);

        // clear descriptor
        memset(memdesc, 0, sizeof(struct kgsl_memdesc));
    }
    else
    {
        status = GSL_FAILURE;
    }

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_free. Return value %d\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_free0(memdesc, current->tgid);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_read0(const struct kgsl_memdesc *memdesc, void *dst, unsigned int offsetbytes, unsigned int sizebytes, unsigned int touserspace)
{
    int              aperture_index;
    struct kgsl_sharedmem  *shmem;
    unsigned int     gpuoffsetbytes;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_read(struct kgsl_memdesc *memdesc=%x, void *dst=0x%08x, uint offsetbytes=%u, uint sizebytes=%u)\n",
                    (unsigned int) memdesc, (unsigned int) dst, offsetbytes, sizebytes );

    GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index);

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_read. Return value %d\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        KGSL_MEM_VDBG("ERROR: Shared memory not initialized.\n" );
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_read. Return value %d\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(dst);
    DEBUG_ASSERT(sizebytes);

    if (memdesc->gpuaddr < shmem->apertures[aperture_index].memarena->gpubaseaddr)
    {
        return (GSL_FAILURE_BADPARAM);
    }

    if (memdesc->gpuaddr + sizebytes > shmem->apertures[aperture_index].memarena->gpubaseaddr + shmem->apertures[aperture_index].memarena->sizebytes)
    {
        return (GSL_FAILURE_BADPARAM);
    }

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->apertures[aperture_index].memarena->gpubaseaddr) + offsetbytes;

    if (gsl_driver.enable_mmu && (shmem->apertures[aperture_index].memarena->hostbaseaddr >= GSL_LINUX_MAP_RANGE_START) && (shmem->apertures[aperture_index].memarena->hostbaseaddr < GSL_LINUX_MAP_RANGE_END)) {
        kgsl_mem_entry_read(dst, shmem->apertures[aperture_index].memarena->hostbaseaddr+gpuoffsetbytes, sizebytes, touserspace);
    } else {
        if (touserspace)
        {
            if (copy_to_user(dst, (void *)(shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes), sizebytes))
            {
                return;
            }
        }
        else
        {
            memcpy(dst, (void *) (shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes), sizebytes);
        }
    }


    KGSL_MEM_VDBG("<-- kgsl_sharedmem_read. Return value %d\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_read(const struct kgsl_memdesc *memdesc, void *dst, unsigned int offsetbytes, unsigned int sizebytes, unsigned int touserspace)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_read0(memdesc, dst, offsetbytes, sizebytes, touserspace);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_write0(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes, void *src, unsigned int sizebytes, unsigned int fromuserspace)
{
    int              aperture_index;
    struct kgsl_sharedmem  *shmem;
    unsigned int     gpuoffsetbytes;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_write(struct kgsl_memdesc *memdesc=%x, uint offsetbytes=%u, void *src=0x%08x, uint sizebytes=%u)\n",
                    (unsigned int) memdesc, offsetbytes, (unsigned int) src, sizebytes );

    GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index);

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_write. Return value %d\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        KGSL_MEM_VDBG("ERROR: Shared memory not initialized.\n" );
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_write. Return value %d\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(src);
    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc->gpuaddr >= shmem->apertures[aperture_index].memarena->gpubaseaddr);
    DEBUG_ASSERT((memdesc->gpuaddr + sizebytes) <= (shmem->apertures[aperture_index].memarena->gpubaseaddr + shmem->apertures[aperture_index].memarena->sizebytes));

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->apertures[aperture_index].memarena->gpubaseaddr) + offsetbytes;

    if (gsl_driver.enable_mmu && (shmem->apertures[aperture_index].memarena->hostbaseaddr >= GSL_LINUX_MAP_RANGE_START) && (shmem->apertures[aperture_index].memarena->hostbaseaddr < GSL_LINUX_MAP_RANGE_END)) {
        kgsl_mem_entry_write(src, shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes, sizebytes, fromuserspace);
    } else {
        if (fromuserspace)
        {
            if (copy_from_user((void *)(shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes), src, sizebytes))
            {
                return;
            }
        }
        else
        {
            memcpy((void *)(shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes), src, sizebytes);
        }
    }

    KGSL_DEBUG(GSL_DBGFLAGS_PM4MEM, KGSL_DEBUG_DUMPMEMWRITE((memdesc->gpuaddr + offsetbytes), sizebytes, src));

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_write. Return value %d\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_write(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes, void *src, unsigned int sizebytes, unsigned int fromuserspace)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_write0(memdesc, offsetbytes, src, sizebytes, fromuserspace);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_set0(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes)
{
    int              aperture_index;
    struct kgsl_sharedmem  *shmem;
    unsigned int     gpuoffsetbytes;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_set(struct kgsl_memdesc *memdesc=%x, unsigned int offsetbytes=%d, unsigned int value=0x%08x, unsigned int sizebytes=%d)\n",
                    (unsigned int) memdesc, offsetbytes, value, sizebytes );

    GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index);

    if (GSL_MEMDESC_EXTALLOC_ISMARKED(memdesc))
    {
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_set. Return value %d\n", GSL_FAILURE_BADPARAM );
        return (GSL_FAILURE_BADPARAM);
    }

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        KGSL_MEM_VDBG("ERROR: Shared memory not initialized.\n" );
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_set. Return value %d\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    DEBUG_ASSERT(sizebytes);
    DEBUG_ASSERT(memdesc->gpuaddr >= shmem->apertures[aperture_index].memarena->gpubaseaddr);
    DEBUG_ASSERT((memdesc->gpuaddr + sizebytes) <= (shmem->apertures[aperture_index].memarena->gpubaseaddr + shmem->apertures[aperture_index].memarena->sizebytes));

    gpuoffsetbytes = (memdesc->gpuaddr - shmem->apertures[aperture_index].memarena->gpubaseaddr) + offsetbytes;

    if (gsl_driver.enable_mmu && (shmem->apertures[aperture_index].memarena->hostbaseaddr >= GSL_LINUX_MAP_RANGE_START) && (shmem->apertures[aperture_index].memarena->hostbaseaddr < GSL_LINUX_MAP_RANGE_END)) {
	kgsl_mem_entry_set(shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes, value, sizebytes);
    } else {
        memset((void *)(shmem->apertures[aperture_index].memarena->hostbaseaddr + gpuoffsetbytes), value, sizebytes);
    }

    KGSL_DEBUG(GSL_DBGFLAGS_PM4MEM, KGSL_DEBUG_DUMPMEMSET((memdesc->gpuaddr + offsetbytes), sizebytes, value));

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_set. Return value %d\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes, unsigned int value, unsigned int sizebytes)
{
	int status = GSL_SUCCESS;
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_set0(memdesc, offsetbytes, value, sizebytes);
	mutex_unlock(&gsl_driver.lock);
	return status;
}

//----------------------------------------------------------------------------

unsigned int
kgsl_sharedmem_largestfreeblock(unsigned int device_id, unsigned int flags)
{
    gsl_apertureid_t  aperture_id;
    gsl_channelid_t   channel_id;
    int               aperture_index;
    unsigned int      result = 0;
    struct kgsl_sharedmem   *shmem;

    // device_id is ignored at this level, it would be used with per-device memarena's

    // unreferenced formal parameter
    (void) device_id;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_largestfreeblock(unsigned int device_id=%d, unsigned int flags=%x)\n",
                    device_id, flags );

    GSL_MEMFLAGS_APERTURE_GET(flags, aperture_id);
    GSL_MEMFLAGS_CHANNEL_GET(flags, channel_id);

    mutex_lock(&gsl_driver.lock);

    shmem = &gsl_driver.shmem;

    if (!(shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        KGSL_MEM_VDBG("ERROR: Shared memory not initialized.\n" );
        mutex_unlock(&gsl_driver.lock);
        KGSL_MEM_VDBG("<-- kgsl_sharedmem_largestfreeblock. Return value %d\n", 0 );
        return (0);
    }

    aperture_index = kgsl_sharedmem_getapertureindex(shmem, aperture_id, channel_id);

    if (aperture_id == shmem->apertures[aperture_index].id)
    {
        result = kgsl_memarena_getlargestfreeblock(shmem->apertures[aperture_index].memarena, flags);
    }

    mutex_unlock(&gsl_driver.lock);

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_largestfreeblock. Return value %d\n", result );

    return (result);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_map(unsigned int device_id, unsigned int flags, const gsl_scatterlist_t *scatterlist, struct kgsl_memdesc *memdesc)
{
    int              status = GSL_FAILURE;
    struct kgsl_sharedmem  *shmem = &gsl_driver.shmem;
    int              aperture_index;
    unsigned int   tmp_id;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_map(unsigned int device_id=%d, unsigned int flags=%x, gsl_scatterlist_t scatterlist=%x, struct kgsl_memdesc *memdesc=%x)\n",
                    device_id, flags, (unsigned int) scatterlist, (unsigned int) memdesc );

    // execute pending device action
    tmp_id = (device_id != KGSL_DEVICE_ANY) ? device_id : device_id+1;
    for ( ; tmp_id <= KGSL_DEVICE_MAX; tmp_id++)
    {
        if (gsl_driver.device[tmp_id-1].flags & KGSL_FLAGS_INITIALIZED)
        {
            kgsl_device_runpending(&gsl_driver.device[tmp_id-1]);

            if (tmp_id == device_id)
            {
                break;
            }
        }
    }

    // convert any device to an actual existing device
    if (device_id == KGSL_DEVICE_ANY)
    {
        for ( ; ; )
        {
            device_id++;

            if (device_id <= KGSL_DEVICE_MAX)
            {
                if (gsl_driver.device[device_id-1].flags & KGSL_FLAGS_INITIALIZED)
                {
                    break;
                }
            }
            else
            {
                KGSL_MEM_VDBG("ERROR: Invalid device.\n" );
                KGSL_MEM_VDBG("<-- kgsl_sharedmem_map. Return value %d\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
        }
    }

    DEBUG_ASSERT(device_id > KGSL_DEVICE_ANY && device_id <= KGSL_DEVICE_MAX);

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        aperture_index = kgsl_sharedmem_getapertureindex(shmem, GSL_APERTURE_EMEM, GSL_CHANNEL_1);

        if (kgsl_memarena_isvirtualized(shmem->apertures[aperture_index].memarena))
        {
            DEBUG_ASSERT(scatterlist->num);
            DEBUG_ASSERT(scatterlist->pages);

            status = kgsl_memarena_alloc(shmem->apertures[aperture_index].memarena, flags, scatterlist->num *PAGE_SIZE, memdesc);
            if (status == GSL_SUCCESS)
            {
                GSL_MEMDESC_APERTURE_SET(memdesc, aperture_index);
                GSL_MEMDESC_DEVICE_SET(memdesc, device_id);

                // mark descriptor's memory as externally allocated -- i.e. outside GSL
                GSL_MEMDESC_EXTALLOC_SET(memdesc, 1);

                status = kgsl_mmu_map(&gsl_driver.device[device_id-1].mmu, memdesc->gpuaddr, scatterlist, flags, current->tgid);
                if (status != GSL_SUCCESS)
                {
                    kgsl_memarena_free(shmem->apertures[aperture_index].memarena, memdesc);
                }
            }
        }
    }

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_map. Return value %d\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_unmap(struct kgsl_memdesc *memdesc)
{
    return (kgsl_sharedmem_free0(memdesc, current->tgid));
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_getmap(const struct kgsl_memdesc *memdesc, gsl_scatterlist_t *scatterlist)
{
    int              status = GSL_SUCCESS;
    int              aperture_index;
    unsigned int   device_id;
    struct kgsl_sharedmem  *shmem;

KGSL_MEM_VDBG(                    "--> int kgsl_sharedmem_getmap(struct kgsl_memdesc *memdesc=%x, gsl_scatterlist_t scatterlist=%x)\n",
                    (unsigned int) memdesc, (unsigned int) scatterlist );

    GSL_MEMDESC_APERTURE_GET(memdesc, aperture_index);
    GSL_MEMDESC_DEVICE_GET(memdesc, device_id);

    shmem = &gsl_driver.shmem;

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        DEBUG_ASSERT(scatterlist->num);
        DEBUG_ASSERT(scatterlist->pages);
        DEBUG_ASSERT(memdesc->gpuaddr >= shmem->apertures[aperture_index].memarena->gpubaseaddr);
        DEBUG_ASSERT((memdesc->gpuaddr + memdesc->size) <= (shmem->apertures[aperture_index].memarena->gpubaseaddr + shmem->apertures[aperture_index].memarena->sizebytes));

        memset(scatterlist->pages, 0, sizeof(unsigned int) * scatterlist->num);

        if (kgsl_memarena_isvirtualized(shmem->apertures[aperture_index].memarena))
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

    KGSL_MEM_VDBG("<-- kgsl_sharedmem_getmap. Return value %d\n", status );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_querystats(struct kgsl_sharedmem *shmem, gsl_sharedmem_stats_t *stats)
{
#ifdef GSL_STATS_MEM
    int  status = GSL_SUCCESS;
    int  i;

    DEBUG_ASSERT(stats);

    if (shmem->flags & KGSL_FLAGS_INITIALIZED)
    {
        for (i = 0; i < shmem->numapertures; i++)
        {
            if (shmem->apertures[i].memarena)
            {
                stats->apertures[i].id      = shmem->apertures[i].id;
                stats->apertures[i].channel = shmem->apertures[i].channel;

                status |= kgsl_memarena_querystats(shmem->apertures[i].memarena, &stats->apertures[i].memarena);
            }
        }
    }
    else
    {
        memset(stats, 0, sizeof(gsl_sharedmem_stats_t));
    }

    return (status);
#else
    // unreferenced formal parameters
    (void) shmem;
    (void) stats;

    return (GSL_FAILURE_NOTSUPPORTED);
#endif // GSL_STATS_MEM
}

//----------------------------------------------------------------------------

unsigned int
kgsl_sharedmem_convertaddr(unsigned int addr, int type)
{
    struct kgsl_sharedmem  *shmem  = &gsl_driver.shmem;
    unsigned int     cvtaddr = 0;
    unsigned int     gpubaseaddr, hostbaseaddr, sizebytes;
    int              i;

    if ((shmem->flags & KGSL_FLAGS_INITIALIZED))
    {
        for (i = 0; i < shmem->numapertures; i++)
        {
            hostbaseaddr = shmem->apertures[i].memarena->hostbaseaddr;
            gpubaseaddr  = shmem->apertures[i].memarena->gpubaseaddr;
            sizebytes    = shmem->apertures[i].memarena->sizebytes;

            // convert from gpu to host
            if (type == 0)
            {
                if (addr >= gpubaseaddr && addr < (gpubaseaddr + sizebytes))
                {
                    cvtaddr = hostbaseaddr + (addr - gpubaseaddr);
                    break;
                }
            }
            // convert from host to gpu
            else if (type == 1)
            {
                if (addr >= hostbaseaddr && addr < (hostbaseaddr + sizebytes))
                {
                    cvtaddr = gpubaseaddr + (addr - hostbaseaddr);
                    break;
                }
            }
        }
    }

    return (cvtaddr);
}

//----------------------------------------------------------------------------

int
kgsl_sharedmem_cacheoperation(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes, unsigned int sizebytes, unsigned int operation)
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
kgsl_sharedmem_fromhostpointer(unsigned int device_id, struct kgsl_memdesc *memdesc, void* hostptr)
{
    int status  = GSL_FAILURE;

    memdesc->gpuaddr = (uint32_t)hostptr;  /* map physical address with hostptr    */
    memdesc->hostptr = hostptr;             /* set virtual address also in memdesc  */

    /* unreferenced formal parameter */
    (void)device_id;

    return (status);
}
