#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/parser_h264.h"
#include "../radio/radiopackets2.h"

//  [packet header][video segment header][video seg header important][video data][0000  ][dbg]
//  | pPH          | pPHVS               | pPHVSImp                  |pActualVideoData  |
//                                       [     <- error corrected data ->               ]
//                                       [     <- video block packet size   ->          ]
//                                                                   [-vid size-]

typedef struct
{
   u8* pRawData;
   u8* pVideoData; // pointer inside pRawData
   t_packet_header* pPH; // pointer inside pRawData
   t_packet_header_video_segment* pPHVS; // pointer inside pRawData
   t_packet_header_video_segment_important* pPHVSImp; // pointer inside pRawData
   bool bEmpty;
}
type_tx_video_packet_info;


class VideoTxPacketsBuffer
{
   public:
      VideoTxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex);
      virtual ~VideoTxPacketsBuffer();

      bool init(Model* pModel);
      bool uninit();
      void discardBuffer();
      void setLastFrameTimers(u32 uTimeReadCamera, u32 uTimeSendVideo, u32 uTimeExpectedSendVideoTime, u32 uTimeSendOthers, u32 uTotalProcessingTime);
      void setLastFrameDistanceMs(u32 uDistanceMs);
      void setCurrentRealFPS(int iFPS);
      int  getCurrentRealFPS();

      void setCustomECScheme(u16 uECScheme);
      int  getCurrentTotalBlockPackets();
      void updateVideoHeader(Model* pModel);
      void appendDataToCurrentFrame(u8* pVideoData, int iDataSize, u32 uNALPresenceFlags, bool bIsEndOfFrame, u32 uTimeDataAvailable);
      bool hasPendingPacketsToSend();
      int  sendAvailablePackets(int iCountPacketsAferVideo);
      void resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex);
      void resendVideoPacketsFromFrameEnd(u32 uRetransmissionId, u16 uH264FrameIndex, u8 uPacketsToEOF);

      void setTelemetryInfoVideoThroughput(u32 uVideoBitsPerSec);

      u32 getLastFrameExpectedTxTimeMicros();
      u32 getLastFrameSentDRMin_BPS();
      u32 getLastFrameSentDRMax_BPS();
      int getCurrentUsableRawVideoDataSize();
      bool getResetOverflowFlag();
      int getCurrentMaxUsableRawVideoDataSize();

   protected:

      void _checkAllocatePacket(int iBufferIndex, int iPacketIndex);
      void _fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, bool bIsECPacket, int iRawVideoDataSize, bool bIsLastPacket);
      int _addNewVideoPacket(u8* pRawVideoData, int iRawVideoDataSize, int iRemainingVideoPackets, bool bIsLastPacket);
      void _sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId, int iCountPacketsAferVideo);
      static int m_siVideoBuffersInstancesCount;
      bool m_bInitialized;
      bool m_bOverflowFlag;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;
      int m_iVideoStreamInfoIndex;
      ParserH264 m_ParserInputH264;

      u16 m_uCurrentH264FrameIndex;
      u32 m_uLastFrameTimers;
      u32 m_uLastFrameDistanceMs;
      int m_iCurrentRealFPS;
      u32 m_uNextVideoBlockIndexToGenerate;
      u32 m_uNextVideoBlockPacketIndexToGenerate;
      u32 m_uNextBlockPacketSize;
      u32 m_uNextBlockDataPackets;
      u32 m_uNextBlockECPackets;
      u32 m_uLastAppliedECSchemeDataPackets;
      u32 m_uLastAppliedECSchemeECPackets;
      u16 m_uCustomECScheme; // low byte: ec, high byte: data, 0 or 0xFFFF for default
      t_packet_header m_PacketHeader;
      t_packet_header_video_segment m_PacketHeaderVideo;
      t_packet_header_video_segment_important m_PacketHeaderVideoImportant;
      t_packet_header_video_segment* m_pLastPacketHeaderVideoFilledIn;
      t_packet_header_video_segment_important* m_pLastPacketHeaderVideoImportantFilledIn;
      int m_iUsableRawVideoDataSize;

      int m_iNextBufferIndexToFill;
      int m_iNextBufferPacketIndexToFill;
      int m_iCurrentBufferIndexToSend;
      int m_iCurrentBufferPacketIndexToSend;
      u32 m_uTimeDataAvailable;
      u8* m_pTempVideoFrameBuffer;
      int m_iTempVideoFrameBufferSize;
      int m_iTempVideoBufferFilledBytes;
      u32 m_uTempNALPresenceFlags;
      type_tx_video_packet_info m_VideoPackets[MAX_RXTX_BLOCKS_BUFFER][MAX_TOTAL_PACKETS_IN_BLOCK];

      u32 m_uRadioStreamPacketIndex;
      u32 m_uExpectedFrameTransmissionTimeMicros;

      u32 m_uLastTimeSentVideoPacketMicros;
      u32 m_uLastSentVideoPacketDurationMicros;
      u32 m_uLastSentVideoPacketDatarateBPS;
      u32 m_uLastSentVideoPacketDatarateMinBPS;
      u32 m_uLastSentVideoPacketDatarateMaxBPS;

      u32 m_uTelemetryVideoBitsPerSec;
      u32 m_uTelemetryTotalVideoBitsPerSec;
      u32 m_uTelemetryVideoTxTimeMsPerSec;
};

