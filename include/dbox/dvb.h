#ifndef __DVB_H
#define __DVB_H

#include <linux/types.h>
#include <ost/frontend.h>

/* remove wrong ids from kernel includes */
#ifdef I2C_DRIVERID_VES1893
#undef I2C_DRIVERID_VES1893
#endif
#ifdef I2C_DRIVERID_VES1820
#undef I2C_DRIVERID_VES1820
#endif

#define I2C_DRIVERID_VES1893    32      /* DVB-S decoder                */
#define I2C_DRIVERID_VES1820    33      /* DVB-C decoder                */

#define BASE_VIDIOCPRIVATE      192             /* 192-255 are private */

#define DVB_SET_FRONTEND        _IOW('v',  BASE_VIDIOCPRIVATE+0x10, struct frontend)
#define DVB_GET_FRONTEND        _IOR('v',  BASE_VIDIOCPRIVATE+0x11, struct frontend)
#define DVB_INIT_FRONTEND       _IOR('v',  BASE_VIDIOCPRIVATE+0x12, void)
#define DVB_RESET               _IOR('v',  BASE_VIDIOCPRIVATE+0x13, void)
#define DVB_WRITEREG            _IOR('v',  BASE_VIDIOCPRIVATE+0x14, u8 *)
#define DVB_READREG             _IOR('v',  BASE_VIDIOCPRIVATE+0x15, u8 *)

/* frontend struct, should move to videodev.h at some time */

struct frontend {
	int type;              /* type of frontend (tv tuner, dvb tuner/decoder, etc. */
#define FRONT_TV   0
#define FRONT_DVBS 1
#define FRONT_DVBC 2
#define FRONT_DVBT 3

	/* Sat line control */
        int power;             /* LNB power 0=off/pass through, 1=on */
	int volt;              /* 14/18V (V=0/H=1) */
	int ttk;               /* 22KHz */
	int diseqc;            /* Diseqc input select */
        

	/* cable line control */



	/* signal decoding, transponder info */

	__u32 freq;            /* offset frequency (from local oscillator) in Hz */
        int AFC;
	__u32 srate;           /* Symbol rate in Hz */
	int qam;               /* QAM mode for cable decoder, sat is always QPSK */
        int inv;               /* Inversion */
	int fec;               /* Forward Error Correction */

        /* channel info */
	__u16 video_pid;        
	__u16 audio_pid;
        __u16 tt_pid;          /* Teletext PID */
        __u16 pnr;             /* Program number = Service ID */
        int channel_flags;             
#define DVB_CHANNEL_FTA        0
#define DVB_CHANNEL_CA         1


	/* status information */

	int fsync;             /* frequency sync (from tuner) */
        __u32 curfreq;         /* frequency which is actually used, e.g. after AFC */

	int sync;              /* sync from decoder */
#define DVB_SYNC_SIGNAL        1
#define DVB_SYNC_CARRIER       2
#define DVB_SYNC_VITERBI       4
#define DVB_SYNC_FSYNC         8
#define DVB_SYNC_FRONT        16
	__s32 afc;               /* frequency offset in Hz */
	__u16 agc;             /* gain */
	__u16 nest;            /* noise estimation */
        __u32 vber;            /* viterbi bit error rate */

	int flags;
#define FRONT_TP_CHANGED  1 
#define FRONT_FREQ_CHANGED 2
#define FRONT_RATE_CHANGED 4
};

typedef struct qpsk_s {
        struct qpskEvent        events[8];
        int                     eventw;
        int                     eventr;
        int                     overflow;
        wait_queue_head_t       eventq;
        spinlock_t              eventlock;
} qpsk_t;

#endif