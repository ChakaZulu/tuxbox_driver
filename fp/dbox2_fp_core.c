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
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/8xx_immap.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>

#include "dbox/fp.h"

/*
      exported functions:
      
      int fp_set_tuner_dword(int type, u32 tw);
        T_QAM
        T_QPSK
      int fp_set_polarisation(int pol);
        P_HOR
        P_VERT

*/

int fp_set_tuner_dword(int type, u32 tw);

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

// #define DEBUG_FP

#define FP_MAJOR            60
#define FP_MINOR            0
#define RC_MINOR            1

#define FP_INTERRUPT        SIU_IRQ2
#define I2C_FP_DRIVERID     0xF060
#define RCBUFFERSIZE        16
#define FP_GETID            0x1D

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

static spinlock_t rc_lock=SPIN_LOCK_UNLOCKED;
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


static int fp_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
  unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
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
    default:
      return -EINVAL;
    }
  }
  case RC_MINOR:
    return -EINVAL;
  default:
    return -EINVAL;
  }
}

static ssize_t fp_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  return 0;
}                             

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

    count&=~1;
    read=0;

again:
    if (rcbeg==rcend)
    {
      if (file->f_flags & O_NONBLOCK)
        return read;

      add_wait_queue(&rcwait, &wait);
      set_current_state(TASK_INTERRUPTIBLE);
      schedule();
      current->state = TASK_RUNNING;
      remove_wait_queue(&rcwait, &wait);
      if (signal_pending(current)) 
        return -ERESTARTSYS;
      goto again;
    }

    for (i=0; i<count; i+=2)
    {
      if (rcbeg==rcend)
        break;
      *((u16*)(buf+i))=rcbuffer[rcbeg++];
      read+=2;
      if (rcbeg>=RCBUFFERSIZE)
        rcbeg=0;
    }
    return read;
  }
  default:
    return -EINVAL;
  }
}

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
}

static int fp_detach_client(struct i2c_client *client)
{
  int err;
  free_irq(FP_INTERRUPT, client->data);
  if ((err=i2c_detach_client(client)))
  {
    printk("fp.o: couldn't detach client driver.\n");
    return err;
  }
  kfree(client);
  return 0; 
}

static int fp_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags, int kind)
{
  int err=0;
  struct i2c_client *new_client;
  struct fp_data *data;
  const char *client_name="DBox2 Frontprocessor client";
  if (!(new_client=kmalloc(sizeof(struct i2c_client)+sizeof(struct fp_data), GFP_KERNEL)))
    return -ENOMEM;
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
    if ((fpid=fp_getid(new_client))!=0x5A)
    {
      printk("bogus fpID %d\n", fpid);
      goto ERROR1;
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
    goto ERROR3;
  if (request_8xxirq(FP_INTERRUPT, fp_interrupt, 0, "fp", data) != 0)
    panic("Could not allocate FP IRQ!");
  printk("attached fp @%02x\n", address>>1);
  return 0;
ERROR3:
ERROR1:
  kfree(new_client);
  return err;
}

static int fp_attach_adapter(struct i2c_adapter *adapter)
{
  return i2c_probe(adapter, &addr_data, &fp_detect_client);
}

static int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, int size)
{
  struct i2c_msg msg[2];
#ifdef DEBUG_FP
  int i;
#endif    
  msg[0].flags=0;
  msg[1].flags=I2C_M_RD;
  msg[0].addr=msg[1].addr=client->addr;

  msg[0].buf=&cmd;
  msg[0].len=1;
  
  msg[1].buf=res;
  msg[1].len=size;
  
  i2c_transfer(client->adapter, msg, 2);
  
#ifdef DEBUG_FP
  printk("fp_cmd: %02x\n", cmd);
  printk("fp_recv:");
  for (i=0; i<size; i++)
    printk(" %02x", res[i]);
  printk("\n");
#endif
  return 0;
}

static int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1)
{
  u8 cmd[2]={b0, b1};
#ifdef DEBUG_FP
  printk("fp_sendcmd: %02x %02x\n", b0, b1);
#endif
  if (i2c_master_send(client, cmd, 2)!=2)
    return -1;
  return 0;
}

static int fp_getid(struct i2c_client *client)
{
  u8 id[3]={0, 0, 0};
  if (fp_cmd(client, FP_GETID, id, 3))
    return 0;
  return id[0];
}


static void fp_add_event(int code)
{
//  spin_lock_irq(&rc_lock);
  rcbuffer[rcend]=code;
  rcend++;
  
  if (rcend>=RCBUFFERSIZE)
    rcend=0;
  if (rcbeg==rcend)
    printk("RC overflow.\n");
  else
    wake_up(&rcwait);
//  spin_unlock_irq(&rc_lock);
}

static void fp_handle_rc(struct fp_data *dev)
{
  u16 rc;
  fp_cmd(dev->client, 0x1, (u8*)&rc, 2);
  fp_add_event(rc);
}

static void fp_handle_button(struct fp_data *dev)
{
  u8 rc;
  fp_cmd(dev->client, 0x25, (u8*)&rc, 1);
  fp_add_event(rc);
}

static void fp_handle_unknown(struct fp_data *dev)
{
  u8 rc;
  fp_cmd(dev->client, 0x24, (u8*)&rc, 1);
  printk("misterious interrupt source 0x40: %x\n", rc);
}

static void fp_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
  immap_t *immap=(immap_t*)IMAP_ADDR;
  immap->im_ioport.iop_padat|=2;
//  schedule_task(&fp_tasklet);
  fp_task(0);
  return ;
}


static int fp_init(void)
{
  int res;
  printk("DBox2 FP driver v0.1\n");

  if ((res=i2c_add_driver(&fp_driver)))
  {
    printk("fp: Driver registration failed, module not inserted.\n");
    return res;
  }
  if (!defdata)
  {
    i2c_del_driver(&fp_driver);
    printk("Couldn't find FP.\n");
    return -EBUSY;
  }
  init_waitqueue_head(&rcwait);
  if (register_chrdev(FP_MAJOR, "fp", &fp_fops))
  {
    printk("fp.o: unable to get major %d\n", FP_MAJOR);
    return -EIO;
  }
  
  ppc_md.restart=fp_restart;
  ppc_md.power_off=fp_power_off;
  ppc_md.halt=fp_halt;

  return 0;
}

static int fp_close(void)
{
  int res;
  if ((res=i2c_del_driver(&fp_driver)))
  {
    printk("fp: Driver unregistration failed, module not removed.\n");
    return res;
  }
  if ((res=unregister_chrdev(FP_MAJOR, "fp")))
  {
    printk("fp.o: unable to release major %d\n", FP_MAJOR);
    return res;
  }
  
  if (ppc_md.restart==fp_restart)
    ppc_md.restart=0;
    
  if (ppc_md.power_off==fp_power_off)
    ppc_md.power_off=0;
  
  if (ppc_md.halt==fp_halt)
    ppc_md.halt=0;
        
  return 0;
}

int fp_do_reset(int type)
{
  char msg[2]={0x22, type};
  if (i2c_master_send(defdata->client, msg, 2)!=2)
    return -1;
  udelay(100*1000);     // so issen in der br soft auch. ich weiss. das ist scheisse.
  msg[1]=0xBF;
  if (i2c_master_send(defdata->client, msg, 2)!=2)
    return -1;
  return 0;  
}

int fp_set_tuner_dword(int type, u32 tw)
{
  switch (type)
  {
  case T_QAM:
  {
    char msg[7]={0, 7, 0xC0};
    *((u32*)(msg+3))=tw;
#ifdef DEBUG_FP
    printk("fp_set_tuner_dword: QAM %08x\n", tw);
#endif
    if (i2c_master_send(defdata->client, msg, 7)!=7)
      return -1;
    return 0;
  }
  case T_QPSK:
  {
    char msg[6]={0, 5};
    *((u32*)(msg+2))=tw;
#ifdef DEBUG_FP
    printk("fp_set_tuner_dword: QPSK %08x\n", tw);
#endif
    if (i2c_master_send(defdata->client, msg, 6)!=6)
      return -1;
    return 0;
  }
  }
  return -1;
}

static void fp_check_queues(void)
{
  u8 status;
  int iwork=0;
  
  printk("checking queues.\n");

  fp_cmd(defdata->client, 0x23, &status, 1);
  if (status)
    printk("fp.o: Oops, status is %x (dachte immer der wäre 0...)\n", status);
  
  iwork=0;
  do
  {
    fp_cmd(defdata->client, 0x20, &status, 1); 
  
    if (status&1)
      fp_handle_rc(defdata);
  
    if (status&0x10)
      fp_handle_button(defdata);
  
    if (status&0x40)
      fp_handle_unknown(defdata);
    if (iwork++ > 100)
    {
      printk("fp.o: Too much work at interrupt.\n");
      break;
    }
  } while (status & (0x51));            // only the ones we can handle
  if (status)
    printk("fp.o: unhandled interrupt source %x\n", status);
}

static void fp_task(void *arg)
{
  immap_t *immap=(immap_t*)IMAP_ADDR;
  fp_check_queues();
  immap->im_ioport.iop_padat&=~2;
}

int fp_send_diseqc(u32 dw)
{
  char msg[]={0, 0x1B};
  int c=0;
  *(u32*)(msg+2)=dw;
  i2c_master_send(defdata->client, msg, 6);
  for (;;)
  {
    fp_cmd(defdata->client, 0x2D, msg, 1);
    if (!msg[0])
      break;
    if (c++>100)
      break;
  }
  if (c>100)
    printk("DiSEqC/DISEqC/DISeQc/dIS.,.... TIMEOUT jedenfalls.\n");
  else
    printk("ja, das d.... hat schon nach %d versuchen ...  egal.\n", c);
  return 0;
}

int fp_set_polarisation(int pol)
{
  char msg[2]={0x21, 0};
//  int i;
  msg[1]=(pol==P_HOR)?0x51:0x71;
#ifdef DEBUG_FP
  printk("fp_set_polarisation: %s\n", (pol==P_HOR)?"horizontal":"vertical");
#endif
  if (i2c_master_send(defdata->client, msg, 2)!=2)
    return -1;
  
  return 0;
}

EXPORT_SYMBOL(fp_set_tuner_dword);
EXPORT_SYMBOL(fp_set_polarisation);
EXPORT_SYMBOL(fp_do_reset);
EXPORT_SYMBOL(fp_send_diseqc);

static void fp_restart(char *cmd)
{
  fp_sendcmd(defdata->client, 0, 20);
  for (;;);
}

static void fp_power_off(void)
{
  fp_sendcmd(defdata->client, 0, 3);
  for (;;);
}

static void fp_halt(void)
{
  fp_power_off();
}
                  
#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 Frontprocessor");

int init_module(void)
{
  return fp_init();
}

///////////////////////////////////////////////////////////////////////////////

void cleanup_module(void)
{
  fp_close();
}
#endif
