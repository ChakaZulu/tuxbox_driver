#ifdef MODULE

#define TM_DRAM  0x00
#define TM_GBUS  0x80
#define TM_SRAM  0xC0

extern u32 avia_rd(int mode, int address);
extern void avia_wr(int mode, u32 address, u32 data);

extern u32 avia_command(u32 cmd, ...);
extern u32 avia_wait(u32 status);

#define wGB(a, d) avia_wr(TM_GBUS, a, d)
#define rGB(a) avia_rd(TM_GBUS, a)
#define wSR(a, d) avia_wr(TM_SRAM, a, d)
#define rSR(a) avia_rd(TM_SRAM, a)
#define wDR(a, d) avia_wr(TM_DRAM, a, d)
#define rDR(a) avia_rd(TM_DRAM, a)

#define Abort            0x8120
#define Digest           0x0621
#define Fade             0x0223
#define Freeze           0x0125
#define NewPlayMode      0x0028
#define Pause            0x022A
#define Reset            0x802D
#define Resume           0x002E

#define SelectStream     0x0231
#define SetFill          0x0532

#define CancelStill      0x0040
#define DigitalStill     0x0241
#define NewChannel       0x8342
#define Play             0x0343
#define SetWindow        0x0244
#define WindowClear      0x0045
#define SetStreamType    0x8146
#define SwitchOSDBuffer  0x8254
#define CancelTimeCode   0x0147
#define SetTimeCode1     0x0148
#define SetTimeCode      0x0248
#define GenerateTone     0x0449

#define OSDCopyData      0x0350
#define OSDCopyRegion    0x0651
#define OSDFillData      0x0352
#define OSDFillRegion    0x0553
#define OSDXORData       0x0357
#define OSDXORegion      0x0658
#define PCM_Mix          0x0659
#define PCM_MakeWaves    0x065A

#endif