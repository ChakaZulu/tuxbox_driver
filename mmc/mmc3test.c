//  $Id: mmc3test.c,v 1.1 2006/07/28 12:09:55 guenther Exp $
//
// !!!! This version is not tested and WILL have a lot of errors !!!!!!!
//
//
//  MMC3test.c - Development Version ONLY !!!!!
//  MMC device driver for Nokia (and updated Sagems)
//  Modem connector pins PA9,PA8,PA7,PB17 used 
// 
//  This version of MMC is used to test optimisation possibilities with the 
//  processor's CPM-SMC2 channel only

//  24 Jul 2006 
//  sd/mmc card connection scheme
//
//                     ----------------
//                    / 1 2 3 4 5 6 7 x| MMC/SC Card
//                    |x               |
//
//    _----------------------------------------
//   | |        dbox2 tuner Nokia              |
//    ~----------------------------------------
//     |        1  3  5  7  9 11 13 15 17 19
//     |        2  4  6  8 10 12 14 16 18 20
//
//                                                   ---- Modem_CN ---
//                                                    NOKIA SAGEM PHIL
// PA9  = SMRXD2 = SD_DO = 0x0040 = SD_CARD Pin 7   =  12     2    11
// PA8  = SMTXD2 = SD_DI = 0x0080 = SD_CARD Pin 2   =  11     1     9   (Sagem: PIN1 close to power supply)
// PA7  = BRGO1  = SD_CLK= 0x0100 = SD_CARD Pin 5   =  14     x     ?   (Sagem: solder line from board :0, see posting xxx)
// PB22 = SMSYN2 = SD_CS = 0x0200 = SD_CARD Pin 1   =  18     x     ?   (Sagem: solder line from board :0, see posting xxx)
// GND  =        =       = Masse  = SD_Card Pin 3,6 =  10     3     2
// VCC  =        =       = 3,3V   = SD_Card Pin 4   =  16     5     x   (Philips: connect in series 3 Diodes (1N4007) from MODEM_CN 1 to SD/MMC card pin 4)
//
// Also connect a pullup resistor 100 KOhm from SD/MMC card pin 7 (SD_DO) to SD/MMC card pin 4 (VCC)
//
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

typedef unsigned int uint32;

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

#define NUMBER_OF_BURST_BYTES 512
#define SLOW_SPI_IO_CLK 1000
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// low-level driver 
unsigned char test_buffer_rx[NUMBER_OF_BURST_BYTES+10];
unsigned char test_buffer_tx[NUMBER_OF_BURST_BYTES+10];
// general port definition
#define PAPAR (((iop8xx_t *) &immap->im_ioport)->iop_papar)
#define PADIR (((iop8xx_t *) &immap->im_ioport)->iop_padir)
#define PAODR (((iop8xx_t *) &immap->im_ioport)->iop_paodr)
#define PADAT (((iop8xx_t *) &immap->im_ioport)->iop_padat)

#define PBPAR (((cpm8xx_t *) &immap->im_cpm)->cp_pbpar)
#define PBDIR (((cpm8xx_t *) &immap->im_cpm)->cp_pbdir)
#define PBODR (((cpm8xx_t *) &immap->im_cpm)->cp_pbodr)
#define PBDAT (((cpm8xx_t *) &immap->im_cpm)->cp_pbdat)

// Pin connection definition
#define SD_DO  0x0040 // GPIO A, on SD/MMC card pin 7
#define SD_DI  0x0080 // GPIO A, on SD/MMC card pin 2
#define SD_CLK 0x0100 // GPIO A, on SD/MMC card pin 5
#define SD_CS  0x0200 // GPIO B, on SD/MMC card pin 1

#define SMC_CLK_SET_HIGH (PADAT |=  SD_CLK )
#define SMC_CLK_SET_LOW  (PADAT &= ~SD_CLK )

#define SMC_DI_SET_HIGH  (PADAT |=  SD_DI  )
#define SMC_DI_SET_LOW   (PADAT &= ~SD_DI  )

#define SMC_CS_SET_HIGH  (PBDAT |=  SD_CS  )
#define SMC_CS_SET_LOW   (PBDAT &=  ~SD_CS  )

#define SMC_DO_IS_SET  ((PADAT & SD_DO)?0x01:0x00) 


// BIT-BANG driver 

// functions
static void mmc_spi_cs_low(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat &= ~(SD_CS);
}

static void mmc_spi_cs_high(void) {
  volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
  cp->cp_pbdat |= SD_CS;
}

static int mmc_hardware_init(void) {
	volatile cpm8xx_t *cp  = (cpm8xx_t *) &immap->im_cpm;
	volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;

	printk("mmc3: Hardware init\n");
    //          PA8     SMTXD2          (MMC2_DI)
	cpi->iop_papar &= ~(SD_DI);
	cpi->iop_padir |=   SD_DI;
    //          PA9     SMRXD2          (MMC2_DO)
	cpi->iop_papar &= ~(SD_DO);
	cpi->iop_paodr &= ~(SD_DO);
	cpi->iop_padir &=  ~SD_DO;
    //          PB22    SMSYN2          (MMC_CLK)
	cp->cp_pbpar &=   ~(SD_CS);
	cp->cp_pbodr &=   ~(SD_CS);
	cp->cp_pbdir |=    (SD_CS);
    //          PA7     CLK1,  BRGO1    (MMC_ DO)
	cpi->iop_papar &=   ~(SD_CLK);
	cpi->iop_papar &=   ~(SD_CLK);
	cpi->iop_papar |=    (SD_CLK);

	// Clock + CS low
	cpi->iop_padat &= ~(SD_CLK);
	cp->cp_pbdat   &= ~(SD_CS);   // CS low???
	//cp->cp_pbdat   |= (SD_CS);   // CS low???
	cpi->iop_padat &= ~SD_DI;
	return 0;
}

static unsigned char mmc_spi_io(unsigned char data_out) {
  volatile iop8xx_t *cpi = (iop8xx_t *) &immap->im_ioport;
  unsigned char result = 0;
  unsigned char  i;
  volatile int zzz=0;
  volatile int yyy=0;

  for(i = 0x80; i != 0; i >>= 1) 
  {
    if (data_out & i)
      cpi->iop_padat |= SD_DI;
    else
      cpi->iop_padat &= ~SD_DI;

    for(zzz=0;zzz<SLOW_SPI_IO_CLK;zzz++)yyy=zzz; // slow down processor before next clock
    cpi->iop_padat |= SD_CLK;
    if (cpi->iop_padat & SD_DO) 
    {
    	result |= i;
    }
    for(zzz=0;zzz<SLOW_SPI_IO_CLK;zzz++)yyy=zzz; // slow down processor before next clock
    cpi->iop_padat &= ~SD_CLK;
  }
  return result;
}


// SMC driver 
//#define SMC_ENABLED

//(((cpm8xx_t *) &immap->im_ioport)->iop_papar)
// DPRAM SMC

#define DPRAM_BASE (&immap->im_cpm.cp_dpmem)
#define RX_BD_STATUS  *((unsigned short*)(DPRAM_BASE +0x00))
#define RX_BD_LENGTH  *((unsigned short*)(DPRAM_BASE +0x02))
#define RX_BD_POINTER *((unsigned int*)  (DPRAM_BASE +0x04))
#define TX_BD_STATUS  *((unsigned short*)(DPRAM_BASE +0x10))
#define TX_BD_LENGTH  *((unsigned short*)(DPRAM_BASE +0x12))
#define TX_BD_POINTER *((unsigned int*)  (DPRAM_BASE +0x14))
// SMC Parameter RAM Memory MAP
#define SMC2_BASE ((&immap->im_cpm.cp_dparam)+0x0380) 
#define RBASE *((unsigned short*)(SMC2_BASE + 0x00))
#define TBASE *((unsigned short*)(SMC2_BASE + 0x02))
#define RFCR  *((unsigned char*) (SMC2_BASE + 0x04))
#define TFCR  *((unsigned char*) (SMC2_BASE + 0x05))
#define MRBLR *((unsigned short*)(SMC2_BASE + 0x06))
// Clock
#define BRGC1  (immap->im_cpm.cp_brgc1)
// Interrupts
#define CPCR   (immap->im_cpm.cp_cpcr)
#define CIMR   (immap->im_cpic.cpic_cimr)
#define CICR   (immap->im_cpm.cpic_cicr)
// SMC TRANSPARENT
#define SMCMR  (immap->im_cpm.cp_smc[1].smc_smcmr)
#define SMCM   (immap->im_cpm.cp_smc[1].smc_smcm)
#define SMCE   (immap->im_cpm.cp_smc[1].smc_smce)
#define SIMODE (immap->im_cpm.cp_simode)
#define SDCR   (immap->im_siu_conf.sc_sdcr)
////////////////////////
// Module definition
//#define SMC_ISR_ENABLED
#define SMC_TIMEOUT 999999

// SMC commands
#define SMC_IS_READ_BUSY    RX_BD_STATUS & 0x8000
#define SMC_SET_RX_BD_READY RX_BD_STATUS |= 0x8000

#define SMC_SET_SYNC        SMC_CS_SET_HIGH    
#define SMC_CLEAR_SYNC      SMC_CS_SET_LOW   

#define SMC_START_SMC_RX    SMCMR |=  0x0001
#define SMC_STOP_SMC_RX     SMCMR &= ~0x0001

#define SMC_START_CLOCK     BRGC1 |=  0x00010000
#define SMC_STOP_CLOCK      BRGC1 &= ~0x00010000

void mmc_smc_port_init(void)
{
    /* 1. Configure the port B pins to enable the SMTXD2, SMRXD2, and SMSYN2 pins. Write PAPAR/PBPAR bits 8, 9, and 22 with ones and then PADIR / PBDIR and PAODR / PBODR bits 8, 9, and 22 with zeros.*/
    //          PA8     SMTXD2          (MMC2_DI)
    //          PA9     SMRXD2          (MMC2_DO)
    PAPAR |=  (SD_DO | SD_DI);
    PAODR &= ~(SD_DO | SD_DI);
    PADIR &= ~(SD_DO | SD_DI);

    //          PB22    SMSYN2          (MMC_CLK)
    PBPAR |=  SD_CS;
    PBODR &= ~SD_CS;
    PBDIR &= ~SD_CS;
    //SMC_CLEAR_SYNC;   

    /* 2. Configure the port A pins to enable BRG. Write PAPAR bit 7 with a one and PADIR bit 7 with a one. */
    //          PA7     CLK1,  BRGO1    (MMC_ DO)
    PAPAR |=  SD_CLK;
    PADIR |=  SD_CLK;
    PAODR &= ~SD_CLK;
}

void mmc_smc_hardware_init(void)
{
    /* 3a. Configure the BRG1. Write 0x00000006 to BRGC1. The DIV16 bit is not used and the divider is 3 (= 22Mhz) . */
    //BRGC1 = 0x00000006;
    //BRGC1 = 0x00000101; // we start with a very slow bit rate 66Mhz/16/128  = 32kHz ;)
    BRGC1 = 0x00001001;   // we start with a very slow bit rate 66Mhz/16/2048 =  2kHz ;)

    /* 3b. Connect the BRG1 clock to SMC2 using the serial interface. Write the SMC2 bit in SIMODE with a 0 and the SMC2CS field in SIMODE register with 0x000. */
    SIMODE &= ~0xF0000000;

    /* 4. Write RBASE and TBASE in the SMCx parameter RAM to point to the RX buffer descriptor and TX buffer descriptor in the dual-port RAM. Assuming one RX buffer descriptor at the beginning of the dual-port RAM and one TX buffer descriptor following that RX buffer descriptor, write RBASE with 0x2000 and TBASE with 0x2008. */
    //RBASE = (unsigned short)(DPRAM_BASE +0x00);
    //TBASE = (unsigned short)(DPRAM_BASE +0x10);

    /* 5. Program the CPCR to execute the INIT RX AND TX PARAMS command of SMC2. Write 0x00D1 to the CPCR. */
    CPCR = 0x00D1;

    /* 6. Write 0x0001 to the SDCR to initialize the SDMA configuration register. */
    SDCR = 0x0001;

    /* 7. Write 0x18 to RFCR and TFCR for normal operation. */
    RFCR = 0x18;
    TFCR = 0x18;

    /* 8. Write MRBLR with the maximum number of bytes per receive buffer. Assume 512 bytes, so MRBLR = 0x0200. */
    MRBLR = NUMBER_OF_BURST_BYTES;

    /* 9. Initialize the RX buffer descriptor and assume the RX data buffer is at 0x00001000 in main memory. Write 0xB000 to RX_BD_Status, 0x0000 to RX_BD_Length (optional), and 0x00001000 to RX_BD_Pointer. */
    RX_BD_STATUS =  0x3000;
    RX_BD_LENGTH =  NUMBER_OF_BURST_BYTES;
    RX_BD_POINTER = (unsigned int)test_buffer_rx;

    /* 10.Initialize the TX buffer descriptor and assume the TX data buffer is at 0x00002000 in main memory and contains five 8-bit characters. Write 0xB800 to TX_BD_Status (ready, final buffer,interrupt, Last in Message), 0x0005 to TX_BD_Length, and 0x00002000 to TX_BD_Pointer. */
    TX_BD_STATUS  =  0x3800;
    TX_BD_LENGTH  =  NUMBER_OF_BURST_BYTES;
    TX_BD_POINTER =  (unsigned int)test_buffer_tx;

    /* 11.Write 0xFF to the SMCE-Transparent register to clear any previous events. */
    SMCE = 0xFF;

    /* 14.Write 0x3830 to the SMCMR to configure 8-bit characters, unreversed data, and normal operation (not loopback). Notice that the transmitter and receiver have not been enabled yet. */
    SMCMR = 0x3930;  // reverse mode!!!!   
    //SMCMR = 0x3830;  //    
}

int mmc_smc_read(char* buffer, unsigned int length)
{
    int timer = 0;
    
    MRBLR = length;
    // init rx buffer
    RX_BD_POINTER = (unsigned int)buffer;
    RX_BD_LENGTH  = length;
    SMC_SET_RX_BD_READY;  // ready buffer    

    SMC_SET_SYNC;
    SMC_START_SMC_RX;
    SMC_START_CLOCK;
    SMC_CLEAR_SYNC; // SMC does start on the sync edge, so we could clear it aftwards anytime

    while ( SMC_IS_READ_BUSY && timer++ < SMC_TIMEOUT);   // wait for 

    SMC_STOP_CLOCK;
    SMC_STOP_SMC_RX;
    
    return (timer);
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// High-level driver 
static int mmc_write_block(unsigned int dest_addr, unsigned char *data)
{
	unsigned int address;
	unsigned char r = 0;
	unsigned char ab0, ab1, ab2, ab3;
	int i;

	address = dest_addr;

	ab3 = 0xff & (address >> 24);
	ab2 = 0xff & (address >> 16);
	ab1 = 0xff & (address >> 8);
	ab0 = 0xff & address;
	mmc_spi_cs_low();
	for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x58);
	mmc_spi_io(ab3); /* msb */
	mmc_spi_io(ab2);
	mmc_spi_io(ab1);
	mmc_spi_io(ab0); /* lsb */
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
		return(1);
	}

	mmc_spi_io(0xfe);
	for (i = 0; i < 512; i++) mmc_spi_io(data[i]);
	for (i = 0; i < 2; i++) mmc_spi_io(0xff);

	for (i = 0; i < 1000000; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0xff) break;
	}
	if (r != 0xff)
	{
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return(3);
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	return(0);
}

static int mmc_read_block(unsigned char *data, unsigned int src_addr)
{
	unsigned char r = 0;
	int i;

	mmc_spi_cs_low();
	for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x51);
	mmc_spi_io(0xff & (src_addr >> 24)); /* msb */
	mmc_spi_io(0xff & (src_addr >> 16));
	mmc_spi_io(0xff & (src_addr >> 8));
	mmc_spi_io(0xff & src_addr); /* lsb */

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
		return(1);
	}
	for (i = 0; i < 100000; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0xfe) break;
	}
	if (r != 0xfe)
	{
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return(2);
	}

#if SMC_ISR_ENABLED
    mmc_smc_port_init(); // Init HW for SMC
    i=mmc_smc_read(data,NUMBER_OF_BURST_BYTES);
    mmc_hardware_init(); // Init HW for bit banging 
#else
	for (i = 0; i < 512; i++) data[i] = mmc_spi_io(0xff);
#endif
    printk("RX t:%6d,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,
            data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9]);

	for (i = 0; i < 2; i++) mmc_spi_io(0xff);

	mmc_spi_cs_high();
	mmc_spi_io(0xff);

	return(0);
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
					printk("mmc: error in mmc_read_block (%d)\n", rc);
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
					printk("mmc: error in mmc_write_block (%d)\n", rc);
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

extern struct gendisk hd_gendisk;

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

static int mmc_card_init(void)
{
	unsigned char r = 0;
	short i, j;
	unsigned long flags;

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
	major:		MAJOR_NR,
	major_name:	DEVICE_NAME,
	minor_shift:	6,
	max_p:		1 << 6,
	part:		hd,
	sizes:		hd_sizes,
	fops:		&mmc_bdops,
};

static int mmc_init(void)
{
	int rc;

#ifdef SMC_ENABLED    
    mmc_smc_hardware_init();
#endif    
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
			return -1;
		}
	}

	memset(hd_sizes, 0, sizeof(hd_sizes));
	rc = mmc_card_config();
	if ( rc != 0)
	{
		printk("mmc: error in mmc_card_config (%d)\n", rc);
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
			if (rc != 0) printk("mmc: error in mmc_init (%d)\n", rc);
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

static int __init mmc_driver_init(void)
{
	int rc;

    printk("------ MMC3test SMC Driver Version $Id: mmc3test.c,v 1.1 2006/07/28 12:09:55 guenther Exp $\n");
    printk("------ optimized SMC version \n");
    printk("------ Instable version, use for development only !!!! \n");
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
	del_timer(&mmc_timer);

	for (i = 0; i < (1 << 6); i++)
		fsync_dev(MKDEV(MAJOR_NR, i));

	devfs_register_partitions(&hd_gendisk, 0<<6, 1);
	devfs_unregister_blkdev(MAJOR_NR, DEVICE_NAME);
	del_gendisk(&hd_gendisk);
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	mmc_exit();
	printk("removing mmc3.o\n");
}

module_init(mmc_driver_init);
module_exit(mmc_driver_exit);
