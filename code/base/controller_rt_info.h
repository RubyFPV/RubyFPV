#pragma once
#include "base.h"
#include "config.h"
#include "hardware_radio.h"

#define SHARED_MEM_CONTROLLER_RUNTIME_INFO "/SYSTEM_RUBY_CONTROLLER_RT_INFO"
#define SHARED_MEM_CONTROLLER_DEBUG_VIDEO_RUNTIME_INFO "/SYSTEM_RUBY_CONTROLLER_DEBUG_VIDEO_RT_INFO"

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
   int iRecvVideoDataRate[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];

   u32 uFlagsAdaptiveVideo[SYSTEM_RT_INFO_INTERVALS];
   u32 uTotalCountOutputSkippedBlocks;

   controller_runtime_info_vehicle vehicles[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info;

typedef struct
{
   int iCurrentFrameBufferIndex;

   u16 uPreviousReceivedH264Frame;
   u16 uCurrentReceivedH264Frame;
   int iCurrentFrameRecvBytes;
   u32 uCurrentFrameFirstPacketTimeTensMS;
   u32 uCurrentFrameLastPacketTimeTensMS;

   u16 uLastComputedFPSAtFrameId;
   u32 uLastComputedFPSAtTime;
   int iComputedRxVideoFPS;
   u32 uReceivedFrameStartTime[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u8  uReceivedFrameDurationTensMs[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u8  uReceivedFrameNALFlags[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u16 uReceivedFrameTotalSizeBytes[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u32 uReceivedFrameThroughputBPS[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u8  uCaptureFramesDistanceTimes[SYSTEM_RT_INFO_INTERVALS_FRAMES];
   u32 uOutputFramePackets[SYSTEM_RT_INFO_INTERVALS_FRAMES];
      // byte 0: output regular packets
      // byte 1: output retransmitted packets
      // byte 2: outputed reconstructed packets (EC)
      // byte 3:
      //    bit 0: max EC was used
   u32 uOutputedFramesSizes[SYSTEM_RT_INFO_INTERVALS_FRAMES]; // high byte: is I/P frame; // lower bytes: frame size in bytes
   u32 uOutputFramesInfo[SYSTEM_RT_INFO_INTERVALS];
     // byte 0: end of frame or last rx delta milisec from start of this slice
     // byte 1: VIDEO_STATUS_FLAGS2_IS_NAL*** flags
     // byte 2-3: output video bytes
   u32 uVideoFramesProcessingTimes[SYSTEM_RT_INFO_INTERVALS_FRAMES];
     // bit 0..3 read camera
     // bit 4..7 time ms sent other packets
     // byte 1 time ms sent video packets
     // byte 2 time ms total processing frame
     // byte 3 time ms expected send video packets
} ALIGN_STRUCT_SPEC_INFO controller_debug_video_runtime_info;

controller_debug_video_runtime_info* controller_debug_video_rt_info_open_for_read();
controller_debug_video_runtime_info* controller_debug_video_rt_info_open_for_write();
void controller_debug_video_rt_info_close(controller_debug_video_runtime_info* pAddress);
void controller_debug_video_rt_info_init(controller_debug_video_runtime_info* pCVideoRTInfo);
void controller_debug_video_rt_info_advance_frame(controller_debug_video_runtime_info* pCVideoRTInfo, u16 uNewCurrentFrameH264Index, int iCurrentFPS, u32 uTimeNowMs);

controller_runtime_info* controller_rt_info_open_for_read();
controller_runtime_info* controller_rt_info_open_for_write();
void controller_rt_info_close(controller_runtime_info* pAddress);
void controller_rt_info_init(controller_runtime_info* pCRTInfo);

controller_runtime_info_vehicle* controller_rt_info_get_vehicle_info(controller_runtime_info* pRTInfo, u32 uVehicleId);
void controller_rt_info_update_ack_rt_time(controller_runtime_info* pRTInfo, u32 uVehicleId, int iRadioLink, u32 uRoundTripTime, u8 uAckType);
int controller_rt_info_will_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs);
int controller_rt_info_check_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs);
#ifdef __cplusplus
}  
#endif 
