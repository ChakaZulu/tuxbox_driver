/*
 *   fp.c - FP driver (dbox-II-project)
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
 *   $Log: fp.c,v $
 *   Revision 1.12  2001/03/04 18:48:07  gillem
 *   - fix for sagem box
 *
 *   Revision 1.11  2001/03/03 18:20:39  waldi
 *   complete move to devfs; doesn't compile without devfs
 *
 *   Revision 1.10  2001/03/03 13:03:20  gillem
 *   - fix code
 *
 *   Revision 1.9  2001/02/25 21:11:36  gillem
 *   - fix fpid
 *
 *   Revision 1.8  2001/02/23 18:44:43  gillem
 *   - add ioctl
 *   - add debug option
 *   - some changes ...
 *
 *
 *   $Revision: 1.12 $
 *
 */

/* ---------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/tqueue.h>
#include <linux/i2c.h>
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/8xx_immap.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/signal.h>

#include "dbox/fp.h"

#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle[2];

/* ---------------------------------------------------------------------- */

#ifdef MODULE
static int debug=0;
#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

/* ---------------------------------------------------------------------- */

/*
      exported functions:
      
      int fp_set_tuner_dword(int type, u32 tw);
        T_QAM
        T_QPSK
      int fp_set_polarisation(int pol);
        P_HOR
        P_VERT

*/

/*
   tut mir leid wenn der treiber an einigen stellen nicht so schön
   ist wie er sein könnte, insbesondere im bezug auf mehrere
   instanzen etc. (ja, ich weiss dass das argument "aber mehr als
   einen FP gibts doch eh nicht?!" nicht gilt.)

   auch wenn zweimal auf /dev/rc zugegriffen wird - SORRY.   
*/

/*
fp:
 03  deep standby
 10 led on
 11 led off
 dez.
 20 reboot
 21 reboot
 42 lcd off / led off ( alloff ;-) )
 ADDR VAL
 18   X0  X = dimm 0=off F=on
 22 off
*/

#define FP_INTERRUPT        SIU_IRQ2
#define I2C_FP_DRIVERID     0xF060
#define RCBUFFERSIZE        16
#define FP_GETID            0x1D

/* ---------------------------------------------------------------------- */

/* Scan 0x60 */
static unsigned short normal_i2c[] = { 0x60>>1,I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0x60>>1, 0x60>>1, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

static int fp_id=0;

struct fp_data
{
  int fpID;
  struct i2c_client *client;
};

struct fp_data *defdata=0;

//static spinlock_t rc_lock=SPIN_LOCK_UNLOCKED;
static u16 rcbuffer[RCBUFFERSIZE];
static u16 rcbeg, rcend, rc_opened;
static wait_queue_head_t rcwait;

static void fp_task(void *);

struct tq_struct fp_tasklet=
{
  routine: fp_task,
  data: 0
};

static int fp_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int fp_open (struct inode *inode, struct file *file);
static ssize_t fp_write (struct file *file, const char *buf, size_t count, loff_t *offset);
static ssize_t fp_read (struct file *file, char *buf, size_t count, loff_t *offset);

static int fp_detach_client(struct i2c_client *client);
static int fp_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags, int kind);
static int fp_attach_adapter(struct i2c_adapter *adapter);
static int fp_getid(struct i2c_client *client);
static void fp_interrupt(int irq, void *dev, struct pt_regs * regs);
static int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, int size);
static int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1);
static void fp_check_queues(void);

static void fp_restart(char *cmd);
static void fp_power_off(void);
static void fp_halt(void);

/* ------------------------------------------------------------------------- */

static struct i2c_driver fp_driver=
{
  name:                 "DBox2 Frontprocessor driver",
  id:                   I2C_FP_DRIVERID,
  flags:                I2C_DF_NOTIFY,
  attach_adapter:       &fp_attach_adapter,
  detach_client:        &fp_detach_client,
  command:              0,
  inc_use:              0,
  dec_use:              0
};

static struct file_operations fp_fops = {
        owner:          THIS_MODULE,
        read:           fp_read,
        write:          fp_write,
        ioctl:          fp_ioctl,
        open:           fp_open,
};

/* ------------------------------------------------------------------------- */

static int fp_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
    int val;

	switch (minor)
	{
		case FP_MINOR:
		{
			switch (cmd)
			{
				case FP_IOCTL_GETID:
					return fp_getid(defdata->client);
					break;

				case FP_IOCTL_POWEROFF:
					return fp_sendcmd(defdata->client, 0, 3);
					break;

				case FP_IOCTL_LCD_DIMM:
					if (copy_from_user(&val, (void*)arg, sizeof(val)) )
					{
						return -EFAULT;
					}

					return fp_sendcmd(defdata->client, 0x18, val&0x0f);
					break;

				case FP_IOCTL_LED:
					if (copy_from_user(&val, (void*)arg, sizeof(val)) )
					{
						return -EFAULT;
					}

					return fp_sendcmd(defdata->client, 0x10|(val&1), 0);
					break;

				default:
					return -EINVAL;
			}
		}

		case RC_MINOR:
			return -EINVAL;

		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static ssize_t fp_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
	return 0;
}                             

/* ------------------------------------------------------------------------- */

static ssize_t fp_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read;

	switch (minor)
	{
		case FP_MINOR:
			return -EINVAL;

		case RC_MINOR:
		{
			int i;
			DECLARE_WAITQUEUE(wait, current);

			for(;;)
			{
				if (rcbeg==rcend)
				{
					if (file->f_flags & O_NONBLOCK)
					{
						return count;
					}

					add_wait_queue(&rcwait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&rcwait, &wait);

					if (signal_pending(current))
					{
						return -ERESTARTSYS;
					}

					continue;
				}

				break;
			}

			count&=~1;
			read=0;

			for (i=0; i<count; i+=2)
			{
				if (rcbeg==rcend)
				{
					break;
				}

				*((u16*)(buf+i))=rcbuffer[rcbeg++];
				read+=2;

				if (rcbeg>=RCBUFFERSIZE)
				{
					rcbeg=0;
				}
			}

			return read;
		}

		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static int fp_open (struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);

	switch (minor)
	{
		case FP_MINOR:
			return 0;

		case RC_MINOR:
			if (rc_opened)              // ich weiss, so macht man das nicht...
				return -EAGAIN;
//    rc_opened++;  later
			return 0;

		default:
			return -ENODEV;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static int fp_detach_client(struct i2c_client *client)
{
	int err;

	free_irq(FP_INTERRUPT, client->data);

	if ((err=i2c_detach_client(client)))
	{
		dprintk("fp.o: couldn't detach client driver.\n");
		return err;
	}

	kfree(client);
	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags, int kind)
{
	int err = 0;
	struct i2c_client *new_client;
	struct fp_data *data;
	const char *client_name="DBox2 Frontprocessor client";

	if (!(new_client=kmalloc(sizeof(struct i2c_client)+sizeof(struct fp_data), GFP_KERNEL)))
	{
		return -ENOMEM;
	}

	new_client->data=new_client+1;
	defdata=data=(struct fp_data*)(new_client->data);
	rcbeg=0;
	rcend=0;
	new_client->addr=address;
	data->client=new_client;
	new_client->data=data;
	new_client->adapter=adapter;
	new_client->driver=&fp_driver;
	new_client->flags=0;
  
	if (kind<0)
	{
		int fpid;
		u8 buf[2];
		immap_t *immap=(immap_t*)IMAP_ADDR;

		/* FP ID
		 * NOKIA: 0x5A
		 * SAGEM: 0x52 ???
		 *
		 */

		fpid=fp_getid(new_client);

		if ( (fpid!=0x52) && (fpid!=0x5a) )
		{
			dprintk("fp.o: bogus fpID %d\n", fpid);
			kfree(new_client);
			return -1;
		}
    
		immap->im_ioport.iop_papar&=~2;
		immap->im_ioport.iop_paodr&=~2;
		immap->im_ioport.iop_padir|=2;
		immap->im_ioport.iop_padat&=~2;

/*    fp_sendcmd(new_client, 0x22, 0x40);
    fp_sendcmd(new_client, 0x22, 0xbf);
    fp_cmd(new_client, 0x25, buf, 2);
    fp_sendcmd(new_client, 0x19, 0x04);
    fp_sendcmd(new_client, 0x18, 0xb3);
    fp_cmd(new_client, 0x1e, buf, 2);
    fp_sendcmd(new_client, 0x26, 0x80); */
    
		fp_cmd(new_client, 0x23, buf, 1);
		fp_cmd(new_client, 0x20, buf, 1);
		fp_cmd(new_client, 0x01, buf, 2);
	}

	strcpy(new_client->name, client_name);
	new_client->id=fp_id++;

	if ((err=i2c_attach_client(new_client)))
	{
		kfree(new_client);
		return err;
	}

	if (request_8xxirq(FP_INTERRUPT, fp_interrupt, SA_ONESHOT, "fp", data) != 0)
	{
		panic("Could not allocate FP IRQ!");
	}

	dprintk("fp.o: attached fp @%02x\n", address>>1);

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, &fp_detect_client);
}

/* ------------------------------------------------------------------------- */

static int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, int size)
{
	struct i2c_msg msg[2];
	int i;

	msg[0].flags=0;
	msg[1].flags=I2C_M_RD;
	msg[0].addr=msg[1].addr=client->addr;

	msg[0].buf=&cmd;
	msg[0].len=1;
  
	msg[1].buf=res;
	msg[1].len=size;
  
	i2c_transfer(client->adapter, msg, 2);
  
	dprintk("fp.o: fp_cmd: %02x\n", cmd);
	dprintk("fp.o: fp_recv:");

	if(debug)
	{
		for (i=0; i<size; i++)
			dprintk(" %02x", res[i]);
		dprintk("\n");
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1)
{
	u8 cmd[2]={b0, b1};

	dprintk("fp.o: fp_sendcmd: %02x %02x\n", b0, b1);

	if (i2c_master_send(client, cmd, 2)!=2)
	{
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_getid(struct i2c_client *client)
{
	u8 id[3]={0, 0, 0};

	if (fp_cmd(client, FP_GETID, id, 3))
	{
		return 0;
	}

	return id[0];
}

/* ------------------------------------------------------------------------- */

static void fp_add_event(int code)
{
//  spin_lock_irq(&rc_lock);
	rcbuffer[rcend]=code;
	rcend++;
  
	if (rcend>=RCBUFFERSIZE)
	{
		rcend=0;
	}

	if (rcbeg==rcend)
	{
		printk("fp.o: RC overflow.\n");
	} else
	{
		wake_up(&rcwait);
	}
//  spin_unlock_irq(&rc_lock);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_rc(struct fp_data *dev)
{
	u16 rc;

	fp_cmd(dev->client, 0x1, (u8*)&rc, 2);
	fp_add_event(rc);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_button(struct fp_data *dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x25, (u8*)&rc, 1);
	fp_add_event(rc);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_unknown(struct fp_data *dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x24, (u8*)&rc, 1);
	dprintk("fp.o: misterious interrupt source 0x40: %x\n", rc);
}

/* ------------------------------------------------------------------------- */

static void fp_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	immap_t *immap=(immap_t*)IMAP_ADDR;

	immap->im_ioport.iop_padat|=2;
	schedule_task(&fp_tasklet);
	return;
}

/* ------------------------------------------------------------------------- */

static int fp_init(void)
{
	int res;

	printk("fp.o: DBox2 FP driver v0.1\n");

	init_waitqueue_head(&rcwait);

	if ((res=i2c_add_driver(&fp_driver)))
	{
		dprintk("fp.o: Driver registration failed, module not inserted.\n");
		return res;
	}

	if (!defdata)
	{
		i2c_del_driver(&fp_driver);
		dprintk("fp.o: Couldn't find FP.\n");
		return -EBUSY;
	}

//	if (register_chrdev(FP_MAJOR, "fp", &fp_fops))
//	{
//		i2c_del_driver(&fp_driver);
//		dprintk("fp.o: unable to get major %d\n", FP_MAJOR);
//		return -EIO;
//	}

  devfs_handle[FP_MINOR] =
    devfs_register ( NULL, "dbox/fp0", DEVFS_FL_DEFAULT, 0, FP_MINOR,
                     S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                     &fp_fops, NULL );

  if ( ! devfs_handle[FP_MINOR] )
  {
    i2c_del_driver ( &fp_driver );
    return -EIO;
  }

  devfs_handle[RC_MINOR] =
    devfs_register ( NULL, "dbox/rc0", DEVFS_FL_DEFAULT, 0, RC_MINOR,
                     S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                     &fp_fops, NULL );

  if ( ! devfs_handle[RC_MINOR] )
  {
    devfs_unregister ( devfs_handle[FP_MINOR] );
    i2c_del_driver ( &fp_driver );
    return -EIO;
  }

	ppc_md.restart=fp_restart;
	ppc_md.power_off=fp_power_off;
	ppc_md.halt=fp_halt;

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_close(void)
{
	int res;

	if ((res=i2c_del_driver(&fp_driver)))
	{
		dprintk("fp.o: Driver unregistration failed, module not removed.\n");
		return res;
	}

//	if ((res=unregister_chrdev(FP_MAJOR, "fp")))
//	{
//		dprintk("fp.o: unable to release major %d\n", FP_MAJOR);
//		return res;
//	}

  devfs_unregister ( devfs_handle[FP_MINOR] );
  devfs_unregister ( devfs_handle[RC_MINOR] );
  
	if (ppc_md.restart==fp_restart)
	{
		ppc_md.restart=0;
	}
    
	if (ppc_md.power_off==fp_power_off)
	{
		ppc_md.power_off=0;
	}
  
	if (ppc_md.halt==fp_halt)
	{
		ppc_md.halt=0;
	}
        
	return 0;
}

/* ------------------------------------------------------------------------- */

int fp_do_reset(int type)
{
	char msg[2]={0x22, type};

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	/* TODO: make better */
	udelay(100*1000);

	msg[1]=0xBF;

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static void fp_check_queues(void)
{
	u8 status;
	int iwork=0;
  
	dprintk("checking queues.\n");

	fp_cmd(defdata->client, 0x23, &status, 1);

	if (status)
	{
		dprintk("fp.o: Oops, status is %x (dachte immer der wäre 0...)\n", status);
	}

	iwork=0;

	do
	{
		fp_cmd(defdata->client, 0x20, &status, 1);
  
		/* remote control */
		if (status&9)
		{
			fp_handle_rc(defdata);
		}
  
		/* front button */
		if (status&0x10)
		{
			fp_handle_button(defdata);
		}

		/* ??? */
		if (status&0x40)
		{
			fp_handle_unknown(defdata);
		}

		if (iwork++ > 100)
		{
			dprintk("fp.o: Too much work at interrupt.\n");
			break;
		}

	} while (status & (0x59));            // only the ones we can handle

	if (status)
		dprintk("fp.o: unhandled interrupt source %x\n", status);

	return;
}

/* ------------------------------------------------------------------------- */

static void fp_task(void *arg)
{
	immap_t *immap=(immap_t*)IMAP_ADDR;

	fp_check_queues();
	immap->im_ioport.iop_padat&=~2;
	enable_irq(FP_INTERRUPT);
}

/* ------------------------------------------------------------------------- */

int fp_set_tuner_dword(int type, u32 tw)
{
	char msg[7]={0, 7, 0xC0};	/* default qam */
    int len=0;

	switch (type)
	{
		case T_QAM:
		{
			*((u32*)(msg+3))=tw;

			len = 7;

			dprintk("fp.o: fp_set_tuner_dword: QAM %08x\n", tw);

			break;
		}

		case T_QPSK:
		{
			*((u32*)(msg+2))=tw;
			msg[1] = 5;
			len = 6;

			dprintk("fp.o: fp_set_tuner_dword: QPSK %08x\n", tw);

			break;
  		}

		default:
			break;
	}

	if(len)
	{
		if (i2c_master_send(defdata->client, msg, len)!=len)
		{
			return -1;
		}
	}

	return -1;
}

/* ------------------------------------------------------------------------- */

int fp_send_diseqc(u32 dw)
{
	char msg[]={0, 0x1B};
	int c;
	*(u32*)(msg+2)=dw;

	i2c_master_send(defdata->client, msg, 6);

	for (c=0;c<100;c++)
	{
		fp_cmd(defdata->client, 0x2D, msg, 1);

		if ( !msg[0] )
		{
			break;
		}
	}

	if (c>100)
	{
		dprintk("fp.o: DiSEqC/DISEqC/DISeQc/dIS.,.... TIMEOUT jedenfalls.\n");
	} else
	{
		dprintk("fp.o: ja, das d.... hat schon nach %d versuchen ...  egal.\n", c);
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

int fp_set_polarisation(int pol)
{
	char msg[2]={0x21, 0};

	msg[1]=(pol==P_HOR)?0x51:0x71;

	dprintk("fp.o: fp_set_polarisation: %s\n", (pol==P_HOR)?"horizontal":"vertical");

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}
  
	return 0;
}

/* ------------------------------------------------------------------------- */

EXPORT_SYMBOL(fp_set_tuner_dword);
EXPORT_SYMBOL(fp_set_polarisation);
EXPORT_SYMBOL(fp_do_reset);
EXPORT_SYMBOL(fp_send_diseqc);

/* ------------------------------------------------------------------------- */

static void fp_restart(char *cmd)
{
	fp_sendcmd(defdata->client, 0, 20);
	for (;;);
}

/* ------------------------------------------------------------------------- */

static void fp_power_off(void)
{
	fp_sendcmd(defdata->client, 0, 3);
	for (;;);
}

/* ------------------------------------------------------------------------- */

static void fp_halt(void)
{
	fp_power_off();
}
                  
/* ------------------------------------------------------------------------- */

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 Frontprocessor");

MODULE_PARM(debug,"i");

int init_module(void)
{
	return fp_init();
}

/* ------------------------------------------------------------------------- */

void cleanup_module(void)
{
	fp_close();
}
#endif

/* ------------------------------------------------------------------------- */
