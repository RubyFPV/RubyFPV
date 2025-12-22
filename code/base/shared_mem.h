#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "base.h"
#include "config.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_rc.h"

#include "shared_mem_radio.h"

#define SHARED_MEM_RADIO_STATS "/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS"
#define SHARED_MEM_RADIO_STATS_RX_HIST "/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS_RX_HIST"

#define SHARED_MEM_VIDEO_FRAMES_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO"
#define SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_IN "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO_RADIO_IN"
#define SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_OUT "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO_RADIO_OUT"
#define SHARED_MEM_RC_DOWNLOAD_INFO "R_SHARED_MEM_VEHICLE_RC_DOWNLOAD_INFO"
#define SHARED_MEM_RC_UPSTREAM_FRAME "R_SHARED_MEM_RC_UPSTREAM_FRAME"

#define SHARED_MEM_WATCHDOG_CENTRAL "/SYSTEM_SHARED_MEM_WATCHDOG_CENTRAL"
#define SHARED_MEM_WATCHDOG_ROUTER_RX "/SYSTEM_SHARED_MEM_WATCHDOG_ROUTER_RX"
#define SHARED_MEM_WATCHDOG_TELEMETRY_RX "/SYSTEM_SHARED_MEM_WATCHDOG_TELEMETRY_RX"
#define SHARED_MEM_WATCHDOG_RC_TX "/SYSTEM_SHARED_MEM_WATCHDOG_RC_TX"
#define SHARED_MEM_WATCHDOG_MPP_PLAYER "/SYSTEM_SHARED_MEM_MPP_PLAYER"
#define SHARED_MEM_WATCHDOG_ROUTER_TX "/SYSTEM_SHARED_MEM_WATCHDOG_ROUTER_TX"
#define SHARED_MEM_WATCHDOG_TELEMETRY_TX "/SYSTEM_SHARED_MEM_WATCHDOG_TELEMETRY_TX"
#define SHARED_MEM_WATCHDOG_COMMANDS_RX "/SYSTEM_SHARED_MEM_WATCHDOG_COMMANDS_RX"
#define SHARED_MEM_WATCHDOG_RC_RX "/SYSTEM_SHARED_MEM_WATCHDOG_RC_RX"

#define SHARED_MEM_RASPIVIDEO_COMMAND "/SYSTEM_SHARED_MEM_RASPIVID_COMM"
#define SIZE_OF_SHARED_MEM_RASPIVID_COMM 32
// it's a 32 byte shared mem (8 u32) gives the commands to raspivid
// first u32 and last u32: modify counter, for checksum purpose
// u32 1...6 - commands: byte 0: command number, byte 1: command type, byte 2,3: params


#ifdef __cplusplus
extern "C" {
#endif 

#define PROCESS_ALARM_NONE 0
#define PROCESS_ALARM_RADIO_INTERFACE_BEHIND 1
#define PROCESS_ALARM_RADIO_STREAM_RESTARTED 2



typedef struct
{
   u32 lastActiveTime;
   u32 lastRadioRxTime;
   u32 lastRadioTxTime;
   u32 lastIPCIncomingTime;
   u32 lastIPCOutgoingTime;
   u32 timeLastReceivedPacket;
   u32 uInBlockingOperation;
   u32 alarmFlags;
   u32 alarmTime;
   u32 uLoopCounter;
   u32 uLoopCounter1;
   u32 uLoopCounter2;
   u32 uLoopCounter3;
   u32 uLoopCounter4;
   u32 uLoopCounter5;
   u32 uLoopSubStep;
   u32 uLoopTimer1;
   u32 uLoopTimer2;
   u32 uLoopTimer3;
   u32 uLoopTimer4;
   u32 uTotalLoopTime;
   u32 uAverageLoopTimeMs;
   u32 uMaxLoopTimeMs;
} ALIGN_STRUCT_SPEC_INFO shared_mem_process_stats;


typedef struct
{
   u32 lastActiveTime;
   u32 uInBlockingOperation;
} shared_mem_player_process_stats;

#define MAX_INTERVALS_VIDEO_LINK_SWITCHES 50
#define MAX_INTERVALS_VIDEO_LINK_STATS 24
#define VIDEO_LINK_STATS_REFRESH_INTERVAL_MS 80
// one every 80 milisec, for 2 sec total
// !!! Interval should be the same as the one send by controller in link stats: CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL


#define MAX_INTERVALS_VIDEO_BITRATE_HISTORY 70

typedef struct
{
   u8  uMinVideoDataRateMbps; // in Mbps (mcs is converted to Mbps)
   u8  uVideoQuantization; // H264 quantization
   u16 uVideoBitrateCurrentProfileKb; // set video bitrate for current video profile
   u16 uVideoBitrateTargetKb; // target video bitrate for current video profile and overwrites
   u16 uVideoBitrateKb; // only video data
   u16 uVideoBitrateAvgKb; // only video data
   u16 uTotalVideoBitrateAvgKb; // video data + EC + radio headers
   u8  uVideoProfileSwitches; // bit 0..3 - level, bit 4..7 - profile 
} ALIGN_STRUCT_SPEC_INFO shared_mem_dev_video_bitrate_history_datapoint;

typedef struct
{
   u32 uGraphSliceInterval;
   u32 uLastGraphSliceTime;
   u32 uLastTimeSendToController;
   u8  uTotalDataPoints;
   u8  uCurrentDataPoint;
   u8  uQuantizationOverflowValue;
   u32 uCurrentTargetVideoBitrate;

   shared_mem_dev_video_bitrate_history_datapoint  history[MAX_INTERVALS_VIDEO_BITRATE_HISTORY];
} ALIGN_STRUCT_SPEC_INFO shared_mem_dev_video_bitrate_history;


#define MAX_FRAMES_SAMPLES 121
#define VIDEO_FRAME_TYPE_MASK 0xC0
#define VIDEO_FRAME_DURATION_MASK 0x3F
typedef struct
{
   u32 uLastTimeStatsUpdate; // Last time statistics where updated
   u32 uLastFrameIndex; // Increases on each video frame detected
   u32 uLastFrameTime;
   u8 uFramesTypesAndDuration[MAX_FRAMES_SAMPLES];
     // highest 2 bits: 0-pframe, 1-iframe, 2-other frame
     // lower 6 bits: frame duration in ms
   u8 uFramesSizes[MAX_FRAMES_SAMPLES];
     // Frame size in kbytes

   // Computed on H264 parser
   u32 uDetectedFPS;
   u32 uDetectedSlices;
   u32 uDetectedKeyframeIntervalMs;

   // Computed on each frame update
   u32 uAverageIFrameSizeBytes;
   u32 uAveragePFrameSizeBytes;

} ALIGN_STRUCT_SPEC_INFO shared_mem_video_frames_stats;

void* open_shared_mem(const char* name, int size, int readOnly);
void* open_shared_mem_for_write(const char* name, int size);
void* open_shared_mem_for_read(const char* name, int size);

shared_mem_process_stats* shared_mem_process_stats_open_read(const char* szName);
shared_mem_process_stats* shared_mem_process_stats_open_write(const char* szName);
void shared_mem_process_stats_close(const char* szName, shared_mem_process_stats* pAddress);

void process_stats_reset(shared_mem_process_stats* pStats, u32 timeNow);
void process_stats_mark_active(shared_mem_process_stats* pStats, u32 timeNow);

shared_mem_radio_stats* shared_mem_radio_stats_open_for_read();
shared_mem_radio_stats* shared_mem_radio_stats_open_for_write();
void shared_mem_radio_stats_close(shared_mem_radio_stats* pAddress);

shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_read();
shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_write();
void shared_mem_radio_stats_rx_hist_close(shared_mem_radio_stats_rx_hist* pAddress);

shared_mem_video_frames_stats* shared_mem_video_frames_stats_open_for_read();
shared_mem_video_frames_stats* shared_mem_video_frames_stats_open_for_write();
void shared_mem_video_frames_stats_close(shared_mem_video_frames_stats* pAddress);

shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_in_open_for_read();
shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_in_open_for_write();
void shared_mem_video_frames_stats_radio_in_close(shared_mem_video_frames_stats* pAddress);

shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_out_open_for_read();
shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_out_open_for_write();
void shared_mem_video_frames_stats_radio_out_close(shared_mem_video_frames_stats* pAddress);

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_read();
t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_write();
void shared_mem_rc_downstream_info_close(t_packet_header_rc_info_downstream* pRCInfo);

t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_read();
t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_write();
void shared_mem_rc_upstream_frame_close(t_packet_header_rc_full_frame_upstream* pRCFrame);

void update_shared_mem_video_frames_stats(shared_mem_video_frames_stats* pSMVIStats, u32 uTimeNow);
void update_shared_mem_video_frames_stats_on_new_frame(shared_mem_video_frames_stats* pSMVFStats, u32 uLastFrameSizeBytes, int iFrameType, int iDetectedSlices, int iDetectedFPS, u32 uTimeNow);

#ifdef __cplusplus
}  
#endif 