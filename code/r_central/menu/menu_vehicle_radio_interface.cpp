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
#include "menu_vehicle_radio_interface.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../../base/tx_powers.h"

const char* s_szMenuRadio_SingleCard3 = "Note: You can not change the function and capabilities of the radio interface as there is a single one present on the vehicle.";

MenuVehicleRadioInterface::MenuVehicleRadioInterface(int iRadioInterface)
:Menu(MENU_ID_VEHICLE_RADIO_INTERFACE, "Vehicle Radio Interface Parameters", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_iRadioInterface = iRadioInterface;

   log_line("Opening menu for radio interface %d parameters...", m_iRadioInterface);

   char szBuff[256];

   sprintf(szBuff, "Vehicle Radio Interface %d Parameters", m_iRadioInterface+1);
   setTitle(szBuff);
   //addTopLine("You have a single radio interface on the vehicle side for this radio link. You can't change the radio interface parameters. They are derived automatically from the radio link params.");
   //addMenuItem(new MenuItem("Ok"));

   m_pItemsSelect[0] = createMenuItemCardModelSelector("Card Model");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexCardModel = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Output Boost Mode"), L("If this radio interface has an output RF power booster connected to it, select the one you have."));
   m_pItemsSelect[1]->addSelection(L("None"));
   m_pItemsSelect[1]->addSelection(L("2W Booster"));
   m_pItemsSelect[1]->addSelection(L("4W Booster"));
   m_pItemsSelect[1]->setIsEditable();
   m_iIndexBoostMode = addMenuItem(m_pItemsSelect[1]);
}

MenuVehicleRadioInterface::~MenuVehicleRadioInterface()
{
}

void MenuVehicleRadioInterface::onShow()
{
   valuesToUI();
   Menu::onShow();

   invalidate();
}

void MenuVehicleRadioInterface::valuesToUI()
{
   setSelectionForCardModelSelector(m_pItemsSelect[0], g_pCurrentModel->radioInterfacesParams.interface_card_model[m_iRadioInterface] );

   m_pItemsSelect[1]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[m_iRadioInterface] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
      m_pItemsSelect[1]->setSelectedIndex(1);
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[m_iRadioInterface] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
      m_pItemsSelect[1]->setSelectedIndex(2);
}

void MenuVehicleRadioInterface::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleRadioInterface::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleRadioInterface::sendInterfaceCapabilitiesFlags(int iInterfaceIndex)
{

}

void MenuVehicleRadioInterface::sendInterfaceRadioFlags(int iInterfaceIndex)
{

}

void MenuVehicleRadioInterface::onSelectItem()
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

   if ( m_IndexCardModel == m_SelectedIndex )
   {
      u32 uParam = m_iRadioInterface;
      u32 uCardModel = getSelectedCardForCardModelSelector(m_pItemsSelect[0]);
      uParam |= ((uCardModel & 0xFF) << 8);

      log_line("MenuVehicleRadioInterface: Selected card model %u, selection index: %d", uCardModel, m_pItemsSelect[0]->getSelectedIndex());
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_CARD_MODEL, uParam, NULL, 0) )
         valuesToUI();

      return;
   }

   if ( m_iIndexBoostMode == m_SelectedIndex )
   {
      u32 uNewFlags = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[m_iRadioInterface];
      uNewFlags &= ~RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W;
      uNewFlags &= ~RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W;
      if ( 1 == m_pItemsSelect[1]->getSelectedIndex() )
         uNewFlags |= RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W;
      if ( 2 == m_pItemsSelect[1]->getSelectedIndex() )
         uNewFlags |= RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W;

      if ( uNewFlags == g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[m_iRadioInterface] )
         return;

      u32 uParam = m_iRadioInterface;
      uParam = uParam | ((uNewFlags << 8) & 0xFFFFFF00);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES, uParam, NULL, 0) )
         valuesToUI();
   }
}