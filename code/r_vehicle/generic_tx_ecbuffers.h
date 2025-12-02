#pragma once

typedef struct
{
   u8 uPacketData[MAX_PACKET_PAYLOAD];
   int iFilledBytes;
   bool bSent;
}
type_generic_tx_ec_packet;

typedef struct
{
   u32 uBlockIndex;
   int iFilledPackets;
   type_generic_tx_ec_packet* pPackets[MAX_TOTAL_PACKETS_IN_BLOCK];
}
type_generic_tx_ec_block;


class GenericTxECBuffers
{
   public:
      GenericTxECBuffers();
      virtual ~GenericTxECBuffers();

      void init(int iMaxBlocks, bool bEnableCRC, u32 uDataPackets, u32 uECPackets, int iPacketLength);
      int addData(u8* pData, int iDataLength);
      int getUnsendPacketsCount();
      type_generic_tx_ec_packet* getMarkFirstUnsendPacket(u32* puBlockIndex, int* piBufferIndex, int* piPacketIndex);
      u8* getPacket(u32 uBlockIndex, u32 uPacketIndex, int* piOutputSize);
      void markPacketAsSent(int iBufferIndex, u32 uPacketIndex);
      
   protected:
      void _deleteBuffers();
      void _reinitBuffers();
      void _computeECDataOnCurrentBlock();
      bool m_bEnableCRC;
      int  m_iMaxBlocks;
      type_generic_tx_ec_block* m_pBlocks;
      u32  m_uBlockDataPackets;
      u32  m_uBlockECPackets;
      int  m_iBlockPacketLength;

      u32 m_uCurrentBlockIndex;
      u32 m_uCurrentBlockPacketIndex;
      int m_iBottomBufferIndex;
      int m_iTopBufferIndex;
      int m_iBottomBufferIndexFirstUnsend;
      int m_iBottomPacketIndexFirstUnsend;

      u8* m_p_ec_data_packets[MAX_DATA_PACKETS_IN_BLOCK];
      u8* m_p_ec_ec_packets[MAX_FECS_PACKETS_IN_BLOCK];
};
