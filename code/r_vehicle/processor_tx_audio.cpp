/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_procs.h"
#include "processor_tx_audio.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"
#if defined (HW_PLATFORM_OPENIPC_CAMERA)
#include "video_source_majestic.h"
#endif

u8* p_ec_audio_packets[MAX_DATA_PACKETS_IN_BLOCK];
u8* p_ec_audio_ecs[MAX_FECS_PACKETS_IN_BLOCK];

ProcessorTxAudio::ProcessorTxAudio()
{
   m_iAudioStream = -1;
   m_fAudioRecordingFile = NULL;
   m_bLocalRecording = false;
   m_uTimeLastTryReadAudioInputStream = 0;
   m_StatsAudioInputComputedBps = 0;
   m_StatsTmpAudioInputReadBytes = 0;
   m_StatsTimeLastComputeAudioInputBps = 0;
   
   m_iBreakStampMatchPosition = 0;

   strcpy(m_szBreakStamp, "0123456789");
   m_szBreakStamp[10] = 10;
   m_szBreakStamp[11] = 0;
   m_pBuffers = new GenericTxECBuffers();
   resetState(NULL);
}

ProcessorTxAudio::~ProcessorTxAudio()
{
   stopLocalRecording();
   closeAudioStream();
   delete m_pBuffers;
   m_pBuffers = NULL;
}

void ProcessorTxAudio::init(Model* pModel)
{
   resetState(pModel);
}

void ProcessorTxAudio::resetState(Model* pModel)
{
   m_iSchemePacketSize = DEFAULT_AUDIO_PACKET_LENGTH;
   m_iSchemeDataPackets = 4;
   m_iSchemeECPackets = 2;

   if ( NULL == pModel )
   {
      log_line("[AudioTx] Reset state (no model). Current EC scheme: %d/%d, packet length: %d bytes", m_iSchemeDataPackets, m_iSchemeECPackets, m_iSchemePacketSize);
      return;
   }
   m_iSchemePacketSize = (int)pModel->audio_params.uPacketLength;
   m_iSchemeDataPackets = (int)((pModel->audio_params.uECScheme >> 4) & 0x0F);
   m_iSchemeECPackets = (int)(pModel->audio_params.uECScheme & 0x0F);

   if ( m_iSchemeDataPackets < 1 )
      m_iSchemeDataPackets = 1;

   if ( m_iSchemePacketSize > MAX_PACKET_PAYLOAD )
      m_iSchemePacketSize = MAX_PACKET_PAYLOAD;

   if ( NULL != m_pBuffers )
      m_pBuffers->init(MAX_BUFFERED_AUDIO_PACKETS, true, (u32)m_iSchemeDataPackets, (u32)m_iSchemeECPackets, m_iSchemePacketSize);
   log_line("[AudioTx] Reset state. Current EC scheme: %d/%d, packet length: %d bytes", m_iSchemeDataPackets, m_iSchemeECPackets, m_iSchemePacketSize);
}


u32 ProcessorTxAudio::getAverageAudioInputBps()
{
   return m_StatsAudioInputComputedBps;
}

int ProcessorTxAudio::openAudioStream()
{
   if ( m_iAudioStream >= 0 )
      close(m_iAudioStream);
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);

   m_iAudioStream = -1;
   m_fAudioRecordingFile = NULL;
   m_StatsTmpAudioInputReadBytes = 0;

   m_iBreakStampMatchPosition = 0;

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[AudioTx] Invalid params to open audio stream. Null model.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.enabled )
   {
      log_line("[AudioTx] Audio is disabled on vehicle. No audio stream to open.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      log_line("[AudioTx] No audio devices on vehicle. No audio stream to open.");
      return 0;
   }
  
   log_line("[AudioTx] Current EC scheme: %u/%u, packet size: %d bytes",
       m_iSchemeDataPackets, m_iSchemeECPackets, m_iSchemePacketSize);

   #if defined (HW_PLATFORM_RASPBERRY)
   log_line("[AudioTx] Opening audio input stream: %s", FIFO_RUBY_AUDIO1);
   m_iAudioStream = open(FIFO_RUBY_AUDIO1, O_RDONLY);
   if ( m_iAudioStream < 0 )
   {
      log_softerror_and_alarm("[AudioTx] Failed to open audio input stream: %s", FIFO_RUBY_AUDIO1);
      return 0;
   }
   log_line("[AudioTx] Opened audio input stream: %s successfully. fd = %d", FIFO_RUBY_AUDIO1, m_iAudioStream);
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   video_source_majestic_clear_audio_buffers();
   #endif

   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   startLocalRecording();
   #endif

   return 1;
}

void ProcessorTxAudio::closeAudioStream()
{
   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   stopLocalRecording();
   #endif

   if ( -1 != m_iAudioStream )
   {
      log_line("[AudioTx] Closed audio input stream %s", FIFO_RUBY_AUDIO1);
      close(m_iAudioStream);
   }
   m_iAudioStream = -1;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   video_source_majestic_clear_audio_buffers();
   #endif

   m_StatsAudioInputComputedBps = 0;
}

int ProcessorTxAudio::startLocalRecording()
{
   return 0;
   if ( m_bLocalRecording )
      return 0;

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[AudioTx] Invalid params to start recording. Null model.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.enabled )
   {
      log_line("[AudioTx] Audio is disabled on vehicle. No audio stream to record.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      log_line("[AudioTx] No audio devices on vehicle. No audio stream to record.");
      return 0;
   }

   m_bLocalRecording = true;
   m_iBreakStampMatchPosition = 0;
   
   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("[AudioTx] Local recording started. Opening local recording output file...");
   m_iRecordingFileNumber = 0;
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);

   char szFile[512];
   sprintf(szFile, "%s%s%d", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);
   m_fAudioRecordingFile = fopen(szFile, "wb");
   if ( NULL == m_fAudioRecordingFile )
   {
      log_softerror_and_alarm("[AudioTx] Failed to open output recording file %s", szFile);
      return 0;
   }
   else
      log_line("[AudioTx] Opened audio recording output file %s, fd:%d", szFile, fileno(m_fAudioRecordingFile));
   #endif

   return 1;
}

int ProcessorTxAudio::stopLocalRecording()
{
   return 0;
   if ( ! m_bLocalRecording )
      return 0;

   m_bLocalRecording = false;

   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("[AudioTx] Local recording stopped. Closing local recording output file...");
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);
   m_fAudioRecordingFile = NULL;

   char szBuff[256];
   for( int i=0; i<5; i++ )
   {
      m_iRecordingFileNumber++;
      sprintf(szBuff, "rm -rf %s%s%d 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);
      hw_execute_bash_command(szBuff, NULL);
   }
   m_iRecordingFileNumber = 0;
   #endif
   return 0;
}

int ProcessorTxAudio::tryReadAudioInputStream()
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->isAudioCapableAndEnabled()) )
      return 0;

   // Try to read only every 10 ms (100 times/sec)

   if ( g_TimeNow < m_uTimeLastTryReadAudioInputStream + 10 )
      return 0;
   
   m_uTimeLastTryReadAudioInputStream = g_TimeNow;

   u8 uBuffer[MAX_PACKET_PAYLOAD];
   int iCountRead = 0;

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( -1 == m_iAudioStream )
      return 0;
   
   fd_set readset;
   FD_ZERO(&readset);
   FD_SET(m_iAudioStream, &readset);

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 100; // 0.1 miliseconds timeout

   int selectResult = select(m_iAudioStream+1, &readset, NULL, NULL, &timePipeInput);
   if ( selectResult <= 0 )
      return 0;

   if( 0 == FD_ISSET(m_iAudioStream, &readset) )
      return 0;

   iCountRead = read(m_iAudioStream, uBuffer, m_iSchemePacketSize);
   if ( iCountRead < 0 )
   {
      log_error_and_alarm("[AudioTx] Failed to read from audio input fifo: %s, returned code: %d, error: %s", FIFO_RUBY_AUDIO1, iCountRead, strerror(errno));
      return -1;
   }
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   iCountRead = video_source_majestic_get_audio_data(uBuffer, m_iSchemePacketSize);
   #endif

   if ( iCountRead == 0 )
      return 0;

   m_StatsTmpAudioInputReadBytes += iCountRead;

   if ( g_TimeNow >= m_StatsTimeLastComputeAudioInputBps+500 )
   {
      m_StatsTimeLastComputeAudioInputBps = g_TimeNow;
      m_StatsAudioInputComputedBps = m_StatsTmpAudioInputReadBytes * 8 * 2;
      m_StatsTmpAudioInputReadBytes = 0;
      static int s_iCounterAudioTxStats = 0;
      s_iCounterAudioTxStats++;
      if ( (s_iCounterAudioTxStats % 10) == 0 )
         log_line("[AudioTx] Output audio bitrate: %u bps", m_StatsAudioInputComputedBps);
   }

   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   if ( m_bLocalRecording )
      _localRecordBuffer(uBuffer, iCountRead);
   #endif

   if ( NULL != m_pBuffers )
      m_pBuffers->addData(uBuffer, iCountRead);
   return 1;
}

void ProcessorTxAudio::_localRecordBuffer(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength <= 0) || (! m_bLocalRecording))
      return;

   bool bFoundBreakStamp = false;
   int iBreakFoundPosition = -1;
   u8* pTmp = pBuffer;
   for( int i=0; i<iLength; i++ )
   {
      if ( (*pTmp) != m_szBreakStamp[m_iBreakStampMatchPosition] )
      {
         pTmp++;
         m_iBreakStampMatchPosition = 0;
         continue;
      }
      pTmp++;
      m_iBreakStampMatchPosition++;
      if ( m_szBreakStamp[m_iBreakStampMatchPosition] == 0 )
      {
         iBreakFoundPosition = i+1;
         bFoundBreakStamp = true;
         break;
      }
   }

   if ( ! bFoundBreakStamp )
   {
      if ( NULL != m_fAudioRecordingFile )
      {
         int iWrite = fwrite(pBuffer, 1, iLength, m_fAudioRecordingFile);
         if ( iWrite != iLength )
            log_softerror_and_alarm("[AudioTx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, iLength);
      }
      return;
   }
     

   if ( NULL != m_fAudioRecordingFile )
   {
      if ( iBreakFoundPosition > 11 )
      { 
         int iWrite = fwrite(pBuffer, 1, iBreakFoundPosition-11, m_fAudioRecordingFile);
         if ( iWrite != iBreakFoundPosition-11 )
            log_softerror_and_alarm("[AudioTx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, iBreakFoundPosition);
      }
      fclose(m_fAudioRecordingFile);
   }

   m_iRecordingFileNumber++;
   char szBuff[256];
   sprintf(szBuff, "%s%s%d", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);

   m_fAudioRecordingFile = fopen(szBuff, "wb");
   if ( NULL == m_fAudioRecordingFile )
   {
      log_softerror_and_alarm("[AudioTx] Failed to open output recording file %s", szBuff);
      return;
   }

   log_line("[AudioTx] Opened audio recording output file %s, fd:%d", szBuff, fileno(m_fAudioRecordingFile));

   int toWrite = iLength - iBreakFoundPosition;
   if ( toWrite > 0 )
   {
      int iWrite = fwrite(&pBuffer[iBreakFoundPosition], 1, toWrite, m_fAudioRecordingFile);
      if ( iWrite != toWrite )
         log_softerror_and_alarm("[AudioTx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, toWrite);
   }
}

void ProcessorTxAudio::_sendAudioPacket(u8* pBuffer, int iLength, u32 uAudioPacketIndex)
{
   if ( (NULL == g_pCurrentModel) || (NULL == pBuffer) || ( iLength < m_iSchemePacketSize) )
      return;
   
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_AUDIO, PACKET_TYPE_AUDIO_SEGMENT, STREAM_ID_AUDIO);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + iLength;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uAudioPacketIndex, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), pBuffer, iLength);

   send_packet_to_radio_interfaces(packet, PH.total_length, -1);
}

int ProcessorTxAudio::getPendingAudioPackets()
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->isAudioCapableAndEnabled()) )
      return 0;
   return m_pBuffers->getUnsendPacketsCount();
}

void ProcessorTxAudio::sendAudioPackets()
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->isAudioCapableAndEnabled()) )
      return;
  
   u32 uBufferIndex = 0;
   int iBufferIndex = -1;
   int iPacketIndex = -1;
   type_generic_tx_ec_packet* pPacket = m_pBuffers->getMarkFirstUnsendPacket(&uBufferIndex, &iBufferIndex, &iPacketIndex);
   while ( NULL != pPacket )
   {   
      u32 uAudioPacketIndex = ((uBufferIndex & 0xFFFFFF) << 8) | ((u32)iPacketIndex);
      _sendAudioPacket(pPacket->uPacketData, pPacket->iFilledBytes, uAudioPacketIndex);

      //m_pBuffers->markPacketAsSent(iBufferIndex, iPacketIndex);
      pPacket = m_pBuffers->getMarkFirstUnsendPacket(&uBufferIndex, &iBufferIndex, &iPacketIndex);
   }
}