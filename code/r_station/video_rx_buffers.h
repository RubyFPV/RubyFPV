#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"


//  [packet header][video segment header][video seg header important][video data][000]
//  | pPH          | pPHVS               | pPHVSImp                  |pActualVideoData
//  | pRawData ptr                       | pVideoData ptr
//                                       [    <- video block packet size ->          ]
//                                                                   [-vid size-]

typedef struct
{
   u8* pRawData;
   u8* pVideoData; // pointer inside pRawData
   t_packet_header* pPH; // pointer inside pRawData
   t_packet_header_video_segment* pPHVS; // pointer inside pRawData
   t_packet_header_video_segment_important* pPHVSImp; // pointer inside pRawData
   u32 uReceivedTime;
   u32 uRequestedTime; // non zero if it was requested for retransmission
   bool bEmpty;
   bool bReconstructed;
}
type_rx_video_packet_info;

typedef struct
{
   type_rx_video_packet_info packets[MAX_TOTAL_PACKETS_IN_BLOCK];
   u16 uH264FrameIndex;
   u32 uVideoBlockIndex;
   int iTotalFramePackets;
   int iFramePacketStart;
   int iFramePacketEnd;
   int iMaxReceivedDataPacketIndex;
   int iMaxReceivedDataOrECPacketIndex;
   int iLastRecordedEOF;
   u32 uReceivedTime;
   int iBlockDataSize;
   int iBlockDataPackets;
   int iBlockECPackets;
   int iRecvDataPackets;
   int iRecvECPackets;
   int iReconstructedECUsed;
}
type_rx_video_block_info;

typedef struct
{
   unsigned int decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int decode_ec_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* p_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* p_decode_ec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
} type_fec_info;


class VideoRxPacketsBuffer
{
   public:
      VideoRxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex);
      virtual ~VideoRxPacketsBuffer();

      bool init(Model* pModel);
      bool uninit();
      void emptyBuffers(const char* szReason);
      
      bool hasVideoPacket(u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex);
      // Returns true if the packet was added
      bool checkAddVideoPacket(u8* pPacket, int iPacketLength);

      int getBufferBottomIndex();
      u32 getBufferBottomVideoBlockIndex();
      int getBufferTopIndex();
      u32 getBufferTopVideoBlockIndex();
      int getTopBufferMaxReceivedVideoBlockPacketIndex();

      int getCountBlocksInBuffer();
      type_rx_video_block_info* getTopBlockInBuffer();
      type_rx_video_block_info* getBlockInBufferFromBottom(int iDeltaPosition);
      type_rx_video_packet_info* getBottomBlockAndPacketInBuffer(type_rx_video_block_info** ppOutputBlock);
      int discardOldBlocks(u32 uCutOffTime);
      bool discardBottomBlockIfIncomplete();
      void advanceBottomPacketInBuffer();

   protected:

      bool _check_allocate_video_block_in_buffer(int iBufferIndex);
      void _empty_block_buffer_packet_index(int iBufferIndex, int iPacketIndex);
      void _empty_block_buffer_index(int iBufferIndex);
      void _empty_buffers(const char* szReason, t_packet_header* pPH, t_packet_header_video_segment* pPHVS);
      void _check_do_ec_for_video_block(int iBufferIndex);
      bool _add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength);

      static int m_siVideoBuffersInstancesCount;
      bool m_bInitialized;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;

      // Buffers state
      type_rx_video_block_info m_VideoBlocks[MAX_RXTX_BLOCKS_BUFFER];
      u8 m_TempVideoBuffer[MAX_PACKET_TOTAL_SIZE];
      bool m_bBuffersEmpty;
      int m_iTopBufferIndex;
      int m_iBottomBufferIndex;
      int m_iBottomBufferPacketIndex;

      type_fec_info m_ECRxInfo;
};

