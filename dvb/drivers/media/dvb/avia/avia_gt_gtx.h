#ifndef __GTX_H__
#define __GTX_H__

#undef CR0
#undef CR1

#define GTX_REG_BASE	0x08400000
#define GTX_REG_SIZE	0x00003000
#define GTX_MEM_BASE	0x08000000
#define GTX_MEM_SIZE	0x00200000
#define GTX_FB_OFFSET	0x0100000
#define GTX_INTERRUPT	SIU_IRQ1

#define AVIA_GT_GTX_IR_CLOCK	40500000

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
#define GTX_REG_DPR	0x112

/* Framer */
#define GTX_REG_PCRPID	0x120
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

/* IDC Interface */
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
#define GTX_REG_CBWn	0x200
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
#define GTX_REG_CWP	0x2A0
#define GTX_REG_CWPH	0x2A2
#define GTX_REG_MSPR	0x2A4
#define GTX_REG_MSPL	0x2A6
#define GTX_REG_RTC	0x2A8
#define GTX_REG_RTP	0x2AA
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
#define GTX_IRQ_TT		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 15)

#pragma pack(1)

/* Graphics */
typedef struct {

	unsigned GMD: 2;
	unsigned L: 1;
	unsigned F: 1;
	unsigned C: 1;
	unsigned I: 1;
	unsigned CFT: 2;
	unsigned BLEV1: 4;
	unsigned BLEV0: 4;
	unsigned Reserved1: 5;
	unsigned STRIDE: 10;
	unsigned Reserved2: 1;

} sGTX_REG_GMR;

typedef struct {

	unsigned Reserved1: 8;
	unsigned Addr: 8;

} sGTX_REG_CLTA;

typedef struct {

	unsigned T: 1;
	unsigned R: 5;
	unsigned G: 5;
	unsigned B: 5;

} sGTX_REG_CLTD;

typedef struct {

	unsigned E: 1;
	unsigned R: 5;
	unsigned G: 5;
	unsigned B: 5;

} sGTX_REG_TCR;

typedef struct {

	unsigned Reserved1: 1;
	unsigned R: 5;
	unsigned G: 5;
	unsigned B: 5;

} sGTX_REG_CCR;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 21;
	unsigned Reserved2: 1;

} sGTX_REG_GVSA;

typedef struct {

	unsigned SPP: 5;
	unsigned Reserved1: 1;
	unsigned XPOS: 10;
	unsigned Reserved2: 6;
	unsigned YPOS: 10;

} sGTX_REG_GVP;

typedef struct {

	unsigned IPS: 5;
	unsigned Reserved1: 1;
	unsigned XSZ: 10;
	unsigned Reserved2: 6;
	unsigned YSZ: 10;

} sGTX_REG_GVS;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 21;
	unsigned Reserved2: 1;

} sGTX_REG_CSA;

typedef struct {

	unsigned Reserved1: 6;
	unsigned XPOS: 10;
	unsigned Reserved2: 6;
	unsigned YPOS: 10;

} sGTX_REG_CPOS;

typedef struct {

	unsigned Reserved1: 11;
	unsigned D: 1;
	unsigned Bank: 4;

} sGTX_REG_GFUNC;



/* SAR */
typedef struct {

	unsigned Reserved1: 10;
	unsigned Base: 12;
	unsigned Index: 8;
	unsigned Reserved2: 2;

} sGTX_REG_TRP;

typedef struct {

	unsigned Reserved1: 6;
	unsigned Limit: 8;
	unsigned Reserved2: 1;
	unsigned D: 1;

} sGTX_REG_TRW;

typedef struct {

	unsigned Reserved1: 6;
	unsigned Limit: 8;
	unsigned Reserved2: 1;
	unsigned D: 1;

} sGTX_REG_TRL;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Base: 10;
	unsigned Index: 10;
	unsigned Reserved2: 2;

} sGTX_REG_RRP;

typedef struct {

	unsigned Reserved1: 4;
	unsigned Limit: 10;
	unsigned Reserved2: 1;
	unsigned D: 1;

} sGTX_REG_RRW;

typedef struct {

	unsigned Reserved1: 4;
	unsigned Limit: 10;
	unsigned Reserved2: 1;
	unsigned D: 1;

} sGTX_REG_RRL;

typedef struct {

	unsigned Reserved1: 12;
	unsigned Addr: 4;

} sGTX_REG_RVCA;

typedef struct {

	unsigned Entry: 16;

} sGTX_REG_RVCD;

typedef struct {

	unsigned Reserved1: 14;
	unsigned TXC: 2;

} sGTX_REG_STXC;

typedef struct {

	unsigned Reserved1: 14;
	unsigned RXC: 2;

} sGTX_REG_SRXC;



/* Smart Card */
typedef struct {

	unsigned Reserved1: 10;
	unsigned Transmit_Data_DRAM_Address: 21;
	unsigned Reserved2: 1;

} sGTX_REG_TMA;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Receive_Data_DRAM_Address: 21;
	unsigned Reserved2: 1;

} sGTX_REG_RMA;

typedef struct {

	unsigned SmartCardStatus: 16;

} sGTX_REG_SCS;

typedef struct {

	unsigned Reserved1: 7;
	unsigned MessageSize: 9;

} sGTX_REG_TMS;

typedef struct {

	unsigned Reserved1: 7;
	unsigned MessageSize: 9;

} sGTX_REG_RMS;

typedef struct {

	unsigned SMR: 1;
	unsigned RD: 1;
	unsigned RL: 1;
	unsigned RST: 1;
	unsigned COE: 1;
	unsigned CD: 1;
	unsigned VCC: 1;
	unsigned VPL: 1;
	unsigned VPE: 1;
	unsigned CRC: 1;
	unsigned IE: 1;
	unsigned PT: 1;
	unsigned TE: 1;
	unsigned RE: 1;
	unsigned PTS: 1;
	unsigned SAR: 1;

} sGTX_REG_SCC;

typedef struct {

	unsigned Reserved1: 12;
	unsigned CWI: 4;

} sGTX_REG_CWI;

typedef struct {

	unsigned Reserved1: 1;
	unsigned CP: 1;
	unsigned C: 1;
	unsigned EP: 1;
	unsigned ETU: 12;

} sGTX_REG_ETU;

typedef struct {

	unsigned Reserved1: 8;
	unsigned GuardTime: 8;

} sGTX_REG_GDT;

typedef struct {

	unsigned Reserved1: 14;
	unsigned COD: 1;
	unsigned SCD: 1;

} sGTX_REG_SCON;



/* Interrupt */
typedef struct {

	unsigned Reserved1: 6;
	unsigned IDC: 1;
	unsigned PCR: 1;
	unsigned DROP: 1;
	unsigned LOCK: 1;
	unsigned WT: 1;
	unsigned NR: 1;
	unsigned PO: 1;
	unsigned MC: 1;
	unsigned CD: 1;
	unsigned PE: 1;

} sGTX_REG_ISR0;

typedef struct {

	unsigned TT: 1;
	unsigned CD: 1;
	unsigned VL1: 1;
	unsigned AD: 1;
	unsigned VL0: 1;
	unsigned PF: 1;
	unsigned IR: 1;
	unsigned IT: 1;
	unsigned RL: 1;
	unsigned RW: 1;
	unsigned RE: 1;
	unsigned RC: 1;
	unsigned TL: 1;
	unsigned TW: 1;
	unsigned TE: 1;
	unsigned TC: 1;

} sGTX_REG_ISR1;

typedef struct {

	unsigned Q0: 1;
	unsigned Q1: 1;
	unsigned Q2: 1;
	unsigned Q3: 1;
	unsigned Q4: 1;
	unsigned Q5: 1;
	unsigned Q6: 1;
	unsigned Q7: 1;
	unsigned Q8: 1;
	unsigned Q9: 1;
	unsigned Q10: 1;
	unsigned Q11: 1;
	unsigned Q12: 1;
	unsigned Q13: 1;
	unsigned Q14: 1;
	unsigned Q15: 1;

} sGTX_REG_ISR2;

typedef struct {

	unsigned Q16: 1;
	unsigned Q17: 1;
	unsigned Q18: 1;
	unsigned Q19: 1;
	unsigned Q20: 1;
	unsigned Q21: 1;
	unsigned Q22: 1;
	unsigned Q23: 1;
	unsigned Q24: 1;
	unsigned Q25: 1;
	unsigned Q26: 1;
	unsigned Q27: 1;
	unsigned Q28: 1;
	unsigned Q29: 1;
	unsigned Q30: 1;
	unsigned Q31: 1;

} sGTX_REG_ISR3;

typedef struct {

	unsigned Reserved1: 6;
	unsigned IDC: 1;
	unsigned PCR: 1;
	unsigned DROP: 1;
	unsigned LOCK: 1;
	unsigned WT: 1;
	unsigned NR: 1;
	unsigned PO: 1;
	unsigned MC: 1;
	unsigned CD: 1;
	unsigned PE: 1;

} sGTX_REG_IMR0;

typedef struct {

	unsigned TT: 1;
	unsigned CD: 1;
	unsigned VL1: 1;
	unsigned AD: 1;
	unsigned VL0: 1;
	unsigned PF: 1;
	unsigned IR: 1;
	unsigned IT: 1;
	unsigned RL: 1;
	unsigned RW: 1;
	unsigned RE: 1;
	unsigned RC: 1;
	unsigned TL: 1;
	unsigned TW: 1;
	unsigned TE: 1;
	unsigned TC: 1;

} sGTX_REG_IMR1;

typedef struct {

	unsigned Q0: 1;
	unsigned Q1: 1;
	unsigned Q2: 1;
	unsigned Q3: 1;
	unsigned Q4: 1;
	unsigned Q5: 1;
	unsigned Q6: 1;
	unsigned Q7: 1;
	unsigned Q8: 1;
	unsigned Q9: 1;
	unsigned Q10: 1;
	unsigned Q11: 1;
	unsigned Q12: 1;
	unsigned Q13: 1;
	unsigned Q14: 1;
	unsigned Q15: 1;

} sGTX_REG_IMR2;

typedef struct {

	unsigned Q16: 1;
	unsigned Q17: 1;
	unsigned Q18: 1;
	unsigned Q19: 1;
	unsigned Q20: 1;
	unsigned Q21: 1;
	unsigned Q22: 1;
	unsigned Q23: 1;
	unsigned Q24: 1;
	unsigned Q25: 1;
	unsigned Q26: 1;
	unsigned Q27: 1;
	unsigned Q28: 1;
	unsigned Q29: 1;
	unsigned Q30: 1;
	unsigned Q31: 1;

} sGTX_REG_IMR3;

typedef struct {

	unsigned Reserved1: 6;
	unsigned IDC: 1;
	unsigned PCR: 1;
	unsigned DROP: 1;
	unsigned LOCK: 1;
	unsigned WT: 1;
	unsigned NR: 1;
	unsigned PO: 1;
	unsigned MC: 1;
	unsigned CD: 1;
	unsigned PE: 1;

} sGTX_REG_IPR0;

typedef struct {

	unsigned TT: 1;
	unsigned CD: 1;
	unsigned VL1: 1;
	unsigned AD: 1;
	unsigned VL0: 1;
	unsigned PF: 1;
	unsigned IR: 1;
	unsigned IT: 1;
	unsigned RL: 1;
	unsigned RW: 1;
	unsigned RE: 1;
	unsigned RC: 1;
	unsigned TL: 1;
	unsigned TW: 1;
	unsigned TE: 1;
	unsigned TC: 1;

} sGTX_REG_IPR1;

typedef struct {

	unsigned Q0: 1;
	unsigned Q1: 1;
	unsigned Q2: 1;
	unsigned Q3: 1;
	unsigned Q4: 1;
	unsigned Q5: 1;
	unsigned Q6: 1;
	unsigned Q7: 1;
	unsigned Q8: 1;
	unsigned Q9: 1;
	unsigned Q10: 1;
	unsigned Q11: 1;
	unsigned Q12: 1;
	unsigned Q13: 1;
	unsigned Q14: 1;
	unsigned Q15: 1;

} sGTX_REG_IPR2;

typedef struct {

	unsigned Q16: 1;
	unsigned Q17: 1;
	unsigned Q18: 1;
	unsigned Q19: 1;
	unsigned Q20: 1;
	unsigned Q21: 1;
	unsigned Q22: 1;
	unsigned Q23: 1;
	unsigned Q24: 1;
	unsigned Q25: 1;
	unsigned Q26: 1;
	unsigned Q27: 1;
	unsigned Q28: 1;
	unsigned Q29: 1;
	unsigned Q30: 1;
	unsigned Q31: 1;

} sGTX_REG_IPR3;



/* CRC */
typedef struct {

	unsigned CMD: 2;
	unsigned Reserved1: 2;
	unsigned LEN: 3;
	unsigned L: 1;
	unsigned Reserved2: 2;
	unsigned Addr: 21;
	unsigned F: 1;

} sGTX_REG_CRCC;

typedef struct {

	unsigned CRC: 32;

} sGTX_REG_TCRC;

typedef struct {

	unsigned CRC: 32;

} sGTX_REG_RCRC;



/* Blitter */
typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 18;
	unsigned Count: 4;

} sGTX_REG_BDST;

typedef struct {

	unsigned M: 16;

} sGTX_REG_BMR;

typedef struct {

	unsigned D: 16;

} sGTX_REG_BDR;

typedef struct {

	unsigned COLOR0: 8;
	unsigned COLOR1: 8;
	unsigned COLOR2: 8;
	unsigned COLOR3: 8;

} sGTX_REG_BCLR;

typedef struct {

	unsigned Reserved1: 8;
	unsigned Width: 8;

} sGTX_REG_BPW;

typedef struct {

	unsigned M: 1;
	unsigned F: 1;
	unsigned T: 1;
	unsigned K: 1;
	unsigned Reserved1: 8;
	unsigned Offs: 4;

} sGTX_REG_BPO;



/* Infrared */
typedef struct {

	unsigned Reserved1: 5;
	unsigned CarrierWavePeriod: 11;

} sGTX_REG_CWP;

typedef struct {

	unsigned Reserved1: 6;
	unsigned WavePulseHigh: 10;

} sGTX_REG_CWPH;

typedef struct {

	unsigned Reserved1: 4;
	unsigned P: 1;
	unsigned E: 1;
	unsigned MSP: 10;

} sGTX_REG_MSPR;

typedef struct {

	unsigned Reserved1: 8;
	unsigned PulseLowLen: 8;

} sGTX_REG_MSPL;

typedef struct {

	unsigned Reserved1: 7;
	unsigned S: 1;
	unsigned RTC: 8;

} sGTX_REG_RTC;

typedef struct {

	unsigned Reserved1: 3;
	unsigned TickPeriod: 13;

} sGTX_REG_RTP;



/* SPI */
typedef struct {

	unsigned DataMSB: 8;
	unsigned DataLSB: 8;

} sGTX_REG_SPID;

typedef struct {

	unsigned Reserved1: 2;
	unsigned M: 1;
	unsigned Speed: 5;
	unsigned D: 1;
	unsigned Reserved2: 4;
	unsigned L: 1;
	unsigned R: 1;
	unsigned C: 1;

} sGTX_REG_SPIC;



/* PCM Audio */
typedef struct {

	unsigned NSAMP: 10;
	unsigned Addr: 21;
	unsigned W: 1;

} sGTX_REG_PCMA;

typedef struct {

	unsigned PCMAL: 7;
	unsigned Reserved1: 1;
	unsigned PCMAR: 7;
	unsigned Reserved2: 1;
	unsigned MPEGAL: 7;
	unsigned Reserved3: 1;
	unsigned MPEGAR: 7;
	unsigned Reserved4: 1;

} sGTX_REG_PCMN;

typedef struct {

	unsigned R: 2;
	unsigned W: 1;
	unsigned C: 1;
	unsigned S: 1;
	unsigned T: 1;
	unsigned V: 1;
	unsigned P: 1;
	unsigned M: 1;
	unsigned I: 1;
	unsigned ADV: 2;
	unsigned ACD: 2;
	unsigned BCD: 2;

} sGTX_REG_PCMC;

typedef struct {

	unsigned Reserved1: 10;
	unsigned B: 21;
	unsigned Reserved2: 1;

} sGTX_REG_PCMD;



/* Video */
typedef struct {

	unsigned Reserved1: 7;
	unsigned E: 1;
	unsigned Y: 8;
	unsigned Cr: 8;
	unsigned Cb: 8;

} sGTX_REG_VBR;

typedef struct {

	unsigned S: 1;
	unsigned P: 1;
	unsigned C: 1;
	unsigned A: 1;
	unsigned HP: 2;
	unsigned FP: 2;
	unsigned E: 1;
	unsigned D: 1;
	unsigned N: 1;
	unsigned DELAY: 5;

} sGTX_REG_VCR;

typedef struct {

	unsigned Reserved1: 6;
	unsigned LINE: 9;
	unsigned F: 1;

} sGTX_REG_VLC;

typedef struct {

	unsigned E: 1;
	unsigned Reserved1: 5;
	unsigned LINE: 9;
	unsigned F: 1;

} sGTX_REG_VLI1;

typedef struct {

	unsigned SD: 2;
	unsigned Reserved1: 4;
	unsigned Width: 10;

} sGTX_REG_VHT;

typedef struct {

	unsigned VBI: 5;
	unsigned Reserved1: 1;
	unsigned Lines: 10;

} sGTX_REG_VLT;

typedef struct {

	unsigned E: 1;
	unsigned Reserved1: 5;
	unsigned LINE: 9;
	unsigned F: 1;

} sGTX_REG_VLI2;



/* Configuration and Control */
typedef struct {

	unsigned PIG: 1;
	unsigned VCAP: 1;
	unsigned VID: 1;
	unsigned ACLK: 1;
	unsigned COPY: 1;
	unsigned DRAM: 1;
	unsigned PCM: 1;
	unsigned SPI: 1;
	unsigned IR: 1;
	unsigned BLIT: 1;
	unsigned CRC: 1;
	unsigned INT: 1;
	unsigned SCD: 1;
	unsigned SRX: 1;
	unsigned STX: 1;
	unsigned GV: 1;

} sGTX_REG_RR0;

typedef struct {

	unsigned Reserved1: 8;
	unsigned TTX: 1;
	unsigned DAC: 1;
	unsigned RISC: 1;
	unsigned FRMR: 1;
	unsigned CHAN: 1;
	unsigned AVD: 1;
	unsigned IDC: 1;
	unsigned DESC: 1;

} sGTX_REG_RR1;

typedef struct {

	unsigned REV_ID: 4;
	unsigned GOF: 1;
	unsigned SOF: 1;
	unsigned POF: 1;
	unsigned WBD: 1;
	unsigned DD1: 1;
	unsigned DD0: 1;
	unsigned DOD: 1;
	unsigned SPI: 1;
	unsigned _16M: 1;
	unsigned RFD: 1;
	unsigned MAP: 1;
	unsigned RES: 1;

} sGTX_REG_CR0;

typedef struct {

	unsigned BRD_ID: 8;
	unsigned Reserved1: 3;
	unsigned UPQ: 1;
	unsigned TCP: 1;
	unsigned FH: 1;
	unsigned ACP: 1;
	unsigned VCP: 1;

} sGTX_REG_CR1;

typedef struct {

	unsigned Address: 12;
	unsigned Size: 4;

} sGTX_REG_C0CR;

typedef struct {

	unsigned Address: 12;
	unsigned Size: 4;

} sGTX_REG_C1CR;



/* DAC */
typedef struct {

	unsigned DAC_Pulse_Count: 16;
	unsigned Reserved1: 11;
	unsigned Prescale: 5;

} sGTX_REG_DPCR;



/* Framer */
typedef struct {

	unsigned Reserved1: 2;
	unsigned E: 1;
	unsigned PID: 13;

} sGTX_REG_PCRPID;

typedef struct {

	unsigned PCR_Base: 16;

} sGTX_REG_PCR2;

typedef struct {

	unsigned PCR_Base: 16;

} sGTX_REG_PCR1;

typedef struct {

	unsigned PCR_Base: 1;
	unsigned Reserved1: 6;
	unsigned PCR_Extension: 9;

} sGTX_REG_PCR0;

typedef struct {

	unsigned Latched_STC_Base: 16;

} sGTX_REG_LSTC2;

typedef struct {

	unsigned Latched_STC_Base: 16;

} sGTX_REG_LSTC1;

typedef struct {

	unsigned Latched_STC_Base: 1;
	unsigned Reserved1: 6;
	unsigned Latched_STC_Extension: 9;

} sGTX_REG_LSTC0;

typedef struct {

	unsigned STC_Count: 16;

} sGTX_REG_STCC2;

typedef struct {

	unsigned STC_Count: 16;

} sGTX_REG_STCC1;

typedef struct {

	unsigned STC_Count: 1;
	unsigned Reserved1: 6;
	unsigned STC_Extension: 9;

} sGTX_REG_STCC0;

typedef struct {

	unsigned FE: 1;
	unsigned FH: 1;
	unsigned TEI: 1;
	unsigned DM: 1;
	unsigned CDP: 1;
	unsigned CCP: 1;
	unsigned DO: 1;
	unsigned FD: 1;
	unsigned SyncByte: 8;

} sGTX_REG_FCR;

typedef struct {

	unsigned Reserved1: 8;
	unsigned SyncDrop: 3;
	unsigned SyncLock: 5;

} sGTX_REG_SYNCH;

typedef struct {

	unsigned Reserved1: 9;
	unsigned FIFO_Depth: 7;

} sGTX_REG_PFIFO;



/* IDC Interface */
typedef struct {

	unsigned Reserved1: 2;
	unsigned SN: 1;
	unsigned VD: 1;
	unsigned Byte2Rd: 4;
	unsigned MM: 1;
	unsigned LB: 1;
	unsigned FR: 1;
	unsigned FT: 1;
	unsigned IE: 1;
	unsigned RS: 1;
	unsigned ME: 1;
	unsigned SE: 1;

} sGTX_REG_IDCCR;

typedef struct {

	unsigned LA: 4;
	unsigned SN: 1;
	unsigned TxE: 1;
	unsigned SD: 1;
	unsigned RS: 1;
	unsigned SAd: 1;
	unsigned MI: 1;
	unsigned SI: 1;
	unsigned NAK: 1;
	unsigned AK: 1;
	unsigned RxR: 1;
	unsigned Gen: 1;
	unsigned CSD: 1;

} sGTX_REG_IDCSR;

typedef struct {

	unsigned Reserved1: 9;
	unsigned SAddr: 7;

} sGTX_REG_IDCSA;

typedef struct {

	unsigned Reserved1: 8;
	unsigned RxData: 8;

} sGTX_REG_IDCRD;

typedef struct {

	unsigned Reserved1: 8;
	unsigned MAddr: 7;
	unsigned RW: 1;

} sGTX_REG_IDCMA;

typedef struct {

	unsigned Reserved1: 8;
	unsigned TxData: 8;

} sGTX_REG_IDCTD;

typedef struct {

	unsigned Reserved1: 9;
	unsigned SCL: 7;

} sGTX_REG_IDCC;

typedef struct {

	unsigned Reserved1: 4;
	unsigned Byte2Rd: 4;
	unsigned RxCNT: 4;
	unsigned TxCNT: 4;

} sGTX_REG_IDCFF;



/* Audio/Video Decoder Interface */
typedef struct {

	unsigned HEN: 1;
	unsigned HAA: 1;
	unsigned HWT: 1;
	unsigned VRQ: 1;
	unsigned ARQ: 1;
	unsigned VEN: 1;
	unsigned AEN: 1;
	unsigned INV: 1;
	unsigned SerRT_Max: 2;
	unsigned ParRT_Max: 2;
	unsigned Audio_Xfer_Rate: 4;
	unsigned Reserved1: 11;
	unsigned H: 1;
	unsigned AS: 1;
	unsigned AP: 1;
	unsigned VS: 1;
	unsigned VP: 1;

} sGTX_REG_AVI;



/* RISC Engine */
typedef struct {

	unsigned Reserved1: 7;
	unsigned PC: 9;

} sGTX_REG_RISCPC;

typedef struct {

	unsigned Reserved1: 14;
	unsigned SP: 1;
	unsigned SS: 1;

} sGTX_REG_RISCCON;



/* Queue Write Pointer */
typedef struct {

	unsigned Reserved1: 6;
	unsigned Q_Size: 4;
	unsigned Upper_WD_n: 6;

} sGTX_REG_QWPnH;

typedef struct {

	unsigned Queue_n_Write_Pointer: 16;

} sGTX_REG_QWPnL;

typedef struct {

	unsigned QIM: 1;
	unsigned InterruptMatchingAddress: 5;
	unsigned Reserved1: 10;

} sGTX_REG_QIR;



/* Audio Queue Manager */
typedef struct {

	unsigned AudioQueueReadPointer: 16;

} sGTX_REG_AQRPL;

typedef struct {

	unsigned H: 1;
	unsigned Reserved1: 9;
	unsigned AudioQueueReadPointer: 6;

} sGTX_REG_AQRPH;

typedef struct {

	unsigned AudioQueueWritePointer: 16;

} sGTX_REG_AQWPL;

typedef struct {

	unsigned Reserved1: 6;
	unsigned Q_Size: 4;
	unsigned AudioQueueWritePointer: 6;

} sGTX_REG_AQWPH;



/* Teletext Queue Manager */
typedef struct {

	unsigned TeletextQueueReadPointer: 16;

} sGTX_REG_TQRPL;

typedef struct {

	unsigned H: 1;
	unsigned Reserved1: 9;
	unsigned TeletextQueueReadPointer: 6;

} sGTX_REG_TQRPH;

typedef struct {

	unsigned TeletextQueueWritePointer: 16;

} sGTX_REG_TQWPL;

typedef struct {

	unsigned Reserved1: 6;
	unsigned Q_Size: 4;
	unsigned TeletextQueueWritePointer: 6;

} sGTX_REG_TQWPH;



/* Video Queue Manager */
typedef struct {

	unsigned VideoQueueReadPointer: 16;

} sGTX_REG_VQRPL;

typedef struct {

	unsigned H: 1;
	unsigned Reserved1: 9;
	unsigned VideoQueueReadPointer: 6;

} sGTX_REG_VQRPH;

typedef struct {

	unsigned VideoOueueWritePointer: 16;

} sGTX_REG_VQWPL;

typedef struct {

	unsigned Reserved1: 6;
	unsigned Q_Size: 4;
	unsigned VideoQueueWritePointer: 6;

} sGTX_REG_VQWPH;



/* Queue Interrupt */
typedef struct {

	unsigned M: 1;
	unsigned BLOCK: 5;
	unsigned Reserved1: 10;

} sGTX_REG_QIn;



/* Copy Engine */
typedef struct {

	unsigned Word_n: 16;

} sGTX_REG_CBWn;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 22;

} sGTX_REG_CCSA;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 22;

} sGTX_REG_CDA;

typedef struct {

	unsigned Reserved1: 3;
	unsigned NS: 1;
	unsigned RWT: 1;
	unsigned DADD: 1;
	unsigned WE: 1;
	unsigned RE: 1;
	unsigned Reserved2: 4;
	unsigned LEN: 4;

} sGTX_REG_CCOM;

typedef struct {

	unsigned Reserved1: 8;
	unsigned T_Color: 8;

} sGTX_REG_RWTC;

typedef struct {

	unsigned Reserved1: 2;
	unsigned R: 1;
	unsigned NS: 1;
	unsigned RWT: 1;
	unsigned DADD: 1;
	unsigned WE: 1;
	unsigned RE: 1;
	unsigned Reserved2: 4;
	unsigned LEN: 4;

} sGTX_REG_CCOM2;

typedef struct {

	unsigned Reserved1: 2;
	unsigned R: 1;
	unsigned NS: 1;
	unsigned RWT: 1;
	unsigned DADD: 1;
	unsigned WE: 1;
	unsigned RE: 1;
	unsigned Reserved2: 4;
	unsigned LEN: 4;

} sGTX_REG_CCOM3;



/* Video Plane Display */
typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 21;
	unsigned E: 1;

} sGTX_REG_VPSA;

typedef struct {

	unsigned OFFSET: 16;
	unsigned Reserved1: 4;
	unsigned STRIDE: 11;
	unsigned B: 1;

} sGTX_REG_VPO;

typedef struct {

	unsigned Reserved1: 6;
	unsigned HPOS: 9;
	unsigned Reserved2: 7;
	unsigned VPOS: 9;
	unsigned F: 1;

} sGTX_REG_VPP;

typedef struct {

	unsigned Reserved1: 6;
	unsigned WIDTH: 9;
	unsigned S: 1;
	unsigned Reserved2: 6;
	unsigned HEIGHT: 9;
	unsigned P: 1;

} sGTX_REG_VPS;

typedef struct {

	unsigned Reserved1: 14;
	unsigned EXT: 2;

} sGTX_REG_VPOE;



/* Video Capture */
typedef struct {

	unsigned Reserved1: 10;
	unsigned Addr: 21;
	unsigned E: 1;

} sGTX_REG_VCSA;

typedef struct {

	unsigned V: 1;
	unsigned Reserved1: 5;
	unsigned HPOS: 9;
	unsigned Reserved2: 3;
	unsigned OVOFFS: 4;
	unsigned EVPOS: 9;
	unsigned Reserved3: 1;

} sGTX_REG_VCSP;

typedef struct {

	unsigned HDEC: 4;
	unsigned Reserved1: 2;
	unsigned HSIZE: 9;
	unsigned F: 1;
	unsigned VDEC: 4;
	unsigned Reserved2: 2;
	unsigned VSIZE: 9;
	unsigned B: 1;

} sGTX_REG_VCS;



/* Semaphore */
typedef struct {

	unsigned Reserved1: 8;
	unsigned PID: 7;
	unsigned A: 1;

} sGTX_REG_SEM1;

typedef struct {

	unsigned Reserved1: 8;
	unsigned PID: 7;
	unsigned A: 1;

} sGTX_REG_SEM2;



/* Teletext */
typedef struct {

	unsigned PTS0: 16;

} sGTX_REG_PTS0;

typedef struct {

	unsigned PTS1: 16;

} sGTX_REG_PTS1;

typedef struct {

	unsigned PTS_OFFSET: 16;

} sGTX_REG_PTSO;

typedef struct {

	unsigned Reserved1: 1;
	unsigned PE: 1;
	unsigned Reserved2: 1;
	unsigned RP: 1;
	unsigned FP: 1;
	unsigned Reserved3: 1;
	unsigned GO: 1;
	unsigned IE: 1;
	unsigned Data_ID: 8;

} sGTX_REG_TTCR;

typedef struct {

	unsigned P: 1;
	unsigned R: 1;
	unsigned E: 1;
	unsigned Reserved1: 8;
	unsigned State: 5;

} sGTX_REG_TSR;



/* Infrared */
typedef struct {

	unsigned Reserved1: 7;
	unsigned P: 1;
	unsigned Filt_H: 4;
	unsigned Filt_L: 4;

} sGTX_REG_RFR;

typedef struct {

	unsigned Reserved1: 8;
	unsigned RTCH: 8;

} sGTX_REG_RPH;

typedef struct {

	unsigned Reserved1: 10;
	unsigned Address: 13;
	unsigned Reserved2: 9;

} sGTX_REG_IRQA;

typedef struct {

	unsigned E: 1;
	unsigned L: 1;
	unsigned Reserved1: 7;
	unsigned Offset: 7;

} sGTX_REG_IRRE;

typedef struct {

	unsigned E: 1;
	unsigned C: 1;
	unsigned Reserved1: 7;
	unsigned Offset: 7;

} sGTX_REG_IRTE;

typedef struct {

	unsigned Reserved1: 9;
	unsigned Offset: 7;

} sGTX_REG_IRRO;

typedef struct {

	unsigned Reserved1: 9;
	unsigned Offset: 7;

} sGTX_REG_IRTO;

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

#define gtx_reg_set_32s(register, field, value) { u32 tmp_reg_val = gtx_reg_32(register); ((sGTX_REG_##register *)&tmp_reg_val)->field = value; gtx_reg_32(register) = tmp_reg_val; }
#define gtx_reg_set_16s(register, field, value) { u16 tmp_reg_val = gtx_reg_16(register); ((sGTX_REG_##register *)&tmp_reg_val)->field = value; gtx_reg_16(register) = tmp_reg_val; }
#define gtx_reg_set(register, field, value) do { if (sizeof(sGTX_REG_##register) == 4) gtx_reg_set_32s(register, field, value) else if (sizeof(sGTX_REG_##register ) == 2) gtx_reg_set_16s(register, field, value) else printk("ERROR: struct size is %d\n", sizeof(sGTX_REG_##register)); } while(0)
#define gtx_reg_set_bit(register, bit) gtx_reg_set(register, bit, 1)
#define gtx_reg_clear_bit(register, bit) gtx_reg_set(register, bit, 0)

#endif /* __GTX_H__ */
