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
#include <asm/semaphore.h>
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/8xx_immap.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>

#include <dbox/fp.h>

#define CAM_MAJOR            61
#define CAM_CODE_MINOR       0
#define CAM_DATA_MINOR       1
#define CAM_MINOR            2
#define I2C_DRIVERID_CAM     0x6E

#define USE_TASKLET
#define CAM_INTERRUPT        SIU_IRQ3
#define CAM_CODE_BASE        0xC000000
#define CAM_CODE_SIZE        0x20000
                              // BUG BUG ... oder? .so meint: 0xE00 0000, aber das geht nicht.
#define CAM_DATA_BASE        0xC020000
#define CAM_DATA_SIZE        0x10000

#define CAM_QUEUE_SIZE       0x1000

static DECLARE_MUTEX_LOCKED(cam_busy);

#ifdef USE_TASKLET
static int irqok=1;
#endif
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
        
unsigned char cam_code_buffer[CAM_CODE_SIZE];     // sorry
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

static int cam_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                      unsigned long arg)
{
  return -ENOSYS;
}

static ssize_t cam_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
  
  if ((minor == CAM_CODE_MINOR) || (minor == CAM_DATA_MINOR))
  {
    int numb, size;
    void *base;
  
    switch (minor)
    {
    case CAM_CODE_MINOR:
      base=cam_code_buffer;               // cam_code_base
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
  
    if (copy_from_user(((u8*)base)+file->f_pos, buf, numb))
      return -EFAULT; 
    
    *offset+=numb;
  
    if ((*offset==size) && (minor==CAM_CODE_MINOR))
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
      memcpy(code_base, cam_code_buffer, CAM_CODE_SIZE);
      memset(data_base, 'Z', CAM_DATA_SIZE);
      cp->cp_pbdat|=1;
      cp->cp_pbdat&=~2;
      cp->cp_pbdat|=2;
      udelay(100);
      fp_do_reset(0xAF);
    }
    return numb;
  } else if (minor==CAM_MINOR)
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

int cam_init(void)
{
  int res;
  init_waitqueue_head(&queuewait);

  code_base=ioremap(CAM_CODE_BASE, CAM_CODE_SIZE);
  data_base=ioremap(CAM_DATA_BASE, CAM_DATA_BASE);
  if (!code_base || !data_base)
    panic("couldn't map CAM-io.\n");
  if (register_chrdev(CAM_MAJOR, "cam", &cam_fops))
  {
    printk("cam.o: unable to get major %d\n", CAM_MAJOR);
    return -EIO;
  }

  if ((res = i2c_add_driver(&cam_driver)))
  {
    printk("CAM: Driver registration failed, module not inserted.\n");
    return res;
  }

  if (!dclient)
  {
    i2c_del_driver(&cam_driver);
    printk("CAM: cam not found.\n");
    return -EBUSY;
  }
  
  up(&cam_busy);

  if (request_8xxirq(CAM_INTERRUPT, cam_interrupt, 0, "cam", THIS_MODULE) != 0)
    panic("Could not allocate CAM IRQ!");

  return 0;
}

void cam_fini(void)
{
  int res;
  if ((res=unregister_chrdev(CAM_MAJOR, "cam")))
    printk("cam.o: unable to release major %d\n", CAM_MAJOR);
  free_irq(CAM_INTERRUPT, THIS_MODULE);
  schedule(); // HACK: let all task queues run.
  
  if ((res=down_interruptible(&cam_busy)))
    return;

  iounmap(code_base);
  iounmap(data_base);
  if ((res = i2c_del_driver(&cam_driver)))
  {
    printk("cam: Driver deregistration failed, module not removed.\n");
    return;
  }
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
#ifdef USE_TASKLET  
  if (down_interruptible(&cam_busy))
  {
    irqok=1;
    return;
  }
#endif

  if (i2c_master_recv(dclient, buffer, 2)!=2)
  {
    printk("i2c-CAM read error.\n");
#ifdef USE_TASKLET
    up(&cam_busy);
    irqok=1;
#endif
    return;
  }

  len=buffer[1]&0x7F;
  if (i2c_master_recv(dclient, buffer, len+3)!=len+3)
  {
    printk("i2c-CAM read error.\n");
#ifdef USE_TASKLET
    up(&cam_busy);
    irqok=1;
#endif
    return;
  }
  
  if ((buffer[1]&0x7F) != len)
  {
    len=buffer[1]&0x7F;
    printk("CAM: unsure length, reading again.\n");
    if (i2c_master_recv(dclient, buffer, len+3)!=len+3)
    {
      printk("i2c-CAM read error.\n");
#ifdef USE_TASKLET
      up(&cam_busy);
      irqok=1;
#endif
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
  
#ifdef USE_TASKLET
  up(&cam_busy);
  irqok=1;
#endif
}

static void cam_interrupt(int irq, void *dev, struct pt_regs * regs)
{
#ifdef USE_TASKLET
  char dummy[2];
  i2c_master_recv(dclient, (unsigned char*)dummy, 2);          // yo, we're reading.
  printk("CAM\n");
  if (irqok)
  {
    printk("cam_interrupt\n");
    irqok=0;
    schedule_task(&cam_tasklet);
  }
#else
  cam_task(0);
#endif
}

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 CAM Driver");

int init_module(void)
{
  return cam_init();
}

///////////////////////////////////////////////////////////////////////////////

void cleanup_module(void)
{
  cam_fini();
}
#endif
