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
#define TICK_QUEUE_MAX  010000	 								/* max video rate before changing queue */
#define TICK_HI_RATE ((TICK_RATE)/600)        /* 45KHz tick rate ; for hi portion of Pcr */
#define MAX_HI_DELTA ((TICK_HI_RATE)/5)       /* now 200 mse before 100 msec */
#define TIME_THRESHOLD ((TICK_RATE)/4)        /* 250 msec @ 27 MHz */
#if 1
#define TIME_HI_THRESHOLD ((TICK_HI_RATE)/2)  /* 250 msec @ 45 KHz */
#else
#define TIME_HI_THRESHOLD ((TICK_HI_RATE)/2)  /* 500 msec @ 45 KHz */
#endif
#define queeue pid==TICK_QUEUE_MAX						/* video queue update rate */
#define MAX_DELTA_COUNT 3               /* max # of large deltas detected before declaring discontinuity */
#define MAX_DAC 0x7FFF                  /*#define MAX_DAC 0x1FFF*/  /* maximum DAC range (+/-) */
#define LARGE_POSITIVE_NUMBER 32760
#define AV_SYNC_DELAY 1000
                             

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid);

                        // nokia api stuff
#define GTX_OUTPUT_TS           0
#define GTX_OUTPUT_TSPAYLOAD    2
#define GTX_OUTPUT_PESPAYLOAD   3
#define GTX_OUTPUT_8BYTE        4
#define GTX_OUTPUT_16BYTE       5

#define GTX_FILTER_PID          0
#define GTX_FILTER_SECTION      1

typedef struct gtx_demux_secfilter_s
{
  dmx_section_filter_t filter;
  int index;
  int state;
  
  struct gtx_demux_secfilter_s *next;
  struct gtx_demux_feed_s *feed;
} gtx_demux_secfilter_t;

typedef struct gtx_demux_filter_s
{
  int index;
  int state;

  int type;

        // PID Entry Specification
        // PID Parsing Control Table
  // type=PID/SECTION
  int output;
  int wait_pusi, invalid, pid;
  int queue, fork, cw_offset, cc, start_up, pec;
  // type=SECTION
  int filt_tab_idx, no_of_filters;

  struct gtx_demux_feed_s *feed;
} gtx_demux_filter_t;

typedef struct gtx_demux_feed_s
{
  union
  {
    dmx_ts_feed_t ts;
    dmx_section_feed_t sec;
    dmx_pes_feed_t pes;
  } feed;
  union
  {
    dmx_ts_cb ts;
    dmx_section_cb sec;
    dmx_pes_cb pes;
  } cb;

  struct gtx_demux_s *demux;
  int type;
  int state;

#define DMX_TYPE_TS  0
#define DMX_TYPE_SEC 1
#define DMX_TYPE_PES 2

#define DMX_STATE_FREE      0
#define DMX_STATE_ALLOCATED 1
#define DMX_STATE_SET       2
#define DMX_STATE_READY     3
#define DMX_STATE_GO        4

  dmx_ts_pes_t pes_type;
  int output;
  int pid;
  gtx_demux_filter_t *filter;
  gtx_demux_secfilter_t *secfilter;

  // for sections
  __u8 *sec_buffer;
  int sec_recv;
  int sec_len;
  int check_crc;
  char sec_ccn;

  int index, int_nr, int_bit;

  int base, end, size, readptr;
} gtx_demux_feed_t;

typedef struct gtx_demux_s
{
  dmx_demux_t dmx;
   // keine ahnung was hier noch kommt :) evtl. die wait_queues oder so.
  int users;
  gtx_demux_feed_t feed[32];
  gtx_demux_filter_t filter[32];
  gtx_demux_secfilter_t secfilter[32];

  struct list_head frontend_list;
} gtx_demux_t;

/* data tables */

typedef struct pid_allocation_entry_s {
	u8	wait_pusi : 1;
	u8	valid 		: 1;
	u8						: 1;
	u8	pid_hi		: 5;
	u8	pid_lo		: 8;
} pid_allocation_entry_t;

typedef struct pid_allocation_s {
	pid_allocation_entry_t	pid_entry[32];
} pid_allocation_t;

typedef struct pid_parsing_control_entry_s {
	u8	type					: 3;
	u8	qid						: 5;
	u8	fork					: 1;
	u8	cw_offset			:	3;
	u8	cc						:	4;
	u8								: 1;
	u8	start_up			:	1;
	u8	pec						:	1;
	u8	filt_tab_idx	: 5;
	u8	state					: 3;
	u8								: 1;
	u8	no_of_filt		: 4;
} pid_parsing_control_entry_t;

typedef struct pid_parsing_control_s {
	pid_parsing_control_entry_t pid_entry[32];
} pid_parsing_control_t;

typedef struct filter_definition_entry_s {
	u8	and_or_flag			: 1;
	u8	filter_param_id	: 5;
	u8									: 2;
} filter_definition_entry_t;

typedef struct filter_definition_s {
	filter_definition_entry_t	filter_entry[32];
} filter_definition_t;

typedef struct filter_param_entry_1_s {
	u8	mask_0					: 8;
	u8	filter_0				: 8;
	u8	mask_1					: 8;
	u8	filter_1				: 8;
	u8	mask_2					: 8;
	u8	filter_2				: 8;
} filter_param_entry_1_t;

typedef struct filter_param_table_1_s {
	filter_param_entry_1_t filter_param_entry[32];
} filter_param_table_1_t;

typedef struct filter_param_entry_2_s {
	u8	mask_3					: 8;
	u8	filter_3				: 8;
	u8	mask_4					: 8;
	u8	filter_4				: 8;
	u8	mask_5					: 8;
	u8	filter_5				: 8;
} filter_param_entry_2_t;

typedef struct filter_param_table_2_s {
	filter_param_entry_2_t filter_param_entry[32];
} filter_param_table_2_t;

typedef struct filter_param_entry_3_s {
	u8	mask_6					: 8;
	u8	filter_6				: 8;
	u8	mask_7					: 8;
	u8	filter_7				: 8;
	u8									: 3;
	u8	not_flag				: 1;
	u8									: 2;
	u8	not_flag_ver		: 1;
	u8									: 1;
	u8									: 8;
} filter_param_entry_3_t;

typedef struct filter_param_table_3_s {
	filter_param_entry_3_t filter_param_entry[32];
} filter_param_table_3_t;

#endif
