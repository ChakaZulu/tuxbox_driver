/*
 *   cam.c - CAM driver (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <dbox/fp.h>

static int debug;
static int mio;
static char *firmware;

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

#define I2C_DRIVERID_CAM	0x6E
#define CAM_INTERRUPT		SIU_IRQ3
#define CAM_CODE_SIZE		0x20000
#define CAM_QUEUE_SIZE		0x800

static int attach_adapter(struct i2c_adapter *adap);
static int detach_client(struct i2c_client *client);

static struct i2c_client *dclient;

static struct i2c_driver cam_driver = {
	"DBox2-CAM",
	I2C_DRIVERID_CAM,
	I2C_DF_NOTIFY,
	attach_adapter,
	detach_client,
	0,
	0,
	0,
};

static struct i2c_client client_template = {
	"DBOX2-CAM",
	I2C_DRIVERID_CAM,
	0,
	(0x6E>> 1),
	NULL,
	&cam_driver,
	NULL
};

static DECLARE_MUTEX_LOCKED(cam_busy);
static void cam_task(void *);
static void cam_interrupt(int irq, void *dev, struct pt_regs * regs);

int cam_reset(void);
int cam_write_message( char * buf, size_t count );
int cam_read_message( char * buf, size_t count );

static void *code_base;

unsigned char cam_queue[CAM_QUEUE_SIZE];
static int cam_queuewptr, cam_queuerptr;
static wait_queue_head_t queuewait;

static int attach_adapter(struct i2c_adapter *adap)
{
	struct i2c_client *client;
	client_template.adapter=adap;

	if (!(client=kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
	  return -ENOMEM;

	memcpy(client, &client_template, sizeof(struct i2c_client));
	dclient=client;

	client->data=0;

	printk("CAM: attaching CAM at 0x%02x\n", (client->addr)<<1);
	i2c_attach_client(client);

	printk("CAM: attached to adapter %s\n", adap->name);
	return 0;
}

static int detach_client(struct i2c_client *client)
{
	printk("CAM: detach_client\n");
	i2c_detach_client(client);
	kfree(client);
	return 0;
}

struct tq_struct cam_tasklet=
{
	routine: cam_task,
	data: 0
};

static void cam_task(void *data)
{
	unsigned char buffer[130], caid[9]={0x50,0x06,0x23,0x84,0x01,0x02,0xFF,0xFF,0};
	int len, i;

	if (down_interruptible(&cam_busy))
	{
	  enable_irq(CAM_INTERRUPT);
	  return;
	}

	if (i2c_master_recv(dclient, buffer, 2)!=2)
	{
	  printk("i2c-CAM read error.\n");
	  up(&cam_busy);
	  enable_irq(CAM_INTERRUPT);
	  return;
	}

	len=buffer[1]&0x7F;

	if (i2c_master_recv(dclient, buffer, len+3)!=len+3)
	{
	  printk("i2c-CAM read error.\n");
	  up(&cam_busy);
	  enable_irq(CAM_INTERRUPT);
	  return;
	}

	if ((buffer[1]&0x7F) != len)
	{
	  len=buffer[1]&0x7F;
	  printk("CAM: unsure length, reading again.\n");
	  if (i2c_master_recv(dclient, buffer, len+3)!=len+3)
	  {
	    printk("i2c-CAM read error.\n");
	    up(&cam_busy);
	    enable_irq(CAM_INTERRUPT);
	    return;
	  }
	}

	dprintk("CAM says:");
	for (i=0; i<len+3; i++)
	  dprintk(" %02x", buffer[i]);
	dprintk("\n");

	len+=3;
	if ( buffer[2] == 0x23 && buffer[3]<=7 )
	{
		int csum=0x6E;
		caid[6]=buffer[5];
		caid[7]=buffer[6];
		for (i=0; i<8; i++)
			csum^=caid[i];
		caid[8]=csum;
		dprintk ("set CAID: %02x%02x \n", caid[6],caid[7]);
		up(&cam_busy);
		cam_write_message(caid,9);
	} else
	{
	  i=cam_queuewptr-cam_queuerptr;
	  if (i<0)
	    i+=CAM_QUEUE_SIZE;

	  i=CAM_QUEUE_SIZE-i;

	  if (i<len)
	    cam_queuewptr=cam_queuerptr;
		else
	  {
	    i=0;

	    cam_queue[cam_queuewptr++]=0x6F;
	    if (cam_queuewptr==CAM_QUEUE_SIZE)
	      cam_queuewptr=0;

		  while (len--)
	    {
	      cam_queue[cam_queuewptr++]=buffer[i++];
	      if (cam_queuewptr==CAM_QUEUE_SIZE)
		cam_queuewptr=0;
	    }
	  }
	}
	wake_up(&queuewait);

	up(&cam_busy);
	enable_irq(CAM_INTERRUPT);
}

static void cam_interrupt(int irq, void *dev, struct pt_regs * regs)
{
	schedule_task(&cam_tasklet);
}

int cam_reset()
{
	return dbox2_fp_reset_cam();
}

int cam_write_message( char * buf, size_t count )
{
	int res;

	if ((res = down_interruptible(&cam_busy)))
		return res;

	res = i2c_master_send(dclient, buf, count);

	cam_queuewptr=cam_queuerptr;  // mich stoerte der Buffer ....		// ?? was soll das? (tmb)
	up(&cam_busy);
	return res;
}

int cam_read_message( char * buf, size_t count )
{
	int cb;

	cb=cam_queuewptr-cam_queuerptr;

	if (cb<0)
		cb+=CAM_QUEUE_SIZE;

	if (count<cb)
		cb=count;

	if ((cam_queuerptr+cb)>CAM_QUEUE_SIZE)
	{
		memcpy(buf, cam_queue+cam_queuerptr, CAM_QUEUE_SIZE-cam_queuerptr);
		memcpy(buf+(CAM_QUEUE_SIZE-cam_queuerptr), cam_queue, cb-(CAM_QUEUE_SIZE-cam_queuerptr));
		cam_queuerptr=cb-(CAM_QUEUE_SIZE-cam_queuerptr);
	}
	else
	{
		memcpy(buf, cam_queue+cam_queuerptr, cb);
		cam_queuerptr+=cb;
	}

	return cb;
}

unsigned int cam_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &queuewait, wait);

	if (cam_queuerptr != cam_queuewptr)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static void do_firmwrite( u32 *buffer )
{
	int size,i;
	void *base;
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR ;
	volatile cpm8xx_t *cp = &immap->im_cpm;

	base = (void *)buffer;
	size = CAM_CODE_SIZE;

	cp->cp_pbpar&=~15;  // GPIO (not cpm-io)
	cp->cp_pbodr&=~15;  // driven output (not tristate)
	cp->cp_pbdir|=15;   // output (not input)

	cp->cp_pbdat|=0xF;
	cp->cp_pbdat&=~2;
	cp->cp_pbdat|=2;

	for (i=0; i<8; i++)
	{
		cp->cp_pbdat&=~8;
		udelay(100);
		cp->cp_pbdat|=8;
		udelay(100);
	}

	cam_reset();

	cp->cp_pbdat&=~1;

	memcpy(code_base, base, size);

	cp->cp_pbdat|=1;
	cp->cp_pbdat&=~2;
	cp->cp_pbdat|=2;

	udelay(100);

	cam_reset();
}

static int errno;

static int do_firmread(const char *fn, char **fp)
{
	/* shameless stolen from sound_firmware.c */

	int fd;
	  long l;
	  char *dp;

	fd = open(fn,0,0);

	if (fd == -1)
	{
		dprintk(KERN_ERR "cam.o: Unable to load '%s'.\n", firmware);
		return 0;
	}

	l = lseek(fd, 0L, 2);

	if (l<=0)
	{
		dprintk(KERN_ERR "cam.o: Firmware wrong size '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	lseek(fd, 0L, 0);

	dp = vmalloc(l);

	if (dp == NULL)
	{
		dprintk(KERN_ERR "cam.o: Out of memory loading '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	if (read(fd, dp, l) != l)
	{
		dprintk(KERN_ERR "cam.o: Failed to read '%s'.%d\n", firmware,errno);
		vfree(dp);
		sys_close(fd);
		return 0;
	}

	close(fd);

	*fp = dp;

	return (int) l;
}

int __init cam_init(void)
{
	int res;
	mm_segment_t fs;
	u32 *microcode;

	printk("$Id: cam.c,v 1.27 2003/09/30 05:45:34 obi Exp $\n");
	
	if (!mio) {
	
		printk("cam: mio address unknown\n");
		
		return -EINVAL;
		
	}
	
	if (!(code_base = ioremap(mio, CAM_CODE_SIZE))) {
	
		printk("cam: could not map mio=0x%08X\n", mio);
		
		return -EFAULT;
		
	}

	init_waitqueue_head(&queuewait);

	/* load microcode */
	fs = get_fs();

	set_fs(get_ds());

	/* read firmware */
	if (do_firmread(firmware, (char**)&microcode) == 0)
	{
		set_fs(fs);
		return -EIO;
	}

	set_fs(fs);

	do_firmwrite(microcode);

	vfree(microcode);

	 if ((res = i2c_add_driver(&cam_driver)))
	{
		printk("CAM: Driver registration failed, module not inserted.\n");
		return res;
	}

	 if ( ! dclient )
	{
	  i2c_del_driver ( &cam_driver );
	  printk ( "CAM: cam not found.\n" );
	  return -EBUSY;
	}

	up(&cam_busy);

	if (request_irq(CAM_INTERRUPT, cam_interrupt, SA_ONESHOT, "cam", THIS_MODULE) != 0)
		panic("Could not allocate CAM IRQ!");

	return 0;
}

void __exit cam_cleanup(void)
{
	int res;
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR ;
	volatile cpm8xx_t *cp = &immap->im_cpm;

	free_irq(CAM_INTERRUPT, THIS_MODULE);
	schedule(); // HACK: let all task queues run.

	if ((res=down_interruptible(&cam_busy)))
		return;

	iounmap(code_base);

	if ((res = i2c_del_driver(&cam_driver)))
	{
		printk("cam: Driver deregistration failed, module not removed.\n");
		return;
	}

	cp->cp_pbdat|=0xF;
	cp->cp_pbdat&=~2;
	udelay(100);
	cp->cp_pbdat|=2;
	
}

EXPORT_SYMBOL(cam_poll);
EXPORT_SYMBOL(cam_read_message);
EXPORT_SYMBOL(cam_reset);
EXPORT_SYMBOL(cam_write_message);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 CAM Driver");
MODULE_PARM(debug,"i");
MODULE_PARM(mio,"i");
MODULE_PARM(firmware,"s");
MODULE_LICENSE("GPL");

module_init(cam_init);
module_exit(cam_cleanup);
