#ifndef AVIA_GT_NAPI_H
#define AVIA_GT_NAPI_H

#pragma pack(1)

struct dvb_ts_header {

	u32 synch_byte: 8;
	u32 transport_error_indicator: 1;
	u32 payload_unit_start_indicator: 1;
	u32 transport_priority: 1;
	u32 PID: 13;
	u32 transport_scrambling_control: 2;
	u32 adaptation_field: 1;
	u32 payload: 1;
	u32 continuity_counter: 4;

};

#pragma pack()

#endif
