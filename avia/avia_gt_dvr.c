/*
 *   enx-dvr.c - Queue-Insertion driver (dbox-II-project)
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
 *
 *   Revision 1.0  2001/31/07 00:37:12  TripleDES
 *   - initial release
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
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

#include "dbox/gtx.h"
#include "dbox/enx.h"
#include "dbox/avia.h"

extern void avia_set_pcr(u32 hi, u32 lo);

static unsigned char* enxmem;
static unsigned char* gtxreg;

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
static void videoint(int x,int y)
{
	unsigned int vqpoint;

	if(vstate)
		wake_up(&frame_wait);

//	vqpoint=rh(VQRPH)<<16;
//	vqpoint|=rh(VQRPL);
		
//	printk("VINT:%d\n",vqpoint);	

		
}

static void audioint(int x,int y)
{
	unsigned int vqpoint;
	if(astate)
		wake_up(&aframe_wait);
		
//	vqpoint=rh(AQRPH)<<16;
//	vqpoint|=rh(AQRPL);
		
//	printk("AINT:%d\n",vqpoint);	
}

static ssize_t iframe_write (struct file *file, const char *buf, size_t count,loff_t *offset)
{
		int write=count;

		if((vpointer + count) >= vqsize)
		{
			write=((vqsize)-vpointer);
			memcpy(enxmem+vpointer,buf,write);
			vpointer=0;
			memcpy(enxmem+vpointer,buf+write,count-write);
			vpointer=count-write;
		}	
		else
		{
			memcpy(enxmem+vpointer,buf,count);
			vpointer+=count;
		}	

#ifdef ENX		
		enx_reg_h(VQWPL)=vpointer & 0xffff; 
		enx_reg_h(VQWPH)=(getQ_Size(vqsize)<<6)|(vpointer >> 16); 
#else
		rh(VQWPL)=vpointer & 0xffff;
		rh(VQWPH)=(getQ_Size(vqsize)<<6)|(vpointer >> 16); 
#endif		
		
		if(vpointer >= vqsize) vpointer=0;
		
#if 1		
		DECLARE_WAITQUEUE(wait,current);
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
		unsigned char *buffer=buf;
		
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
			memcpy(enxmem+vqsize+apointer,buf,write);
			apointer=0;
			memcpy(enxmem+vqsize+apointer,buf+write,count-write);
			apointer=count-write;
		}	
		else
		{
			memcpy(enxmem+vqsize+apointer,buf,count);
			apointer+=count;
		}	

#ifdef ENX
		enx_reg_h(AQWPL)= apointer & 0xffff; 
		enx_reg_h(AQWPH)= (getQ_Size(aqsize)<<6)|(4+((apointer >> 16) & 0x3f));
#else
		rh(AQWPL)= apointer & 0xffff; 
		rh(AQWPH)= (getQ_Size(aqsize)<<6)|(4+((apointer >> 16) & 0x3f));
#endif		
		
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
		
#ifdef ENX		
		enxmem=enx_get_mem_addr();	
#else		
		enxmem=gtx_get_mem();
		gtxreg=gtx_get_reg();
#endif

		memset(enxmem,0,1024*1024*2);
		
#ifdef ENX				
		enx_reg_w(CFGR0) |= 3;

		enx_reg_w(CFGR0)&=~0x10;
		for(i=0;i<16;i++)
		{
			oldQWPL[i]=enx_reg_h(QWPnL+(i*4));
			oldQWPH[i]=enx_reg_h(QWPnH+(i*4));
			enx_reg_h(QWPnL+(i*4))=0;
			enx_reg_h(QWPnH+(i*4))=8;
		}	
		enx_reg_w(CFGR0)|=0x10;
		for(i=0;i<16;i++)
		{
			oldQWPL[16+i]=enx_reg_h(QWPnL+(i*4));
			oldQWPH[16+i]=enx_reg_h(QWPnH+(i*4));
			enx_reg_h(QWPnL+(i*4))=0;
			enx_reg_h(QWPnH+(i*4))=8;
		}	
		enx_reg_h(TQRPL)=0; 
		enx_reg_h(TQRPH)=8; 
		enx_reg_h(TQWPL)=0; 
		enx_reg_h(TQWPH)=8; 
#else
		rh(CR1) |= 3;

#if 1
		rh(CR1)&=~(1<<4);
		for(i=0;i<16;i++)
		{
			oldQWPL[i]=rh(QWPnL+(i*4));
			oldQWPH[i]=rh(QWPnH+(i*4));
			rh(QWPnL+(i*4))=0;
		
			rh(QWPnH+(i*4))=10;
		}	
		rw(CR1)|=(1<<4);
		for(i=0;i<16;i++)
		{
			oldQWPL[16+i]=rh(QWPnL+(i*4));
			oldQWPH[16+i]=rh(QWPnH+(i*4));
			rh(QWPnL+(i*4))=0;
			rh(QWPnH+(i*4))=10;
		}
		rh(TQRPL)=0; 
		rh(TQRPH)=10; 
		rh(TQWPL)=0; 
		rh(TQWPH)=10; 
#endif			


#endif				

		memset(enxmem,0,vqsize+aqsize);

#ifdef ENX
		enx_reg_h(VQRPL)=0; 
		enx_reg_h(VQRPH)=0; 
		enx_reg_h(VQWPL)=0; 
		enx_reg_h(VQWPH)=(getQ_Size(vqsize)<<6)|0; 

		enx_reg_h(AQRPL)=0; 
		enx_reg_h(AQRPH)=4; 
		enx_reg_h(AQWPL)=0; 
		enx_reg_h(AQWPH)=(getQ_Size(aqsize)<<6)|4; 
#else
		rh(VQRPL)=0; 
		rh(VQWPL)=0; 
		rh(VQWPH)=(getQ_Size(vqsize)<<6)|0; 
		rh(VQRPH)=0; 

		rh(AQRPL)=0; 
		rh(AQWPL)=0; 
		rh(AQWPH)=(getQ_Size(aqsize)<<6)|4; 
		rh(AQRPH)=4; 
#endif		
	
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

#ifdef ENX		
		enx_reg_w(CFGR0)&=~0x10;
		enx_reg_h(QnINT) = (1<<15)|(1<<14); //14
		enx_reg_h(QnINT+2) = (1<<15)|(1<<13);
		enx_reg_h(QnINT+4) = (1<<15)|(1<<13);
#else
		rh(CR1)&=~(1<<4);
		rh(QI0) = (1<<15)|(1<<14); //14
		rh(QI0+2) = (1<<15)|(1<<14);
		rh(QI0+4) = (1<<15)|(1<<14);

		rh(CR1)&=~(1<<4);
		rh(QI0) = (1<<14); //14
		rh(QI0+2) = (1<<11);
		rh(QI0+4) = (1<<10);
		
		rh(CR0) &= ~(1<<8);
#endif	


#ifdef ENX		
		enx_free_irq(5,6);			
		enx_free_irq(5,7);			

		enx_allocate_irq(5,6,videoint);
		enx_allocate_irq(5,7,audioint);
#else
		gtx_free_irq(2,0);			
		gtx_free_irq(2,1);			

		gtx_allocate_irq(2,0,videoint);
		gtx_allocate_irq(2,1,audioint);

#endif		
		
		init_waitqueue_head(&frame_wait);
		init_waitqueue_head(&aframe_wait);
		up(&lock_open);
		up(&alock_open);

    return 0;
}

static void enx_close(void)
{
	int i;
	
#ifdef ENX	
	enx_free_irq(5,6);			
	enx_free_irq(5,7);
	enx_reg_w(CFGR0) &= ~3;
#else
	gtx_free_irq(2,0);			
	gtx_free_irq(2,1);
	rw(CR1) &= ~3;
#endif	

#if 0
	enx_reg_w(CFGR0)&=~0x10;

	for(i=0;i<16;i++)
	{
		enx_reg_h(QWPnL+(i*4))=oldQWPL[i];
		enx_reg_h(QWPnH+(i*4))=oldQWPH[i];
	}	
	enx_reg_w(CFGR0)|=0x10;
	for(i=0;i<16;i++)
	{
		enx_reg_h(QWPnL+(i*4))=oldQWPL[16+i];
		enx_reg_h(QWPnH+(i*4))=oldQWPH[16+i];
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
