#ifndef __CRAP_H
#define __CRAP_H

#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/version.h>
#include <linux/fs.h>


/**
 *  a sleeping delay function, waits i ms
 *
 */
static
inline void ddelay(int i)
{
	current->state=TASK_INTERRUPTIBLE;
	schedule_timeout((HZ*i)/1000);
}


static inline
void kernel_thread_setup (const char *thread_name)
{
        lock_kernel ();

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,61))
        daemonize ();
	strncpy (current->comm, thread_name, sizeof(current->comm));
#else
        daemonize (thread_name);
#endif

/*      not needed anymore in 2.5.x, done in daemonize() */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
        reparent_to_init ();
#endif

        sigfillset (&current->blocked);
        unlock_kernel ();
}



/**
 *  compatibility crap for old kernels. No guarantee for a working driver
 *  even when everything compiles.
 */

/* we don't mess with video_usercopy() any more,
we simply define out own dvb_usercopy(), which will hopefull become
generic_usercopy()  someday... */

extern int dvb_usercopy(struct inode *inode, struct file *file,
	                    unsigned int cmd, unsigned long arg,
			    int (*func)(struct inode *inode, struct file *file,
			    unsigned int cmd, void *arg));

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


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,66))
#define devfs_mk_dir(parent,name,info) devfs_mk_dir(name)
#endif


#endif

