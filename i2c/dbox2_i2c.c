/* modified for kernel */
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

#include "commproc.h"
#include <linux/i2c.h>

#define TXBD_R 0x8000  /* Transmit buffer ready to send */
#define TXBD_W 0x2000  /* Wrap, last buffer in buffer circle */
#define TXBD_I 0x1000
#define TXBD_L 0x0800  /* Last, this buffer is the last in this frame */
                       /* This bit causes the STOP condition to be sent */
#define TXBD_S 0x0400  /* Start condition.  Causes this BD to transmit a start */

#define TXBD_NAK 0x0004	 /* indicates nak */
#define TXBD_UN  0x0002  /* indicates underrun */
#define TXBD_CO  0x0001  /* indicates collision */

#define RXBD_E 0x8000  /* Receive buffer is empty and can be used by CPM */
#define RXBD_W 0x2000  /* Wrap, last receive buffer in buffer circle */
#define RXBD_I 0x1000  /* Int. */
#define RXBD_L 0x0800  /* Last */

#define RXBD_OV 0x0002		/* indicates overrun */


#define I2C_TX_LEN      128
#define I2C_RX_LEN      128

typedef volatile struct I2C_BD
{
  unsigned short status;
  unsigned short length;
  unsigned char  *addr;
} I2C_BD;

typedef volatile struct RX_TX_BD {
 I2C_BD rxbd;
 I2C_BD txbd[2];
} RXTXBD;

static I2C_BD *rxbd, *txbd;

unsigned char *rxbuf, *txbuf;

static	volatile i2c8xx_t	*i2c;
static	volatile iic_t		*iip;
static	volatile cpm8xx_t	*cp;

volatile cbd_t		*tbdf, *rbdf;

static inline int i2c_roundrate (int hz, int speed, int filter, int modval,
				    int *brgval, int *totspeed)
{
	int moddiv = 1 << (5-(modval & 3)),
	brgdiv,
	div;

  brgdiv = hz / (moddiv * speed);

  *brgval = brgdiv / 2 - 3 - 2*filter ;

  if ((*brgval < 0) || (*brgval > 255))
		return -1 ;

  brgdiv = 2 * (*brgval + 3 + 2 * filter) ;
  div  = moddiv * brgdiv ;
  *totspeed = hz / div ;

  return  0;
}

static int i2c_setrate (int hz, int speed)
{
  immap_t	*immap = (immap_t *)IMAP_ADDR ;
  i2c8xx_t	*i2c	= (i2c8xx_t *)&immap->im_i2c;
  int brgval,
      modval,           // 0-3
      bestspeed_diff = speed,
      bestspeed_brgval=0,
      bestspeed_modval=0,
      bestspeed_filter=0,
      totspeed,
      filter=0;         // Use this fixed value

  for (modval = 0; modval < 4; modval++)
  {
    if (i2c_roundrate(hz, speed, filter, modval, &brgval, &totspeed) == 0)
    {
      int diff = speed - totspeed;

      if ((diff >= 0) && (diff < bestspeed_diff))
      {
        bestspeed_diff=diff;
        bestspeed_modval=modval;
        bestspeed_brgval=brgval;
        bestspeed_filter=filter;
      }
    }
  }

#ifdef DEBUG_I2C_RATE
	printk("Best is:\n");
	printk("CPU=%dhz RATE=%d F=%d I2MOD=%08x I2BRG=%08x DIFF=%dhz\n",
	    hz, speed,
	    bestspeed_filter, bestspeed_modval, bestspeed_brgval,
	    bestspeed_diff);
#endif

  i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3);
  i2c->i2c_i2brg = bestspeed_brgval & 0xff;

#ifdef DEBUG_I2C_RATE
  printk("i2mod=%08x i2brg=%08x\n", i2c->i2c_i2mod, i2c->i2c_i2brg);
#endif

  return 1 ;
}

void i2c_init(int speed)
{
  immap_t *immap=(immap_t*)IMAP_ADDR;
  
  cp = (cpm8xx_t *)&immap->im_cpm ;
  iip = (iic_t *)&cp->cp_dparam[PROFF_IIC];
  i2c = (i2c8xx_t *)&(immap->im_i2c);

  // Disable relocation
  iip->iic_rbase = 0 ;

  // Initialize Port B I2C pins.
  cp->cp_pbpar |= 0x00000030;
  cp->cp_pbdir |= 0x00000030;
  cp->cp_pbodr |= 0x00000030;

  // Disable interrupts.
  i2c->i2c_i2mod = 0;
  i2c->i2c_i2cmr = 0;
  i2c->i2c_i2cer = 0xff;

  // Set the I2C BRG Clock division factor from desired i2c rate
  // and current CPU rate (we assume sccr dfbgr field is 0;
  // divide BRGCLK by 1)
  i2c_setrate (66*1024*1024, speed) ;
  
  /* Set I2C controller in master mode
   */
  i2c->i2c_i2com = 0x01;

  // Set SDMA bus arbitration level to 5 (SDCR)
  immap->im_siu_conf.sc_sdcr = 0x0001 ;

  iip->iic_rbptr = iip->iic_rbase = m8xx_cpm_dpalloc(16); //align(8) ;
  iip->iic_tbptr = iip->iic_tbase = iip->iic_rbase + sizeof(I2C_BD);
  
  rxbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_rbase]);
  txbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_tbase]);

  printk("RBASE = %04x\n", iip->iic_rbase);
  printk("TBASE = %04x\n", iip->iic_tbase);
  printk("RXBD1 = %08x\n", (int)rxbd);
  printk("TXBD1 = %08x\n", (int)txbd);

  /* Set big endian byte order
   */
  iip->iic_tfcr = 0x15;
  iip->iic_rfcr = 0x15;

  /* Set maximum receive size.
   */
  iip->iic_mrblr = 128;

// i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3);
  i2c->i2c_i2brg = 7;

    // Rx: Wrap, no interrupt, empty
  rxbuf      = (unsigned char*)m8xx_cpm_hostalloc(128);

	if (rxbuf==0)
	{
		printk("INIT ERROR: no RXBUF mem available\n");
	}

  rxbd->addr = (unsigned char*)__pa(rxbuf);
  printk("RXBD1->ADDR = %08X\n",(uint)rxbd->addr);
  rxbd->status = 0xa800;
  rxbd->length = 0;

  // Tx: Wrap, no interrupt, not ready to send, last
  txbuf      = (unsigned char*)m8xx_cpm_hostalloc(128);

	if (txbuf==0)
	{
		printk("INIT ERROR: no TXBUF mem available\n");
	}

  txbd->addr = (unsigned char*)__pa(txbuf);
  printk("TXBD1->ADDR = %08X\n",(uint)txbd->addr);

  txbd->status = 0x2800;
  txbd->length = 0;

	// Initialize the BD's
  while (cp->cp_cpcr & CPM_CR_FLG);
  cp->cp_cpcr = mk_cr_cmd(CPM_CR_CH_I2C, CPM_CR_INIT_TRX) | CPM_CR_FLG;
  while (cp->cp_cpcr & CPM_CR_FLG);

	// Clear events and interrupts
  i2c->i2c_i2cer = 0xff ;
  i2c->i2c_i2cmr = 0 ;
}

///////////////////////////////////////////////////////////////////////////////

int i2c_send( unsigned char address,  unsigned short size, unsigned char *dataout )
{
  int i,j,ret;

  if( size > I2C_TX_LEN )  /* Trying to send message larger than BD */
    return -1;

  i=0;
  while( (txbd->status & TXBD_R) && (i<1000))
  {
    i++;
    udelay(1000);
  }

	if (i==1000)
	{
    printk("TXBD: INITIAL TIMEOUT\n");
		return -1;
	}

  if(size==0)
   size++;
   
	ret = size;

  txbd->length = size + 1;  /* Length of message plus dest address */
  txbuf[0] = address;  /* Write destination address to BD */
  txbuf[0] &= ~(0x01);  /* Set address to write */
  i = 1;

  for(j=0; j<size; i++,j++)
    txbuf[i]=dataout[j];

  /* Ready to Transmit, wrap, last */
  txbd->status |= TXBD_R | TXBD_W;

  /* Enable I2C */
  i2c->i2c_i2mod |= 1;

  /* Transmit */
  i2c->i2c_i2com |= 0x80;

  while( txbd->status & TXBD_R );               // todo: sleep with irq

	/* some error msg */
	if ( txbd->status & TXBD_NAK )
	{
		printk("TXBD: NAK AT %02X\n",address);
		ret = -1;
	}
	if ( txbd->status & TXBD_UN )
	{
		printk("TXBD: UNDERRUN\n");
		ret = -1;
	}
	if ( txbd->status & TXBD_CO )
	{
		printk("TXBD: COLLISION\n");
		ret = -1;
	}
  if ( i==1000 )
	{
    printk("TXBD: TIMEOUT\n");
		ret = -1;
	}

  i2c->i2c_i2mod &= (~1);

	/* clear flags */
  txbd->status &= (~(TXBD_NAK|TXBD_UN|TXBD_CO));

  iip->iic_rstate = 0;
  iip->iic_tstate = 0;

  return ret;
}

///////////////////////////////////////////////////////////////////////////////
int i2c_receive(unsigned char address,
                unsigned short size_to_expect, unsigned char *datain )
{
  int i,j,ret;

  if( size_to_expect > I2C_RX_LEN )
	{
		return -1;
	}

	ret = size_to_expect;

  /* Setup TXBD for destination address */
  txbuf[0] = address | 0x01;

  txbd->length = 1 + size_to_expect;

	for(i=0;i<size_to_expect;i++)
		txbuf[i+1] = datain[i];

  if (!size_to_expect)
	{
    txbd->length++;
		txbuf[1] = 0;
	}

  txbd->status |= TXBD_R | TXBD_W | TXBD_S | TXBD_L;

  /* Reset the rxbd */
  rxbd->status |= RXBD_E | RXBD_W;

  /* Turn on I2C */
  i2c->i2c_i2mod |= 0x01;

  /* Begin transmission */
  i2c->i2c_i2com |= 0x80;

  i=0;
  while( (txbd->status & TXBD_R) && (i<1000))
  {
    i++;
    udelay(1000);
  }

	/* some error msg */
	if ( txbd->status & TXBD_NAK )
	{
		printk("TXBD: NAK AT %02X LEN: %02X\n",address,size_to_expect);
		ret = -1;
	}
	if ( txbd->status & TXBD_UN )
	{
		printk("TXBD: UNDERRUN\n");
		ret = -1;
	}
	if ( txbd->status & TXBD_CO )
	{
		printk("TXBD: COLLISION\n");
		ret = -1;
	}
  if ( i==1000 )
	{
    printk("TXBD: TIMEOUT\n");
		ret = -1;
	}

	/* clear flags */
  txbd->status &= (~(TXBD_NAK|TXBD_UN|TXBD_CO));

	if ( ret != -1 )
	{
		i=0;
		while( (rxbd->status & RXBD_E) && (i<1000) )  /* Wait until receive is finished */
		{
			i++;
			udelay(1000);
		}

		/* some error msg */
	  if ( i==1000 )
		{
	    printk("RXBD: TIMEOUT\n");
			ret = -1;
		}
		if ( rxbd->length == 0  )
		{
			printk("RXBD: NO DATA\n");
			ret = -1;
		}
		if ( rxbd->status & RXBD_OV )
		{
			printk("RXBD: OVERRUN\n");
		 	rxbd->status &= (~RXBD_OV);
			ret = -1;
		}

	  if (ret != -1)
		{
	      for(i=0;i<rxbd->length;i++)
	        datain[i]=rxbuf[i];
		}
	}

  /* Turn off I2C */
  i2c->i2c_i2mod&=(~1);

  iip->iic_rstate=0;
  iip->iic_tstate=0;

  return ret;
}

///////////////////////////////////////////////////////////////////////////////

static int xfer_8xx(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num)
{
  struct i2c_msg *pmsg;
  int i;

  for (i=0; i<num; i++)
  {
    pmsg = &msgs[i];
//    printk("i2c: addr:=%x, flags:=%x, len:=%x\n", pmsg->addr, pmsg->flags, pmsg->len);
    if (pmsg->flags & I2C_M_RD )
    {
      if (i2c_receive(pmsg->addr<<1, pmsg->len, pmsg->buf)<0)
        return -EREMOTEIO;
    } else
    {
      if (i2c_send(pmsg->addr<<1, pmsg->len, pmsg->buf)<0)
        return -EREMOTEIO;
    }
  }
  return num;
}

///////////////////////////////////////////////////////////////////////////////

static int algo_control(struct i2c_adapter *adapter,
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

static u32 p8xx_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL; //  | I2C_FUNC_10BIT_ADDR;  10bit auch erstmal nicht.
}


///////////////////////////////////////////////////////////////////////////////

static struct i2c_algorithm i2c_8xx_algo = {
	"PowerPC 8xx Algo",
	I2C_ALGO_EXP,                   /* vorerst */
	xfer_8xx,
	NULL,
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	p8xx_func,			/* functionality	*/
};

///////////////////////////////////////////////////////////////////////////////

static struct i2c_adapter adap;

///////////////////////////////////////////////////////////////////////////////

static int __init i2c_algo_8xx_init (void)
{
  printk("i2c-8xx.o: mpc 8xx i2c init\n");
  i2c_init(100000);

  adap.id=i2c_8xx_algo.id;
  adap.algo=&i2c_8xx_algo;
  adap.timeout=100;
  adap.retries=3;

#ifdef MODULE
//  MOD_INC_USE_COUNT;
#endif

  printk("adapter: %x\n", i2c_add_adapter(&adap));
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

int i2c_8xx_del_bus(struct i2c_adapter *adap)
{
  int res;

  if ((res = i2c_del_adapter(adap)) < 0)
    return res;

  printk("i2c-8xx.o: adapter unregistered: %s\n",adap->name);

#ifdef MODULE
//  MOD_DEC_USE_COUNT;
#endif

  return 0;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>, gillem <XXX@XXX.XXX>");
MODULE_DESCRIPTION("I2C-Bus MPC8xx Intgrated I2C");

MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(i2c_debug,
            "debug level - 0 off; 1 normal; 2,3 more verbose; 9 bit-protocol");

int init_module(void)
{
  i2c_algo_8xx_init();
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

void cleanup_module(void)
{
  i2c_8xx_del_bus(&adap);
}
#endif
