//
//  mmccombo.c v 1.x
//  MMC device driver for all dBox2, 3 different wiring schemes selectable
//
//  optional argument wiringopt defines the wiring scheme used! - defaults to 0 = autodetection
//
//          =1    =2    =4
//  SD_DO   PA7   PA9   PB18
//  SD_DI   PB19  PA8   PB19
//  SD_CLK  PB22  PB17  PB17
//  SD_CS   PB23  PB16  PB16
//
//  Optional parameter opt defaults to 1. Uses failsafe low-level IO if set to 0
//  Optional parameter forcehw, if > 0, hardware will be aasigned even if otherwise used
//                     this is needed for mmc2 wiring, where tts/1 (smc2) uses PA8, PA9
//
//  e.g. use: insmod mmccombo.o wiringopt=1 opt=1 forcehw=0
//  to load for wiring schematic 1 with optimizations using hardware only if available
//
//  This version of MMC brings the optimization of mmc2 driver to the earlier
//  mmc and a new connection scheme and makes low-level write about 1/3 faster
//  on wiring model 1 and 4.
//
//  09 May 2007
// 
// initially written by Madsuk/Rohde/TaGana
// optimized by just_me/Guenther/DboxBar
// new connection scheme PB[16:19], further write optimization, combined driver ++ by satsuse
//
// to do:
// - a lot! - I would consider this still to be experimental
//
/*
$Log: mmccombo.c,v $
Revision 1.2  2007/05/21 08:37:28  satsuse
faster low-level write for wo={1, 4}

Revision 1.1  2007/05/20 08:55:14  satsuse
combined mmc driver

*/

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>


#define DEVICE_NAME "mmc"
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define MAJOR_NR 121

#include <linux/blk.h>


//#define CHECK_MEDIA_CHANGE  // for developement ONLY, not working yet

#define MMCPA07 0x0100 // SD_DO  on WO=1
#define MMCPA08 0x0080 // SD_DI  on WO=2
#define MMCPA09 0x0040 // SD_DO  on WO=2
#define MMCPB16 0x8000 // SD_CS  on WO=2,4
#define MMCPB17 0x4000 // SD_CLK on WO=2,4
#define MMCPB18 0x2000 // SD_DO  on WO=4
#define MMCPB19 0x1000 // SD_DI  on WO=1,4
#define MMCPB22 0x0200 // SD_CLK on WO=1
#define MMCPB23 0x0100 // SD_CS  on WO=1


MODULE_AUTHOR("Madsuk/Rohde/TaGana");
MODULE_DESCRIPTION("Driver MMC/SD-Cards");
MODULE_SUPPORTED_DEVICE("All dbox2 in different configurations, see README");
MODULE_LICENSE("GPL");
MODULE_PARM(wiringopt, "b");
MODULE_PARM_DESC(wiringopt, "Wiring option for mmc connection 1-4");
MODULE_PARM(opt, "b");
MODULE_PARM_DESC(opt, "0 = use optimized IO, defaults to 1");
MODULE_PARM(forcehw, "b");
MODULE_PARM_DESC(forcehw, "> 0 do force hardware allocation even if otherwise assigned");

// options on module call
static char wiringopt = 0;
static char opt = 1;
static char forcehw = 0;
void          (*pntwri)    (unsigned char *, unsigned short); // write byte
unsigned char (*pntio)     (unsigned char);                   // read/write byte
int           (*pntinit)   (void);                            // init hardware
void          (*pntcslow)  (void);                            // not chip select
void          (*pntcshigh) (void);                            // chip select
void          (*pntread)   (unsigned char *, unsigned short); // read multiple

// hardware handling
static char actualhard = 0;

struct portsettings {
	unsigned int papar;	// portA assignment bits to be reset on exit
	unsigned int paodr;	// portA open drain bits to be reset on exit
	unsigned int padi0;	// portA direction bits to be cleared on exit
	unsigned int padi1;	// portA direction bits to be reset on exit
	unsigned int pada0;	// portA data bits to be cleared on exit
	unsigned int pada1;	// portA data bits to be reset on exit

	unsigned int pbpar;	// portB assignment bits to be reset on exit
	unsigned int pbodr;	// portB open drain bits to be reset on exit
	unsigned int pbdi0;	// portB direction bits to be cleared on exit
	unsigned int pbdi1;	// portB direction bits to be reset on exit
	unsigned int pbda0;	// portB data bits to be cleared on exit
	unsigned int pbda1;	// portB data bits to be reset on exit
} myports;			// space to save hardware settings


volatile immap_t *immap=(immap_t *)IMAP_ADDR ;

/* we have only one device */
static int hd_sizes[1<<6];
static int hd_blocksizes[1<<6];
static int hd_hardsectsizes[1<<6];
static int hd_maxsect[1<<6];
static struct hd_struct hd[1<<6];

static struct timer_list mmc_timer;
static int mmc_media_detect = 0;
static int mmc_media_changed = 1;

extern struct gendisk hd_gendisk;

/////////////////////
// prototypes
static int mmc_open(struct inode *inode, struct file *filp);
static int mmc_release(struct inode *inode, struct file *filp);
static int mmc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
static void mmc_request(request_queue_t *q);
static void __exit mmc_driver_exit(void);

static struct block_device_operations mmc_bdops =
{
	open: mmc_open,
	release: mmc_release,
	ioctl: mmc_ioctl,
#ifdef CHECK_MEDIA_CHANGE
	check_media_change: mmc_check_media_change,
	revalidate: mmc_revalidate,
#endif
};

static struct gendisk hd_gendisk = {
	major:		MAJOR_NR,
	major_name:	DEVICE_NAME,
	minor_shift:	6,
	max_p:		1 << 6,
	part:		hd,
	sizes:		hd_sizes,
	fops:		&mmc_bdops,
};

///////////////////////////////////////////////////////////////////////////////////////////////
/////// Low-Level Driver
///////////////////////////////////////////////////////////////////////////////////////////////

static void mmc_hardware_cleanup(void) {
	volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
	volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

	// usefulness check
	if (actualhard == 0) {
		printk(KERN_INFO "mmc: Hardware cleanup - nothing to clean\n");
		return;	
	}

	// if we stole something, try to give it back ;)
	// set to 1 what was cleared by us (but 1 before) and vice versa
	cpi->iop_papar |= myports.papar;	// reset assignment bits
	 cp->cp_pbpar  |= myports.pbpar;
	cpi->iop_paodr |= myports.paodr;	// reset open drain bits
	 cp->cp_pbodr  |= myports.pbodr;
	cpi->iop_padir &= myports.padi0;	// reclear direction bits
	 cp->cp_pbdir  &= myports.pbdi0;
	cpi->iop_padir |= myports.padi1;	// reset direction bits
	 cp->cp_pbdir  |= myports.pbdi1;
	cpi->iop_padat &= myports.pada0;	// reclear data bits
	 cp->cp_pbdat  &= myports.pbda0;
	cpi->iop_padat |= myports.pada1;	// reset data bits
	 cp->cp_pbdat  |= myports.pbda1;
	actualhard = 0;
	printk(KERN_INFO "mmc: Hardware cleanup - done\n");
}


static int mmc_hardware_init(void) {
	volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
	volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
	int rc;

	if (actualhard == wiringopt) {
		// nothing to set
		printk(KERN_INFO "mmc: hardware already set\n");
		return 0;
	}

	// restore old settings, if any saved
	mmc_hardware_cleanup();

	// DI, CLK and CS out - DO in - all assigned to GPIO - none open drain
	switch (wiringopt)
	{
		case 1:
		rc = (cpi->iop_papar &  MMCPA07) | (cp->cp_pbpar  & (MMCPB22 | MMCPB23 | MMCPB19));

		if ((forcehw > 0) || (rc == 0)) {
			myports.papar =    cpi->iop_papar &  MMCPA07;
			myports.pbpar =     cp->cp_pbpar  & (MMCPB22 | MMCPB23 | MMCPB19);
			myports.paodr =    cpi->iop_paodr &  MMCPA07;
			myports.pbodr =     cp->cp_pbodr  & (MMCPB22 | MMCPB23 | MMCPB19);
			myports.padi1 =    cpi->iop_padir &  MMCPA07;
			myports.pbdi1 = 0;
			myports.padi0 = ~(~cpi->iop_padir &  0);
			myports.pbdi0 =  ~(~cp->cp_pbdir  & (MMCPB22 | MMCPB23 | MMCPB19));
			myports.pada1 = 0;
			myports.pbda1 =     cp->cp_pbdat  & (MMCPB22 | MMCPB23 | MMCPB19);
			myports.pada0 = ~(~cpi->iop_padat &  0);
			myports.pbda0 =  ~(~cp->cp_pbdat  & (MMCPB22 | MMCPB23 | MMCPB19));

			cp->cp_pbpar &=   ~(MMCPB22 | MMCPB23 | MMCPB19);
			cp->cp_pbodr &=   ~(MMCPB22 | MMCPB23 | MMCPB19);
			cp->cp_pbdir |=    (MMCPB22 | MMCPB23 | MMCPB19);

			cpi->iop_papar &= ~MMCPA07;
			cpi->iop_paodr &= ~MMCPA07;
			cpi->iop_padir &= ~MMCPA07;
			// Clock low
			cp->cp_pbdat &= ~MMCPB22;
			// DI + CS high
			cp->cp_pbdat |= (MMCPB19 | MMCPB23);
		}
		break;

		case 2:
		rc = (cpi->iop_papar & (MMCPA09 | MMCPA08)) | (cp->cp_pbpar  & (MMCPB17 | MMCPB16));

		if ((forcehw > 0) || (rc == 0)) {
			myports.papar =    cpi->iop_papar & (MMCPA09 | MMCPA08);
			myports.pbpar =     cp->cp_pbpar  & (MMCPB17 | MMCPB16);
			myports.paodr =    cpi->iop_paodr & (MMCPA09 | MMCPA08);
			myports.pbodr =     cp->cp_pbodr  & (MMCPB17 | MMCPB16);
			myports.padi1 =    cpi->iop_padir &  MMCPA09;
			myports.pbdi1 = 0;
			myports.padi0 = ~(~cpi->iop_padir &  MMCPA08);
			myports.pbdi0 =  ~(~cp->cp_pbdir  & (MMCPB17 | MMCPB16));
			myports.pada1 =    cpi->iop_padat &  MMCPA08;
			myports.pbda1 =     cp->cp_pbdat  & (MMCPB17 | MMCPB16);
			myports.pada0 = ~(~cpi->iop_padat &  MMCPA08);
			myports.pbda0 =  ~(~cp->cp_pbdat  & (MMCPB16 | MMCPB17));

			cp->cp_pbpar &=   ~(MMCPB17 | MMCPB16);
			cp->cp_pbodr &=   ~(MMCPB17 | MMCPB16);
			cp->cp_pbdir |=    (MMCPB17 | MMCPB16);

			cpi->iop_papar &= ~(MMCPA09 | MMCPA08);
			cpi->iop_paodr &= ~(MMCPA09 | MMCPA08);
			cpi->iop_padir |=   MMCPA08;
			cpi->iop_padir &=  ~MMCPA09;
			// Clock low
			cp->cp_pbdat &= ~MMCPB17;
			// DI + CS high
			cpi->iop_padat |= MMCPA08;
			cp->cp_pbdat |= MMCPB16;
		}
		break;

		case 4:
		rc = cp->cp_pbpar  & (MMCPB17 | MMCPB16 | MMCPB18 | MMCPB19);

		if ((forcehw > 0) || (rc == 0)) {
			myports.papar =    cpi->iop_papar &  0;
			myports.pbpar =     cp->cp_pbpar  & (MMCPB17 | MMCPB16 | MMCPB18 | MMCPB19);
			myports.paodr =    cpi->iop_paodr &  0;
			myports.pbodr =     cp->cp_pbodr  & (MMCPB17 | MMCPB16 | MMCPB18 | MMCPB19);
			myports.padi1 =    cpi->iop_padir &  0;
			myports.pbdi1 = 0;
			myports.padi0 = ~(~cpi->iop_padir &  0);
			myports.pbdi0 =  ~(~cp->cp_pbdir  & (MMCPB17 | MMCPB16 | MMCPB19));
			myports.pada1 = 0;
			myports.pbda1 =     cp->cp_pbdat  & (MMCPB17 | MMCPB16 | MMCPB19);
			myports.pada0 = ~(~cpi->iop_padat &  0);
			myports.pbda0 =  ~(~cp->cp_pbdat  & (MMCPB16 | MMCPB17 | MMCPB19));

			cp->cp_pbpar &=   ~(MMCPB17 | MMCPB16 | MMCPB18 | MMCPB19);
			cp->cp_pbodr &=   ~(MMCPB17 | MMCPB16 | MMCPB18 | MMCPB19);
			cp->cp_pbdir |=    (MMCPB17 | MMCPB16 | MMCPB19);
			cp->cp_pbdir &=   ~MMCPB18;
			// Clock low 
			cp->cp_pbdat &= ~MMCPB17;
			// DI + CS high
			cp->cp_pbdat |= (MMCPB16 | MMCPB19);
		}
	}

	printk(KERN_INFO "mmc: Hardware init for wiringopt = %X - ", wiringopt); 
	if (rc == 0) printk("OK\n");
	else
		if (forcehw > 0) {
			printk("FORCED! But A:%X B:%X\n", myports.papar, myports.pbpar);
			rc = 0;
		}
		else	
			printk("failed! A:%X B:%X\n", myports.papar, myports.pbpar);

	if (rc == 0) actualhard = wiringopt;

	return rc;
}

static void mmc_spi_cs_low_PB23(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat &= ~(MMCPB23);
}

static void mmc_spi_cs_low_PB16(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat &= ~(MMCPB16);
}

static void mmc_spi_cs_high_PB23(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat |= MMCPB23;
}

static void mmc_spi_cs_high_PB16(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat |= MMCPB16;
}

static unsigned char mmc_spi_io_1(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned char result = 0;
  unsigned char  i;

  // establish data-out
  for(i = 0x80; i != 0; i >>= 1) {
    if (data_out & i)
      cp->cp_pbdat |= MMCPB19;
    else
      cp->cp_pbdat &= ~MMCPB19;

    // latch data into card
    cp->cp_pbdat |= MMCPB22;
    // read data out from card
    if (cpi->iop_padat & MMCPA07) {
    	result |= i;
    }
    cp->cp_pbdat &= ~MMCPB22;
  }

  return result;
}

static unsigned char mmc_spi_io_2(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned char result = 0;
  unsigned char  i;

  for(i = 0x80; i != 0; i >>= 1) {
    if (data_out & i)
      cpi->iop_padat |= MMCPA08;
    else
      cpi->iop_padat &= ~MMCPA08;

    // latch data on SD_DI into card
    cp->cp_pbdat |= MMCPB17;
    // read data from cards SD_DO
    if (cpi->iop_padat & MMCPA09) {
    	result |= i;
    }
    cp->cp_pbdat &= ~MMCPB17;
  }

  return result;
}

static unsigned char mmc_spi_io_4(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  unsigned char result = 0;
  unsigned char  i;

  for(i = 0x80; i != 0; i >>= 1) {
    if (data_out & i)
      cp->cp_pbdat |= MMCPB19;
    else
      cp->cp_pbdat &= ~MMCPB19;

    cp->cp_pbdat |= MMCPB17;
    if (cp->cp_pbdat & MMCPB18) {
    	result |= i;
    }
    cp->cp_pbdat &= ~MMCPB17;
  }

  return result;
}


//////////////////////////////////
// optimized read/write functions

static void mmc_spi_write_14(unsigned char *data_out, unsigned short charcnt) {
	// point cpm in memorymap
	volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;		
	
	unsigned int cldl;	
	unsigned int cldh;	
	unsigned int chdl; 	
	unsigned int chdh; 	
	unsigned int byte_out;

	if (wiringopt == 1) {
		cldl = cp->cp_pbdat & ~(MMCPB19 | MMCPB22);  // setup data: clock low data low
		cldh = (cp->cp_pbdat | MMCPB19) & ~MMCPB22 ; // setup data : clock low data high
		chdl = (cp->cp_pbdat | MMCPB22) & ~MMCPB19;  // latch : clock high data low
		chdh = cp->cp_pbdat | MMCPB22 | MMCPB19;     // latch : clock high data high
	} else {
		// must be 4
		cldl = cp->cp_pbdat & ~(MMCPB19 | MMCPB17);
		cldh = (cp->cp_pbdat | MMCPB19) & ~MMCPB17 ;
		chdl = (cp->cp_pbdat | MMCPB17) & ~MMCPB19;
		chdh = cp->cp_pbdat | MMCPB17 | MMCPB19;
	}


	while(charcnt > 0)
	{
		byte_out = *data_out++;
		// needs 1 clock less, if done here and not directly in the while statement
		charcnt--;			

		// bit 7
		if (byte_out & 0x80) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 6
		if (byte_out & 0x40) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 5
		if (byte_out & 0x20) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 4
		if (byte_out & 0x10) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 3
		if (byte_out & 0x08) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 2
		if (byte_out & 0x04) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
	 		cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 1
		if (byte_out & 0x02) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

		// bit 0
		if (byte_out & 0x01) {
			wmb();
			cp->cp_pbdat = cldh;	// set data-high
			wmb();
 			cp->cp_pbdat = chdh;	// latch
		}     
		else {
			wmb();
			cp->cp_pbdat = cldl;	// set data-low
			wmb();
			cp->cp_pbdat = chdl;	// latch
		}

	}

	// cleanup bus
	cp->cp_pbdat = cldh;
}

static void mmc_spi_write_2(unsigned char *data_out, unsigned short charcnt) {
	volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
	volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
	unsigned long clk_high = cp->cp_pbdat | MMCPB17;
	unsigned long clk_low =  cp->cp_pbdat & ~MMCPB17;

	unsigned long data_high = cpi->iop_padat | MMCPA08;
	unsigned long data_low =  cpi->iop_padat & ~MMCPA08;

	unsigned char *pntend = data_out + charcnt;

	for(; data_out < pntend; data_out++)
	{
		if (*data_out & 0x80)	cpi->iop_padat = data_high;// 1 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x40)	cpi->iop_padat = data_high;// 2 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x20)	cpi->iop_padat = data_high;// 3 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x10)	cpi->iop_padat = data_high;// 4 bit
		else			cpi->iop_padat= data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x08)	cpi->iop_padat = data_high;// 5 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x04)	cpi->iop_padat = data_high;// 6 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x02)	cpi->iop_padat = data_high;// 7 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
		if (*data_out & 0x01)	cpi->iop_padat = data_high;// 8 bit
		else			cpi->iop_padat = data_low;
		cp->cp_pbdat = clk_high;
		cp->cp_pbdat = clk_low;
	}
}

////////////////////////////////////////
// burst transfer quite more optimized (approx 10% faster than above)

void mmc_read_multiple_1(unsigned char *dest, unsigned short charcnt)
{
   volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
   volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

   // load clock port into register ram and predefine with pin low/high,
   // to avoid reload on any clock trigger, this speeds up the transmission significantly.
   // Keep dataout high (we only send 1s)
   unsigned long clk_high = cp->cp_pbdat | MMCPB22 | MMCPB19;   
   unsigned long clk_low =  (cp->cp_pbdat | MMCPB19) & ~MMCPB22;

   unsigned char result=0;

   // DI high, send 1s (set high, before next clock l->h tansition)
   cp->cp_pbdat |= MMCPB19;

   for (; charcnt > 0; charcnt--)
   {
      cp->cp_pbdat = clk_high; // Bit 0
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 1
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 2
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 3
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 4
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 5
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 6
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 7
      result <<= 1;result |= ((cpi->iop_padat & MMCPA07)?0x01:0x00);
      cp->cp_pbdat = clk_low;

      *dest++ = result;       
      result = 0;
   }
}

void mmc_read_multiple_2(unsigned char *dest, unsigned short charcnt)
{
   volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
   volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

   unsigned long clk_high = cp->cp_pbdat | MMCPB17;
   unsigned long clk_low =  cp->cp_pbdat & ~MMCPB17;

   // DI high, send 1s
   cpi->iop_padat |= MMCPA08;

   unsigned char result=0;

   for (; charcnt > 0; charcnt--)
   {
      cp->cp_pbdat = clk_high; // Bit 0 (MSB)
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 1
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 2
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 3 
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 4
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 5
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 6
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 7
      result <<= 1;result |= ((cpi->iop_padat & MMCPA09)?0x01:0x00);
      cp->cp_pbdat = clk_low;

      *dest++ = result;       
      result = 0;
   }
}

void mmc_read_multiple_4(unsigned char *dest, unsigned short charcnt)
{
   volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;

   unsigned long clk_high = cp->cp_pbdat | MMCPB17 | MMCPB19;
   unsigned long clk_low =  (cp->cp_pbdat | MMCPB19) & ~MMCPB17;

   unsigned char result=0;

   // DI high, send 1s (set high, before next clock l->h tansition)
   cp->cp_pbdat |= MMCPB19;

   for (; charcnt > 0; charcnt--)
   {
      cp->cp_pbdat = clk_high; // Bit 0 (MSB)
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 1
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 2
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 3 
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 4
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 5
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 6
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;
      cp->cp_pbdat = clk_high; // Bit 7
      result <<= 1;result |= ((cp->cp_pbdat & MMCPB18)?0x01:0x00);
      cp->cp_pbdat = clk_low;

      *dest++ = result;       
      result = 0;
   }
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////// High-Level Driver
///////////////////////////////////////////////////////////////////////////////////////////////

static int mmc_write_block(unsigned int dest_addr, unsigned char *data)
{
	static int i;
	static unsigned char mmcbuf[8];
	static unsigned char r;

	if (opt == 0)
	{
		// secure low-level, aka non optimized

		(*pntcslow)();
		for (i = 0; i < 4; i++) (*pntio)(0xff);
		(*pntio)(0x58);
		(*pntio)(0xff & (dest_addr >> 24)); // msb
		(*pntio)(0xff & (dest_addr >> 16));
		(*pntio)(0xff & (dest_addr >> 8));
		(*pntio)(0xff & dest_addr); // lsb
		(*pntio)(0xff);
		for (i = 0; i < 8; i++)
		{
			r = (*pntio)(0xff);
			if (r == 0x00) break;
		}
		if (r != 0x00)
		{
			(*pntcshigh)();
			(*pntio)(0xff);
			return(1);
		}

		(*pntio)(0xfe);
		for (i = 0; i < 512; i++) (*pntio)(data[i]);
		for (i = 0; i < 2; i++) (*pntio)(0xff);

		for (i = 0; i < 1000000; i++)
		{
			r = (*pntio)(0xff);
			if (r == 0xff) break;
		}
		if (r != 0xff)
		{
			(*pntcshigh)();
			(*pntio)(0xff);
			return(3);
		}
		(*pntcshigh)();
		(*pntio)(0xff);
	}
	else
	{
		(*pntcslow)();
		(*pntread)(mmcbuf, 4);

		mmcbuf[0] = 0x58;
		mmcbuf[4] = 0xFF & dest_addr;
		mmcbuf[3] = 0xFF & (dest_addr >>= 8);
		mmcbuf[2] = 0xFF & (dest_addr >>= 8);
		mmcbuf[1] = 0xFF & (dest_addr >>= 8);
		mmcbuf[5] = 0xFF;
		mmcbuf[6] = 0xFF;
		mmcbuf[7] = 0xFE;

		(*pntwri)(mmcbuf, 6);

		for (i = 0; i < 8; i++)
		{
		(*pntread)(mmcbuf, 1);
		if (*mmcbuf == 0x00) break;
		}
		if (*mmcbuf != 0x00)
		{
			(*pntcshigh)();
			(*pntread)(mmcbuf, 1);
			return(1);
		}
	
		(*pntwri)(mmcbuf + 7, 1);

		// mmc_write_block_low_level(data);
		(*pntwri)(data, 512);
		// send CRC for nothing
		(*pntwri)(mmcbuf + 5, 2);

		for (i = 0; i < 1000000; i++)
		{
			(*pntread)(mmcbuf, 1);
			if (*mmcbuf == 0xff) break;
		}
		if (*mmcbuf != 0xff)
		{
			(*pntcshigh)();
			(*pntread)(mmcbuf, 1);
			return(3);
		}
		(*pntcshigh)();
		(*pntread)(mmcbuf, 1);
	}
	return(0);
}

static int mmc_read_block(unsigned char *data, unsigned int src_addr)
{
	static int i;
	static unsigned char mmcbuf[5];
	static unsigned char r = 0;

	if (opt == 0)
	{
		// secure low-level, aka non optimized

		(*pntcslow)();
		for (i = 0; i < 4; i++) (*pntio)(0xff);
		(*pntio)(0x51);
		(*pntio)(0xff & (src_addr >> 24)); // msb 
		(*pntio)(0xff & (src_addr >> 16));
		(*pntio)(0xff & (src_addr >> 8));
		(*pntio)(0xff & src_addr); // lsb

		(*pntio)(0xff);
		for (i = 0; i < 8; i++)
		{
			r = (*pntio)(0xff);
			if (r == 0x00) break;
		}
		if (r != 0x00)
		{
			(*pntcshigh)();
			(*pntio)(0xff);
			return(1);
		}
		for (i = 0; i < 100000; i++)
		{
			r = (*pntio)(0xff);
			if (r == 0xfe) break;
		}
		if (r != 0xfe)
		{
			(*pntcshigh)();
			(*pntio)(0xff);
			return(2);
		}
		for (i = 0; i < 512; i++)
		{
			r = (*pntio)(0xff);
			data[i] = r;
		}
		for (i = 0; i < 2; i++)
		{
			r = (*pntio)(0xff);
		}
		(*pntcshigh)();
		(*pntio)(0xff);
	}
	else
	{
		(*pntcslow)();
		(*pntread)(mmcbuf, 4);

		mmcbuf[0] = 0x51;
		mmcbuf[4] = 0xFF & src_addr;
		mmcbuf[3] = 0xFF & (src_addr >>= 8);
		mmcbuf[2] = 0xFF & (src_addr >>= 8);
		mmcbuf[1] = 0xFF & (src_addr >>= 8);
	
		(*pntwri)(mmcbuf, 5);

		(*pntread)(mmcbuf, 1);
		for (i = 0; i < 8; i++)
		{
			(*pntread)(mmcbuf, 1);
			if (*mmcbuf == 0x00) break;
		}
		if (*mmcbuf != 0x00)
		{
			(*pntcshigh)();
			(*pntread)(mmcbuf, 1);
			return(1);
		}
		for (i = 0; i < 100000; i++)
		{
			(*pntread)(mmcbuf, 1);
			if (*mmcbuf == 0xfe) break;
		}
		if (*mmcbuf != 0xfe)
		{
			(*pntcshigh)();
			(*pntread)(mmcbuf, 1);
			return(2);
		}
		(*pntread)(data, 512);

		(*pntread)(mmcbuf, 2); // just read the CRC for nothing

		(*pntcshigh)();
		(*pntread)(mmcbuf, 1);
	}
	return(0);
}

static int mmc_card_init(unsigned char silent)
{
	unsigned char r = 0;
	short i, j;
	unsigned long flags;

	save_flags(flags);
	cli();

	if (silent == 1) printk(KERN_INFO "mmc: Card init\n");
	(*pntcshigh)();
	for (i = 0; i < 1000; i++) (*pntio)(0xff);

	(*pntcslow)();

	(*pntio)(0x40);
	for (i = 0; i < 4; i++) (*pntio)(0x00);
	(*pntio)(0x95);
	for (i = 0; i < 8; i++)
	{
		r = (*pntio)(0xff);
		if (r == 0x01) break;
	}
	(*pntcshigh)();
	(*pntio)(0xff);
	if (r != 0x01)
	{
		restore_flags(flags);
		return(1);
	}

	if (silent == 1) printk(KERN_INFO "mmc: Card init *OK1*\n");
	for (j = 0; j < 30000; j++)
	{
		(*pntcslow)();

		(*pntio)(0x41);
		for (i = 0; i < 4; i++) (*pntio)(0x00);
		(*pntio)(0xff);
		for (i = 0; i < 8; i++)
		{
			r = (*pntio)(0xff);
			if (r == 0x00) break;
		}
		(*pntcshigh)();
		(*pntio)(0xff);
		if (r == 0x00)
		{
			restore_flags(flags);
			if (silent == 1) printk(KERN_INFO "mmc: Card init *OK2*\n");
			return(0);
		}
	}
	restore_flags(flags);

	return(2);
}

static int mmc_card_config(void)
{
	unsigned char r = 0;
	short i;
	unsigned char csd[32];
	unsigned int c_size;
	unsigned int c_size_mult;
	unsigned int mult;
	unsigned int read_bl_len;
	unsigned int blocknr = 0;
	unsigned int block_len = 0;
	unsigned int size = 0;

	(*pntcslow)();
	for (i = 0; i < 4; i++) (*pntio)(0xff);
	(*pntio)(0x49);
	for (i = 0; i < 4; i++) (*pntio)(0x00);
	(*pntio)(0xff);
	for (i = 0; i < 8; i++)
	{
		r = (*pntio)(0xff);
		if (r == 0x00) break;
	}
	if (r != 0x00)
	{
		(*pntcshigh)();
		(*pntio)(0xff);
		return(1);
	}
	for (i = 0; i < 8; i++)
	{
		r = (*pntio)(0xff);
		if (r == 0xfe) break;
	}
	if (r != 0xfe)
	{
		(*pntcshigh)();
		(*pntio)(0xff);
		return(2);
	}
	for (i = 0; i < 16; i++)
	{
		r = (*pntio)(0xff);
		csd[i] = r;
	}
	for (i = 0; i < 2; i++)
	{
		r = (*pntio)(0xff);
	}
	(*pntcshigh)();
	(*pntio)(0xff);
	if (r == 0x00) return(3);

	c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
	c_size >>= 6;
	c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
	c_size_mult >>= 7;
	read_bl_len = csd[5] & 0x0f;
	mult = 1;
	mult <<= c_size_mult + 2;
	blocknr = (c_size + 1) * mult;
	block_len = 1;
	block_len <<= read_bl_len;
	size = block_len * blocknr;
	size >>= 10;

	for(i=0; i<(1<<6); i++) {
	  hd_blocksizes[i] = 1024;
	  hd_hardsectsizes[i] = block_len;
	  hd_maxsect[i] = 256;
	}
	hd_sizes[0] = size;
	hd[0].nr_sects = blocknr;


    printk(KERN_INFO "mmc: Media found Size = %d, hardsectsize = %d, sectors = %d\n",
	       size, block_len, blocknr);

	return 0;
}


#ifdef CHECK_MEDIA_CHANGE
static int mmc_check_media_change(kdev_t dev)
{
	(void)dev;
	if (mmc_media_changed == 1)
	{
		mmc_media_changed = 0;
		return 1;
	}
	else return 0;
}
#endif

static int mmc_init(void)
{
	int rc;

	rc = mmc_card_init(1);
	if ( rc != 0)
	{
		// Give it an extra shot
		rc = mmc_card_init(1);
		if ( rc != 0)
		{
			printk(KERN_ERR "mmc: error in mmc_card_init (%d)\n", rc);
			return -1;
		}
	}

	memset(hd_sizes, 0, sizeof(hd_sizes));
	rc = mmc_card_config();
	if ( rc != 0)
	{
		printk(KERN_ERR "mmc: error in mmc_card_config (%d)\n", rc);
		return -1;
	}


	blk_size[MAJOR_NR] = hd_sizes;

	memset(hd, 0, sizeof(hd));
	hd[0].nr_sects = hd_sizes[0]*2;

	blksize_size[MAJOR_NR] = hd_blocksizes;
	hardsect_size[MAJOR_NR] = hd_hardsectsizes;
	max_sectors[MAJOR_NR] = hd_maxsect;

	hd_gendisk.nr_real = 1;

	register_disk(&hd_gendisk, MKDEV(MAJOR_NR,0), 1<<6,&mmc_bdops, hd_sizes[0]*2);

	return 0;
}

static void mmc_exit(void)
{
	blk_size[MAJOR_NR] = NULL;
	blksize_size[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;
	max_sectors[MAJOR_NR] = NULL;
	hd[0].nr_sects = 0;
}

static void mmc_check_media(void)
{
	int old_state;
	int rc;

	old_state = mmc_media_detect;

	// TODO: Add card detection here
	mmc_media_detect = 1;
	if (old_state != mmc_media_detect)
	{
		mmc_media_changed = 1;
		if (mmc_media_detect == 1)
		{
			rc = mmc_init();
			if (rc != 0) printk(KERN_ERR "mmc: error in mmc_init (%d)\n", rc);
		}
		else
		{
			mmc_exit();
		}
	}

#ifdef CHECK_MEDIA_CHANGE
    del_timer(&mmc_timer);
	mmc_timer.expires = jiffies + 10*HZ;
	add_timer(&mmc_timer); 
#endif	
	
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////// Driver Interface
///////////////////////////////////////////////////////////////////////////////////////////////
static int mmc_revalidate(kdev_t dev)
{
	int target, max_p, start, i;
	if (mmc_media_detect == 0) return -ENODEV;

	target = DEVICE_NR(dev);

	max_p = hd_gendisk.max_p;
	start = target << 6;
	for (i = max_p - 1; i >= 0; i--) {
		int minor = start + i;
		invalidate_device(MKDEV(MAJOR_NR, minor), 1);
		hd_gendisk.part[minor].start_sect = 0;
		hd_gendisk.part[minor].nr_sects = 0;
	}

	grok_partitions(&hd_gendisk, target, 1 << 6,
			hd_sizes[0] * 2);

	return 0;
}

static int mmc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (!inode || !inode->i_rdev)
		return -EINVAL;

	switch(cmd) {
	case BLKGETSIZE:
		return put_user(hd[MINOR(inode->i_rdev)].nr_sects, (unsigned long *)arg);
	case BLKGETSIZE64:
		return put_user((u64)hd[MINOR(inode->i_rdev)].
				nr_sects, (u64 *) arg);
	case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;

		return mmc_revalidate(inode->i_rdev);
	case HDIO_GETGEO:
	{
		struct hd_geometry *loc, g;
		loc = (struct hd_geometry *) arg;
		if (!loc)
			return -EINVAL;
		g.heads = 4;
		g.sectors = 16;
		g.cylinders = hd[0].nr_sects / (4 * 16);
		g.start = hd[MINOR(inode->i_rdev)].start_sect;
		return copy_to_user(loc, &g, sizeof(g)) ? -EFAULT : 0;
	}
	default:
		return blk_ioctl(inode->i_rdev, cmd, arg);
	}
}

static void mmc_request(request_queue_t *q)
{
	unsigned int mmc_address;
	unsigned char *buffer_address;
	int nr_sectors;
	int i;
	int cmd;
	int rc, code;

	(void)q;
	while (1)
	{
		code = 1; // Default is success
		INIT_REQUEST;
		mmc_address = (CURRENT->sector + hd[MINOR(CURRENT->rq_dev)].start_sect) * hd_hardsectsizes[0];
		buffer_address = CURRENT->buffer;
		nr_sectors = CURRENT->current_nr_sectors;
		cmd = CURRENT->cmd;
		if (((CURRENT->sector + CURRENT->current_nr_sectors + hd[MINOR(CURRENT->rq_dev)].start_sect) > hd[0].nr_sects) || (mmc_media_detect == 0))
		{
			code = 0;
		}
		else if (cmd == READ)
		{
			spin_unlock_irq(&io_request_lock);
			for (i = 0; i < nr_sectors; i++)
			{
				rc = mmc_read_block(buffer_address, mmc_address);
				if (rc != 0)
				{
					printk(KERN_ERR "mmc: error in mmc_read_block (%d)\n", rc);
					code = 0;
					break;
				}
				else
				{
					mmc_address += hd_hardsectsizes[0];
					buffer_address += hd_hardsectsizes[0];
				}
			}
			spin_lock_irq(&io_request_lock);
		}
		else if (cmd == WRITE)
		{
			spin_unlock_irq(&io_request_lock);
			for (i = 0; i < nr_sectors; i++)
			{
				rc = mmc_write_block(mmc_address, buffer_address);
				if (rc != 0)
				{
					printk(KERN_ERR "mmc: error in mmc_write_block (%d)\n", rc);
					code = 0;
					break;
				}
				else
				{
					mmc_address += hd_hardsectsizes[0];
					buffer_address += hd_hardsectsizes[0];
				}
			}
			spin_lock_irq(&io_request_lock);
		}
		else
		{
			code = 0;
		}
		end_request(code);
	}
}

static int mmc_open(struct inode *inode, struct file *filp)
{
  //int device;
	(void)filp;

	if (mmc_media_detect == 0) return -ENODEV;

#if defined(MODULE)
	MOD_INC_USE_COUNT;
#endif
	return 0;
}

static int mmc_release(struct inode *inode, struct file *filp)
{
	(void)filp;
	fsync_dev(inode->i_rdev);
        invalidate_buffers(inode->i_rdev);

#if defined(MODULE)
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

static void assign_pnt(void) {

	// init function call pointer
	switch (wiringopt) 
	{
		case 1:
		pntinit 	= mmc_hardware_init;
		pntcslow 	= mmc_spi_cs_low_PB23;
		pntcshigh 	= mmc_spi_cs_high_PB23;
		pntio 		= mmc_spi_io_1;
		pntwri 		= mmc_spi_write_14;
		pntread		= mmc_read_multiple_1;
		break;

		case 2:
		pntinit 	= mmc_hardware_init;
		pntcslow 	= mmc_spi_cs_low_PB16;
		pntcshigh 	= mmc_spi_cs_high_PB16;
		pntio 		= mmc_spi_io_2;
		pntwri 		= mmc_spi_write_2;
		pntread		= mmc_read_multiple_2;
		break;

		case 4:
		pntinit 	= mmc_hardware_init;
		pntcslow 	= mmc_spi_cs_low_PB16;
		pntcshigh 	= mmc_spi_cs_high_PB16;
		pntio 		= mmc_spi_io_4;
		pntwri 		= mmc_spi_write_14;
		pntread		= mmc_read_multiple_4;
	}
}

static int set_wiring_and_try(unsigned char wiring) {

	unsigned short retrycnt = 2;

	if (wiring == 2) retrycnt = 5;

	wiringopt = wiring;
	assign_pnt();
	if ((*pntinit)() == 0) {
		for (; retrycnt > 0; retrycnt--) {
			if (mmc_card_init(0) == 0) return 0;
		}
	}
	return -1;
}

static int __init mmc_driver_init(void)
{
	int rc;

	printk(KERN_INFO "$Id: mmccombo.c,v 1.2 2007/05/21 08:37:28 satsuse Exp $\n");

	if ((wiringopt > 4) || (wiringopt == 3))
	{
		printk(KERN_WARNING "mmc: wiringopt out of range or unsupported by this version\n");
		return -1;
	}
	
	if (opt > 1)
	{
		printk(KERN_WARNING "mmc: opt out of range(0..1)\n");
		return -2;
	}

	rc = devfs_register_blkdev(MAJOR_NR, DEVICE_NAME, &mmc_bdops);
	if (rc < 0)
	{
		printk(KERN_WARNING "mmc: can't get major %d\n", MAJOR_NR);
		return -3;
	}

	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), mmc_request);

	read_ahead[MAJOR_NR] = 8;
	add_gendisk(&hd_gendisk);

	if (forcehw > 0) printk(KERN_INFO "mmc: force hardware allocation, even if otherwise assigned.\n");

	if (wiringopt < 1) {
		printk(KERN_INFO "mmc: User asked for autodetection, I'll try ...\n");
		if (set_wiring_and_try(4) != 0)
			if (set_wiring_and_try(1) != 0)
				if (set_wiring_and_try(2) != 0) {
					printk(KERN_WARNING "mmc: autodetection failed. Quit\n");
					mmc_driver_exit();
					return -4;
				}
	}
	else {
		assign_pnt();
		if ((*pntinit)() != 0) {
			printk(KERN_WARNING "mmc: Could not get hardware. Quit\n");
			mmc_driver_exit();
			return -5;
		}
	}

	printk(KERN_INFO "mmc: used wiringopt = %X\n", wiringopt);
	if (opt == 0) printk(KERN_INFO "mmc: Uses failsafe IO, no optimizations\n");

	mmc_check_media();

#ifdef CHECK_MEDIA_CHANGE
	init_timer(&mmc_timer);
	mmc_timer.expires = jiffies + HZ;
	mmc_timer.function = (void *)mmc_check_media;
	add_timer(&mmc_timer);
#endif	

	return 0;
}

static void __exit mmc_driver_exit(void)
{
	int i;
	del_timer(&mmc_timer);


	for (i = 0; i < (1 << 6); i++)
		fsync_dev(MKDEV(MAJOR_NR, i));

	devfs_register_partitions(&hd_gendisk, 0<<6, 1);
	devfs_unregister_blkdev(MAJOR_NR, DEVICE_NAME);
	del_gendisk(&hd_gendisk);
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	mmc_exit();
	mmc_hardware_cleanup();
	printk(KERN_INFO "mmc: removing driver\n");
}

module_init(mmc_driver_init);
module_exit(mmc_driver_exit);
