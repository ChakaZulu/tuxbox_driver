#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

/*
 * helper function -- handles userspace copying for ioctl arguments
 */
int
video_usercopy(struct inode *inode, struct file *file,
		 unsigned int cmd, unsigned long arg,
		 int (*func)(struct inode *inode, struct file *file,
			     unsigned int cmd, void *arg))
{
	char	sbuf[128];
	void    *mbuf = NULL;
	void	*parg = NULL;
	int	err  = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		parg = (void *)arg;
		break;
	case _IOC_READ: /* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd),GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}
		
		err = -EFAULT;
		if (copy_from_user(parg, (void *)arg, _IOC_SIZE(cmd)))
			goto out;
		break;
	}

	/* call driver */
	if ((err = func(inode, file, cmd, parg)) == -ENOIOCTLCMD)
		err = -EINVAL;

	if (err < 0)
		goto out;

	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd))
	{
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	if (mbuf)
		kfree(mbuf);

	return err;
}
