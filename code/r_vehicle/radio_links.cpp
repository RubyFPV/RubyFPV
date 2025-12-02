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

#include "radio_links.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_radio_serial.h"
#include "../base/ruby_ipc.h"
#include "../base/radio_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_duplicate_det.h"
#include "../utils/utils_vehicle.h"
#include "shared_vars.h"
#include "timers.h"
#include "processor_relay.h"

static bool s_bRadioLinksAreMarkedForRestart = false;
static u32 s_uRadioLinksStartTime = 0;


// Returns true if configuration has been updated
// It's called only on vehicle side

bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_radio_stats* pSMRadioStats, shared_mem_process_stats* pProcessStats)
{
   bool bMissmatch = false;

   log_line("--------------------------------");
   log_line("CONFIGURE RADIO INTERFACES START");

   if ( NULL == pModel )
   {
      log_line("Configuring all radio interfaces (%d radio interfaces) failed.", hardware_get_radio_interfaces_count());
      log_error_and_alarm("INVALID MODEL parameter (no model, NULL)");
      log_line("CONFIGURE RADIO END ------------------------------------------------------------");
      return false;
   }

   // Populate radio interfaces radio flags from radio links radio flags and rates

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( (pModel->radioInterfacesParams.interface_link_id[i] >= 0) && (pModel->radioInterfacesParams.interface_link_id[i] < pModel->radioLinksParams.links_count) )
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioLinksParams.link_radio_flags[pModel->radioInterfacesParams.interface_link_id[i]];
      else
         log_softerror_and_alarm("No radio link or invalid radio link (%d of %d) assigned to radio interface %d.", pModel->radioInterfacesParams.interface_link_id[i], pModel->radioLinksParams.links_count, i+1);
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      int nRadioLinkId = pModel->radioInterfacesParams.interface_link_id[i];
      if ( (nRadioLinkId < 0) || (nRadioLinkId >= pModel->radioLinksParams.links_count) )
         continue;

      if ( ! pRadioHWInfo->isConfigurable )
      {
         if ( NULL != pSMRadioStats )
            radio_stats_set_card_current_frequency(pSMRadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }
   }

   log_line("Configuring all radio interfaces (%d radio interfaces, %d radio links)", hardware_get_radio_interfaces_count(), pModel->radioLinksParams.links_count);

   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("A relay link is enabled on radio link %d (now at %s), on %s", pModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(pModel->radioLinksParams.link_frequency_khz[pModel->relay_params.isRelayEnabledOnRadioLinkId]), str_format_frequency(pModel->relay_params.uRelayFrequencyKhz));

   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++ )
   {
      if ( pModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_softerror_and_alarm("Radio Link %d is disabled! Skipping it.", iLink+1 );
         continue;
      }

      u32 uRadioLinkFrequency = pModel->radioLinksParams.link_frequency_khz[iLink];

      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
      if ( pModel->relay_params.uRelayFrequencyKhz != 0 )
      {
         uRadioLinkFrequency = pModel->relay_params.uRelayFrequencyKhz;
         log_line("Radio link %d is a relay link on %s", iLink+1, str_format_frequency(uRadioLinkFrequency));
      }
   
      log_line("Radio link %d must be set to %s", iLink+1, str_format_frequency(uRadioLinkFrequency));

      int iLinkConfiguredInterfacesCount = 0;

      for( int iInterface=0; iInterface<hardware_get_radio_interfaces_count(); iInterface++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[iInterface] != iLink )
            continue;
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         {
            log_softerror_and_alarm("Radio Interface %d (assigned to radio link %d) is disabled!", iInterface+1, iLink+1 );
            continue;
         }

         if ( ! hardware_radioindex_supports_frequency(iInterface, uRadioLinkFrequency ) )
         {
            log_softerror_and_alarm("Radio interface %d (assigned to radio link %d) does not support radio link frequency %s!", iInterface+1, iLink+1, str_format_frequency(uRadioLinkFrequency));
            bMissmatch = true;
            continue;
         }

         if ( hardware_radio_index_is_sik_radio(iInterface) )
         {
            log_line("Configuring SiK radio interface %d for radio link %d", iInterface+1, iLink+1);
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
            if ( NULL == pRadioHWInfo )
            {
               log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", iInterface+1);
               continue;
            }
            u32 uFreqKhz = uRadioLinkFrequency;
            u32 uTxPower = pModel->radioInterfacesParams.interface_raw_power[iInterface];
            u32 uECC = (pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_ECC)? 1:0;
            u32 uLBT = (pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_LBT)? 1:0;
            u32 uMCSTR = (pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_MCSTR)? 1:0;
            u32 uDataRate = pModel->radioLinksParams.downlink_datarate_data_bps[iLink];
            bool bDataRateOk = false;
            for( int k=0; k<getSiKAirDataRatesCount(); k++ )
            {
               if ( (int)uDataRate == getSiKAirDataRates()[k] )
               {
                  bDataRateOk = true;
                  break;
               }
            }

            if ( ! bDataRateOk )
            {
               log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
               uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
            }

            int iRetry = 0;
            while ( iRetry < 2 )
            {
               int iRes = hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
                  uRadioLinkFrequency, uTxPower, uDataRate,
                  uECC, uLBT, uMCSTR,
                  pProcessStats);
               if ( iRes != 1 )
               {
                  log_softerror_and_alarm("Failed to configure SiK radio interface %d", iInterface+1);
                  iRetry++;
               }
               else
               {
                  log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                     iInterface+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
                  if ( NULL != pSMRadioStats )
                     radio_stats_set_card_current_frequency(pSMRadioStats, iInterface, uFreqKhz);
                  break;
               }
            }
            if ( iRetry < 2 )
               iLinkConfiguredInterfacesCount++;
         }
         else if ( hardware_radio_index_is_serial_radio(iInterface) )
         {
            iLinkConfiguredInterfacesCount++;          
         }
         else
         {
            radio_utils_set_interface_frequency(pModel, iInterface, iLink, uRadioLinkFrequency, pProcessStats, 0);
            if ( NULL != pSMRadioStats )
               radio_stats_set_card_current_frequency(pSMRadioStats, iInterface, uRadioLinkFrequency);
            iLinkConfiguredInterfacesCount++;
         }

         if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d in relay mode)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
         }
         else
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         }
         pModel->radioInterfacesParams.interface_current_frequency_khz[iInterface] = uRadioLinkFrequency;
      }
      log_line("Configured a total of %d radio interfaces for radio link %d", iLinkConfiguredInterfacesCount, iLink+1);
   }

   if ( bMissmatch )
   if ( 1 == pModel->radioLinksParams.links_count )
   if ( 1 == pModel->radioInterfacesParams.interfaces_count )
   {
      log_line("There was a missmatch of frequency configuration for the single radio link present on vehicle. Reseting it to default.");
      pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY;
      pModel->radioInterfacesParams.interface_current_frequency_khz[0] = pModel->radioLinksParams.link_frequency_khz[0];
      radio_utils_set_interface_frequency(pModel, 0, 0, pModel->radioLinksParams.link_frequency_khz[0], pProcessStats, 0);
      if ( NULL != pSMRadioStats )
         radio_stats_set_card_current_frequency(pSMRadioStats, 0, pModel->radioLinksParams.link_frequency_khz[0]);
      log_line("Set radio interface 1 to link 1 frequency %s", str_format_frequency(pModel->radioLinksParams.link_frequency_khz[0]) );
   }
   hardware_save_radio_info();
   hardware_sleep_ms(50);

   log_line("CONFIGURE RADIO END ---------------------------------------------------------");

   return bMissmatch;
}

int radio_links_open_rxtx_radio_interfaces()
{
   log_line("OPENING INTERFACES BEGIN =========================================================");
   s_uRadioLinksStartTime = g_TimeNow = get_current_timestamp_ms();
   log_line("[RadioLinks] Opening RX/TX radio interfaces...");
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("Relaying is enabled on radio link %d on frequency: %s.", g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));

   int countOpenedForRead = 0;
   int countOpenedForWrite = 0;
   int iCountSikInterfacesOpened = 0;
   int iCountSerialInterfacesOpened = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iRadioLinkId;
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iRadioLinkId;
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   // Init RX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
         log_line("Open radio interface %d (%s) for radio link %d in relay mode ...", i+1, pRadioHWInfo->szName, iRadioLinkId+1);
      else
         log_line("Open radio interface %d (%s) for radio link %d ...", i+1, pRadioHWInfo->szName, iRadioLinkId+1);

      if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
           (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
      {
         int nRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLinkId];
         radio_utils_set_datarate_atheros(g_pCurrentModel, i, nRateTx, 0);
      }

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_sik_open_for_read_write(i) > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForRead++;
               iCountSikInterfacesOpened++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_serial_open_for_read_write(i) > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForRead++;
               iCountSerialInterfacesOpened++;
            }
            radio_tx_set_serial_packet_size(i, DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE);
         }
         else
         {
            if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
            else
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;      
            countOpenedForRead++;
         }
      }
   }

   s_uRadioLinksStartTime = g_TimeNow = get_current_timestamp_ms();

   if ( countOpenedForRead == 0 )
   {
      log_error_and_alarm("[RadioLinks] Failed to find or open any RX interface for receiving data.");
      radio_links_close_rxtx_radio_interfaces();
      return -1;
   }


   // Init TX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( ! g_SM_RadioStats.radio_interfaces[i].openedForRead )
            {
               if ( hardware_radio_sik_open_for_read_write(i) > 0 )
               {
                 g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
                 countOpenedForWrite++;
                 iCountSikInterfacesOpened++;
               }
            }
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWrite++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( ! g_SM_RadioStats.radio_interfaces[i].openedForRead )
            {
               if ( hardware_radio_serial_open_for_read_write(i) > 0 )
               {
                 g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
                 countOpenedForWrite++;
                 iCountSerialInterfacesOpened++;
               }
            }
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWrite++;
            }
         }
         else
         {
            radio_open_interface_for_write(i);
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
            countOpenedForWrite++;
         }
      }
   }

   if ( (0 < iCountSikInterfacesOpened) || (0 < iCountSerialInterfacesOpened) )
   {
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_start_tx_thread();
   }

   s_uRadioLinksStartTime = g_TimeNow = get_current_timestamp_ms();

   if ( 0 == countOpenedForWrite )
   {
      log_error_and_alarm("[RadioLinks] Can't find any TX interfaces for video/data or failed to init it.");
      radio_links_close_rxtx_radio_interfaces();
      return -1;
   }

   log_line("[RadioLinks] Opening RX/TX radio interfaces complete. %d interfaces for RX, %d interfaces for TX :", countOpenedForRead, countOpenedForWrite);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      char szFlags[128];
      szFlags[0] = 0;
      str_get_radio_capabilities_description(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i], szFlags);

      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for read/write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Interface %s, %s, %s opened for read, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else
         log_line(" * Interface %s, %s, %s not used. Flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
   }
   log_line("OPENING INTERFACES END ============================================================");

   g_pCurrentModel->logVehicleRadioInfo();
   return 0;
}


void radio_links_close_rxtx_radio_interfaces()
{
   log_line("[RadioLinks] Closing all radio interfaces (rx/tx).");

   s_uRadioLinksStartTime = 0;

   radio_tx_mark_quit();
   hardware_sleep_ms(10);
   radio_tx_stop_tx_thread();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         hardware_radio_sik_close(i);
      else if ( hardware_radio_is_serial_radio(pRadioHWInfo) )
         hardware_radio_serial_close(i);
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->openedForWrite )
         radio_close_interface_for_write(i);
   }

   radio_close_interfaces_for_read();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }
   log_line("[RadioLinks] Closed all radio interfaces (rx/tx)."); 
}


bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParamsNew)
{
   if ( (NULL == pModel) || (NULL == pRadioLinkParamsNew) )
      return false;
   if ( (iRadioLink < 0) || (iRadioLink >= pModel->radioLinksParams.links_count) )
      return false;

   s_uRadioLinksStartTime = g_TimeNow = get_current_timestamp_ms();

   // Update frequencies if needed
   // Update HT20/HT40 if needed

   bool bUpdateFreq = false;
   if ( pRadioLinkParamsOld->link_frequency_khz[iRadioLink] != pRadioLinkParamsNew->link_frequency_khz[iRadioLink] )
      bUpdateFreq = true;
   if ( (pRadioLinkParamsOld->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) != 
        (pRadioLinkParamsNew->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) )
      bUpdateFreq = true;

   if ( bUpdateFreq )     
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         if ( iRadioLink != pModel->radioInterfacesParams.interface_link_id[i] )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         if ( ! hardware_radioindex_supports_frequency(i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]) )
            continue;
         radio_utils_set_interface_frequency(pModel, i, iRadioLink, pRadioLinkParamsNew->link_frequency_khz[iRadioLink], g_pProcessStats, 0);
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]);
      }

      hardware_save_radio_info();
   }

   // Apply data rates

   // If downlink data rate for an Atheros card has changed, update it.
      
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
           (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
         continue;

      int nRateTx = pRadioLinkParamsNew->downlink_datarate_video_bps[iRadioLink];
      update_atheros_card_datarate(pModel, i, nRateTx, g_pProcessStats);
      g_TimeNow = get_current_timestamp_ms();
   }

   // Radio flags are applied on the fly, when sending each radio packet
   s_uRadioLinksStartTime = g_TimeNow = get_current_timestamp_ms();   
   return true;
}

u32 radio_links_get_last_start_time()
{
   return s_uRadioLinksStartTime;
}

bool radio_links_are_marked_for_restart()
{
   return s_bRadioLinksAreMarkedForRestart;
}

// Returns false if radio interfaces can't be configured or none are present
bool radio_links_restart(bool bAsync)
{
   if ( bAsync )
   {
      log_line("[RadioLinks] Marked for restart, async. Currently where marked for restart? %s", s_bRadioLinksAreMarkedForRestart?"yes":"no");
      s_bRadioLinksAreMarkedForRestart = true;
      return true;
   }
   log_line("-----------------------------------");
   log_line("[RadioLinks] Restart radio links...");
   s_bRadioLinksAreMarkedForRestart = false;
   radio_rx_stop_rx_thread();
   radio_links_close_rxtx_radio_interfaces();

   packets_queue_init(&g_QueueRadioPacketsOut);
   packets_queue_init(&s_QueueControlPackets);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      memset(&g_UplinkInfoRxStats[i], 0, sizeof(type_uplink_rx_info_stats));
      g_UplinkInfoRxStats[i].lastReceivedDBM = 1000;
      g_UplinkInfoRxStats[i].lastReceivedDBMNoise = 1000;
      g_UplinkInfoRxStats[i].lastReceivedSNR = 1000;
      g_UplinkInfoRxStats[i].lastReceivedDataRate = 0;
      g_UplinkInfoRxStats[i].timeLastLogWrongRxPacket = 0;
   }
   relay_init_and_set_rx_info_stats(&(g_UplinkInfoRxStats[0]));

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   if ( NULL == g_pCurrentModel )
   {
      log_error_and_alarm("[RadioLinks] Current model is NULL.");
      return false;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("[RadioLinks] No radio interfaces present.");
      return false;
   }

   configure_radio_interfaces_for_current_model(g_pCurrentModel, &g_SM_RadioStats, g_pProcessStats);
   
   radio_duplicate_detection_remove_data_for_all_except(g_uControllerId);

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX )
      radio_set_use_pcap_for_tx(1);
   else
      radio_set_use_pcap_for_tx(0);

   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS )
      radio_set_bypass_socket_buffers(1);
   else
      radio_set_bypass_socket_buffers(0);

   int iDevMode = (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?1:0;
   radio_tx_set_dev_mode(iDevMode);
   radio_rx_set_dev_mode(iDevMode);
   radio_set_debug_flag(iDevMode);

   radio_links_open_rxtx_radio_interfaces();
   radio_rx_start_rx_thread(&g_SM_RadioStats, 0, g_pCurrentModel->getVehicleFirmwareType());
    
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   log_line("[RadioLinks] Restart radio links: completed.");
   log_line("--------------------------------------------");

   static bool s_bFirstRadioStart = true;

   if ( ! s_bFirstRadioStart )
   {
      log_line("[RadioLinks] Notify other processes to reload model");
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_GENERIC<<8);
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
               
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
   s_bFirstRadioStart = false;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

   return true;
}
