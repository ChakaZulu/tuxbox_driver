//  $Id: mmc2test.c,v 1.1 2006/07/28 12:09:55 guenther Exp $
//
//  MMC2_test.c - Development Version ONLY !!!!!
//  General MMC device driver
//  Modem connector pins PA9,PA8,PB16,PB17 used -> MM2 connection sheme
//
//  This version of MMC is used to test various optimisation variants only
//
// 24 Jul 2006
//                                           ---- Modem_CN ---
//                                           SAGEM NOKIA  PHIL
// PA9  = SD_DO = 0x0040 = SD_Card Pin 7   =   2    12     11
// PA8  = SD_DI = 0x0080 = SD_Card Pin 2   =   1    11      9
// PB17 = SD_CLK= 0x4000 = SD_Card Pin 5   =  10     2      7
// PB16 = SD_CS = 0x8000 = SD_Card Pin 1   =   6     6      5
// GND  =       = Masse  = SD_Card Pin 3,6 =   3    10      2
// VCC  =       = 3,3V   = SD_Card Pin 4   =   5    16      x (philips: connect in series 3 Diodes (1N4007) from MODEM_CN 1 to SD/MMC card pin 4)
//
// Speed of write Optimisation (MCC_WO) is not tested yet
// Speed of read  Optimisation (MCC_RO):
// MCC_RO  1: 10 MB read (51 seconds, 200 kb/s)
// MCC_RO  2: 10 MB read (38 seconds, 260 kb/s)
// MCC_RO  3: 10 MB read (34 seconds, 290 kb/s)
// MCC_RO  4: 10 MB read (35 seconds, 290 kb/s)
// MCC_RO  6: 10 MB read (31 seconds, 320 kb/s)

// MCC_RO  8: 10 MB read (32 seconds, 310 kb/s)
// MCC_RO 10: 10 MB read (31 seconds, 320 kb/s)
// MCC_RO 11: 10 MB read (32 seconds, 310 kb/s)

// Original MMC2 driver: 10 MB read (65 seconds, 150 kb/s).
//


#define MCC_RO 10
#define MCC_WO 3

#include <linux/delay.h>
#include <linux/timer.h>

#include <linux/module.h>
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

MODULE_AUTHOR("Madsuk/Rohde/TaGana");
MODULE_DESCRIPTION("Driver MMC/SD-Cards");
MODULE_SUPPORTED_DEVICE("all dbox2 on com2 connector");
MODULE_LICENSE("GPL");

unsigned int mmc_error_counter[20]={0,};
unsigned int mmc_diag_counter[20]={0,};
unsigned int mmc_time_counter[20]={0xffffffff,0,0xffffffff,0,0xffffffff,0,0xffffffff,0,0xffffffff,0,0xffffffff,0};

typedef unsigned int uint32;

/* we have only one device */
static int hd_sizes[1<<6];
static int hd_blocksizes[1<<6];
static int hd_hardsectsizes[1<<6];
static int hd_maxsect[1<<6];
static struct hd_struct hd[1<<6];

static struct timer_list mmc_timer;
static int mmc_media_detect = 0;
static int mmc_media_changed = 1;

static int mmc_open(struct inode *inode, struct file *filp);
static int mmc_release(struct inode *inode, struct file *filp);
static int mmc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

static struct block_device_operations mmc_bdops =
{
    open: mmc_open,
    release: mmc_release,
    ioctl: mmc_ioctl,
#if 0
    check_media_change: mmc_check_media_change,
    revalidate: mmc_revalidate,
#endif
};

static struct gendisk hd_gendisk = {
    major:        MAJOR_NR,
    major_name:    DEVICE_NAME,
    minor_shift:    6,
    max_p:        1 << 6,
    part:        hd,
    sizes:        hd_sizes,
    fops:        &mmc_bdops,
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/////// Low-Level Driver
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

volatile immap_t *immap=(immap_t *)IMAP_ADDR ;

#define SD_DO_NUM 6             // on SD/MMC card pin 7
#define SD_DO (1<<SD_DO_NUM)    //#define SD_DO  0x0040 // on SD/MMC card pin 7
#define SD_DI  0x0080           // on SD/MMC card pin 2
#define SD_CLK 0x4000           // on SD/MMC card pin 5
#define SD_CS  0x8000           // on SD/MMC card pin 1

static void mmc_spi_cs_low(void)
{
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat &= ~(SD_CS);
}

static void mmc_spi_cs_high(void)
{
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat |= SD_CS;
}

static int mmc_hardware_init(void) {
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    mmc_diag_counter[9]++;
    printk("mmc2: Hardware init\n");
    cp->cp_pbpar &=   ~(SD_CLK | SD_CS);
    cp->cp_pbodr &=   ~(SD_CLK | SD_CS);
    cp->cp_pbdir |=    (SD_CLK | SD_CS);
    cpi->iop_papar &= ~(SD_DO | SD_DI);
    cpi->iop_paodr &= ~(SD_DO);
    cpi->iop_padir |=   SD_DI;
    cpi->iop_padir &=  ~SD_DO;

    // Clock + CS low
    cp->cp_pbdat &= ~(SD_CLK | SD_CS);
    cpi->iop_padat &= ~SD_DI;
    return 0;
}

static unsigned char mmc_spi_io(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned char result = 0;
  unsigned char  i;

  for(i = 0x80; i != 0; i >>= 1) {
    if (data_out & i)
      cpi->iop_padat |= SD_DI;
    else
      cpi->iop_padat &= ~SD_DI;

    cp->cp_pbdat |= SD_CLK;
    if (cpi->iop_padat & SD_DO) {
        result |= i;
    }
    cp->cp_pbdat &= ~SD_CLK;
  }

  return result;
}

/////////////////////////////
/// write variants
#if (MCC_WO == 1)
static inline void mmc_spi_write(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned char  i;

  for(i = 0x80; i != 0; i >>= 1) {
    if (data_out & i)
      cpi->iop_padat |= SD_DI;
    else
      cpi->iop_padat &= ~SD_DI;

    cp->cp_pbdat |= SD_CLK;

    cp->cp_pbdat &= ~SD_CLK;
  }
}
#endif
#if (MCC_WO == 2)
void inline mmc_spi_write(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned long clk_high = cp->cp_pbdat | SD_CLK;
  unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    if (data_out & 0x80) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x40) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x20) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x10) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x08) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x04) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x02) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x01) cpi->iop_padat |=  SD_DI;
    else                 cpi->iop_padat &= ~SD_DI;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
}
#endif
#if (MCC_WO == 3)

void mmc_spi_write(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned long clk_high = cp->cp_pbdat | SD_CLK;
  unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

  unsigned long data_high = cpi->iop_padat | SD_DI;
  unsigned long data_low =  cpi->iop_padat & ~SD_DI;

    if (data_out & 0x80) cpi->iop_padat = data_high;// 1 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x40) cpi->iop_padat = data_high;// 2 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x20) cpi->iop_padat = data_high;// 3 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x10) cpi->iop_padat = data_high;// 4 bit
    else                 cpi->iop_padat= data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x08) cpi->iop_padat = data_high;// 5 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x04) cpi->iop_padat = data_high;// 6 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x02) cpi->iop_padat = data_high;// 7 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    if (data_out & 0x01) cpi->iop_padat = data_high;// 8 bit
    else                 cpi->iop_padat = data_low;
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
}
#endif
#if (MCC_WO == 4)
void inline mmc_spi_write(unsigned char data_out) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned long clk_high = cp->cp_pbdat | SD_CLK;
  unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

  volatile unsigned char data = cpi->iop_padat;

    // 1 bit
    cpi->iop_padat = data | ((data_out & 0x80));
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 2 bit
    cpi->iop_padat = data | ((data_out & 0x40)<<1);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 3 bit
    cpi->iop_padat = data | ((data_out & 0x20)<<2);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 4 bit
    cpi->iop_padat = data | ((data_out & 0x10)<<3);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 5 bit
    cpi->iop_padat = data | ((data_out & 0x08)<<4);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 6 bit
    cpi->iop_padat = data | ((data_out & 0x04)<<5);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 7 bit
    cpi->iop_padat = data | ((data_out & 0x02)<<6);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
    // 8 bit
    cpi->iop_padat = data | ((data_out & 0x01)<<7);
    cp->cp_pbdat = clk_high;
    cp->cp_pbdat = clk_low;
}
#endif

/////////////////////////////
/// read variants

#if ((MCC_RO != 1) && (MCC_RO != 2) && (MCC_RO != 4) )
//#if (MCC_RO == 3)
static inline unsigned char mmc_spi_read(void)
{
  unsigned char result = 0;

   volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
   volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

  unsigned long clk_high = cp->cp_pbdat | SD_CLK;
  unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

   cpi->iop_padat |= SD_DI;

    cp->cp_pbdat = clk_high; //1
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //2
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //3
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //4
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //5
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //6
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //7
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //8
         result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;

    return result;
}
#endif

#if (MCC_RO == 10)
void mmc_read_block_low_level(unsigned char *dest)
{
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    cpi->iop_padat |= SD_DI;

    unsigned char result=0;
    unsigned int i=0;

    for (i = 0; i < 512; i++)
    {
        cp->cp_pbdat = clk_high; //1
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //2
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //3
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //4
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //5
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //6
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //7
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;
        cp->cp_pbdat = clk_high; //8
             result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
        cp->cp_pbdat = clk_low;

        *dest++ = result;
        result = 0;
    }
}
#endif


#if (MCC_RO == 1)
static inline unsigned char mmc_spi_read(void) {
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
    unsigned char result = 0;
    unsigned char  i;

    cpi->iop_padat |= SD_DI;
    for(i = 0x80; i != 0; i >>= 1) {
        cp->cp_pbdat |= SD_CLK;
        if (cpi->iop_padat & SD_DO) {
           result |= i;
           }
        cp->cp_pbdat &= ~SD_CLK;
    }

    return result;
}
#endif

#if (MCC_RO == 2)
static inline unsigned char mmc_spi_read(void) {
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
    unsigned char result = 0;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    cpi->iop_padat |= SD_DI;

    // 1 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x80;
    cp->cp_pbdat = clk_low;
    // 2 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x40;
    cp->cp_pbdat = clk_low;
    // 3 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x20;
    cp->cp_pbdat = clk_low;
    // 4 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x10;
    cp->cp_pbdat = clk_low;
    // 5 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x08;
    cp->cp_pbdat = clk_low;
    // 6 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x04;
    cp->cp_pbdat = clk_low;
    // 7 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x02;
    cp->cp_pbdat = clk_low;
    // 8 bit
    cp->cp_pbdat = clk_high;
    if (cpi->iop_padat & SD_DO) result |= 0x01;
    cp->cp_pbdat = clk_low;

  return result;
}
#endif


#if (MCC_RO == 4)
static inline unsigned char mmc_spi_read(void)
{
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    unsigned char result = 0;

    cpi->iop_padat |= SD_DI;

    // 1 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) << 1;
    cp->cp_pbdat = clk_low;
    // 2 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO);
    cp->cp_pbdat = clk_low;
    // 3 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 1;
    cp->cp_pbdat = clk_low;
    // 4 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 2;
    cp->cp_pbdat = clk_low;
    // 5 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 3;
    cp->cp_pbdat = clk_low;
    // 6 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 4;
    cp->cp_pbdat = clk_low;
    // 7 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 5;
    cp->cp_pbdat = clk_low;
    // 8 bit
    cp->cp_pbdat = clk_high;
    result |=  (cpi->iop_padat & SD_DO) >> 6;
    cp->cp_pbdat = clk_low;

    return result;
}
#endif


#if (MCC_RO == 6)
static inline unsigned int mmc_spi_read_int(void)
{
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    unsigned int result = 0;

    cpi->iop_padat |= SD_DI;

    // Byte 0
    cp->cp_pbdat = clk_high; //1
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //2
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //3
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //4
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //5
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //6
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //7
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //8
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    // Byte 1
    cp->cp_pbdat = clk_high; //1
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //2
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //3
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //4
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //5
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //6
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //7
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //8
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    // Byte 2
    cp->cp_pbdat = clk_high; //1
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //2
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //3
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //4
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //5
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //6
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //7
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //8
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    // Byte 3
    cp->cp_pbdat = clk_high; //1
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //2
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //3
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //4
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //5
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //6
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //7
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    cp->cp_pbdat = clk_high; //8
    result <<= 1;result |= ((cpi->iop_padat & SD_DO)?0x01:0x00);
    cp->cp_pbdat = clk_low;
    return result;
}
#endif


#if (MCC_RO == 8)
// Achtung, hier sind garantiert noch Bugs drin und das Timing ist zu knapp
static __inline__ int set_bit_from_bit( const unsigned int dest_nr,
                                        unsigned int dest,
                                        const unsigned int src_nr,
                                        const unsigned int src)
{
        __asm__ __volatile__("\n\
        rlwinm %[src] ,%[src],32-%[src_nr],0,31 \n\
        rlwimi %[dest],%[src],%[dest_nr],31-%[dest_nr],31-%[dest_nr] \n"
        : [dest] "=&r" (dest)
        : [src] "r" (src), [dest_nr] "i" (dest_nr), [src_nr] "i" (src_nr)
        : "cc");
         return dest;
}

void mmc_read_block_low_level(unsigned int *dest)
{
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    //volatile unsigned long * data =(volatile unsigned long *)&cpi->iop_padat;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    unsigned int i;
    unsigned int in;

    cpi->iop_padat |= SD_DI;

    for(i = 0; i<128; i++)
    {
        in = 0x00;
        /* xx ns delay after clock before reading SD_DO is needed -
        insert NOPs or move the clocksetting within the macro */
        #define GET_THIS_BIT(n) cp->cp_pbdat = clk_high; \
                        in = set_bit_from_bit((n), in, SD_DO_NUM, cpi->iop_padat); \
                        cp->cp_pbdat = clk_low;

        GET_THIS_BIT(31);
        GET_THIS_BIT(30);
        GET_THIS_BIT(29);
        GET_THIS_BIT(28);
        GET_THIS_BIT(27);
        GET_THIS_BIT(26);
        GET_THIS_BIT(25);
        GET_THIS_BIT(24);
        GET_THIS_BIT(23);
        GET_THIS_BIT(22);
        GET_THIS_BIT(21);
        GET_THIS_BIT(20);
        GET_THIS_BIT(19);
        GET_THIS_BIT(18);
        GET_THIS_BIT(17);
        GET_THIS_BIT(16);
        GET_THIS_BIT(15);
        GET_THIS_BIT(14);
        GET_THIS_BIT(13);
        GET_THIS_BIT(12);
        GET_THIS_BIT(11);
        GET_THIS_BIT(10);
        GET_THIS_BIT(9);
        GET_THIS_BIT(8);
        GET_THIS_BIT(7);
        GET_THIS_BIT(6);
        GET_THIS_BIT(5);
        GET_THIS_BIT(4);
        GET_THIS_BIT(3);
        GET_THIS_BIT(2);
        GET_THIS_BIT(1);
        GET_THIS_BIT(0);

        *dest++ = in; /* endianness ok? */
    }
}
#endif

#if (MCC_RO == 11)
static __inline__ int set_bit_from_bit( const unsigned int dest_nr,
                                        unsigned int dest,
                                        const unsigned int src_nr,
                                        const unsigned int src)
{
         __asm__ __volatile__("\n\
        rlwinm %[src] ,%[src],32-%[src_nr],0,31 \n\
        rlwimi %[dest],%[src],%[dest_nr],31-%[dest_nr],31-%[dest_nr] \n"
        : [dest] "=&r" (dest)
        : [src] "r" (src), [dest_nr] "i" (dest_nr), [src_nr] "i" (src_nr)
        : "cc");
         return dest;
}

void mmc_read_block_low_level(unsigned char *dest)
{
    volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
    volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

    //volatile unsigned long * data =(volatile unsigned long *)&cpi->iop_padat;

    unsigned long clk_high = cp->cp_pbdat | SD_CLK;
    unsigned long clk_low =  cp->cp_pbdat & ~SD_CLK;

    unsigned int i;
    unsigned int in;

    cpi->iop_padat |= SD_DI;

    for(i = 0; i<512; i++)
    {
        in = 0x00;
        /* xx ns delay after clock before reading SD_DO is needed -
        insert NOPs or move the clocksetting within the macro */
        #define GET_THIS_BIT(n) cp->cp_pbdat = clk_high; \
                            in = set_bit_from_bit((n), in, SD_DO_NUM, cpi->iop_padat); \
                            cp->cp_pbdat = clk_low;

        GET_THIS_BIT(7);
        GET_THIS_BIT(6);
        GET_THIS_BIT(5);
        GET_THIS_BIT(4);
        GET_THIS_BIT(3);
        GET_THIS_BIT(2);
        GET_THIS_BIT(1);
        GET_THIS_BIT(0);

        *dest++ = in; /* endianness ok? */
    }
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/////// High-Level Driver
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

static void mmc_print_diag(void)
{
    printk("--------------------\n");
    printk("MMC2opt (%d-%d) Diagnostic Data:\n",MCC_WO,MCC_RO);
    printk("--------------------\n");
    printk("write_block:%4d\n",mmc_diag_counter[0]);
    printk("read block: %4d\n",mmc_diag_counter[1]);
    printk("request:    %4d\n",mmc_diag_counter[2]);
    printk("open:       %4d\n",mmc_diag_counter[3]);
    printk("release:    %4d\n",mmc_diag_counter[4]);
    printk("revalidate: %4d\n",mmc_diag_counter[5]);
    printk("ioctl:      %4d\n",mmc_diag_counter[6]);
    printk("card_init:  %4d\n",mmc_diag_counter[7]);
    printk("config:     %4d\n",mmc_diag_counter[8]);
    printk("init_hw:    %4d\n",mmc_diag_counter[9]);
    printk("init:       %4d\n",mmc_diag_counter[10]);
    printk("exit:       %4d\n",mmc_diag_counter[11]);
    printk("check:      %4d\n",mmc_diag_counter[12]);
    printk("driver_init:%4d\n",mmc_diag_counter[13]);

    printk("write err:  %4d,%4d\n",mmc_error_counter[0],mmc_error_counter[1]);
    printk("read err:   %4d,%4d\n",mmc_error_counter[2],mmc_error_counter[3]);
    printk("request err:%4d,%4d\n",mmc_error_counter[4],mmc_error_counter[5]);
    printk("cardini err:%4d,%4d\n",mmc_error_counter[6],mmc_error_counter[7]);
    printk("cardcon err:%4d,%4d,%4d\n",mmc_error_counter[8],mmc_error_counter[9],mmc_error_counter[10]);
    printk("init err:   %4d,%4d,%4d\n",mmc_error_counter[11],mmc_error_counter[12],mmc_error_counter[13]);

    printk("write timer:%4d,%4d\n",mmc_time_counter[2],mmc_time_counter[3]);
    printk("read timer: %4d,%4d\n",mmc_time_counter[4],mmc_time_counter[5]);
    printk("--------------------\n");

}


static void mmc_reset_error(void)
{
    int i = 0;
    for( i = 0; i< 20; i++)
    {
        mmc_error_counter[i]=0;
    }
}

static void mmc_reset_diag(void)
{
int i = 0;
    for( i = 0; i< 20; i++)
    {
        mmc_diag_counter[i]=0;
        mmc_time_counter[i]=0;
    }
}


int mmc_write_block(unsigned int dest_addr, unsigned char *data)
{
    int i;

    mmc_diag_counter[0]++;

    mmc_spi_cs_low();
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);

    mmc_spi_write(0x58);
    mmc_spi_write(0xff & (dest_addr >> 24)); /* msb */
    mmc_spi_write(0xff & (dest_addr >> 16));
    mmc_spi_write(0xff & (dest_addr >> 8));
    mmc_spi_write(0xff & dest_addr); /* lsb */
    mmc_spi_write(0xff);

    for (i=0;;)
    {
        if (mmc_spi_read() == 0x00) break;
        if (i++ >= 8)
        {
            // ERROR: exit function after timeout!!!
            mmc_spi_cs_high();
            mmc_spi_write(0xFF);
            mmc_error_counter[0]++;
            return(1);
        }
    }

    mmc_spi_write(0xfe);
    for (i = 0; i < 512; i++)
    {
        mmc_spi_write(data[i]);
    }
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);

    for (i=0;;)
    {
        if (mmc_spi_read() == 0xff) break;
        if (i++ >= 1000000)
        {
            // ERROR: exit function after timeout!!!
            mmc_spi_cs_high();
            mmc_spi_write(0xFF);
            mmc_error_counter[1]++;
            return(1);
        }
    }

    if(i < mmc_time_counter[2]) mmc_time_counter[2] = i;
    if(i > mmc_time_counter[3]) mmc_time_counter[3] = i;

    mmc_spi_cs_high();
    mmc_spi_write(0xff);
    return(0);
}

int mmc_read_block(unsigned char *data, unsigned int src_addr)
{
    unsigned int i;

    mmc_diag_counter[1]++;

    // activate Device
    mmc_spi_cs_low();

    // send dummy bytes for sync (1 might be enough?)
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);
    mmc_spi_write(0xff);

    // send adress
    mmc_spi_write(0x51);
    mmc_spi_write(0xff & (src_addr >> 24)); /* msb */
    mmc_spi_write(0xff & (src_addr >> 16));
    mmc_spi_write(0xff & (src_addr >> 8));
    mmc_spi_write(0xff & src_addr); /* lsb */
    mmc_spi_write(0xff);

    // wait for no error response
    for (i=0;;)
    {
        if (mmc_spi_read() == 0x00) break;
                if (i++ >= 8)
        {
            // ERROR: exit function after timeout!!!
            mmc_spi_cs_high();
            mmc_spi_write(0xFF);
            mmc_error_counter[2]++;
            return(1);
        }
    }

    // wait for start data block
    /*    The maximum read access time is calculated by the host as follows.
        N.AC(max)= 100 ((TAAC * fPP) + (100 * NSAC)) ;
        fPP is the interface clock rate and TAAC & NSAC are given in the CSD Register.*/
    for (i=0;;)
    {
        if (mmc_spi_read() == 0xfe) break;
        if (i++ >= 100000)
        {
            // ERROR: exit function after timeout!!!
            mmc_spi_cs_high();
            mmc_spi_write(0xFF);
            mmc_error_counter[3]++;
            return(2);
        }
    }

    if(i < mmc_time_counter[4]) mmc_time_counter[4] = i;
    if(i > mmc_time_counter[5]) mmc_time_counter[5] = i;


#if ( (MCC_RO == 11) || (MCC_RO == 10))
    mmc_read_block_low_level(data);
#endif
#if (MCC_RO == 8)
    mmc_read_block_low_level((unsigned int*)data);
#endif
#if ((MCC_RO == 1) || (MCC_RO == 2) || (MCC_RO == 3) || (MCC_RO == 4) )
    for(i=0;i<512;i++) data[i] = mmc_spi_read();
#endif
#if (MCC_RO == 6 )
    for(i=0;i<128;i++) ((unsigned int *)data)[i] = mmc_spi_read();
#endif

#if 0 // debug print
    printk("Rx Data: %08x:%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",src_addr
        ,data[0],data[1],data[2],data[3],data[4]
        ,data[5],data[6],data[7],data[8],data[9]);
#endif

    // read CRC for nothing
    mmc_spi_read();
    mmc_spi_read();

    // release device
    mmc_spi_cs_high();
    mmc_spi_write(0xff);

    return(0);
}


static int mmc_card_init(void)
{
    unsigned char r = 0;
    short i, j;
    unsigned long flags;

    mmc_diag_counter[7]++;

    save_flags(flags);
    cli();

        printk("mmc Card init\n");
    mmc_spi_cs_high();
    for (i = 0; i < 1000; i++) mmc_spi_io(0xff);

    mmc_spi_cs_low();

    mmc_spi_io(0x40);
    for (i = 0; i < 4; i++) mmc_spi_io(0x00);
    mmc_spi_io(0x95);
    for (i = 0; i < 8; i++)
    {
        r = mmc_spi_io(0xff);
        if (r == 0x01) break;
    }
    mmc_spi_cs_high();
    mmc_spi_io(0xff);
    if (r != 0x01)
    {
        restore_flags(flags);
        (mmc_error_counter[6])++;
        return(1);
    }

    printk("mmc Card init *1*\n");
    for (j = 0; j < 30000; j++)
    {
        mmc_spi_cs_low();

        mmc_spi_io(0x41);
        for (i = 0; i < 4; i++) mmc_spi_io(0x00);
        mmc_spi_io(0xff);
        for (i = 0; i < 8; i++)
        {
            r = mmc_spi_io(0xff);
            if (r == 0x00) break;
        }
        mmc_spi_cs_high();
        mmc_spi_io(0xff);
        if (r == 0x00)
        {
            restore_flags(flags);
            printk("mmc Card init *2*\n");
            return(0);
        }
    }
    restore_flags(flags);
    (mmc_error_counter[7])++;

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

    mmc_diag_counter[8]++;

    mmc_spi_cs_low();
    for (i = 0; i < 4; i++) mmc_spi_io(0xff);
    mmc_spi_io(0x49);
    for (i = 0; i < 4; i++) mmc_spi_io(0x00);
    mmc_spi_io(0xff);
    for (i = 0; i < 8; i++)
    {
        r = mmc_spi_io(0xff);
        if (r == 0x00) break;
    }
    if (r != 0x00)
    {
        mmc_spi_cs_high();
        mmc_spi_io(0xff);
        (mmc_error_counter[8])++;
        return(1);
    }
    for (i = 0; i < 8; i++)
    {
        r = mmc_spi_io(0xff);
        if (r == 0xfe) break;
    }
    if (r != 0xfe)
    {
        mmc_spi_cs_high();
        mmc_spi_io(0xff);
        (mmc_error_counter[9])++;
        return(2);
    }
    for (i = 0; i < 16; i++)
    {
        r = mmc_spi_io(0xff);
        csd[i] = r;
    }
    for (i = 0; i < 2; i++)
    {
        r = mmc_spi_io(0xff);
    }
    mmc_spi_cs_high();
    mmc_spi_io(0xff);
    if (r == 0x00)
    {
        (mmc_error_counter[10])++;
        return(3);
    }

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


    printk("Size = %d, hardsectsize = %d, sectors = %d\n",
           size, block_len, blocknr);

    return 0;
}

/*
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
*/

static int mmc_init(void)
{
    int rc;

    mmc_diag_counter[10]++;
    rc = mmc_hardware_init();

    if ( rc != 0)
    {
        printk("mmc: error in mmc_hardware_init (%d)\n", rc);
        return -1;
    }

    rc = mmc_card_init();
    if ( rc != 0)
    {
        // Give it an extra shot
        rc = mmc_card_init();
        if ( rc != 0)
        {
            printk("mmc: error in mmc_card_init (%d)\n", rc);
            (mmc_error_counter[11])++;
            return -1;
        }
    }
    else
    {
        (mmc_error_counter[12])++;
    }

    memset(hd_sizes, 0, sizeof(hd_sizes));
    rc = mmc_card_config();
    if ( rc != 0)
    {
        printk("mmc: error in mmc_card_config (%d)\n", rc);
        (mmc_error_counter[13])++;
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
    mmc_diag_counter[11]++;

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

    mmc_diag_counter[12]++;
    old_state = mmc_media_detect;

    // TODO: Add card detection here
    mmc_media_detect = 1;
    if (old_state != mmc_media_detect)
    {
        mmc_media_changed = 1;
        if (mmc_media_detect == 1)
        {
            rc = mmc_init();
            if (rc != 0) printk("mmc: error in mmc_init (%d)\n", rc);
            else
                printk("mmc: media found\n");
        }
        else
        {
            mmc_exit();
        }
    }

    /* del_timer(&mmc_timer);
    mmc_timer.expires = jiffies + 10*HZ;
    add_timer(&mmc_timer); */
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/////// Driver Interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

static void mmc_request(request_queue_t *q)
{
    unsigned int mmc_address;
    unsigned char *buffer_address;
    int nr_sectors;
    int i;
    int cmd;
    int rc, code;

    mmc_diag_counter[2]++;
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
                    printk("mmc: error in mmc_read_block (%d)\n", rc);
                    code = 0;
                    (mmc_error_counter[4])++;
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
                    printk("mmc: error in mmc_write_block (%d)\n", rc);
                    code = 0;
                    (mmc_error_counter[5])++;
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

    mmc_diag_counter[3]++;
    if (mmc_media_detect == 0) return -ENODEV;

#if defined(MODULE)
    MOD_INC_USE_COUNT;
#endif
    return 0;
}

static int mmc_release(struct inode *inode, struct file *filp)
{
    (void)filp;
    mmc_diag_counter[4]++;
    fsync_dev(inode->i_rdev);
        invalidate_buffers(inode->i_rdev);

#if defined(MODULE)
    MOD_DEC_USE_COUNT;
#endif
    return 0;
}

extern struct gendisk hd_gendisk;

static int mmc_revalidate(kdev_t dev)
{
    int target, max_p, start, i;
    if (mmc_media_detect == 0) return -ENODEV;

    mmc_diag_counter[5]++;

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

    mmc_diag_counter[6]++;

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

static int __init mmc_driver_init(void)
{
    int rc;

    printk("------ MMC2test Driver Version (%d-%d) $Id: mmc2test.c,v 1.1 2006/07/28 12:09:55 guenther Exp $\n",MCC_WO,MCC_RO);
    printk("------ optimized bit banging version \n");
    printk("------ Instable version, use for development only !!!! \n");
    mmc_diag_counter[13]++;
    rc = devfs_register_blkdev(MAJOR_NR, DEVICE_NAME, &mmc_bdops);
    if (rc < 0)
    {
        printk(KERN_WARNING "mmc: can't get major %d\n", MAJOR_NR);
        return rc;
    }

    blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), mmc_request);

    read_ahead[MAJOR_NR] = 8;
    add_gendisk(&hd_gendisk);

    mmc_check_media();

    /*init_timer(&mmc_timer);
    mmc_timer.expires = jiffies + HZ;
    mmc_timer.function = (void *)mmc_check_media;
    add_timer(&mmc_timer);*/

    return 0;
}

static void __exit mmc_driver_exit(void)
{
    int i;
    printk("mmc_driver_exit\n");
    del_timer(&mmc_timer);

    printk("mmc_driver_exit\n");
    for (i = 0; i < (1 << 6); i++)
        fsync_dev(MKDEV(MAJOR_NR, i));

    printk("mmc_driver_exit\n");
    devfs_register_partitions(&hd_gendisk, 0<<6, 1);
    devfs_unregister_blkdev(MAJOR_NR, DEVICE_NAME);
    del_gendisk(&hd_gendisk);
    printk("mmc_driver_exit\n");
    blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
    mmc_exit();
    printk("removing mmc2opt.o\n");
    mmc_print_diag();
    mmc_reset_error();
    mmc_reset_diag();
}

module_init(mmc_driver_init);
module_exit(mmc_driver_exit);
