#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem_controller_only.h"
#include "../base/parser_h264.h"
#include "video_rx_buffers.h"
#include "shared_vars_state.h"

#define MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH 20

typedef struct
{
   u32 uRetransmissionTimeMinim; // in miliseconds
   u32 uRetransmissionTimeAverage;
   u32 uRetransmissionTimeLast;
   u32 retransmissionTimePreviousBuffer[MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH];
   u32 uRetransmissionTimePreviousIndex;
   u32 uRetransmissionTimePreviousSum;
   u32 uRetransmissionTimePreviousSumCount;
}
type_retransmission_stats;

#define RX_PACKET_STATE_EMPTY 0
#define RX_PACKET_STATE_RECEIVED 0x01
#define RX_PACKET_STATE_OUTPUTED 0x02

typedef struct
{
   u32 uState;
   int video_data_length;
   int packet_length;
   u8 uRetrySentCount;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u8* pData;
}
type_received_block_packet_info;

typedef struct
{
   u32 video_block_index; // MAX_U32 if it's empty
   int video_data_length;
   int data_packets;
   int fec_packets;
   int received_data_packets;
   int received_fec_packets;

   int totalPacketsRequested;
   u32 uTimeFirstPacketReceived;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u32 uTimeLastUpdated; //0 for none
   type_received_block_packet_info packetsInfo[MAX_TOTAL_PACKETS_IN_BLOCK];

} type_received_block_info;


class ProcessorRxVideo
{
   public:
      ProcessorRxVideo(u32 uVehicleId, u8 uVideoStreamIndex);
      virtual ~ProcessorRxVideo();

      static void oneTimeInit();
      static ProcessorRxVideo* getVideoProcessorForVehicleId(u32 uVehicleId, u32 uVideoStreamIndex);
      //static void log(const char* format, ...);

      bool init();
      bool uninit();
      void fullResetState(const char* szReason);
      void resetReceiveState();
      void onControllerSettingsChanged();

      void pauseProcessing();
      void resumeProcessing();

      void setMustParseStream(bool bParse);
      bool isParsingStream();

      u32 getLastActivationTime();
      u32 getLastRetransmissionId();
      u32 getLastTimeRequestedRetransmission();
      u32 getLastTimeReceivedRetransmission();
      u32 getLastTimeVideoStreamChanged();
      u32 getLastestVideoPacketReceiveTime();
      int getVideoWidth();
      int getVideoHeight();
      int getVideoFPS();
      int getVideoType();
      u32 getLastTempRetrPauseResume();
      
      void updateHistoryStats(u32 uTimeNow);
      // Returns how many retransmission packets where requested, if any
      int periodicLoopProcessor(u32 uTimeNow, bool bForceSyncNow);
      void handleReceivedVideoRetrPacket(int interfaceNb, u8* pBuffer, int iBufferLength);
      void handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int iBufferLength);

      static int m_siInstancesCount;

      u32 m_uVehicleId;
      u8 m_uVideoStreamIndex;
      int m_iIndexVideoDecodeStats;
      VideoRxPacketsBuffer* m_pVideoRxBuffer;

   protected:
      void _resetOutputState();
      
      void updateControllerRTInfoAndVideoDecodingStats(u8* pRadioPacket, int iPacketLength);
      
      void _updateDebugStatsOnVideoPacket(type_rx_video_packet_info* pVideoPacket);
      void _checkUpdateRetransmissionsState();
      void checkUpdateRetransmissionsState();
      // Returns how many retransmission packets where requested, if any
      int checkAndRequestMissingPackets(bool bForceSyncNow);
      void checkAndDiscardBlocksTooOld();

      void _checkAndOutputAvailablePackets(type_global_state_vehicle_runtime_info* pRuntimeInfo, Model* pModel);
      void processAndOutputVideoPacket(type_rx_video_block_info* pVideoBlock, type_rx_video_packet_info* pVideoPacket, bool bWaitFullFrame);

      bool m_bInitialized;
      int m_iInstanceIndex;
      bool m_bPaused;
      bool m_bPauseTempRetrUntillANewVideoPacket;
      u32 m_uTimeLastResumedTempRetrPause;
      u32 m_uLastTimeActivated;
      
      // Configuration

      u32 m_uRetryRetransmissionAfterTimeoutMiliseconds;
      int m_iMilisecondsMaxRetransmissionWindow;
      u32 m_uTimeIntervalMsForRequestingRetransmissions;

      // Output state
      t_packet_header_video_segment m_LastOutputedVideoPacketInfo;
      type_rx_video_block_info m_CopyLastOutputedVideoRxBlockInfo;
      u32 m_uTimeLastOutputedVideoPacket;
      u32 m_uTimeReceivedLastOutputedVideoPacket;

      // Rx state 
      t_packet_header_video_segment m_NewestReceivedVideoPacketInfo;
      type_rx_video_block_info m_CopyNewestReceivedVideoRxBlockInfo;
      u32 m_uNewestReceivedVideoPacketTime;
      bool m_bMustParseStream;
      bool m_bWasParsingStream;
      ParserH264 m_ParserH264;

      u8 m_uLastReceivedVideoLinkProfile;

      u32 m_uLastTimeCheckedForMissingPackets;
      u32 m_uRequestRetransmissionUniqueId;
      u32 m_uLastTimeRequestedRetransmission;
      u32 m_uLastTimeReceivedRetransmission;

      u32 m_uLastTopBlockIdRequested;
      int m_iMaxRecvPacketTopBlockWhenRequested;

      u32 m_uEncodingsChangeCount;
      u32 m_uTimeLastVideoStreamChanged;

      u32 m_TimeLastHistoryStatsUpdate;
      u32 m_TimeLastRetransmissionsStatsUpdate;

      u32 m_uLastVideoBlockIndexResolutionChange;
      u32 m_uLastVideoBlockPacketIndexResolutionChange;
};


void discardRetransmissionsInfoAndBuffersOnLengthyOp();
