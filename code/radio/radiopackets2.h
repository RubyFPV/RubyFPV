/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../public/telemetry_info.h"
#include "local_packets.h"
#include "radiopackets_short.h"

#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
#define MAX_RXTX_BLOCKS_BUFFER 100
#define MAX_TOTAL_PACKETS_IN_BLOCK 32
#define MAX_DATA_PACKETS_IN_BLOCK 16
#define MAX_FECS_PACKETS_IN_BLOCK 16
#else
#define MAX_RXTX_BLOCKS_BUFFER 70
#define MAX_TOTAL_PACKETS_IN_BLOCK 32
#define MAX_DATA_PACKETS_IN_BLOCK 16
#define MAX_FECS_PACKETS_IN_BLOCK 16
#endif

#define MAX_PACKET_RADIO_HEADERS 100
#define MAX_PACKET_PAYLOAD 1250
#define MAX_PACKET_TOTAL_SIZE 1500

#define RADIO_PORT_ROUTER_UPLINK 0x0E     // from controller to vehicle
#define RADIO_PORT_ROUTER_DOWNLINK 0x0F   // from vehicle to controller

#define BROADCAST_VEHICLE_ID 1111111111

#define MAX_RADIO_STREAMS 8
#define MAX_VIDEO_STREAMS 4
// (must be less than max radio streams - id of first video stream value below)

#define STREAM_ID_DATA    ((u32)0)
#define STREAM_ID_TELEMETRY ((u32)1)
#define STREAM_ID_AUDIO   ((u32)2)
#define STREAM_ID_DATA2    ((u32)3)
#define STREAM_ID_VIDEO_1 ((u32)4)

// Packet flags is a byte and contains flags:
// bit 0..2 - target module type (video/telem/rc/comm) (3 bits)
// bit 3 - header only CRC
// bit 4 - chained flag: has more radio packets in the same buffer, just after this packet
// bit 5 - has extra data after the regular packet data
// bit 6 - packet is encrypted (everything after packet_flags field is encrypted)
// bit 7 - is a high priority packet

#define PACKET_FLAGS_MASK_MODULE 0b0111
#define PACKET_FLAGS_MASK_STREAM_PACKET_IDX 0x0FFFFFFF
#define PACKET_FLAGS_MASK_STREAM_INDEX (((u32)0x0F)<<28)
#define PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX (28)
#define PACKET_FLAGS_BIT_HEADERS_ONLY_CRC ((u8)(1<<3))
#define PACKET_FLAGS_BIT_RETRANSMITED     ((u8)(1<<4))
// Deprecated in 9.7 (inclusive)
// #define _FLAGS_BIT_EXTRA_DATA       ((u8)(1<<5))
#define PACKET_FLAGS_BIT_HAS_ENCRYPTION   ((u8)(1<<6))
#define PACKET_FLAGS_BIT_HIGH_PRIORITY    ((u8)(1<<7))

#define PACKET_FLAGS_EXTENDED_BIT_SEND_ON_HIGH_CAPACITY_LINK_ONLY  (((u16)1)<<8)
#define PACKET_FLAGS_EXTENDED_BIT_SEND_ON_LOW_CAPACITY_LINK_ONLY  (((u16)1)<<9)
#define PACKET_FLAGS_EXTENDED_BIT_REQUIRE_ACK  (((u16)1)<<10)

// Max 8 components, for the first 3 bits of packet_flags field
#define PACKET_COMPONENT_LOCAL_CONTROL 0 // Used only internally, to exchange data between processes
#define PACKET_COMPONENT_VIDEO 1
#define PACKET_COMPONENT_TELEMETRY 2
#define PACKET_COMPONENT_COMMANDS 3
#define PACKET_COMPONENT_RC 4
#define PACKET_COMPONENT_RUBY 5  // Used for control messages between vehicle and controller
#define PACKET_COMPONENT_AUDIO 6


#define PACKET_TYPE_EMBEDED_FULL_PACKET 0xFF
#define PACKET_TYPE_EMBEDED_SHORT_PACKET 0x00

//----------------------------------------
// All packets start with this basic header and then a packet specific header
// Different packets have different headers, but all headers are preceided by this basic header
// Packet: [header] + [specific header] + [payload_data];
//         | <-total headers length-> |                    // less the start CRC
//         |              <- total length ->           |   // less the start CRC
//
// radio_link_packet_index is used only to detect missing rx packets on a radio link
// vehicle_id_src and stream_packet_idx are used to detect rx packets duplication (same packet received on multiple radio links)

typedef struct
{
   u32 uCRC; // computed for the entire packet or header only. start point is after this crc.
             // Highest byte is set to 0x00 to be able to detect and distinguish radio packets from other systems. (starting in version 8.0)
             // After the packet it received and checked for CRC, this field will be updated to contain the radio datarate it was received at (as int)
   u8 packet_flags;
   u8 packet_type; // 1...150: components packets types, 150-200: local control controller packets, 200-250: local control vehicle packets
   u32 stream_packet_idx; // high 4 bits: stream id (0..15), lower 28 bits: stream packet index
                          // monotonically increassing, to detect missing/lost packets on each stream

   u16 packet_flags_extended;  // Added in 7.4: it replaced (length of all headers)
             // byte 0:
             // not used
             // byte 1:
             //    bit 0: 1: send on high capacity links only;
             //    bit 1: 1: sent on low capacity links only;
             //    bit 2: 1: requires ACK for this packet
   u16 total_length; // Total length, including all the header data, including CRC
   u16 radio_link_packet_index; // Introduced in 7.7: monotonically increasing for each radio packet sent on a radio link
                                // used to detect missing packets on receive side on a radio link, not for duplicate detection
   u32 vehicle_id_src;
   u32 vehicle_id_dest;
} __attribute__((packed)) t_packet_header;



// PING packets do not increase stream packet index as they are sent on each radio link separatelly
#define PACKET_TYPE_RUBY_PING_CLOCK 3
// params after header:
//   u8 ping id
//   u8 controller local radio link id
//   u8 dummy; relay flags for the receiving side (for who receives this ping) // deprecated in 11.5
//   u8 dummy; relay mode for the receiving side (for who receives this ping) // deprecated in 11.5
//   u8 ping flags
//       bit 0: OSD plugins require full telemetry


#define PACKET_TYPE_RUBY_PING_CLOCK_REPLY 4
// has:
//  u8 original ping id after header
//  u32 local time in miliseconds
//  u8 sender original local radio link id
//  u8 reply local radio link id (vehicle local radio link)

#define PACKET_TYPE_RUBY_RADIO_REINITIALIZED 5 // Sent by vehicle when a radio interface crashed on the vehicle.

#define PACKET_TYPE_RUBY_MODEL_SETTINGS 6 // Sent by vehicle after boot or when requested all model settings as zip from controller. Sends all model settings (compressed) to controller;
// Updated in v7.7
// Params:
// u32 startflags 0xFFFFFFFF
//                0xFFFFFFF0 added in 8.3 for tar+gzip format file
// u32 uUniqueSendCounter;
// u8 uSendAsSmallSegments: 0 - big segments / 1 - small segments

// Data itself, after params, can have one of two type of responses:
//   * the zip model settings, if single packet (more than 150 bytes)
//   * segments of the zip model settings, if multiple small segments response is received (smaller than 150 bytes, for slow links)
//     each segment has:
//        1 byte - segment index
//        1 byte - total segments
//        1 byte - segment size
//        N bytes - segment data


#define PACKET_TYPE_RUBY_PAIRING_REQUEST 7
// Sent by controller when it has link with vehicle for first time. So that vehicle has controller id.
// For 11.1 or older:
// Has an optional u32 param after header: count of retires;
// Has an optional u32 param after header: (bool) bDeveloperMode
// Has an optional u32 param: controller board type
//
// For 11.2 or newer:
//
// Has an optional u32 param after header: count of retires;
// Has an optional u32 param after header: uDeveloperFlags
// Has an optional u32 param: controller board type

#define PACKET_TYPE_RUBY_PAIRING_CONFIRMATION 8
// Sent by vehicle to controller.
// Has an optional u32 param after header: count of received pairing requests;
// Has an optional software verion u16 byte-maj.byte-minor

#define PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED 9 // Sent by vehicle to controller to let it know about the current radio config.
                                           // Contains a type_relay_parameters, type_radio_interfaces_parameters and a type_radio_links_parameters


//---------------------------------------
// COMPONENT COMMANDS PACKETS

#define PACKET_TYPE_COMMAND 11
#define PACKET_TYPE_COMMAND_RESPONSE 12


#define COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED ( (u16) (((u16)0x01)<<15) )
#define COMMAND_TYPE_MASK ( (u16) (0x07FF) )

//----------------------------------------------
// packet_header_command
//
typedef struct
{
   u16 command_type;
   u32 command_counter; // command index;
   u8  command_resend_counter; // how many times we tried to send the command; or 255 if not using fast retry logic
   u32 command_param;
   u32 command_param2;
} __attribute__((packed)) t_packet_header_command;


//----------------------------------------------
// packet_header_command_response
//
typedef struct
{
   u16 origin_command_type;
   u32 origin_command_counter;
   u8  origin_command_resend_counter;
   u32 response_counter;
   u8  command_response_flags;
   u32 command_response_param; // to be used if needed to send back a param in the response
} __attribute__((packed)) t_packet_header_command_response;


#define PACKET_TYPE_RUBY_LOG_FILE_SEGMENT 13 // from vehicle to controller, contains a file segment header too and then file data segment

#define PACKET_TYPE_RUBY_MESSAGE 14
// u16 message id, monotonically increasing
// u8 type: 0 debug, 1 info, 2 warning, 3 error
// 0...n: string, null terminated


#define PACKET_TYPE_RUBY_ALARM 15
// Contains 4 u32: alarm index (u32), alarm id(s) (u32), flags1 (u32) and flags2 (u32)

typedef struct
{
   u32 file_id; // what file is this (live log, configuration files, etc)
   u32 segment_index;
   u32 segment_size; // size in bytes of the segment
   u32 total_segments; // total segments in file
} __attribute__((packed)) t_packet_header_file_segment;

#define PACKET_TYPE_FIRST_PAIRING_DONE 16


#define PACKET_TYPE_AUDIO_SEGMENT 18
// Format updated in 10.6, incompatible with older version
// 4 + N bytes
// bytes 0..3 BBBP  (BBB block, higher 3 bytes; P packet index, lower byte)
// byte 4-N - (CRC32) + audio data

#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
// params after header:
//   u32: retransmission request id
//   u8: video stream index
//   u8: flags:
//         bit 0: contains re-requested packets
//         bit 1: contains request for start of video frame packets at the end
//         bit 2: contains request for end of video frame packets at the end
//   u8: number of individual video packets requested
//   (u32+u8)*n = each (video block index + video packet index) requested
//   (u16+u8) frame id and frame packets from start to get
//   (u16+u8) frame id and frame packets to EOF to get

#define PACKET_TYPE_VIDEO_DATA 22

#define VIDEO_STREAM_INFO_FLAG_NONE 0
#define VIDEO_STREAM_INFO_FLAG_SIZE 1
#define VIDEO_STREAM_INFO_FLAG_FPS 2
#define VIDEO_STREAM_INFO_FLAG_EC_TX_TIME 3
#define VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS 4
#define VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID 5
#define VIDEO_STREAM_INFO_FLAG_SET_KF_MS 6
#define VIDEO_STREAM_INFO_FLAG_LAST_FRAME_TIMERS 7
#define VIDEO_STREAM_INFO_FLAG_VIDEO_BYTES_PER_SEC 8 // low 2 bytes: video only, kbytes/sec, high 2 bytes: video+ec, kbytes/sec
#define VIDEO_STREAM_INFO_FLAG_LAST 9

//  [packet header][video segment header][video seg header important][video data][0000  ][dbg]
//  | pPH          | pPHVS               | pPHVSImp                  |pActualVideoData  |
//                                       [     <- error corrected data ->               ]
//                                       [     <- video block packet size   ->          ]
//                                                                   [-vid size-]
typedef struct
{
   u8 uVideoStreamIndexAndType;
      // bits 0...3: video stream index
      // bits 4...7: video stream type: H264, H265, IP, etc
   u32 uVideoStatusFlags2; // see flags_video.h VIDEO_STATUS_FLAGS2_*
      // byte 0:
      //    packets count till the end of (transmission) current frame (current uH264FrameIndex, not current NAL or slice)
      // byte 1:
      //    bit 0  - 0/1: has (t_packet_header_video_segment_debug_info) after video packet;
      //    bit 1  - deprecated in 10.2
      //    bit 2  - 0/1: is on lower video bitrate
      //    bit 3  - 0/1: is start of a NAL
      //    bit 4  - 0/1: is end of a NAL
      //    bit 5  - 0/1: contains I-NAL unit
      //    bit 6  - 0/1: contains P-NAL unit
      //    bit 7  - 0/1: contains other NAL unit
      // byte 2: data/other packets count after video packets
      // byte 3:
      //    bit 0  - 1 if it was reconstructed on Rx side (valid only on rx side)

   u8 uStreamInfoFlags;
   // See enum above
   //  0: none;
   //  1: video width (low 16 bits) and video height (high 16 bits)
   //  2: video fps
   //  3: fec time: how long FEC took, in microseconds/second
   //  4: contains uProfileEncodingFlags; same as video link profile's uProfileEncodingFlags;
   //  5: retransmission id
   //  6: set video bitrate
   //  7: set keyframe ms
   //  8: last frame timers
   
   u32 uStreamInfo; // value dependent on uStreamInfoFlags;

   u8  uCurrentVideoLinkProfile;
   u16 uCurrentVideoKeyframeIntervalMs;
   u32 uCurrentVideoBitrateBPS;

   u32 uCurrentBlockIndex;
   u8  uCurrentBlockPacketIndex;
   u16 uCurrentBlockPacketSize;
   u8  uCurrentBlockDataPackets;
   u8  uCurrentBlockECPackets;

   u16 uH264FrameIndex; // H264/H265 frame index (monotonically increasing)
   u16 uRuntimeMetrics; // byte 0: distance between last frame, in milisec

   // Future
   u16 uFramePacketsInfo; // low byte: current packet in this frame, high byte: total number of packets in this frame.
   u32 uDummy2;
   // After video header comes the importad video header, part of error reconstruction as video data
} __attribute__((packed)) t_packet_header_video_segment;

typedef struct
{
   u16 uVideoDataLength;
   u8  uVideoImportantFlags; // See VIDEO_IMPORTANT_FLAG_x defines
     // bit 0: EndOfFrame
     // bit 1: Has data after EOF
} __attribute__((packed)) t_packet_header_video_segment_important;

typedef struct
{
   u16 uFrameIndex;
   u32 uTime1; // local timestamp camera frame available;
   u32 uTime2; // local timestamp sent to radio tx;
   u32 uTime3; // local timestamp received;
   u32 uTime4; // local timestamp sent to video streamer;
      
} __attribute__((packed)) t_packet_header_video_segment_debug_info;


//---------------------------------------
// COMPONENT RC PACKETS

#define PACKET_TYPE_RC_FULL_FRAME     25   // RC Info sent from ground to vehicle
#define PACKET_TYPE_RC_DOWNLOAD_INFO  26   // RC Info sent back from vehicle to ground station

//---------------------------------------
// Ruby misc packets

#define PACKET_TYPE_DEBUG_INFO 28
// has:
// * type_u32_couters structure for vehicle router main loop info

//---------------------------------------
// COMPONENT TELEMETRY PACKETS

#define PACKET_TYPE_RUBY_TELEMETRY_SHORT 29   // Ruby telemetry, small version, contains t_packet_header and t_packet_ruby_telemetry_short

#define PACKET_TYPE_RUBY_TELEMETRY_EXTENDED 30   // Ruby telemetry, extended version
// Contains:
// t_packet_header
// t_packet_header_ruby_telemetry_extended_v5
// t_packet_header_ruby_telemetry_extended_extra_info
// t_packet_header_ruby_telemetry_extended_extra_info_retransmissions
// extraData (0 or more, as part of the packet, after headers)

#define PACKET_TYPE_FC_TELEMETRY 31
#define PACKET_TYPE_FC_TELEMETRY_EXTENDED 32 // FC telemetry + FC message
#define PACKET_TYPE_FC_RC_CHANNELS 34
#define PACKET_TYPE_RC_TELEMETRY 33

// Deprecated in 11.1 #define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS 35 // has a shared_mem_video_link_stats_and_overwrites structure as data
// Deprecated in 11.1 #define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS 36 // has a shared_mem_video_link_graphs structure as data

#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY 37 // has a t_packet_header_vehicle_tx_gap_history structure
#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS 38
// 1 byte: 0xFF if sending all stats at once, 0xF0 if single radio stats, 0x0F if sending single compact stats
// 1 byte: card index if it has a single card
//         card count if full stats
// then (count cards * shared_mem_radio_stats_radio_interface) or single stats compact after the header
#define PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY 39 // has a shared_mem_dev_video_bitrate_history structure


#define PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD 41 // download telemetry data packet from vehicle to controller
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data
#define PACKET_TYPE_TELEMETRY_RAW_UPLOAD 42  // upload telemetry data packet from controller to vehicle
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data


//----------------------------------------------
// packet_header_ruby_telemetry
//
#define FLAG_RUBY_TELEMETRY_RC_ALIVE    ((u32)1)
#define FLAG_RUBY_TELEMETRY_RC_FAILSAFE ((u32)(((u32)0x01)<<1))
// Unused anymore #define FLAG_RUBY_TELEMETRY_HAS_RC_INFO ((u32)(((u32)0x01)<<2))
#define FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY ((u32)(((u32)0x01)<<3))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA ((u32)(((u32)0x01)<<4))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO ((u32)(((u32)0x01)<<5))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_HAS_KEY ((u32)(((u32)0x01)<<6))
#define FLAG_RUBY_TELEMETRY_HAS_RC_RSSI ((u32)(((u32)0x01)<<7))
#define FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI ((u32)(((u32)0x01)<<7))
#define FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RX_RSSI ((u32)(((u32)0x01)<<8))
#define FLAG_RUBY_TELEMETRY_HAS_RELAY_LINK ((u32)(((u32)0x01)<<9))  // true if the link to the relayed vehicle is present and live
#define FLAG_RUBY_TELEMETRY_IS_RELAYING ((u32)(((u32)0x01)<<10))  // true if the vehicle is currently relaying another vehicle
#define FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO ((u32)(((u32)0x01)<<11)) // if true, has the extended telemetry info after this telemetry header
#define FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA ((u32)(((u32)0x01)<<12)) // if true, vehicle has at least one camera
#define FLAG_RUBY_TELEMETRY_HAS_VEHICLE_TELEMETRY_DATA ((u32)(((u32)0x01)<<13)) // if the FC sends any valid data to Ruby serial port
#define FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER ((u32)(((u32)0x01)<<14)) // false if the fast uplink from controller is lost for more than TIMEOUT_LINK_TO_CONTROLLER_LOST
#define FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER ((u32)(((u32)0x01)<<15)) // false if the slow uplink from controller is lost for more than TIMEOUT_LINK_TO_CONTROLLER_LOST

#define FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_VERSION ((u32)(0x07))
#define FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE ((u32)(((u32)0x01)<<3))
#define FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE_HOT ((u32)(((u32)0x01)<<4))

typedef struct
{
   u16 uRubyFlags;    // see above
   u8  uFCFlags;
   u8  rubyVersion;  // version x.y 4bits each        
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[3]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   
   u8 flight_mode;
   u8 throttle;
   u16 voltage; // 1/1000 volts
   u16 current; // 1/1000 amps
   u32 altitude; // 1/100 meters  -1000 m
   u32 altitude_abs; // 1/100 meters -1000 m
   u32 distance; // 1/100 meters
   u16 heading;
   
   u32 vspeed; // 1/100 meters -1000 m
   u32 aspeed; // airspeed (1/100 meters - 1000 m)
   u32 hspeed; // 1/100 meters -1000 m
} __attribute__((packed)) t_packet_header_ruby_telemetry_short;


typedef struct // introduced in version 7.4
{
   u16 uRubyFlags;    // see above
   u8  rubyVersion;  // version x.y 4bits each
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[6]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate_bps; // The transmitted video bitrate by vehicle
   u32 downlink_tx_video_all_bitrate_bps; // Total transmitted video bitrate (+EC + headers) by vehicle
   u32 downlink_tx_data_bitrate_bps;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperatureC;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   int last_sent_datarate_bps[6][2]; // in bps, positive, negative: mcs rates; 0: never, index 0 - video, index 1 - data
   int last_recv_datarate_bps[6]; // in bps, positive, negative: mcs rates, 0: never
   u8  uplink_rssi_dbm[6]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_link_quality[6]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available

   u16 txTimePerSec; // miliseconds
   u16 extraFlags; // bits 0..3 : structure version (0 for now, first one, starting at v3)
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v3;


typedef struct // introduced in version 10.4
{
   u16 uRubyFlags;    // see above
   u8  rubyVersion;  // version x.y 4bits each        
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[6]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate_bps; // The transmitted video bitrate by vehicle
   u32 downlink_tx_video_all_bitrate_bps; // Total transmitted video bitrate (+EC + headers) by vehicle
   u32 downlink_tx_data_bitrate_bps;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperatureC;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   int last_sent_datarate_bps[6][2]; // in bps, positive, negative: mcs rates; 0: never, index 0 - video, index 1 - data
   int last_recv_datarate_bps[6]; // in bps, positive, negative: mcs rates, 0: never
   u8  uplink_rssi_dbm[6]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_rssi_snr[6]; // 0 for negative, 0xFF for missing
   u8  uplink_link_quality[6]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available

   u16 txTimePerSec; // miliseconds
   u16 uExtraRubyFlags; // see above
      // bits 0..3 : structure version (0 for now, first one, starting at v3)
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v4;


typedef struct // introduced in version 11.2
{
   u16 uRubyFlags;    // see above
   u8  rubyVersion;  // version x.y 4bits each (high bits: major, low bits: minor)
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[6]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate_bps; // The transmitted video bitrate by vehicle
   u32 downlink_tx_video_all_bitrate_bps; // Total transmitted video bitrate (+EC + headers) by vehicle
   u32 downlink_tx_data_bitrate_bps;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperatureC;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   int last_sent_datarate_bps[6][2]; // in bps, positive, negative: mcs rates; 0: never, index 0 - video, index 1 - data
   int last_recv_datarate_bps[6]; // in bps, positive, negative: mcs rates, 0: never
   u8  uplink_rssi_dbm[6]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_rssi_snr[6]; // 0 for negative, 0xFF for missing
   u8  uplink_link_quality[6]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available
   
   int iTxPowers[6]; // current Tx powers, per radio link, in mW. positive: as set, negative: adjusted down
   u16 txTimePerSec; // miliseconds
   u16 uExtraRubyFlags; // see above
      // bits 0..3 : structure version (0 for now, first one, starting at v3)
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v5;


typedef struct // introduced in version 11.5
{
   u16 uRubyFlags;    // see above
   u8  rubyVersion;  // version x.y 4bits each (high bits: major, low bits: minor)
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[MAX_RADIO_INTERFACES]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate_bps; // The transmitted video bitrate by vehicle
   u32 downlink_tx_video_all_bitrate_bps; // Total transmitted video bitrate (+EC + headers) by vehicle
   u32 downlink_tx_data_bitrate_bps;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperatureC;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   int last_sent_datarate_bps[MAX_RADIO_INTERFACES][2]; // in bps, positive, negative: mcs rates; 0: never, index 0 - video, index 1 - data
   int last_recv_datarate_bps[MAX_RADIO_INTERFACES]; // in bps, positive, negative: mcs rates, 0: never
   u8  uplink_rssi_dbm[MAX_RADIO_INTERFACES]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_rssi_snr[MAX_RADIO_INTERFACES]; // 0 for negative, 0xFF for missing
   u8  uplink_link_quality[MAX_RADIO_INTERFACES]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available
   
   int iTxPowers[MAX_RADIO_INTERFACES]; // current Tx powers, per radio link, in mW. positive: as set, negative: adjusted down
   u16 uDummyT1; // deprecated in 11.5
   u16 uExtraRubyFlags; // see above
      // bits 0..3 : structure version (0 for now, first one, starting at v3)
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v6;

// Flags for structure t_packet_header_ruby_telemetry_extended_extra_info 
#define FLAG_RUBY_TELEMETRY_EXTRA_INFO_IS_VALID ((u32)(((u32)0x01)<<1))

typedef struct
{
   u32 uExtraFlags;
   u32 uTimeNow; // Vehicle time
   u32 uRelayedVehicleId;
   u16 uThrottleInput; // usually 1000...2000
   u16 uThrottleOutput; // usually 0...100

   u32 uDummy[10];
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_extra_info;

typedef struct
{
   u16 totalReceivedRetransmissionsRequestsUnique;
   u16 totalReceivedRetransmissionsRequestsDuplicate;

   u16 totalReceivedRetransmissionsRequestsSegmentsUnique;
   u16 totalReceivedRetransmissionsRequestsSegmentsDuplicate;
   u16 totalReceivedRetransmissionsRequestsSegmentsRetried;
      
   u16 totalReceivedRetransmissionsRequestsUniqueLast5Sec;
   u16 totalReceivedRetransmissionsRequestsDuplicateLast5Sec;

   u16 totalReceivedRetransmissionsRequestsSegmentsUniqueLast5Sec;
   u16 totalReceivedRetransmissionsRequestsSegmentsDuplicateLast5Sec;
   u16 totalReceivedRetransmissionsRequestsSegmentsRetriedLast5Sec;
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_extra_info_retransmissions;

//----------------------------------------------
// packet_header_fc_telemetry
//
typedef struct
{
   u8 uFCFlags;
   u8 fc_telemetry_type; // type of telemetry sent by the FC. The ones defined in models.h
   u8 flight_mode;
   u32 arm_time;
   u8 throttle;
   u16 voltage; // 1/1000 volts
   u16 current; // 1/1000 amps
   u16 voltage2; // 1/1000 volts
   u16 current2; // 1/1000 amps
   u16 mah;
   u32 altitude; // 1/100 meters  -1000 m
   u32 altitude_abs; // 1/100 meters -1000 m
   u32 distance; // 1/100 meters
   u32 total_distance; // 1/100 meters

   u32 vspeed; // 1/100 meters -1000 m
   u32 aspeed; // airspeed (1/100 meters - 1000 m)
   u32 hspeed; // 1/100 meters -1000 m
   u32 roll; // 1/100 degree + 180 degree
   u32 pitch; // 1/100 degree + 180 degree

   u8 satelites;
   u8 gps_fix_type;
   u16 hdop; // 1/100 th
   u16 heading;
   int32_t latitude; // 1/10000000;
   int32_t longitude; // 1/10000000;

   u8 temperatureC; // temperature - 100 degree celsius (value 100 = 0 celsius)
   // Changed in v7.5 to 2 8 bit values
   //u16 fc_kbps; // kbits/s serial link volume from FC to Pi
   u8 fc_hudmsgpersec; // low 4 bits: heartbeat messages/sec; high 4 bits: system messages/sec; 
   u8 fc_kbps;
   u8 rc_rssi;
   u8 extra_info[12];
               // byte 0: not used
               // byte 1: satelites2; // 0xFF if no second GPS
               // byte 2: gps_fix_type2; // 0xFF if no second GPS
               // byte 3,4: hdop2; // 0xFF if no second GPS
               // byte 5: FC telemetry packet index (ascending)
               // byte 6: MAVLink messages per second
               // byte 7-8: wind heading (+1, 0 for N/A)
               // byte 9-10: wind speed (1/100 m) (+1, 0 for N/A)
} __attribute__((packed)) t_packet_header_fc_telemetry;


typedef struct
{
   u16 channels[MAX_MAVLINK_RC_CHANNELS];
} __attribute__((packed)) t_packet_header_fc_rc_channels;


//----------------------------------------------
// packet_header_fc_extra, present in PACKET_TYPE_TELEMETRY_ALL2
//
typedef struct
{
   u8 chunk_index;  // 0 for a complete single message; 1 based index if it's a part of a longer message
   u8 text[FC_MESSAGE_MAX_LENGTH];     // text is zero based terminated, even if part of a longer message.
} __attribute__((packed)) t_packet_header_fc_extra;


//----------------------------------------------
// packet_header_telemetry_raw, present in PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD and PACKET_TYPE_TELEMETRY_RAW_UPLOAD
//
typedef struct
{
   u32 telem_segment_index; // monotonically increasing
   u32 telem_total_data; // total bytes sent to the other radio end (controller or vehicle)
   u32 telem_total_serial; // total bytes read from FC serial port or controller serial port
} __attribute__((packed)) t_packet_header_telemetry_raw;


#define PACKET_TYPE_TELEMETRY_MSP 43
// Has a t_packet_header_telemetry_msp header and then data
#define MSP_FLAGS_FC_TYPE_MASK ((u32)0x07)
#define MSP_FLAGS_FC_TYPE_BETAFLIGHT 1
#define MSP_FLAGS_FC_TYPE_INAV 2
#define MSP_FLAGS_FC_TYPE_ARDUPILOT 3
#define MSP_FLAGS_FC_TYPE_PITLAB 4

#define MSP_FLAG_GOT_FC_TYPE ((u32)(((u32)0x01)<<5))
#define MSP_FLAG_GOT_FC_DISPLAY_OPTIONS ((u32)(((u32)0x01)<<6))
#define MSP_FLAG_FC_DID_ADJUSTED_OSD_SIZE ((u32)(((u32)0x01)<<7))
#define MSP_FLAG_AUTO_ADJUSTED_OSD_SIZE ((u32)(((u32)0x01)<<8))

typedef struct
{
   u32 uMSPFlags; // See flags above
   // bit 0..2: FC type (see above): 1 BF, 2 INAV, 3 Ardupilot
   u8 uMSPOSDRows;
   u8 uMSPOSDCols;
   u32 uSegmentIdAndExtraInfo; // byte 0..1: segment id (monotonically increasing), byte 2: checksum
} __attribute__((packed)) t_packet_header_telemetry_msp;


#define PACKET_TYPE_AUX_DATA_LINK_UPLOAD 45  // upload data link packet from controller to vehicle
// payload is a data link segment index and data
#define PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD 46  // upload data link packet from controller to vehicle
// payload is a data link segment index and data

#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS 47 // has a shared_mem_video_frames_stats structure

#define MAX_HISTORY_VEHICLE_TX_STATS_SLICES 40
typedef struct
{
   int iSliceInterval; // in miliseconds
   u8 uCountValues;
   u8 historyTxGapMaxMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxGapMinMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxGapAvgMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxPackets[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyVideoPacketsGapMax[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyVideoPacketsGapAvg[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u32 tmp_uAverageTxSum;
   u32 tmp_uAverageTxCount;
   u32 tmp_uVideoIntervalsSum;
   u32 tmp_uVideoIntervalsCount;
} __attribute__((packed)) t_packet_header_vehicle_tx_history;


#define PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY 48
// has:
// u32 - interface index;
// shared_mem_radio_stats_interface_rx_hist structure


#define PACKET_TYPE_RUBY_RELAY_RADIO_INFO 49
// has a t_packet_header_relay_radio_info structure
typedef struct
{
   int last_rx_datarates_bps[MAX_RADIO_INTERFACES][2]; // in bps, positive, negative: mcs rates; 0: never;
                                                       // index 0 - video, index 1 - data
   u8  rssi_dbm[MAX_RADIO_INTERFACES]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  rssi_snr[MAX_RADIO_INTERFACES]; // 0 for negative, 0xFF for missing
   u8  link_quality[MAX_RADIO_INTERFACES]; // 0...100
} __attribute__((packed)) t_packet_header_relay_radio_info;



#define PACKET_TYPE_VEHICLE_RECORDING 50
// Has extra info 8 bytes
/*
byte 0: command type:
         1 - start audio recording
         2 - stop audio recording
         3 - start video recording
         4 - stop video recording
         5 - start AV recording
         6 - stop AV recording
         10 - notif: video recording started
         11 - notif: video recording stopped
         7 - notif: video recording restarted due to resolution change
*/

#define PACKET_TYPE_TEST_RADIO_LINK 51
/*
byte 0: protocol version (1)
byte 1: header size (5 bytes)
byte 2: vehicle (model) radio link id to test
byte 3: test number; // added in 8.3
byte 4: command type:
   0 - status message;
         byte 5: length of string including end byte;
         byte 6+: has zero terminated string after that;
   1 - start test link: (from controller to vehicle and vicevera for confirmation)
         byte 5+: radio_link_params structure;
   2 - ping uplink(from controller to vehicle and viceversa)
         byte 5..8:  u32: pings count sent by sender of the message;
         byte 9..12: u32: pings count received so far by the sender of the message from the other side;
   3 - ping downlink(from controller to vehicle and viceversa)
         byte 5..8:  u32: pings count sent by sender of the message;
         byte 9..12: u32: pings count received so far by the sender of the message from the other side;
   4 - end test link (from controller to vehicle and viceversa)
         byte 5: success (0/1)
   5 - test ended (from controller to central)
         byte 5: success (0/1)
*/
#define PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE 5
#define PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION 1
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS 0
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START  1
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK   2
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK 3
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END    4
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_ENDED  5


#define FLAG_ADAPTIVE_VIDEO_BITRATE 1
#define FLAG_ADAPTIVE_VIDEO_EC 2
#define FLAG_ADAPTIVE_VIDEO_DATARATE 4
#define FLAG_ADAPTIVE_VIDEO_KEYFRAME 8
#define FLAG_ADAPTIVE_VIDEO_DR_BOOST 16
#define FLAG_ADAPTIVE_IN_TEST_MODE 128

#define PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS 60
// From controller to vehicle. Contains:
// u32 - request id, monotonically increasing
// u8  - flags, see above
// u8  - video stream index
// u32 - video bitrate, 0 for default
// u16 - ec (low byte: ec, high byte: data), 0 or 0xFFFF for default
// int - radio datarate, 0 for default
// int - keyframe ms (0 or negative: auto)
// u8  - datarate boost, 0xFF for none

#define PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK 61
// From vehicle to controller. Contains:
// u32 - request id ack


#define PACKET_TYPE_SIK_CONFIG 70
//
// Can be send locally or to vehicle
// u8: vehicle radio link id
// u8: command id:
//        0 - get SiK config
//        1 - pause SiK interface
//        2 - resume SiK interface
// u8+: data response

#define PACKET_TYPE_NEGOCIATE_RADIO_LINKS 72
#define NEGOCIATE_RADIO_TEST_PARAMS 1
#define NEGOCIATE_RADIO_END_TESTS 2
#define NEGOCIATE_RADIO_KEEP_ALIVE 3
#define NEGOCIATE_RADIO_APPLY_PARAMS 255
//
// u8   uTestIndex
// u8:  uCommand
//
// Command 1: test radio params
//    u8:  vehicle radio link to test/apply to
//    int: radio datarate to test/apply
//    u32: radio flags to test/apply
//    int: tx power (mw) to test/apply
//
// Command 2: end tests
//    u8:  1 - canceled; 0 - finished normally

// Command 255: apply radio params
//    u8:  vehicle radio link to test/apply to
//    int: radio datarate to test/apply
//    u32: radio flags to test/apply
//    int: tx power (mw) to test/apply
//    when sending to vehicle, has also a type_radio_runtime_capabilities_parameters structure at the end

#define PACKET_TYPE_OTA_UPDATE_STATUS 75
//
// From vehicle to controller, during OTA update
// u8 status
// u32 counter
#define OTA_UPDATE_STATUS_START_PROCESSING 1
#define OTA_UPDATE_STATUS_UNPACK 2
#define OTA_UPDATE_STATUS_UPDATING 3
#define OTA_UPDATE_STATUS_POST_UPDATING 4
#define OTA_UPDATE_STATUS_COMPLETED 5
#define OTA_UPDATE_STATUS_REBOOT 6
#define OTA_UPDATE_STATUS_FAILED_DISK_SPACE 250
#define OTA_UPDATE_STATUS_FAILED 255


//#define PACKET_TYPE_DEBUG_VEHICLE_RT_INFO 110 // deprecated in 11.7
// contains a vehicle_runtime_info structure

#ifdef __cplusplus
extern "C" {
#endif  

void radio_packet_init(t_packet_header* pPH, u8 component, u8 packet_type, u32 uStreamId);
void radio_packet_compute_crc(u8* pBuffer, int length);
int radio_packet_check_crc(u8* pBuffer, int length);

void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v3(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v3* pV3);
void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v4(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v4* pV4);
void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v5(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v5* pV5);

void radio_packets_log_sizes();
#ifdef __cplusplus
}
#endif

