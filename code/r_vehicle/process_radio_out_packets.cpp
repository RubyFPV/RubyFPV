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

#include "process_radio_out_packets.h"
#include "../base/ruby_ipc.h"
#include "../base/tx_powers.h"
#include "../radio/radiolink.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"

extern u16 s_countTXVideoPacketsOutPerSec[2];
extern u16 s_countTXDataPacketsOutPerSec[2];
extern u16 s_countTXCompactedPacketsOutPerSec[2];


void preprocess_radio_out_packet(u8* pPacketBuffer, int iPacketLength, bool bIsEndOfTransmissionFrame)
{
   if ( (NULL == pPacketBuffer) || (iPacketLength <= 0) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
      log_line("Sending pairing request confirmation to controller (from VID %u to CID %u)", pPH->vehicle_id_src, pPH->vehicle_id_dest);

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      // Update Ruby telemetry info if we are sending Ruby telemetry to controller
      // Update also the telemetry extended extra info: retransmissions info

      if ( pPH->packet_type == PACKET_TYPE_DEBUG_INFO )
      {
         type_u32_couters* pCounters = (type_u32_couters*) (pPacketBuffer + sizeof(t_packet_header));
         memcpy(pCounters, &g_CoutersMainLoop, sizeof(type_u32_couters));
      }

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
      {
         t_packet_header_ruby_telemetry_short* pPHRTShort = (t_packet_header_ruby_telemetry_short*) (pPacketBuffer + sizeof(t_packet_header));
         if ( g_bHasFastUplinkFromController )
            pPHRTShort->uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER;
         else
            pPHRTShort->uRubyFlags &= ~FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER;

         if ( g_bHasSlowUplinkFromController )
            pPHRTShort->uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER;
         else
            pPHRTShort->uRubyFlags &= ~FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER;
      }

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      {
         t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*) (pPacketBuffer + sizeof(t_packet_header));
         
         g_iVehicleSOCTemperatureC = pPHRTE->temperatureC;
         pPHRTE->downlink_tx_video_bitrate_bps = g_pProcessorTxVideo->getCurrentVideoBitrateAverageLastMs(500);
         pPHRTE->downlink_tx_video_all_bitrate_bps = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverageLastMs(500);
         if ( NULL != g_pProcessorTxAudio )
            pPHRTE->downlink_tx_data_bitrate_bps += g_pProcessorTxAudio->getAverageAudioInputBps();
         else
            pPHRTE->downlink_tx_data_bitrate_bps = 0;

         pPHRTE->downlink_tx_video_packets_per_sec = s_countTXVideoPacketsOutPerSec[0] + s_countTXVideoPacketsOutPerSec[1];
         pPHRTE->downlink_tx_data_packets_per_sec = s_countTXDataPacketsOutPerSec[0] + s_countTXDataPacketsOutPerSec[1];
         pPHRTE->downlink_tx_compacted_packets_per_sec = s_countTXCompactedPacketsOutPerSec[0] + s_countTXCompactedPacketsOutPerSec[1];

         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            // positive: regular datarates, negative: mcs rates;
            pPHRTE->last_sent_datarate_bps[i][0] = get_last_tx_used_datarate_bps_video(i);
            pPHRTE->last_sent_datarate_bps[i][1] = get_last_tx_used_datarate_bps_data(i);

            pPHRTE->last_recv_datarate_bps[i] = g_UplinkInfoRxStats[i].lastReceivedDataRate;

            pPHRTE->uplink_rssi_dbm[i] = 0;
            if ( g_UplinkInfoRxStats[i].lastReceivedDBM < 500 )
            if ( g_UplinkInfoRxStats[i].lastReceivedDBM > -200 )
               pPHRTE->uplink_rssi_dbm[i] = g_UplinkInfoRxStats[i].lastReceivedDBM + 200;

            if ( g_UplinkInfoRxStats[i].lastReceivedSNR < 500 )
            {
               pPHRTE->uplink_rssi_snr[i] = 0;
               if ( g_UplinkInfoRxStats[i].lastReceivedSNR >= 0 )
                  pPHRTE->uplink_rssi_snr[i] = g_UplinkInfoRxStats[i].lastReceivedSNR;
            }
            else
               pPHRTE->uplink_rssi_snr[i] = 0xFF;

            pPHRTE->uplink_link_quality[i] = g_SM_RadioStats.radio_interfaces[i].rxQuality;
            
            if ( hardware_radio_index_is_wifi_radio(i))
            {
               if ( g_TimeLastReceivedFastRadioPacketFromController < g_TimeNow-TIMEOUT_LINK_TO_CONTROLLER_LOST )
                  pPHRTE->uplink_link_quality[i] = 0;
               else if ( (TIMEOUT_LINK_TO_CONTROLLER_LOST >= 2000) && (g_TimeLastReceivedFastRadioPacketFromController < g_TimeNow-(TIMEOUT_LINK_TO_CONTROLLER_LOST-1000)) && (pPHRTE->uplink_link_quality[i] > 20) )
                  pPHRTE->uplink_link_quality[i] = 20;
            }
            else
            {
               if ( g_TimeLastReceivedSlowRadioPacketFromController < g_TimeNow-TIMEOUT_LINK_TO_CONTROLLER_LOST )
                  pPHRTE->uplink_link_quality[i] = 0;
               else if ( (TIMEOUT_LINK_TO_CONTROLLER_LOST >= 2000) && (g_TimeLastReceivedSlowRadioPacketFromController < g_TimeNow-(TIMEOUT_LINK_TO_CONTROLLER_LOST-1000)) && (pPHRTE->uplink_link_quality[i] > 20) )
                  pPHRTE->uplink_link_quality[i] = 20;
            }

            if ( hardware_radio_index_is_wifi_radio(i) )
            if ( ! g_bHasFastUplinkFromController )
               pPHRTE->uplink_link_quality[i] = 0;

            if ( ! hardware_radio_index_is_wifi_radio(i) )
            if ( ! g_bHasSlowUplinkFromController )
               pPHRTE->uplink_link_quality[i] = 0;
         }

         if ( g_bHasFastUplinkFromController )
            pPHRTE->uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER;
         else
            pPHRTE->uRubyFlags &= ~FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER;

         if ( g_bHasSlowUplinkFromController )
            pPHRTE->uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER;
         else
            pPHRTE->uRubyFlags &= ~FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER;

         if ( g_pCurrentModel->uModelRuntimeStatusFlags & MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE )
            pPHRTE->uExtraRubyFlags |= FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE;
         else
            pPHRTE->uExtraRubyFlags &= ~FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE;

         if ( g_pCurrentModel->uModelRuntimeStatusFlags & MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE_TEMPERATURE )
            pPHRTE->uExtraRubyFlags |= FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE_HOT;
         else
            pPHRTE->uExtraRubyFlags &= ~FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE_HOT;
         
         // Update tx powers info
         for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
         {
            pPHRTE->iTxPowers[iLink] = 0;

            for( int iInt=0; iInt < hardware_get_radio_interfaces_count(); iInt++ )
            {
                if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[iInt] == iLink )
                if ( hardware_radio_index_is_wifi_radio(iInt) )
                {
                   int iRawTxPower = get_last_tx_power_used_for_radiointerface(iInt);
                   if ( iRawTxPower != 0 )
                   {
                      int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[iInt];
                      if ( iRadioInterfacelModel < 0 )
                         iRadioInterfacelModel = -iRadioInterfacelModel;
                      int imWPower = tx_powers_convert_raw_to_mw(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel, iRawTxPower);

                      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iInt] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
                         imWPower = tx_powers_get_mw_boosted_value_from_mw(imWPower, true, false);
                      else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iInt] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
                         imWPower = tx_powers_get_mw_boosted_value_from_mw(imWPower, false, true);
                      
                      if ( iRawTxPower < 0 )
                         imWPower = -imWPower;
                      pPHRTE->iTxPowers[iLink] = imWPower;
                   }
                   break;
                }
            }
         }

         // Update extra info: retransmissions
         
         if ( pPHRTE->extraSize > 0 )
         if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
         if ( pPH->total_length == (sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
         {
            memcpy( pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6), (u8*)&g_PHTE_Retransmissions, sizeof(g_PHTE_Retransmissions));
         }
      }
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY )
   {
      t_packet_header_fc_telemetry* pPHFCTelem = (t_packet_header_fc_telemetry*) (pPacketBuffer + sizeof(t_packet_header));
      if ( pPHFCTelem->flight_mode & FLIGHT_MODE_ARMED )
         g_bVehicleArmed = true;
      else if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED )
         g_bVehicleArmed = true;
      else
         g_bVehicleArmed = false;
   }
}