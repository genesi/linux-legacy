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

#include <linux/slab.h>

#include "gsl.h"
#include "gsl_hal.h"

/*
 * defines
 *
 * these are a bit overzealous: a whole byte for a bit flag?
 */
#define GSL_MEMARENAPRIV_SIGNATURE_MASK         0x0000FFFF
#define GSL_MEMARENAPRIV_MMUVIRTUALIZED_MASK    0x0F000000
#define GSL_MEMARENAPRIV_CONPHYS_MASK		0x00F00000 //neko

#define GSL_MEMARENAPRIV_SIGNATURE_SHIFT        0
#define GSL_MEMARENAPRIV_MMUVIRTUALIZED_SHIFT   24
#define GSL_MEMARENAPRIV_CONPHYS_SHIFT		20 //neko

#define GSL_MEMARENA_INSTANCE_SIGNATURE         0x0000CAFE

#ifdef GSL_STATS_MEM
#define GSL_MEMARENA_STATS(x)   x
#else
#define GSL_MEMARENA_STATS(x)
#endif // GSL_STATS_MEM


// macros
#define GSL_MEMARENA_SET_SIGNATURE          (memarena->priv |= ((GSL_MEMARENA_INSTANCE_SIGNATURE << GSL_MEMARENAPRIV_SIGNATURE_SHIFT) & GSL_MEMARENAPRIV_SIGNATURE_MASK))
#define GSL_MEMARENA_SET_MMU_VIRTUALIZED    (memarena->priv |= ((mmu_virtualized << GSL_MEMARENAPRIV_MMUVIRTUALIZED_SHIFT) & GSL_MEMARENAPRIV_MMUVIRTUALIZED_MASK))
#define GSL_MEMARENA_SET_CONPHYS            (memarena->priv |= ((conphys << GSL_MEMARENAPRIV_CONPHYS_SHIFT) & GSL_MEMARENAPRIV_CONPHYS_MASK))//neko

#define GSL_MEMARENA_GET_SIGNATURE          ((memarena->priv & GSL_MEMARENAPRIV_SIGNATURE_MASK)   >> GSL_MEMARENAPRIV_SIGNATURE_SHIFT)
#define GSL_MEMARENA_IS_MMU_VIRTUALIZED     ((memarena->priv & GSL_MEMARENAPRIV_MMUVIRTUALIZED_MASK) >> GSL_MEMARENAPRIV_MMUVIRTUALIZED_SHIFT)
#define GSL_MEMARENA_IS_CONPHYS		    ((memarena->priv & GSL_MEMARENAPRIV_CONPHYS_MASK) >> GSL_MEMARENAPRIV_CONPHYS_SHIFT)//neko

//  validate
#define GSL_MEMARENA_VALIDATE(memarena)                                 \
    DEBUG_ASSERT(memarena);                                               \
    if (GSL_MEMARENA_GET_SIGNATURE != GSL_MEMARENA_INSTANCE_SIGNATURE)  \
    {                                                                   \
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR,   \
			"ERROR: Memarena validation failed.\n" );       \
        return (GSL_FAILURE);                                           \
    }

// " neko

//  block alignment shift count
static __inline unsigned int
gsl_memarena_alignmentshift(gsl_flags_t flags)
{
    int alignshift = ((flags & GSL_MEMFLAGS_ALIGN_MASK) >> GSL_MEMFLAGS_ALIGN_SHIFT);
    if (alignshift == 0)
        alignshift = 5; //  32 bytes is the minimum alignment boundary
    return (alignshift);
}

//  address alignment
static __inline unsigned int
gsl_memarena_alignaddr(unsigned int address, int shift)
{
    //
    //  the value of the returned address is guaranteed to be an even multiple
    //  of the block alignment shift specified.
    //
    unsigned int alignedbaseaddr = ((address) >> shift) << shift;
    if (alignedbaseaddr < address)
    {
        alignedbaseaddr += (1 << shift);
    }
    return (alignedbaseaddr);
}


//                         memory management API
#ifdef GSL_MEMARENA_NODE_POOL_ENABLED
static __inline memblk_t*
kgsl_memarena_getmemblknode_pool(gsl_memarena_t *memarena)
{
    gsl_nodepool_t  *nodepool    = memarena->nodepool;
    memblk_t        *memblk      = NULL;
    int             allocnewpool = 1;
    int             i;

    if (nodepool)
    {
        // walk through list of existing pools
        for ( ; ; )
        {
            // if there is a pool with a free memblk node
            if (nodepool->priv != (1 << GSL_MEMARENA_NODE_POOL_MAX)-1)
            {
                // get index of the first free memblk node
                for (i = 0; i < GSL_MEMARENA_NODE_POOL_MAX; i++)
                {
                    if (((nodepool->priv >> i) & 0x1) == 0)
                    {
                        break;
                    }
                }

                // mark memblk node as used
                nodepool->priv |= 1 << i;

                memblk                = &nodepool->memblk[i];
                memblk->nodepoolindex = i;
                memblk->blkaddr       = 0;
                memblk->blksize       = 0;

                allocnewpool = 0;

                break;
            }
            else
            {
                nodepool = nodepool->next;

                if (nodepool == memarena->nodepool)
                {
                    // no free memblk node found
                    break;
                }
            }
        }
    }

    // if no existing pool has a free memblk node
    if (allocnewpool)
    {
        // alloc new pool of memblk nodes
        nodepool = ((gsl_nodepool_t *)kmalloc(sizeof(gsl_nodepool_t), GFP_KERNEL));
        if (nodepool)
        {
            memset(nodepool, 0, sizeof(gsl_nodepool_t));

            if (memarena->nodepool)
            {
                nodepool->next                 = memarena->nodepool->next;
                nodepool->prev                 = memarena->nodepool;
                memarena->nodepool->next->prev = nodepool;
                memarena->nodepool->next       = nodepool;
            }
            else
            {
                nodepool->next = nodepool;
                nodepool->prev = nodepool;
            }

            // reposition pool head
            memarena->nodepool = nodepool;

            // mark memblk node as used
            nodepool->priv       |= 0x1;

            memblk                = &nodepool->memblk[0];
            memblk->nodepoolindex = 0;
        }
    }

    DEBUG_ASSERT(memblk);

    return (memblk);
}
#endif

static __inline memblk_t*
kgsl_memarena_getmemblknode(gsl_memarena_t *memarena)
{
#ifdef GSL_MEMARENA_NODE_POOL_ENABLED
    return( kgsl_memarena_getmemblknode_pool(gsl_memarena_t *memarena) );
#else
    // unreferenced formal parameter
    (void) memarena;

    return ((memblk_t *)kmalloc(sizeof(memblk_t), GFP_KERNEL));
#endif // GSL_MEMARENA_NODE_POOL_ENABLED
}

//----------------------------------------------------------------------------

#ifdef GSL_MEMARENA_NODE_POOL_ENABLED
static __inline void
kgsl_memarena_releasememblknode_pool(gsl_memarena_t *memarena, memblk_t *memblk)
{
    gsl_nodepool_t *nodepool = memarena->nodepool;

    DEBUG_ASSERT(memblk);
    DEBUG_ASSERT(nodepool);

    // locate pool to which this memblk node belongs
    while (((unsigned int) memblk) < ((unsigned int) nodepool) ||
           ((unsigned int) memblk) > ((unsigned int) nodepool) + sizeof(gsl_nodepool_t))
    {
        nodepool = nodepool->prev;

        DEBUG_ASSERT(nodepool != memarena->nodepool);
    }

    // mark memblk node as unused
    nodepool->priv &= ~(1 << memblk->nodepoolindex);

    // free pool when all its memblk nodes are unused
    if (nodepool->priv == 0)
    {
        if (nodepool != nodepool->prev)
        {
            // reposition pool head
            if (nodepool == memarena->nodepool)
            {
                memarena->nodepool = nodepool->prev;
            }

            nodepool->prev->next = nodepool->next;
            nodepool->next->prev = nodepool->prev;
        }
        else
        {
            memarena->nodepool = NULL;
        }

        kfree((void *)nodepool);
    }
    else
    {
        // leave pool head in last pool a memblk node was released
        memarena->nodepool = nodepool;
    }
}
#endif

static __inline void
kgsl_memarena_releasememblknode(gsl_memarena_t *memarena, memblk_t *memblk)
{
#ifdef GSL_MEMARENA_NODE_POOL_ENABLED
	kgsl_memarena_releasememblknode(memarena, memblk)
#else
    // unreferenced formal parameter
    (void) memarena;

    kfree((void *)memblk);
#endif // GSL_MEMARENA_NODE_POOL_ENABLED
}

//----------------------------------------------------------------------------

gsl_memarena_t*
kgsl_memarena_create(int mmu_virtualized, unsigned int hostbaseaddr, gpuaddr_t gpubaseaddr, int sizebytes)
{
    static int      count = 0;
    gsl_memarena_t  *memarena;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> gsl_memarena_t* kgsl_memarena_create(gpuaddr_t gpubaseaddr=0x%08x, int sizebytes=%d)\n", gpubaseaddr, sizebytes );

    memarena = (gsl_memarena_t *)kmalloc(sizeof(gsl_memarena_t), GFP_KERNEL);

    if (!memarena)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR,
                        "ERROR: Memarena allocation failed.\n" );
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "<-- kgsl_memarena_create. Return value: 0x%08x\n", NULL );
        return (NULL);
    }

    memset(memarena, 0, sizeof(gsl_memarena_t));

    GSL_MEMARENA_SET_SIGNATURE;
    GSL_MEMARENA_SET_MMU_VIRTUALIZED;

    mutex_init(&memarena->lock);

    // set up the memory arena
    memarena->hostbaseaddr = hostbaseaddr;
    memarena->gpubaseaddr  = gpubaseaddr;
    memarena->sizebytes    = sizebytes;

    // allocate a memory block in free list which represents all memory in arena
    memarena->freelist.head          = kgsl_memarena_getmemblknode(memarena);
    memarena->freelist.head->blkaddr = 0;
    memarena->freelist.head->blksize = memarena->sizebytes;
    memarena->freelist.head->next    = memarena->freelist.head;
    memarena->freelist.head->prev    = memarena->freelist.head;
    memarena->freelist.allocrover    = memarena->freelist.head;
    memarena->freelist.freerover     = memarena->freelist.head;

    count++;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_create. Return value: 0x%08x\n", memarena );

    return (memarena);
}

//----------------------------------------------------------------------------

int
kgsl_memarena_destroy(gsl_memarena_t *memarena)
{
    int       status = GSL_SUCCESS;
    int	      err;
    memblk_t  *p, *next;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_memarena_destroy(gsl_memarena_t *memarena=0x%08x)\n", memarena );

    GSL_MEMARENA_VALIDATE(memarena);

    err = mutex_lock_interruptible(&memarena->lock);
    if (err == -EINTR) {
	kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
			"WARNING: memarena mutex lock was interrupted\n");
    }

#ifdef _DEBUG
    // memory leak check
    if (memarena->freelist.head->blksize != memarena->sizebytes)
    {
            // external memory leak detected
            kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
                            "ERROR: External memory leak detected.\n" );
            return (GSL_FAILURE);
    }
#endif // _DEBUG

    p = memarena->freelist.head;
    do
    {
        next = p->next;
        kgsl_memarena_releasememblknode(memarena, p);
        p = next;
    } while (p != memarena->freelist.head);

    mutex_unlock(&memarena->lock);

    kfree((void *)memarena);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_destroy. Return value: %B\n", GSL_SUCCESS );

    return (status);
}

//----------------------------------------------------------------------------

int
kgsl_memarena_isvirtualized(gsl_memarena_t *memarena)
{
    // mmu virtualization enabled
    return (GSL_MEMARENA_IS_MMU_VIRTUALIZED);
}

//----------------------------------------------------------------------------

int
kgsl_memarena_checkconsistency(gsl_memarena_t *memarena)
{
    memblk_t *p;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_memarena_checkconsistency(gsl_memarena_t *memarena=0x%08x)\n", memarena );

    // go through list of free blocks and make sure there are no detectable errors

    p = memarena->freelist.head;
    do
    {
        if (p->next->blkaddr != memarena->freelist.head->blkaddr)
        {
            if (p->prev->next->blkaddr  != p->blkaddr ||
                p->next->prev->blkaddr  != p->blkaddr ||
                p->blkaddr + p->blksize >= p->next->blkaddr)
            {
                DEBUG_ASSERT(0);
                kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_checkconsistency. Return value: %B\n", GSL_FAILURE );
                return (GSL_FAILURE);
            }
        }
        p = p->next;

    } while (p != memarena->freelist.head);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_checkconsistency. Return value: %B\n", GSL_SUCCESS );

    return (GSL_SUCCESS);
}

//----------------------------------------------------------------------------

int
kgsl_memarena_querystats(gsl_memarena_t *memarena, gsl_memarena_stats_t *stats)
{
#ifdef GSL_STATS_MEM
    DEBUG_ASSERT(stats);
    GSL_MEMARENA_VALIDATE(memarena);

    memcpy(stats, &memarena->stats, sizeof(gsl_memarena_stats_t));

    return (GSL_SUCCESS);
#else
    // unreferenced formal parameters
    (void) memarena;
    (void) stats;

    return (GSL_FAILURE_NOTSUPPORTED);
#endif // GSL_STATS_MEM
}

//----------------------------------------------------------------------------

int
kgsl_memarena_checkfreeblock(gsl_memarena_t *memarena, int bytesneeded)
{
    memblk_t  *p;
    int err;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_memarena_checkfreeblock(gsl_memarena_t *memarena=0x%08x, int bytesneeded=%d)\n", memarena, bytesneeded );

    GSL_MEMARENA_VALIDATE(memarena);

    if (bytesneeded < 1)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Illegal number of bytes needed.\n" );
        DEBUG_ASSERT(0);
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_checkfreeblock. Return value: %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    err = mutex_lock_interruptible(&memarena->lock);
    if (err == -EINTR) {
	kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
			"WARNING: memarena mutex lock was interrupted\n");
    }

    p = memarena->freelist.head;
    do
    {
        if (p->blksize >= (unsigned int)bytesneeded)
        {
            kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_checkfreeblock. Return value: %B\n", GSL_SUCCESS );
            return (GSL_SUCCESS);
        }

        p = p->next;
    } while (p != memarena->freelist.head);

    mutex_unlock(&memarena->lock);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_checkfreeblock. Return value: %B\n", GSL_FAILURE );

    return (GSL_FAILURE);
}

//----------------------------------------------------------------------------

int
kgsl_memarena_alloc(gsl_memarena_t *memarena, gsl_flags_t flags, int size, gsl_memdesc_t *memdesc)
{
    int           result = GSL_FAILURE_OUTOFMEM;
    memblk_t      *ptrfree, *ptrlast, *p;
    unsigned int  blksize;
    unsigned int  baseaddr, alignedbaseaddr, alignfragment;
    int           freeblk, alignmentshift;
    memblk_t *ptrbest = NULL;
    unsigned int fitsize = ~0UL;
    int		  err;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_memarena_alloc(gsl_memarena_t *memarena=0x%08x, gsl_flags_t flags=%x, int size=%d, gsl_memdesc_t *memdesc=%M)\n", memarena, flags, size, memdesc );

    GSL_MEMARENA_VALIDATE(memarena);

    if (size <= 0)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Invalid size for memory allocation.\n" );
        DEBUG_ASSERT(0);
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_alloc. Return value: %B\n", GSL_FAILURE );
        return (GSL_FAILURE);
    }

    //
    // go through the list of free blocks.  check to find block which can satisfy the alloc request
    //
    // if no block can satisfy the alloc request this implies that the memory is too fragmented
    // and the requestor needs to free up other memory blocks and re-request the allocation
    //
    // if we do find a block that can satisfy the alloc request then reduce the size of free block
    // by blksize and return the address after allocating the memory.  if the free block size becomes
    // 0 then remove this node from the free list
    //
    // there would be no node on the free list if all available memory were to be allocated.
    // handling an empty list would require executing error checking code in the main branch which
    // is not desired.  instead, the free list will have at least one node at all times. This node
    // could have a block size of zero
    //
    // we use a next fit allocation mechanism that uses a roving pointer on a circular free block list.
    // the pointer is advanced along the chain when searching for a fit. Thus each allocation begins
    // looking where the previous one finished.
    //

    // when allocating from external memory aperture, round up size of requested block to multiple of page size if needed
        if ((flags & GSL_MEMFLAGS_FORCEPAGESIZE) || GSL_MEMARENA_IS_MMU_VIRTUALIZED)
        {
            if (size & (GSL_PAGESIZE-1))
            {
                size = ((size >> GSL_PAGESIZE_SHIFT) + 1) << GSL_PAGESIZE_SHIFT;
            }
        }

    // determine shift count for alignment requested
    alignmentshift = gsl_memarena_alignmentshift(flags);

    // adjust size of requested block to include alignment
    blksize = (unsigned int)((size + ((1 << alignmentshift) - 1)) >> alignmentshift) << alignmentshift;

    err = mutex_lock_interruptible(&memarena->lock);
    if (err == -EINTR) {
	kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
			"WARNING: memarena mutex lock was interrupted\n");
    }

    // check consistency, debug only
    KGSL_DEBUG(GSL_DBGFLAGS_MEMMGR, kgsl_memarena_checkconsistency(memarena));

    ptrfree = memarena->freelist.allocrover;
    ptrlast = memarena->freelist.head->prev;
    freeblk = 0;

    do
    {
	int aba;
        // align base address
        baseaddr = ptrfree->blkaddr + memarena->gpubaseaddr;
	aba = gsl_memarena_alignaddr(baseaddr, alignmentshift);

	if (((aba - baseaddr) == 0) && (ptrfree->blksize == blksize)) {
		ptrbest = ptrfree;
		alignfragment = aba - baseaddr;
		alignedbaseaddr = aba;
		result = GSL_SUCCESS;
		break;
	}

        if ((ptrfree->blksize >= (blksize + aba - baseaddr)) &&
	   (fitsize > ptrfree->blksize))
        {
	    fitsize = ptrfree->blksize;
	    alignfragment = aba - baseaddr;
	    alignedbaseaddr = aba;
            result  = GSL_SUCCESS;
	    ptrbest = ptrfree;
	}

	ptrfree = ptrfree->next;
    } while (ptrfree != memarena->freelist.allocrover);

   if (ptrbest) {
            memdesc->gpuaddr = alignedbaseaddr;
            memdesc->hostptr = kgsl_memarena_gethostptr(memarena, memdesc->gpuaddr);
            memdesc->size    = blksize;

            if (alignfragment > 0)
            {
                // insert new node to handle newly created (small) fragment
                p = kgsl_memarena_getmemblknode(memarena);
                p->blkaddr = ptrbest->blkaddr;
                p->blksize = alignfragment;

                p->next = ptrbest;
                p->prev = ptrbest->prev;
                ptrbest->prev->next = p;
                ptrbest->prev       = p;

                if (ptrbest == memarena->freelist.head)
                {
                    memarena->freelist.head = p;
                }
            }

            ptrbest->blkaddr += alignfragment + blksize;
            ptrbest->blksize -= alignfragment + blksize;

            memarena->freelist.allocrover = ptrbest;

            if ((ptrbest->blksize == 0) && (ptrbest != ptrlast))
            {
                ptrbest->prev->next = ptrbest->next;
                ptrbest->next->prev = ptrbest->prev;
                if (ptrbest == memarena->freelist.head)
                {
                    memarena->freelist.head = ptrbest->next;
                }
                if (ptrbest == memarena->freelist.allocrover)
                {
                    memarena->freelist.allocrover = ptrbest->next;
                }
                if (ptrbest == memarena->freelist.freerover)
                {
                    memarena->freelist.freerover = ptrbest->prev;
                }
                p       = ptrbest;
                ptrfree = ptrbest->prev;
                kgsl_memarena_releasememblknode(memarena, p);
            }
        }

    mutex_unlock(&memarena->lock);

    if (result == GSL_SUCCESS)
    {
        GSL_MEMARENA_STATS(
        {
            int i = 0;
            while (memdesc->size >> (GSL_PAGESIZE_SHIFT + i))
            {
                i++;
            }
            i = i > (GSL_MEMARENA_PAGE_DIST_MAX-1) ? (GSL_MEMARENA_PAGE_DIST_MAX-1) : i;
            memarena->stats.allocs_pagedistribution[i]++;
        });

        GSL_MEMARENA_STATS(memarena->stats.allocs_success++);
    }
    else
    {
        GSL_MEMARENA_STATS(memarena->stats.allocs_fail++);
    }

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_alloc. Return value: %B\n", result );

    return (result);
}

//----------------------------------------------------------------------------

void
kgsl_memarena_free(gsl_memarena_t *memarena, gsl_memdesc_t *memdesc)
{
    //
    // request to free a malloc'ed block from the memory arena
    // add this block to the free list
    // adding a block to the free list requires the following:
    // going through the list of free blocks to decide where to add this free block (based on address)
    // coalesce free blocks
    //
    memblk_t      *ptrfree, *ptrend, *p;
    int           mallocfreeblk, clockwise;
    unsigned int  addrtofree;
    int           err;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> void kgsl_memarena_free(gsl_memarena_t *memarena=0x%08x, gsl_memdesc_t *memdesc=%M)\n", memarena, memdesc );

    DEBUG_ASSERT(memarena);
    if (GSL_MEMARENA_GET_SIGNATURE != GSL_MEMARENA_INSTANCE_SIGNATURE)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_free.\n" );
        return;
    }

    // check size of malloc'ed block
    if (memdesc->size <= 0)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_ERROR, "ERROR: Illegal size for the memdesc.\n" );
        DEBUG_ASSERT(0);

        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_free.\n" );
        return;
    }

    // check address range
    DEBUG_ASSERT( memarena->gpubaseaddr <= memdesc->gpuaddr);
    DEBUG_ASSERT((memarena->gpubaseaddr + memarena->sizebytes) >= memdesc->gpuaddr + memdesc->size);

    err = mutex_lock_interruptible(&memarena->lock);
    if (err == -EINTR) {
	kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
			"WARNING: memarena mutex lock was interrupted\n");
    }

    // check consistency of memory map, debug only
    KGSL_DEBUG(GSL_DBGFLAGS_MEMMGR, kgsl_memarena_checkconsistency(memarena));

    addrtofree    = memdesc->gpuaddr - memarena->gpubaseaddr;
    mallocfreeblk = 1;

    if (addrtofree < memarena->freelist.head->blkaddr)
    {
        // add node to head of free list

        if (addrtofree + memdesc->size == memarena->freelist.head->blkaddr)
        {
            memarena->freelist.head->blkaddr  = addrtofree;
            memarena->freelist.head->blksize += memdesc->size;

            mallocfreeblk = 0;
        }

        ptrfree = memarena->freelist.head->prev;
    }
    else if (addrtofree >= memarena->freelist.head->prev->blkaddr)
    {
        // add node to tail of free list

        ptrfree = memarena->freelist.head->prev;

        if (ptrfree->blkaddr + ptrfree->blksize == addrtofree)
        {
            ptrfree->blksize += memdesc->size;

            mallocfreeblk = 0;
        }
    }
    else
    {
        // determine range of free list nodes to traverse and orientation in which to traverse them
        // keep this code segment unrolled for performance reasons!
        if (addrtofree > memarena->freelist.freerover->blkaddr)
        {
            if (addrtofree - memarena->freelist.freerover->blkaddr < memarena->freelist.head->prev->blkaddr - addrtofree)
            {
                ptrfree   = memarena->freelist.freerover;       // rover
                ptrend    = memarena->freelist.head->prev;      // tail
                clockwise = 1;
            }
            else
            {
                ptrfree   = memarena->freelist.head->prev->prev; // tail
                ptrend    = memarena->freelist.freerover->prev;  // rover
                clockwise = 0;
            }
        }
        else
        {
            if (addrtofree - memarena->freelist.head->blkaddr < memarena->freelist.freerover->blkaddr - addrtofree)
            {
                ptrfree   = memarena->freelist.head;            // head
                ptrend    = memarena->freelist.freerover;       // rover
                clockwise = 1;
            }
            else
            {
                ptrfree   = memarena->freelist.freerover->prev; // rover
                ptrend    = memarena->freelist.head->prev;      // head
                clockwise = 0;
            }
        }

        // traverse the nodes
        do
        {
            if ((addrtofree >= ptrfree->blkaddr + ptrfree->blksize) &&
                (addrtofree + memdesc->size <= ptrfree->next->blkaddr))
            {
                if (addrtofree == ptrfree->blkaddr + ptrfree->blksize)
                {
                    memblk_t  *next;

                    ptrfree->blksize += memdesc->size;
                    next              = ptrfree->next;

                    if (ptrfree->blkaddr + ptrfree->blksize == next->blkaddr)
                    {
                        ptrfree->blksize += next->blksize;
                        ptrfree->next     = next->next;
                        next->next->prev  = ptrfree;

                        if (next == memarena->freelist.allocrover)
                        {
                            memarena->freelist.allocrover = ptrfree;
                        }

                        kgsl_memarena_releasememblknode(memarena, next);
                    }

                    mallocfreeblk = 0;
                }
                else if (addrtofree + memdesc->size == ptrfree->next->blkaddr)
                {
                    ptrfree->next->blkaddr  = addrtofree;
                    ptrfree->next->blksize += memdesc->size;

                    mallocfreeblk  = 0;
                }

                break;
            }

            if (clockwise)
            {
                ptrfree = ptrfree->next;
            }
            else
            {
                ptrfree = ptrfree->prev;
            }

        } while (ptrfree != ptrend);
    }

    // this free block could not be coalesced, so create a new free block
    // and add it to the free list in the memory arena
    if (mallocfreeblk)
    {
        p = kgsl_memarena_getmemblknode(memarena);
        p->blkaddr = addrtofree;
        p->blksize = memdesc->size;

        p->next             = ptrfree->next;
        p->prev             = ptrfree;
        ptrfree->next->prev = p;
        ptrfree->next       = p;

        if (p->blkaddr < memarena->freelist.head->blkaddr)
        {
            memarena->freelist.head = p;
        }

        memarena->freelist.freerover = p;
    }
    else
    {
        memarena->freelist.freerover = ptrfree;
    }

    mutex_unlock(&memarena->lock);

    GSL_MEMARENA_STATS(
    {
        int i = 0;
        while (memdesc->size >> (GSL_PAGESIZE_SHIFT + i))
        {
            i++;
        }
        i = i > (GSL_MEMARENA_PAGE_DIST_MAX-1) ? (GSL_MEMARENA_PAGE_DIST_MAX-1) : i;
        memarena->stats.frees_pagedistribution[i]++;
    });

    GSL_MEMARENA_STATS(memarena->stats.frees++);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_free.\n" );
}

//----------------------------------------------------------------------------

void *kgsl_memarena_gethostptr(gsl_memarena_t *memarena, gpuaddr_t gpuaddr)
{
    //
    // get the host mapped address for a hardware device address
    //

    void  *hostptr = NULL;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> void* kgsl_memarena_gethostptr(gsl_memarena_t *memarena=0x%08x, gpuaddr_t gpuaddr=0x%08x)\n", memarena, gpuaddr );

    DEBUG_ASSERT(memarena);
    if (GSL_MEMARENA_GET_SIGNATURE != GSL_MEMARENA_INSTANCE_SIGNATURE)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_gethostptr. Return value: 0x%08x\n", NULL );
        return (NULL);
    }

    // check address range
    DEBUG_ASSERT(gpuaddr >= memarena->gpubaseaddr);
    DEBUG_ASSERT(gpuaddr <  memarena->gpubaseaddr + memarena->sizebytes);

    hostptr = (void *)((gpuaddr - memarena->gpubaseaddr) + memarena->hostbaseaddr);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_gethostptr. Return value: 0x%08x\n", hostptr );

    return (hostptr);
}

//----------------------------------------------------------------------------

gpuaddr_t kgsl_memarena_getgpuaddr(gsl_memarena_t *memarena, void *hostptr)
{
    //
    // get the hardware device address for a host mapped address
    //

    gpuaddr_t  gpuaddr = 0;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> int kgsl_memarena_getgpuaddr(gsl_memarena_t *memarena=0x%08x, void *hostptr=0x%08x)\n", memarena, hostptr );

    DEBUG_ASSERT(memarena);
    if (GSL_MEMARENA_GET_SIGNATURE != GSL_MEMARENA_INSTANCE_SIGNATURE)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_getgpuaddr. Return value: 0x%08x\n", 0 );
        return (0);
    }

    // check address range
    DEBUG_ASSERT(hostptr >= (void *)memarena->hostbaseaddr);
    DEBUG_ASSERT(hostptr <  (void *)(memarena->hostbaseaddr + memarena->sizebytes));

    gpuaddr = ((unsigned int)hostptr - memarena->hostbaseaddr) + memarena->gpubaseaddr;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_getgpuaddr. Return value: 0x%08x\n", gpuaddr );

    return (gpuaddr);
}

//----------------------------------------------------------------------------

unsigned int
kgsl_memarena_getlargestfreeblock(gsl_memarena_t *memarena, gsl_flags_t flags)
{
    memblk_t      *ptrfree;
    unsigned int  blocksize, largestblocksize = 0;
    int           alignmentshift;
    int           err;

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE,
                    "--> unsigned int kgsl_memarena_getlargestfreeblock(gsl_memarena_t *memarena=0x%08x, gsl_flags_t flags=%x)\n", memarena, flags );

    DEBUG_ASSERT(memarena);
    if (GSL_MEMARENA_GET_SIGNATURE != GSL_MEMARENA_INSTANCE_SIGNATURE)
    {
        kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_getlargestfreeblock. Return value: %d\n", 0 );
        return (0);
    }

    // determine shift count for alignment requested
    alignmentshift = gsl_memarena_alignmentshift(flags);

    err = mutex_lock_interruptible(&memarena->lock);
    if (err == -EINTR) {
	kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_FATAL,
			"WARNING: memarena mutex lock was interrupted\n");
    }

    ptrfree = memarena->freelist.head;

    do
    {
        blocksize = ptrfree->blksize - (ptrfree->blkaddr - ((ptrfree->blkaddr >> alignmentshift) << alignmentshift));

        if (blocksize > largestblocksize)
        {
            largestblocksize = blocksize;
        }

        ptrfree = ptrfree->next;

    } while (ptrfree != memarena->freelist.head);

    mutex_unlock(&memarena->lock);

    kgsl_log_write( KGSL_LOG_GROUP_MEMORY | KGSL_LOG_LEVEL_TRACE, "<-- kgsl_memarena_getlargestfreeblock. Return value: %d\n", largestblocksize );

    return (largestblocksize);
}
