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

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia x00 Driver");

volatile u8 *aviamem;
u32 *microcode;
int aviarev;

static int dev;

#define TM_DRAM  0x00
#define TM_GBUS  0x80
#define TM_SRAM  0xC0

#define AVIA_INTERRUPT         SIU_IRQ4

                /* finally i got them */
#define UX_MAGIC                        0x00
#define UX_NUM_SECTIONS                 0x01
#define UX_LENGTH_FILE                  0x02
#define UX_FIRST_SECTION_START          0x03
#define UX_SECTION_LENGTH_OFFSET        0x01
#define UX_SECTION_WRITE_ADDR_OFFSET    0x02
#define UX_SECTION_CHECKSUM_OFFSET      0x03
#define UX_SECTION_DATA_OFFSET          0x04
#define UX_SECTION_DATA_GBUS_TABLE      0x02FF
#define UX_SECTION_DATA_IMEM            0x0200
#define UX_SECTION_HEADER_LENGTH        0x04

#define NTSC_16MB_WO_ROM_SRAM   7
#define NTSC_16MB_PL_ROM_SRAM   9
#define PAL_16MB_WO_ROM_SRAM    10
#define PAL_20MB_WO_ROM_SRAM    12

u32 avia_rd(int mode, int address)
{
  int data=0;
  address&=0x3FFFFF;
  aviamem[6]=((address>>16)|mode)&0xFF;
  aviamem[5]=(address>>8)&0xFF;
  aviamem[4]=address&0xFF;
  data=aviamem[3]<<24;
  data|=aviamem[2]<<16;
  data|=aviamem[1]<<8;
  data|=aviamem[0];
  return data;
}

void avia_wr(int mode, u32 address, u32 data)
{
  address&=0x3FFFFF;
  aviamem[6]=((address>>16)|mode)&0xFF;
  aviamem[5]=(address>>8)&0xFF;
  aviamem[4]=address&0xFF;
  aviamem[3]=(data>>24)&0xFF;
  aviamem[2]=(data>>16)&0xFF;
  aviamem[1]=(data>>8)&0xFF;
  aviamem[0]=data&0xFF;
}
        
#define wGB(a, d) avia_wr(TM_GBUS, a, d)
#define rGB(a) avia_rd(TM_GBUS, a)
#define wSR(a, d) avia_wr(TM_SRAM, a, d)
#define rSR(a) avia_rd(TM_SRAM, a)
#define wDR(a, d) avia_wr(TM_DRAM, a, d)
#define rDR(a) avia_rd(TM_DRAM, a)

inline void wIM(u32 addr, u32 data)
{
  wGB(0x36, addr);
  wGB(0x34, data);
}


inline u32 rIM(u32 addr)
{
  wGB(0x3A, 0x0B);
  wGB(0x3B, addr);
  wGB(0x3A, 0x0E);
  return rGB(0x3B);
}

void InitialGBus(void)
{
  unsigned long *ptr=((unsigned long*)microcode)+0x306;
  int words=*ptr--, data, addr;
  printk("performing %d initial G-bus Writes. (don't panic! ;)\n", words);
  while (words--)
  {
    addr=*ptr--;
    data=*ptr--;
    wGB(addr, data);
  }
}

void FinalGBus(void)
{
  unsigned long *ptr=((unsigned long*)microcode)+0x306;
  int words=*ptr--, data, addr;
  *ptr-=words;
  ptr-=words*4;
  words=*ptr--;
  printk("performing %d final G-bus Writes.\n", words);
  while (words--)
  {
    addr=*ptr--;
    data=*ptr--;
    wGB(addr, data);
  }
}

unsigned long endian_swap(unsigned long v)
{
  return ((v&0xFF)<<24)|((v&0xFF00)<<8)|((v&0xFF0000)>>8)|((v&0xFF000000)>>24);
}

void dram_memcpyw(u32 dst, u32 *src, int words)
{
  while (words--)
  {
    wDR(dst, *src++);
    dst+=4;
  }
}

void load_dram_image(u32 section_start)
{
  u32 dst, *src, words, errors=0;
  words=endian_swap(microcode[section_start+UX_SECTION_LENGTH_OFFSET])/4;
  dst=endian_swap(microcode[section_start+UX_SECTION_WRITE_ADDR_OFFSET]);
  src=microcode+section_start+UX_SECTION_DATA_OFFSET;
//  printk("processing section at %x: src: %x, dst: %x, words: %x\n", section_start, src-microcode, dst, words);
  dram_memcpyw(dst, src, words);
  while (words--)
  {
    if (rDR(dst)!=*src++)
      errors++;
    dst+=4;
  }
  if (errors)
    printk("ERROR: microcode verify: %d errors.\n", errors);
}

void load_imem_image(u32 data_start)
{
  u32 *src, i, words, errors=0;
  src=microcode+data_start+UX_SECTION_DATA_IMEM;
  words=256;
  for (i=0; i<words; i++)
    wIM(i, src[i]);
  wGB(0x3A, 0xB);               // CPU_RMADR2
  wGB(0x3B, 0);                 // set starting address
  wGB(0x3A, 0xE);               // indirect regs
  for (i=0; i<words; i++)
    if (rGB(0x3B)!=src[i])
      errors++;
  if (errors)
    printk("ERROR: imem verify: %d errors\n", errors);
} 

void LoaduCode(void)
{
  int num_sections=endian_swap(microcode[1]);
  int data_offset=3;
  
  printk("%d sections in microcode.\n", num_sections);
  
  while (num_sections--)
  {
    load_dram_image(data_offset);
    if (num_sections)
      data_offset+=(endian_swap(microcode[data_offset+UX_SECTION_LENGTH_OFFSET])/4)+UX_SECTION_HEADER_LENGTH;
  }
}

extern u8 a500v093[];
extern u8 a600vb017[];

static int intmask;

void avia_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
  int status=rDR(0x2AC);
  
  printk("avia_interrupt %x\n", status);
  
  wGB(0, rGB(0)&~1);
  wDR(0x2AC, 0); // ACK
  return;
}


u32 Command(u32 command, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6)
{
  u32 stataddr, tries;
  printk("waiting for avia ready.\n");
                // TODO: kernel lock (somewhat DRINGEND wenn wir mal irgendwann .. und so weiter)
  tries=1000;
  while (!rDR(0x5C)) { udelay(1000); if (! (tries--)) { printk("timeout.\n"); return -1; } }
  
  wDR(0x40, command);
  wDR(0x44, a1);
  wDR(0x48, a2);
  wDR(0x4c, a3);
  wDR(0x50, a4);
  wDR(0x54, a5);
  wDR(0x58, a6);

  printk("executing command.\n");
  wDR(0x5C, 0);
    // TODO: host-to-decoder interrupt
   
  tries=1000;
  while (!(stataddr=rDR(0x5C))) { udelay(1000); if (! (tries--)) { printk("timeout.\n"); return -1; } }
  return stataddr;
}

u32 WaitCommand(u32 sa)
{
  int tries=10000;
  while (1)
  {
    int status;
    if (!tries--)
    {
      printk("timeout.\n");
      return -1;
    }
    status=rDR(sa);
    printk(":%x", status);
    if (status>=3)
      return status;
    udelay(1000);
  }
}

int init_module(void)
{
  int pal=1, tries;
  aviamem=(unsigned char*)ioremap(0xA000000, 0x200);
  printk("remapped avia io to %p\n", aviamem);
  
  if (!aviamem)
  {
    printk("failed to remap memory.\n");
    return -1;
  }
  (void)aviamem[0];

  if (request_8xxirq(AVIA_INTERRUPT, avia_interrupt, 0, "avia", &dev) != 0)
    panic("Could not allocate AViA IRQ!");
  
  
//  wGB(0, 3<<22);                // reset decoder
  wGB(0x39, 0xF00000);
  udelay(100);

  wGB(0, 0x1000);            // enable host access
  aviarev=(rGB(0)>>16)&3;
  
  switch (aviarev)
  {
  case 0:
    printk("AVIA 600 found.\n");
    microcode=(u32*)a600vb017;                         // todo!
    break;
  case 1:
    printk("WARNING: AVIA 500 LB3 found. Maybe we load the wrong ucode... continue at your own risk!!!\n");
    microcode=(u32*)a500v093;
    break;
  default:
    printk("AVIA 500 LB4 found.\n");
    microcode=(u32*)a500v093;
    break;
  }
  
  if (!aviarev)
  {
    wGB(0x22, 0x10000);                   // D.4.3 - Initialize the SDRAM DMA Controller
    wGB(0x23, 0x5FBE);
    wGB(0x22, 0x12);
    wGB(0x23, 0x3a1800);
  } else
  {
    wGB(0x22, 0xF);
    wGB(0x23, 0x14EC);
  }
  
  InitialGBus();

  LoaduCode();
  load_imem_image(UX_FIRST_SECTION_START+UX_SECTION_DATA_OFFSET);
  
                 // TODO: ADDR_SD_MODE = 0xA ?!?
  wDR(0x7C, 1);         // HSYNC/VSYNC master, BT.656 output
  wDR(0x68, 0);
  if (pal)
  {
    wDR(0x6c, 0); /* ucode memory = dram */
    wDR(0x21C, PAL_16MB_WO_ROM_SRAM);
    wDR(0x64, 4); /* No SRAM */
    wDR(0x68, 0x00); /* 16MBit DRAM */
    wDR(0x6C, 0); /* Microcode in DRAM  -  nochmal?*/
  } else
  {
    wDR(0x6c, 0);
    wDR(0x21C, NTSC_16MB_WO_ROM_SRAM);
    wDR(0x64, 0);
    wDR(0x68, 0);
    wDR(0x6C,0);
  }
  
 // wDR(0x200, -1 &~(1<<6));       // all but vsync
 

  FinalGBus();
  wGB(0, rGB(0)|0x80);  // enable interrupts
  wGB(0, (rGB(0)&~1)|2);  // enable interrupts
  wDR(0x200, 0xFFFFFF);       // all but vsync
  wDR(0x2AC, 0);
  
  tries=20;
  
  while (rDR(0x2A0)!=0x2)
  {
    if (!--tries)
      break;
    udelay(100*1000);
  }

  wDR(0x98, 0x3F33AA);          // border color
  wDR(0x9C, 0x7F2233);          // background color
  
  printk("final PROC_STATE: %x\n", rDR(0x2A0));
  if (!tries)
    printk("timeout waiting for deocder init complete.\n");
  
  printk("current state: %x\n", rDR(0x2a0));
 
/*  if (WaitCommand(Command(0x8146, 0xB, 0, 0, 0, 0, 0))==-1)
    return 0;*/
  printk("avia soft-reset: fstatus: %x\n", WaitCommand(Command(0x802d, 0, 0, 0, 0, 0, 0))); 
  udelay (2000*1000);

  //we want new audio- and videochannels
  printk("SelectStream (vid): %x\n", WaitCommand(Command(0x0231, 0x3, 0x0, 0, 0, 0, 0)));
  printk("SelectStream (aud): %x\n", WaitCommand(Command(0x0231, 0x0, 0x0, 0, 0, 0, 0)));
  //accept video- and audio-PES streams
  printk("SelectStream (vid): %x\n", WaitCommand(Command(0x0231, 0xb, 0x0, 0, 0, 0, 0)));

//  printk("SelectStream (vid): %x\n", WaitCommand(Command(0x0231, 0x0, 0x65, 0, 0, 0, 0)));
//  printk("SelectStream (aud): %x\n", WaitCommand(Command(0x0231, 0x3, 0x66, 0, 0, 0, 0)));
  printk("play: %x\n", Command(0x343, 0, 0x65, 0, 0, 0, 0));

/*  
  printk("set streamtype: %x\n", WaitCommand(Command(0x8146, 0x10, 0x65, 0, 0, 0, 0)));
  printk("set streamtype: %x\n", WaitCommand(Command(0x8146, 0x11, 0x66, 0, 0, 0, 0)));
  printk("newplaymode: %x\n", WaitCommand(Command(0x0028, 0, 0, 0, 0, 0, 0)));
  printk("fill: %x\n", WaitCommand(Command(0x532, -1, 0, 0, 0, 0xE0FFFF, 0)));
  printk("fill: %x\n", WaitCommand(Command(0x532, 0, 0, 100, 100, 0xE0FF00, 0)));
  printk("fill: %x\n", WaitCommand(Command(0x532, 75, 75, 120, 120, 0xE000FF, 0)));*/
/*  printk("select stream (vid): %x\n", WaitCommand(Command(0x231, 0, 0x65, 0, 0, 0, 0)));
  printk("select stream (aud): %x\n", WaitCommand(Command(0x231, 3, 0x66, 0, 0, 0, 0))); */

/*  printk("select stream (pes): %x\n", WaitCommand(Command(0x231, 0xB, 0, 0, 0, 0, 0)));*/
//  printk("play: %x\n", WaitCommand(Command(0x343, 0, 0x65, 0x66, 0, 0, 0)));
/*  printk("tone: %x\n", WaitCommand(Command(0x449, 440, 32768, 0, 0, 0, 0))); */
  
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

void cleanup_module(void)
{
  free_irq(AVIA_INTERRUPT, &dev);

  if (aviamem)
  {
    iounmap((void*)aviamem);
  }
}
#endif
