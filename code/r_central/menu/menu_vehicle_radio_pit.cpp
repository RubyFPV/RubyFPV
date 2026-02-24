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
#include "menu_vehicle_radio_pit.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../../base/ctrl_settings.h"

MenuVehicleRadioLinkPITModes::MenuVehicleRadioLinkPITModes()
:Menu(MENU_ID_VEHICLE_RADIO_PIT, L("Pit Mode / Auto Power"), NULL)
{
   m_Width = 0.3;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.18;

}

MenuVehicleRadioLinkPITModes::~MenuVehicleRadioLinkPITModes()
{
}

void MenuVehicleRadioLinkPITModes::onShow()
{
  int iTmp = getSelectedMenuItemIndex();

   Menu::onShow();

   if ( m_SelectedIndex >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleRadioLinkPITModes::addItems()
{
   removeAllItems();
   removeAllTopLines();

   Preferences* pP = get_Preferences();

   m_pItemsSelect[0] = new MenuItemSelect(L("Enable PIT Mode"), L("Enables or disables PIT mode (reduced Tx power mode)."));
   m_pItemsSelect[0]->addSelection(L("No"));
   m_pItemsSelect[0]->addSelection(L("Yes"));
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnablePIT = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[0]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE )
      m_pItemsSelect[0]->setSelectedIndex(1);

   m_pItemsSelect[1] = new MenuItemSelect(L("On Arm/Disarm"), L("Enables or disables PIT mode (reduced Tx power mode) when vehicle gets armed/disarmed."));
   m_pItemsSelect[1]->addSelection(L("Disabled"));
   m_pItemsSelect[1]->addSelection(L("Enabled"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexPITModeArm = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[1]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_ARMDISARM )
      m_pItemsSelect[1]->setSelectedIndex(1);

   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
      m_pItemsSelect[1]->setEnabled(false);

   m_pItemsSelect[2] = new MenuItemSelect(L("On High Temperature"), L("Enables or disables PIT mode (reduced Tx power mode) when vehicle gets too hot."));
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexPITModeTemp = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[2]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_TEMP )
      m_pItemsSelect[2]->setSelectedIndex(1);

   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
      m_pItemsSelect[2]->setEnabled(false);


   m_pItemsSlider[0] = new MenuItemSlider(L("Threshold Temperature"), L("Sets the temperature at which to activate the PIT mode."), 0,120,50, 0.12);
   m_pItemsSlider[0]->setMargin(0.01 * Menu::getScaleFactor());
   m_iIndexPITTemperature = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[0]->setCurrentValue((g_pCurrentModel->hwCapabilities.uHWFlags & 0xFF00) >> 8);
   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
      m_pItemsSlider[0]->setEnabled(false);
   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_TEMP) )
      m_pItemsSlider[0]->setEnabled(false);


   m_pItemsSelect[3] = new MenuItemSelect(L("On QA Button"), L("Enables or disables PIT mode (reduced Tx power mode) using a QA button."));
   m_pItemsSelect[3]->addSelection(L("None"));
   m_pItemsSelect[3]->addSelection(L("Button QA1"));
   m_pItemsSelect[3]->addSelection(L("Button QA2"));
   m_pItemsSelect[3]->addSelection(L("Button QA3"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexPITModeQAButton = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[3]->setSelectedIndex(0);
   if ( pP->iActionQuickButton1 == quickActionPITMode )
      m_pItemsSelect[3]->setSelectedIndex(1);
   if ( pP->iActionQuickButton2 == quickActionPITMode )
      m_pItemsSelect[3]->setSelectedIndex(2);
   if ( pP->iActionQuickButton3 == quickActionPITMode )
      m_pItemsSelect[3]->setSelectedIndex(3);

   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
      m_pItemsSelect[3]->setEnabled(false);


   m_pItemsSelect[4] = new MenuItemSelect(L("Manual"), L("Enables or disables PIT mode (reduced Tx power mode) manually."));
   m_pItemsSelect[4]->addSelection(L("Disabled"));
   m_pItemsSelect[4]->addSelection(L("Enabled"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexPITModeManual = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL )
      m_pItemsSelect[4]->setSelectedIndex(1);

   if ( ! (g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
      m_pItemsSelect[4]->setEnabled(false);
}

void MenuVehicleRadioLinkPITModes::valuesToUI()
{
   addItems();
}

void MenuVehicleRadioLinkPITModes::Render()
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


void MenuVehicleRadioLinkPITModes::sendRadioInterfacesFlags()
{
   u32 uPITFlags = 0;

   if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
      uPITFlags |= RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE;
   if ( 1 == m_pItemsSelect[1]->getSelectedIndex() )
      uPITFlags |= RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_ARMDISARM;
   if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
      uPITFlags |= RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_TEMP;
   if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      uPITFlags |= RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL;

   u32 uCommandParam = uPITFlags;
   uCommandParam |= (((u32)(m_pItemsSlider[0]->getCurrentValue())) & 0xFF) << 8;
   if ( g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower )
      uCommandParam |= ((u32)(0x01))<<16;
   if ( g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower )
      uCommandParam |= ((u32)(0x01))<<17;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS, uCommandParam, NULL, 0) )
      valuesToUI();
}

void MenuVehicleRadioLinkPITModes::onSelectItem()
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

   if ( (m_IndexEnablePIT == m_SelectedIndex) || (m_IndexPITModeArm == m_SelectedIndex) ||
        (m_IndexPITModeTemp == m_SelectedIndex) || (m_IndexPITModeManual == m_SelectedIndex) ||
        (m_iIndexPITTemperature == m_SelectedIndex) )
      sendRadioInterfacesFlags();

   if ( m_IndexPITModeQAButton == m_SelectedIndex )
   {
      Preferences* pP = get_Preferences();

      if ( pP->iActionQuickButton1 == quickActionPITMode )
         pP->iActionQuickButton1 = quickActionNone;
      if ( pP->iActionQuickButton2 == quickActionPITMode )
         pP->iActionQuickButton2 = quickActionNone;
      if ( pP->iActionQuickButton3 == quickActionPITMode )
         pP->iActionQuickButton3 = quickActionNone;

      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         pP->iActionQuickButton1 = quickActionPITMode;
      if ( 2 == m_pItemsSelect[3]->getSelectedIndex() )
         pP->iActionQuickButton2 = quickActionPITMode;
      if ( 3 == m_pItemsSelect[3]->getSelectedIndex() )
         pP->iActionQuickButton3 = quickActionPITMode;
      save_Preferences();
   }
}