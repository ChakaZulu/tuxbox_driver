#ifndef __GTX_H
#define __GTX_H


#define GTX_REG_BASE		0x08400000
#define GTX_REG_SIZE		0x00003000
#define GTX_MEM_BASE		0x08000000
#define GTX_MEM_SIZE		0x00100000


#define GTX_FB_OFFSET		0x0100000


#define GTX_INTERRUPT		SIU_IRQ1

                    
#undef CR0
#define gRR0            0x100
#define gRR1            0x102
#define gCR0            0x104
#define gCR1            0x106

#define gVBR            0xF0
#define gVCR            0xF4
#define gVLC            0xF6
#define gVLI1           0xF8
#define gVHT            0xFA
#define gVLT            0xFC
#define gVLI2           0xFE

#define gGMR            0x00
#define gCLTA           0x04
#define gCLTD           0x06
#define gTCR            0x08
#define gCCR            0x0A
#define gGVSA           0x0C
#define gGVP            0x10
#define gGVS            0x14
#define gCSA            0x18
#define gCPOS           0x1C
#define gGFUNC          0x20

#define gQWPnL          0x180
#define gQWPnH          0x182

#define gISR0           0x80
#define gISR1           0x82
#define gISR2           0x84
#define gISR3           0x92            // ?!

#define gIMR0           0x86
#define gIMR1           0x88
#define gIMR2           0x8A
#define gIMR3           0x94            // ?!

#define gIPR0           0x8C
#define gIPR1           0x8E
#define gIPR2           0x90
#define gIPR3           0x94            // ?!

#define gAVI            0x150

#define gRISCPC         0x170
#define gRISCCON        0x178
#define gRISC           0x1000

#define gPCRPID         0x120
#define gPCR2           0x122
#define gPCR1           0x124
#define gPCR0           0x126
#define gLSTC2          0x128
#define gLSTC1          0x12A
#define gLSTC0          0x12C
#define gSTC2           0x12E
#define gSTC1           0x130
#define gSTC0           0x132

#define gFCR            0x134
#define gSYNCH          0x136
#define gPFIFO          0x138
#define gDPCR           0x110

#define gPCMA           0xe0
#define gPCMN           0xe4
#define gPCMC           0xe8
#define gPCMD           0xec

#define gPTS0			0x280
#define gPTS1			0x282
#define gPTSO			0x284
#define gTTCR			0x286
#define gTTSR			0x288

#define gAQRPL			0x1E0
#define gAQRPH			0x1E2
#define gAQWPL			0x1E4
#define gAQWPH			0x1E6
#define gTQRPL			0x1E8
#define gTQRPH			0x1EA
#define gTQWPL			0x1EC
#define gTQWPH			0x1EE
#define gVQRPL			0x1F0
#define gVQRPH			0x1F2
#define gVQWPL			0x1F4
#define gVQWPH			0x1F6

#define gC0CR			0x10C
#define gC1CR			0x10E

#define gQI0            0x1C0

#define gVSCA 		0x260
#define gVSCP			0x264
#define gVCS			0x268

#define VCR_SET_HP(X)    rh(VCR) = ((rh(VCR)&(~(3<<10))) | ((X&3)<<10))
#define VCR_SET_FP(X)    rh(VCR) = ((rh(VCR)&(~(3<<8 ))) | ((X&3)<<8 ))
#define GVP_SET_SPP(X)   rw(GVP) = ((rw(GVP)&(~(0x01F<<26))) | ((X&0x1F)<<26))
#define GVP_SET_X(X)     rw(GVP) = ((rw(GVP)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVP_SET_Y(X)     rw(GVP) = ((rw(GVP)&(~0x3FF))|(X&0x3FF))
#define GVP_SET_COORD(X,Y) GVP_SET_X(X); GVP_SET_Y(Y)

#define GVS_SET_XSZ(X)   rw(GVS) = ((rw(GVS)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVS_SET_YSZ(X)   rw(GVS) = ((rw(GVS)&(~0x3FF))|(X&0x3FF))


#define GTX_REG_ISR0	0x080
#define GTX_REG_ISR1	0x082
#define GTX_REG_ISR2	0x084
#define GTX_REG_ISR3	0x092            // ?!
#define GTX_REG_IMR0	0x086
#define GTX_REG_IMR1	0x088
#define GTX_REG_IMR2	0x08A
#define GTX_REG_IMR3	0x094            // ?!
#define GTX_REG_IPR0	0x08C
#define GTX_REG_IPR1	0x08E
#define GTX_REG_IPR2	0x090
#define GTX_REG_IPR3	0x094            // ?!
#define GTX_REG_PCMA	0x0E0
#define GTX_REG_PCMN	0x0E4
#define GTX_REG_PCMC	0x0E8
#define GTX_REG_PCMD	0x0EC
#define GTX_REG_VLI1	0x0F8
#define GTX_REG_RR0	0x100
#define GTX_REG_RR1	0x102
#define GTX_REG_CR0	0x104
#define GTX_REG_CR1	0x106
#define GTX_REG_VPSA	0x240
#define GTX_REG_VPO	0x244
#define GTX_REG_VPP	0x248
#define GTX_REG_VPS	0x24C
#define GTX_REG_VCSA	0x260
#define GTX_REG_VCSP	0x264
#define GTX_REG_VCS	0x268




#define GTX_IRQ_REG_ISR0	0
#define GTX_IRQ_REG_ISR1	1
#define GTX_IRQ_REG_ISR2	2
#define GTX_IRQ_REG_ISR3	3

#define GTX_IRQ_PCR		AVIA_GT_IRQ(GTX_IRQ_REG_ISR0, 8)
#define GTX_IRQ_IR_TX		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 8)
#define GTX_IRQ_IR_RX		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 9)
#define GTX_IRQ_PCM_PF		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 10)
#define GTX_IRQ_PCM_AD		AVIA_GT_IRQ(GTX_IRQ_REG_ISR1, 12)




typedef struct {

    unsigned short NSAMP: 10;
    unsigned int Addr: 21;
    unsigned char W: 1;

} sGTX_REG_PCMA;

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

    unsigned short Reserved1: 10;
    unsigned int Addr: 21;
    unsigned char E: 1;

} sGTX_REG_VCSA;

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

extern void avia_gt_gtx_clear_irq(unsigned char irq_reg, unsigned char irq_bit);
extern unsigned short avia_gt_gtx_get_irq_mask(unsigned char irq_reg);
extern unsigned short avia_gt_gtx_get_irq_status(unsigned char irq_reg);
extern void avia_gt_gtx_mask_irq(unsigned char irq_reg, unsigned char irq_bit);
extern void avia_gt_gtx_unmask_irq(unsigned char irq_reg, unsigned char irq_bit);
extern void avia_gt_gtx_init(void);
extern void avia_gt_gtx_exit(void);

#define gtx_reg_16(register) ((unsigned short)(*((unsigned short*)(avia_gt_get_reg_addr() + GTX_REG_ ## register))))
#define gtx_reg_16n(offset) ((unsigned short)(*((unsigned short*)(avia_gt_get_reg_addr() + offset))))
#define gtx_reg_32(register) ((unsigned int)(*((unsigned int*)(avia_gt_get_reg_addr() + GTX_REG_ ## register))))
#define gtx_reg_32n(offset) ((unsigned int)(*((unsigned int*)(avia_gt_get_reg_addr() + offset))))
#define gtx_reg_o(offset) (avia_gt_get_reg_addr() + offset)
#define gtx_reg_s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_32s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_16s(register) ((sGTX_REG_##register *)(&gtx_reg_16(register)))

#define rw(a) (*((volatile unsigned long*)(avia_gt_get_reg_addr()+g ## a)))
#define rh(a) (*((volatile unsigned short*)(avia_gt_get_reg_addr()+g ## a)))

#define rwn(a) (*((volatile unsigned long*)(avia_gt_get_reg_addr()+a))) 
#define rhn(a) (*((volatile unsigned short*)(avia_gt_get_reg_addr()+a))) 


#endif
