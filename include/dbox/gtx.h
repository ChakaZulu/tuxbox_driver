#ifndef __GTX_H
#define __GTX_H

#define GTX_PHYSBASE    0x8000000
#define GTX_FB_OFFSET	0x0100000
                    
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

#define rw(a) (*((volatile unsigned long*)(gtxreg+g ## a)))
#define rh(a) (*((volatile unsigned short*)(gtxreg+g ## a)))

#define rwn(a) (*((volatile unsigned long*)(gtxreg+a))) 
#define rhn(a) (*((volatile unsigned short*)(gtxreg+a))) 

#define dumpw(a) printk(#a ": %08x\n", rw(a));
#define dumph(a) printk(#a ":     %04x\n", rh(a));

#define VCR_SET_HP(X)    rh(VCR) = ((rh(VCR)&(~(3<<10))) | ((X&3)<<10))
#define VCR_SET_FP(X)    rh(VCR) = ((rh(VCR)&(~(3<<8 ))) | ((X&3)<<8 ))
#define GVP_SET_SPP(X)   rw(GVP) = ((rw(GVP)&(~(0x01F<<26))) | ((X&0x1F)<<26))
#define GVP_SET_X(X)     rw(GVP) = ((rw(GVP)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVP_SET_Y(X)     rw(GVP) = ((rw(GVP)&(~0x3FF))|(X&0x3FF))
#define GVP_SET_COORD(X,Y) GVP_SET_X(X); GVP_SET_Y(Y)

#define GVS_SET_XSZ(X)   rw(GVS) = ((rw(GVS)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVS_SET_YSZ(X)   rw(GVS) = ((rw(GVS)&(~0x3FF))|(X&0x3FF))





#define GTX_REG_VLI1	0x0F8
#define GTX_REG_RR0	0x100
#define GTX_REG_VPSA	0x240
#define GTX_REG_VPO	0x244
#define GTX_REG_VPP	0x248
#define GTX_REG_VPS	0x24C
#define GTX_REG_VCSA	0x260
#define GTX_REG_VCSP	0x264
#define GTX_REG_VCS	0x268




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





extern int gtx_allocate_dram(int size, int align);
extern int gtx_allocate_irq(int reg, int bit, void (*isr)(int, int));
extern void gtx_free_irq(int reg, int bit);

extern unsigned char* gtx_get_mem(void);
extern unsigned char* gtx_get_reg(void);

#define gtx_get_mem_addr gtx_get_mem
#define gtx_get_reg_addr gtx_get_reg

#define gtx_reg_16(register) ((unsigned short)(*((unsigned short*)(gtx_get_reg_addr() + GTX_REG_ ## register))))
#define gtx_reg_16n(offset) ((unsigned short)(*((unsigned short*)(gtx_get_reg_addr() + offset))))
#define gtx_reg_32(register) ((unsigned int)(*((unsigned int*)(gtx_get_reg_addr() + GTX_REG_ ## register))))
#define gtx_reg_32n(offset) ((unsigned int)(*((unsigned int*)(gtx_get_reg_addr() + offset))))
#define gtx_reg_o(offset) (gtx_get_reg_addr() + offset)
#define gtx_reg_s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_32s(register) ((sGTX_REG_##register *)(&gtx_reg_32(register)))
#define gtx_reg_16s(register) ((sGTX_REG_##register *)(&gtx_reg_16(register)))

#endif
