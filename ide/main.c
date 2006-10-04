/*
 * $Id: main.c,v 1.10 2006/10/04 00:36:41 carjay Exp $
 *
 * Copyright (C) 2006 Uli Tessel <utessel@gmx.de>
 * Linux 2.6 port: Copyright (C) 2006 Carsten Juttner <carjay@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; (version 2 of the License)
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/ide.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/irq.h>
#include <asm/8xx_immap.h>
#include <asm/commproc.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/device.h>
#include <linux/platform_device.h>
#endif

static uint idebase = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct platform_device *ide_dev;
#else
static int ideindex = -1;
#endif

/* address-offsets of features in the CPLD 
  (we can't call them registers...) */
#define CPLD_READ_DATA          0x00000000
#define CPLD_READ_FIFO          0x00000A00
#define CPLD_READ_CTRL          0x00000C00

#define CPLD_WRITE_FIFO         0x00000980
#define CPLD_WRITE_FIFO_HIGH    0x00000900
#define CPLD_WRITE_FIFO_LOW     0x00000880
#define CPLD_WRITE_CTRL_TIMING  0x00000860
#define CPLD_WRITE_CTRL         0x00000840
#define CPLD_WRITE_TIMING       0x00000820
#define CPLD_WRITE_DATA         0x00000800

/* bits in the control word */
#define CPLD_CTRL_WRITING 0x20
#define CPLD_CTRL_ENABLE  0x40
#define CPLD_CTRL_REPEAT  0x80

/* helping macros to access the CPLD */
#define CPLD_OUT(offset, value) ( *(volatile uint*)(idebase+(offset)) = (value))
#define CPLD_IN(offset) ( *(volatile uint*)(idebase+(offset)))
#define CPLD_FIFO_LEVEL() (CPLD_IN( CPLD_READ_CTRL)>>28)

/* hidden(?) function of linux ide part */
extern void ide_probe_module(int);

/* assembler implementation of transfer loops */
extern void dboxide_insw_loop(uint ctrl_address,
			      uint data_address, void *dest, int count);

extern void dboxide_outsw_loop(uint ctrl_address,
			       uint data_address, void *src, int count);

/* trace routines */
extern void dboxide_log_trace(unsigned int typ, unsigned int a, unsigned int b);
extern void dboxide_print_trace(void);

#define TRACE_LOG_INB   0x01
#define TRACE_LOG_INW   0x02
#define TRACE_LOG_INSW  0x03
#define TRACE_LOG_OUTB  0x04
#define TRACE_LOG_OUTW  0x05
#define TRACE_LOG_OUTSW 0x06

const char *dboxide_trace_msg[] = {
	NULL,
	"INB  ",
	"INW  ",
	"INSW ",
	"OUTB ",
	"OUTW ",
	"OUTSW",
};

/* some functions are not implemented and I don't expect we ever
   need them. But if one of them is called, we can work on that. */
#define NOT_IMPL(txt, a...) printk( "dboxide: NOT IMPLEMENTED: "txt, ##a )

/* whenever a something didn't work as expected: print everything
   that might be interesting for the developers what has happened */
void dboxide_problem(const char *msg)
{
	printk("dboxide: %s\n", msg);
	printk("CPLD Status is %08x\n", CPLD_IN(CPLD_READ_CTRL));
	dboxide_print_trace();
}

/* compat code for 2.6 kernel */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define IDE_DELAY() ide_delay_50ms()
#else
#define IDE_DELAY() \
	do { \
		__set_current_state(TASK_UNINTERRUPTIBLE); \
		schedule_timeout(1+HZ/20); \
	} while (0);
#endif

#define WAIT_FOR_FIFO_EMPTY() wait_for_fifo_empty()
#define MAX_WAIT_FOR_FIFO_EMPTY  1000
static void wait_for_fifo_empty(void)
{
	int cnt = MAX_WAIT_FOR_FIFO_EMPTY;
	uint level;

	do {
		cnt--;
		level = CPLD_FIFO_LEVEL();
	} while ((level != 0) && (cnt > 0));

	if (cnt <= 0)
		dboxide_problem("fifo didn't get empty in time");
}

/*---------------------------------------------------------*/
/* These functions are called via function pointer by the  */
/* IDE Subsystem of the Linux Kernel                       */
/*---------------------------------------------------------*/

/* inb reads one byte from an IDE Register */
static u8 dboxide_inb(unsigned long port)
{
	int val;

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("inb: fifo not empty?!\n");

	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE);

	while (CPLD_FIFO_LEVEL() == 0) {
	};

	val = CPLD_IN(CPLD_READ_FIFO);

	val >>= 8;
	val &= 0xFF;

	dboxide_log_trace(TRACE_LOG_INB, port, val);

	return val;
}

/* inw reads one word from an IDE Register
   As only the data register has 16 bit, and that is read
   with insw, this function might never be called */
static u16 dboxide_inw(unsigned long port)
{
	int val;

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("inw: fifo not empty?!");

	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE);

	while (CPLD_FIFO_LEVEL() == 0) {
	};

	val = CPLD_IN(CPLD_READ_FIFO);

	val &= 0xFFFF;

	dboxide_log_trace(TRACE_LOG_INW, port, val);

	return val;
}

/* insw reads several words from an IDE register.
   Typically from the data register. This is the most important
   function to read data */
static void dboxide_insw(unsigned long port, void *addr, u32 count)
{
	uint *dest = addr;

	/* special for ATAPI: the kernel calls insw with count=1?! */
	if (count<4)
	{
		short * sdest = addr;
		/*printk("dboxide: short insw %d\n", (int)count);*/
		for (;count>0;count--)
		{
			*sdest++ = dboxide_inw( port ); 
		}
		return;
	}


	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("insw: fifo not empty?!");
	dboxide_log_trace(TRACE_LOG_INSW, port, count);

	/* activate reading to fifo with auto repeat */
	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE | CPLD_CTRL_REPEAT);

	/* todo: replace the code below by an assembler implementation in this
	   routine */
	dboxide_insw_loop(idebase + CPLD_READ_CTRL, idebase + CPLD_READ_FIFO,
			  dest, count);

	{
		register uint a;
		register uint b;
		register uint c;
		register uint d;

		while (count > 16) {
			while (CPLD_FIFO_LEVEL() != 0xF) {
			};
			a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			while (CPLD_FIFO_LEVEL() != 0xF) {
			};
			c = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			d = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			dest[0] = a;
			dest[1] = b;
			dest[2] = c;
			dest[3] = d;
			count -= 8;
			dest += 4;
		}

		while (count > 4) {
			while (CPLD_FIFO_LEVEL() != 0xF) {
			};
			a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			dest[0] = a;
			dest[1] = b;
			count -= 4;
			dest += 2;
		}

	}

	if (count != 4)
		printk
		    ("dboxide: oops: insw: something has gone wrong: count is %d\n",
		     count);

	/* wait until fifo is full = 4 Words */
	while (CPLD_FIFO_LEVEL() != 0xF) {
	};

	/* then stop reading from ide */
	CPLD_OUT(CPLD_WRITE_CTRL, port);

	/* and read the final 4 16 bit words */
	dest[0] = CPLD_IN(CPLD_READ_FIFO);
	dest[1] = CPLD_IN(CPLD_READ_FIFO);
}

/* insl reads several 32 bit words from an IDE register.
   The IDE Bus has only 16 bit words, but the CPLD always
   generates 32 Bit words from that, so the same routine
   as for 16 bit can be used. */
static void dboxide_insl(unsigned long port, void *addr, u32 count)
{
	dboxide_insw(port, addr, count * 2);
}

/* inl: read a single 32 bit word from IDE. 
   As there are no 32 Bit IDE registers, this function
   is not implemented. */
static u32 dboxide_inl(unsigned long port)
{
	NOT_IMPL("inl %lx\n", port);
	return 0xFFFFFFFF;
}

/* outb: write a single byte to an IDE register */
static void dboxide_outb(u8 value, unsigned long port)
{
	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("outb: fifo not empty?!");

	dboxide_log_trace(TRACE_LOG_OUTB, port, value);

	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE | CPLD_CTRL_WRITING);
	CPLD_OUT(CPLD_WRITE_FIFO_LOW, value << 8);

	WAIT_FOR_FIFO_EMPTY();
}

/* outbsync: write a single byte to an IDE register, typically
   an IDE command. */
/* todo: the sync is related to interrupts */
static void dboxide_outbsync(ide_drive_t * drive, u8 value, unsigned long port)
{
	/* todo: use a different cycle-length here?! */
	dboxide_outb(value, port);
}

/* outw: write a single 16-bit word to an IDE register. 
   As only the data register hast 16 bit, and that is written 
   with outsw, this function is not implemented. */
static void dboxide_outw(u16 value, unsigned long port)
{
	NOT_IMPL("outw %lx %x\n", port, value);
}

/* outl: write a single 32-bit word to an IDE register. 
   As there are no 32 Bit IDE registers, this function 
   is not implemented. */
static void dboxide_outl(u32 value, unsigned long port)
{
	NOT_IMPL("outl %lx %lx\n", port, port);
}

/* write several 16 bit words to an IDE register, typically to the
   data register. 
   This is the most important function to write data */
static void dboxide_outsw(unsigned long port, void *addr, u32 count)
{
	uint *src = addr;

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("outsw: fifo not empty?!");

	dboxide_log_trace(TRACE_LOG_OUTSW, port, count);

	/* activate writing to fifo with auto repeat */
	CPLD_OUT(CPLD_WRITE_CTRL,
		 port | CPLD_CTRL_WRITING | CPLD_CTRL_ENABLE |
		 CPLD_CTRL_REPEAT);

	dboxide_outsw_loop(idebase + CPLD_READ_CTRL, idebase + CPLD_READ_FIFO,
			   src, count);

	{
		register int a;
		register int b;

		while (count > 0) {
			a = *src++;
			b = *src++;
			while (CPLD_FIFO_LEVEL() != 0) {
			};
			CPLD_OUT(CPLD_WRITE_FIFO, a);
			CPLD_OUT(CPLD_WRITE_FIFO, b);

			count -= 4;
		}

		if (count == 2) {
			a = *src++; 
			while (CPLD_FIFO_LEVEL()!=0) {
			};
			CPLD_OUT( CPLD_WRITE_FIFO, a );
		}
	}

	WAIT_FOR_FIFO_EMPTY();

	/* and stop writing to IDE */
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_WRITING);
}

/* outsl writes several 32 bit words to an IDE register. 
   The IDE Bus has only 16 bit words, but the CPLD always 
   generates 32 Bit words from that, so the same routine
   as for 16 bit can be used. */
static void dboxide_outsl(unsigned long port, void *addr, u32 count)
{
	dboxide_outsw(port, addr, count * 2);
}

/*---------------------------------------------------------*/
/* acknowledge the interrupt? */
/*---------------------------------------------------------*/

int dboxide_ack_intr(ide_hwif_t * hwif)
{
	printk("dboxide: ack irq\n");
	return 1;
}

/*---------------------------------------------------------*/
/* some other functions that might be important, but it    */
/* also works without them                                 */
/*---------------------------------------------------------*/

void dboxide_tuneproc(ide_drive_t * drive, u8 pio)
{
	printk("dboxide: tuneproc called: %d\n", pio);
}

/*---------------------------------------------------------*/
/* end of functions called via function pointer */
/*---------------------------------------------------------*/

static int configure_interrupt(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;

	/* configure Port C, Pin 15 so, that it creates interrupts 
	   only on the falling edge. (That it will create interrupts
	   at all is done by the Kernel itself) */

	immap->im_ioport.iop_pcint |= 0x0001;
	/* As this routine is the only one that needs to know about
	   wich interrupt is used in this code, it returns the number
	   so it can be given to the kernel */
	return CPM_IRQ_OFFSET + CPMVEC_PIO_PC15;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void reset_function_pointer(ide_hwif_t *hwif)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hwif->hw.io_ports); i++) {
		uint addr;

		addr = hwif->hw.io_ports[i];
		if ((addr > idebase) && (addr <= idebase + 0x100))
			hwif->hw.io_ports[i] = addr - idebase;

		addr = hwif->io_ports[i];
		if ((addr > idebase) && (addr <= idebase + 0x100))
			hwif->io_ports[i] = addr - idebase;
	}
}
#endif

/* set the function pointer in the kernel structur to our
   functions */
static void set_access_functions(ide_hwif_t * hwif)
{
	hwif->mmio = 2;
	hwif->OUTB = dboxide_outb;
	hwif->OUTBSYNC = dboxide_outbsync;
	hwif->OUTW = dboxide_outw;
	hwif->OUTL = dboxide_outl;
	hwif->OUTSW = dboxide_outsw;
	hwif->OUTSL = dboxide_outsl;
	hwif->INB = dboxide_inb;
	hwif->INW = dboxide_inw;
	hwif->INL = dboxide_inl;
	hwif->INSW = dboxide_insw;
	hwif->INSL = dboxide_insl;

	hwif->tuneproc = dboxide_tuneproc;

#if 0
	hwif->ack_intr = dboxide_ack_intr;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* now, after setting the function pointer, the port info is not
	   an address anymore, so remove the idebase again */
	reset_function_pointer(hwif);
#endif

}

/* common function to setup the io_ports. "idebase" is necessary
   for the 2.4 kernel since the first time the ports are
   used as addresses which is not what we really want so the
   initial address is different for 2.4 and 2.6. */
static void init_hw_struct( ide_hwif_t *hwif, unsigned long idebase)
{
	hw_regs_t *hw;

	hwif->chipset = ide_unknown;
	hwif->tuneproc = NULL;
	hwif->mate = NULL;
	hwif->channel = 0;

	strncpy (hwif->name, "dbox2 ide", sizeof(hwif->name));

	hw = &hwif->hw;
	memset(hw, 0, sizeof(hw_regs_t));

	hw->io_ports[IDE_DATA_OFFSET] 		= idebase + 0x0010;
	hw->io_ports[IDE_ERROR_OFFSET] 		= idebase + 0x0011;
	hw->io_ports[IDE_NSECTOR_OFFSET] 	= idebase + 0x0012;
	hw->io_ports[IDE_SECTOR_OFFSET] 	= idebase + 0x0013;
	hw->io_ports[IDE_LCYL_OFFSET] 		= idebase + 0x0014;
	hw->io_ports[IDE_HCYL_OFFSET] 		= idebase + 0x0015;
	hw->io_ports[IDE_SELECT_OFFSET] 	= idebase + 0x0016;
	hw->io_ports[IDE_STATUS_OFFSET] 	= idebase + 0x0017;

	hw->io_ports[IDE_CONTROL_OFFSET] 	= idebase + 0x004E;
	hw->io_ports[IDE_IRQ_OFFSET] 		= idebase + 0x004E;

	hwif->irq = hw->irq = configure_interrupt();

	memcpy(hwif->io_ports, hw->io_ports, sizeof(hw->io_ports));
}

/* the CPLD is connected to CS2, which should be inactive.
   if not there might be something using that hardware and
   we don't want to disturb that */
static int activate_cs2(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;
	memctl8xx_t *memctl = &immap->im_memctl;
	uint br2 = memctl->memc_br2;

	if (br2 & 0x1) {
		printk("dboxide: cs2 already activated\n");
		return 0;
	}

	if (br2 != 0x02000080) {
		printk("dboxide: cs2: unexpected value for br2: %08x\n", br2);
		return 0;
	}

	br2 |= 0x1;

	printk("dboxide: activating cs2\n");
	memctl->memc_br2 = br2;

	return 1;
}

/* Deactivate CS2 when driver is not loaded */
static int deactivate_cs2(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;
	memctl8xx_t *memctl = &immap->im_memctl;
	uint br2 = memctl->memc_br2;

	if (br2 != 0x02000081) {
		printk("dboxide: cs2 configuration unexpected: %08x\n", br2);
		return 0;
	}

	br2 &= ~1;

	printk("dboxide: deactivating cs2\n");
	memctl->memc_br2 = br2;

	return 1;
}

/* detect_cpld: Check that the CPLD really works */
static int detect_cpld(void)
{
	int i;
	uint check, back;
	uint patterns[2] = { 0xCAFEFEED, 0xBEEFC0DE };

	/* This detection code not only checks that there is a CPLD,
	   but also that it does work more or less as expected.  */

	/* first perform a walking bit test via data register:
	   this checks that there is a data register and
	   that the data bus is correctly connected */

	for (i = 0; i < 31; i++) {
		/* only one bit is 1 */
		check = 1 << i;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			printk
			    ("dboxide: probing dbox2 IDE CPLD: walking bit test failed: %08x != %08x\n",
			     check, back);
			return 0;
		}

		/* only one bit is 0 */
		check = ~check;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			printk
			    ("dboxide: probing dbox2 IDE CPLD: walking bit test failed: %08x != %08x\n",
			     check, back);
			return 0;
		}
	}

	/* second: check ctrl register.
	   this also activates the IDE Reset. */
	check = 0x00FF0007;
	CPLD_OUT(CPLD_WRITE_CTRL_TIMING, check);
	back = CPLD_IN(CPLD_READ_CTRL);
	if ((back & check) != check) {
		printk
		    ("dboxide: probing dbox2 IDE CPLD: ctrl register not valid: %08x != %08x\n",
		     check, back & check);
		return 0;
	}

	/* Now test the fifo:
	   If there is still data inside, read it out to clear it */
	for (i = 3; (i > 0) && ((back & 0xF0000000) != 0); i--) {
		CPLD_IN(CPLD_READ_FIFO);
		back = CPLD_IN(CPLD_READ_CTRL);
	}

	if (i == 0) {
		printk
		    ("dboxide: fifo seems to have data but clearing did not succeed\n");
		return 0;
	}

	/* then write two long words to the fifo */
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[0]);
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[1]);

	/* and read them back */
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[0]) {
		printk("dboxide: fifo did not store first test pattern\n");
		return 0;
	}
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[1]) {
		printk("dboxide: fifo did not store second test pattern\n");
		return 0;
	}

	/* now the fifo must be empty again */
	back = CPLD_IN(CPLD_READ_CTRL);
	if ((back & 0xF0000000) != 0) {
		printk("dboxide: fifo not empty after test\n");
		return 0;
	}

	/* Clean up: clear bits in fifo */
	check = 0;
	CPLD_OUT(CPLD_WRITE_FIFO, check);
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != check) {
		printk("dboxide: final fifo clear did not work: %x!=%x\n", back,
		       check);
		return 0;
	}

	/* CPLD is valid!
	   Hopefully the IDE part will also work:
	   A test for that part is not implemented, but the kernel
	   will probe for drives etc, so this will check a lot
	 */

	/* before going releasing IDE Reset, wait some time... */
	for (i = 0; i < 10; i++)
		IDE_DELAY();

	/* Activate PIO Mode 4 timing and remove IDE Reset */
	CPLD_OUT(CPLD_WRITE_CTRL_TIMING, 0x0012001F);

	/* finally set all bits in data register, so nothing
	   useful is read when the CPLD is accessed by the
	   original inb/w/l routines */
	CPLD_OUT(CPLD_WRITE_DATA, 0xFFFFFFFF);

	return 1;
}

/* map_memory: we know the physical address of our chip. 
   But the kernel has to give us a virtual address. */
static void map_memory(void)
{
	unsigned long mem_addr = 0x02000000;
	unsigned long mem_size = 0x00001000;
	/* ioremap also activates the guard bit in the mmu, 
	   so the MPC8xx core does not do speculative reads
	   to these addresses
	 */

	idebase = (uint) ioremap(mem_addr, mem_size);
}

/* unmap_memory: we will not use these virtual addresses anymore */
static void unmap_memory(void)
{
	if (idebase) {
		iounmap((uint *) idebase);
		idebase = 0;
	}
}

static int setup_cpld(void)
{
	if (activate_cs2() == 0)
		return -ENODEV;

	map_memory();

	if (idebase == 0) {
		printk(KERN_ERR "dboxide: address space of dbox2 IDE CPLD not mapped to kernel address space\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "dboxide: address space of DBox2 IDE CPLD is at: 0x%08x\n", idebase);

	if (detect_cpld() == 0) {
		printk(KERN_ERR "dboxide: not a valid dbox2 IDE CPLD detected\n");
		unmap_memory();
		return -ENODEV;
	}
	
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void dboxide_register(void)
{
	ide_hwif_t *hwif = NULL;
	ide_hwif_t tmp_hwif;	/* we only use the hw_ports stucture */

	/*
	   I think this is a hack, but it works...

	   the io_port values are not really used as addresses by
	   this driver and, of course, they will not really work if
	   used like that.
	   "Not really" because the kernel might use the default
	   inb/etc. routines if the detect module is already loaded.
	   To avoid problems with that I use addresses where I know
	   what happens, because these addresses access the CPLD:

	   Writing to these addresses does nothing, and reading will
	   return the data register, which is (should be) at this
	   time FFFFFFFF  (see cpld_detect).

	   The result is that the detection of a disk will fail when
	   it is done with the original kernel functions.

	   A clean order is to load the ide-detect module after
	   loading this module!
	 */
	 
	/* set up with "fake" ioport addresses (that do not fault) */
	init_hw_struct(&tmp_hwif, idebase);

	/* we register the ports and get back a pointer to
	   the hwif that we are supposed to use */
	ideindex = ide_register_hw(&tmp_hwif.hw, &hwif);

	if (hwif != NULL) {
		if (ideindex == -1) {
			/* registering failed? This is not wrong because for the
			   kernel there is no drive on this controller because
			   wrong routines were used to check that.
			   Or the kernel didn't check at all.
			   But to unregister this driver, we will need this
			   index. */

			ideindex = hwif - ide_hwifs;
		}

		/* now change the IO Access functions and use the
		   real values for the IDE Ports */
		set_access_functions(hwif);

		SELECT_DRIVE(&hwif->drives[0]);

		/* finally: probe again: this time with my routines,
		   so this time the detection will not fail (if there
		   is a drive connected) */
		ide_probe_module(1);

	} else {
		printk("dboxide: no hwif was given\n");
	}
}

/* dboxide_scan: this is called by the IDE part of the kernel via
   a function pointer when the kernel thinks it is time to check
   this ide controller. So here the real activation of the CPLD
   is done (using the functions above) */
static void dboxide_scan(void)
{
	/*  
	   todo: If u-boot uses the IDE CPLD, then it should remove the CS2 activation 
	   or it should tell us somehow, so we can skip the detection...
	 */

	/* check BR2 register that CS2 is enabled.  if so, then something has activated it. 
	   Maybe there is RAM or a different hardware?  Don't try to use the CPLD then.  */
	int ret;
	
	ret = setup_cpld();
	if (ret)
		return;
	
	dboxide_register();
}

/* dbox_ide_init is called when the module is loaded */
static int __init dboxide_init(void)
{
	/* register driver will call the scan function above, maybe immediately 
	   when we are a module, or later when it thinks it is time to do so */
	printk(KERN_INFO
	       "dboxide: $Id: main.c,v 1.10 2006/10/04 00:36:41 carjay Exp $\n");

	ide_register_driver(dboxide_scan);

	return 0;
}

/* dbox_ide_exit is called when the module is unloaded */
static void __exit dboxide_exit(void)
{
	if (idebase != 0) {
		CPLD_OUT(CPLD_WRITE_CTRL_TIMING, 0x00FF0007);
	}

	idebase = 0;

	if (ideindex != -1)
		ide_unregister(ideindex);

	unmap_memory();
	deactivate_cs2();

	printk("dboxide: driver unloaded\n");
}
#else
static int dbox2_ide_probe(struct platform_device *pdev)
{
	ide_hwif_t *hwif = &ide_hwifs[pdev->id];

	init_hw_struct(hwif, 0);
	set_access_functions(hwif);

	hwif->drives[pdev->id].hwif = hwif;
	SELECT_DRIVE(&hwif->drives[pdev->id]);

	/* tell kernel to the interface
		(the default is not to probe) */
	hwif->noprobe = 0;
	probe_hwif_init(hwif);

	platform_set_drvdata(pdev, hwif);
	
	return 0;
}

static int dbox2_ide_remove(struct platform_device *pdev)
{
	ide_hwif_t *hwif = platform_get_drvdata(pdev);

	ide_unregister(hwif - ide_hwifs);
	
	return 0;
}

static struct platform_driver dbox2_ide_driver = {
	.driver {
		.name = "dbox2_ide"
	},
	.probe 	= dbox2_ide_probe,
	.remove = dbox2_ide_remove
};

/* dbox_ide_init is called when the module is loaded */
static int __init dboxide_init(void) {
	int ret;

	printk(KERN_INFO
	       "dboxide: $Id: main.c,v 1.10 2006/10/04 00:36:41 carjay Exp $\n");

	ret = setup_cpld();
	if (ret < 0)
		return ret;

	/* register the driver */
	ret = platform_driver_register(&dbox2_ide_driver);
	if (ret < 0)
		return ret;
	
	/* "insert" the corresponding device to trigger the probe */
	ide_dev = platform_device_register_simple("dbox2_ide", 0, NULL, 0);
	if (IS_ERR(ide_dev)) {
		platform_driver_unregister(&dbox2_ide_driver);
		return PTR_ERR(ide_dev);
	}
	
	return 0;
}

/* dbox_ide_exit is called when the module is unloaded */
static void __exit dboxide_exit(void)
{
	if (idebase != 0)
		CPLD_OUT(CPLD_WRITE_CTRL_TIMING, 0x00FF0007);

	if (ide_dev)
		platform_device_unregister(ide_dev);

	platform_driver_unregister(&dbox2_ide_driver);

	unmap_memory();
	deactivate_cs2();

	printk("dboxide: driver unloaded\n");
}
#endif

module_init(dboxide_init);
module_exit(dboxide_exit);

MODULE_AUTHOR("Uli Tessel <utessel@gmx.de>");
MODULE_DESCRIPTION("DBOX IDE CPLD Interface driver");
MODULE_LICENSE("GPL");
