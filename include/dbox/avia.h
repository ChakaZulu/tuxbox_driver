#ifdef MODULE

#define TM_DRAM  0x00
#define TM_GBUS  0x80
#define TM_SRAM  0xC0

extern u32 avia_rd(int mode, int address);
extern void avia_wr(int mode, u32 address, u32 data);

extern u32 avia_command(u32 cmd, ...);
extern u32 avia_wait(u32 status);
extern void avia_flush_pcr(void);

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

/* DRAM Register */
#define AC3_ACMOD_CMIXLEV 		0x418
#define AC3_BSI_FRAME 			0x40c
#define AC3_BSI_IS_BEING_READ 	0x404
#define AC3_BSI_VALID 			0x408
#define AC3_BSID_BSMOD			0x414
#define AC3_C_LEVEL				0x130
#define AC3_COMPR_LANGCOD		0x424
#define AC3_DIALNORM2_COMPR2	0x42c
#define AC3_FRAME_NUMBER		0x400
#define AC3_FSCOD_FRMSIZECOD	0x410
#define AC3_HIGH_CUT			0x11c
#define AC3_L_LEVEL				0x12c
#define AC3_LANGCOD2_MIXLEV2	0x430
#define AC3_LFE_LEVEL			0x140
#define AC3_LFE_OUTPUT_ENABLE	0x124
#define AC3_LFEON_DIALNORM		0x420
#define AC3_LOW_BOOST			0x118
#define AC3_MIXLEV_ROOMTYP		0x428
#define AC3_OPERATION_MODE		0x114
#define AC3_ORIGBS_TIMECOD1		0x438
#define AC3_OUTPUT_MODE			0x110
#define AC3_PCM_SCALE_FACTOR	0x120
#define AC3_R_LEVEL				0x134
#define AC3_ROOMTYP2_COPYRIGHTB	0x434
#define AC3_SURMIXLEV_DSURMOD	0x41c
#define AC3_SR_LEVEL			0x13c
#define AC3_SL_LEVEL			0x138
#define AC3_TIMECOD2_EBITS		0x43c

#define ARGUMENT_1				0x44
#define ARGUMENT_2				0x48
#define ARGUMENT_3				0x4c
#define ARGUMENT_4				0x50
#define ARGUMENT_5				0x54
#define ARGUMENT_6				0x58

#define ASPECT_RATIO			0x3b8
#define ASPECT_RATIO_MODE		0x84

#define AUDIO_ATTENUATION				0xf4
#define AUDIO_CLOCK_SELECTION			0xec
#define AUDIO_CONFIG					0xe0
#define AUDIO_DAC_MODE					0xe8
#define AUDIO_EMPTINESS					0x2cc
#define AUDIO_PTS_DELAY					0x1c0
#define AUDIO_PTS_REPEAT_THRESHOLD_1	0x1c8
#define AUDIO_PTS_REPEAT_THRESHOLD_2	0x1d0
#define AUDIO_PTS_SKIP_THRESHOLD_1		0x1c4
#define AUDIO_PTS_SKIP_THRESHOLD_2		0x1cc
#define AUDIO_TYPE						0x3f0

#define AV_SYNC_MODE		0x1b0

#define BACKGROUND_COLOR	0x9c
#define BIT_RATE			0x3c0
#define BITSTREAM_SOURCE	0x1a4
#define BORDER_COLOR		0x98

#define BUFF_INT_SRC			0x2b4

#define COMMAND					0x40
#define CUR_PIC_DISPLAYED		0x2d0
#define DATE_TIME				0x324
#define DISABLE_OSD				0x250
#define DISP_SIZE_H_V			0x3cc
#define DISPLAY_ASPECT_RATIO	0x80
#define DRAM_INFO				0x68

#define ERR_ASPECT_RATIO_INFORMATION	0xc0
#define ERR_CONCEALMENT_LEVEL			0xb4
#define ERR_FRAME_RATE_CODE				0xc4
#define ERR_HORIZONTAL_SIZE				0xb8
#define ERR_INT_SRC						0x2c4
#define ERR_STILL_DEF_HSIZE				0xac
#define ERR_STILL_DEF_VSIZE				0xb0
#define ERR_VERTICAL_SIZE				0xbc
#define EXTENDED_VERSION				0x334
#define FORCE_CODED_ASPECT_RATIO		0xc8
#define FRAME_RATE						0x3bc
#define GOP_FLAGS						0x3d4
#define H_SIZE							0x3b0
#define I_SLICE							0xd8
#define IEC_958_CHANNEL_STATUS_BITS		0xfc
#define ERR_958_DELAY					0xf0
#define INT_MASK						0x200
#define INT_STATUS						0x2ac
#define INTERPRET_USER_DATA				0x1d8
#define INTERPRET_USER_DATA_MASK		0x1dc
#define MEMORY_MAP						0x21c

#define MIXPCM_BUFSTART		0x580
#define MIXPCM_BUFEND		0x584
#define MIXPCM_BUFPTR		0x588
#define MIXPCM_LOOPCNTR		0x58c
#define MIXPCM_CMDWAITING	0x594

#define ML_HEARTBEAT		0x470
#define MPEG_AUDIO_HEADER1	0x400
#define MPEG_AUDIO_HEADER2	0x404

#define MR_AUD_PTS		0x30c
#define MR_AUD_STC		0x310
#define MR_PIC_PTS		0x2f0
#define MR_PIC_STC		0x2f4

#define MRC_ID				0x2A0
#define MRC_STATUS		0x2A8

#define N_AUD_DECODED	0x2f8
#define N_AUD_ERRORS	0x320
#define N_AUD_SLOWDOWN1	0x300
#define N_AUD_SLOWDOWN2	0x308
#define N_AUD_SPEEDUP1	0x2fc
#define N_AUD_SPEEDUP2	0x304

#define N_DECODED		0x2e4
#define N_REPEATED		0x2ec
#define N_SKIPPED		0x2e8
#define N_SYS_ERRORS	0x318
#define N_VID_ERRORS	0x31c

#define NEW_AUDIO_CONFIG	0x468
#define NEW_AUDIO_MODE		0x460

#define NEXT_PIC_DISPLAYED	0x2d4

#define OSD_BUFFER_END			0x244
#define OSD_BUFFER_IDLE_START	0x248
#define OSD_BUFFER_START		0x240
#define OSD_EVEN_FIELD			0xa0
#define OSD_ODD_FIELD			0xa4
#define OSD_VALID				0x2e0

#define PAN_SCAN_HORIZONTAL_OFFSET	0x8c
#define PAN_SCAN_SOURCE				0x88

#define PIC_HEADER	0x3e4
#define PIC_TYPE	0x3dc

#define PIC1_CLOSED_CAPTION	0x350
#define PIC1_EXTENDED_DATA	0x354
#define PIC1_PAN_SCAN		0x348
#define PIC1_PICTURE_START	0x340
#define PIC1_PTS			0x344
#define PIC1_TREF_PTYP_FLGS	0x358
#define PIC1_USER_DATA		0x34c

#define PIC2_CLOSED_CAPTION		0x370
#define PIC2_EXTENDED_DATA		0x374
#define PIC2_PAN_SCAN			0x368
#define PIC2_PICTURE_START		0x360
#define PIC2_PTS				0x364
#define PIC2_TREF_PTYP_FLGS		0x378
#define PIC2_USER_DATA			0x36c

#define PIC3_CLOSED_CAPTION		0x380
#define PIC3_EXTENDED_DATA		0x390
#define PIC3_PAN_SCAN			0x394
#define PIC3_PICTURE_START		0x388
#define PIC3_PTS				0x384
#define PIC3_TREF_PTYP_FLGS		0x398
#define PIC3_USER_DATA			0x38c

#define PROC_STATE		0x2a0
#define SEQ_FLAGS		0x3c8
#define STATUS_ADDRESS	0x5c
#define TEMP_REF		0x3d8
#define TIME_CODE		0x3d0
#define TM_MODE			0x1a8
#define UCODE_MEMORY	0x6c
#define UND_INT_SRC		0x2b8

#define USER_DATA_BUFFER_END	0x274
#define USER_DATA_BUFFER_START	0x270
#define USER_DATA_READ			0x278
#define USER_DATA_WRITE			0x27c

#define V_SIZE		0x3b4

#define VBV_DELAY	0x3e0
#define VBV_SIZE	0x3c4

#define VERSION		0x330

#define VIDEO_EMPTINESS				0x2c8
#define VIDEO_FIELD					0x2d8
#define VIDEO_MODE					0x7c
#define VIDEO_PTS_DELAY				0x1b4
#define VIDEO_PTS_REPEAT_THRESHOLD	0x1bc
#define VIDEO_PTS_SKIP_THRESHOLD	0x1b8

#define VSYNC_HEARTBEAT				0x46c

#define AUDIO_SEQUENCE_ID	0x540
#define NEW_AUDIO_SEQUENCE	0x544

#endif
