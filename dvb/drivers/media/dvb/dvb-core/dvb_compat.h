#ifndef __CRAP_H
#define __CRAP_H

#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>

#define iminor(xx) minor(xx->i_rdev)

#define irqreturn_t void
#define IRQ_NONE
#define IRQ_HANDLED
#define strlcpy strncpy
extern devfs_handle_t dvb_devfs_handle;

int devfs_mk_cdev(dev_t dev, umode_t mode, const char *fmt, ...);

/* necessary dummy functions due to the > 2.5.67 kernel i2c changes */
#define i2c_get_adapdata(adapter) (struct saa7146_dev*)adapter->data;

/**
 *  compatibility crap for old kernels. No guarantee for a working driver
 *  even when everything compiles.
 */

/* FIXME: check what is really necessary in here */
#include <linux/module.h>
#include <linux/list.h>


#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif


#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
                pos = n, n = pos->next)
#endif


#ifndef __devexit_p
#if defined(MODULE)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif
#endif



#ifndef minor
#define minor(dev) MINOR(dev)
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20))
static inline
void cond_resched (void)
{
	if (current->need_resched)
		schedule();
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
extern u32 crc32_le (u32 crc, unsigned char const *p, size_t len);
#else
#include <linux/crc32.h>
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,48))
static inline 
int try_module_get(struct module *mod)
{
	if (!MOD_CAN_QUERY(mod))
		return 0;
	__MOD_INC_USE_COUNT(mod);
	return 1;
}

#define module_put(mod) __MOD_DEC_USE_COUNT(mod)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20)
extern struct page * vmalloc_to_page(void *addr);
#define unlikely(x)    __builtin_expect((x),0)
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0) 
#include <linux/tqueue.h> 
#ifndef work_struct 
#define work_struct tq_struct 
#endif 
#ifndef INIT_WORK 
#define INIT_WORK(wq,routine,data) INIT_TQUEUE(wq,routine,data) 
#endif 
#ifndef schedule_work 
#define schedule_work(wq) schedule_task(wq) 
#endif 
#ifndef flush_scheduled_work 
#define flush_scheduled_work() flush_scheduled_tasks() 
#endif 
#else 
#include <linux/workqueue.h> 
#endif 

#endif

