/*
 *   avia_gt_dvr.c - Queue-Insertion driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Ronny "3des" Strutz (3des@tuxbox.org)
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
 *   $Log: avia_gt_dvr.c,v $
 *   Revision 1.3  2002/06/11 22:37:18  Jolt
 *   DVR fixes
 *
 *   Revision 1.2  2002/06/11 22:12:52  Jolt
 *   DVR merge
 *
 *   Revision 1.1  2002/06/11 22:09:18  Jolt
 *   DVR driver added
 *
 *   Revision 1.0  2001/07/31 00:37:12  TripleDES
 *   - initial release
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>

#include "dbox/avia.h"
#include "dbox/avia_gt.h"

extern void avia_set_pcr(u32 hi, u32 lo);

static sAviaGtInfo *gt_info;

static unsigned int vpointer=0;
static unsigned int apointer=0;

unsigned int oldQWPH[32];
unsigned int oldQWPL[32];

int blax=0;
int uga=0;

unsigned int oldvqrp=0;

static int vqsize=1024*256; //256
static int aqsize=1024*32; //16

static wait_queue_head_t frame_wait;
static wait_queue_head_t aframe_wait;
static DECLARE_MUTEX_LOCKED(lock_open);
static DECLARE_MUTEX_LOCKED(alock_open);

static int vstate=0;
static int astate=0;

static int start=1;

static int ax=2,vx=30;

static devfs_handle_t devfs_handle;
static ssize_t iframe_write (struct file *file, const char *buf, size_t count,loff_t *offset);

static devfs_handle_t adevfs_handle;
static ssize_t aiframe_write (struct file *file, const char *buf, size_t count,loff_t *offset);

static int iframe_open (struct inode *inode, struct file *file);
static int iframe_release (struct inode *inode, struct file *file);

static int aiframe_open (struct inode *inode, struct file *file);
static int aiframe_release (struct inode *inode, struct file *file);

static struct file_operations iframe_fops = {
    owner:	THIS_MODULE,
    write:	iframe_write,
    open:	iframe_open,
    release:	iframe_release,
};

static struct file_operations aiframe_fops = {
    owner:	THIS_MODULE,
    write:	aiframe_write,
    open:	aiframe_open,
    release:	aiframe_release,
};

static int iframe_open (struct inode *inode, struct file *file){
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	switch (minor)
	{
		case 0:
			if (file->f_flags & O_NONBLOCK)
			{
				if (down_trylock(&lock_open))
					return -EAGAIN;
			}	else
			{
			if (down_interruptible(&lock_open))
					return -ERESTARTSYS;
			}
			printk("dvr-video: open\n");
			return 0;
	default:
		return -ENODEV;
	}

  return 0;
}

static int iframe_release (struct inode *inode, struct file *file){
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
		
	switch (minor)
	{
		case 0:
			up(&lock_open);
			return 0;
	}
	return -EINVAL;
}
static int aiframe_open (struct inode *inode, struct file *file){
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	switch (minor)
	{
		case 0:
			if (file->f_flags & O_NONBLOCK)
			{
				if (down_trylock(&alock_open))
					return -EAGAIN;
			}	else
			{
			if (down_interruptible(&alock_open))
					return -ERESTARTSYS;
			}
			printk("dvr-audio: open\n");

			start=1;
			avia_flush_pcr();
			wDR(AV_SYNC_MODE,0);
			avia_command(NewChannel,0,0,0);
			//wDR(0x468,0x1);
			
			return 0;
	default:
		return -ENODEV;
	}
  return 0;
}

static int aiframe_release (struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
		
	switch (minor)
	{
		case 0:
			up(&alock_open);
			return 0;
	}
	return -EINVAL;
}

static int getQ_Size(unsigned int Bytes)
{
	int i;	
	for(i=0;i<17;i++) if((Bytes/64 >> i)==1) break;
	return i;
}
static void videoint(u16 irq)
{
//	unsigned int vqpoint;

	if(vstate)
		wake_up(&frame_wait);

//	vqpoint=gtx_reg_16(VQRPH)<<16;
//	vqpoint|=gtx_reg_16(VQRPL);
		
//	printk("VINT:%d\n",vqpoint);	

		
}

static void audioint(u16 irq)
{
//	unsigned int vqpoint;
	if(astate)
		wake_up(&aframe_wait);
		
//	vqpoint=gtx_reg_16(AQRPH)<<16;
//	vqpoint|=gtx_reg_16(AQRPL);
		
//	printk("AINT:%d\n",vqpoint);	
}

static ssize_t iframe_write (struct file *file, const char *buf, size_t count,loff_t *offset)
{
		int write=count;
		DECLARE_WAITQUEUE(wait,current);

		if((vpointer + count) >= vqsize)
		{
			write=((vqsize)-vpointer);
			memcpy(gt_info->mem_addr+vpointer,buf,write);
			vpointer=0;
			memcpy(gt_info->mem_addr+vpointer,buf+write,count-write);
			vpointer=count-write;
		}	
		else
		{
			memcpy(gt_info->mem_addr+vpointer,buf,count);
			vpointer+=count;
		}	

    if (avia_gt_chip(ENX)) {
	
		enx_reg_16(VQWPL)=vpointer & 0xffff; 
		enx_reg_16(VQWPH)=(getQ_Size(vqsize)<<6)|(vpointer >> 16); 
		
    } else if (avia_gt_chip(GTX)) {
	
		gtx_reg_16(VQWPL)=vpointer & 0xffff;
		gtx_reg_16(VQWPH)=(getQ_Size(vqsize)<<6)|(vpointer >> 16); 
		
	}
		
		if(vpointer >= vqsize) vpointer=0;
		
#if 1		
		add_wait_queue(&frame_wait,&wait);
		set_current_state(TASK_INTERRUPTIBLE);
		vstate=1;
		schedule();
		current->state = TASK_RUNNING;
		remove_wait_queue(&frame_wait,&wait);
		vstate=0;
		vx=10;
#endif 

	  return count;
}        

static ssize_t aiframe_write (struct file *file, const char *buf, size_t count,loff_t *offset)
{
		int write=count;
		int i;
		u32 ptc=0;
		unsigned char *buffer= (unsigned char *)buf;
		
		if(start)
		for(i=0;i<count-13;i++)
		{
			if(buffer[i]==0x00 && buffer[i+1]==0x00 && buffer[i+2]==0x01 &&
			 ((buffer[+7]&0xc0)==0xc0 ||	(buffer[+7]&0xc0)==0x80))
				if((buffer[i+9] & 0xF0) == 0x20 || (buffer[i+9] & 0xF0) == 0x30)
				{
					//for(x=i;x<i+14;x++)printk("%02x ",buffer[x]);
					//printk("\n");
					
					ptc = (buffer[i+13] & 0xfe) >> 1;
					ptc|=  buffer[i+12]<< 7;
					ptc|= (buffer[i+11]>> 1)<<15;
					ptc|=  buffer[i+10]<< 22;
					ptc|=(((buffer[i+9] & 0xe) >> 1)&0x7F )<<30;

					printk("%08x\n",ptc & 0xffffff);
					//avia_set_pcr((buffer[i+9] & 8)<<31|ptc>>1,(ptc & 1)<<31);
					start++;
					if(start>=1)start=0;
					break;
				}
		}			
					
		if((apointer + count) >= aqsize)
		{
			write=((aqsize)-apointer);
			memcpy(gt_info->mem_addr+vqsize+apointer,buf,write);
			apointer=0;
			memcpy(gt_info->mem_addr+vqsize+apointer,buf+write,count-write);
			apointer=count-write;
		}	
		else
		{
			memcpy(gt_info->mem_addr+vqsize+apointer,buf,count);
			apointer+=count;
		}	

    if (avia_gt_chip(ENX)) {
	
		enx_reg_16(AQWPL)= apointer & 0xffff; 
		enx_reg_16(AQWPH)= (getQ_Size(aqsize)<<6)|(4+((apointer >> 16) & 0x3f));

    } else if (avia_gt_chip(GTX)) {

		gtx_reg_16(AQWPL)= apointer & 0xffff; 
		gtx_reg_16(AQWPH)= (getQ_Size(aqsize)<<6)|(4+((apointer >> 16) & 0x3f));

	}
		
		if(apointer >= aqsize) apointer=0;
		
#if 1		
		DECLARE_WAITQUEUE(wait,current);
		add_wait_queue(&aframe_wait,&wait);
		set_current_state(TASK_INTERRUPTIBLE);
		astate=1;
		schedule();
		current->state = TASK_RUNNING;
		remove_wait_queue(&aframe_wait,&wait);
		astate=0;
		ax=2;
#endif		
	  return count;
}        



static int enx_iframe_init(void)
{
		int i;
    devfs_handle = devfs_register(NULL,"dvrv",DEVFS_FL_DEFAULT,0,0,S_IFCHR|
				  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
				  &iframe_fops,NULL);

    adevfs_handle = devfs_register(NULL,"dvra",DEVFS_FL_DEFAULT,0,0,S_IFCHR|
				  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
				  &aiframe_fops,NULL);

    printk("avia_gt_dvr: $Id: avia_gt_dvr.c,v 1.3 2002/06/11 22:37:18 Jolt Exp $\n");
	
    gt_info = avia_gt_get_info();
		
    if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		
        printk("avia_gt_dvr: Unsupported chip type\n");
					
        return -EIO;
							
    }
										
		memset(gt_info->mem_addr,0,1024*1024*2);
		
    if (avia_gt_chip(ENX)) {

		enx_reg_32(CFGR0) |= 3;

		enx_reg_32(CFGR0)&=~0x10;
		for(i=0;i<16;i++)
		{
			oldQWPL[i]=enx_reg_16(QWPnL+(i*4));
			oldQWPH[i]=enx_reg_16(QWPnH+(i*4));
			enx_reg_16(QWPnL+(i*4))=0;
			enx_reg_16(QWPnH+(i*4))=8;
		}	
		enx_reg_32(CFGR0)|=0x10;
		for(i=0;i<16;i++)
		{
			oldQWPL[16+i]=enx_reg_16(QWPnL+(i*4));
			oldQWPH[16+i]=enx_reg_16(QWPnH+(i*4));
			enx_reg_16(QWPnL+(i*4))=0;
			enx_reg_16(QWPnH+(i*4))=8;
		}	
		enx_reg_16(TQRPL)=0; 
		enx_reg_16(TQRPH)=8; 
		enx_reg_16(TQWPL)=0; 
		enx_reg_16(TQWPH)=8; 
		
    } else if (avia_gt_chip(GTX)) {
	
		gtx_reg_16(CR1) |= 3;

#if 1
		gtx_reg_16(CR1)&=~(1<<4);
		for(i=0;i<16;i++)
		{
			oldQWPL[i]=gtx_reg_16(QWPnL+(i*4));
			oldQWPH[i]=gtx_reg_16(QWPnH+(i*4));
			gtx_reg_16(QWPnL+(i*4))=0;
		
			gtx_reg_16(QWPnH+(i*4))=10;
		}	
		gtx_reg_32(CR1)|=(1<<4);
		for(i=0;i<16;i++)
		{
			oldQWPL[16+i]=gtx_reg_16(QWPnL+(i*4));
			oldQWPH[16+i]=gtx_reg_16(QWPnH+(i*4));
			gtx_reg_16(QWPnL+(i*4))=0;
			gtx_reg_16(QWPnH+(i*4))=10;
		}
		gtx_reg_16(TQRPL)=0; 
		gtx_reg_16(TQRPH)=10; 
		gtx_reg_16(TQWPL)=0; 
		gtx_reg_16(TQWPH)=10; 
#endif			

	}
		
		memset(gt_info->mem_addr,0,vqsize+aqsize);

    if (avia_gt_chip(ENX)) {

		enx_reg_16(VQRPL)=0; 
		enx_reg_16(VQRPH)=0; 
		enx_reg_16(VQWPL)=0; 
		enx_reg_16(VQWPH)=(getQ_Size(vqsize)<<6)|0; 

		enx_reg_16(AQRPL)=0; 
		enx_reg_16(AQRPH)=4; 
		enx_reg_16(AQWPL)=0; 
		enx_reg_16(AQWPH)=(getQ_Size(aqsize)<<6)|4; 

    } else if (avia_gt_chip(GTX)) {

		gtx_reg_16(VQRPL)=0; 
		gtx_reg_16(VQWPL)=0; 
		gtx_reg_16(VQWPH)=(getQ_Size(vqsize)<<6)|0; 
		gtx_reg_16(VQRPH)=0; 

		gtx_reg_16(AQRPL)=0; 
		gtx_reg_16(AQWPL)=0; 
		gtx_reg_16(AQWPH)=(getQ_Size(aqsize)<<6)|4; 
		gtx_reg_16(AQRPH)=4; 

	}
		
		printk("AQRPL:%d\n",vqsize & 0xffff);
		printk("AQRPH:%d\n",(vqsize & 0xff0000)>>16);
		printk("AQWPL:%d\n",vqsize & 0xff);
		printk("AQWPH:%d\n",(vqsize & 0xff0000)>>16);

		printk("VQSIZE:%x\n",vqsize);
		printk("AQSIZE:%x\n",aqsize);

		printk("VQSIZE:%x\n",getQ_Size(vqsize));
		printk("AQSIZE:%x\n",getQ_Size(aqsize));
		
		wDR(AV_SYNC_MODE,0);
		wDR(VIDEO_PTS_DELAY,0);
		wDR(AUDIO_PTS_DELAY,0);
		avia_flush_pcr();
		avia_command(SetStreamType,0xB);
	  avia_command(NewChannel,0,0,0);        

    if (avia_gt_chip(ENX)) {

		enx_reg_32(CFGR0)&=~0x10;
		enx_reg_16(QnINT) = (1<<15)|(1<<14); //14
		enx_reg_16(QnINT+2) = (1<<15)|(1<<13);
		enx_reg_16(QnINT+4) = (1<<15)|(1<<13);

    } else if (avia_gt_chip(GTX)) {

		gtx_reg_16(CR1)&=~(1<<4);
		gtx_reg_16(QI0) = (1<<15)|(1<<14); //14
		gtx_reg_16(QI0+2) = (1<<15)|(1<<14);
		gtx_reg_16(QI0+4) = (1<<15)|(1<<14);

		gtx_reg_16(CR1)&=~(1<<4);
		gtx_reg_16(QI0) = (1<<14); //14
		gtx_reg_16(QI0+2) = (1<<11);
		gtx_reg_16(QI0+4) = (1<<10);
		
		gtx_reg_16(CR0) &= ~(1<<8);

	}

    if (avia_gt_chip(ENX)) {

		avia_gt_free_irq(AVIA_GT_IRQ(5, 6));			
		avia_gt_free_irq(AVIA_GT_IRQ(5, 7));			

		avia_gt_alloc_irq(AVIA_GT_IRQ(5, 6), videoint);
		avia_gt_alloc_irq(AVIA_GT_IRQ(5, 7), audioint);

    } else if (avia_gt_chip(GTX)) {
	
		avia_gt_free_irq(AVIA_GT_IRQ(2, 0));			
		avia_gt_free_irq(AVIA_GT_IRQ(2, 1));			

		avia_gt_alloc_irq(AVIA_GT_IRQ(2, 0), videoint);
		avia_gt_alloc_irq(AVIA_GT_IRQ(2, 1), audioint);

	}
		
		init_waitqueue_head(&frame_wait);
		init_waitqueue_head(&aframe_wait);
		up(&lock_open);
		up(&alock_open);

    return 0;
}

static void enx_close(void)
{
//	int i;
	
    if (avia_gt_chip(ENX)) {

		avia_gt_free_irq(AVIA_GT_IRQ(5, 6));			
		avia_gt_free_irq(AVIA_GT_IRQ(5, 7));
		enx_reg_32(CFGR0) &= ~3;

    } else if (avia_gt_chip(GTX)) {

		avia_gt_free_irq(AVIA_GT_IRQ(2, 0));			
		avia_gt_free_irq(AVIA_GT_IRQ(2, 1));
		gtx_reg_32(CR1) &= ~3;

	}

#if 0
	enx_reg_32(CFGR0)&=~0x10;

	for(i=0;i<16;i++)
	{
		enx_reg_16(QWPnL+(i*4))=oldQWPL[i];
		enx_reg_16(QWPnH+(i*4))=oldQWPH[i];
	}	
	enx_reg_32(CFGR0)|=0x10;
	for(i=0;i<16;i++)
	{
		enx_reg_16(QWPnL+(i*4))=oldQWPL[16+i];
		enx_reg_16(QWPnH+(i*4))=oldQWPH[16+i];
	}	
#endif		
	
			
 	devfs_unregister(devfs_handle);
 	devfs_unregister(adevfs_handle);
	down(&lock_open);
	down(&alock_open);

  return;
}

#ifdef MODULE
MODULE_AUTHOR("Ronny Strutz <3des@elitedvb.com>");
MODULE_DESCRIPTION("Video/Audio Playback Driver");

int init_module(void) {
  return enx_iframe_init();
}

void cleanup_module(void) {
  enx_close();
}
#endif
