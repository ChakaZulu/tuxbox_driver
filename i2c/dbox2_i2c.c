/*
 *   i2c-8xx.c - ppc i2c driver (dbox-II-project)
 *
 *   Copyright (C) 2000-2001 Tmbinc, Gillem (htoa@gmx.net)
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
 *   $Log: dbox2_i2c.c,v $
 *   Revision 1.13  2001/02/09 17:26:12  gillem
 *   global commproc.h
 *
 *   Revision 1.12  2001/01/06 10:06:01  gillem
 *   cvs check
 *
 *   $Revision: 1.13 $
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
#include <asm/hardirq.h>
#include <linux/i2c.h>

/* HACK HACK HACK */
#include <commproc.h>

/* parameter stuff */
static int debug = 0;

/* mutex stuff */
#define I2C_NONE			0
#define I2C_INT_EVENT 1

int i2c_event_mask = 0;
static DECLARE_MUTEX(i2c_mutex);

/* interrupt stuff */
static void i2c_interrupt(void*);
static wait_queue_head_t i2c_wait;

#define dprintk(fmt, args...) if (debug) printk( fmt, ## args )

#define TXBD_R 		0x8000  /* Transmit buffer ready to send */
#define TXBD_W 		0x2000  /* Wrap, last buffer in buffer circle */
#define TXBD_I 		0x1000
#define TXBD_L 		0x0800  /* Last, this buffer is the last in this frame */
                       		/* This bit causes the STOP condition to be sent */
#define TXBD_S 		0x0400  /* Start condition.  Causes this BD to transmit a start */

#define TXBD_NAK 	0x0004	/* indicates nak */
#define TXBD_UN  	0x0002  /* indicates underrun */
#define TXBD_CO  	0x0001  /* indicates collision */

#define RXBD_E 		0x8000  /* Receive buffer is empty and can be used by CPM */
#define RXBD_W 		0x2000  /* Wrap, last receive buffer in buffer circle */
#define RXBD_I 		0x1000  /* Int. */
#define RXBD_L 		0x0800  /* Last */

#define RXBD_OV 	0x0002	/* indicates overrun */

#define I2C_BUF_LEN      128

#define I2C_PPC_MASTER	 1
#define I2C_PPC_SLAVE		 0

#define MAXBD 4

typedef volatile struct I2C_BD
{
  unsigned short status;
  unsigned short length;
  unsigned char  *addr;
} I2C_BD;

typedef volatile struct RX_TX_BD {
	I2C_BD *rxbd;
	I2C_BD *txbd;
	unsigned char *rxbuf[MAXBD];
	unsigned char *txbuf[MAXBD];
	int rxnum;
	int txnum;
} RX_TX_BD;

static struct RX_TX_BD I2CBD;

static I2C_BD *rxbd, *txbd;

static unsigned char *rxbuf, *txbuf;

static	volatile i2c8xx_t	*i2c;
static	volatile iic_t		*iip;
static	volatile cpm8xx_t	*cp;

volatile cbd_t	*tbdf, *rbdf;

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

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

	dprintk("Best is:\n");
	dprintk("CPU=%dhz RATE=%d F=%d I2MOD=%08x I2BRG=%08x DIFF=%dhz\n", \
	    hz, speed, \
	    bestspeed_filter, bestspeed_modval, bestspeed_brgval, \
	    bestspeed_diff );

  i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3);
  i2c->i2c_i2brg = bestspeed_brgval & 0xff;

  dprintk("i2mod=%08x i2brg=%08x\n", i2c->i2c_i2mod, i2c->i2c_i2brg);

  return 1 ;
}

///////////////////////////////////////////////////////////////////////////////

int i2c_init(int speed)
{
	cpic8xx_t *cpic;
	int i;
	int ret = 0;

	/* get immap addr */
  immap_t *immap=(immap_t*)IMAP_ADDR;

	/* get cpm */
  cp = (cpm8xx_t *)&immap->im_cpm ;
	/* get i2c */
  iip = (iic_t *)&cp->cp_dparam[PROFF_IIC];
	/* get i2c ppc */
  i2c = (i2c8xx_t *)&(immap->im_i2c);

  cpic = (cpic8xx_t *)&(immap->im_cpic);

  /* disable relocation */
  iip->iic_rbase = 0 ;

  /* Initialize Port B I2C pins. */
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
  
  /* Set I2C controller in master mode */
  i2c->i2c_i2com = I2C_PPC_MASTER;

  // Set SDMA bus arbitration level to 5 (SDCR)
  immap->im_siu_conf.sc_sdcr = 0x0001 ;

  iip->iic_rbptr = iip->iic_rbase = m8xx_cpm_dpalloc (MAXBD*sizeof(I2C_BD)*2);
  iip->iic_tbptr = iip->iic_tbase = iip->iic_rbase + (MAXBD*sizeof(I2C_BD));

  I2CBD.rxbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_rbase]);
  I2CBD.txbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_tbase]);

  dprintk("RBASE = %04x\n", iip->iic_rbase);
  dprintk("TBASE = %04x\n", iip->iic_tbase);
  dprintk("RXBD1 = %08x\n", (int)I2CBD.rxbd);
  dprintk("TXBD1 = %08x\n", (int)I2CBD.txbd);

	for(i=0;i<MAXBD;i++)
	{
	  I2CBD.rxbuf[i] = (unsigned char*)m8xx_cpm_hostalloc(I2C_BUF_LEN);
	  I2CBD.txbuf[i] = (unsigned char*)m8xx_cpm_hostalloc(I2C_BUF_LEN);
	  I2CBD.rxbd[i].addr = (unsigned char*)__pa(I2CBD.rxbuf[i]);
	  I2CBD.txbd[i].addr = (unsigned char*)__pa(I2CBD.txbuf[i]);

	  I2CBD.rxbd[i].status = RXBD_E;
	  I2CBD.txbd[i].status = 0;
	  I2CBD.rxbd[i].length = 0;
	  I2CBD.txbd[i].length = 0;

	  dprintk("RXBD%d->ADDR = %08X\n",i,(uint)I2CBD.rxbd[i].addr);
	  dprintk("TXBD%d->ADDR = %08X\n",i,(uint)I2CBD.txbd[i].addr);
	}

  I2CBD.rxbd[i-1].status |= RXBD_W;
  I2CBD.txbd[i-1].status |= TXBD_W;

/*
	if (rxbuf==0)
	{
		dprintk("INIT ERROR: no RXBUF mem available\n");
		return -1;
	}

  dprintk("RXBD1->ADDR = %08X\n",(uint)rxbd->addr);

	if (txbuf==0)
	{
		dprintk("INIT ERROR: no TXBUF mem available\n");
		return -1;
	}

  dprintk("TXBD1->ADDR = %08X\n",(uint)txbd[0].addr);
  dprintk("TXBD2->ADDR = %08X\n",(uint)txbd[1].addr);
*/

  /* Set big endian byte order */
  iip->iic_tfcr = 0x15;
  iip->iic_rfcr = 0x15;

  /* Set maximum receive size. */
  iip->iic_mrblr = I2C_BUF_LEN;

// i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3);
  i2c->i2c_i2brg = 7;

	// Initialize the BD's
  while (cp->cp_cpcr & CPM_CR_FLG);
  cp->cp_cpcr = mk_cr_cmd(CPM_CR_CH_I2C, CPM_CR_INIT_TRX) | CPM_CR_FLG;
  while (cp->cp_cpcr & CPM_CR_FLG);

	// Clear events
  i2c->i2c_i2cer = 0xff ;

  i2c->i2c_i2cmr = 0x17;

	init_waitqueue_head(&i2c_wait);

	// Install Interrupt handler
	cpm_install_handler( CPMVEC_I2C, i2c_interrupt, (void*)iip );

	/* Enable i2c interrupt */
	cpic->cpic_cimr |= 0x10000;

//	cpic->cpic_cicr |= 0x8080;

	return ret;
}

///////////////////////////////////////////////////////////////////////////////

int i2c_send( unsigned char address,
							unsigned short size, unsigned char *dataout )
{
	unsigned long flags;
  int i,j,ret;

  if( size > I2C_BUF_LEN )  /* Trying to send message larger than BD */
    return -1;

  i=0;

  if(size==0)
	{
		dprintk("ZERO BYTE WRITE (SEND)\n");
   	size++;
	}

	ret = size;

  txbd[0].length = size + 1;  /* Length of message plus dest address */

  txbuf[0] = address;  /* Write destination address to BD */
  txbuf[0] &= ~(0x01);  /* Set address to write */
  i = 1;

  for(j=0; j<size; i++,j++)
    txbuf[i]=dataout[j];

  /* Ready to Transmit, wrap, last */
  txbd[0].status |= TXBD_R | TXBD_W | TXBD_I | TXBD_S | TXBD_L;

	save_flags(flags);cli();

  /* Transmit */
  i2c->i2c_i2com |= 0x80;

//  printk("TXBD: WAIT INT\n");

	interruptible_sleep_on_timeout(&i2c_wait,1000*1000);

	restore_flags(flags);

//  while( txbd->status & TXBD_R );               // todo: sleep with irq

//  printk("TXBD: READY INT\n");

	/* some error msg */
	if ( txbd[0].status & TXBD_NAK )
	{
		dprintk("TXBD: NAK AT %02X\n",address);
		ret = -1;
	}
	if ( txbd[0].status & TXBD_UN )
	{
		dprintk("TXBD: UNDERRUN\n");
		ret = -1;
	}
	if ( txbd[0].status & TXBD_CO )
	{
		dprintk("TXBD: COLLISION\n");
		ret = -1;
	}
  if ( i==1000 )
	{
    dprintk("TXBD: TIMEOUT\n");
		ret = -1;
	}

//  i2c->i2c_i2mod &= (~1);

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
	unsigned long flags;
  int i,ret;

  if( size_to_expect > I2C_BUF_LEN )
	{
		return -1;
	}

	ret = size_to_expect;

	for(i=0;i<size_to_expect;i++)
		txbuf[i+1] = datain[i];

	/* dummy write */
  txbuf[0] = address;
	txbuf[0]&=~(0x01);

  txbd[0].length = 2;
  txbd[0].status = TXBD_R | TXBD_S;
  txbd[0].status&= ~(TXBD_W|TXBD_I);

  /* Setup TXBD for destination address */
  txbuf[0+I2C_BUF_LEN] = (address | 0x01);

  txbd[1].length = 1 + size_to_expect;

	for(i=0;i<size_to_expect;i++)
		txbuf[i+1+I2C_BUF_LEN] = datain[i];

  if (!size_to_expect)
	{
		dprintk("ZERO BYTE WRITE (RECV)\n");
    txbd[1].length++;
		txbuf[1+I2C_BUF_LEN] = 0;
	}

  /* Reset the rxbd */
  rxbd->status |= RXBD_E | RXBD_W | RXBD_I;

  txbd[1].status = TXBD_R | TXBD_W | TXBD_S | TXBD_L; // | xflags;

	save_flags(flags);cli();

  i2c->i2c_i2cer = 0xff;

  /* Enable I2C */
  i2c->i2c_i2mod |= 1;

  /* Transmit */
  i2c->i2c_i2com |= 0x80;

//  printk("TXBD: WAIT INT\n");

	interruptible_sleep_on_timeout(&i2c_wait,1000*1000);

	restore_flags(flags);

//  udelay(1000);
/*
  i=0;
  while( (txbd->status & TXBD_R) && (i<1000))
  {
    i++;
  }
*/
	/* some error msg */
	if ( txbd->status & TXBD_NAK )
	{
		dprintk("TXBD: NAK AT %02X LEN: %02X\n",address,size_to_expect);
		ret = -1;
	}
	if ( txbd->status & TXBD_UN )
	{
		dprintk("TXBD: UNDERRUN\n");
		ret = -1;
	}
	if ( txbd->status & TXBD_CO )
	{
		dprintk("TXBD: COLLISION\n");
		ret = -1;
	}
  if ( i==1000 )
	{
    dprintk("TXBD: TIMEOUT\n");
		ret = -1;
	}

	/* clear flags */
  txbd->status &= (~(TXBD_NAK|TXBD_UN|TXBD_CO));

	if ( ret != -1 )
	{
		if ( (rxbd->status & RXBD_E) )
		{
	    dprintk("RXBD: RX DATA IS EMPTY\n");
			ret = -1;
		}
		if ( rxbd->length == 0  )
		{
			dprintk("RXBD: NO DATA\n");
			ret = -1;
		}
		if ( rxbd->status & RXBD_OV )
		{
			dprintk("RXBD: OVERRUN\n");
		 	rxbd->status &= (~RXBD_OV);
			ret = -1;
		}

	  if (ret != -1)
		{
	      for(i=0;i<rxbd->length;i++)
	        datain[i]=rxbuf[i];
		}
	}

  iip->iic_rstate=0;
  iip->iic_tstate=0;

  return ret;
}

///////////////////////////////////////////////////////////////////////////////

static void i2c_interrupt( void * dev_id )
{
        volatile iic_t *iip;
        volatile i2c8xx_t *i2c;

        i2c = (i2c8xx_t *)&(((immap_t *)IMAP_ADDR)->im_i2c);

        iip = (iic_t *)dev_id;

			  dprintk("[I2C INT]: MOD: %04X CER: %04X\n",i2c->i2c_i2mod,i2c->i2c_i2cer);

			  i2c->i2c_i2mod &= (~1);

        /* Clear interrupt. */
        i2c->i2c_i2cer = 0xff;

        /* Get 'me going again. */
        wake_up_interruptible( &i2c_wait );
}

///////////////////////////////////////////////////////////////////////////////

static int parse_send_msg( unsigned char address, unsigned short size,
													 unsigned char *dataout, int last )
{
	int i,j;

  if( size > I2C_BUF_LEN )  /* Trying to send message larger than BD */
    return -1;

  if(size==0)
	{
		dprintk("ZERO BYTE WRITE (SEND)\n");
   	size++;
	}

  I2CBD.txbd[I2CBD.txnum].length = size + 1;  /* Length of message plus dest address */

  I2CBD.txbuf[I2CBD.txnum][0] = (address&(~1));  /* Write destination address to BD */
  i = 1;

  for(j=0; j<size; i++,j++)
    I2CBD.txbuf[I2CBD.txnum][i]=dataout[j];

	I2CBD.txbd[I2CBD.txnum].status = TXBD_R | TXBD_S/*| TXBD_I*/ /*| TXBD_W*/;

	if(last)
	{
		dprintk("LAST SEND\n");
		I2CBD.txbd[I2CBD.txnum].status |= TXBD_L | TXBD_I | TXBD_W;
	}

	I2CBD.txnum++;

	dprintk("PARSE SEND MSG OK\n");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

static int parse_recv_msg( unsigned char address, unsigned short size,
													 unsigned char *datain, int last )
{
  if( size > I2C_BUF_LEN )  /* Trying to send message larger than BD */
    return -1;

  if(size==0)
	{
		dprintk("ZERO BYTE WRITE (RECV)\n");
   	size++;
	}

  I2CBD.txbuf[I2CBD.txnum][0] = (address | 0x01);
  I2CBD.txbuf[I2CBD.txnum][1] = 0;

  I2CBD.txbd[I2CBD.txnum].length = 1 + size;
  I2CBD.rxbd[I2CBD.rxnum].length = 0;

	I2CBD.txbd[I2CBD.txnum].status = TXBD_R | TXBD_S/*| TXBD_W*/ /*| TXBD_I*/;
	I2CBD.rxbd[I2CBD.txnum].status = RXBD_E | RXBD_I;

	if (last)
	{
		dprintk("LAST RECV\n");
		I2CBD.rxbd[I2CBD.rxnum].status |= RXBD_I | RXBD_W;
		I2CBD.txbd[I2CBD.txnum].status |= TXBD_L | TXBD_W;
	}

	I2CBD.rxnum++;
	I2CBD.txnum++;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

static int xfer_8xx(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num)
{
	unsigned long flags;
  struct i2c_msg *pmsg;
  int i,last,ret;

	down(&i2c_mutex);

	ret = num;

	if ( num > (MAXBD*2) )
	{
		up(&i2c_mutex);
	  return -EREMOTEIO;
	}

	for(i=0;i<MAXBD;i++)
	{
	  I2CBD.rxbd[i].status = RXBD_E;
	  I2CBD.txbd[i].status = 0;
	  I2CBD.rxbd[i].length = 0;
	  I2CBD.txbd[i].length = 0;
	}

  I2CBD.rxbd[i-1].status |= RXBD_W;
  I2CBD.txbd[i-1].status |= TXBD_W;

	/* reset buffer pointer */
	I2CBD.rxnum = 0;
	I2CBD.txnum = 0;

  iip->iic_rstate=0;
  iip->iic_tstate=0;

	last = 0;

	/* parse msg's */
  for (i=0; i<num; i++)
  {
    pmsg = &msgs[i];

		if(i==(num-1))
			last++;

    dprintk("i2c: addr:=%x, flags:=%x, len:=%x num:=%d\n", pmsg->addr, pmsg->flags, pmsg->len, num);

	  I2CBD.rxbd[I2CBD.rxnum].length = 0;
	  I2CBD.txbd[I2CBD.txnum].length = 0;

    if (pmsg->flags & I2C_M_RD )
    {
      if (parse_recv_msg( pmsg->addr<<1, pmsg->len, pmsg->buf, last )<0)
			{
				up(&i2c_mutex);
        return -EREMOTEIO;
			}
    } else
    {
      if (parse_send_msg(pmsg->addr<<1, pmsg->len, pmsg->buf, last )<0)
			{
				up(&i2c_mutex);
        return -EREMOTEIO;
			}
    }
  }

	if ( !in_interrupt() )
		save_flags(flags);cli();

  i2c->i2c_i2cer = 0xff;

  /* Enable I2C */
  i2c->i2c_i2mod |= 1;

  /* Transmit */
  i2c->i2c_i2com |= 0x80;

	if ( in_interrupt() )
	{
		udelay(1000*10);
	  dprintk("[I2C INT]: MOD: %04X CER: %04X\n",i2c->i2c_i2mod,i2c->i2c_i2cer);
	}
	else
	{
		i=interruptible_sleep_on_timeout(&i2c_wait,1000);
		restore_flags(flags);
	}

	/* copy rx-buffer */
	I2CBD.rxnum = 0;
	I2CBD.txnum = 0;

	for (i=0; i<num; i++)
  {
    pmsg = &msgs[i];

    if ( pmsg->flags & I2C_M_RD )
    {
			if ( (I2CBD.rxbd[I2CBD.rxnum].status & RXBD_E) )
			{
		    dprintk("RXBD: RX DATA IS EMPTY\n");
				ret = -EREMOTEIO;
			}
			else if ( I2CBD.rxbd[I2CBD.rxnum].length == 0  )
			{
				dprintk("RXBD: NO DATA\n");
				ret = -EREMOTEIO;
			}
			if ( I2CBD.rxbd[I2CBD.rxnum].status & RXBD_OV )
			{
				dprintk("RXBD: OVERRUN\n");
			 	rxbd->status &= (~RXBD_OV);
				ret = -EREMOTEIO;
			}

			memcpy( pmsg->buf, I2CBD.rxbuf[I2CBD.rxnum], I2CBD.rxbd[I2CBD.rxnum].length );
			I2CBD.rxnum++;
		}
		else
		{
			if ( I2CBD.txbd[I2CBD.txnum].status & TXBD_NAK )
			{
				dprintk("TXBD: NAK\n");
				ret = -EREMOTEIO;
			}
			if ( I2CBD.txbd[I2CBD.txnum].status & TXBD_UN )
			{
				dprintk("TXBD: UNDERRUN\n");
				ret = -EREMOTEIO;
			}
			if ( I2CBD.txbd[I2CBD.txnum].status & TXBD_CO )
			{
				dprintk("TXBD: COLLISION\n");
				ret = -EREMOTEIO;
			}

			/* clear flags */
			I2CBD.txbd[I2CBD.rxnum].status &= (~(TXBD_NAK|TXBD_UN|TXBD_CO));

			I2CBD.txnum++;
		}
	}

  /* Turn off I2C */
  i2c->i2c_i2mod&=(~1);

	up(&i2c_mutex);

  return ret;
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

//////////////////////////////////////////////////////////////////////////////
static int __init i2c_algo_8xx_init (void)
{
  printk("i2c-8xx.o: mpc 8xx i2c init\n");

  if ( i2c_init(100000) < 0 )
	{
	  printk("i2c-8xx.o: init failed\n");
		return -1;
	}

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

MODULE_PARM(debug,"i");
MODULE_PARM_DESC(debug,
            "debug level - 0 off; 1 on");

int init_module(void)
{
  return i2c_algo_8xx_init();
}

///////////////////////////////////////////////////////////////////////////////

void cleanup_module(void)
{
  i2c_8xx_del_bus(&adap);
}
#endif
