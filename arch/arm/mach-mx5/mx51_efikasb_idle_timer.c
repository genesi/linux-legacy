/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 */


#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/keyboard.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <asm/io.h>
#include "mx51_efikasb.h"
#include "iomux.h"

#define DEFAULT_PERIOD    60  
#define IDLE_EVENT        1

void mxc_reset_idle_timer(void);

struct efikasb_idle_timer {
        int enable;
        int timeout;
        spinlock_t lock;
        struct timer_list timer;
        unsigned long period;   /* in second */
};

static struct efikasb_idle_timer *idle_timer = NULL;

static BLOCKING_NOTIFIER_HEAD(idle_notifier_list);

int register_idle_notifier(struct notifier_block *nb)
{
        return blocking_notifier_chain_register(&idle_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_idle_notifier);

int unregister_idle_notifier(struct notifier_block *nb)
{
        return blocking_notifier_chain_unregister(&idle_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_idle_notifier);

static int key_event_call(struct notifier_block *blk, unsigned long code, void *_param)
{

        switch (code) {
        case KBD_KEYCODE:
                mxc_reset_idle_timer();
                return NOTIFY_OK;
        }
        
        return NOTIFY_DONE;
}

static struct notifier_block key_nb = {
        .notifier_call = key_event_call,
};

static void idle_timeout_fn(unsigned long data)
{
        struct efikasb_idle_timer *efikasb_timer = (struct efikasb_idle_timer *)data;
        unsigned long flags;

        spin_lock_irqsave(&efikasb_timer->lock, flags);

        efikasb_timer->timeout = 1;
        efikasb_timer->enable = 0;

        spin_unlock_irqrestore(&efikasb_timer->lock, flags);

        blocking_notifier_call_chain(&idle_notifier_list, IDLE_EVENT, 1);

}

static void idle_timer_enable(void)
{
        unsigned long flags;

        if(idle_timer->enable && timer_pending(&idle_timer->timer))
                return;
   
        spin_lock_irqsave(&idle_timer->lock, flags);

        idle_timer->timer.data = (unsigned long)idle_timer;
        idle_timer->timer.function = idle_timeout_fn;
        if(idle_timer->period == 0)
                idle_timer->period = DEFAULT_PERIOD;

        idle_timer->timer.expires = jiffies + idle_timer->period * HZ;

        add_timer(&idle_timer->timer);
        idle_timer->enable = 1;
        idle_timer->timeout = 0;

        spin_unlock_irqrestore(&idle_timer->lock, flags);
}

static void idle_timer_disable(void)
{
        unsigned long flags;

        if(!idle_timer->enable && !timer_pending(&idle_timer->timer))
                return;

        spin_lock_irqsave(&idle_timer->lock, flags);

        del_timer(&idle_timer->timer);
        idle_timer->enable = 0;
        idle_timer->timeout = 0;

        spin_unlock_irqrestore(&idle_timer->lock, flags);
}

static void idle_timer_reset(void)
{
        unsigned long flags;

        if(!idle_timer->enable && !timer_pending(&idle_timer->timer))
                return;

        spin_lock_irqsave(&idle_timer->lock, flags);

        mod_timer(&idle_timer->timer, jiffies + idle_timer->period * HZ);
        idle_timer->timeout = 0;

        spin_unlock_irqrestore(&idle_timer->lock, flags);
}

static ssize_t timer_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", idle_timer->enable);
}

static ssize_t timer_enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
        if(strncmp(buf, "1", 1) == 0)
                idle_timer_enable();
        else if(strncmp(buf, "0", 1) == 0)
                idle_timer_disable();

        return count;

}

static ssize_t timer_period_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", (unsigned int) idle_timer->period);
}

static ssize_t timer_period_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
        char *end;
        int arg;
        
        arg = simple_strtoul(buf, &end, 10);
        if(arg < 0) {
                return -EINVAL;
        }

        idle_timer->period = arg;
        idle_timer_reset();

        return count;

}

static ssize_t timer_timeout_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

        if (!idle_timer->enable)
                return sprintf(buf, "0\n");
        else
                return sprintf(buf, "%d\n", 
                              jiffies_to_msecs(idle_timer->timer.expires - jiffies) / 1000);
}



static struct kobj_attribute timer_enable_attribute = 
        __ATTR(enable, 0666, timer_enable_show, timer_enable_store);
static struct kobj_attribute timer_period_attribute = 
        __ATTR(period, 0666, timer_period_show, timer_period_store);
static struct kobj_attribute timer_timeout_attribute = 
        __ATTR(timeout, S_IFREG | S_IRUGO, timer_timeout_show, NULL);

static struct attribute *idle_attrs[] = {
        &timer_enable_attribute.attr,
        &timer_period_attribute.attr,
        &timer_timeout_attribute.attr,
        NULL,
};

static struct attribute_group idle_attr_group = {
        .attrs = idle_attrs,
};

static struct platform_device mxc_idle_timer_dev = {
        .name = "mxc_idle_timer",
};

static int __init mxc_init_idle_timer(void)
{
        int retval;
        static struct kobject *idle_timer_kobj;

        platform_device_register(&mxc_idle_timer_dev);

        idle_timer_kobj = kobject_create_and_add("setting", &mxc_idle_timer_dev.dev.kobj);
        if(!idle_timer_kobj)
                return -ENOMEM;
        
        retval = sysfs_create_group(idle_timer_kobj, &idle_attr_group);
        if(retval) {
                kobject_put(idle_timer_kobj);
                return retval;
        }
        
        idle_timer = kzalloc(sizeof(*idle_timer), GFP_KERNEL);
        if(!idle_timer) {
                sysfs_remove_group(idle_timer_kobj, &idle_attr_group);
                kobject_put(idle_timer_kobj);
                return -ENOMEM;
        }

        init_timer(&idle_timer->timer);
        idle_timer->lock = SPIN_LOCK_UNLOCKED;

        register_keyboard_notifier(&key_nb);

        return 0;
}
late_initcall(mxc_init_idle_timer);

void mxc_reset_idle_timer(void)
{
        if(idle_timer->timeout) {
                blocking_notifier_call_chain(&idle_notifier_list, IDLE_EVENT, 0);
        } else {
                idle_timer_reset();
        }
}
EXPORT_SYMBOL(mxc_reset_idle_timer);

