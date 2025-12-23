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

#include "hw_config_check.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../radio/radiolink.h"
#include "../common/string_utils.h"

static bool s_bHWCheckDefault24Used = false;
static bool s_bHWCheckDefault58Used = false;
   

void log_full_current_radio_configuration(Model* pModel)
{
   log_line("[HW Radio Check] Current hardware radio interfaces present: %d", hardware_get_radio_interfaces_count() );
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      log_line("* Radio HW Interface %d: MAC: %s, name: %s, driver str: %s, driver id: %d (%s), radio type: %d (%s), card model: %s",
         i+1, pRadioInfo->szMAC, pRadioInfo->szName, pRadioInfo->szDriver, 
         pRadioInfo->iRadioDriver, str_get_radio_driver_description(pRadioInfo->iRadioDriver),
         pRadioInfo->iRadioType, str_get_radio_type_description(pRadioInfo->iRadioType),
         str_get_radio_card_model_string(pRadioInfo->iCardModel));
   }

   if ( NULL != pModel )
      pModel->logVehicleRadioInfo();

}

// Returns true if hardware nics have changed

bool _check_update_hardware_one_interface_after_and_before(Model* pModel)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(0);
   if ( NULL == pRadioHWInfo )
   {
      log_error_and_alarm("Failed to get radio interface info for radio interface 1!");
      log_line("[HW Radio Check] Radio hardware check: Aborted.");
      return false;
   }

   if ( (0 != pModel->radioInterfacesParams.interface_szMAC[0][0]) && (0 != pRadioHWInfo->szMAC[0]) )
   if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[0], pRadioHWInfo->szMAC) )
   {
      log_line("[HW Radio Check] Radio hardware check complete: No change in radio interfaces (same single one present in the sistem before and after)");
      return false;
   }

   log_line("[HW Radio Check] Radio hardware check: The single radio interface has changed in the system. Updating system to the new one.");
   log_line("[HW Radio Check] Radio HW Interface 1: MAC: %s, name: %s, driver str: %s, driver id: %d (%s), radio type: %d (%s), card model: %s",
         pRadioHWInfo->szMAC, pRadioHWInfo->szName, pRadioHWInfo->szDriver, 
         pRadioHWInfo->iRadioDriver, str_get_radio_driver_description(pRadioHWInfo->iRadioDriver),
         pRadioHWInfo->iRadioType, str_get_radio_type_description(pRadioHWInfo->iRadioType),
         str_get_radio_card_model_string(pRadioHWInfo->iCardModel));

   type_radio_interfaces_parameters currentRadioInterfacesParams;
   type_radio_links_parameters currentRadioLinksParams;
   memcpy(&currentRadioLinksParams, &(pModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&currentRadioInterfacesParams, &(pModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));

   pModel->radioInterfacesParams.interface_supported_bands[0] = pRadioHWInfo->supportedBands;
   pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = (pRadioHWInfo->iRadioType & 0xFF) | ((pRadioHWInfo->iRadioDriver << 8) & 0xFF00);
   if ( pRadioHWInfo->isSupported )
      pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = pModel->radioInterfacesParams.interface_radiotype_and_driver[0] | 0xFF0000;
   strncpy(pModel->radioInterfacesParams.interface_szMAC[0], pRadioHWInfo->szMAC, MAX_MAC_LENGTH-1 );
   pModel->radioInterfacesParams.interface_szMAC[0][MAX_MAC_LENGTH-1] = 0;
   strncpy(pModel->radioInterfacesParams.interface_szPort[0], pRadioHWInfo->szUSBPort, MAX_RADIO_PORT_NAME_LENGTH-1);
   pModel->radioInterfacesParams.interface_szPort[0][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;

   if ( ! hardware_radio_supports_frequency(pRadioHWInfo, pModel->radioLinksParams.link_frequency_khz[0]) )
   {
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_433 )
            pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY_433;
         if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_868 )
            pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY_868;
         if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_915 )
            pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY_915;
         pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] &= ~(RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
         pModel->radioLinksParams.downlink_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_SIK_AIR;
         pModel->radioLinksParams.downlink_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_SIK_AIR;
         pModel->radioLinksParams.uplink_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_SIK_AIR;
         pModel->radioLinksParams.uplink_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      }
      else if ( pRadioHWInfo->isSerialRadio )
      {
         pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] &= ~(RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
         pModel->radioLinksParams.link_capabilities_flags[0] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         if ( pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
            pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;
         pModel->radioLinksParams.downlink_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
         pModel->radioLinksParams.downlink_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
         pModel->radioLinksParams.uplink_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
         pModel->radioLinksParams.uplink_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;

         pModel->radioLinksParams.link_frequency_khz[0] = pRadioHWInfo->uCurrentFrequencyKhz;
      }
      else
      {
         pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY;
         if ( pModel->radioInterfacesParams.interface_supported_bands[0] & RADIO_HW_SUPPORTED_BAND_58 )
            pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY58;

         pModel->resetRadioLinkDataRatesAndFlags(0);
         pModel->setVideoProfilesDefaultVideoBitrates();
      }
   }
   else
   {
      if ( hardware_radio_index_is_wifi_radio(0) )
      {
         pModel->resetRadioLinkDataRatesAndFlags(0);
         pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         pModel->radioInterfacesParams.interface_capabilities_flags[0] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         pModel->radioLinksParams.link_capabilities_flags[0] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      
         pModel->setVideoProfilesDefaultVideoBitrates();
      }
   }
   // Populate radio interfaces radio flags and rates from radio links radio flags and rates

   pModel->radioInterfacesParams.interface_current_frequency_khz[0] = pModel->radioLinksParams.link_frequency_khz[0];
   pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];
   pModel->radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_TX_POWER;

   log_line("[HW Radio Check] Radio hardware check: Updated radio links based on current hardware radio interfaces. Completed.");
   return true;
}

bool _check_update_hardware_one_interface_after_multiple_before(Model* pModel)
{
   log_line("[HW Radio Check] Radio hardware check: Radio interfaces where removed, only one remaining.");
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(0);
   if ( NULL == pRadioInfo )
   {
      log_error_and_alarm("Failed to get radio interface info for radio interface 1!");
      log_line("[HW Radio Check] Radio hardware check: Aborted.");
      return false;
   }

   // See if the interface was present in the system before

   int iRadioInterfaceIndex = -1;
   int iRadioLinkIndex = -1;

   for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
   {
      if ( (0 != pModel->radioInterfacesParams.interface_szMAC[k][0]) && (0 != pRadioInfo->szMAC[0]) )
      if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[k], pRadioInfo->szMAC) )
      {
         iRadioInterfaceIndex = k;
         break;
      }
   }

   if ( iRadioInterfaceIndex == -1 )
   {
      // It's a new radio interface.
      // Try to assign an existing radio link to this new interface. If not possible, just reset to the default radio link params.

      for( int k=0; k<pModel->radioLinksParams.links_count; k++ )
      {
         if ( hardware_radio_supports_frequency(pRadioInfo, pModel->radioLinksParams.link_frequency_khz[k]) )
         {
            iRadioLinkIndex = k;
            break;
         }
      }

      if ( -1 == iRadioLinkIndex )
      {
         log_line("[HW Radio Check] Could not reasign an existing radio link to the new radio interface in the system. Reseting radio links and radio interfaces to default.");
         pModel->radioInterfacesParams.interfaces_count = 1;
         pModel->populateRadioInterfacesInfoFromHardware();
         return true;
      }

      // Remove all radio links except the one we are keeping
      // Move it to first in the list of radio links and make sure it can be used for data and video

      pModel->copy_radio_link_params(iRadioLinkIndex, 0);
      
      pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      pModel->radioLinksParams.links_count = 1;

      // Update the radio interface to match the assigned radio link

      pModel->radioInterfacesParams.interface_card_model[0] = pRadioInfo->iCardModel;
      pModel->radioInterfacesParams.interface_link_id[0] = 0;
      pModel->radioInterfacesParams.interface_current_frequency_khz[0] = pModel->radioLinksParams.link_frequency_khz[0];
      pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];
      pModel->radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_TX_POWER;

      pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = (pRadioInfo->iRadioType & 0xFF) | ((pRadioInfo->iRadioDriver << 8) & 0xFF00);
      pModel->radioInterfacesParams.interface_supported_bands[0] = pRadioInfo->supportedBands;
      if ( pRadioInfo->isSupported )
         pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = pModel->radioInterfacesParams.interface_radiotype_and_driver[0] | 0xFF0000;
      
      strncpy(pModel->radioInterfacesParams.interface_szMAC[0], pRadioInfo->szMAC, MAX_MAC_LENGTH-1 );
      pModel->radioInterfacesParams.interface_szMAC[0][MAX_MAC_LENGTH-1] = 0;
      strncpy(pModel->radioInterfacesParams.interface_szPort[0], pRadioInfo->szUSBPort, MAX_RADIO_PORT_NAME_LENGTH-1);
      pModel->radioInterfacesParams.interface_szPort[0][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;

      pModel->radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      pModel->radioInterfacesParams.interfaces_count = 1;

      pModel->resetRadioLinkDataRatesAndFlags(0);
      if ( hardware_radio_index_is_wifi_radio(0) )
      {
         pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         pModel->radioInterfacesParams.interface_capabilities_flags[0] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         pModel->radioLinksParams.link_capabilities_flags[0] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      }
      pModel->setVideoProfilesDefaultVideoBitrates();
      log_line("[HW Radio Check] Radio hardware check: Updated radio link and radio interface based on current hardware radio interface. Completed.");
      return true;
   }


   // The remaining radio interface was present in the system before,
   // Just remove the radio links that do not have a radio interface anymore;

   iRadioLinkIndex = pModel->radioInterfacesParams.interface_link_id[iRadioInterfaceIndex];

   // Remove all radio links except the one we are keeping
   // Move it to first in the list of radio links and make sure it can be used for data and video

   pModel->copy_radio_link_params(iRadioLinkIndex, 0);
 
   pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   pModel->radioLinksParams.links_count = 1;

   // Update the radio interface to match the assigned radio link

   pModel->radioInterfacesParams.interface_card_model[0] = pRadioInfo->iCardModel;
   pModel->radioInterfacesParams.interface_link_id[0] = 0;
   pModel->radioInterfacesParams.interface_current_frequency_khz[0] = pModel->radioLinksParams.link_frequency_khz[0];
   pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];

   pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = (pRadioInfo->iRadioType & 0xFF) | ((pRadioInfo->iRadioDriver << 8) & 0xFF00);
   pModel->radioInterfacesParams.interface_supported_bands[0] = pRadioInfo->supportedBands;
   if ( pRadioInfo->isSupported )
      pModel->radioInterfacesParams.interface_radiotype_and_driver[0] = pModel->radioInterfacesParams.interface_radiotype_and_driver[0] | 0xFF0000;
      
   strncpy(pModel->radioInterfacesParams.interface_szMAC[0], pRadioInfo->szMAC, MAX_MAC_LENGTH-1 );
   pModel->radioInterfacesParams.interface_szMAC[0][MAX_MAC_LENGTH-1] = 0;
   strncpy(pModel->radioInterfacesParams.interface_szPort[0], pRadioInfo->szUSBPort, MAX_RADIO_PORT_NAME_LENGTH-1);
   pModel->radioInterfacesParams.interface_szPort[0][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;

   pModel->radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   pModel->radioInterfacesParams.interfaces_count = 1;

   pModel->validateRadioSettings();
   log_line("[HW Radio Check] Radio hardware check: Updated radio links based on current hardware radio interfaces. Completed.");
   return true;
}

// Radio links are added from here, on vehicle start or from model populateDefaultRadioLinksInfoFromRadioInterfaces()

void _add_new_radio_link_for_hw_radio_interface(int iInterfaceIndex, Model* pModel)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("[HW Radio Check] Failed to add a new radio link in the current model for HW radio interface %d. Can't get radio info.", iInterfaceIndex+1);
      return;
   }
   
   log_line("[HW Radio Check] Adding a new radio link in the current model for hardware radio interface %d (%s) [%s]...", iInterfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->szMAC);
   
   if ( pModel->radioInterfacesParams.interface_link_id[iInterfaceIndex] != -1 )
   {
      log_softerror_and_alarm("[HW Radio Check] Tried to add a new radio link to radio interface %d which is already assigned to radio link %d.",
         iInterfaceIndex+1, pModel->radioInterfacesParams.interface_link_id[iInterfaceIndex]+1);
      return;
   }

   // Add a new radio link

   int iRadioLink = pModel->radioLinksParams.links_count;
   
   pModel->radioLinksParams.link_frequency_khz[iRadioLink] = 0;
   pModel->resetRadioLinkDataRatesAndFlags(iRadioLink);
   if ( hardware_radio_index_is_wifi_radio(iInterfaceIndex) )
   {
      pModel->radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      pModel->radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
   }
   pModel->setVideoProfilesDefaultVideoBitrates();

   if ( 0 == iRadioLink )
   {
      log_line("[HW Radio Check] Make sure radio link 1 (first one in the model) has all required capabilities flags. Enable them on radio link 1.");
      pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
         pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   }

   if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
   {
      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_433 )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY_433;
      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_868 )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY_868;
      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_915 )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY_915;

      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] &= ~(RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
      pModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      pModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      pModel->radioLinksParams.uplink_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      pModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SIK_AIR;
   }
   else if ( pRadioHWInfo->isSerialRadio )
   {
      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] &= ~(RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      pModel->radioLinksParams.link_capabilities_flags[iRadioLink] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      if ( pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
         pModel->radioLinksParams.link_capabilities_flags[iRadioLink] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;

      pModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      pModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      pModel->radioLinksParams.uplink_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      pModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;

      pModel->radioLinksParams.link_frequency_khz[iRadioLink] = pRadioHWInfo->uCurrentFrequencyKhz;
   }


   // Assign the radio link to the radio interface

   pModel->radioInterfacesParams.interface_link_id[iInterfaceIndex] = iRadioLink;
   pModel->radioInterfacesParams.interface_current_radio_flags[iInterfaceIndex] = pModel->radioLinksParams.link_radio_flags[iRadioLink];
   
   // Assign a frequency to the new radio link and the radio interface

   //if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
   //if ( ! pRadioHWInfo->isSerialRadio )
   if ( hardware_radio_is_wifi_radio(pRadioHWInfo) )
   {
      log_line("[HW Radio Check] Assign a frequency to the new radio link...");
      pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY;
      if ( pModel->radioInterfacesParams.interface_supported_bands[iInterfaceIndex] & RADIO_HW_SUPPORTED_BAND_58 )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY58;
         
      if ( s_bHWCheckDefault58Used && pModel->radioLinksParams.link_frequency_khz[iRadioLink] == DEFAULT_FREQUENCY58 )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY58_2;
      if ( s_bHWCheckDefault24Used && pModel->radioLinksParams.link_frequency_khz[iRadioLink] == DEFAULT_FREQUENCY )
         pModel->radioLinksParams.link_frequency_khz[iRadioLink] = DEFAULT_FREQUENCY_2;
      
      pModel->radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex] = pModel->radioLinksParams.link_frequency_khz[iRadioLink];
   }
   else
      log_line("[HW Radio Check] New radio link is serial. Use adapter frequency for the radio link frequency.");

   pModel->radioLinksParams.links_count++;
   log_line("[HW Radio Check] Added a new model radio link (radio link %d) for hardware radio interface %d [%s], on %s", 
      pModel->radioLinksParams.links_count,
      iInterfaceIndex+1, pRadioHWInfo->szMAC, str_format_frequency(pModel->radioLinksParams.link_frequency_khz[pModel->radioLinksParams.links_count-1]));
}

bool recheck_disabled_radio_interfaces(Model* pModel)
{
   log_line("[HW Radio Check] Recheck disabled radio interfaces...");
   bool bUpdated = false;
   if ( 1 == hardware_get_supported_radio_interfaces_count() )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         if ( pRadioHWInfo->isSupported )
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         {
            pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_DISABLED;
            bUpdated = true;
         }

         if ( pRadioHWInfo->isSupported )
         if ( 0xFF0000 != (pModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF0000) )
         {
            pModel->radioInterfacesParams.interface_radiotype_and_driver[i] |= 0xFF0000;
            bUpdated = true;
         }
      }
   }
   log_line("[HW Radio Check] Done recheck disabled radio interfaces.");
   return bUpdated;
}

bool check_update_hardware_nics_vehicle(Model* pModel)
{
   log_line("[HW Radio Check] Checking for radio interfaces hardware changes...");
   if ( NULL == pModel )
   {
      log_error_and_alarm("[HW Radio Check] Checking forRadio hardware change failed: No model.");
      return false;
   }

   // Remove invalid radio links (no radio interface or frequency not supported by assigned radio interfaces)
   bool bAnyLinkRemoved = false;

   if ( pModel->radioLinksParams.links_count > 1 )
   for( int iRadioLink=0; iRadioLink<pModel->radioLinksParams.links_count; iRadioLink++ )
   {
      bool bLinkOk = true;
      bool bAssignedCard = false;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[k] != iRadioLink )
            continue;
         bAssignedCard = true;
         if ( ! (pModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS) )
         if ( ! isFrequencyInBands(pModel->radioLinksParams.link_frequency_khz[iRadioLink], pModel->radioInterfacesParams.interface_supported_bands[k]) )
         {
            bLinkOk = false;
            break;
         }
      }
      if ( bAssignedCard && bLinkOk )
         continue;

      // Remove this radio link

      log_line("[HW Radio Check] Remove model's radio link %d as it has no valid model card assigned to it.", iRadioLink+1);

      // Unassign any cards that still use this radio link

      for( int card=0; card<pModel->radioInterfacesParams.interfaces_count; card++ )
         if ( pModel->radioInterfacesParams.interface_link_id[card] == iRadioLink )
            pModel->radioInterfacesParams.interface_link_id[card] = -1;

      for( int k=iRadioLink; k<pModel->radioLinksParams.links_count-1; k++ )
      {
         pModel->copy_radio_link_params(k+1, k);
         
         for( int card=0; card<pModel->radioInterfacesParams.interfaces_count; card++ )
            if ( pModel->radioInterfacesParams.interface_link_id[card] == k+1 )
               pModel->radioInterfacesParams.interface_link_id[card] = k;
      }
      pModel->radioLinksParams.links_count--;
      bAnyLinkRemoved = true;
   }

   /*
   if ( bAnyLinkRemoved )
   {
      log_line("[HW Radio Check] There where invalid radio links. Repopulate all model's radio interfaces and radio links from current hardware.");
      pModel->populateRadioInterfacesInfoFromHardware();
      log_line("[HW Radio Check] Done checking for Radio interfaces hardware changes by repopulating model's radio interfaces and model's radio links from current hardware.");
      return true;
   }
   */

   int iRadioHWInterfacesCount = hardware_get_radio_interfaces_count();

   // Handle the case with a single radio interface before and after ( 1 interface before and 1 interface now)

   if ( 1 == pModel->radioLinksParams.links_count )
   if ( 1 == iRadioHWInterfacesCount && 1 == pModel->radioInterfacesParams.interfaces_count )
      return _check_update_hardware_one_interface_after_and_before(pModel);

   // Handle the case with a single remaining radio interface ( multiple interfaces before and only one interface now)

   if ( pModel->radioLinksParams.links_count > 1 )
   if ( 1 == iRadioHWInterfacesCount && pModel->radioInterfacesParams.interfaces_count > 1 )
      return _check_update_hardware_one_interface_after_multiple_before(pModel);

   // Handle the case with multiple radio interfaces

   int iOriginalModelInterfacesCount = pModel->radioInterfacesParams.interfaces_count;

   int iCountSameInterfaces = 0;
   int iCountMovedInterfaces = 0;
   bool bIsExistingInterface[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      bIsExistingInterface[i] = false;

   // Find unchanged interfaces, and move them in the currently assigned hardware slot index;
   log_line("[HW Radio Check] Try to match current hardware radio interfaces (%d interfaces) to existing model radio interfaces (%d interfaces)...", iRadioHWInterfacesCount, pModel->radioInterfacesParams.interfaces_count);

   for( int i=0; i<iRadioHWInterfacesCount; i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      bool bMatched = false;

      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( (0 != pModel->radioInterfacesParams.interface_szMAC[k][0]) && (0 != pRadioInfo->szMAC[0]) )
         if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[k], pRadioInfo->szMAC) )
         {
            bMatched = true;
            iCountSameInterfaces++;
            bIsExistingInterface[i] = true;
            if ( pModel->radioInterfacesParams.interface_current_frequency_khz[k] == DEFAULT_FREQUENCY )
               s_bHWCheckDefault24Used = true;
            if ( pModel->radioInterfacesParams.interface_current_frequency_khz[k] == DEFAULT_FREQUENCY58 )
               s_bHWCheckDefault58Used = true;

            if ( k == i )
               log_line("[HW Radio Check] Found HW radio interface %s on hardware position %d same as on model radio interface position %d. Same position, no change in model radio interface order for this radio interface.", pRadioInfo->szMAC, i+1, k+1);
            if ( k != i )
            {
               log_line("[HW Radio Check] Found HW radio interface %s on hardware position %d and on model radio interface position %d.", pRadioInfo->szMAC, i+1, k+1);
               log_line("[HW Radio Check] Moving existing model radio interface %s (%s, now at %s) from slot index %d to slot index %d", pRadioInfo->szMAC, str_get_radio_driver_description((pModel->radioInterfacesParams.interface_radiotype_and_driver[k]>>8) & 0xFF), str_format_frequency(pModel->radioInterfacesParams.interface_current_frequency_khz[k]), k+1, i+1);
               iCountMovedInterfaces++;
               int tmp;
               u32 u;
               char szTmp[MAX_MAC_LENGTH+1];

               tmp = pModel->radioInterfacesParams.interface_card_model[i];
               pModel->radioInterfacesParams.interface_card_model[i] = pModel->radioInterfacesParams.interface_card_model[k];
               pModel->radioInterfacesParams.interface_card_model[k] = tmp;

               tmp = pModel->radioInterfacesParams.interface_link_id[i];
               pModel->radioInterfacesParams.interface_link_id[i] = pModel->radioInterfacesParams.interface_link_id[k];
               pModel->radioInterfacesParams.interface_link_id[k] = tmp;

               tmp = pModel->radioInterfacesParams.interface_raw_power[i];
               pModel->radioInterfacesParams.interface_raw_power[i] = pModel->radioInterfacesParams.interface_raw_power[k];
               pModel->radioInterfacesParams.interface_raw_power[k] = tmp;

               u = pModel->radioInterfacesParams.interface_radiotype_and_driver[i];
               pModel->radioInterfacesParams.interface_radiotype_and_driver[i] = pModel->radioInterfacesParams.interface_radiotype_and_driver[k];
               pModel->radioInterfacesParams.interface_radiotype_and_driver[k] = u;

               u = pModel->radioInterfacesParams.interface_supported_bands[i];
               pModel->radioInterfacesParams.interface_supported_bands[i] = pModel->radioInterfacesParams.interface_supported_bands[k];
               pModel->radioInterfacesParams.interface_supported_bands[k] = u;

               strcpy( szTmp, pModel->radioInterfacesParams.interface_szMAC[i]);
               strncpy( pModel->radioInterfacesParams.interface_szMAC[i], pModel->radioInterfacesParams.interface_szMAC[k], MAX_MAC_LENGTH-1);
               pModel->radioInterfacesParams.interface_szMAC[i][MAX_MAC_LENGTH-1] = 0;
               strncpy( pModel->radioInterfacesParams.interface_szMAC[k], szTmp, MAX_MAC_LENGTH-1);
               pModel->radioInterfacesParams.interface_szMAC[k][MAX_MAC_LENGTH-1] = 0;

               strcpy( szTmp, pModel->radioInterfacesParams.interface_szPort[i]);
               strncpy( pModel->radioInterfacesParams.interface_szPort[i], pModel->radioInterfacesParams.interface_szPort[k], MAX_RADIO_PORT_NAME_LENGTH-1);
               pModel->radioInterfacesParams.interface_szPort[i][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;
               strncpy( pModel->radioInterfacesParams.interface_szPort[k], szTmp, MAX_RADIO_PORT_NAME_LENGTH-1);
               pModel->radioInterfacesParams.interface_szPort[k][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;

               u = pModel->radioInterfacesParams.interface_capabilities_flags[i];
               pModel->radioInterfacesParams.interface_capabilities_flags[i] = pModel->radioInterfacesParams.interface_capabilities_flags[k];
               pModel->radioInterfacesParams.interface_capabilities_flags[k] = u;

               u = pModel->radioInterfacesParams.interface_current_frequency_khz[i];
               pModel->radioInterfacesParams.interface_current_frequency_khz[i] = pModel->radioInterfacesParams.interface_current_frequency_khz[k];
               pModel->radioInterfacesParams.interface_current_frequency_khz[k] = u;

               u = pModel->radioInterfacesParams.interface_current_radio_flags[i];
               pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioInterfacesParams.interface_current_radio_flags[k];
               pModel->radioInterfacesParams.interface_current_radio_flags[k] = u;

               tmp = pModel->radioInterfacesParams.interface_raw_power[i];
               pModel->radioInterfacesParams.interface_raw_power[i] = pModel->radioInterfacesParams.interface_raw_power[k];
               pModel->radioInterfacesParams.interface_raw_power[k] = tmp;
            }
            break;
         }
      } 
      if ( ! bMatched )
         log_line("[HW Radio Check] Hardware radio interface [%s] (on position %d) was not mached to any existing model radio interfaces.", pRadioInfo->szMAC, i+1);
   }

   log_line("[HW Radio Check] Done matching current hardware radio interfaces (%d interfaces) to existing model radio interfaces (%d interfaces): %d hardware radio interfaces where matched (of which %d where moved in position).", iRadioHWInterfacesCount, pModel->radioInterfacesParams.interfaces_count, iCountSameInterfaces, iCountMovedInterfaces);

   if ( 0 < iCountSameInterfaces )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( ! bIsExistingInterface[i] )
            continue;
         radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioInfo )
            continue;

         if ( pModel->radioInterfacesParams.interface_link_id[i] < 0 )
            log_line("[HW Radio Check] HW radio interface %d, %s, %s is mirrored on model radio interface position %d. Not used on any radio link.", i+1, pRadioInfo->szName, pRadioInfo->szMAC, i+1);
         else
            log_line("[HW Radio Check] HW radio interface %d, %s, %s is mirrored on model radio interface position %d. Used on radio link %d", i+1, pRadioInfo->szName, pRadioInfo->szMAC, i+1, pModel->radioInterfacesParams.interface_link_id[i]+1);
      }
   }

   for( int i=0; i<iRadioHWInterfacesCount; i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         log_softerror_and_alarm("[HW Radio Check] Failed to get hardware radio interface %d info.", i+1);
         continue;
      }
      if ( bIsExistingInterface[i] )
         continue;

      // Populate new radio interface info

      log_line("[HW Radio Check] Add new radio interface to model's radio interfaces: %s, hardware index: %d, model interface index: %d", pRadioInfo->szName, i+1, i+1);
      pModel->radioInterfacesParams.interface_card_model[i] = pRadioInfo->iCardModel;
      pModel->radioInterfacesParams.interface_current_frequency_khz[i] = 0;
      pModel->radioInterfacesParams.interface_current_radio_flags[i] = 0;
      pModel->radioInterfacesParams.interface_raw_power[i] = DEFAULT_RADIO_TX_POWER;

      pModel->radioInterfacesParams.interface_radiotype_and_driver[i] = (pRadioInfo->iRadioType & 0xFF) | ((pRadioInfo->iRadioDriver << 8) & 0xFF00);
      pModel->radioInterfacesParams.interface_supported_bands[i] = pRadioInfo->supportedBands;
      if ( pRadioInfo->isSupported )
         pModel->radioInterfacesParams.interface_radiotype_and_driver[i] |= 0xFF0000;

      strncpy(pModel->radioInterfacesParams.interface_szMAC[i], pRadioInfo->szMAC, MAX_MAC_LENGTH-1 );
      pModel->radioInterfacesParams.interface_szMAC[i][MAX_MAC_LENGTH-1] = 0;
      strncpy(pModel->radioInterfacesParams.interface_szPort[i], pRadioInfo->szUSBPort, MAX_RADIO_PORT_NAME_LENGTH-1);
      pModel->radioInterfacesParams.interface_szPort[i][MAX_RADIO_PORT_NAME_LENGTH-1] = 0;

      pModel->radioInterfacesParams.interface_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      if ( hardware_radio_is_sik_radio(pRadioInfo) )
      {
         pModel->radioInterfacesParams.interface_current_frequency_khz[i] = pRadioInfo->uCurrentFrequencyKhz;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_SIK;
      }
      else if ( pRadioInfo->isSerialRadio )
      {
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         if ( pRadioInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
            pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;
         
         pModel->radioInterfacesParams.interface_current_frequency_khz[i] = pRadioInfo->uCurrentFrequencyKhz;
      }

      pModel->radioInterfacesParams.interface_link_id[i] = -1;
   }

   pModel->radioInterfacesParams.interfaces_count = iRadioHWInterfacesCount;

   // Now model has same radio interfaces as hardware radio interfaces

   log_line("[HW Radio Check] Now all the hardware radio interfaces are mirrored to the model's radio interfaces (%d interfaces):", iRadioHWInterfacesCount);
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         log_softerror_and_alarm("[HW Radio Check] Failed to get hardware radio interface %d info.", i+1);
         continue;
      }
      if ( pModel->radioInterfacesParams.interface_link_id[i] < 0 )
         log_line("[HW Radio Check] *  Model radio interface %d, %s (not used on any radio link) is mirrored from hardware radio interface %d %s, %s;",
            i+1, pModel->radioInterfacesParams.interface_szMAC[i],
            i+1, pRadioInfo->szMAC, pRadioInfo->szName);
      else
         log_line("[HW Radio Check] *  Model radio interface %d, %s (radio link %d) is mirrored from hardware radio interface %d %s, %s;",
            i+1, pModel->radioInterfacesParams.interface_szMAC[i], pModel->radioInterfacesParams.interface_link_id[i]+1,
            i+1, pRadioInfo->szMAC, pRadioInfo->szName);
   }

   // Remove unused radio links

   bAnyLinkRemoved = false;
   for( int iRadioLink=0; iRadioLink<pModel->radioLinksParams.links_count; iRadioLink++ )
   {
      bool bLinkOk = true;
      bool bAssignedCard = false;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[k] != iRadioLink )
            continue;
         bAssignedCard = true;
         if ( ! (pModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS) )
         if ( ! isFrequencyInBands(pModel->radioLinksParams.link_frequency_khz[iRadioLink], pModel->radioInterfacesParams.interface_supported_bands[k]) )
         {
            bLinkOk = false;
            break;
         }
      }
      if ( bAssignedCard && bLinkOk )
         continue;

      // Remove this radio link

      log_line("[HW Radio Check] Remove model's radio link %d as it has no valid model radio interface assigned to it.", iRadioLink+1);

      // Unassign any cards that still use this radio link

      for( int card=0; card<pModel->radioInterfacesParams.interfaces_count; card++ )
         if ( pModel->radioInterfacesParams.interface_link_id[card] == iRadioLink )
            pModel->radioInterfacesParams.interface_link_id[card] = -1;

      for( int k=iRadioLink; k<pModel->radioLinksParams.links_count-1; k++ )
      {
         pModel->copy_radio_link_params(k+1, k);
         
         for( int card=0; card<pModel->radioInterfacesParams.interfaces_count; card++ )
            if ( pModel->radioInterfacesParams.interface_link_id[card] == k+1 )
               pModel->radioInterfacesParams.interface_link_id[card] = k;
      }
      pModel->radioLinksParams.links_count--;
      bAnyLinkRemoved = true;
   }

   if ( bAnyLinkRemoved )
   {
      log_line("[HW Radio Check] Removed invalid radio links. This is the current vehicle configuration after adding all the radio interfaces and removing any invalid radio links:");
      log_full_current_radio_configuration(pModel);
   }
   else
      log_line("[HW Radio Check] No invalid radio links where removed from model configuration.");


   // Case: No change in radio interfaces, all are the same, maybe different order

   if ( iCountSameInterfaces == iRadioHWInterfacesCount )
   {
      if ( iCountMovedInterfaces > 0 )
      {
         log_line("[HW Radio Check] Same radio interfaces are present on the system. Updated only positions for %d radio interfaces.", iCountMovedInterfaces);
         return true;
      }
      log_line("[HW Radio Check] Same radio interfaces are present on the system on the same positions, no change detected or required.");
      return false;
   }

   // Case: Only added new radio interfaces

   if ( iCountSameInterfaces == iOriginalModelInterfacesCount )
   if ( iOriginalModelInterfacesCount < iRadioHWInterfacesCount )
   {
      log_line("[HW Radio Check] Detected only new radio interfaces added (%d added). All existing ones where present.", iRadioHWInterfacesCount - iCountSameInterfaces);
      log_line("[HW Radio Check] %d existing radio interfaces (same as old ones), %d new radio interfaces.", iCountSameInterfaces, iRadioHWInterfacesCount - iCountSameInterfaces );

      // Add new radio links for all new radio interfaces

      for( int i=0; i<iRadioHWInterfacesCount; i++ )
      {
         if ( bIsExistingInterface[i] )
            continue;
         radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioInfo )
            continue;

         _add_new_radio_link_for_hw_radio_interface(i, pModel);
      }
      return true;
   }

   // Case: Only removed radio interfaces, no new ones

   if ( iOriginalModelInterfacesCount > iRadioHWInterfacesCount )
   {
      log_line("[HW Radio Check] Radio interfaces where only removed. All other radio interfaces are the same. No new radio interfaces detected.");
      return true;
   }

   // Case: mix of new and removed radio interfaces

   log_line("[HW Radio Check] Some radio interfaces where removed and some new ones where added. Adding radio links for new radio interfaces...");
   
   // Create radio links for the new cards

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( pModel->radioInterfacesParams.interface_link_id[i] >= 0 )
      {
         log_line("[HW Radio Check] Radio interface %d is already assigned to radio link %d. No new radio link needed for it.", i+1, pModel->radioInterfacesParams.interface_link_id[i]);
         continue;
      }
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;

      _add_new_radio_link_for_hw_radio_interface(i, pModel);
   }

   return true;
}
