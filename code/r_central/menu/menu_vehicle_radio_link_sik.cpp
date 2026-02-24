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
#include "menu_vehicle_radio_link_sik.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../../base/ctrl_settings.h"

MenuVehicleRadioLinkSiK::MenuVehicleRadioLinkSiK(int iRadioLink)
:Menu(MENU_ID_VEHICLE_RADIO_LINK_SIK, "Vehicle SiK Radio Link Parameters", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_iRadioLink = iRadioLink;


   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      log_line("Opening menu for relay radio link %d ...", m_iRadioLink+1);
   else
      log_line("Opening menu for radio link %d ...", m_iRadioLink+1);

   char szBuff[256];

   sprintf(szBuff, "Vehicle SiK Radio Link %d Parameters", m_iRadioLink+1);
   setTitle(szBuff);

   char szBands[128];
   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);

   if ( -1 == iRadioInterfaceId )
   {
      log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", m_iRadioLink+1);
      m_bHasValidInterface = false;
      return;
   }

   log_line("Opened menu to configure SiK radio link %d, used by radio interface %d on the model side.", m_iRadioLink+1, iRadioInterfaceId+1);

   m_bHasValidInterface = true;
   str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBands);

   u32 controllerBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      controllerBands |= pRadioHWInfo->supportedBands;
   }

   m_SupportedChannelsCount = getSupportedChannels( controllerBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);

   m_pItemsSelect[0] = new MenuItemSelect("Enabled", "Enables or disables this radio link.");
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   //m_pItemsSelect[0]->setUseMultiViewLayout();
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnabled = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Frequency");

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      sprintf(szBuff,"Relay %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
      m_pItemsSelect[1]->addSelection(szBuff);
   }
   else
   {
      for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
      {
         if ( m_SupportedChannels[ch] == 0 )
         {
            m_pItemsSelect[1]->addSeparator();
            continue;
         }
         strcpy(szBuff, str_format_frequency(m_SupportedChannels[ch]));
         m_pItemsSelect[1]->addSelection(szBuff);
      }
   }
   m_SupportedChannelsCount = getSupportedChannels( controllerBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);

   m_pItemsSelect[1]->setIsEditable();
   m_IndexFrequency = addMenuItem(m_pItemsSelect[1]);

   m_IndexUsage = -1;
   /*
   m_pItemsSelect[7] = new MenuItemSelect("Usage", "Selects what type of data gets sent/received on this radio link, or disable it.");
   m_pItemsSelect[7]->addSelection("Disabled");
   m_pItemsSelect[7]->addSelection("Video & Data");
   m_pItemsSelect[7]->addSelection("Video");
   m_pItemsSelect[7]->addSelection("Data");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexUsage = addMenuItem(m_pItemsSelect[7]);
   */

   m_pItemsSelect[2] = new MenuItemSelect("Radio Data Rate", "Sets the physical radio air data rate to use on this radio link. Lower radio data rates gives longer radio range.");
   for( int i=0; i<getSiKAirDataRatesCount(); i++ )
   {
      int iAirRate = getSiKAirDataRates()[i];
      if ( iAirRate < 10000 )
         sprintf(szBuff, "%d bps", iAirRate);
      else
         sprintf(szBuff, "%d kbps", (iAirRate)/1000);
      m_pItemsSelect[2]->addSelection(szBuff);
   }

   m_pItemsSelect[2]->setIsEditable();
   m_IndexDataRate = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[0] = new MenuItemSlider("Packet Size", "Sets the default packet size sent over air. If data to be sent is bigger, it is split into packets of this size.", DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE, DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE, DEFAULT_SIK_PACKET_SIZE, 0.12*Menu::getScaleFactor());
   m_iPacketSize = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[3] = new MenuItemSelect("Error Correction", "Enables hardware error correction on the radio link. Cuts the usable radio air data rate by half.");
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   //m_pItemsSelect[3]->setUseMultiViewLayout();
   m_pItemsSelect[3]->setIsEditable();
   m_IndexSiKECC = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect("Listen Before Talk", "Enables listen before talk functionality. Can lead to lower throughput on the air link.");
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   //m_pItemsSelect[4]->setUseMultiViewLayout();
   m_pItemsSelect[4]->setIsEditable();
   m_IndexSiKLBT = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("Enable Manchester Encoding", "Enables Manchester type of encoding at the hardware level. Can lead to lower throughput on the air link but better link quality.");
   m_pItemsSelect[5]->addSelection("No");
   m_pItemsSelect[5]->addSelection("Yes");
   //m_pItemsSelect[5]->setUseMultiViewLayout();
   m_pItemsSelect[5]->setIsEditable();
   m_IndexSiKMCSTR = addMenuItem(m_pItemsSelect[5]);
}

MenuVehicleRadioLinkSiK::~MenuVehicleRadioLinkSiK()
{
}

void MenuVehicleRadioLinkSiK::onShow()
{
   valuesToUI();
   Menu::onShow();

   invalidate();
}

void MenuVehicleRadioLinkSiK::valuesToUI()
{
   if ( (! m_bHasValidInterface) || (m_iRadioLink < 0) )
      return;

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   m_pItemsSlider[0]->setCurrentValue(pCS->iSiKPacketSize);

   u32 uLinkCapabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink];
   u32 uLinkRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags_tx[m_iRadioLink];

   if ( uLinkCapabilities & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      log_line("Menu: Radio SiK link %d is used for relay. No values to update.", m_iRadioLink+1);
      m_pItemsSelect[0]->setEnabled(false);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
      return;
   }

   m_pItemsSelect[0]->setEnabled(true);
   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[4]->setEnabled(true);
   m_pItemsSelect[5]->setEnabled(true);
   m_pItemsSlider[0]->setEnabled(true);

   m_pItemsSelect[0]->setSelectedIndex(1);

   if ( uLinkCapabilities & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      log_line("Menu: Radio SiK link %d is disabled. No values to update.", m_iRadioLink+1);
      m_pItemsSelect[0]->setSelectedIndex(0);

      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
   }

   log_line("Menu: Radio Sik link %d current frequency: %s", m_iRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink]));
   int selectedIndex = 0;
   for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink] == m_SupportedChannels[ch])
         break;
      selectedIndex++;
   }

   m_pItemsSelect[1]->setSelection(selectedIndex);

   log_line("Menu: Radio SiK link %d current air data rates: %d/%d", m_iRadioLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iRadioLink], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iRadioLink]);
   selectedIndex = 0;
   for( int i=0; i<getSiKAirDataRatesCount(); i++ )
   {
      if ( getSiKAirDataRates()[i] == g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iRadioLink] )
         break;
      selectedIndex++;
   }

   m_pItemsSelect[2]->setSelectedIndex(selectedIndex);

   if ( uLinkRadioFlags & RADIO_FLAGS_SIK_ECC )
      m_pItemsSelect[3]->setSelectedIndex(1);
   else
      m_pItemsSelect[3]->setSelectedIndex(0);

   if ( uLinkRadioFlags & RADIO_FLAGS_SIK_LBT )
      m_pItemsSelect[4]->setSelectedIndex(1);
   else
      m_pItemsSelect[4]->setSelectedIndex(0);

   if ( uLinkRadioFlags & RADIO_FLAGS_SIK_MCSTR )
      m_pItemsSelect[5]->setSelectedIndex(1);
   else
      m_pItemsSelect[5]->setSelectedIndex(0);
}

void MenuVehicleRadioLinkSiK::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);

      if ( i == 1 && ((1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count)) )
      {
         g_pRenderEngine->setColors(get_Color_MenuText());
      }
   }
   RenderEnd(yTop);
}


void MenuVehicleRadioLinkSiK::sendLinkCapabilitiesFlags(int linkIndex)
{
}

void MenuVehicleRadioLinkSiK::sendRadioLinkFlags(int linkIndex)
{
   if ( linkIndex < 0 || linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
      return;

   u32 uRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags_tx[linkIndex];
   int datarate_bps = 0;

   int indexRate = m_pItemsSelect[2]->getSelectedIndex();

   if ( (indexRate < 0) || (indexRate >= getSiKAirDataRatesCount()) )
      return;
   datarate_bps = getSiKAirDataRates()[indexRate];


   uRadioFlags &= ~(RADIO_FLAGS_SIK_ECC | RADIO_FLAGS_SIK_LBT | RADIO_FLAGS_SIK_MCSTR);

   if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
      uRadioFlags |= RADIO_FLAGS_SIK_ECC;
   if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      uRadioFlags |= RADIO_FLAGS_SIK_LBT;

   if ( 1 == m_pItemsSelect[5]->getSelectedIndex() )
      uRadioFlags |= RADIO_FLAGS_SIK_MCSTR;

   u8 buffer[64];
   u32* p = (u32*)&(buffer[0]);
   *p = (u32)linkIndex;
   p++;
   *p = uRadioFlags;
   p++;
   int* pi = (int*)p;
   *pi = datarate_bps;
   pi++;
   *pi = datarate_bps;

   memcpy(&g_LastGoodRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));

   g_pCurrentModel->radioLinksParams.link_radio_flags_tx[linkIndex] = uRadioFlags;
   g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[linkIndex] = datarate_bps;
   g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[linkIndex] = datarate_bps;
   g_pCurrentModel->validateRadioSettings();
   saveControllerModel(g_pCurrentModel);

   send_model_changed_message_to_router(MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS, linkIndex);

   char szBuff[128];
   str_get_radio_frame_flags_description(uRadioFlags, szBuff);
   log_line("Sending to vehicle new radio link flags for radio link %d: %s and datarates: %d/%d", linkIndex+1, szBuff, datarate_bps, datarate_bps);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FLAGS, 0, buffer, 2*sizeof(u32) + 2*sizeof(int)) )
   {
      valuesToUI();
      memcpy(&(g_pCurrentModel->radioLinksParams), &g_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));
   }
   else
   {
      link_set_is_reconfiguring_radiolink(linkIndex, false, true, true);
      warnings_add_configuring_radio_link(linkIndex, "Changing radio link flags");
   }
}

void MenuVehicleRadioLinkSiK::onSelectItem()
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
      return;
   }

   if ( ! m_bHasValidInterface )
      return;

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      return;


   if ( m_IndexFrequency == m_SelectedIndex )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         return;

      int index = m_pItemsSelect[1]->getSelectedIndex();
      u32 freq = m_SupportedChannels[index];
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink] )
         return;

      if ( link_is_reconfiguring_radiolink() )
      {
         add_menu_to_stack(new MenuConfirmation("Configuration In Progress","Another radio link configuration change is in progress. Please wait.", 0, true));
         valuesToUI();
         return;
      }
      log_line("MenuRadioLinkSiK: user changed radio link %d frequency to %s", m_iRadioLink+1, str_format_frequency(freq));

      u32 param = freq & 0xFFFFFF;
      param = param | (((u32)m_iRadioLink)<<24) | 0x80000000; // Highest bit set to 1 to mark the new format of the param
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
         valuesToUI();
      else
      {
         link_set_is_reconfiguring_radiolink(m_iRadioLink, false, true, true);
         warnings_add_configuring_radio_link(m_iRadioLink, "Changing frequency");
      }
      return;
   }

   if ( m_IndexDataRate == m_SelectedIndex ||
        m_IndexSiKECC == m_SelectedIndex ||
        m_IndexSiKLBT == m_SelectedIndex ||
        m_IndexSiKMCSTR == m_SelectedIndex )
   {
      sendRadioLinkFlags(m_iRadioLink);
      return;
   }

   if ( m_iPacketSize == m_SelectedIndex )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iSiKPacketSize = m_pItemsSlider[0]->getCurrentValue();
      save_ControllerSettings();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SIK_PACKET_SIZE, (u32) pCS->iSiKPacketSize, NULL, 0) )
         valuesToUI();
   }
}