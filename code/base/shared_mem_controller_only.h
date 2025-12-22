#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_CONTROLLER_ROUTER_VEHICLES_INFO "R_SHARED_MEM_CONTROLLER_ROUTER_VEHICLE_INFO"
#define SHARED_MEM_VIDEO_STREAM_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_STATS"
#define SHARED_MEM_RADIO_RX_QUEUE_INFO_STATS "/SYSTEM_SHARED_MEM_RADIO_RX_QUEUE_STATS"
#define SHARED_MEM_CONTROLLER_PING_STATS "/SYSTEM_CTRL_PING_STATS"

#define MAX_HISTORY_VIDEO_INTERVALS 50
#define MAX_HISTORY_STACK_RETRANSMISSION_INFO 100
#define MAX_RETRANSMISSION_PACKETS_IN_REQUEST 20

#define MAX_RADIO_RX_QUEUE_INFO_VALUES 50

typedef struct
{
   int iCountHitRSSI;
   int iCountHitSNR;
   int iCountHitRetr;
   int iCountHitRxLost;
   int iCountHitECUsed;
   int iCountHitECMax;
   int iCountHitVideoLost;
} ALIGN_STRUCT_SPEC_INFO type_adaptive_info;

typedef struct
{
   u32 uVehicleId;
   u8 uVideoStreamIndex;
   t_packet_header_video_segment PHVS;
   u32 uCurrentVideoProfileEncodingFlags;
   int iCurrentVideoWidth;
   int iCurrentVideoHeight;
   int iCurrentVideoTxSourceFPS;
   u32 uLastSetVideoBitrate;
   u32 uLastSetVideoKeyframeMs;
   u32 uCurrentECTimeMsPerSec;
   u32 uCurrentTxTimeMsPerSec;
   int iCurrentPacketsInBuffers;
   int iMaxPacketsInBuffers;
   u8 uDetectedH264Profile;
   u8 uDetectedH264ProfileConstrains;
   u8 uDetectedH264Level;
   int iDetectedFPS;
   int iDetectedSlices;
   int iDetectedKeyframeMs;
   int iAdaptiveVideoLevelNow;
   type_adaptive_info adaptiveHitsLow;
   type_adaptive_info adaptiveHitsHigh;
   bool bIsOnLowestAdaptiveLevel;
} ALIGN_STRUCT_SPEC_INFO shared_mem_video_stream_stats;

typedef struct
{
   shared_mem_video_stream_stats video_streams[MAX_VIDEO_PROCESSORS];
} ALIGN_STRUCT_SPEC_INFO shared_mem_video_stream_stats_rx_processors;

typedef struct
{
   u32 uVehiclesIds[MAX_CONCURENT_VEHICLES];

   bool bIsVehicleFastUplinkFromControllerLost[MAX_CONCURENT_VEHICLES];
   bool bIsVehicleSlowUplinkFromControllerLost[MAX_CONCURENT_VEHICLES];
   u32 uLastTimeReceivedAckFromVehicle[MAX_CONCURENT_VEHICLES];
   int iVehicleClockDeltaMilisec[MAX_CONCURENT_VEHICLES];
   bool bIsDoingRetransmissions[MAX_CONCURENT_VEHICLES];
   bool bIsAdaptiveVideoActive[MAX_CONCURENT_VEHICLES];
   u32 uAverageCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
   u32 uMaxCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
   u32 uMinCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
   u16 uCurrentAdaptiveECScheme[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO shared_mem_router_vehicles_runtime_info;


typedef struct
{
   u8 uPendingRxPackets[MAX_RADIO_RX_QUEUE_INFO_VALUES];
   u32 uMeasureIntervalMs;
   u32 uLastMeasureTime;
   u8 uCurrentIndex;
} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_rx_queue_info;


#define MAX_DBG_PING_DATAPOINTS 100
typedef struct
{
   int iCurrentDataPointIndex[MAX_RADIO_INTERFACES];
   u32 uRTTime[MAX_RADIO_INTERFACES][MAX_DBG_PING_DATAPOINTS];
} ALIGN_STRUCT_SPEC_INFO shared_mem_ctrl_ping_stats;

shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_read();
shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_write();
void shared_mem_video_stream_stats_rx_processors_close(shared_mem_video_stream_stats_rx_processors* pAddress);
shared_mem_video_stream_stats* get_shared_mem_video_stream_stats_for_vehicle(shared_mem_video_stream_stats_rx_processors* pSM, u32 uVehicleId);
void reset_video_stream_stats_for_vehicle(shared_mem_video_stream_stats_rx_processors* pSM, u32 uVehicleId);
void reset_video_stream_stats_detected_info(shared_mem_video_stream_stats* pVSStats);

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_read();
shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_write();
void shared_mem_router_vehicles_runtime_info_close(shared_mem_router_vehicles_runtime_info* pAddress);

shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_read();
shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_write();
void shared_mem_radio_rx_queue_info_close(shared_mem_radio_rx_queue_info* pAddress);

shared_mem_ctrl_ping_stats* shared_mem_ctrl_ping_stats_info_open_for_read();
shared_mem_ctrl_ping_stats* shared_mem_rctrl_ping_stats_info_open_for_write();
void shared_mem_ctrl_ping_stats_info_close(shared_mem_ctrl_ping_stats* pAddress);
