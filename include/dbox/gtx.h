#ifndef __GTX_H__
#define __GTX_H__

#undef CR0
#undef CR1

#define GTX_REG_BASE		0x08400000
#define GTX_REG_SIZE		0x00003000
#define GTX_MEM_BASE		0x08000000
#define GTX_MEM_SIZE		0x00200000
#define GTX_FB_OFFSET		0x0100000
#define GTX_INTERRUPT		SIU_IRQ1

/* Graphics */
#define GTX_REG_GMR	0x000
#define GTX_REG_CLTA	0x004
#define GTX_REG_CLTD	0x006
#define GTX_REG_TCR	0x008
#define GTX_REG_CCR	0x00A
#define GTX_REG_GVSA	0x00C
#define GTX_REG_GVP	0x010
#define GTX_REG_GVS	0x014
#define GTX_REG_CSA	0x018
#define GTX_REG_CPOS	0x01C
#define GTX_REG_GFUNC	0x020

/* SAR */
#define GTX_REG_TRP	0x040
#define GTX_REG_TRW	0x044
#define GTX_REG_TRL	0x046
#define GTX_REG_RRP	0x048
#define GTX_REG_RRW	0x04C
#define GTX_REG_RRL	0x04E
#define GTX_REG_RVCA	0x050
#define GTX_REG_RVCD	0x052
#define GTX_REG_STXC	0x054
#define GTX_REG_SRXC	0x056

/* Smart Card */
#define GTX_REG_TMA	0x060
#define GTX_REG_RMA	0x064
#define GTX_REG_SCS	0x068
#define GTX_REG_TMS	0x06A
#define GTX_REG_RMS	0x06C
#define GTX_REG_SCC	0x06E
#define GTX_REG_CWI	0x070
#define GTX_REG_ETU	0x072
#define GTX_REG_GDT	0x074
#define GTX_REG_SCON	0x076

/* Interrupt */
#define GTX_REG_ISR0	0x080
#define GTX_REG_ISR1	0x082
#define GTX_REG_ISR2	0x084
#define GTX_REG_IMR0	0x086
#define GTX_REG_IMR1	0x088
#define GTX_REG_IMR2	0x08A
#define GTX_REG_IPR0	0x08C
#define GTX_REG_IPR1	0x08E
#define GTX_REG_IPR2	0x090
#define GTX_REG_ISR3	0x092
#define GTX_REG_IMR3	0x094
#define GTX_REG_IPR3	0x096
#define GTX_REG_ICC	0x09C
#define GTX_REG_DCC	0x09E

/* CRC */
#define GTX_REG_CRCC	0x0A0
#define GTX_REG_TCRC	0x0A4
#define GTX_REG_RCRC	0x0A8

/* Blitter */
#define GTX_REG_BDST	0x0B0
#define GTX_REG_BMR	0x0B4
#define GTX_REG_BDR	0x0B6
#define GTX_REG_BCLR	0x0B8
#define GTX_REG_BPW	0x0BC
#define GTX_REG_BPO	0x0BE

/* Infrared */
#define GTX_REG_CWP	0x0C0
#define GTX_REG_CWPH	0x0C2
#define GTX_REG_MSP	0x0C4
#define GTX_REG_MSPL	0x0C6
#define GTX_REG_RTC	0x0C8
#define GTX_REG_RTP	0x0CA

/* SPI */
#define GTX_REG_SPID	0x0D0
#define GTX_REG_SPIC	0x0D2

/* PCM Audio */
#define GTX_REG_PCMA	0x0E0
#define GTX_REG_PCMN	0x0E4
#define GTX_REG_PCMC	0x0E8
#define GTX_REG_PCMD	0x0EC

/* Video */
#define GTX_REG_VBR	0x0F0
#define GTX_REG_VCR	0x0F4
#define GTX_REG_VLC	0x0F6
#define GTX_REG_VLI1	0x0F8
#define GTX_REG_VHT	0x0FA
#define GTX_REG_VLT	0x0FC
#define GTX_REG_VLI2	0x0FE

/* Configuration and Control */
#define GTX_REG_RR0	0x100
#define GTX_REG_RR1	0x102
#define GTX_REG_CR0	0x104
#define GTX_REG_CR1	0x106
#define GTX_REG_C0CR	0x10C
#define GTX_REG_C1CR	0x10E

/* DAC */
#define GTX_REG_DPCR	0x110

/* Framer */
#define GTX_REG_PRCPID	0x120
#define GTX_REG_PCR2	0x122
#define GTX_REG_PCR1	0x124
#define GTX_REG_PCR0	0x126
#define GTX_REG_LSTC2	0x128
#define GTX_REG_LSTC1	0x12A
#define GTX_REG_LSTC0	0x12C
#define GTX_REG_STCC2	0x12E
#define GTX_REG_STCC1	0x130
#define GTX_REG_STCC0	0x132
#define GTX_REG_FCR	0x134
#define GTX_REG_SYNCH	0x136
#define GTX_REG_PFIFO	0x138

/* IDC */
#define GTX_REG_IDCCR	0x140
#define GTX_REG_IDCSR	0x142
#define GTX_REG_IDCSA	0x144
#define GTX_REG_IDCRD	0x146
#define GTX_REG_IDCMA	0x148
#define GTX_REG_IDCTD	0x14A
#define GTX_REG_IDCC	0x14C
#define GTX_REG_IDCFF	0x14E

/* Audio/Video Decoder Interface */
#define GTX_REG_AVI	0x150

/* RISC Engine */
#define GTX_REG_RISCPC	0x170
#define GTX_REG_RISCCON	0x178

/* Queue Write Pointer */
#define GTX_REG_QWPnL	0x180
#define GTX_REG_QWPnH	0x182
#define GTX_REG_QWP0L	0x180
#define GTX_REG_QWP0H	0x182
#define GTX_REG_QWP1L	0x184
#define GTX_REG_QWP1H	0x186
#define GTX_REG_QWP2L	0x188
#define GTX_REG_QWP2H	0x18A
#define GTX_REG_QWP3L	0x18C
#define GTX_REG_QWP3H	0x18E
#define GTX_REG_QWP4L	0x190
#define GTX_REG_QWP4H	0x192
#define GTX_REG_QWP5L	0x194
#define GTX_REG_QWP5H	0x196
#define GTX_REG_QWP6L	0x198
#define GTX_REG_QWP6H	0x19A
#define GTX_REG_QWP7L	0x19C
#define GTX_REG_QWP7H	0x19E
#define GTX_REG_QWP8L	0x1A0
#define GTX_REG_QWP8H	0x1A2
#define GTX_REG_QWP9L	0x1A4
#define GTX_REG_QWP9H	0x1A6
#define GTX_REG_QWP10L	0x1A8
#define GTX_REG_QWP10H	0x1AA
#define GTX_REG_QWP11L	0x1AC
#define GTX_REG_QWP11H	0x1AE
#define GTX_REG_QWP12L	0x1B0
#define GTX_REG_QWP12H	0x1B2
#define GTX_REG_QWP13L	0x1B4
#define GTX_REG_QWP13H	0x1B6
#define GTX_REG_QWP14L	0x1B8
#define GTX_REG_QWP14H	0x1BA
#define GTX_REG_QWP15L	0x1BC
#define GTX_REG_QWP15H	0x1BE

/* Queue Interrupt */
#define GTX_REG_QIn	0x1C0
#define GTX_REG_QI0	0x1C0
#define GTX_REG_QI1	0x1C2
#define GTX_REG_QI2	0x1C4
#define GTX_REG_QI3	0x1C6
#define GTX_REG_QI4	0x1C8
#define GTX_REG_QI5	0x1CA
#define GTX_REG_QI6	0x1CC
#define GTX_REG_QI7	0x1CE
#define GTX_REG_QI8	0x1D0
#define GTX_REG_QI9	0x1D2
#define GTX_REG_QI10	0x1D4
#define GTX_REG_QI11	0x1D6
#define GTX_REG_QI12	0x1D8
#define GTX_REG_QI13	0x1DA
#define GTX_REG_QI14	0x1DC
#define GTX_REG_QI15	0x1DE

/* Audio Queue Manager */
#define GTX_REG_AQRPL	0x1E0
#define GTX_REG_AQRPH	0x1E2
#define GTX_REG_AQWPL	0x1E4
#define GTX_REG_AQWPH	0x1E6

/* Teletext Queue Manager */
#define GTX_REG_TQRPL	0x1E8
#define GTX_REG_TQRPH	0x1EA
#define GTX_REG_TQWPL	0x1EC
#define GTX_REG_TQWPH	0x1EE

/* Video Queue Manager */
#define GTX_REG_VQRPL	0x1F0
#define GTX_REG_VQRPH	0x1F2
#define GTX_REG_VQWPL	0x1F4
#define GTX_REG_VQWPH	0x1F6

/* Copy Engine */
#define GTX_REG_CBW0	0x200
#define GTX_REG_CBW1	0x202
#define GTX_REG_CBW2	0x204
#define GTX_REG_CBW3	0x206
#define GTX_REG_CBW4	0x208
#define GTX_REG_CBW5	0x20A
#define GTX_REG_CBW6	0x20C
#define GTX_REG_CBW7	0x20E
#define GTX_REG_CCSA	0x220
#define GTX_REG_CDA	0x224
#define GTX_REG_CCOM	0x228
#define GTX_REG_RWTC	0x22A
#define GTX_REG_CCOM2	0x22C
#define GTX_REG_CCOM3	0x22E

/* Video Plane Display */
#define GTX_REG_VPSA	0x240
#define GTX_REG_VPO	0x244
#define GTX_REG_VPP	0x248
#define GTX_REG_VPS	0x24C
#define GTX_REG_VPOE	0x250

/* Video Capture */
#define GTX_REG_VCSA	0x260
#define GTX_REG_VCSP	0x264
#define GTX_REG_VCS	0x268

/* Semaphore */
#define GTX_REG_SEM1	0x270
#define GTX_REG_SEM2	0x272

/* Teletext */
#define GTX_REG_PTS0	0x280
#define GTX_REG_PTS1	0x282
#define GTX_REG_PTSO	0x284
#define GTX_REG_TTCR	0x286
#define GTX_REG_TSR	0x288

/* Infrared */
#define GTX_REG_RFR	0x2AC
#define GTX_REG_RPH	0x2AE
#define GTX_REG_IRQA	0x2B0
#define GTX_REG_IRRE	0x2B4
#define GTX_REG_IRTE	0x2B6
#define GTX_REG_IRRO	0x2B8
#define GTX_REG_IRTO	0x2BA

#define GTX_REG_RISC	0x1000

#define GTX_IRQ_REG_ISR0	0
#define GTX_IRQ_REG_ISR1	1
#define GTX_IRQ_REG_ISR2	2
#define GTX_IRQ_REG_ISR3	3

#define GTX_IRQ_PCR		AVIA_GT_IRQ(GTX_IRQ_REG_ISR0, 8)
#define GTX_IRQ_IR_TX		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 8)
#define GTX_IRQ_IR_RX		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 9)
#define GTX_IRQ_PCM_PF		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 10)
#define GTX_IRQ_VL0		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 11)
#define GTX_IRQ_PCM_AD		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 12)
#define GTX_IRQ_VL1		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 13)

#pragma pack(1)

/* Graphics */
typedef struct {

	unsigned char GMD: 2;
	unsigned char L: 1;
	unsigned char F: 1;
	unsigned char C: 1;
	unsigned char I: 1;
	unsigned char CFT: 2;
	unsigned char BLEV1: 4;
	unsigned char BLEV0: 4;
	unsigned char Reserved1: 5;
	unsigned short STRIDE: 10;
	unsigned char Reserved2: 1;

} sGTX_REG_GMR;

typedef struct {

	unsigned char Reserved1: 8;
	unsigned char Addr: 8;

} sGTX_REG_CLTA;

typedef struct {

	unsigned char T: 1;
	unsigned char R: 5;
	unsigned char G: 5;
	unsigned char B: 5;

} sGTX_REG_CLTD;

typedef struct {

	unsigned char E: 1;
	unsigned char R: 5;
	unsigned char G: 5;
	unsigned char B: 5;

} sGTX_REG_TCR;

typedef struct {

	unsigned char Reserved1: 1;
	unsigned char R: 5;
	unsigned char G: 5;
	unsigned char B: 5;

} sGTX_REG_CCR;

typedef struct {

	unsigned short Reserved1: 10;
	unsigned int Addr: 21;
	unsigned char Reserved2: 1;

} sGTX_REG_GVSA;

typedef struct {

	unsigned char SPP: 5;
	unsigned char Reserved1: 1;
	unsigned short XPOS: 10;
	unsigned char Reserved2: 6;
	unsigned short YPOS: 10;

} sGTX_REG_GVP;

typedef struct {

	unsigned char IPS: 5;
	unsigned char Reserved1: 1;
	unsigned short XSZ: 10;
	unsigned char Reserved2: 6;
	unsigned short YSZ: 10;

} sGTX_REG_GVS;

typedef struct {

	unsigned short Reserved1: 10;
	unsigned int Addr: 21;
	unsigned char Reserved2: 1;

} sGTX_REG_CSA;

typedef struct {

	unsigned short Reserved1: 11;
	unsigned char D: 1;
	unsigned char Bank: 4;

} sGTX_REG_GFUNC;



/* PCM Audio */
typedef struct {

	unsigned short NSAMP: 10;
	unsigned int Addr: 21;
	unsigned char W: 1;

} sGTX_REG_PCMA;

typedef struct {

	unsigned char PCMAL: 7;
	unsigned char Reserved1: 1;
	unsigned char PCMAR: 7;
	unsigned char Reserved2: 1;
	unsigned char MPEGAL: 7;
	unsigned char Reserved3: 1;
	unsigned char MPEGAR: 7;
	unsigned char Reserved4: 1;

} sGTX_REG_PCMN;

typedef struct {

	unsigned char R: 2;
	unsigned char W: 1;
	unsigned char C: 1;
	unsigned char S: 1;
	unsigned char T: 1;
	unsigned char V: 1;
	unsigned char P: 1;
	unsigned char M: 1;
	unsigned char I: 1;
	unsigned char ADV: 2;
	unsigned char ACD: 2;
	unsigned char BCD: 2;

} sGTX_REG_PCMC;



/* Video */
typedef struct {

	unsigned char Reserved1: 7;
	unsigned char E: 1;
	unsigned char Y: 8;
	unsigned char Cr: 8;
	unsigned char Cb: 8;

} sGTX_REG_VBR;

typedef struct {

	unsigned char S: 1;
	unsigned char P: 1;
	unsigned char C: 1;
	unsigned char A: 1;
	unsigned char HP: 2;
	unsigned char FP: 2;
	unsigned char E: 1;
	unsigned char D: 1;
	unsigned char N: 1;
	unsigned char DELAY: 5;

} sGTX_REG_VCR;

typedef struct {

	unsigned char Reserved1: 6;
	unsigned short LINE: 9;
	unsigned char F: 1;

} sGTX_REG_VLC;

typedef struct {

	unsigned char E: 1;
	unsigned char Reserved1: 5;
	unsigned short LINE: 9;
	unsigned char F: 1;

} sGTX_REG_VLI1;

typedef struct {

	unsigned char SD: 2;
	unsigned char Reserved1: 4;
	unsigned short Width: 10;

} sGTX_REG_VHT;

typedef struct {

	unsigned char VBI: 5;
	unsigned char Reserved1: 1;
	unsigned short Lines: 10;

} sGTX_REG_VLT;

typedef struct {

	unsigned char E: 1;
	unsigned char Reserved1: 5;
	unsigned short LINE: 9;
	unsigned char F: 1;

} sGTX_REG_VLI2;



/* Configuration and Control */
typedef struct {

	unsigned char PIG: 1;
	unsigned char VCAP: 1;
	unsigned char VID: 1;
	unsigned char ACLK: 1;
	unsigned char COPY: 1;
	unsigned char DRAM: 1;
	unsigned char PCM: 1;
	unsigned char SPI: 1;
	unsigned char IR: 1;
	unsigned char BLIT: 1;
	unsigned char CRC: 1;
	unsigned char INT: 1;
	unsigned char SCD: 1;
	unsigned char SRX: 1;
	unsigned char STX: 1;
	unsigned char GV: 1;

} sGTX_REG_RR0;

typedef struct {

	unsigned char Reserved1: 8;
	unsigned char TTX: 1;
	unsigned char DAC: 1;
	unsigned char RISC: 1;
	unsigned char FRMR: 1;
	unsigned char CHAN: 1;
	unsigned char AVD: 1;
	unsigned char IDC: 1;
	unsigned char DESC: 1;

} sGTX_REG_RR1;

typedef struct {

	unsigned char REV_ID: 4;
	unsigned char GOF: 1;
	unsigned char SOF: 1;
	unsigned char POF: 1;
	unsigned char WBD: 1;
	unsigned char DD1: 1;
	unsigned char DD0: 1;
	unsigned char DOD: 1;
	unsigned char SPI: 1;
	unsigned char _16M: 1;
	unsigned char RFD: 1;
	unsigned char MAP: 1;
	unsigned char RES: 1;

} sGTX_REG_CR0;

typedef struct {

	unsigned char BRD_ID: 8;
	unsigned char Reserved1: 3;
	unsigned char UPQ: 1;
	unsigned char TCP: 1;
	unsigned char FH: 1;
	unsigned char ACP: 1;
	unsigned char VCP: 1;

} sGTX_REG_CR1;



/* Queue Write Pointer */
typedef struct {

	unsigned char Reserved1: 6;
	unsigned char Q_Size: 4;
	unsigned char Upper_WD_n: 6;

} sGTX_REG_QWPnH;

typedef struct {

	unsigned short Queue_n_Write_Pointer: 16;

} sGTX_REG_QWPnL;



/* Video Queue Manager */
typedef struct {

	unsigned short Video_Read: 16;

} sGTX_REG_VQRPL;

typedef struct {

	unsigned char H: 1;
	unsigned short Reserved1: 9;
	unsigned char Video_Read: 6;

} sGTX_REG_VQRPH;

typedef struct {

	unsigned short Video_Write: 16;

} sGTX_REG_VQWPL;

typedef struct {

	unsigned char Reserved1: 6;
	unsigned char Q_SIZE: 4;
	unsigned char Video_Write: 6;

} sGTX_REG_VQWPH;



/* Video Plane Display */
typedef struct {

	unsigned short Reserved1: 10;
	unsigned int Addr: 21;
	unsigned char E: 1;

} sGTX_REG_VPSA;

typedef struct {

	unsigned short OFFSET;
	unsigned char Reserved1: 4;
	unsigned short STRIDE: 11;
	unsigned char B: 1;

} sGTX_REG_VPO;

typedef struct {

	unsigned char Reserved1: 6;
	unsigned short HPOS: 9;
	unsigned char Reserved2: 7;
	unsigned short VPOS: 9;
	unsigned char F: 1;

} sGTX_REG_VPP;

typedef struct {

	unsigned char Reserved1: 6;
	unsigned short WIDTH: 9;
	unsigned char S: 1;
	unsigned char Reserved2: 6;
	unsigned short HEIGHT: 9;
	unsigned char P: 1;

} sGTX_REG_VPS;



/* Video Capture */
typedef struct {

	unsigned short Reserved1: 10;
	unsigned int Addr: 21;
	unsigned char E: 1;

} sGTX_REG_VCSA;

typedef struct {

	unsigned char V: 1;
	unsigned char Reserved1: 5;
	unsigned short HPOS: 9;
	unsigned char Reserved2: 3;
	unsigned char OVOFFS: 4;
	unsigned short EVPOS: 9;
	unsigned char Reserved3: 1;

} sGTX_REG_VCSP;

typedef struct {

	unsigned short HDEC: 4;
	unsigned char Reserved1: 2;
	unsigned short HSIZE: 9;
	unsigned char F: 1;
	unsigned char VDEC: 4;
	unsigned char Reserved2: 2;
	unsigned short VSIZE: 9;
	unsigned char B: 1;

} sGTX_REG_VCS;



/* Teletext */
typedef struct {

	unsigned char Reserved1: 1;
	unsigned char PE: 1;
	unsigned char Reserved2: 1;
	unsigned char RP: 1;
	unsigned char FP: 1;
	unsigned char Reserved3: 1;
	unsigned char GO: 1;
	unsigned char IE: 1;
	unsigned char Data_ID: 8;

} sGTX_REG_TTCR;

#pragma pack()

extern void avia_gt_gtx_clear_irq(unsigned char irq_reg, unsigned char irq_bit);
extern unsigned short avia_gt_gtx_get_irq_mask(unsigned char irq_reg);
extern unsigned short avia_gt_gtx_get_irq_status(unsigned char irq_reg);
extern void avia_gt_gtx_mask_irq(unsigned char irq_reg, unsigned char irq_bit);
extern void avia_gt_gtx_unmask_irq(unsigned char irq_reg, unsigned char irq_bit);
extern void avia_gt_gtx_init(void);
extern void avia_gt_gtx_exit(void);

#define gtx_reg_16(register) ((unsigned short)(*((unsigned short*)(gt_info->reg_addr + GTX_REG_ ## register))))
#define gtx_reg_16n(offset) ((unsigned short)(*((unsigned short*)(gt_info->reg_addr + offset))))
#define gtx_reg_32(register) ((unsigned int)(*((unsigned int*)(gt_info->reg_addr + GTX_REG_ ## register))))
#define gtx_reg_32n(offset) ((unsigned int)(*((unsigned int*)(gt_info->reg_addr + offset))))
#define gtx_reg_o(offset) (gt_info->reg_addr + offset)
#define gtx_reg_s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_sn(register, offset) ((sGTX_REG_##register *)(gtx_reg_o(offset)))
#define gtx_reg_so(register, offset) ((sGTX_REG_##register *)(&gtx_reg_32(register + (offset))))
#define gtx_reg_32s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_16s(register) ((sGTX_REG_##register *)(&gtx_reg_16(register)))

#define rw(a) (*((volatile unsigned long*)(gt_info->reg_addr+g ## a)))
#define rh(a) (*((volatile unsigned short*)(gt_info->reg_addr+g ## a)))

#define rwn(a) (*((volatile unsigned long*)(gt_info->reg_addr+a)))
#define rhn(a) (*((volatile unsigned short*)(gt_info->reg_addr+a)))

#endif /* __GTX_H__ */
