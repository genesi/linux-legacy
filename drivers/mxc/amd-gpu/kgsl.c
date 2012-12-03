/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include <linux/fsl_devices.h>

#include <linux/mxc_kgsl.h>

#include "kgsl_types.h"
#include "kgsl_linux_map.h"
#include "kgsl_driver.h"

#include "kgsl_device.h"

#include "kgsl_sharedmem.h"
#include "kgsl_cmdstream.h"
#include "kgsl_g12_cmdwindow.h"

#include "kgsl_halconfig.h"

struct kgsl_alloc_list
{
	struct list_head node;
	struct kgsl_memdesc allocated_block;
	u32 allocation_number;
};

struct kgsl_file_private
{
	struct list_head allocated_blocks_head; // list head
	u32 maximum_number_of_blocks;
	u32 number_of_allocated_blocks;
	s8 created_contexts_array[KGSL_DEVICE_MAX][KGSL_CONTEXT_MAX];
};

static const s8 EMPTY_ENTRY = -1;





/*
 * Local helper functions to check and convert device/context id's (1 based)
 * to index (0 based).
 */
static u32 device_id_to_device_index(unsigned int device_id)
{
    DEBUG_ASSERT((KGSL_DEVICE_ANY < device_id) && 
               (device_id <= KGSL_DEVICE_MAX));
    return (u32)(device_id - 1);
}

/* 
 * Local helper function to check and get pointer to per file descriptor data 
 */
static struct kgsl_file_private *get_fd_private_data(struct file *fd)
{
    struct kgsl_file_private *datp; 

    DEBUG_ASSERT(fd);
    datp = (struct kgsl_file_private *)fd->private_data;
    DEBUG_ASSERT(datp);
    return datp;
}

static s8 *find_first_entry_with(s8 *subarray, s8 context_id)
{
    s8 *entry = NULL;
    int i;

//printk(KERN_DEBUG "At %s, ctx_id = %d\n", __func__, context_id);

    DEBUG_ASSERT(context_id >= EMPTY_ENTRY);
    DEBUG_ASSERT(context_id <= KGSL_CONTEXT_MAX);  // TODO: check the bound.

    for(i = 0; i < KGSL_CONTEXT_MAX; i++)        // TODO: check the bound.
    {
        if(subarray[i] == (s8)context_id)
        {
            entry = &subarray[i];
            break;
        }
    }

    return entry;
}


/*
 * Add a memdesc into a list of allocated memory blocks for this file 
 * descriptor. The list is build in such a way that it implements FIFO (i.e.
 * list). Traces of tiger, tiger_ri and VG11 CTs should be analysed to make
 * informed choice.
 *
 * NOTE! struct kgsl_memdescs are COPIED so user space should NOT change them.
 */
int add_memblock_to_allocated_list(struct file *fd,
                                   struct kgsl_memdesc *allocated_block)
{
    int err = 0;
    struct kgsl_file_private *datp;
    struct kgsl_alloc_list *lisp;
    struct list_head *head;

    DEBUG_ASSERT(allocated_block);

    datp = get_fd_private_data(fd);

    head = &datp->allocated_blocks_head;
    DEBUG_ASSERT(head);

    /* allocate and put new entry in the list of allocated memory descriptors */
    lisp = (struct kgsl_alloc_list *)kzalloc(sizeof(struct kgsl_alloc_list), GFP_KERNEL);
    if(lisp)
    {
        INIT_LIST_HEAD(&lisp->node);

        /* builds FIFO (list_add() would build LIFO) */
        list_add_tail(&lisp->node, head);
        memcpy(&lisp->allocated_block, allocated_block, sizeof(struct kgsl_memdesc));
        lisp->allocation_number = datp->maximum_number_of_blocks;
//        printk(KERN_DEBUG "List entry #%u allocated\n", lisp->allocation_number);

        datp->maximum_number_of_blocks++;
        datp->number_of_allocated_blocks++;

        err = 0;
    }
    else
    {
        printk(KERN_ERR "%s: Could not allocate new list element\n", __func__);
        err = -ENOMEM;
    }

    return err;
}

/* Delete a previously allocated memdesc from a list of allocated memory blocks */
int del_memblock_from_allocated_list(struct file *fd,
                                     struct kgsl_memdesc *freed_block)
{
    struct kgsl_file_private *datp;
    struct kgsl_alloc_list *cursor, *next;
    struct list_head *head;
//    int is_different;

    DEBUG_ASSERT(freed_block);

    datp = get_fd_private_data(fd);

    head = &datp->allocated_blocks_head;
    DEBUG_ASSERT(head);

    DEBUG_ASSERT(datp->number_of_allocated_blocks > 0);

    if(!list_empty(head))
    {
        list_for_each_entry_safe(cursor, next, head, node)
        {
            if(cursor->allocated_block.gpuaddr == freed_block->gpuaddr)
            {
//                is_different = memcmp(&cursor->allocated_block, freed_block, sizeof(struct kgsl_memdesc));
//                DEBUG_ASSERT(!is_different);

                list_del(&cursor->node);
//                printk(KERN_DEBUG "List entry #%u freed\n", cursor->allocation_number);
                kfree(cursor);
                datp->number_of_allocated_blocks--;
                return 0;
            }
        }
    }
    return -EINVAL; // tried to free entry not existing or from empty list.
}

/* Delete all previously allocated memdescs from a list */
int del_all_memblocks_from_allocated_list(struct file *fd)
{
    struct kgsl_file_private *datp;
    struct kgsl_alloc_list *cursor, *next;
    struct list_head *head;

    datp = get_fd_private_data(fd);

    head = &datp->allocated_blocks_head;
    DEBUG_ASSERT(head);

    if(!list_empty(head))
    {
        printk(KERN_INFO "Not all allocated memory blocks were freed. Doing it now.\n");
        list_for_each_entry_safe(cursor, next, head, node)
        {
            printk(KERN_INFO "Freeing list entry #%u, gpuaddr=%x\n", (u32)cursor->allocation_number, cursor->allocated_block.gpuaddr);
            kgsl_sharedmem_free(&cursor->allocated_block);
            list_del(&cursor->node);
            kfree(cursor);
        }
    }

    DEBUG_ASSERT(list_empty(head));
    datp->number_of_allocated_blocks = 0;

    return 0;
}

void init_created_contexts_array(s8 *array)
{
    memset((void*)array, EMPTY_ENTRY, KGSL_DEVICE_MAX * KGSL_CONTEXT_MAX);
}


void add_device_context_to_array(struct file *fd,
                                 unsigned int device_id,
                                 unsigned int context_id)
{
    struct kgsl_file_private *datp;
    s8 *entry;
    s8 *subarray;
    u32 device_index = device_id_to_device_index(device_id);

    datp = get_fd_private_data(fd);

    subarray = datp->created_contexts_array[device_index];
    entry = find_first_entry_with(subarray, EMPTY_ENTRY);

    DEBUG_ASSERT(entry);
    DEBUG_ASSERT((datp->created_contexts_array[device_index] <= entry) &&
               (entry < datp->created_contexts_array[device_index] + KGSL_CONTEXT_MAX));
    DEBUG_ASSERT(context_id < 127);
    *entry = (s8)context_id;
}

void del_device_context_from_array(struct file *fd, 
                                   unsigned int device_id,
                                   unsigned int context_id)
{
    struct kgsl_file_private *datp;
    u32 device_index = device_id_to_device_index(device_id);
    s8 *entry;
    s8 *subarray;

    datp = get_fd_private_data(fd);

    DEBUG_ASSERT(context_id < 127);
    subarray = &(datp->created_contexts_array[device_index][0]);
    entry = find_first_entry_with(subarray, context_id);
    DEBUG_ASSERT(entry);
    DEBUG_ASSERT((datp->created_contexts_array[device_index] <= entry) &&
               (entry < datp->created_contexts_array[device_index] + GSL_CONTEXT_MAX));
    *entry = EMPTY_ENTRY;
}

void del_all_devices_contexts(struct file *fd)
{
    struct kgsl_file_private *datp;
    unsigned int id;
    u32 device_index;
    u32 ctx_array_index;
    s8 ctx;
    int err;

    datp = get_fd_private_data(fd);

    /* device_id is 1 based */
    for(id = KGSL_DEVICE_ANY + 1; id <= KGSL_DEVICE_MAX; id++)
    {
        device_index = device_id_to_device_index(id);
        for(ctx_array_index = 0; ctx_array_index < KGSL_CONTEXT_MAX; ctx_array_index++)
        {
            ctx = datp->created_contexts_array[device_index][ctx_array_index];
            if(ctx != EMPTY_ENTRY)
            {
		struct kgsl_device *device = &gsl_driver.device[id];

		if (device && device->ftbl.device_drawctxt_destroy) {
			mutex_lock(&gsl_driver.lock);
        	        err = device->ftbl.device_drawctxt_destroy(device, ctx);
			mutex_unlock(&gsl_driver.lock);
		} else
			err = GSL_FAILURE;

                if(err != GSL_SUCCESS)
                {
                    printk(KERN_ERR "%s: could not destroy context %d on device id = %u\n", __func__, ctx, id);
                }
                else
                {
                    printk(KERN_DEBUG "%s: Destroyed context %d on device id = %u\n", __func__, ctx, id);
                }
            }
        }
    }
}




















//#define GSL_IOCTL_DEBUG

int gpu_2d_irq, gpu_3d_irq;

phys_addr_t gpu_2d_regbase;
int gpu_2d_regsize;
phys_addr_t gpu_3d_regbase;
int gpu_3d_regsize;
int gmem_size;
phys_addr_t gpu_reserved_mem;
int gpu_reserved_mem_size;
int z160_version;
int enable_mmu;

static ssize_t kgsl_read(struct file *fd, char __user *buf, size_t len, loff_t *ptr);
static ssize_t kgsl_write(struct file *fd, const char __user *buf, size_t len, loff_t *ptr);
static int kgsl_ioctl(struct inode *inode, struct file *fd, unsigned int cmd, unsigned long arg);
static int kgsl_mmap(struct file *fd, struct vm_area_struct *vma);
static int kgsl_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
static int kgsl_open(struct inode *inode, struct file *fd);
static int kgsl_release(struct inode *inode, struct file *fd);
static irqreturn_t z160_irq_handler(int irq, void *dev_id);
static irqreturn_t z430_irq_handler(int irq, void *dev_id);

static int gsl_kmod_major;
static struct class *gsl_kmod_class;
DEFINE_MUTEX(gsl_mutex);

static const struct file_operations kgsl_fops =
{
	.owner = THIS_MODULE,
	.ioctl = kgsl_ioctl,
	.mmap = kgsl_mmap,
	.open = kgsl_open,
	.release = kgsl_release
	/* good chance these aren't required to be set */
	//.read = kgsl_read,
	//.write = kgsl_write,
};

/* qcom: doesn't do this */
static struct vm_operations_struct kgsl_vmops =
{
	.fault = kgsl_fault,
};

static ssize_t kgsl_read(struct file *fd, char __user *buf, size_t len, loff_t *ptr)
{
    return 0;
}

static ssize_t kgsl_write(struct file *fd, const char __user *buf, size_t len, loff_t *ptr)
{
    return 0;
}

static int kgsl_ioctl_cmdwindow_write(struct file *fd, void __user *arg)
{
	int result = GSL_SUCCESS;
	struct kgsl_cmdwindow_write param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = GSL_FAILURE; // -EFAULT
		goto done;
	}
	if (param.device_id == KGSL_DEVICE_YAMATO) {
		result = GSL_FAILURE; // -EINVAL
	} else if (param.device_id == KGSL_DEVICE_G12) {
		// qcom: pre-hw-access, kgsl_g12_cmdwindow_write
		struct kgsl_device *device = &gsl_driver.device[param.device_id-1];
		result = kgsl_g12_cmdwindow_write0(device,
				param.target, param.addr, param.data);
	} else {
		result = GSL_FAILURE; // -EINVAL
	}

	if (result != GSL_SUCCESS)
		goto done;

/* qcom:
	if (copy_to_user(arg, &param, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}
*/

done:
	return result;
}

static int kgsl_ioctl_device_getproperty(struct file *fd, void __user *arg)
{
	int result = GSL_SUCCESS;
	struct kgsl_device_getproperty param;
	void *tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = GSL_FAILURE; // -EFAULT
		goto done;
	}

	tmp = kmalloc(param.sizebytes, GFP_KERNEL);
	if (!tmp) {
                pr_err("%s:kmalloc error\n", __func__);
                result = GSL_FAILURE;
                goto done;
	}

	/* qcom:
	 * - call yamato_getproperty or g12_getproperty
	 * - those functions allocate return value on stack
	 * - copy_to_user the result
	 * - no idea how FSL code is even meant to work here!? does hwaccess_memread do it?
	 */
	result = kgsl_device_getproperty(param.device_id, param.type, tmp, param.sizebytes);

	kfree(tmp);
done:
	return result;
}


static int kgsl_ioctl_device_setproperty(struct file *fd, void __user *arg)
{
	int result;
	struct kgsl_device_setproperty param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = GSL_FAILURE; // -EFAULT
		goto done;
	}

	result = kgsl_device_setproperty(param.device_id, param.type, param.value, param.sizebytes);

done:
	return result;
}

static int kgsl_ioctl_device_regread(struct file *fd, void __user *arg)
{
	int status = GSL_SUCCESS;
	struct kgsl_device_regread param;
	unsigned int tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
		status = GSL_FAILURE; // -EFAULT
		goto done;
	}

	/* qcom: call direct predicated on device id. g12 has pre-hw-access */
	status = kgsl_device_regread(param.device_id, param.offsetwords, &tmp);

	if (status == GSL_SUCCESS) {
		/* qcom: arg, &param, sizeof(param) */
		/* fsl code should never work? */
		if (copy_to_user(param.value, &tmp, sizeof(unsigned int))) {
			pr_err("%s: copy_to_user error\n", __func__);
			status = GSL_FAILURE;
                }
	}
done:
	return status;
}

/* not implemented in qcom kernel? */
static int kgsl_ioctl_device_regwrite(struct file *fd, void __user *arg)
{
	int status = GSL_SUCCESS;
	struct kgsl_device_regwrite param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	status = kgsl_device_regwrite(param.device_id, param.offsetwords, param.value);

	return status;
}

/* device_waittimestamp in qcom */
static int kgsl_ioctl_cmdstream_waittimestamp(struct file *fd, void __user *arg)
{
	int result = GSL_SUCCESS;
	struct kgsl_cmdstream_waittimestamp param;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	/* qcom: per-device waittimestamp, driver lock around function, runpending to finish */
	result = kgsl_cmdstream_waittimestamp(param.device_id, param.timestamp, param.timeout);

	return result;
}

/* ringbuffer_issueibcmds in qcom */
static int kgsl_ioctl_cmdstream_issueibcmds(struct file *fd, void __user *arg)
{
	int status = GSL_SUCCESS;
	struct kgsl_cmdstream_issueibcmds param;
	unsigned int tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	/* qcom: per-device opencoded */
	status = kgsl_cmdstream_issueibcmds(param.device_id, param.drawctxt_index,
						param.ibaddr, param.sizedwords, &tmp, param.flags);

	if (status == GSL_SUCCESS) {
		/* don't understand how copying it to param.timestamp gets it back into userspace...
		   this is not how the newer drivers work or how any driver does it for real..? */
		if (copy_to_user(param.timestamp, &tmp, sizeof(unsigned int))) {
			pr_err("%s: copy_to_user error\n", __func__);
			status = GSL_FAILURE;
                }
	}

	return status;
}

static int kgsl_ioctl_sharedmem_alloc(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_alloc param;
	struct kgsl_memdesc tmp;
	int status;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	/* alternative for us: lock, alloc0, unlock */
	mutex_lock(&gsl_driver.lock);
	status = kgsl_sharedmem_alloc0(param.device_id, param.flags, param.sizebytes, &tmp);
	mutex_unlock(&gsl_driver.lock);

	if (status == GSL_SUCCESS) {
		if (copy_to_user(param.memdesc, &tmp, sizeof(struct kgsl_memdesc))) {
			kgsl_sharedmem_free(&tmp); // check result?

	                pr_err("%s: copy_to_user error\n", __func__);
			return GSL_FAILURE;
		}
		add_memblock_to_allocated_list(fd, &tmp);
	} else {
		pr_err("%s (id=%d,flags=%x,size=%d) failed!\n", __func__,
				param.device_id, param.flags, param.sizebytes);
        }

	return status;
}

static int kgsl_ioctl_sharedmem_free(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_free param;
	struct kgsl_memdesc tmp;
	int status;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, param.memdesc, sizeof(tmp))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	/* qcom: kgsl_sharedmem_find => kgsl_remove_mem_entry */
	status = del_memblock_from_allocated_list(fd, &tmp);
	if (status) {
		pr_err("%s: tried to free memdesc that was not allocated!\n", __func__);
	}

	status = kgsl_sharedmem_free(&tmp);
	if (status == GSL_SUCCESS) {
		if (copy_to_user(param.memdesc, &tmp, sizeof(tmp))) {
        	        pr_err("%s: copy_to_user error\n", __func__);
			return GSL_FAILURE;
		}
	}

	return status;
}

static int kgsl_ioctl_sharedmem_read(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_read param;
	struct kgsl_memdesc tmp;
	int status;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, param.memdesc, sizeof(tmp))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	status = kgsl_sharedmem_read(&tmp, param.dst, param.offsetbytes, param.sizebytes, true);
	if (status != GSL_SUCCESS) {
                pr_err("%s: kgsl_sharedmem_read failed\n", __func__);
	}

	return status;
}

static int kgsl_ioctl_sharedmem_write(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_write param;
	struct kgsl_memdesc tmp;
	int status;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, param.memdesc, sizeof(tmp))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	status = kgsl_sharedmem_write(&tmp, param.offsetbytes, param.src, param.sizebytes, true);
	if (status != GSL_SUCCESS) {
                pr_err("%s: kgsl_sharedmem_write failed\n", __func__);
	}

	return status;
}

static int kgsl_ioctl_sharedmem_set(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_set param;
	struct kgsl_memdesc tmp;
	int status;

	if (copy_from_user(&param, arg, sizeof(param))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, param.memdesc, sizeof(tmp))) {
                pr_err("%s: copy_from_user error\n", __func__);
		return GSL_FAILURE;
	}

	status = kgsl_sharedmem_set(&tmp, param.offsetbytes, param.value, param.sizebytes);
	if (status != GSL_SUCCESS) {
                pr_err("%s: kgsl_sharedmem_set failed\n", __func__);
	}

	return status;
}

static int kgsl_ioctl_device_start(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_device_start param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_device_start(param.device_id, param.flags);
	return status;
}

static int kgsl_ioctl_device_stop(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_device_stop param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_device_stop(param.device_id);
	return status;
}

static int kgsl_ioctl_device_idle(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_device_idle param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_device_idle(param.device_id, param.timeout);
	return status;
}

static int kgsl_ioctl_device_isidle(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_device_isidle param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_device_isidle(param.device_id);
	return status;
}

static int kgsl_ioctl_device_clock(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_device_clock param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_device_clock(param.device, param.enable);
	return status;
}

static int kgsl_ioctl_cmdstream_readtimestamp(struct file *fd, void __user *arg)
{
	int status = GSL_SUCCESS;
	struct kgsl_cmdstream_readtimestamp param;
	unsigned int tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	/* qcom: per device here */
	tmp = kgsl_cmdstream_readtimestamp(param.device_id, param.type);
	if (copy_to_user(param.timestamp, &tmp, sizeof(unsigned int))) {
		pr_err("%s: copy_to_user error\n", __func__);
		status = GSL_FAILURE;
	}
	return status;
}

static int kgsl_ioctl_cmdstream_freememontimestamp(struct file *fd, void __user *arg)
{
	int status = GSL_SUCCESS;
	struct kgsl_cmdstream_freememontimestamp param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	/* qcom: per device */
	status = del_memblock_from_allocated_list(fd, param.memdesc);
	if (status) {
		/* tried to remove a block of memory that is not allocated!
		 * NOTE that -EINVAL is Linux kernel's error codes!
		 * the drivers error codes COULD mix up with kernel's.
		 */
                status = -EINVAL;
	} else {
		status = kgsl_cmdstream_freememontimestamp(param.device_id,
							param.memdesc,
							param.timestamp,
							param.type);
		/* qcom: runpending here */
	}
	return status;
}

static int kgsl_ioctl_context_create(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_context_create param;
	unsigned int tmp;
	struct kgsl_device *device;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	/* qcom: per device */
	device = &gsl_driver.device[param.device_id-1];
	mutex_lock(&gsl_driver.lock);
	status = device->ftbl.device_drawctxt_create(device, param.type, &tmp, param.flags);
	mutex_unlock(&gsl_driver.lock);

	if (status == GSL_SUCCESS) {
		if (copy_to_user(param.drawctxt_id, &tmp, sizeof(unsigned int))) {
			mutex_lock(&gsl_driver.lock);
			device->ftbl.device_drawctxt_destroy(device, *param.drawctxt_id); // check result?
			mutex_unlock(&gsl_driver.lock);

			pr_err("%s: copy_to_user error\n", __func__);
			status = GSL_FAILURE;
                } else {
			add_device_context_to_array(fd, param.device_id, tmp);
		}
	}
	return status;
}

static int kgsl_ioctl_context_destroy(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_context_destroy param;
	struct kgsl_device *device;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	device = &gsl_driver.device[param.device_id-1];
	mutex_lock(&gsl_driver.lock);
	status = device->ftbl.device_drawctxt_destroy(device, param.drawctxt_id);
	mutex_unlock(&gsl_driver.lock);

	/* qcom: runpending */

	del_device_context_from_array(fd, param.device_id, param.drawctxt_id);

	return status;
}

static int kgsl_ioctl_sharedmem_largestfreeblock(struct file *fd, void __user *arg)
{
	struct kgsl_sharedmem_largestfreeblock param;
	unsigned int largestfreeblock;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	largestfreeblock = kgsl_sharedmem_largestfreeblock(param.device_id, param.flags);
	if (copy_to_user(param.largestfreeblock, &largestfreeblock, sizeof(unsigned int))) {
		pr_err("%s: copy_to_user error\n", __func__);
                return GSL_FAILURE;
	}

	return GSL_SUCCESS;
}


static int kgsl_ioctl_sharedmem_cacheoperation(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_sharedmem_cacheoperation param;
	struct kgsl_memdesc tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, (void __user *)param.memdesc, sizeof(tmp))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

        status = kgsl_sharedmem_cacheoperation(&tmp, param.offsetbytes, param.sizebytes, param.operation);
	return status;
}

static int kgsl_ioctl_sharedmem_fromhostpointer(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_sharedmem_fromhostpointer param;
	struct kgsl_memdesc tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	if (copy_from_user(&tmp, (void __user *)param.memdesc, sizeof(tmp))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_sharedmem_fromhostpointer(param.device_id, &tmp, param.hostptr);
	return status;
}

static int kgsl_ioctl_drawctxt_bind_gmem_shadow(struct file *fd, void __user *arg)
{
	int status;
	struct kgsl_drawctxt_bind_gmem_shadow param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	status = kgsl_drawctxt_bind_gmem_shadow(param.device_id, param.drawctxt_id,
						param.gmem_rect, param.shadow_x,
						param.shadow_y, param.shadow_buffer,
						param.buffer_id);
	return status;
}

static int kgsl_ioctl_add_timestamp(struct file *fd, void __user *arg)
{
	struct kgsl_add_timestamp param;
	unsigned int tmp;

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy_from_user error\n", __func__);
                return GSL_FAILURE;
	}

	tmp = kgsl_add_timestamp(param.device_id, &tmp);

	if (copy_to_user(param.timestamp, &tmp, sizeof(unsigned int))) {
		pr_err("%s: copy_to_user error\n", __func__);
		return GSL_FAILURE;
	}
	return GSL_SUCCESS;
}

static int kgsl_ioctl(struct inode *inode, struct file *fd, unsigned int cmd, unsigned long arg)
{
	int result = GSL_FAILURE;

//	pr_info("%s: cmd=%08x\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_KGSL_DEVICE_REGREAD:
		result = kgsl_ioctl_device_regread(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_DEVICE_REGWRITE:
		result = kgsl_ioctl_device_regwrite(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_DEVICE_GETPROPERTY:
		result = kgsl_ioctl_device_getproperty(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_DEVICE_SETPROPERTY:
		result = kgsl_ioctl_device_setproperty(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_ALLOC:
		/* qcom: runpending on each device first */
		result = kgsl_ioctl_sharedmem_alloc(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_FREE:
		result = kgsl_ioctl_sharedmem_free(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_READ:
		result = kgsl_ioctl_sharedmem_read(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_WRITE:
		result = kgsl_ioctl_sharedmem_write(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_SET:
		result = kgsl_ioctl_sharedmem_set(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_FROMHOSTPOINTER:
		result = kgsl_ioctl_sharedmem_fromhostpointer(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_LARGESTFREEBLOCK:
		result = kgsl_ioctl_sharedmem_largestfreeblock(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_CACHEOPERATION:
		/* qcom: only if cache enabled (debug option) */
		result = kgsl_ioctl_sharedmem_cacheoperation(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_CMDSTREAM_ISSUEIBCMDS:
		/* qcom: if cache enable, clean all, if drm, mem flush, THEN: */
		result = kgsl_ioctl_cmdstream_issueibcmds(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_CMDSTREAM_WAITTIMESTAMP:
		result = kgsl_ioctl_cmdstream_waittimestamp(fd, (void __user *)arg);
		/* order reads to tbe buffer written to by the GPU */
		rmb();
		break;
	case IOCTL_KGSL_CMDSTREAM_READTIMESTAMP:
		result = kgsl_ioctl_cmdstream_readtimestamp(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP:
		result = kgsl_ioctl_cmdstream_freememontimestamp(fd, (void __user *)arg);
		break;
	case IOCTL_KGSL_CMDWINDOW_WRITE:
		result = kgsl_ioctl_cmdwindow_write(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_CONTEXT_CREATE:
		result = kgsl_ioctl_context_create(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_CONTEXT_DESTROY:
		result = kgsl_ioctl_context_destroy(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DEVICE_STOP:
		result = kgsl_ioctl_device_stop(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DEVICE_START:
		result = kgsl_ioctl_device_start(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DEVICE_IDLE:
		result = kgsl_ioctl_device_idle(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DEVICE_ISIDLE:
		result = kgsl_ioctl_device_isidle(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DEVICE_CLOCK:
		result = kgsl_ioctl_device_clock(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_DRAWCTXT_BIND_GMEM_SHADOW:
		result = kgsl_ioctl_drawctxt_bind_gmem_shadow(fd, (void __user *) arg);
		break;
	case IOCTL_KGSL_ADD_TIMESTAMP:
		result = kgsl_ioctl_add_timestamp(fd, (void __user *) arg);
		break;
	default:
		pr_err("%s: invalid ioctl code %08x\n", __func__, cmd);
		result = -ENOTTY;
		break;
	}

	/* qcom: post hwaccess here (explains why none of their funcs do it..?) */
	return result;
}

static int kgsl_mmap(struct file *fd, struct vm_area_struct *vma)
{
	int result;
	int status = 0;
	unsigned long vma_start = vma->vm_start;
	unsigned long vma_size = vma->vm_end - vma->vm_start;
	unsigned long addr = vma->vm_pgoff << PAGE_SHIFT;
	void *va = NULL;

	/* qcom:
	 * no difference between enable or disable mmu re remap_pfn_range usage
	 * also holds gsl_driver.lock!
	 */

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (gsl_driver.enable_mmu && (addr < GSL_LINUX_MAP_RANGE_END) && (addr >= GSL_LINUX_MAP_RANGE_START)) {
		va = kgsl_sharedmem_find(addr);
		while (vma_size > 0) {
			if (remap_pfn_range(vma, vma_start, vmalloc_to_pfn(va), PAGE_SIZE, vma->vm_page_prot))
				return -EAGAIN;
		}
		vma_start += PAGE_SIZE;
		va += PAGE_SIZE;
		vma_size -= PAGE_SIZE;
	} else {
		if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma_size, vma->vm_page_prot))
			status = -EAGAIN;
	}
	vma->vm_ops = &kgsl_vmops;

	return status;
}

static int kgsl_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}

static int kgsl_open(struct inode *inodep, struct file *filep)
{
	unsigned int flags = 0;
	struct kgsl_file_private *private;
	int result = 0;

	if (filep->f_flags & O_EXCL) {
		return -EBUSY;
	}

	private = kzalloc(sizeof(*private), GFP_KERNEL);
	if (private == NULL) {
		return -ENOMEM;
	}

	if(mutex_lock_interruptible(&gsl_mutex))
		return -EINTR;

	/* qcom: first_open_locked? */
	if (kgsl_driver_entry(flags) != GSL_SUCCESS) {
		pr_info("%s: kgsl_driver_entry error\n", __func__);
		result = -EIO;  // TODO: not sure why did it fail?
	}

	init_created_contexts_array(private->created_contexts_array[0]);
	INIT_LIST_HEAD(&private->allocated_blocks_head);

	filep->private_data = (void *) private;

	mutex_unlock(&gsl_mutex);

	return result;
}

static int kgsl_release(struct inode *inodep, struct file *filep)
{
	struct kgsl_file_private *private;
	int result = 0;

	/* qcom: pre_hwaccess */

	if(mutex_lock_interruptible(&gsl_mutex))
		return -EINTR;

	/* make sure contexts are destroyed */
	/* qcom: walk lists manually here */
	del_all_devices_contexts(filep);

	/* qcom: last_release_locked? */
	if (kgsl_driver_exit() != GSL_SUCCESS) {
		pr_info("%s: kgsl_driver_exit error\n", __func__);
		result = -EIO; // TODO: find better error code
	} else {
		/* release per file descriptor data structure */
		private = (struct kgsl_file_private *)filep->private_data;
		/* qcom: remove_mem_entry for_each_entry_safe */
		del_all_memblocks_from_allocated_list(filep);
		/* qcom: tear down mmu */
		filep->private_data = NULL;
		kfree(private);
	}

	/* qcom: post_hwaccess */
	mutex_unlock(&gsl_mutex);

	return result;
}

static struct class *gsl_kmod_class;

static irqreturn_t z160_irq_handler(int irq, void *dev_id)
{
	kgsl_intr_isr(&gsl_driver.device[KGSL_DEVICE_G12-1]);
	return IRQ_HANDLED;
}

static irqreturn_t z430_irq_handler(int irq, void *dev_id)
{
	kgsl_intr_isr(&gsl_driver.device[KGSL_DEVICE_YAMATO-1]);
	return IRQ_HANDLED;
}

static int gpu_probe(struct platform_device *pdev)
{
    struct resource *res;
    struct device *dev;
    struct mxc_gpu_platform_data *gpu_data = NULL;

    gpu_data = (struct mxc_gpu_platform_data *)pdev->dev.platform_data;

    if (gpu_data == NULL)
	return 0;

    z160_version = gpu_data->z160_revision;
    enable_mmu = gpu_data->enable_mmu;

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "gpu_2d_irq");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find 2D gpu irq\n");
	goto nodev;
    } else {
	gpu_2d_irq = res->start;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "gpu_3d_irq");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find 3D gpu irq\n");
	goto nodev;
    } else {
	gpu_3d_irq = res->start;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_2d_registers");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find 2D gpu registers\n");
	goto nodev;
    } else {
	gpu_2d_regbase = res->start;
	gpu_2d_regsize = res->end - res->start + 1;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_3d_registers");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find 3D gpu registers\n");
	goto nodev;
    } else {
	gpu_3d_regbase = res->start;
	gpu_3d_regsize = res->end - res->start + 1;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_graphics_mem");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find gpu graphics memory\n");
	goto nodev;
    } else {
	gmem_size = res->end - res->start + 1;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_reserved_mem");
    if (!res) {
	printk(KERN_ERR "gpu: unable to find gpu reserved memory\n");
	goto nodev;
    } else {
	gpu_reserved_mem = res->start;
	gpu_reserved_mem_size = res->end - res->start + 1;
    }

    if (gpu_3d_irq > 0)
    {
	if (request_irq(gpu_3d_irq, z430_irq_handler, 0, "ydx", NULL) < 0) {
	    printk(KERN_ERR "%s: request_irq error\n", __func__);
	    gpu_3d_irq = 0;
	    goto request_irq_error;
	}
    }

    if (gpu_2d_irq > 0)
    {
	if (request_irq(gpu_2d_irq, z160_irq_handler, 0, "g12", NULL) < 0) {
	    printk(KERN_ERR "Could not allocate IRQ for OpenVG!\n");
	    gpu_2d_irq = 0;
	}
    }

    if (kgsl_driver_init() != GSL_SUCCESS) {
	printk(KERN_ERR "%s: kgsl_driver_init error\n", __func__);
	goto kgsl_driver_init_error;
    }

    gsl_kmod_major = register_chrdev(0, "gsl_kmod", &kgsl_fops);
    kgsl_vmops.fault = kgsl_fault;

    if (gsl_kmod_major <= 0)
    {
        pr_err("%s: register_chrdev error\n", __func__);
        goto register_chrdev_error;
    }

    gsl_kmod_class = class_create(THIS_MODULE, "gsl_kmod");

    if (IS_ERR(gsl_kmod_class))
    {
        pr_err("%s: class_create error\n", __func__);
        goto class_create_error;
    }

    #if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28))
        dev = device_create(gsl_kmod_class, NULL, MKDEV(gsl_kmod_major, 0), "gsl_kmod");
    #else
        dev = device_create(gsl_kmod_class, NULL, MKDEV(gsl_kmod_major, 0), NULL, "gsl_kmod");
    #endif

    if (!IS_ERR(dev))
    {
        gsl_driver.pdev = pdev;
        return 0;
    }

    pr_err("%s: device_create error\n", __func__);

class_create_error:
    class_destroy(gsl_kmod_class);

register_chrdev_error:
    unregister_chrdev(gsl_kmod_major, "gsl_kmod");

kgsl_driver_init_error:
    kgsl_driver_close();
    if (gpu_2d_irq > 0) {
	free_irq(gpu_2d_irq, NULL);
    }
    if (gpu_3d_irq > 0) {
	free_irq(gpu_3d_irq, NULL);
    }
request_irq_error:
    return -ENXIO;

nodev:
    gpu_2d_regbase = 0;
    gpu_2d_regsize = 0;
    gpu_3d_regbase = 0;
    gpu_2d_regsize = 0;
    gmem_size = 0;
    gpu_reserved_mem = 0;
    gpu_reserved_mem_size = 0;
    return -ENODEV;
}

static int gpu_remove(struct platform_device *pdev)
{
    device_destroy(gsl_kmod_class, MKDEV(gsl_kmod_major, 0));
    class_destroy(gsl_kmod_class);
    unregister_chrdev(gsl_kmod_major, "gsl_kmod");

    if (gpu_3d_irq)
    {
        free_irq(gpu_3d_irq, NULL);
    }

    if (gpu_2d_irq)
    {
        free_irq(gpu_2d_irq, NULL);
    }

    kgsl_driver_close();
    return 0;
}

#ifdef CONFIG_PM
static int gpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;
	struct kgsl_powerprop power;

	/* this is hideous! */
	power.flags = GSL_PWRFLAGS_POWER_OFF;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		kgsl_device_setproperty(
			(unsigned int) (i+1),
			KGSL_PROP_DEVICE_POWER,
			&power,
			sizeof(struct kgsl_powerprop));
	}

	return 0;
}

static int gpu_resume(struct platform_device *pdev)
{
	int i;
	struct kgsl_powerprop power;

	power.flags = GSL_PWRFLAGS_POWER_ON;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		kgsl_device_setproperty(
			(unsigned int) (i+1),
			KGSL_PROP_DEVICE_POWER,
			&power,
			sizeof(struct kgsl_powerprop));
	}
	return 0;
}
#else
#define	gpu_suspend	NULL
#define	gpu_resume	NULL
#endif /* !CONFIG_PM */

static struct platform_driver kgsl_driver = {
	.driver = {
		.name = "mxc_kgsl", // DRIVER_NAME please
	},
	.probe = gpu_probe,
	.remove = gpu_remove,
	.suspend = gpu_suspend,
	.resume = gpu_resume,
};

static int __init kgsl_mod_init(void)
{
	return platform_driver_register(&kgsl_driver);
}

static void __exit kgsl_mod_exit(void)
{
	platform_driver_unregister(&kgsl_driver);
}

#ifdef MODULE
module_init(kgsl_mod_init);
#else
late_initcall(kgsl_mod_init);
#endif
module_exit(kgsl_mod_exit);
MODULE_AUTHOR("Advanced Micro Devices");
MODULE_DESCRIPTION("AMD graphics core driver for i.MX");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl");
