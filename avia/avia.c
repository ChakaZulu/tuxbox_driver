// #define a500v093 _binary_500v092_bin_start
// #define a500v093        _binary_500v090_bin_start  
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

#include "../fp/fp.h"
#include "dbox/avia.h"

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia x00 Driver");

void avia_set_pcr(u32 hi, u32 lo);

volatile u8 *aviamem;
u32 *microcode;
int aviarev;

static int dev;
                                                // der interrupt geht nicht.
                                                
                                                // bis der interrupt nicht geht suckt der treiber
                                                
                                                // wenn der treiber suckt kann ich da auch nichts machen
                                                
                                                // der interrupt geht ja nicht.
#define AVIA_INTERRUPT         SIU_IRQ4
                                                // FIX DAS MAL WER! :)
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
        
EXPORT_SYMBOL(avia_wr);
EXPORT_SYMBOL(avia_rd);

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
}

u32 avia_command(u32 command, ...)
{
  u32 stataddr, tries, i;
  va_list ap;
  va_start(ap, command);
                // TODO: kernel lock (somewhat DRINGEND wenn wir mal irgendwann .. und so weiter)
  tries=100;
  while (!rDR(0x5C)) { udelay(10000); if (! (tries--)) { printk("AVIA timeout.\n"); return -1; } }
  
  wDR(0x40, command);
  
  for (i=0; i<((command&0x7F00)>>8); i++)
    wDR(0x44+i*4, va_arg(ap, int));
  va_end(ap);
  for (; i<8; i++)
    wDR(0x44+i*4, 0);

  wDR(0x5C, 0);
    // TODO: host-to-decoder interrupt

  tries=100;
  while (!(stataddr=rDR(0x5C))) { udelay(10000); if (! (tries--)) { printk("AVIA timeout.\n"); return -1; } }
  return stataddr;
}

u32 avia_wait(u32 sa)                 // sagte ich dass wir DRINGEND nen irq brauchen?
{
  int tries=2000;
  if (sa==-1)
    return -1;
  while (1)
  {
    int status;
    if (!tries--)
    {
      printk("COMMAND timeout.\n");
      return -1;
    }
    status=rDR(sa);
    if (!status)
      return status;
    if (status>=3)
      return status;
    schedule();         // bin ich nicht freundlich? gibts da noch was verträglicheres? poll/select?
    udelay(1000);
  }
}

static void avia_audio_init(void);

int init_module(void)
{
  int pal=1, tries;
  aviamem=(unsigned char*)ioremap(0xA000000, 0x200);
  if (!aviamem)
  {
    printk("failed to remap memory.\n");
    return -1;
  }
  (void)aviamem[0];

  printk("AViA x00 Driver: ");

  if (request_8xxirq(AVIA_INTERRUPT, avia_interrupt, 0, "avia", &dev) != 0)
    panic("Could not allocate AViA IRQ!");

#if 0
  printk("%x %x %x %x %x\n", rDR(0x2C8), rDR(0x2CC), rDR(0x2B4), rDR(0x2B8), rDR(0x2C4));
  printk("%x\n", rDR(0x2AC));
  printk("%x %x %x\n", rDR(0x2E4), rDR(0x2E8), rDR(0x2EC));
  printk("%x %x %x\n", rDR(0x318), rDR(0x31c), rDR(0x320));
#endif

  printk("resetting cpu.\n");
  wGB(0x39, 0xF00000);          // reset the cpu
  printk("fp-reset\n");
  fp_do_reset(0xBF & ~ (2));
  udelay(1000);
  printk("done, enabling hostaccess.\n");
  wGB(0, 0x1000);            // enable host access
  wGB(0x39, 0xF00000);          // reset the cpu
  printk("öööh.\n");

  aviarev=(rGB(0)>>16)&3;
  
  switch (aviarev)
  {
  case 0:
    printk("AVIA 600 found. geht nicht.\n");
    microcode=(u32*)a600vb017;
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

  if (!aviarev)                 // ach ja, die 600er intialization geht noch nicht :)
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

  wDR(0x1A8, 0xA);              // TM_MODE              (nokia) (br: evtl. 0x18, aber das wäre seriell.. eNX?)
  wDR(0x7C, 0);                 // walter says 3, tries say 1, nokia says 0, br too  ... HSYNC/VSYNC master, BT.656 output

  wDR(0xEC, 6);                 // AUDIO CLOCK SELECTION (nokia 3) (br 7)

  wDR(0xE8, 2);                 // AUDIO_DAC_MODE
  wDR(0xE0, 0x2F);              // nokia: 0xF, br: 0x2D

  wDR(0xE0, 0x70F);
  wDR(0xF4, 0);                // AUDIO_ATTENUATION (br: 0)

  wDR(0x250, 1);                // DISABLE_OSD: yes (br)
  wDR(0x1B0, 6);                // AV_SYNC_MODE: (NO_SYNC) 6: AV_SYNC
  wDR(0x468, 1);                // NEW_AUDIO_CONFIG

  wDR(0x1C4, 0x1004);           // AUDIO_PTS_SKIP_THRESHOLD_1
  wDR(0x1C8, 0x1004);
  wDR(0x1C0, 0xE10);
  wDR(0x68, 0);                 // 16Mbit DRAM (nokia) (walter) (br)
  wDR(0x6C, 0);                 // (nokia) (br)
  wDR(0x64, 0);
  if (pal)                      // (br) (walter)
    wDR(0x21C, PAL_16MB_WO_ROM_SRAM);
  else
    wDR(0x21C, NTSC_16MB_WO_ROM_SRAM);
  wDR(0x1B8, 0xE10);            // VIDEO_PTS_SKIP_THRESHOLD
  wDR(0x1BC, 0xE10);

  wDR(0x1D8, 0);                // INTERPRET_USER_DATA (br) disable
  FinalGBus();

                // mpg_enAViAIntr aus der BN:
  wDR(0x2AC, 0);
  wGB(0, rGB(0)|0x80);  // enable interrupts
  wGB(0, (rGB(0)&~1)|2);  // enable interrupts
                // ...
//  wDR(0x200, 0xFFFFFFFF);       // all (aber es kommt nix :)
                                // TAUSCHE: 50% der cam-interrupts (das sind eh genug) gegen ein paar avia-interrupts ("command fertig" z.b.)
  tries=20;
  
  while ((rDR(0x2A0)!=0x2))
  {
    if (!--tries)
      break;
    udelay(10*1000);
    schedule();
  }

  if (!tries)
  {
    printk("timeout waiting for decoder initcomplete.\n");
    return 0;
  }
  
  avia_audio_init();
  
  wDR(0x468, 0xFFFF);           // NEW_AUDIO_CONFIG (br)

  tries=20;
  
  while (rDR(0x468))
  {
    if (!--tries)
      break;
    udelay(10*1000);
    schedule();
  }

  if (!tries)
    printk("new_audio_config timeout\n");

  if (avia_wait(avia_command(SetStreamType, 0xB))==-1)             // BR
    return 0;

/*  printk("SelectStream (vid): %x\n", WaitCommand(Command(0x0231, 0xB, 0, 0, 0, 0, 0)));
  printk("SelectStream (aud): %x\n", WaitCommand(Command(0x0231, 0x0, 0xFFFF, 0, 0, 0, 0)));
  printk("SelectStream (aud): %x\n", WaitCommand(Command(0x0231, 0x2, 0xFFFF, 0, 0, 0, 0)));
  printk("SelectStream (aud): %x\n", WaitCommand(Command(0x0231, 0x3, 0xFFFF, 0, 0, 0, 0)));
*/

  avia_set_pcr(0xFF00, 00);

  avia_wait(avia_command(SelectStream, 0, 0xFF));
  avia_wait(avia_command(SelectStream, 2, 0x100));
  avia_wait(avia_command(SelectStream, 3, 0x100));
  avia_command(Play, 0, 0, 0);

  printk("Using avia firmware revision %c%c%c%c\n", rDR(0x330)>>24, rDR(0x330)>>16, rDR(0x330)>>8, rDR(0x330));
  printk("%x %x %x %x %x\n", rDR(0x2C8), rDR(0x2CC), rDR(0x2B4), rDR(0x2B8), rDR(0x2C4));
  return 0;
}

void avia_set_pcr(u32 hi, u32 lo)
{
  u32 data1=(hi>>16)&0xFFFF;
  u32 data2=hi&0xFFFF;
  u32 timer_high=((1<<21))|((data1 & 0xE000L) << 4) 
               | (( 1 << 16)) | ((data1 & 0x1FFFL) << 3)
               | ((data2 & 0xC000L) >> 13) | ((1L));
  u32 timer_low = ((data2 & 0x03FFFL) << 2)
               | ((lo & 0x8000L) >> 14) | (( 1L ));
  printk("setting avia PCR: %08x:%08x\n", hi, lo);
  wGB(0x02, timer_high);
  wGB(0x03, timer_low);
}

static void avia_audio_init(void)               // brauch ich. keine ahnung was genau. sollte man nochmal checken
{
  u32 val;

  val = 0;

                        // AUDIO_CONFIG 12,11,7,6,5,4 reserved or must be set to 0
  val |= (0<<10);       // 64 DAI BCKs per DAI LRCK
  val |= (0<<9);        // input is I2S
  val |= (0<<8);        // output constan low (no clock)
  val |= (0<<3);        // 0: normal 1:I2S output
  val |= (0<<2);        // 0:off 1:on channels
  val |= (0<<1);        // 0:off 1:on IEC-958
  val |= (0);           // 0:encoded 1:decoded output
  wDR(0xE0, val);

  val = 0;

                        // AUDIO_DAC_MODE 0 reserved
  val |= (0<<8);
  val |= (0<<6);
  val |= (0<<4);
  val |= (0<<3);        // 0:high 1:low DA-LRCK polarity
  val |= (0<<2);        // 0:0 as MSB in 24 bit mode 1: sign ext. in 24bit
  val |= (0<<1);        // 0:msb 1:lsb first
  wDR(0xE8, val);

  val = 0;
                        // AUDIO_CLOCK_SELECTION
  val |= (0<<2);
  val |= (0<<1);        // 1:256 0:384 x sampling frequ.
  val |= (1);           // master,slave mode

  wDR(0xEC, val);

  val = 0;

                        // AUDIO_ATTENUATION
  wDR(0xF4, 0);
}

EXPORT_SYMBOL(avia_set_pcr);

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
