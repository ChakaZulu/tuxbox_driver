#ifndef __GTX_DMX_H
#define __GTX_DMX_H

#define GTXDMX_MAJOR    62

#define NUM_QUEUES      32
#define VIDEO_QUEUE     0
#define AUDIO_QUEUE     1
#define TELETEXT_QUEUE  2
#define USER_QUEUE_START 3
#define LAST_USER_QUEUE 30
#define MESSAGE_QUEUE   31
#define HIGHSPEED_QUEUE 32

#define NUM_PID_FILTER  32

typedef struct Pcr_s
{
  u32 hi, lo;
} Pcr_t;

// timing stuff
/* PCR Processing for DAC output */
#define TICK_RATE (27000000)                  /* 27MHz tick rate */
#define TICK_HI_RATE ((TICK_RATE)/600)        /* 45KHz tick rate ; for hi portion of Pcr */
#define MAX_HI_DELTA ((TICK_HI_RATE)/5)       /* now 200 mse before 100 msec */
#define TIME_THRESHOLD ((TICK_RATE)/4)        /* 250 msec @ 27 MHz */
#if 1
#define TIME_HI_THRESHOLD ((TICK_HI_RATE)/2)  /* 250 msec @ 45 KHz */
#else
#define TIME_HI_THRESHOLD ((TICK_HI_RATE)/2)  /* 500 msec @ 45 KHz */
#endif
#define MAX_DELTA_COUNT 3               /* max # of large deltas detected before declaring discontinuity */
#define MAX_DAC 0x7FFF                  /*#define MAX_DAC 0x1FFF*/  /* maximum DAC range (+/-) */
#define LARGE_POSITIVE_NUMBER 32760
#define AV_SYNC_DELAY 1000
                             
                        // nokia api stuff
#define GTX_OUTPUT_TS           0
#define GTX_OUTPUT_TSPAYLOAD    2
#define GTX_OUTPUT_PESPAYLOAD   3
#define GTX_OUTPUT_8BYTE        4
#define GTX_OUTPUT_16BYTE       5

#define GTX_FILTER_PID          0
#define GTX_FILTER_SECTION      1


#define DMX_TYPE_TS  0
#define DMX_TYPE_SEC 1
#define DMX_TYPE_PES 2

#define DMX_STATE_FREE      0
#define DMX_STATE_ALLOCATED 1
#define DMX_STATE_SET       2
#define DMX_STATE_READY     3
#define DMX_STATE_GO        4


#endif
