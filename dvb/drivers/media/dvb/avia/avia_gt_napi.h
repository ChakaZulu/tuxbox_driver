#ifndef AVIA_GT_NAPI_H
#define AVIA_GT_NAPI_H

typedef struct gtx_demux_filter_s
{
  int index;
  int state;

  int output;
  int wait_pusi, invalid, pid;
  int queue, fork, cw_offset, cc, start_up, pec;
  // type section
  int no_of_filters;

  struct gtx_demux_feed_s *feed;
} gtx_demux_filter_t;


#pragma pack(1)

typedef struct {

	u32 synch_byte: 8;
	u32 transport_error_indicator: 1;
	u32 payload_unit_start_indicator: 1;
	u32 transport_priority: 1;
	u32 PID: 13;
	u32 transport_scrambling_control: 2;
	u32 adaptation_field: 1;
	u32 payload: 1;
	u32 continuity_counter: 4;

} sDVBTsHeader;

#pragma pack()

extern struct dvb_demux *avia_gt_napi_get_demux(void);

#endif
