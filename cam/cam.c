/*
 *   cam.c - CAM driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
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
 *
 *   $Log: cam.c,v $
 *   Revision 1.3  2001/03/03 13:03:04  gillem
 *   - add option firmware,debug
 *
 *
 *   $Revision: 1.3 $
 *
 */

/* ---------------------------------------------------------------------- */

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
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <dbox/fp.h>

/* ---------------------------------------------------------------------- */

#ifdef MODULE
static int debug=0;
static char *firmware=0;
#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

#define CAM_MAJOR            61
#define CAM_CODE_MINOR       0
#define CAM_DATA_MINOR       1
#define CAM_MINOR            2
#define I2C_DRIVERID_CAM     0x6E

#define CAM_INTERRUPT        SIU_IRQ3
#define CAM_CODE_BASE        0xC000000
#define CAM_CODE_SIZE        0x20000
                              // BUG BUG ... oder? .so meint: 0xE00 0000, aber das geht nicht.
#define CAM_DATA_BASE        0xC020000
#define CAM_DATA_SIZE        0x10000

#define CAM_QUEUE_SIZE       0x1000

/* ---------------------------------------------------------------------- */

static DECLARE_MUTEX_LOCKED(cam_busy);

static void cam_task(void *);

static struct i2c_client *dclient;

static int attach_adapter(struct i2c_adapter *adap);
static int detach_client(struct i2c_client *client);

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


static void *code_base, *data_base;

static int cam_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int cam_open (struct inode *inode, struct file *file);
static ssize_t cam_write (struct file *file, const char *buf, size_t count, loff_t *offset);
static ssize_t cam_read (struct file *file, char *buf, size_t count, loff_t *offset);
static int cam_release(struct inode *inode, struct file *file);

static void cam_interrupt(int irq, void *dev, struct pt_regs * regs);

        /*
          queue data:
            6f len 23 dd dd ss
            
            6f xx 23  is sync 
        */
        
unsigned char cam_queue[CAM_QUEUE_SIZE];

static int cam_queuewptr=0, cam_queuerptr=0;
static wait_queue_head_t queuewait;

static struct file_operations cam_fops = {
        owner:          THIS_MODULE,
        read:           cam_read,
        write:          cam_write,
        ioctl:          cam_ioctl,
        open:           cam_open,
        release:        cam_release
};

/* ---------------------------------------------------------------------- */

static int cam_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                      unsigned long arg)
{
  return -ENOSYS;
}

static ssize_t cam_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
  
  if (/*(minor == CAM_CODE_MINOR) || */(minor == CAM_DATA_MINOR))
  {
    int numb, size;
    void *base;
  
    switch (minor)
    {
//    case CAM_CODE_MINOR:
//      base=cam_code_buffer;               // cam_code_base
//      size=CAM_CODE_SIZE;
//      break;
    case CAM_DATA_MINOR:
      base=data_base;
      size=CAM_DATA_SIZE;
      break;
    default:
      return -ENOSYS;
    }
  
    numb=size-file->f_pos;
    if (numb<=0)
      return 0;
    if (numb>count)
      numb=count;
  
    if (copy_from_user(((u8*)base)+file->f_pos, buf, numb))
      return -EFAULT; 
    
    *offset+=numb;
  
/*    if ((*offset==size) && (minor==CAM_CODE_MINOR))
    {
      immap_t *immap=(immap_t *)IMAP_ADDR ;
      volatile cpm8xx_t *cp=&immap->im_cpm;
      int i;
      printk("der moment ist gekommen...\n");
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

      fp_do_reset(0xAF);
      cp->cp_pbdat&=~1;
//      memcpy(code_base, cam_code_buffer, CAM_CODE_SIZE);
      memset(data_base, 'Z', CAM_DATA_SIZE);
      cp->cp_pbdat|=1;
      cp->cp_pbdat&=~2;
      cp->cp_pbdat|=2;
      udelay(100);
      fp_do_reset(0xAF);
    } */
    return numb;
  } else
  if (minor==CAM_MINOR)
  {
    int res;
    if ((res=down_interruptible(&cam_busy)))
      return res;
                // check userspace pointer? use buffer?
    res=i2c_master_send(dclient, buf, count);
    up(&cam_busy);
    return res;
  } else
    return -ENODEV;
}                             

static ssize_t cam_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
  unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
  int numb, size;
  void *base;
  
  if ((minor == CAM_CODE_MINOR) || (minor == CAM_DATA_MINOR))
  {
    switch (minor)
    {
    case CAM_CODE_MINOR:
      base=code_base;
      size=CAM_CODE_SIZE;
      break;
    case CAM_DATA_MINOR:
      base=data_base;
      size=CAM_DATA_SIZE;
      break;
    default:
      return -ENOSYS;
    }
  
    numb=size-file->f_pos;
    if (numb<=0)
      return 0;
    if (numb>count)
      numb=count;
  
    if (copy_to_user(buf, ((u8*)base)+file->f_pos, numb))
      return -EFAULT; 

    *offset+=numb;
  
    return numb;
  } else if (minor==CAM_MINOR)
  {
    int cb;
    DECLARE_WAITQUEUE(wait, current);
    
again:
    if (cam_queuewptr==cam_queuerptr)
    {
      if (file->f_flags & O_NONBLOCK)
        return -EWOULDBLOCK;
            add_wait_queue(&queuewait, &wait);
      set_current_state(TASK_INTERRUPTIBLE);
      schedule();
      current->state = TASK_RUNNING;
      remove_wait_queue(&queuewait, &wait);
      if (signal_pending(current))
        return -ERESTARTSYS;
      goto again;
    } 
    
    cb=cam_queuewptr-cam_queuerptr;
    if (cb<0)
      cb+=CAM_QUEUE_SIZE;

    if (count<cb)
      cb=count;
    
    if ((cam_queuerptr+cb)>CAM_QUEUE_SIZE)
    {
      if (copy_to_user(buf, cam_queue+cam_queuerptr, CAM_QUEUE_SIZE-cam_queuerptr))
        return -EFAULT;

      if (copy_to_user(buf, cam_queue, cb-(CAM_QUEUE_SIZE-cam_queuerptr)))
        return -EFAULT;
      cam_queuerptr=cb-(CAM_QUEUE_SIZE-cam_queuerptr);
      return cb;
    } else
    {
      if (copy_to_user(buf, cam_queue+cam_queuerptr, cb))
        return -EFAULT;
      cam_queuerptr+=cb;
      return cb;            
    }
  } else
    return -ENODEV;
}

static int cam_open (struct inode *inode, struct file *file)
{
  printk("cam.o: open\n");
  return 0;
}

static int cam_release(struct inode *inode, struct file *file)
{
  printk("cam.o: release\n");
  return 0; 
}


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
  unsigned char buffer[130];
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
  
  printk("CAM says:");
  for (i=0; i<len+3; i++)
    printk(" %02x", buffer[i]);
  printk("\n");
  
  len+=3;
  
  i=cam_queuewptr-cam_queuerptr;
  if (i<0)
    i+=CAM_QUEUE_SIZE;
  
  i=CAM_QUEUE_SIZE-i;
  
  if (i<len)
    printk("CAM buffer overflow (need %d, have %d)\n", len, i);
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
  
  wake_up(&queuewait);
  
  up(&cam_busy);
  enable_irq(CAM_INTERRUPT);
}

static void cam_interrupt(int irq, void *dev, struct pt_regs * regs)
{
  schedule_task(&cam_tasklet);
}

/* ---------------------------------------------------------------------- */

static void do_firmwrite( u32 *buffer )
{
    int size,i;
    void *base;
    immap_t *immap=(immap_t *)IMAP_ADDR ;
    volatile cpm8xx_t *cp=&immap->im_cpm;

	base = (void*)buffer;
    size=CAM_CODE_SIZE;

	printk("der moment ist gekommen...\n");

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

	fp_do_reset(0xAF);
	cp->cp_pbdat&=~1;

	memcpy(code_base, base, size);
	memset(data_base, 'Z', CAM_DATA_SIZE);

	cp->cp_pbdat|=1;
	cp->cp_pbdat&=~2;
	cp->cp_pbdat|=2;
	udelay(100);

	fp_do_reset(0xAF);
}

/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */

int cam_init(void)
{
	int res;
   	mm_segment_t fs;
	u32 *microcode;

	init_waitqueue_head(&queuewait);

	code_base=ioremap(CAM_CODE_BASE, CAM_CODE_SIZE);
	data_base=ioremap(CAM_DATA_BASE, CAM_DATA_BASE);

	if (!code_base || !data_base)
	{
		panic("couldn't map CAM-io.\n");
	}

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

	if (register_chrdev(CAM_MAJOR, "cam", &cam_fops))
	{
		printk("cam.o: unable to get major %d\n", CAM_MAJOR);
		return -EIO;
	}

	if ((res = i2c_add_driver(&cam_driver)))
	{
		unregister_chrdev(CAM_MAJOR, "cam");
		printk("CAM: Driver registration failed, module not inserted.\n");
		return res;
	}

	if (!dclient)
	{
		unregister_chrdev(CAM_MAJOR, "cam");
		i2c_del_driver(&cam_driver);
		printk("CAM: cam not found.\n");
		return -EBUSY;
	}

	up(&cam_busy);

	if (request_8xxirq(CAM_INTERRUPT, cam_interrupt, SA_ONESHOT, "cam", THIS_MODULE) != 0)
	{
		panic("Could not allocate CAM IRQ!");
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

void cam_fini(void)
{
	int res;

	if ((res=unregister_chrdev(CAM_MAJOR, "cam")))
	{
		printk("cam.o: unable to release major %d\n", CAM_MAJOR);
	}

	free_irq(CAM_INTERRUPT, THIS_MODULE);
	schedule(); // HACK: let all task queues run.

	if ((res=down_interruptible(&cam_busy)))
	{
		return;
	}

	iounmap(code_base);
	iounmap(data_base);

	if ((res = i2c_del_driver(&cam_driver)))
	{
		printk("cam: Driver deregistration failed, module not removed.\n");
		return;
	}
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 CAM Driver");
MODULE_PARM(debug,"i");
MODULE_PARM(firmware,"s");

int init_module(void)
{
	return cam_init();
}

/* ---------------------------------------------------------------------- */

void cleanup_module(void)
{
	cam_fini();
}
#endif

/* ---------------------------------------------------------------------- */
