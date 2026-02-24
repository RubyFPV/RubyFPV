#pragma once
#include "radiopackets2.h"

// RC Channel is:
// u8 lowBits; // Channels lower part, values from 0 to 255.
// u4 highBits;  // 4 extra most significant bits for each channel;
// Making the channel final range from 0 to 4096; regular range is 1000 to 2000, mid is 1500, 700 is failsafe/missing


//----------------------------------------------
// packet_header_rc_full_frame_upstream
//
#define RC_FULL_FRAME_FLAGS_HAS_INPUT 0x01

typedef struct
{
   u8 rc_frame_index;
   u8 ch_lowBits[MAX_RC_CHANNELS]; // Channels lower part, values from 0 to 255.
   u8 ch_highBits[MAX_RC_CHANNELS/2];  // 4 extra most significant bits for each channel, channel 0 are the lowest bits,  making the channel final range from 0 to 4096, 1000 to 2000 used
   u8 flags; // bit 0 - has input (on the controller side)
   u8 extra_info1; // not used, for future use
   u8 extra_info2; // not used, for future use
   u8 extra_info3; // not used, for future use
} ALIGN_STRUCT_SPEC_INFO t_packet_header_rc_full_frame_upstream;


#define RC_INFO_HISTORY_SIZE 50 // every 50ms

//----------------------------------------------
// packet_header_rc_info_downstream
//
typedef struct
{
   u16 rc_channels[MAX_RC_CHANNELS]; // current value for all channels, 1000-2000 range
   u32 lost_packets;
   u32 recv_packets;
   u8 is_failsafe;
   int failsafe_count;
   u8 history[RC_INFO_HISTORY_SIZE]; // bit 0..3 - count received, bit 4..7 - frames gap
   u8 last_history_slice;
   u8 rc_rssi;
   u32 extra_flags; // not used now. for future use
} ALIGN_STRUCT_SPEC_INFO t_packet_header_rc_info_downstream;



#ifdef __cplusplus
extern "C" {
#endif

void packet_header_rc_full_set_rc_channel_value(t_packet_header_rc_full_frame_upstream* pphrc, u16 ch, u16 val);
u16 packet_header_rc_full_get_rc_channel_value(t_packet_header_rc_full_frame_upstream* pphrc, u16 ch);

#ifdef __cplusplus
}
#endif

