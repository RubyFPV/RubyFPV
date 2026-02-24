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
#include "menu_vehicle_radio_link_elrs.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../../base/ctrl_settings.h"

MenuVehicleRadioLinkELRS::MenuVehicleRadioLinkELRS(int iRadioLink)
:Menu(MENU_ID_VEHICLE_RADIO_LINK_ELRS, "Vehicle ELRS Radio Link Parameters", NULL)
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

   sprintf(szBuff, "Vehicle ELRS Radio Link %d Parameters", m_iRadioLink+1);
   setTitle(szBuff);

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);

   if ( -1 == iRadioInterfaceId )
   {
      log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", m_iRadioLink+1);
      m_bHasValidInterface = false;
      return;
   }

   log_line("Opened menu to configure ELRS radio link %d, used by radio interface %d on the model side.", m_iRadioLink+1, iRadioInterfaceId+1);

   m_bHasValidInterface = true;

   addMenuItem(new MenuItemText("Use ELRS configurator to fully configure your ELRS radio module."));
   addMenuItem(new MenuItemText("Set the Ruby air data rate for this ELRS radio link (using the option below) to match the air datrate you set in your ELRS module."));

   m_pItemsSelect[0] = new MenuItemSelect("Enabled", "Enables or disables this radio link.");
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnabled = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[2] = new MenuItemSelect("Air Radio Data Rate", "Sets the physical radio air data rate to use on this radio link. Lower radio data rates gives longer radio range.");
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
   m_IndexAirDataRate = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[0] = new MenuItemSlider("Air Packet Size", "Sets the default packet size sent over air. If data to be sent is bigger, it is split into packets of this size.", DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE, DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE, DEFAULT_SIK_PACKET_SIZE, 0.12*Menu::getScaleFactor());
   m_IndexAirPacketSize = addMenuItem(m_pItemsSlider[0]);

}

MenuVehicleRadioLinkELRS::~MenuVehicleRadioLinkELRS()
{
}

void MenuVehicleRadioLinkELRS::onShow()
{
   valuesToUI();
   Menu::onShow();

   invalidate();
}

void MenuVehicleRadioLinkELRS::valuesToUI()
{
   if ( (! m_bHasValidInterface) || (m_iRadioLink < 0) )
      return;

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   m_pItemsSlider[0]->setCurrentValue(pCS->iSiKPacketSize);

   u32 uLinkCapabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink];

   if ( uLinkCapabilities & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      log_line("Menu: Radio serial link %d is used for relay. No values to update.", m_iRadioLink+1);
      m_pItemsSelect[0]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
      return;
   }

   m_pItemsSelect[0]->setEnabled(true);
   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSlider[0]->setEnabled(true);

   m_pItemsSelect[0]->setSelectedIndex(1);

   if ( uLinkCapabilities & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      log_line("Menu: Radio serial link %d is disabled. No values to update.", m_iRadioLink+1);
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
   }

   log_line("Menu: Radio ELRS link %d current air data rates: %d/%d", m_iRadioLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[m_iRadioLink], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iRadioLink]);
   int selectedIndex = 0;
   for( int i=0; i<getSiKAirDataRatesCount(); i++ )
   {
      if ( getSiKAirDataRates()[i] == g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[m_iRadioLink] )
         break;
      selectedIndex++;
   }

   m_pItemsSelect[2]->setSelectedIndex(selectedIndex);
}

void MenuVehicleRadioLinkELRS::Render()
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


void MenuVehicleRadioLinkELRS::sendRadioLinkFlags(int linkIndex)
{
   if ( linkIndex < 0 || linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
      return;

   u32 uRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags_tx[linkIndex];
   int datarate_bps = 0;

   int indexRate = m_pItemsSelect[2]->getSelectedIndex();

   if ( (indexRate < 0) || (indexRate >= getSiKAirDataRatesCount()) )
      return;
   datarate_bps = getSiKAirDataRates()[indexRate];

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

void MenuVehicleRadioLinkELRS::onSelectItem()
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


   if ( m_IndexAirDataRate == m_SelectedIndex )
   {
      sendRadioLinkFlags(m_iRadioLink);
      return;
   }

   if ( m_IndexAirPacketSize == m_SelectedIndex )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iSiKPacketSize = m_pItemsSlider[0]->getCurrentValue();
      save_ControllerSettings();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SIK_PACKET_SIZE, (u32) pCS->iSiKPacketSize, NULL, 0) )
         valuesToUI();
   }
}