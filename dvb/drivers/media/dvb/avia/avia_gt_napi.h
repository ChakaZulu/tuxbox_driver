#ifndef AVIA_GT_NAPI_H
#define AVIA_GT_NAPI_H

struct ts_header {
	unsigned sync_byte			: 8;
	unsigned transport_error_indicator	: 1;
	unsigned payload_unit_start_indicator	: 1;
	unsigned transport_priority		: 1;
	unsigned pid				: 13;
	unsigned transport_scrambling_control	: 2;
	unsigned adaptation_field		: 1;
	unsigned payload			: 1;
	unsigned continuity_counter		: 4;
} __attribute__ ((packed));

/*
 * pes header containing pts value (12 byte)
 *
 * check the following conitions:
 * packet_start_code_prefix == 0x000001
 * stream_id != program_stream_map
 * stream_id != padding_stream
 * stream_id != private_stream_2
 * stream_id != ECM
 * stream_id != EMM
 * stream_id != program_stream_directory
 * stream_id != DSMCC_stream
 * stream_id != ITU-T Rec. H.222.1 type E stream
 * reserved_0 == 0x2
 * pts_flag == 0x1
 * reserved_1 == 0x2
 * marker_bit_0 == 0x1
 * marker_bit_1 == 0x1
 * marker_bit_2 == 0x1
 */
struct pes_header {
	unsigned packet_start_code_prefix	: 24;
	unsigned stream_id			: 8;
	unsigned pes_packet_length		: 16;
	unsigned reserved_0			: 2;
	unsigned pes_scrambling_control		: 2;
	unsigned pes_priority			: 1;
	unsigned data_alignment_indicator	: 1;
	unsigned copyright			: 1;
	unsigned original_or_copy		: 1;
	unsigned pts_flag			: 1;
	unsigned dts_flag			: 1;
	unsigned escr_flag			: 1;
	unsigned es_rate_flag			: 1;
	unsigned dsm_trick_mode_flag		: 1;
	unsigned additional_copy_info_flag	: 1;
	unsigned pes_crc_flag			: 1;
	unsigned pes_extension_flag		: 1;
	unsigned pes_header_data_length		: 8;
	unsigned reserved_1			: 4;
	unsigned pts_hi				: 3;
	unsigned marker_bit_0			: 1;
	unsigned pts_mid			: 15;
	unsigned marker_bit_1			: 1;
	unsigned pts_lo				: 15;
	unsigned marker_bit_2			: 1;
} __attribute__ ((packed));

#endif
