/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga  petrusoroaga@yahoo.com
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
#include "../base/hardware.h"
#include "../common/string_utils.h"
#include "../radio/radioflags.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_short.h"
#include "../radio/radio_duplicate_det.h"
#include "radio_stats.h"

static u32 s_uControllerLinkStats_tmpRecv[MAX_RADIO_INTERFACES];
static u32 s_uControllerLinkStats_tmpRecvBad[MAX_RADIO_INTERFACES];
static u32 s_uControllerLinkStats_tmpRecvLost[MAX_RADIO_INTERFACES];

static u32 s_uLastTimeDebugPacketRecvOnNoLink = 0;
static int s_iRadioStatsEnableHistoryMonitor = 0;


void shared_mem_radio_stats_rx_hist_reset(shared_mem_radio_stats_rx_hist* pStats)
{
   if ( NULL == pStats )
      return;
   for(int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      for( int k=0; k<MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES; k++ )
      {
         pStats->interfaces_history[i].uHistPacketsTypes[k] = 0;
         pStats->interfaces_history[i].uHistPacketsCount[k] = 0;
      }
      pStats->interfaces_history[i].iCurrentSlice = 0;
      pStats->interfaces_history[i].uTimeLastUpdate = 0;
   }
}

void shared_mem_radio_stats_rx_hist_update(shared_mem_radio_stats_rx_hist* pStats, int iInterfaceIndex, u8* pPacket, u32 uTimeNow)
{
   if ( (NULL == pStats) || (NULL == pPacket) )
      return;
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   
   // Ignore video packets
   if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
      return;

   // First ever packet ?

   if ( pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate == 0 )
   if ( pStats->interfaces_history[iInterfaceIndex].iCurrentSlice == 0 )
   if ( pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[0] == 0 )
   {
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[0] = pPH->packet_type;
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[0] = 1;
      pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate = uTimeNow;
      return;
   }

   int iCurrentIndex = pStats->interfaces_history[iInterfaceIndex].iCurrentSlice;
   
   // Different second?

   if ( (u32)(pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate/1000) != (u32)(uTimeNow/1000) )
   {
      iCurrentIndex++;
      if ( iCurrentIndex >= MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES )
         iCurrentIndex = 0;

      pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iCurrentIndex] = 0xFF;
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex] = 0xFF;
      
      iCurrentIndex++;
      if ( iCurrentIndex >= MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES )
         iCurrentIndex = 0;

      pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iCurrentIndex] = pPH->packet_type;
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex] = 1;
      pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate = uTimeNow;
      pStats->interfaces_history[iInterfaceIndex].iCurrentSlice = iCurrentIndex;
      return;
   }

   // Different packet type from last one
   
   if ( pPH->packet_type != pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iCurrentIndex] )
   {
      iCurrentIndex++;
      if ( iCurrentIndex >= MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES )
         iCurrentIndex = 0;

      pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iCurrentIndex] = pPH->packet_type;
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex] = 1;
      pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate = uTimeNow;
      pStats->interfaces_history[iInterfaceIndex].iCurrentSlice = iCurrentIndex;
      return;
   }

   // Now it's the same packet type as the last one

   int iPrevIndex = iCurrentIndex;
   iPrevIndex--;
   if ( iPrevIndex < 0 )
      iPrevIndex = MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES - 1;

   // Add it if it's only the first time that it's duplicate

   if ( pPH->packet_type != pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iPrevIndex] )
   {
      iCurrentIndex++;
      if ( iCurrentIndex >= MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES )
         iCurrentIndex = 0;

      pStats->interfaces_history[iInterfaceIndex].uHistPacketsTypes[iCurrentIndex] = pPH->packet_type;
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex] = 1;
      pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate = uTimeNow;
      pStats->interfaces_history[iInterfaceIndex].iCurrentSlice = iCurrentIndex;
      return;
   }

   // Same packet as lasts ones, just increase the counter

   if ( pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex] < 255 )
      pStats->interfaces_history[iInterfaceIndex].uHistPacketsCount[iCurrentIndex]++;
   pStats->interfaces_history[iInterfaceIndex].uTimeLastUpdate = uTimeNow;
}

void radio_stats_reset(shared_mem_radio_stats* pSMRS, int graphRefreshInterval)
{
   if ( NULL == pSMRS )
      return;

   pSMRS->refreshIntervalMs = 350;
   pSMRS->graphRefreshIntervalMs = graphRefreshInterval;

   pSMRS->countLocalRadioLinks = 0;
   pSMRS->countVehicleRadioLinks = 0;
   pSMRS->countLocalRadioInterfaces = 0;
   
   pSMRS->lastComputeTime = 0;
   pSMRS->lastComputeTimeGraph = 0;

   pSMRS->all_downlinks_tx_time_per_sec = 0;
   pSMRS->tmp_all_downlinks_tx_time_per_sec = 0;
   pSMRS->uLastTimeReceivedAckFromAVehicle = 0;
   pSMRS->iMaxRxQuality = 0;

   pSMRS->timeLastRxPacket = 0;
   pSMRS->timeLastTxPacket = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_interfaces[i].assignedLocalRadioLinkId = -1;
      pSMRS->radio_interfaces[i].assignedVehicleRadioLinkId = -1;
      pSMRS->radio_interfaces[i].lastRecvDataRate = 0;
      pSMRS->radio_interfaces[i].lastRecvDataRateVideo = 0;
      pSMRS->radio_interfaces[i].lastRecvDataRateData = 0;
      pSMRS->radio_interfaces[i].lastSentDataRateVideo = 0;
      pSMRS->radio_interfaces[i].lastSentDataRateData = 0;
      pSMRS->radio_interfaces[i].lastReceivedRadioLinkPacketIndex = MAX_U32;

      pSMRS->radio_interfaces[i].totalRxBytes = 0;
      pSMRS->radio_interfaces[i].totalTxBytes = 0;
      pSMRS->radio_interfaces[i].rxBytesPerSec = 0;
      pSMRS->radio_interfaces[i].txBytesPerSec = 0;
      pSMRS->radio_interfaces[i].totalRxPackets = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsBad = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsLost = 0;
      pSMRS->radio_interfaces[i].totalTxPackets = 0;
      pSMRS->radio_interfaces[i].rxPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].txPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].timeLastRxPacket = 0;
      pSMRS->radio_interfaces[i].timeLastTxPacket = 0;

      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpTxBytes = 0;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;
      pSMRS->radio_interfaces[i].tmpTxPackets = 0;

      pSMRS->radio_interfaces[i].rxQuality = 0;
      pSMRS->radio_interfaces[i].rxRelativeQuality = 0;

      pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex = 0;
      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         pSMRS->radio_interfaces[i].hist_rxPacketsCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k] = 0xFF;
      }
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData = 0;
   }

   // Init radio links

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_links[i].matchingVehicleRadioLinkId = -1;
      pSMRS->radio_links[i].refreshIntervalMs = 500;
      pSMRS->radio_links[i].lastComputeTimePerSec = 0;
      pSMRS->radio_links[i].totalRxBytes = 0;
      pSMRS->radio_links[i].totalTxBytes = 0;
      pSMRS->radio_links[i].rxBytesPerSec = 0;
      pSMRS->radio_links[i].txBytesPerSec = 0;
      pSMRS->radio_links[i].totalRxPackets = 0;
      pSMRS->radio_links[i].totalTxPackets = 0;
      pSMRS->radio_links[i].rxPacketsPerSec = 0;
      pSMRS->radio_links[i].txPacketsPerSec = 0;
      pSMRS->radio_links[i].totalTxPacketsUncompressed = 0;
      pSMRS->radio_links[i].txUncompressedPacketsPerSec = 0;

      pSMRS->radio_links[i].timeLastRxPacket = 0;
      pSMRS->radio_links[i].timeLastTxPacket = 0;

      pSMRS->radio_links[i].tmpRxBytes = 0;
      pSMRS->radio_links[i].tmpTxBytes = 0;
      pSMRS->radio_links[i].tmpRxPackets = 0;
      pSMRS->radio_links[i].tmpTxPackets = 0;
      pSMRS->radio_links[i].tmpUncompressedTxPackets = 0;

      pSMRS->radio_links[i].lastTxInterfaceIndex = -1;
      pSMRS->radio_links[i].lastSentDataRateVideo = 0;
      pSMRS->radio_links[i].lastSentDataRateData = 0;
      pSMRS->radio_links[i].downlink_tx_time_per_sec = 0;
      pSMRS->radio_links[i].tmp_downlink_tx_time_per_sec = 0;
   }

   radio_stats_reset_received_info(pSMRS);
   radio_stats_reset_rx_signal_info(pSMRS);

   log_line("[RadioStats] Reset radio stats: %d ms refresh interval; %d ms refresh graph interval, total radio stats size: %d bytes", pSMRS->refreshIntervalMs, pSMRS->graphRefreshIntervalMs, sizeof(shared_mem_radio_stats));
}

void radio_stats_reset_received_info(shared_mem_radio_stats* pSMRS)
{
   if ( NULL == pSMRS )
      return;

   // Init streams

   for( int k=0; k<MAX_CONCURENT_VEHICLES; k++)
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      pSMRS->radio_streams[k][i].uVehicleId = 0;
      pSMRS->radio_streams[k][i].totalRxBytes = 0;
      pSMRS->radio_streams[k][i].totalTxBytes = 0;
      pSMRS->radio_streams[k][i].rxBytesPerSec = 0;
      pSMRS->radio_streams[k][i].txBytesPerSec = 0;
      pSMRS->radio_streams[k][i].totalRxPackets = 0;
      pSMRS->radio_streams[k][i].totalTxPackets = 0;
      pSMRS->radio_streams[k][i].totalLostPackets = 0;
      pSMRS->radio_streams[k][i].rxPacketsPerSec = 0;
      pSMRS->radio_streams[k][i].txPacketsPerSec = 0;
      pSMRS->radio_streams[k][i].timeLastRxPacket = 0;
      pSMRS->radio_streams[k][i].timeLastTxPacket = 0;
      pSMRS->radio_streams[k][i].uLastRecvStreamPacketIndex = 0;
      pSMRS->radio_streams[k][i].iHasMissingStreamPacketsFlag = 0;

      pSMRS->radio_streams[k][i].tmpRxBytes = 0;
      pSMRS->radio_streams[k][i].tmpTxBytes = 0;
      pSMRS->radio_streams[k][i].tmpRxPackets = 0;
      pSMRS->radio_streams[k][i].tmpTxPackets = 0;
   }

   // Reset radio interfaces

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_interfaces[i].lastRecvDataRate = 0;
      pSMRS->radio_interfaces[i].lastRecvDataRateVideo = 0;
      pSMRS->radio_interfaces[i].lastRecvDataRateData = 0;
      pSMRS->radio_interfaces[i].lastSentDataRateVideo = 0;
      pSMRS->radio_interfaces[i].lastSentDataRateData = 0;
      pSMRS->radio_interfaces[i].lastReceivedRadioLinkPacketIndex = MAX_U32;

      pSMRS->radio_interfaces[i].totalRxBytes = 0;
      pSMRS->radio_interfaces[i].totalTxBytes = 0;
      pSMRS->radio_interfaces[i].rxBytesPerSec = 0;
      pSMRS->radio_interfaces[i].txBytesPerSec = 0;
      pSMRS->radio_interfaces[i].totalRxPackets = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsBad = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsLost = 0;
      pSMRS->radio_interfaces[i].totalTxPackets = 0;
      pSMRS->radio_interfaces[i].rxPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].txPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].timeLastRxPacket = 0;
      pSMRS->radio_interfaces[i].timeLastTxPacket = 0;

      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpTxBytes = 0;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;
      pSMRS->radio_interfaces[i].tmpTxPackets = 0;

      pSMRS->radio_interfaces[i].rxQuality = 0;
      pSMRS->radio_interfaces[i].rxRelativeQuality = 0;

      pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex = 0;
      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         pSMRS->radio_interfaces[i].hist_rxPacketsCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k] = 0xFF;
      }
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData = 0;
   }

   // Reset radio links

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_links[i].totalRxBytes = 0;
      pSMRS->radio_links[i].totalTxBytes = 0;
      pSMRS->radio_links[i].rxBytesPerSec = 0;
      pSMRS->radio_links[i].txBytesPerSec = 0;
      pSMRS->radio_links[i].totalRxPackets = 0;
      pSMRS->radio_links[i].totalTxPackets = 0;
      pSMRS->radio_links[i].rxPacketsPerSec = 0;
      pSMRS->radio_links[i].txPacketsPerSec = 0;
      pSMRS->radio_links[i].totalTxPacketsUncompressed = 0;
      pSMRS->radio_links[i].txUncompressedPacketsPerSec = 0;

      pSMRS->radio_links[i].timeLastRxPacket = 0;
      pSMRS->radio_links[i].timeLastTxPacket = 0;

      pSMRS->radio_links[i].tmpRxBytes = 0;
      pSMRS->radio_links[i].tmpTxBytes = 0;
      pSMRS->radio_links[i].tmpRxPackets = 0;
      pSMRS->radio_links[i].tmpTxPackets = 0;
      pSMRS->radio_links[i].tmpUncompressedTxPackets = 0;

      pSMRS->radio_links[i].lastTxInterfaceIndex = -1;
      pSMRS->radio_links[i].lastSentDataRateVideo = 0;
      pSMRS->radio_links[i].lastSentDataRateData = 0;
      pSMRS->radio_links[i].downlink_tx_time_per_sec = 0;
      pSMRS->radio_links[i].tmp_downlink_tx_time_per_sec = 0;
   }

   radio_stats_reset_rx_signal_info(pSMRS);
   radio_duplicate_detection_remove_data_for_all_except(0);
}

void radio_stats_remove_received_info_for_vid(shared_mem_radio_stats* pSMRS, u32 uVehicleId)
{
   if ( (NULL == pSMRS) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return;

   // Init streams

   for( int k=0; k<MAX_CONCURENT_VEHICLES; k++)
   {
      if ( pSMRS->radio_streams[k][0].uVehicleId != uVehicleId )
         continue;
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         pSMRS->radio_streams[k][i].uVehicleId = 0;
         pSMRS->radio_streams[k][i].totalRxBytes = 0;
         pSMRS->radio_streams[k][i].totalTxBytes = 0;
         pSMRS->radio_streams[k][i].rxBytesPerSec = 0;
         pSMRS->radio_streams[k][i].txBytesPerSec = 0;
         pSMRS->radio_streams[k][i].totalRxPackets = 0;
         pSMRS->radio_streams[k][i].totalTxPackets = 0;
         pSMRS->radio_streams[k][i].totalLostPackets = 0;
         pSMRS->radio_streams[k][i].rxPacketsPerSec = 0;
         pSMRS->radio_streams[k][i].txPacketsPerSec = 0;
         pSMRS->radio_streams[k][i].timeLastRxPacket = 0;
         pSMRS->radio_streams[k][i].timeLastTxPacket = 0;
         pSMRS->radio_streams[k][i].uLastRecvStreamPacketIndex = 0;
         pSMRS->radio_streams[k][i].iHasMissingStreamPacketsFlag = 0;

         pSMRS->radio_streams[k][i].tmpRxBytes = 0;
         pSMRS->radio_streams[k][i].tmpTxBytes = 0;
         pSMRS->radio_streams[k][i].tmpRxPackets = 0;
         pSMRS->radio_streams[k][i].tmpTxPackets = 0;
      }
   }

   radio_duplicate_detection_remove_data_for_vid(uVehicleId);
}

void radio_stats_reset_rx_signal_info(shared_mem_radio_stats* pSMRS)
{
   if ( NULL == pSMRS )
      return;
   
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pSMRS->radio_interfaces[i].signalInfo.iAntennaCount = 1;
      reset_runtime_radio_rx_signal_info(&(pSMRS->radio_interfaces[i].signalInfo.signalInfoAll));
      reset_runtime_radio_rx_signal_info(&(pSMRS->radio_interfaces[i].signalInfo.signalInfoVideo));
      reset_runtime_radio_rx_signal_info(&(pSMRS->radio_interfaces[i].signalInfo.signalInfoData));
   }
}

void radio_stats_reset_interfaces_rx_info(shared_mem_radio_stats* pSMRS, const char* szReason)
{
   if ( NULL == pSMRS )
      return;

   if ( (NULL == szReason) || (0 == szReason[0]) )
      log_line("[RadioStats] Reset all radio interfaces Rx info");
   else
      log_line("[RadioStats] Reset all radio interfaces Rx info (reason: %s)", szReason);
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_interfaces[i].rxBytesPerSec = 0;
      pSMRS->radio_interfaces[i].totalRxBytes = 0;
      pSMRS->radio_interfaces[i].totalRxPackets = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsBad = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsLost = 0;
      pSMRS->radio_interfaces[i].rxPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].timeLastRxPacket = 0;

      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;

      pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex = 0;
      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         pSMRS->radio_interfaces[i].hist_rxPacketsCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k] = 0xFF;
      }
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData = 0;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      for( int k=0; k<MAX_RADIO_STREAMS; k++ )
      {
         pSMRS->radio_streams[i][k].totalRxPackets = 0;
      }
   }   
}

void radio_stats_set_graph_refresh_interval(shared_mem_radio_stats* pSMRS, int graphRefreshInterval)
{
   if ( NULL == pSMRS )
      return;
   pSMRS->graphRefreshIntervalMs = graphRefreshInterval;
   log_line("[RadioStats] Set radio stats graph refresh interval: %d ms", pSMRS->graphRefreshIntervalMs);
}

void radio_stats_enable_history_monitor(int iEnable)
{
   s_iRadioStatsEnableHistoryMonitor = iEnable;
}

void radio_stats_log_info(shared_mem_radio_stats* pSMRS, u32 uTimeNow)
{
   static int sl_iEnableRadioStatsLog = 0;
   static int sl_iEnableRadioStreamsStatsLog = 1;
   static int sl_iEnableRadioStatsLogTx = 0;

   static u32 sl_uLastTimeLoggedRadioStats = 0;

   if ( (0 == sl_iEnableRadioStatsLog) && (0 == sl_iEnableRadioStatsLogTx) && (0 == sl_iEnableRadioStreamsStatsLog) )
      return;

   u32 uInterval = 5000;
   if ( sl_iEnableRadioStreamsStatsLog )
      uInterval = 1000;
   if ( uTimeNow < sl_uLastTimeLoggedRadioStats + uInterval )
      return;

   sl_uLastTimeLoggedRadioStats = uTimeNow;

   char szBuff[512];
   char szBuff2[64];


   if ( sl_iEnableRadioStatsLogTx )
   {
      strcpy(szBuff, "Radio Interfaces total tx packets/bytes: ");
      for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
      {
         sprintf(szBuff2, "i-%d: %u/%u, ", i+1, pSMRS->radio_interfaces[i].totalTxPackets, pSMRS->radio_interfaces[i].totalTxBytes);
         strcat(szBuff, szBuff2);
      }
      log_line(szBuff);
      strcpy(szBuff, "Radio Interfaces tx packets/bytes/bits/sec: ");
      for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
      {
         sprintf(szBuff2, "i-%d: %u/%u/%u/sec, ", i+1, pSMRS->radio_interfaces[i].txPacketsPerSec, pSMRS->radio_interfaces[i].txBytesPerSec, pSMRS->radio_interfaces[i].txBytesPerSec*8);
         strcat(szBuff, szBuff2);
      }
      log_line(szBuff);
      return;
   }

   if ( sl_iEnableRadioStreamsStatsLog )
   {
      strcpy(szBuff, "Radio Streams total RX packets/sec: ");
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( pSMRS->radio_streams[i][0].uVehicleId == 0 )
            continue;
         for( int k=0; k<MAX_RADIO_STREAMS; k++ )
         {
            if ( 0 < pSMRS->radio_streams[i][k].rxPacketsPerSec )
               log_line("RX: VID %u, Stream %s: %d packets/sec (%d bytes/sec)", pSMRS->radio_streams[i][0].uVehicleId, str_get_radio_stream_name(k), pSMRS->radio_streams[i][k].rxPacketsPerSec, pSMRS->radio_streams[i][k].rxBytesPerSec);
         }
      }
      return;
   }

   log_line("----------------------------------------");
   log_line("Radio RX stats (refresh at %d ms, graphs at %d ms), %d local radio interfaces, %d local radio links, %d vehicle radio links:", pSMRS->refreshIntervalMs, pSMRS->graphRefreshIntervalMs, pSMRS->countLocalRadioInterfaces, pSMRS->countLocalRadioLinks, pSMRS->countVehicleRadioLinks);
   
   for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
   {
      if ( pSMRS->radio_streams[k][0].uVehicleId == 0 )
         continue;
      sprintf(szBuff, "Vehicle ID %u Streams total packets: ", pSMRS->radio_streams[k][0].uVehicleId);
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         sprintf(szBuff2, "%u", pSMRS->radio_streams[k][i].totalRxPackets);
         if ( 0 != i )
            strcat(szBuff, ", ");
         strcat(szBuff, szBuff2);
      }
      log_line(szBuff);
   }

   int iCountLinks = pSMRS->countLocalRadioLinks;
   strcpy(szBuff, "Radio Links current packets indexes: ");
   for( int i=0; i<iCountLinks; i++ )
   {
      sprintf(szBuff2, "%u", pSMRS->radio_links[i].totalRxPackets);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf total recv/bad/lost packets: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u/%u/%u", pSMRS->radio_interfaces[i].totalRxPackets, pSMRS->radio_interfaces[i].totalRxPacketsBad, pSMRS->radio_interfaces[i].totalRxPacketsLost );
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf current packets indexes: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u", pSMRS->radio_interfaces[i].totalRxPackets);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf total rx bytes: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u", pSMRS->radio_interfaces[i].totalRxBytes);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf total rx bytes/sec: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u", pSMRS->radio_interfaces[i].rxBytesPerSec);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   log_line("Radio Interfaces Max Rx Quality: %d%%", pSMRS->iMaxRxQuality);

   strcpy(szBuff, "Radio Interf current rx quality: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%d%%", pSMRS->radio_interfaces[i].rxQuality);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf current relative rx quality: ");
   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%d%%", pSMRS->radio_interfaces[i].rxRelativeQuality);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   
   log_line( "Radio streams RX throughput (global):");
   for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
   {
      szBuff[0] = 0;
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( pSMRS->radio_streams[k][i].uVehicleId != 0 )
         if ( (0 < pSMRS->radio_streams[k][i].rxBytesPerSec) || (0 < pSMRS->radio_streams[k][i].rxPacketsPerSec) )
         {
            sprintf(szBuff2, " VID %u, Stream %d: %u bps / %u pckts/s", pSMRS->radio_streams[k][i].uVehicleId, i, pSMRS->radio_streams[k][i].rxBytesPerSec*8, pSMRS->radio_streams[k][i].rxPacketsPerSec);
            if ( 0 != i )
               strcat(szBuff, ", ");
            strcat(szBuff, szBuff2);
         }
      }
      if ( 0 != szBuff[0] )
         log_line(szBuff);
   }
}

void radio_stats_log_tx_info(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uControllerId, u32 uTimeNow)
{
   if ( NULL == pSMRS )
      return;

   log_line( "Radio streams Tx throughput:");
   for( int iVehicle=0; iVehicle<MAX_CONCURENT_VEHICLES; iVehicle++ )
   {
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( pSMRS->radio_streams[iVehicle][i].uVehicleId != 0 )
         if ( (0 < pSMRS->radio_streams[iVehicle][i].txBytesPerSec) || (0 < pSMRS->radio_streams[iVehicle][i].txPacketsPerSec) )
         {
            if ( pSMRS->radio_streams[iVehicle][i].uVehicleId == BROADCAST_VEHICLE_ID )
               log_line("%d. To VID  Broadcast, Stream %d (%s): %s / %u pckts/s", iVehicle+1, i, str_get_radio_stream_name(i), str_format_bitrate_inline(pSMRS->radio_streams[iVehicle][i].txBytesPerSec*8), pSMRS->radio_streams[iVehicle][i].txPacketsPerSec);
            else if ( (uControllerId != 0) && (uControllerId != MAX_U32) && (uControllerId == pSMRS->radio_streams[iVehicle][i].uVehicleId) )
               log_line("%d. To CID %u, Stream %d (%s): %s / %u pckts/s", iVehicle+1, pSMRS->radio_streams[iVehicle][i].uVehicleId, i, str_get_radio_stream_name(i), str_format_bitrate_inline(pSMRS->radio_streams[iVehicle][i].txBytesPerSec*8), pSMRS->radio_streams[iVehicle][i].txPacketsPerSec);
            else if ( (uVehicleId != 0) && (uVehicleId != MAX_U32) && (uVehicleId == pSMRS->radio_streams[iVehicle][i].uVehicleId) )
               log_line("%d. To VID %u (self), Stream %d (%s): %s / %u pckts/s", iVehicle+1, pSMRS->radio_streams[iVehicle][i].uVehicleId, i, str_get_radio_stream_name(i), str_format_bitrate_inline(pSMRS->radio_streams[iVehicle][i].txBytesPerSec*8), pSMRS->radio_streams[iVehicle][i].txPacketsPerSec);
            else
               log_line("%d. To VID %u, Stream %d (%s): %s / %u pckts/s", iVehicle+1, pSMRS->radio_streams[iVehicle][i].uVehicleId, i, str_get_radio_stream_name(i), str_format_bitrate_inline(pSMRS->radio_streams[iVehicle][i].txBytesPerSec*8), pSMRS->radio_streams[iVehicle][i].txPacketsPerSec);
         }
      }
   }
}

void _radio_stats_update_kbps_values(shared_mem_radio_stats* pSMRS, u32 uDeltaTime)
{
   if ( NULL == pSMRS )
      return;

   // Update radio streams

   for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
   {
      if ( 0 == pSMRS->radio_streams[k][0].uVehicleId )
         continue;
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         pSMRS->radio_streams[k][i].rxBytesPerSec = (pSMRS->radio_streams[k][i].tmpRxBytes * 1000) / uDeltaTime;
         pSMRS->radio_streams[k][i].txBytesPerSec = (pSMRS->radio_streams[k][i].tmpTxBytes * 1000) / uDeltaTime;
         pSMRS->radio_streams[k][i].tmpRxBytes = 0;
         pSMRS->radio_streams[k][i].tmpTxBytes = 0;

         pSMRS->radio_streams[k][i].rxPacketsPerSec = (pSMRS->radio_streams[k][i].tmpRxPackets * 1000) / uDeltaTime;
         pSMRS->radio_streams[k][i].txPacketsPerSec = (pSMRS->radio_streams[k][i].tmpTxPackets * 1000) / uDeltaTime;
         pSMRS->radio_streams[k][i].tmpRxPackets = 0;
         pSMRS->radio_streams[k][i].tmpTxPackets = 0;
      }
   }
   // Update radio interfaces

   for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
   {
      pSMRS->radio_interfaces[i].rxBytesPerSec = (pSMRS->radio_interfaces[i].tmpRxBytes * 1000) / uDeltaTime;
      pSMRS->radio_interfaces[i].txBytesPerSec = (pSMRS->radio_interfaces[i].tmpTxBytes * 1000) / uDeltaTime;
      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpTxBytes = 0;

      pSMRS->radio_interfaces[i].rxPacketsPerSec = (pSMRS->radio_interfaces[i].tmpRxPackets * 1000) / uDeltaTime;
      pSMRS->radio_interfaces[i].txPacketsPerSec = (pSMRS->radio_interfaces[i].tmpTxPackets * 1000) / uDeltaTime;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;
      pSMRS->radio_interfaces[i].tmpTxPackets = 0;
   }

   pSMRS->all_downlinks_tx_time_per_sec = (pSMRS->tmp_all_downlinks_tx_time_per_sec * 1000) / uDeltaTime;
   pSMRS->tmp_all_downlinks_tx_time_per_sec = 0;
   // Transform from microsec to milisec
   pSMRS->all_downlinks_tx_time_per_sec /= 1000;
}

void _radio_stats_update_kbps_values_radio_links(shared_mem_radio_stats* pSMRS, int iRadioLinkId, u32 uDeltaTimeMs)
{
   if ( NULL == pSMRS )
      return;

   int iCountRadioLinks = pSMRS->countVehicleRadioLinks;
   if ( pSMRS->countLocalRadioLinks > iCountRadioLinks )
      iCountRadioLinks = pSMRS->countLocalRadioLinks;
   if ( (iRadioLinkId < 0) || (iRadioLinkId >= iCountRadioLinks) )
      return;

   // Update radio link kbps

   pSMRS->radio_links[iRadioLinkId].rxBytesPerSec = (pSMRS->radio_links[iRadioLinkId].tmpRxBytes * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].txBytesPerSec = (pSMRS->radio_links[iRadioLinkId].tmpTxBytes * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].tmpRxBytes = 0;
   pSMRS->radio_links[iRadioLinkId].tmpTxBytes = 0;

   pSMRS->radio_links[iRadioLinkId].rxPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpRxPackets * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].txPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpTxPackets * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].tmpRxPackets = 0;
   pSMRS->radio_links[iRadioLinkId].tmpTxPackets = 0;

   pSMRS->radio_links[iRadioLinkId].txUncompressedPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpUncompressedTxPackets * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].tmpUncompressedTxPackets = 0;

   pSMRS->radio_links[iRadioLinkId].downlink_tx_time_per_sec = (pSMRS->radio_links[iRadioLinkId].tmp_downlink_tx_time_per_sec * 1000) / uDeltaTimeMs;
   pSMRS->radio_links[iRadioLinkId].tmp_downlink_tx_time_per_sec = 0;

   // Transform from microsec to milisec
   pSMRS->radio_links[iRadioLinkId].downlink_tx_time_per_sec /= 1000;
}

// returns 1 if it was updated, 0 if unchanged

int radio_stats_periodic_update(shared_mem_radio_stats* pSMRS, u32 timeNow)
{
   if ( NULL == pSMRS )
      return 0;
   int iReturn = 0;

   int iCountRadioLinks = pSMRS->countLocalRadioLinks;
   for( int i=0; i<iCountRadioLinks; i++ )
   {
      if ( (timeNow >= pSMRS->radio_links[i].lastComputeTimePerSec + 1000) || (timeNow < pSMRS->radio_links[i].lastComputeTimePerSec) )
      {
         u32 uDeltaTime = timeNow - pSMRS->radio_links[i].lastComputeTimePerSec;
         if ( timeNow < pSMRS->radio_links[i].lastComputeTimePerSec )
            uDeltaTime = 500;
         _radio_stats_update_kbps_values_radio_links(pSMRS, i, uDeltaTime );
         pSMRS->radio_links[i].lastComputeTimePerSec = timeNow;
         iReturn = 1;
      }
   }

   int iRefreshIntervalRadioLinksQ = pSMRS->refreshIntervalMs;
   if ( iRefreshIntervalRadioLinksQ > 200 )
      iRefreshIntervalRadioLinksQ = 200;
   if ( (timeNow >= pSMRS->lastComputeTime + iRefreshIntervalRadioLinksQ) || (timeNow < pSMRS->lastComputeTime) )
   {
      pSMRS->lastComputeTime = timeNow;
      iReturn = 1;

      static u32 sl_uTimeLastUpdateRadioInterfaceskbpsValues = 0;

      if ( timeNow >= sl_uTimeLastUpdateRadioInterfaceskbpsValues + 500 )
      {
         u32 uDeltaTime = timeNow - sl_uTimeLastUpdateRadioInterfaceskbpsValues;
         sl_uTimeLastUpdateRadioInterfaceskbpsValues = timeNow;
         _radio_stats_update_kbps_values(pSMRS, uDeltaTime);
      }
  
      // Update RX quality for each radio interface
      int iTimeIntervalToInspect = 2000;
      int iIntervalsToUse = iTimeIntervalToInspect / pSMRS->graphRefreshIntervalMs;
      if ( iIntervalsToUse < 3 )
         iIntervalsToUse = 3;
      if ( iIntervalsToUse >= (int)(sizeof(pSMRS->radio_interfaces[0].hist_rxPacketsCount)/sizeof(pSMRS->radio_interfaces[0].hist_rxPacketsCount[0])) )
         iIntervalsToUse = sizeof(pSMRS->radio_interfaces[0].hist_rxPacketsCount)/sizeof(pSMRS->radio_interfaces[0].hist_rxPacketsCount[0]) - 1;

      pSMRS->iMaxRxQuality = 0;
      for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
      {
         u32 totalRecv = 0;
         u32 totalRecvBad = 0;
         u32 totalRecvLost = 0;
         int iIndex = pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex;
         for( int k=0; k<iIntervalsToUse; k++ )
         {
            totalRecv += pSMRS->radio_interfaces[i].hist_rxPacketsCount[iIndex];
            totalRecvBad += pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[iIndex];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];

            iIndex--;
            if ( iIndex < 0 )
               iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }
         if ( 0 == totalRecv )
            pSMRS->radio_interfaces[i].rxQuality = 0;
         else
            pSMRS->radio_interfaces[i].rxQuality = 100 - (100*(totalRecvLost+totalRecvBad))/(totalRecv+totalRecvLost);
      
         if ( pSMRS->radio_interfaces[i].rxQuality > pSMRS->iMaxRxQuality )
            pSMRS->iMaxRxQuality = pSMRS->radio_interfaces[i].rxQuality;
      }

      // Update relative RX quality for each radio interface

      for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
      {
         u32 totalRecv = 0;
         u32 totalRecvBad = 0;
         u32 totalRecvLost = 0;
         int iIndex = pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex;
         for( int k=0; k<iIntervalsToUse; k++ )
         {
            totalRecv += pSMRS->radio_interfaces[i].hist_rxPacketsCount[iIndex];
            totalRecvBad += pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[iIndex];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];
            iIndex--;
            if ( iIndex < 0 )
               iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }

         totalRecv += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount;
         totalRecvBad += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount;
         totalRecvLost += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo;
         totalRecvLost += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData;

         pSMRS->radio_interfaces[i].rxRelativeQuality = pSMRS->radio_interfaces[i].rxQuality;
         if ( (pSMRS->radio_interfaces[i].signalInfo.signalInfoAll.iDbmMin < 500) && (pSMRS->radio_interfaces[i].signalInfo.signalInfoAll.iDbmMin > -200) )
            pSMRS->radio_interfaces[i].rxRelativeQuality += pSMRS->radio_interfaces[i].signalInfo.signalInfoAll.iDbmMin/2;
         else if ( pSMRS->radio_interfaces[i].rxQuality == 0 )
            pSMRS->radio_interfaces[i].rxRelativeQuality -= 10000;

         pSMRS->radio_interfaces[i].rxRelativeQuality -= totalRecvLost;
         pSMRS->radio_interfaces[i].rxRelativeQuality += (totalRecv-totalRecvBad);
      }

      radio_stats_log_info(pSMRS, timeNow);
   }

   // Update rx graphs

   if ( timeNow >= pSMRS->lastComputeTimeGraph + pSMRS->graphRefreshIntervalMs || timeNow < pSMRS->lastComputeTimeGraph )
   {
      pSMRS->lastComputeTimeGraph = timeNow;
      iReturn = 1;

      for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
      {
         pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex++;
         if ( pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex >= MAX_HISTORY_RADIO_STATS_RECV_SLICES )
            pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex = 0;
         int iIndex = pSMRS->radio_interfaces[i].hist_rxPacketsCurrentIndex;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsCount[iIndex] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsCount[iIndex] = 0xFF;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] = 0xFF;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] = 0xFF;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex] = 0xFF;

         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[iIndex] = 0xFF;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsCount = 0;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBadCount = 0;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountVideo = 0;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLostCountData = 0;
      }
   }
   
   if ( iReturn == 1 )
   {
      static int s_iLogRadioRxTxThroughput = 0;
      static u32 s_uTimeLastLogRadioRxTxThroughput = 0;

      if ( s_iLogRadioRxTxThroughput )
      if ( timeNow >= s_uTimeLastLogRadioRxTxThroughput + 5000 )
      {
         s_uTimeLastLogRadioRxTxThroughput = timeNow;
         for( int i=0; i<pSMRS->countLocalRadioLinks; i++ )
         {
            if ( (0 == pSMRS->radio_links[i].txPacketsPerSec) && (0 == pSMRS->radio_links[i].rxPacketsPerSec) )
               log_line("* Local radio link %d: rx/tx packets/sec: %u / %u (total rx/tx packets: %u/%u)", i+1, pSMRS->radio_links[i].rxPacketsPerSec, pSMRS->radio_links[i].txPacketsPerSec, pSMRS->radio_links[i].totalRxPackets, pSMRS->radio_links[i].totalTxPackets);
            else
               log_line("* Local radio link %d: rx/tx packets/sec: %u / %u", i+1, pSMRS->radio_links[i].rxPacketsPerSec, pSMRS->radio_links[i].txPacketsPerSec);
         }
         for( int i=0; i<pSMRS->countLocalRadioInterfaces; i++ )
         {
            if ( (0 == pSMRS->radio_interfaces[i].txPacketsPerSec) && (0 == pSMRS->radio_interfaces[i].rxPacketsPerSec) )
               log_line("* Local radio interface %d: rx/tx packets/sec: %u / %u (total rx/tx packets: %u/%u)", i+1, pSMRS->radio_interfaces[i].rxPacketsPerSec, pSMRS->radio_interfaces[i].txPacketsPerSec, pSMRS->radio_interfaces[i].totalRxPackets, pSMRS->radio_interfaces[i].totalTxPackets);
            else
               log_line("* Local radio interface %d: rx/tx packets/sec: %u / %u", i+1, pSMRS->radio_interfaces[i].rxPacketsPerSec, pSMRS->radio_interfaces[i].txPacketsPerSec);
         }
         for( int iVehicle=0; iVehicle<MAX_CONCURENT_VEHICLES; iVehicle++)
         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         {
            if ( pSMRS->radio_streams[iVehicle][i].uVehicleId != 0 )
            if ( (pSMRS->radio_streams[iVehicle][i].totalRxPackets > 0 ) || (pSMRS->radio_streams[iVehicle][i].totalTxPackets > 0) )
            {
               char szVID[32];
               if ( pSMRS->radio_streams[iVehicle][i].uVehicleId == BROADCAST_VEHICLE_ID )
                  strcpy(szVID, "BROADCAST");
               else
                  sprintf(szVID, "%u", pSMRS->radio_streams[iVehicle][i].uVehicleId);
               if ( (0 == pSMRS->radio_streams[iVehicle][i].txPacketsPerSec) && (0 == pSMRS->radio_streams[iVehicle][i].rxPacketsPerSec) )
                  log_line("* Local VID/stream %s/%d: rx/tx packets/sec: %u / %u (total rx/tx packets: %u/%u)", szVID, i, pSMRS->radio_streams[iVehicle][i].rxPacketsPerSec, pSMRS->radio_streams[iVehicle][i].txPacketsPerSec, pSMRS->radio_streams[iVehicle][i].totalRxPackets, pSMRS->radio_streams[iVehicle][i].totalTxPackets);
               else
                  log_line("* Local VID/stream %s/%d: rx/tx packets/sec: %u / %u", szVID, i, pSMRS->radio_streams[iVehicle][i].rxPacketsPerSec, pSMRS->radio_streams[iVehicle][i].txPacketsPerSec);
            }
         }
      }
   }

   return iReturn;
}

void radio_stats_set_tx_card_for_radio_link(shared_mem_radio_stats* pSMRS, int iLocalRadioLink, int iTxCard)
{
   if ( NULL == pSMRS )
      return;
   pSMRS->radio_links[iLocalRadioLink].lastTxInterfaceIndex = iTxCard;
}

void radio_stats_set_card_current_frequency(shared_mem_radio_stats* pSMRS, int iRadioInterface, u32 freqKhz)
{
   if ( NULL == pSMRS )
      return;
   pSMRS->radio_interfaces[iRadioInterface].uCurrentFrequencyKhz = freqKhz;
}

void radio_stats_set_bad_data_on_current_rx_interval(shared_mem_radio_stats* pSMRS, int iRadioInterface)
{
   if ( NULL == pSMRS )
      return;
   if ( (iRadioInterface < 0) || (iRadioInterface >= MAX_RADIO_INTERFACES) )
      return;

   if ( 0 == pSMRS->radio_interfaces[iRadioInterface].hist_tmp_rxPacketsBadCount )
      pSMRS->radio_interfaces[iRadioInterface].hist_tmp_rxPacketsBadCount = 1;
   if ( 0 == pSMRS->radio_interfaces[iRadioInterface].hist_tmp_rxPacketsLostCountData )
      pSMRS->radio_interfaces[iRadioInterface].hist_tmp_rxPacketsLostCountData = 1;
   if ( 0 == s_uControllerLinkStats_tmpRecvLost[iRadioInterface] )
      s_uControllerLinkStats_tmpRecvLost[iRadioInterface] = 1;
}


int radio_stats_update_on_new_radio_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength, int iIsShortPacket, int iDataIsOk)
{
   if ( NULL == pSMRS )
      return -1;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d. Invalid radio info.", iInterfaceIndex+1);
      return -1;
   }
   int iIsAudioVideoData = 0;
   if ( ! iIsShortPacket )
   {
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      //if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
      //if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
      if ( (pPH->stream_packet_idx >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) == STREAM_ID_AUDIO )
         iIsAudioVideoData = 1;
      if ( (pPH->stream_packet_idx >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) >= STREAM_ID_VIDEO_1 )
         iIsAudioVideoData = 1;
   }
   pSMRS->timeLastRxPacket = timeNow;

   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nAntennaCount > pSMRS->radio_interfaces[iInterfaceIndex].signalInfo.iAntennaCount )
      pSMRS->radio_interfaces[iInterfaceIndex].signalInfo.iAntennaCount = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nAntennaCount;

   memcpy((u8*)&(pSMRS->radio_interfaces[iInterfaceIndex].signalInfo.signalInfoAll), (u8*)&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll), sizeof(type_runtime_radio_rx_signal_info));
   memcpy((u8*)&(pSMRS->radio_interfaces[iInterfaceIndex].signalInfo.signalInfoVideo), (u8*)&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo), sizeof(type_runtime_radio_rx_signal_info));
   memcpy((u8*)&(pSMRS->radio_interfaces[iInterfaceIndex].signalInfo.signalInfoData), (u8*)&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData), sizeof(type_runtime_radio_rx_signal_info));

   pSMRS->radio_interfaces[iInterfaceIndex].lastRecvDataRate = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;
   if ( iIsAudioVideoData )
      pSMRS->radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;
   else
      pSMRS->radio_interfaces[iInterfaceIndex].lastRecvDataRateData = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;
   
   // -------------------------------------------------------------
   // Begin - Update last received packet time

   u32 uTimeGap = timeNow - pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket;
   if ( 0 == pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket )
      uTimeGap = 0;
   u32 uShortTimeGap = uTimeGap;
   if ( uShortTimeGap > 254 )
      uShortTimeGap = 254;
   if ( pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[pSMRS->radio_interfaces[iInterfaceIndex].hist_rxPacketsCurrentIndex] == 0xFF )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[pSMRS->radio_interfaces[iInterfaceIndex].hist_rxPacketsCurrentIndex] = uShortTimeGap;
   else if ( uShortTimeGap > pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[pSMRS->radio_interfaces[iInterfaceIndex].hist_rxPacketsCurrentIndex] )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[pSMRS->radio_interfaces[iInterfaceIndex].hist_rxPacketsCurrentIndex] = uShortTimeGap;
     
   pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket = timeNow;
   
   
   // End - Update last received packet time
   // ----------------------------------------------------------------

   // ----------------------------------------------------------------------
   // Update rx bytes and packets count on interface

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxBytes += iPacketLength;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxBytes += iPacketLength;

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxPackets++;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxPackets++;

   // -------------------------------------------------------------------------
   // Begin - Update history and good/bad/lost packets for interface 

   pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsCount++;
   s_uControllerLinkStats_tmpRecv[iInterfaceIndex]++;

   if ( (0 == iDataIsOk) || (iPacketLength <= 0) )
   {
      pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsBadCount++;
      s_uControllerLinkStats_tmpRecvBad[iInterfaceIndex]++;
   }

   if ( NULL != pPacketBuffer )
   {
      if ( iIsShortPacket )
      {
         t_packet_header_short* pPHS = (t_packet_header_short*)pPacketBuffer;
         u32 uNext = ((pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex + 1) & 0xFF);
         if ( pPHS->packet_id != uNext  )
         {
            u32 uLost = pPHS->packet_id - uNext;
            if ( pPHS->packet_id < uNext )
               uLost = pPHS->packet_id + 255 - uNext;
            pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLostCountData += uLost;
            pSMRS->radio_interfaces[iInterfaceIndex].totalRxPacketsLost += uLost;
            s_uControllerLinkStats_tmpRecvLost[iInterfaceIndex] += uLost;
         }

         pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex = pPHS->packet_id;
      }
      else
      {
         t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
         
         if ( 0 != pPH->radio_link_packet_index )
         if ( pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex != MAX_U32 )
         if ( pPH->radio_link_packet_index > pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex + 1 )
         {
            u32 uLost = pPH->radio_link_packet_index - pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex - 1;
            //log_line("DBG lost %d packets, gap is %u ms wide, radio pkt %d", uLost, uTimeGap, pPH->radio_link_packet_index);
            if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
               pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLostCountVideo += uLost;
            else
               pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLostCountData += uLost;
            pSMRS->radio_interfaces[iInterfaceIndex].totalRxPacketsLost += uLost;
            s_uControllerLinkStats_tmpRecvLost[iInterfaceIndex] += uLost;
         }

         pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedRadioLinkPacketIndex = pPH->radio_link_packet_index;
      }
   }
   // End - Update history and good/bad/lost packets for interface 

   int nRadioLinkId = pSMRS->radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId;
   if ( nRadioLinkId < 0 || nRadioLinkId >= MAX_RADIO_INTERFACES )
   {
      if ( timeNow > s_uLastTimeDebugPacketRecvOnNoLink + 3000 )
      {
         s_uLastTimeDebugPacketRecvOnNoLink = timeNow;
         log_softerror_and_alarm("Received radio packet on radio interface %d that is not assigned to any radio links.", iInterfaceIndex+1);
      }
      return -1;
   }

   return 1;
}


// Returns 1 if ok, -1 for error

int radio_stats_update_on_unique_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength)
{
   if ( NULL == pSMRS )
      return -1;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d. Invalid radio info.", iInterfaceIndex+1);
      return -1;
   }
   
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   int nRadioLinkId = pSMRS->radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId;

   u32 uVehicleId = pPH->vehicle_id_src;

   u32 uStreamPacketIndex = (pPH->stream_packet_idx) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   u32 uStreamIndex = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   u8 uPacketType = pPH->packet_type;
   
   if ( uStreamIndex >= MAX_RADIO_STREAMS )
   {
      log_softerror_and_alarm("[RadioStats] Received invalid stream id %u, packet index %u, packet type: %s", uStreamIndex, uStreamPacketIndex, str_get_packet_type(uPacketType));
      uStreamIndex = 0;
   }
      
   if ( (uVehicleId == 0) || (uVehicleId == MAX_U32) )
   {
      log_softerror_and_alarm("[RadioStats] Received packet from invalid VID: %u", uVehicleId);
      return -1;
   }
   int iStreamsVehicleIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == pSMRS->radio_streams[i][0].uVehicleId )
      {
         iStreamsVehicleIndex = i;
         break;
      }
   }
   if ( iStreamsVehicleIndex == -1 )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( 0 == pSMRS->radio_streams[i][0].uVehicleId )
         {
            iStreamsVehicleIndex = i;
            pSMRS->radio_streams[iStreamsVehicleIndex][0].uVehicleId = uVehicleId;
            log_line("[RadioStats] Start using vehicle index %d in radio stats structure", iStreamsVehicleIndex, uVehicleId);
            char szTmp[256];
            szTmp[0] = 0;
            for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
            {
               char szT[32];
               sprintf(szT, "%u", pSMRS->radio_streams[k][0].uVehicleId);
               if ( 0 != k )
                  strcat(szTmp, ", ");
               strcat(szTmp, szT);
            }
            log_line("[RadioStats] Current vehicles in radio stats: [%s]", szTmp);
            break;
         }
      }
    
      // No more room for new vehicles. Reuse existing one
      if ( -1 == iStreamsVehicleIndex )
      {
         iStreamsVehicleIndex = MAX_CONCURENT_VEHICLES-1;
         log_softerror_and_alarm("[RadioStats] Rx: No more room in radio stats structure for new rx vehicle VID: %u. Reuse last index: %d", uVehicleId, iStreamsVehicleIndex);
         char szTmp[256];
         szTmp[0] = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            char szT[32];
            sprintf(szT, "%u", pSMRS->radio_streams[i][0].uVehicleId);
            if ( 0 != i )
               strcat(szTmp, ", ");
            strcat(szTmp, szT);
         }
         log_softerror_and_alarm("[RadioStats] Current vehicles in radio stats: [%s]", szTmp);
      }
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         pSMRS->radio_streams[iStreamsVehicleIndex][i].uVehicleId = uVehicleId;
         pSMRS->radio_streams[iStreamsVehicleIndex][i].totalRxBytes = 0;
         pSMRS->radio_streams[iStreamsVehicleIndex][i].tmpRxBytes = 0;

         pSMRS->radio_streams[iStreamsVehicleIndex][i].totalRxPackets = 0;
         pSMRS->radio_streams[iStreamsVehicleIndex][i].tmpRxPackets = 0;
      }
   }

   // -------------------------------------------------------------
   // Begin - Update last received packet time

   pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].timeLastRxPacket = timeNow;

   if ( (nRadioLinkId >= 0) && (nRadioLinkId < MAX_RADIO_INTERFACES) )
   {
      pSMRS->radio_links[nRadioLinkId].timeLastRxPacket = timeNow;
   }
   else
   {
      if ( timeNow > s_uLastTimeDebugPacketRecvOnNoLink + 3000 )
      {
         s_uLastTimeDebugPacketRecvOnNoLink = timeNow;
         log_softerror_and_alarm("[RadioStats] Received radio packet on radio interface %d that is not assigned to any radio links.", iInterfaceIndex+1);
      }
      return -1;
   }

   // End - Update last received packet time
 
   if ( uStreamPacketIndex > pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].uLastRecvStreamPacketIndex )
   {
      if ( pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].uLastRecvStreamPacketIndex != 0 )
      if ( uStreamPacketIndex > pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].uLastRecvStreamPacketIndex + 1 )
         pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].iHasMissingStreamPacketsFlag = 1;
      pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].uLastRecvStreamPacketIndex = uStreamPacketIndex;
   }

   if ( 0 ==  pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].totalRxPackets )
   {
      log_line("[RadioStats] Start receiving radio stream %d (%s), stream packet index: %u, from VID %u, packet module: %d, packet length: %d,%d, type: %s",
        (int)uStreamIndex, str_get_radio_stream_name(uStreamIndex), uStreamPacketIndex, uVehicleId,
        (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->total_length, iPacketLength, str_get_packet_type(pPH->packet_type));
      log_line("[RadioStats] Started receiving on local radio link: %d, local radio interface: %d", nRadioLinkId, iInterfaceIndex);
   }
   pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].totalRxBytes += iPacketLength;
   pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].tmpRxBytes += iPacketLength;

   pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].totalRxPackets++;
   pSMRS->radio_streams[iStreamsVehicleIndex][uStreamIndex].tmpRxPackets++;
   
   if ( (nRadioLinkId >= 0) && (nRadioLinkId < MAX_RADIO_INTERFACES) )
   {
      pSMRS->radio_links[nRadioLinkId].totalRxBytes += iPacketLength;
      pSMRS->radio_links[nRadioLinkId].tmpRxBytes += iPacketLength;
      
      pSMRS->radio_links[nRadioLinkId].totalRxPackets++;
      pSMRS->radio_links[nRadioLinkId].tmpRxPackets++;
   }
   return 1;
}

void radio_stats_update_on_packet_sent_on_radio_interface(shared_mem_radio_stats* pSMRS, u32 timeNow, int interfaceIndex, int iPacketLength)
{
   if ( NULL == pSMRS )
      return;
   if ( interfaceIndex < 0 || interfaceIndex >= hardware_get_radio_interfaces_count() )
      return;
   
   pSMRS->timeLastTxPacket = timeNow;
   
   // Update radio interfaces

   pSMRS->radio_interfaces[interfaceIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_interfaces[interfaceIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_interfaces[interfaceIndex].totalTxPackets++;
   pSMRS->radio_interfaces[interfaceIndex].tmpTxPackets++;
}

void radio_stats_update_on_packet_sent_on_radio_link(shared_mem_radio_stats* pSMRS, u32 timeNow, int iLocalLinkIndex, int iStreamIndex, int iPacketLength)
{
   if ( NULL == pSMRS )
      return;
   if ( iLocalLinkIndex < 0 || iLocalLinkIndex >= MAX_RADIO_INTERFACES )
      iLocalLinkIndex = 0;
   if ( iStreamIndex < 0 || iStreamIndex >= MAX_RADIO_STREAMS )
      iStreamIndex = 0;

   // Update radio link

   pSMRS->radio_links[iLocalLinkIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_links[iLocalLinkIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_links[iLocalLinkIndex].totalTxPackets++;
   pSMRS->radio_links[iLocalLinkIndex].tmpTxPackets++;

   pSMRS->radio_links[iLocalLinkIndex].totalTxPacketsUncompressed++;
   pSMRS->radio_links[iLocalLinkIndex].tmpUncompressedTxPackets++;
}

void radio_stats_update_on_packet_sent_for_radio_stream(shared_mem_radio_stats* pSMRS, u32 timeNow, u32 uVehicleId, int iStreamIndex, u8 uPacketType, int iPacketLength)
{
   if ( NULL == pSMRS )
      return;
   if ( (iStreamIndex < 0) || (iStreamIndex >= MAX_RADIO_STREAMS) )
      iStreamIndex = 0;

   // Broadcasted packet?
   if ( uVehicleId == 0 )
      uVehicleId = BROADCAST_VEHICLE_ID;

   int iStreamsVehicleIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == pSMRS->radio_streams[i][0].uVehicleId )
      {
         iStreamsVehicleIndex = i;
         break;
      }
   }
   if ( iStreamsVehicleIndex == -1 )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( 0 == pSMRS->radio_streams[i][0].uVehicleId )
         {
            iStreamsVehicleIndex = i;
            log_line("[RadioStats] Tx: Start using vehicle index %d in radio stats structure, for VID: %u", iStreamsVehicleIndex, uVehicleId);
            log_line("[RadioStats] Tx: VID dest: %u, packet type: %s", uVehicleId, str_get_packet_type(uPacketType));
            char szTmp[256];
            szTmp[0] = 0;
            for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
            {
               char szT[32];
               sprintf(szT, "%u", pSMRS->radio_streams[k][0].uVehicleId);
               if ( 0 != k )
                  strcat(szTmp, ", ");
               strcat(szTmp, szT);
            }
            log_line("[RadioStats] Current vehicles in radio stats: [%s]", szTmp);
            break;
         }
      }
    
      // No more room for new vehicles. Reuse existing one
      if ( -1 == iStreamsVehicleIndex )
      {
         iStreamsVehicleIndex = MAX_CONCURENT_VEHICLES-1;
         log_softerror_and_alarm("[RadioStats] Tx: No more room in radio stats structure for new tx vehicle VID: %u. Reuse last index: %d", uVehicleId, iStreamsVehicleIndex);
         log_softerror_and_alarm("[RadioStats] Tx: VID dest: %u, packet type: %s", uVehicleId, str_get_packet_type(uPacketType));
         char szTmp[256];
         szTmp[0] = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            char szT[32];
            sprintf(szT, "%u", pSMRS->radio_streams[i][0].uVehicleId);
            if ( 0 != i )
               strcat(szTmp, ", ");
            strcat(szTmp, szT);
         }
         log_softerror_and_alarm("[RadioStats] Current vehicles in radio stats: [%s]", szTmp);
      }
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         pSMRS->radio_streams[iStreamsVehicleIndex][i].uVehicleId = uVehicleId;
   }

   // Update radio streams

   pSMRS->radio_streams[iStreamsVehicleIndex][iStreamIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_streams[iStreamsVehicleIndex][iStreamIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_streams[iStreamsVehicleIndex][iStreamIndex].totalTxPackets++;
   pSMRS->radio_streams[iStreamsVehicleIndex][iStreamIndex].tmpTxPackets++;
}

void radio_stats_set_tx_radio_datarate_for_packet(shared_mem_radio_stats* pSMRS, int iInterfaceIndex, int iLocalRadioLinkIndex, int iDataRate, int iIsVideoPacket)
{
   if ( NULL == pSMRS )
      return;

   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( (iLocalRadioLinkIndex < 0) || (iLocalRadioLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( iIsVideoPacket )
   {
      pSMRS->radio_interfaces[iInterfaceIndex].lastSentDataRateVideo = iDataRate;
      pSMRS->radio_links[iLocalRadioLinkIndex].lastSentDataRateVideo = iDataRate;
   }
   else
   {
      pSMRS->radio_interfaces[iInterfaceIndex].lastSentDataRateData = iDataRate;
      pSMRS->radio_links[iLocalRadioLinkIndex].lastSentDataRateData = iDataRate;    
   }
}

int radio_stats_get_reset_stream_lost_packets_flags(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uStreamIndex)
{
   if ( NULL == pSMRS )
      return 0;

   int iVehicleIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == pSMRS->radio_streams[i][uStreamIndex].uVehicleId )
      {
         iVehicleIndex = i;
         break;
      }
   }
   if ( -1 == iVehicleIndex )
      return -1;

   int iRet = pSMRS->radio_streams[iVehicleIndex][uStreamIndex].iHasMissingStreamPacketsFlag;
   pSMRS->radio_streams[iVehicleIndex][uStreamIndex].iHasMissingStreamPacketsFlag = 0;

   if ( 0 == pSMRS->radio_streams[iVehicleIndex][uStreamIndex].uLastRecvStreamPacketIndex )
      return 0;
   return iRet;
}

u32 radio_stats_get_time_last_received_packet_on_stream(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uStreamIndex)
{
   if ( (NULL == pSMRS) || (uStreamIndex >= MAX_RADIO_STREAMS) )
      return 0;

   int iVehicleIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == pSMRS->radio_streams[i][uStreamIndex].uVehicleId )
      {
         iVehicleIndex = i;
         break;
      }
   }
   if ( -1 == iVehicleIndex )
      return -1;

   return pSMRS->radio_streams[iVehicleIndex][uStreamIndex].timeLastRxPacket;
}
