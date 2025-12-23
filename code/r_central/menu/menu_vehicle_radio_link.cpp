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

#include "menu.h"
#include "menu_vehicle_radio_link.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "../../common/models_connect_frequencies.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../osd/osd_common.h"
#include "../../radio/radiolink.h"
#include "../../base/tx_powers.h"

const char* s_szMenuRadio_SingleCard2 = "Note: You can not change the usage and capabilities of the radio link as there is a single radio link on your vehicle.";

MenuVehicleRadioLink::MenuVehicleRadioLink(int iRadioLink)
:Menu(MENU_ID_VEHICLE_RADIO_LINK, L("Vehicle Radio Link Parameters"), NULL)
{
   m_Width = 0.46;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.1;
   m_iVehicleRadioLink = iRadioLink;
   m_iVehicleRadioInterface = -1;
   m_bWaitingConfirmationFromUser = false;
   m_bWaitingVideoChangeConfirmationFromVehicle = false;

   m_IndexFrequency = -1;
   m_IndexUsage = -1;
   m_IndexCapabilities = -1;
   m_IndexDataRatesType = -1;
   m_IndexDataRateVideo = -1;
   m_IndexDataRateDataDownlink = -1;
   m_IndexDataRateDataUplink = -1;
   m_IndexHT = -1;
   m_IndexLDPC = -1;
   m_IndexSGI = -1;
   m_IndexSTBC = -1;
   m_IndexReset = -1; 

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      log_line("Opening menu for relay radio link %d ...", m_iVehicleRadioLink+1);
   else
      log_line("Opening menu for radio link %d ...", m_iVehicleRadioLink+1);

   char szBuff[256];

   sprintf(szBuff, L("Vehicle Radio Link %d Parameters"), m_iVehicleRadioLink+1);
   setTitle(szBuff);

   m_iVehicleRadioInterface = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iVehicleRadioLink);

   if ( -1 == m_iVehicleRadioInterface )
   {
      log_softerror_and_alarm("Invalid radio link. No vehicle radio interfaces assigned to vehicle radio link %d.", m_iVehicleRadioLink+1);
      return;
   }


   m_uControllerSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      m_uControllerSupportedBands |= pRadioHWInfo->supportedBands;
   }
      
   m_SupportedChannelsCount = getSupportedChannels( m_uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[m_iVehicleRadioInterface], 1, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);

   log_line("Opening menu to configure vehicle radio link %d, used by vehicle radio interface %d on the model side, supports %d frequencies.", m_iVehicleRadioLink+1, m_iVehicleRadioInterface+1, m_SupportedChannelsCount);
   addMenuItems();
   log_line("Created radio link menu for vehicle radio link %d", m_iVehicleRadioLink+1);
}

MenuVehicleRadioLink::~MenuVehicleRadioLink()
{
}

void MenuVehicleRadioLink::onShow()
{
   valuesToUI();
   Menu::onShow();
   invalidate();
   log_line("Showed radio link menu for radio link %d", m_iVehicleRadioLink+1);
   addMenuItems();

   if ( !(g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS) )
      addMessageWithTitleAndIcon(0, L("Radio Not Calibrated"), L("You have not run the radio link wizard to compute the actual capabilities of your radio hardware. It's recommended you run the Radio Wizard firt before manually changing radio settings."), g_idIconRadio);
}

void MenuVehicleRadioLink::addMenuItems()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();

   for( int i=0; i<20; i++ )
      m_pItemsSelect[i] = NULL;
   
   m_IndexFrequency = -1;
   m_IndexUsage = -1;
   m_IndexCapabilities = -1;
   m_IndexDataRatesType = -1;
   m_IndexDataRateVideo = -1;
   m_IndexDataRateDataDownlink = -1;
   m_IndexDataRateDataUplink = -1;
   m_IndexMaxLoad = -1;
   m_IndexHT = -1;
   m_IndexLDPC = -1;
   m_IndexSGI = -1;
   m_IndexSTBC = -1;
   m_IndexReset = -1; 

   log_line("MenuVehicleRadioLink: Add items: radio data rates for link %d: vid: %d, data-down: %d, data-up: %d",
      m_iVehicleRadioLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink],
      g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink],
      g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink]);

   m_SupportedChannelsCount = getSupportedChannels( m_uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[m_iVehicleRadioInterface], 1, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);
   log_line("MenuVehicleRadioLink: Add items: Vehicle radio link %d, used by vehicle radio interface %d on the model side, supports %d frequencies.", m_iVehicleRadioLink+1, m_iVehicleRadioInterface+1, m_SupportedChannelsCount);

   char szBuff[256];
   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, m_iVehicleRadioLink) )
      strcpy(szBuff, "This vehicle radio link is not used.");
   else
      sprintf(szBuff, "Vehicle radio interface used for this radio link: %s", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[m_iVehicleRadioInterface]));
   addMenuItem(new MenuItemText(szBuff, false, 0.0));
   addMenuItemFrequencies();
   addMenuItemsCapabilities();
   
   addMenuItemsDataRates();

   addSection(L("Radio Link Flags"));

   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, m_iVehicleRadioLink) )
      addMenuItem(new MenuItemText("This vehicle radio link is not used."));
   else
   {
      m_pItemsSelect[11] = new MenuItemSelect(L("Channel Bandwidth"), "Sets the radio channel bandwidth when using MCS data rates.");
      m_pItemsSelect[11]->addSelection("20 Mhz");
      m_pItemsSelect[11]->addSelection("40 Mhz Downlink");
      m_pItemsSelect[11]->addSelection("40 Mhz Uplink");
      m_pItemsSelect[11]->addSelection("40 Mhz Both Directions");
      m_pItemsSelect[11]->setIsEditable();
      m_IndexHT = addMenuItem(m_pItemsSelect[11]);
   }

   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[m_iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
      addMenuItemsMCS();
   m_IndexReset = addMenuItem(new MenuItem(L("Reset To Default"), "Resets this radio link parameters to default values."));

   valuesToUI();
   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuVehicleRadioLink::addMenuItemFrequencies()
{
   char szBuff[128];
   m_pItemsSelect[0] = new MenuItemSelect("Frequency");

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      sprintf(szBuff,"Relay %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
      m_pItemsSelect[0]->addSelection(szBuff);
      m_pItemsSelect[0]->setIsEditable();
      m_IndexFrequency = addMenuItem(m_pItemsSelect[0]);
      return;
   }
   
   Preferences* pP = get_Preferences();
   int iCountChCurrentColumn = 0;
   for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
   {
      if ( (pP->iScaleMenus > 0) && (iCountChCurrentColumn > 14 ) )
      {
         m_pItemsSelect[0]->addSeparator();
         iCountChCurrentColumn = 0;
      }
      if ( m_SupportedChannels[ch] == 0 )
      {
         m_pItemsSelect[0]->addSeparator();
         iCountChCurrentColumn = 0;
         continue;
      }
      strcpy(szBuff, str_format_frequency(m_SupportedChannels[ch]));
      m_pItemsSelect[0]->addSelection(szBuff);
      iCountChCurrentColumn++;
   }

   m_SupportedChannelsCount = getSupportedChannels(m_uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[m_iVehicleRadioInterface], 0, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);
   if ( 0 == m_SupportedChannelsCount )
   {
      sprintf(szBuff,"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iVehicleRadioLink]));
      m_pItemsSelect[0]->addSelection(szBuff);
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
   }

   m_pItemsSelect[0]->setIsEditable();
   m_IndexFrequency = addMenuItem(m_pItemsSelect[0]);

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      m_pItemsSelect[0]->setEnabled(false);
   else
      m_pItemsSelect[0]->setEnabled(true);

   int selectedIndex = 0;
   for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iVehicleRadioLink] == m_SupportedChannels[ch] )
         break;
      selectedIndex++;
   }

   m_pItemsSelect[0]->setSelection(selectedIndex);
}

void MenuVehicleRadioLink::addMenuItemsCapabilities()
{
   m_IndexMaxLoad = -1;
   //if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[7] = new MenuItemSelect(L("Max Link Load"), L("Selects maximum data load on this radio link. Higher values increases the video bitrate capacity but decreases the radio link quality/resilience."));
      m_pItemsSelect[7]->addSelection("10%");
      m_pItemsSelect[7]->addSelection("20%");
      m_pItemsSelect[7]->addSelection("30%");
      m_pItemsSelect[7]->addSelection("40%");
      m_pItemsSelect[7]->addSelection("50%");
      m_pItemsSelect[7]->addSelection("60%");
      m_pItemsSelect[7]->addSelection("70%");
      m_pItemsSelect[7]->addSelection("80%");
      m_pItemsSelect[7]->addSelection("90%");
      m_pItemsSelect[7]->setIsEditable();
      m_IndexMaxLoad = addMenuItem(m_pItemsSelect[7]);
      //m_pMenuItems[m_IndexMaxLoad]->setTextColor(get_Color_Dev());
   }

   if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
   {
      //addMenuItem( new MenuItemText(s_szMenuRadio_SingleCard2, true));
      return;
   }

   u32 linkCapabilitiesFlags = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink];

   m_pItemsSelect[1] = new MenuItemSelect("Link Usage", "Selects what type of data gets sent/received on this radio link, or disable it.");
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("Video & Data");
   m_pItemsSelect[1]->addSelection("Video");
   m_pItemsSelect[1]->addSelection("Data");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexUsage = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Capabilities", "Sets the uplink/downlink capabilities of this radio link. If the associated vehicle radio interface has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, it must be marked to be used only for uplink or downlink accordingly.");  
   m_pItemsSelect[2]->addSelection("Uplink & Downlink");
   m_pItemsSelect[2]->addSelection("Downlink Only");
   m_pItemsSelect[2]->addSelection("Uplink Only");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexCapabilities = addMenuItem(m_pItemsSelect[2]);

   // Link usage

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      m_pItemsSelect[1]->setEnabled(false);
   else
      m_pItemsSelect[1]->setEnabled(true);

   m_pItemsSelect[1]->setSelection(0); // Disabled

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[1]->setSelection(1);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      m_pItemsSelect[1]->setSelection(3);

   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[1]->setSelection(2);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
      m_pItemsSelect[1]->setSelection(1);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      m_pItemsSelect[1]->setSelection(0);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
      m_pItemsSelect[1]->setEnabled(false);

   // Capabilities

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      m_pItemsSelect[2]->setSelection(0);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      m_pItemsSelect[2]->setSelection(2);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      m_pItemsSelect[2]->setSelection(1);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
   {
      m_pItemsSelect[2]->setSelection(0);
      m_pItemsSelect[2]->setEnabled(false);
   }
}

void MenuVehicleRadioLink::addMenuItemsDataRates()
{
   char szBuff[128];
   char szText[128];
   addSection(L("Radio Modulations"));

   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, m_iVehicleRadioLink) )
   {
      addMenuItem(new MenuItemText("This vehicle radio link is not used."));
      return;
   }

   m_pItemsSelect[8] = new MenuItemSelect(L("Radio Modulations"), "Use legacy modulation schemes or or the new Modulation Coding Schemes.");
   m_pItemsSelect[8]->addSelection("Legacy Modulations");
   m_pItemsSelect[8]->addSelection("MCS (Modulation Coding Schemes)");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexDataRatesType = addMenuItem(m_pItemsSelect[8]);

   bool bAddMCSRates = false;

   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[m_iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
      bAddMCSRates = true;

   if ( bAddMCSRates )
      m_pItemsSelect[8]->setSelectedIndex(1);

   m_pItemsSelect[3] = new MenuItemSelect(L("Radio Data Rate"), "Sets the physical radio data rate to use on this radio link for video data. If adaptive radio links is enabled, this will get lowered automatically by Ruby as needed.");
   m_pItemsSelect[3]->addSelection(L("Auto"));
   int iSelectedIndex = 0;
   
   if ( bAddMCSRates )
   {
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         str_getDataRateDescription(-1-i, 0, szBuff);
         if ( (!(g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)) ||
              (m_iVehicleRadioLink != 0) )
            snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s", szBuff);
         else
         {
            if ( i < MODEL_MAX_STORED_QUALITIES_VALUES )
               snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s Q: %.1f%%", szBuff, g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[m_iVehicleRadioLink][i]*100.0);
            else
               snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s Q: N/A", szBuff);
         }
         m_pItemsSelect[3]->addSelection(szText);
         if ( g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink] == (-1-i) )
             iSelectedIndex = i+1;
      }
   }
   else
   {
      for( int i=0; i<getLegacyDataRatesCount(); i++ )
      {
         str_getDataRateDescription(getLegacyDataRatesBPS()[i], 0, szBuff);
         if ( (!(g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)) ||
              (m_iVehicleRadioLink != 0) )
            snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s", szBuff);
         else
         {
            if ( i < MODEL_MAX_STORED_QUALITIES_VALUES )
               snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s Q: %.1f%%", szBuff, g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[m_iVehicleRadioLink][i]*100.0);
            else
               snprintf(szText, sizeof(szText)/sizeof(szText[0]), "%s Q: N/A", szBuff);
         }
         m_pItemsSelect[3]->addSelection(szText);
         if ( g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink] == getLegacyDataRatesBPS()[i] )
            iSelectedIndex = i+1;
      }
   }
   m_pItemsSelect[3]->setIsEditable();
   m_IndexDataRateVideo = addMenuItem(m_pItemsSelect[3]);
   m_pItemsSelect[3]->setSelectedIndex(iSelectedIndex);

   m_pItemsSelect[5] = new MenuItemSelect(L("Telemetry Radio Data Rate (for downlink)"), "Sets the physical radio downlink data rate for data packets (excluding video packets).");
   m_pItemsSelect[5]->addSelection(L("Auto"));
   m_pItemsSelect[5]->addSelection(L("Lowest"));

   iSelectedIndex = 0;
   if ( g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] == 0 )
      iSelectedIndex = 0;
   if ( g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] == -100 )
      iSelectedIndex = 1;

   if ( bAddMCSRates )
   {
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         str_getDataRateDescription(-1-i, 0, szBuff);
         m_pItemsSelect[5]->addSelection(szBuff);
         if ( g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] == (-1-i) )
             iSelectedIndex = i+2;
      }
   }
   else
   {
      for( int i=0; i<getLegacyDataRatesCount(); i++ )
      {
         str_getDataRateDescription(getLegacyDataRatesBPS()[i], 0, szBuff);
         m_pItemsSelect[5]->addSelection(szBuff);
         if ( g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] == getLegacyDataRatesBPS()[i] )
            iSelectedIndex = i+2;
      }
   }
   m_pItemsSelect[5]->setIsEditable();
   m_IndexDataRateDataDownlink = addMenuItem(m_pItemsSelect[5]);
   m_pItemsSelect[5]->setSelectedIndex(iSelectedIndex);

   m_pItemsSelect[6] = new MenuItemSelect("Telemetry Radio Data Rate (for uplink)", "Sets the physical radio uplink data rate for data packets.");  
   m_pItemsSelect[6]->addSelection(L("Auto"));
   m_pItemsSelect[6]->addSelection(L("Lowest"));
   m_pItemsSelect[6]->setIsEditable();

   iSelectedIndex = 0;
   if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] == 0 )
      iSelectedIndex = 0;
   if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] == -100 )
      iSelectedIndex = 1;
   
   if ( bAddMCSRates )
   {
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         str_getDataRateDescription(-1-i, 0, szBuff);
         m_pItemsSelect[6]->addSelection(szBuff);
         if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] == (-1-i) )
             iSelectedIndex= i+2;
      }
   }
   else
   {
      for( int i=0; i<getLegacyDataRatesCount(); i++ )
      {
         str_getDataRateDescription(getLegacyDataRatesBPS()[i], 0, szBuff);
         m_pItemsSelect[6]->addSelection(szBuff);
         if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] == getLegacyDataRatesBPS()[i] )
            iSelectedIndex = i+2;
      }
   }
   m_pItemsSelect[6]->setIsEditable();
   m_IndexDataRateDataUplink = addMenuItem(m_pItemsSelect[6]);
   m_pItemsSelect[6]->setSelectedIndex(iSelectedIndex);
}

void MenuVehicleRadioLink::addMenuItemsMCS()
{
   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, m_iVehicleRadioLink) )
   {
      addMenuItem(new MenuItemText("This vehicle radio link is not used."));
      return;
   }

   u32 linkRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[m_iVehicleRadioLink];

   m_pItemsSelect[14] = new MenuItemSelect(L("Enable STBC"), "Enables STBC when using MCS data rates.");
   m_pItemsSelect[14]->addSelection(L("No"));
   m_pItemsSelect[14]->addSelection(L("Downlink"));
   m_pItemsSelect[14]->addSelection(L("Uplink"));
   m_pItemsSelect[14]->addSelection(L("Both Directions"));
   m_pItemsSelect[14]->setIsEditable();
   m_IndexSTBC = addMenuItem(m_pItemsSelect[14]);

   m_pItemsSelect[12] = new MenuItemSelect(L("Enable LDPC"), "Enables LDPC (Low Density Parity Check) when using MCS data rates.");
   m_pItemsSelect[12]->addSelection(L("No"));
   m_pItemsSelect[12]->addSelection(L("Downlink"));
   m_pItemsSelect[12]->addSelection(L("Uplink"));
   m_pItemsSelect[12]->addSelection(L("Both Directions"));
   m_pItemsSelect[12]->setIsEditable();
   m_IndexLDPC = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[13] = new MenuItemSelect(L("Enable SGI"), "Enables SGI (Short Guard Interval) when using MCS data rates.");
   m_pItemsSelect[13]->addSelection(L("No"));
   m_pItemsSelect[13]->addSelection(L("Downlink"));
   m_pItemsSelect[13]->addSelection(L("Uplink"));
   m_pItemsSelect[13]->addSelection(L("Both Directions"));
   m_pItemsSelect[13]->setIsEditable();
   m_IndexSGI = addMenuItem(m_pItemsSelect[13]);

   if ( (linkRadioFlags & RADIO_FLAG_LDPC_VEHICLE) && (linkRadioFlags & RADIO_FLAG_LDPC_CONTROLLER) )
      m_pItemsSelect[12]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_LDPC_VEHICLE )
      m_pItemsSelect[12]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_LDPC_CONTROLLER )
      m_pItemsSelect[12]->setSelectedIndex(2);
   else
      m_pItemsSelect[12]->setSelectedIndex(0);

   if ( (linkRadioFlags & RADIO_FLAG_SGI_VEHICLE) && (linkRadioFlags & RADIO_FLAG_SGI_CONTROLLER) )
      m_pItemsSelect[13]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_SGI_VEHICLE )
      m_pItemsSelect[13]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_SGI_CONTROLLER )
      m_pItemsSelect[13]->setSelectedIndex(2);
   else
      m_pItemsSelect[13]->setSelectedIndex(0);

   if ( (linkRadioFlags & RADIO_FLAG_STBC_VEHICLE) && (linkRadioFlags & RADIO_FLAG_STBC_CONTROLLER) )
      m_pItemsSelect[14]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_STBC_VEHICLE )
      m_pItemsSelect[14]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_STBC_CONTROLLER )
      m_pItemsSelect[14]->setSelectedIndex(2);
   else
      m_pItemsSelect[14]->setSelectedIndex(0);
}

void MenuVehicleRadioLink::valuesToUI()
{
   u32 linkCapabilitiesFlags = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink];
   u32 linkRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[m_iVehicleRadioLink];
   
   if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) ||
        (linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
   {
      for( int i=3; i<20; i++ )
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(false);
      m_pItemsSelect[0]->setEnabled(false);
   }
   else
   {
      for( int i=3; i<20; i++ )
         if ( NULL != m_pItemsSelect[i] ) 
            m_pItemsSelect[i]->setEnabled(true);
      m_pItemsSelect[0]->setEnabled(true);
   }

   if ( -1 != m_IndexMaxLoad )
      m_pItemsSelect[7]->setSelectedIndex(g_pCurrentModel->radioLinksParams.uMaxLinkLoadPercent[m_iVehicleRadioLink]/10-1);

   if ( -1 != m_IndexHT )
   {
      if ( (linkRadioFlags & RADIO_FLAG_HT40_VEHICLE) && (linkRadioFlags & RADIO_FLAG_HT40_CONTROLLER) )
         m_pItemsSelect[11]->setSelectedIndex(3);
      else if ( linkRadioFlags & RADIO_FLAG_HT40_VEHICLE )
         m_pItemsSelect[11]->setSelectedIndex(1);
      else if ( linkRadioFlags & RADIO_FLAG_HT40_CONTROLLER )
         m_pItemsSelect[11]->setSelectedIndex(2);
      else
         m_pItemsSelect[11]->setSelectedIndex(0);
   }
   //char szBands[128];
   //str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[m_iVehicleRadioInterface], szBands);

   char szCapab[256];
   char szFlags[256];
   str_get_radio_capabilities_description(linkCapabilitiesFlags, szCapab);
   str_get_radio_frame_flags_description(linkRadioFlags, szFlags);
   log_line("MenuVehicleRadioLink: Update UI: Current radio link %d capabilities: %s, flags: %s.", m_iVehicleRadioLink+1, szCapab, szFlags);
}

void MenuVehicleRadioLink::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}

int MenuVehicleRadioLink::convertMCSRateToLegacyRate(int iMCSRate, bool bIsHT40)
{
   int iRealDataRate = getRealDataRateFromMCSRate(iMCSRate, false);
   for( int i=0; i<getLegacyDataRatesCount(); i++ )
   {
      int iLegacyRate = getLegacyDataRatesBPS()[i];
      if ( (iLegacyRate*11)/10 >= iRealDataRate  )
         return iLegacyRate;
   }
   int iMaxLegacyRate = getLegacyDataRatesBPS()[getLegacyDataRatesCount()-1];
   return iMaxLegacyRate;
}

int MenuVehicleRadioLink::convertLegacyRateToMCSRate(int iLegacyRate, bool bIsHT40)
{
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      int iRealDataRate = getRealDataRateFromMCSRate(i, false);
      if ( (iRealDataRate*11)/10 >= iLegacyRate )
         return i;
   }
   return 8;
}

void MenuVehicleRadioLink::sendRadioLinkCapabilities(int iRadioLink)
{
   if ( (-1 == m_IndexUsage) || (-1 == m_IndexCapabilities) )
      return;

   int usage = m_pItemsSelect[1]->getSelectedIndex();
   int capab = m_pItemsSelect[2]->getSelectedIndex();

   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink];

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
   
   if ( 0 == usage ) link_capabilities = link_capabilities | RADIO_HW_CAPABILITY_FLAG_DISABLED;
   if ( 1 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   if ( 2 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   if ( 3 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

   if ( 0 == capab ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX)) | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   if ( 2 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX)) | RADIO_HW_CAPABILITY_FLAG_CAN_RX;

   char szBuffC1[128];
   char szBuffC2[128];
   str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink], szBuffC1);
   str_get_radio_capabilities_description(link_capabilities, szBuffC2);

   log_line("MenuVehicleRadioLink: On update link capabilities:");
   log_line("MenuVehicleRadioLink: Radio link current capabilities: %s", szBuffC1);
   log_line("MenuVehicleRadioLink: Radio link new requested capabilities: %s", szBuffC2);

   if ( link_capabilities == g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] )
      return;

   log_line("Sending new link capabilities: %d for radio link %d", link_capabilities, iRadioLink+1);
   u32 param = (((u32)iRadioLink) & 0xFF) | (link_capabilities<<8);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_CAPABILITIES, param, NULL, 0) )
      addMenuItems();
}

void MenuVehicleRadioLink::sendRadioLinkConfig(int iRadioLink)
{
   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink];
   newRadioLinkParams.link_capabilities_flags[iRadioLink] = link_capabilities;

   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink];
   bool bOldIsUsingMCS = false;
   bool bNewIsUsingMCS = false;
   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[m_iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
      bOldIsUsingMCS = true;

   // Clear and set datarate type
   if ( -1 != m_IndexDataRatesType )
   {
      radioFlags &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES);
      if ( 1 == m_pItemsSelect[8]->getSelectedIndex() )
      {
         radioFlags |= RADIO_FLAGS_USE_MCS_DATARATES;
         bNewIsUsingMCS = true;
      }
      else
         radioFlags |= RADIO_FLAGS_USE_LEGACY_DATARATES;
   }
   log_line("MenuVehicleRadioLink: Use MCS modulations? old: %d, new: %d", bOldIsUsingMCS, bNewIsUsingMCS);
   
   if ( -1 != m_IndexHT )
   {
      // Clear and set channel bandwidth
      radioFlags &= ~(RADIO_FLAG_HT40_VEHICLE | RADIO_FLAG_HT40_CONTROLLER);
      if ( 1 == m_pItemsSelect[11]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_HT40_VEHICLE;
      else if ( 2 == m_pItemsSelect[11]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_HT40_CONTROLLER;
      else if ( 3 == m_pItemsSelect[11]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_HT40_VEHICLE | RADIO_FLAG_HT40_CONTROLLER;
   }
   if ( bOldIsUsingMCS && bNewIsUsingMCS )
   if ( (-1 != m_IndexSTBC) && (-1 != m_IndexLDPC) && (-1 != m_IndexSGI) )
   if ( (NULL != m_pItemsSelect[12]) && (NULL != m_pItemsSelect[13]) && (NULL != m_pItemsSelect[14]) )
   {
      radioFlags &= ~RADIO_FLAGS_MCS_MASK;
        
      if ( 1 == m_pItemsSelect[12]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_LDPC_VEHICLE;
      else if ( 2 == m_pItemsSelect[12]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_LDPC_CONTROLLER;
      else if ( 3 == m_pItemsSelect[12]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_LDPC_VEHICLE | RADIO_FLAG_LDPC_CONTROLLER;
     
      if ( 1 == m_pItemsSelect[13]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_SGI_VEHICLE;
      else if ( 2 == m_pItemsSelect[13]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_SGI_CONTROLLER;
      else if ( 3 == m_pItemsSelect[13]->getSelectedIndex() )
         radioFlags |= RADIO_FLAG_SGI_VEHICLE | RADIO_FLAG_SGI_CONTROLLER;

      if ( NULL != m_pItemsSelect[14] )
      {
         if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
            radioFlags |= RADIO_FLAG_STBC_VEHICLE;
         else if ( 2 == m_pItemsSelect[14]->getSelectedIndex() )
            radioFlags |= RADIO_FLAG_STBC_CONTROLLER;
         else if ( 3 == m_pItemsSelect[14]->getSelectedIndex() )
            radioFlags |= RADIO_FLAG_STBC_VEHICLE | RADIO_FLAG_STBC_CONTROLLER;
      }
   }
   newRadioLinkParams.link_radio_flags[iRadioLink] = radioFlags;

   // Data rates video
   
   if ( (-1 != m_IndexDataRateVideo) && (-1 != m_IndexDataRateDataDownlink) && (-1 != m_IndexDataRateDataUplink) )
   {
      log_line("MenuVehicleRadioLink: Selected datarates indexes: %d, %d, %d", 
         m_pItemsSelect[3]->getSelectedIndex(),
         m_pItemsSelect[5]->getSelectedIndex(),
         m_pItemsSelect[6]->getSelectedIndex());
      int indexRate = m_pItemsSelect[3]->getSelectedIndex();
      if ( 0 == indexRate )
         newRadioLinkParams.downlink_datarate_video_bps[m_iVehicleRadioLink] = 0;
      else if ( bNewIsUsingMCS && (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink] > 0) )
      {
         int iMCSRate = convertLegacyRateToMCSRate(g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink], radioFlags & RADIO_FLAG_HT40_VEHICLE);
         newRadioLinkParams.downlink_datarate_video_bps[iRadioLink] = -1 - iMCSRate;
         log_line("MenuVehicleRadioLink: Converted video legacy datarate %d to MCS rate: %d (%d bps)", g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink], iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE) );
      }
      else if ( (!bNewIsUsingMCS) && (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink] < 0) )
      {
         int iMCSRate = -g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink]-1;
         newRadioLinkParams.downlink_datarate_video_bps[iRadioLink] = convertMCSRateToLegacyRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE);
         log_line("MenuVehicleRadioLink: Converted video MCS rate %d (%d bps) to legacy datarate: %d", iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE), newRadioLinkParams.downlink_datarate_video_bps[iRadioLink]);
      }
      else if ( bOldIsUsingMCS == bNewIsUsingMCS )
      {
         if ( bNewIsUsingMCS )
            newRadioLinkParams.downlink_datarate_video_bps[iRadioLink] = -indexRate;
         else
            newRadioLinkParams.downlink_datarate_video_bps[iRadioLink] = getLegacyDataRatesBPS()[indexRate-1];
      }
   }

   if ( 0 != newRadioLinkParams.downlink_datarate_video_bps[m_iVehicleRadioLink] )
   if ( 0 == g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iVehicleRadioLink] )
   {
      int iCountVariableLinks = 0;
      int iCountHighRateLinks = 0;
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         if ( i == m_iVehicleRadioLink )
            continue;
         bool bIsHighCapacityLink = false;
         for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == i )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
            if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
            {
               iCountHighRateLinks++;
               bIsHighCapacityLink = true;
               break;
            }
         }

         if ( bIsHighCapacityLink )
         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         if ( g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i] == 0 )
            iCountVariableLinks++;
      }

      if ( 0 == iCountVariableLinks )
      {
         m_bWaitingConfirmationFromUser = true;
         if ( 0 == iCountHighRateLinks )
            addMessageWithTitle(11, L("Settings auto updated"), L("You did set your radio link to fixed radio rates. Adaptive video will be disabled until you set it back to auto."));
         else
            addMessageWithTitle(11, L("Settings auto updated"), L("You did set all your radio links to fixed radio rates. Adaptive video will be disabled until you set at least one link back to auto."));
      }
   }
 
   // Data rates telemetry
   
   if ( -1 != m_IndexDataRateDataDownlink )
   {
      int indexRate = m_pItemsSelect[5]->getSelectedIndex();
      if ( 0 == indexRate )
         newRadioLinkParams.downlink_datarate_data_bps[m_iVehicleRadioLink] = 0;
      else if ( 1 == indexRate )
         newRadioLinkParams.downlink_datarate_data_bps[m_iVehicleRadioLink] = -100;
      else if ( bNewIsUsingMCS && (g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] > 0) )
      {
         int iMCSRate = convertLegacyRateToMCSRate(g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink], radioFlags & RADIO_FLAG_HT40_VEHICLE);
         newRadioLinkParams.downlink_datarate_data_bps[iRadioLink] = -1 - iMCSRate;
         log_line("MenuVehicleRadioLink: Converted data-downlink legacy datarate %d to MCS rate: %d (%d bps)", g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink], iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE) );
      }
      else if ( (!bNewIsUsingMCS) && (g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink] < 0) )
      {
         int iMCSRate = -g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iVehicleRadioLink]-1;
         newRadioLinkParams.downlink_datarate_data_bps[iRadioLink] = convertMCSRateToLegacyRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE);
         log_line("MenuVehicleRadioLink: Converted data-downlink MCS rate %d (%d bps) to legacy datarate: %d", iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE), newRadioLinkParams.downlink_datarate_data_bps[iRadioLink]);
      }
      else if ( bOldIsUsingMCS == bNewIsUsingMCS )
      {
         if ( bNewIsUsingMCS )
            newRadioLinkParams.downlink_datarate_data_bps[iRadioLink] = -indexRate+1;
         else
            newRadioLinkParams.downlink_datarate_data_bps[iRadioLink] = getLegacyDataRatesBPS()[indexRate-2];
      }
   }

   if ( -1 != m_IndexDataRateDataUplink )
   {
      int indexRate = m_pItemsSelect[6]->getSelectedIndex();
      if ( 0 == indexRate )
         newRadioLinkParams.uplink_datarate_data_bps[m_iVehicleRadioLink] = 0;
      else if ( 1 == indexRate )
         newRadioLinkParams.uplink_datarate_data_bps[m_iVehicleRadioLink] = -100;
      else if ( bNewIsUsingMCS && (g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] > 0) )
      {
         int iMCSRate = convertLegacyRateToMCSRate(g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink], radioFlags & RADIO_FLAG_HT40_VEHICLE);
         newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = -1 - iMCSRate;
         log_line("MenuVehicleRadioLink: Converted data-uplink legacy datarate %d to MCS rate: %d (%d bps)", g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink], iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE) );
      }
      else if ( (!bNewIsUsingMCS) && (g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink] < 0) )
      {
         int iMCSRate = -g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iVehicleRadioLink]-1;
         newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = convertMCSRateToLegacyRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE);
         log_line("MenuVehicleRadioLink: Converted data-uplink MCS rate %d (%d bps) to legacy datarate: %d", iMCSRate, getRealDataRateFromMCSRate(iMCSRate, radioFlags & RADIO_FLAG_HT40_VEHICLE), newRadioLinkParams.uplink_datarate_data_bps[iRadioLink]);
      }
      else if ( bOldIsUsingMCS == bNewIsUsingMCS )
      {
         if ( bNewIsUsingMCS )
            newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = -indexRate+1;
         else
            newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = getLegacyDataRatesBPS()[indexRate-2];
      }
   }

   if ( -1 != m_IndexMaxLoad )
       newRadioLinkParams.uMaxLinkLoadPercent[iRadioLink] = (1+m_pItemsSelect[7]->getSelectedIndex())*10;
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadioLink: No change in radio link config. Do not send command.");
      return;
   }

   memcpy((u8*)&m_RadioLinksParamsToApply, (u8*)&newRadioLinkParams, sizeof(type_radio_links_parameters));

   if ( m_bWaitingConfirmationFromUser )
      return;

   sendRadioLinkConfigParams(&m_RadioLinksParamsToApply, true);
}

void MenuVehicleRadioLink::sendRadioLinkConfigParams(type_radio_links_parameters* pRadioLinkParams, bool bCheckVideo)
{
   if ( NULL == pRadioLinkParams )
      return;

   log_line("MenuVehicleRadioLink: Sending radio config for radio link %d, check video params: %s", m_iVehicleRadioLink+1, bCheckVideo?"yes":"no");
   send_pause_adaptive_to_router(5000);
   send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);

   // Check for video bitrate adjustment to fit new fixed rates
   if ( bCheckVideo )
   if ( pRadioLinkParams->downlink_datarate_video_bps[m_iVehicleRadioLink] != 0 )
   {
      u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForRadioLinks(pRadioLinkParams, &g_pCurrentModel->video_params, &(g_pCurrentModel->video_link_profiles[0]));
      bool bUpdatedVideoBitrate = false;
      u32 uVideoBitrateToSet = 0;

      log_line("MenuVehicleRadioLink: Max video bitrate on new radio config: %.2f Mbps", (float) uMaxVideoBitrate/1000.0/1000.0);
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         if ( 0 == g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS )
            continue;
         if ( g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS > uMaxVideoBitrate )
         {
            log_line("MenuVehicleRadioLink: Must decrease video bitrate (%.2f Mbps) for video profile %s to max allowed on current links: %.2f Mbps",
               (float)g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS/1000.0/1000.0,
               str_get_video_profile_name(i),
               (float)uMaxVideoBitrate/1000.0/1000.0);
            uVideoBitrateToSet = uMaxVideoBitrate;
            bUpdatedVideoBitrate = true;
         }
      }

      if ( bUpdatedVideoBitrate && (0 != uVideoBitrateToSet) )
      {
         
         video_parameters_t paramsNew;
         type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
         memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
         memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));

         for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
         {
            if ( 0 != profiles[i].uTargetVideoBitrateBPS )
               profiles[i].uTargetVideoBitrateBPS = uVideoBitrateToSet;
         }

         if ( handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&profiles, MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
            m_bWaitingVideoChangeConfirmationFromVehicle = true;
         return;
      }
   }

   char szBuff[256];
   str_get_radio_capabilities_description(pRadioLinkParams->link_capabilities_flags[m_iVehicleRadioLink], szBuff);
 
   log_line("MenuVehicleRadioLink: Radio link new requested capabilities: %s", szBuff);

   log_line("MenuVehicleRadioLink: Sending new radio data rates for link %d: vid: %d, data-down: %d, data-up: %d",
      m_iVehicleRadioLink+1, pRadioLinkParams->downlink_datarate_video_bps[m_iVehicleRadioLink],
      pRadioLinkParams->downlink_datarate_data_bps[m_iVehicleRadioLink],
      pRadioLinkParams->uplink_datarate_data_bps[m_iVehicleRadioLink]);

   t_packet_header PH;
   int iHeaderSize = 5;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + iHeaderSize + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioLinkTestNumberCount = 0;
   s_iMenuRadioLinkTestNumberCount++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));

   buffer[sizeof(t_packet_header)] = 1;
   buffer[sizeof(t_packet_header)+1] = iHeaderSize;
   buffer[sizeof(t_packet_header)+2] = (u8)m_iVehicleRadioLink;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioLinkTestNumberCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + iHeaderSize, pRadioLinkParams, sizeof(type_radio_links_parameters));

   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(m_iVehicleRadioLink);
   warnings_add_configuring_radio_link(m_iVehicleRadioLink, L("Updating Radio Link"));
}

void MenuVehicleRadioLink::sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("MenuVehicleRadioLink: Changing radio link %d frequency to %u khz (%s)", iVehicleLinkIndex+1, uNewFreqKhz, str_format_frequency(uNewFreqKhz));

   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, iVehicleLinkIndex) )
   {
      if ( link_is_reconfiguring_radiolink() )
      {
         add_menu_to_stack(new MenuConfirmation("Configuration In Progress","Another radio link configuration change is in progress. Please wait.", 0, true));
         valuesToUI();
         return;
      }
      log_line("MenuVehicleRadio: User changed unused radio link %d frequency to %s", iVehicleLinkIndex+1, str_format_frequency(uNewFreqKhz));

      u32 param = uNewFreqKhz & 0xFFFFFF;
      param = param | (((u32)iVehicleLinkIndex)<<24);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
         valuesToUI();
      else
      {
         link_set_is_reconfiguring_radiolink(iVehicleLinkIndex, false, true, true);
         warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing frequency");
      }
      return;
   }

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   newRadioLinkParams.link_frequency_khz[iVehicleLinkIndex] = uNewFreqKhz;
   
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadioLink: No change in radio link frequency. Do not send command.");
      return;
   }

   t_packet_header PH;
   int iHeaderSize = 5;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + iHeaderSize + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioLinkTestNumberCount2 = 0;
   s_iMenuRadioLinkTestNumberCount2++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));

   buffer[sizeof(t_packet_header)] = 1;
   buffer[sizeof(t_packet_header)+1] = iHeaderSize;
   buffer[sizeof(t_packet_header)+2] = (u8)iVehicleLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioLinkTestNumberCount2;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + iHeaderSize, &newRadioLinkParams, sizeof(type_radio_links_parameters));

   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(iVehicleLinkIndex);
   warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing Frequency");
}

void MenuVehicleRadioLink::onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded)
{
   Menu::onVehicleCommandFinished(uCommandId, uCommandType, bSucceeded);

   if ( uCommandType == COMMAND_ID_SET_VIDEO_PARAMETERS )
   if ( m_bWaitingVideoChangeConfirmationFromVehicle )
   {
      log_line("MenuVehicleRadioLink: finished updating video params. Send new radio link params.");
      addMessageWithTitle(12, L("Updated video bitrate"), L("Your video bitrate was adjusted to fit the new fixed radio datarates."));
   }
}

void MenuVehicleRadioLink::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 11 == iChildMenuId/1000 )
   if ( m_bWaitingConfirmationFromUser )
   {
      m_bWaitingConfirmationFromUser = false;
      sendRadioLinkConfigParams(&m_RadioLinksParamsToApply, true);
      return;
   }
   
   if ( 12 == iChildMenuId/1000 )
   if ( m_bWaitingVideoChangeConfirmationFromVehicle )
   {
      log_line("MenuVehicleRadioLink: Finished waiting for user confirmation on updated video bitrate.");
      m_bWaitingVideoChangeConfirmationFromVehicle = false;
      sendRadioLinkConfigParams(&m_RadioLinksParamsToApply, false);
      return;
   }
}

int MenuVehicleRadioLink::onBack()
{
   return Menu::onBack();
}

void MenuVehicleRadioLink::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( link_is_reconfiguring_radiolink() )
   {
      handle_commands_show_popup_progress();
      addMenuItems();
      return;
   }

   char szBuff[256];

   log_line("MenuVehicleRadioLink: Current vehicle radio interface assigned to vehicle radio link %d: %d", m_iVehicleRadioLink+1, m_iVehicleRadioInterface+1);
   if ( -1 == m_iVehicleRadioInterface )
      return;

   if ( (-1 != m_IndexFrequency) && (m_IndexFrequency == m_SelectedIndex) )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      u32 freq = m_SupportedChannels[index];
      int band = getBand(freq);      
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iVehicleRadioLink] )
         return;

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];
         log_line("Current frequency on card %d: %s", i+1, str_format_frequency(nicFreq[i]));
      }

      int nicFlags[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

      nicFreq[m_iVehicleRadioLink] = freq;

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
   
      if ( NULL != szError && 0 != szError[0] )
      {
         log_line(szError);
         add_menu_to_stack(new MenuConfirmation("Invalid option",szError, 0, true));
         addMenuItems();
         return;
      }

      bool supportedOnController = false;
      bool allSupported = true;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
            continue;
         if ( band & pRadioHWInfo->supportedBands )
            supportedOnController = true;
         else
            allSupported = false;
      }
      if ( ! supportedOnController )
      {
         sprintf(szBuff, "%s frequency is not supported by your controller.", str_format_frequency(freq));
         add_menu_to_stack(new MenuConfirmation("Invalid option",szBuff, 0, true));
         addMenuItems();
         return;
      }
      if ( (! allSupported) && (1 == g_pCurrentModel->radioLinksParams.links_count) )
      {
         char szBuff[256];
         sprintf(szBuff, "Not all radio interfaces on your controller support %s frequency. Some radio interfaces on the controller will not be used to communicate with this vehicle.", str_format_frequency(freq));
         add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         addMenuItems();
      }

      sendNewRadioLinkFrequency(m_iVehicleRadioLink, freq);
      return;
   }

   if ( (-1 != m_IndexUsage) && (m_IndexUsage == m_SelectedIndex) )
   {
      int usage = m_pItemsSelect[1]->getSelectedIndex();

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];

      int nicFlags[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

      nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
      nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
      if ( 0 == usage ) nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] | RADIO_HW_CAPABILITY_FLAG_DISABLED;
      if ( 1 == usage ) nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
      if ( 2 == usage ) nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
      if ( 3 == usage ) nicFlags[m_iVehicleRadioLink] = nicFlags[m_iVehicleRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
   
      if ( NULL != szError && 0 != szError[0] )
      {
         log_line(szError);
         add_menu_to_stack(new MenuConfirmation("Invalid option",szError, 0, true));
         addMenuItems();
         return;
      }

      sendRadioLinkCapabilities(m_iVehicleRadioLink);
      return;
   }

   if ( (-1 != m_IndexCapabilities) && (m_IndexCapabilities == m_SelectedIndex) )
   {
      sendRadioLinkCapabilities(m_iVehicleRadioLink);
      return;
   }

   if ( (-1 != m_IndexDataRatesType) && (m_IndexDataRatesType == m_SelectedIndex) )
   {
      sendRadioLinkConfig(m_iVehicleRadioLink);
   }

   if ( (-1 != m_IndexDataRateVideo) && (m_IndexDataRateVideo == m_SelectedIndex) )
   {
      sendRadioLinkConfig(m_iVehicleRadioLink);
      return;
   }

   if ( ((-1 != m_IndexDataRateDataDownlink) && (m_IndexDataRateDataDownlink == m_SelectedIndex)) ||
        ((-1 != m_IndexDataRateDataUplink) && (m_IndexDataRateDataUplink == m_SelectedIndex)) ||
        ((-1 != m_IndexMaxLoad) && (m_IndexMaxLoad == m_SelectedIndex)) )
   {
      sendRadioLinkConfig(m_iVehicleRadioLink);
      return;
   }

      
   if ( ((-1 != m_IndexSGI) && (m_IndexSGI == m_SelectedIndex)) ||
        ((-1 != m_IndexLDPC) && (m_IndexLDPC == m_SelectedIndex)) ||
        ((-1 != m_IndexSTBC) && (m_IndexSTBC == m_SelectedIndex)) ||
        ((-1 != m_IndexHT) && (m_IndexHT == m_SelectedIndex)) )
   {
      sendRadioLinkConfig(m_iVehicleRadioLink);
      return;
   }

   if ( (-1 != m_IndexReset) && (m_IndexReset == m_SelectedIndex) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_RADIO_LINK, m_iVehicleRadioLink, NULL, 0) )
         addMenuItems();
   }
}

void MenuVehicleRadioLink::onChangeRadioConfigFinished(bool bSucceeded)
{
   addMenuItems();
}
