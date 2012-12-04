/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_types.h"
#include "kgsl_mmu.h"
#include "kgsl_buildconfig.h"
#include "kgsl_device.h"
#include "kgsl_driver.h"
#include "kgsl_linux_map.h"
#include "kgsl_halconfig.h"
#include "kgsl_hal.h"
#include "kgsl_ringbuffer.h" // for defines for the include below!
#include "kgsl_cmdstream.h"
#include "kgsl_g12_vgv3types.h"
#include "kgsl_log.h"


// functions

int kgsl_cmdstream_init(struct kgsl_device *device)
{
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	device->cmdstream_mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (!device->cmdstream_mutex)
		return GSL_FAILURE;
	mutex_init(device->cmdstream_mutex);
#endif
	return GSL_SUCCESS;
}

int kgsl_cmdstream_close(struct kgsl_device *device)
{
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	if (!device->cmdstream_mutex)
		return GSL_FAILURE;
	kfree(device->cmdstream_mutex);
	device->cmdstream_mutex = NULL;
#endif
	return GSL_SUCCESS;
}

unsigned int kgsl_cmdstream_readtimestamp0(unsigned int device_id, enum kgsl_timestamp_type type)
{
	unsigned int   timestamp = 0;
	struct kgsl_device* device  = &gsl_driver.device[device_id-1];

	KGSL_CMD_VDBG("enter (device_id=%d, type=%d)\n", device_id, type );

	if (type == KGSL_TIMESTAMP_CONSUMED) {
		// start-of-pipeline timestamp
		KGSL_CMDSTREAM_GET_SOP_TIMESTAMP(device, &timestamp);
	} else if (type == KGSL_TIMESTAMP_RETIRED) {
		// end-of-pipeline timestamp
		KGSL_CMDSTREAM_GET_EOP_TIMESTAMP(device, &timestamp);
	}

	KGSL_CMD_VDBG("return %d\n", timestamp);

	return timestamp;
}

//----------------------------------------------------------------------------

unsigned int
kgsl_cmdstream_readtimestamp(unsigned int device_id, enum kgsl_timestamp_type type)
{
	unsigned int timestamp = -1;

	mutex_lock(&gsl_driver.lock);

	timestamp = kgsl_cmdstream_readtimestamp0(device_id, type);

	mutex_unlock(&gsl_driver.lock);

	return timestamp;
}

//----------------------------------------------------------------------------

int
kgsl_cmdstream_issueibcmds(unsigned int device_id, int drawctxt_index, uint32_t ibaddr, int sizedwords, unsigned int *timestamp, unsigned int flags)
{
    struct kgsl_device* device  = &gsl_driver.device[device_id-1];
    int status = GSL_FAILURE;

    mutex_lock(&gsl_driver.lock);

    kgsl_device_active(device);

    if (device->ftbl.cmdstream_issueibcmds)
    {
        status = device->ftbl.cmdstream_issueibcmds(device, drawctxt_index, ibaddr, sizedwords, timestamp, flags);
    }

    mutex_unlock(&gsl_driver.lock);

    return status;
}

int kgsl_cmdstream_waittimestamp(unsigned int device_id, unsigned int timestamp, unsigned int timeout)
{
    struct kgsl_device* device  = &gsl_driver.device[device_id-1];
	int status = GSL_FAILURE;
	if (device->ftbl.waittimestamp)
    {
        status = device->ftbl.waittimestamp(device, timestamp, timeout);
    }
    return status;
}

//----------------------------------------------------------------------------

void
kgsl_cmdstream_memqueue_drain(struct kgsl_device *device)
{
    gsl_memnode_t     *memnode, *nextnode, *freehead;
    unsigned int   timestamp, ts_processed;
    gsl_memqueue_t    *memqueue = &device->memqueue;

#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
    mutex_lock(device->cmdstream_mutex);
#endif

    // check head
    if (memqueue->head == NULL)
    {
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	mutex_unlock(device->cmdstream_mutex);
#endif
        return;
    }
    // get current EOP timestamp
    ts_processed = kgsl_cmdstream_readtimestamp0(device->id, KGSL_TIMESTAMP_RETIRED);
    timestamp = memqueue->head->timestamp;
    // check head timestamp
    if (!(((ts_processed - timestamp) >= 0) || ((ts_processed - timestamp) < -KGSL_TIMESTAMP_EPSILON)))
    {
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	mutex_unlock(device->cmdstream_mutex);
#endif
        return;
    }
    memnode  = memqueue->head;
    freehead = memqueue->head;
    // get node list to free
    for(;;)
    {
        nextnode  = memnode->next;
        if (nextnode == NULL)
        {
            // entire queue drained
            memqueue->head = NULL;
            memqueue->tail = NULL;
            break;
        }
        timestamp = nextnode->timestamp;
        if (!(((ts_processed - timestamp) >= 0) || ((ts_processed - timestamp) < -KGSL_TIMESTAMP_EPSILON)))
        {
            // drained up to a point
            memqueue->head = nextnode;
            memnode->next  = NULL;
            break;
        }
        memnode = nextnode;
    }
    // free nodes
    while (freehead)
    {
        memnode  = freehead;
        freehead = memnode->next;
        kgsl_sharedmem_free0(&memnode->memdesc, memnode->pid);
        kfree(memnode);
    }

#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
    mutex_unlock(device->cmdstream_mutex);
#endif
}

//----------------------------------------------------------------------------

int
kgsl_cmdstream_freememontimestamp(unsigned int device_id, struct kgsl_memdesc *memdesc, unsigned int timestamp, enum kgsl_timestamp_type type)
{
    gsl_memnode_t  *memnode;
    struct kgsl_device   *device = &gsl_driver.device[device_id-1];
    gsl_memqueue_t *memqueue;
    (void)type; // unref. For now just use EOP timestamp

    mutex_lock(&gsl_driver.lock);
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
    mutex_lock(device->cmdstream_mutex);
#endif
    memqueue = &device->memqueue;
    memnode  = kmalloc(sizeof(gsl_memnode_t), GFP_KERNEL);

    if (!memnode)
    {
        // other solution is to idle and free which given that the upper level driver probably wont check, probably a better idea
#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
	mutex_unlock(device->cmdstream_mutex);
#endif
	mutex_unlock(&gsl_driver.lock);
        return (GSL_FAILURE);
    }

    memnode->timestamp = timestamp;
    memnode->pid       = current->tgid;
    memnode->next      = NULL;
    memcpy(&memnode->memdesc, memdesc, sizeof(struct kgsl_memdesc));

    // add to end of queue
    if (memqueue->tail != NULL)
    {
        memqueue->tail->next = memnode;
        memqueue->tail       = memnode;
    }
    else
    {
        DEBUG_ASSERT(memqueue->head == NULL);
        memqueue->head = memnode;
        memqueue->tail = memnode;
    }

#ifdef CONFIG_KGSL_FINE_GRAINED_LOCKING
    mutex_unlock(device->cmdstream_mutex);
#endif
    mutex_unlock(&gsl_driver.lock);

    return (GSL_SUCCESS);
}

static int kgsl_cmdstream_timestamp_cmp(unsigned int ts_new, unsigned int ts_old)
{
	unsigned int ts_diff = ts_new - ts_old;
	return (ts_diff >= 0) || (ts_diff < -KGSL_TIMESTAMP_EPSILON);
}

int kgsl_cmdstream_check_timestamp(unsigned int device_id, unsigned int timestamp)
{
	unsigned int ts_processed;
	ts_processed = kgsl_cmdstream_readtimestamp0(device_id, KGSL_TIMESTAMP_RETIRED);
	return kgsl_cmdstream_timestamp_cmp(ts_processed, timestamp);
}
