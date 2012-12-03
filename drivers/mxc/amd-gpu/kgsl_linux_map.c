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

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "kgsl_linux_map.h"

struct kgsl_mem_entry
{
	struct list_head list;
	unsigned int gpu_addr;
	void *kernel_virtual_addr;
	unsigned int size;
};

static LIST_HEAD(kgsl_mem_entry_list);
static DEFINE_MUTEX(kgsl_mem_entry_mutex);

int kgsl_mem_entry_init()
{
	mutex_lock(&kgsl_mem_entry_mutex);
	INIT_LIST_HEAD(&kgsl_mem_entry_list);
	mutex_unlock(&kgsl_mem_entry_mutex);

	return 0;
}

void *kgsl_mem_entry_alloc(unsigned int gpu_addr, unsigned int size)
{
	struct kgsl_mem_entry * map;
	struct list_head *p;
	void *va;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr == gpu_addr){
			mutex_unlock(&kgsl_mem_entry_mutex);
			return map->kernel_virtual_addr;
		}
	}

	va = __vmalloc(size, GFP_KERNEL, pgprot_noncached(pgprot_kernel));
	if(va == NULL){
		mutex_unlock(&kgsl_mem_entry_mutex);
		return NULL;
	}

	map = (struct kgsl_mem_entry *)kmalloc(sizeof(*map), GFP_KERNEL);
	map->gpu_addr = gpu_addr;
	map->kernel_virtual_addr = va;
	map->size = size;

	INIT_LIST_HEAD(&map->list);
	list_add_tail(&map->list, &kgsl_mem_entry_list);

	mutex_unlock(&kgsl_mem_entry_mutex);
	return va;
}

void kgsl_mem_entry_free(unsigned int gpu_addr)
{
	int found = 0;
	struct kgsl_mem_entry * map;
	struct list_head *p;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr == gpu_addr){
			found = 1;
			break;
		}
	}

	if(found){
		vfree(map->kernel_virtual_addr);
		list_del(&map->list);
		kfree(map);
	}

	mutex_unlock(&kgsl_mem_entry_mutex);
}

void *kgsl_sharedmem_find(unsigned int gpu_addr)
{
	struct kgsl_mem_entry * map;
	struct list_head *p;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr == gpu_addr){
			mutex_unlock(&kgsl_mem_entry_mutex);
			return map->kernel_virtual_addr;
		}
	}

	mutex_unlock(&kgsl_mem_entry_mutex);
	return NULL;
}

void *kgsl_mem_entry_read(void *dst, unsigned int gpuoffset, unsigned int sizebytes, unsigned int touserspace)
{
	struct kgsl_mem_entry * map;
	struct list_head *p;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr <= gpuoffset &&
			(map->gpu_addr +  map->size) > gpuoffset){
			void *src = map->kernel_virtual_addr + (gpuoffset - map->gpu_addr);
			mutex_unlock(&kgsl_mem_entry_mutex);
                        if (touserspace)
                        {
                            return (void *)copy_to_user(dst, map->kernel_virtual_addr + gpuoffset - map->gpu_addr, sizebytes);
                        }
                        else
                        {
	                    return memcpy(dst, src, sizebytes);
                        }
		}
	}

	mutex_unlock(&kgsl_mem_entry_mutex);
	return NULL;
}

void *kgsl_mem_entry_write(void *src, unsigned int gpuoffset, unsigned int sizebytes, unsigned int fromuserspace)
{
	struct kgsl_mem_entry * map;
	struct list_head *p;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr <= gpuoffset &&
			(map->gpu_addr +  map->size) > gpuoffset){
			void *dst = map->kernel_virtual_addr + (gpuoffset - map->gpu_addr);
			mutex_unlock(&kgsl_mem_entry_mutex);
                        if (fromuserspace)
                        {
                            return (void *)copy_from_user(map->kernel_virtual_addr + gpuoffset - map->gpu_addr, src, sizebytes);
                        }
                        else
                        {
                            return memcpy(dst, src, sizebytes);
                        }
		}
	}

	mutex_unlock(&kgsl_mem_entry_mutex);
	return NULL;
}

void *kgsl_mem_entry_set(unsigned int gpuoffset, unsigned int value, unsigned int sizebytes)
{
	struct kgsl_mem_entry * map;
	struct list_head *p;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each(p, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		if(map->gpu_addr <= gpuoffset &&
			(map->gpu_addr +  map->size) > gpuoffset){
			void *ptr = map->kernel_virtual_addr + (gpuoffset - map->gpu_addr);
			mutex_unlock(&kgsl_mem_entry_mutex);
			return memset(ptr, value, sizebytes);
		}
	}

	mutex_unlock(&kgsl_mem_entry_mutex);
	return NULL;
}

int kgsl_mem_entry_destroy()
{
	struct kgsl_mem_entry * map;
	struct list_head *p, *tmp;

	mutex_lock(&kgsl_mem_entry_mutex);

	list_for_each_safe(p, tmp, &kgsl_mem_entry_list){
		map = list_entry(p, struct kgsl_mem_entry, list);
		vfree(map->kernel_virtual_addr);
		list_del(&map->list);
		kfree(map);
	}

	INIT_LIST_HEAD(&kgsl_mem_entry_list);

	mutex_unlock(&kgsl_mem_entry_mutex);
	return 0;
}
