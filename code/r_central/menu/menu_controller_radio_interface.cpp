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
#include "menu_objects.h"
#include "menu_controller_radio_interface.h"
#include "menu_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "../../base/tx_powers.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuControllerRadioInterface::MenuControllerRadioInterface(int iInterfaceIndex)
:Menu(MENU_ID_CONTROLLER_RADIO_INTERFACE, L("Controller Radio Interface Settings"), NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_pPopupProgress = NULL;
   m_iInterfaceIndex = iInterfaceIndex;
   
   load_ControllerInterfacesSettings();

   char szBuff[1024];

   sprintf(szBuff, L("Controller Radio Interface %d Settings"), iInterfaceIndex+1);
   setTitle(szBuff);
   
   m_IndexName = -1;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      addTopLine(L("No radio interfaces detected on the system!"));
      return;
   }

   m_pItemsSelect[0] = createMenuItemCardModelSelector(L("Card Model"));
   m_IndexCardModel = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Enabled"), L("Enables or disables this card."));
   m_pItemsSelect[1]->addSelection(L("No"));
   m_pItemsSelect[1]->addSelection(L("Yes"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexEnabled = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Preferred Uplink Card"), L("Sets this card as preffered one for uplink."));
   m_pItemsSelect[2]->addSelection(L("No"));
   m_pItemsSelect[2]->addSelection(L("Yes"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexTXPreferred = addMenuItem(m_pItemsSelect[2]);

   if ( 1 == hardware_get_radio_interfaces_count() )
      addMenuItem(new MenuItemText(L("You have a single radio interface on this controller. You can not change it's main functionality.")));

   m_pItemsSelect[3] = new MenuItemSelect(L("Capabilities"), L("Sets the uplink/downlink capabilities of the card. If the card has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, so it must be marked to be used only for uplink or downlink accordingly."));
   m_pItemsSelect[3]->addSelection(L("Uplink & Downlink"));
   m_pItemsSelect[3]->addSelection(L("Downlink Only"));
   m_pItemsSelect[3]->addSelection(L("Uplink Only"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexCapabilities = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Card Location"), L("Marks this card as internal or external."));
   m_pItemsSelect[4]->addSelection(L("External"));
   m_pItemsSelect[4]->addSelection(L("Internal"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexInternal = addMenuItem(m_pItemsSelect[4]);

   m_pItemsEdit[0] = new MenuItemEdit(L("Custom Name"), L("Sets a custom name for this unique physical radio interface; This name is to be displayed in the OSD and menus, to easily identify and distinguish each physical radio interface from the others."), "");
   m_pItemsEdit[0]->setMaxLength(48);
   m_IndexName = addMenuItem(m_pItemsEdit[0]);
}

void MenuControllerRadioInterface::valuesToUI()
{
   if ( (0 == hardware_get_radio_interfaces_count()) || (m_iInterfaceIndex < 0) || (m_iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
       return;

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
      
   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      
   if ( NULL == pCardInfo )
      return;

   int iRadioLinkId = g_SM_RadioStats.radio_interfaces[m_iInterfaceIndex].assignedLocalRadioLinkId;
   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   setSelectionForCardModelSelector(m_pItemsSelect[0], pCardInfo->cardModel );

   if ( NULL != m_pItemsEdit[0] )
      m_pItemsEdit[0]->setCurrentValue(controllerGetCardUserDefinedName(pNIC->szMAC));


   // Enable/disable

   m_pItemsSelect[1]->setEnabled(true);
   if ( controllerIsCardDisabled(pNIC->szMAC) )
       m_pItemsSelect[1]->setSelection(0);
   else
       m_pItemsSelect[1]->setSelection(1);

   // Preffered tx card

   if ( 1 == hardware_get_radio_interfaces_count() )
   {
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[2]->setSelection(1);
   }
   else if ( controllerIsCardDisabled(pNIC->szMAC) || ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) ) )
   {
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[2]->setSelection(0);
   }
   else
   {
      m_pItemsSelect[2]->setEnabled(true);
      int iCurrentPrefferedIndex = controllerIsCardTXPreferred(pNIC->szMAC);
      log_line("Current card index %d: preffered Tx index: %d, on radio link %d.", m_iInterfaceIndex+1, iCurrentPrefferedIndex, iRadioLinkId+1 );
      int iMinPrefferedIndex = 10000;
      int iMinCardIndex = -1;
      if ( iRadioLinkId >= 0 )
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != iRadioLinkId )
            continue;

         radio_hw_info_t* pNICInfo2 = hardware_get_radio_info(i);
         if ( NULL == pNICInfo2 )
            continue;

         int iCardPriority = controllerIsCardTXPreferred(pNICInfo2->szMAC);
         if ( iCardPriority <= 0 || iCardPriority > iMinPrefferedIndex )
            continue;
         log_line("Card index %d preffered Tx index: %d", i+1, iCardPriority);
         iMinPrefferedIndex = iCardPriority;
         iMinCardIndex = i;
      }

      if (! controllerIsCardDisabled(pNIC->szMAC) )
      if ( iRadioLinkId >= 0 )
      if ( iCurrentPrefferedIndex > 0 )
      if ( iMinCardIndex == m_iInterfaceIndex )
      {
         m_pItemsSelect[2]->setSelectedIndex(1);
         log_line("Current card is preffered Tx card.");
      }
   }

   // Capabilities

   if ( controllerIsCardDisabled(pNIC->szMAC) || (1 == hardware_get_radio_interfaces_count()) )
      m_pItemsSelect[3]->setEnabled(false);
   else
      m_pItemsSelect[3]->setEnabled(true);

   m_pItemsSelect[3]->setSelection(0);
   if ( controllerIsCardRXOnly(pNIC->szMAC) )
      m_pItemsSelect[3]->setSelection(1);
   if ( controllerIsCardTXOnly(pNIC->szMAC) )
      m_pItemsSelect[3]->setSelection(2);

   // Card location

   m_pItemsSelect[4]->setSelectedIndex(controllerIsCardInternal(pNIC->szMAC));
}


void MenuControllerRadioInterface::onShow()
{
   Menu::onShow();
}

void MenuControllerRadioInterface::Render()
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

void MenuControllerRadioInterface::showProgressInfo()
{
   ruby_pause_watchdog("controller update radio interface progress");
   m_pPopupProgress = new Popup(L("Updating Radio Configuration. Please wait..."), 0.3,0.4, 0.5, 15);
   popups_add_topmost(m_pPopupProgress);

   g_pRenderEngine->startFrame();
   render_background_and_paddings(false);
   popups_render();
   popups_render_topmost();
   g_pRenderEngine->endFrame();

}

void MenuControllerRadioInterface::hideProgressInfo()
{
   popups_remove(m_pPopupProgress);
   m_pPopupProgress = NULL;
}

bool MenuControllerRadioInterface::checkFlagsConsistency()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   int iCountInterfacesNowEnabled = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( ! controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         iCountInterfacesNowEnabled++;
   }

   int enabled = m_pItemsSelect[1]->getSelectedIndex();

   if ( 0 == enabled )
   {
      
      if ( iCountInterfacesNowEnabled < 2 )
      {
         addMessage(L("You can't disable all the radio interfaces!"));
         return false;
      }
   }
   return true;
}

bool MenuControllerRadioInterface::setCardFlags()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   if ( ! checkFlagsConsistency() )
      return false;

   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   int enabled = m_pItemsSelect[1]->getSelectedIndex();
   if ( 1 == enabled )
   {
      controllerRemoveCardDisabled(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   }
   else
   {
      controllerSetCardDisabled(pNIC->szMAC);
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_DISABLED;
   }

   int indexCapabilities = m_pItemsSelect[3]->getSelectedIndex();

   cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;

   if ( 0 == indexCapabilities )
      controllerSetCardTXRX(pNIC->szMAC);
   if ( 1 == indexCapabilities )
   {
      controllerSetCardRXOnly(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   }
   if ( 2 == indexCapabilities )
   {
      controllerSetCardTXOnly(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_RX);
   }


   cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   
   controllerSetCardFlags(pNIC->szMAC, cardFlags);
   save_ControllerInterfacesSettings();
   return true;
}

int MenuControllerRadioInterface::onBack()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL != pNIC )
   if ( m_IndexName == m_SelectedIndex )
   if ( m_pItemsEdit[0]->isEditing() )
   {
      m_pItemsEdit[0]->endEdit(false);
      char szBuff[1024];
      strcpy(szBuff, m_pItemsEdit[0]->getCurrentValue());
      controllerSetCardUserDefinedName(pNIC->szMAC, szBuff);
      save_ControllerInterfacesSettings();
      invalidate();
      valuesToUI();
      return 1;
   }

   return Menu::onBack();
}


void MenuControllerRadioInterface::onSelectItem()
{
   if ( m_IndexName == m_SelectedIndex )
   {
      m_pItemsEdit[0]->beginEdit();
      return;
   }

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return;

   int iCountInterfacesNowEnabled = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( ! controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         iCountInterfacesNowEnabled++;
   }

   if ( m_IndexCardModel == m_SelectedIndex )
   {
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      if ( NULL == pCardInfo )
         return;

      u32 uCardModel = getSelectedCardForCardModelSelector(m_pItemsSelect[0]);

      if ( 0xFF == uCardModel )
      {
         pCardInfo->cardModel = pNIC->iCardModel;
         char szMsg[128];
         sprintf(szMsg, "The radio interface was autodetected as: %s", str_get_radio_card_model_string(pCardInfo->cardModel));
         addMessage2(0, szMsg, L("The selected value was updated."));
      }
      else
         pCardInfo->cardModel = -(int)uCardModel;

      save_ControllerInterfacesSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexEnabled == m_SelectedIndex )
   {
      if ( iCountInterfacesNowEnabled < 2 )
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
      {
         m_pItemsSelect[1]->setSelectedIndex(1);
         addMessage(L("You can't disable the single active radio interface."));
         return;
      }

      if ( setCardFlags() )
      {
         showProgressInfo();
         pairing_stop();
         pairing_start_normal();
         hideProgressInfo();
      }
      else
         valuesToUI();
      return;
   }

   if ( m_IndexCapabilities == m_SelectedIndex )
   {
      if ( iCountInterfacesNowEnabled < 2 )
      {
            Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Invalid option"), NULL);
            pm->m_xPos = 0.4; pm->m_yPos = 0.4;
            pm->m_Width = 0.36;
            pm->addTopLine( L("Can't set the interface as RX or TX only because it's the only active interface on the system."));
            add_menu_to_stack(pm);
            valuesToUI();
            return;
      }

      if ( setCardFlags() )
      {
         showProgressInfo();
         pairing_stop();
         pairing_start_normal();
         hideProgressInfo();
      }
      else
         valuesToUI();  
      return;
   }

   if ( m_IndexInternal == m_SelectedIndex )
   {
      controllerSetCardInternal(pNIC->szMAC, (bool)m_pItemsSelect[4]->getSelectedIndex());
      save_ControllerInterfacesSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexTXPreferred == m_SelectedIndex )
   {
      int index = m_pItemsSelect[2]->getSelectedIndex();
      if ( 0 == index )
      {
         controllerRemoveCardTXPreferred(pNIC->szMAC);
      }
      if ( 1 == index )
      {
         if ( controllerIsCardDisabled(pNIC->szMAC) )
            return;
         controllerSetCardTXPreferred(pNIC->szMAC);
      }
      valuesToUI();
      showProgressInfo();
      pairing_stop();
      pairing_start_normal();
      hideProgressInfo();
      return;
   }
}
