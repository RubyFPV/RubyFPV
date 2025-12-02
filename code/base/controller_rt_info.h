#pragma once
#include "base.h"
#include "config.h"
#include "hardware_radio.h"

#define SHARED_MEM_CONTROLLER_RUNTIME_INFO "/SYSTEM_RUBY_CONTROLLER_RT_INFO"
#define SHARED_MEM_CONTROLLER_DEBUG_RUNTIME_INFO "/SYSTEM_RUBY_CONTROLLER_DEBUG_RT_INFO"

#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER ((u32)(((u32)0x01)<<1))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER ((u32)(((u32)0x01)<<2))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE ((u32)(((u32)0x01)<<3))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_USER ((u32)(((u32)0x01)<<5))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_LOWER ((u32)(((u32)0x01)<<6))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_HIGHER ((u32)(((u32)0x01)<<7))
#define CTRL_RT_INFO_FLAG_RECV_ACK ((u32)(((u32)0x01)<<8))

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   u32 uVehicleId;
   int iCountBlocksInVideoRxBuffers;
   u8 uMinAckTime[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uMaxAckTime[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uCountReqRetrPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uCountReqRetransmissions[SYSTEM_RT_INFO_INTERVALS];
   u8 uCountAckRetransmissions[SYSTEM_RT_INFO_INTERVALS];
   u8 uAckTimes[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uAckTypes[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   int iAckTimeIndex[MAX_RADIO_INTERFACES];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info_vehicle;

typedef struct
{
   int iMaxSNRDataForInterface;
   int iMaxDBMDataForInterface;
   u32 uLastUpdateTimeData;
   int iMaxSNRVideoForInterface;
   int iMaxDBMVideoForInterface;
   u32 uLastUpdateTimeVideo;
   int iMaxSNRForInterface;
   int iMaxDBMForInterface;
   u32 uLastUpdateTime;
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info_radio_signals;

typedef struct
{
   u32 uUpdateIntervalMs;
   u32 uCurrentSliceStartTime;
   int iCurrentIndex;
   int iCurrentIndex2;
   int iCurrentIndex3;
   int iDeltaIndexFromVehicle;
   u32 uSliceStartTimeMs[SYSTEM_RT_INFO_INTERVALS];
   u32 uSliceDurationMs[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxLastDeltaTime[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES]; // From start of slice, in ms
   u8 uRxVideoPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxVideoECPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxDataPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxHighPriorityPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxMissingPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxMissingPacketsMaxGap[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxProcessedPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxMaxAirgapSlots[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxMaxAirgapSlots2[SYSTEM_RT_INFO_INTERVALS];

   u8 uTxFirstDeltaTime[SYSTEM_RT_INFO_INTERVALS]; // From start of slice, in ms
   u8 uTxLastDeltaTime[SYSTEM_RT_INFO_INTERVALS]; // From start of slice, in ms
   u8 uTxPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uTxHighPriorityPackets[SYSTEM_RT_INFO_INTERVALS];
   
   u8 uOutputedVideoPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsRetransmitted[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsRetransmittedDiscarded[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocks[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksSkippedBlocks[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksSingleECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksTwoECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksMultipleECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoBlocksMaxECUsed[SYSTEM_RT_INFO_INTERVALS];

   u8 uOutputedAudioPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedAudioPacketsCorrected[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedAudioPacketsSkipped[SYSTEM_RT_INFO_INTERVALS];

   type_runtime_radio_rx_signal_info radioInterfacesSignalInfoVideo[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   type_runtime_radio_rx_signal_info radioInterfacesSignalInfoData[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   controller_runtime_info_radio_signals radioInterfacesSignals[MAX_RADIO_INTERFACES];

   u32 uFlagsAdaptiveVideo[SYSTEM_RT_INFO_INTERVALS];
   u32 uTotalCountOutputSkippedBlocks;

   controller_runtime_info_vehicle vehicles[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info;

typedef struct
{
   int iCurrentFrameBufferIndex;

   u8  uReceivedFrameDurationTensMs[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u8  uReceivedFrameNALFlags[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u16 uReceivedFrameTotalSizeBytes[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u32 uReceivedFrameThroughputBPS[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u8  uCaptureFramesDistanceTimes[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u32 uOutputedFramesSizes[SYSTEM_RT_INFO_INTERVALS_FRAMES]; // high byte: is I/P frame; // lower bytes: frame size in bytes
   u32 uOutputFramesInfo[SYSTEM_RT_INFO_INTERVALS];
     // byte 0: VIDEO_STATUS_FLAGS2_IS_NAL*** flags
     // byte 1-2: output video bytes
     // byte 3: end of frame or last rx delta milisec from start of this slice
   u32 uVideoFramesProcessingTimes[SYSTEM_RT_INFO_INTERVALS_FRAMES];

   int iRecvVideoDataRate[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];

} ALIGN_STRUCT_SPEC_INFO controller_debug_runtime_info;

controller_debug_runtime_info* controller_debug_rt_info_open_for_read();
controller_debug_runtime_info* controller_debug_rt_info_open_for_write();
void controller_debug_rt_info_close(controller_debug_runtime_info* pAddress);
void controller_debug_rt_info_init(controller_debug_runtime_info* pCDebugRTInfo);
void controller_debug_rt_info_advance_frame(controller_debug_runtime_info* pCDebugRTInfo);

controller_runtime_info* controller_rt_info_open_for_read();
controller_runtime_info* controller_rt_info_open_for_write();
void controller_rt_info_close(controller_runtime_info* pAddress);
void controller_rt_info_init(controller_runtime_info* pCRTInfo);

controller_runtime_info_vehicle* controller_rt_info_get_vehicle_info(controller_runtime_info* pRTInfo, u32 uVehicleId);
void controller_rt_info_update_ack_rt_time(controller_runtime_info* pRTInfo, u32 uVehicleId, int iRadioLink, u32 uRoundTripTime, u8 uAckType);
int controller_rt_info_will_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs);
int controller_rt_info_check_advance_index(controller_runtime_info* pRTInfo, controller_debug_runtime_info* pDebugRTInfo, u32 uTimeNowMs);
#ifdef __cplusplus
}  
#endif 
