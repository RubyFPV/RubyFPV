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
#include "menu_controller_radio_interfaces.h"
#include "menu_text.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_controller_radio_interface.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuControllerRadioInterfaces::MenuControllerRadioInterfaces(void)
:Menu(MENU_ID_CONTROLLER_RADIO_INTERFACES, "Controller Radio Interfaces Settings", NULL)
{
   m_Width = 0.4;
   m_xPos = 0.08;
   m_yPos = 0.1;

   load_ControllerInterfacesSettings();

   char szBuff[256];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_IndexStartNics[i] = -1;
   }

   for( int i=0; i<20; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_pItemsSlider[i] = NULL;
      m_pItemsEdit[i] = NULL;
   }

   m_IndexTXInfo = addMenuItem(new MenuItem("Uplink Power Levels"));
   m_pMenuItems[m_IndexTXInfo]->showArrow();

   m_pItemsSelect[0] = new MenuItemSelect("Auto Uplink Card", "Automatically choose the best uplink card based on signal quality.");
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexAutoTx = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Preferred Uplink Card", "Sets the card that is preferred one to be used for uplink (only if there are multiple radio interfaces on the controller).");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexTXPreferred = addMenuItem(m_pItemsSelect[1]);

   m_IndexCurrentTXPreferred = -1;

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);

      char szName[128];
      strcpy(szName, "NoName");

      char* szCardName = controllerGetCardUserDefinedName(pNIC->szMAC);
      if ( NULL != szCardName && 0 != szCardName[0] )
         strcpy(szName, szCardName);
      else if ( NULL != pCardInfo )
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );

      int iRadioLinkId = -1;
      iRadioLinkId = g_SM_RadioStats.radio_interfaces[n].assignedLocalRadioLinkId;
      if ( iRadioLinkId >= 0 )
         sprintf(szBuff, "Int. %d: Radio Link %d, Port %s, %s", n+1, iRadioLinkId+1, pNIC->szUSBPort, szName);
      else
         sprintf(szBuff, "Int. %d: Port %s, %s", n+1, pNIC->szUSBPort, szName);
      m_IndexStartNics[n] = addMenuItem(new MenuItem(szBuff));
      m_pMenuItems[m_IndexStartNics[n]]->showArrow();

      if ( 1 == hardware_get_radio_interfaces_count() )
         addMenuItem(new MenuItemText("You have a single radio interface on this controller. You can not change it's main functionality."));
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
      addTopLine("No radio interfaces detected on the system!");
}

void MenuControllerRadioInterfaces::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   m_pItemsSelect[0]->setSelectedIndex(pCS->nAutomaticTxCard);
   m_pItemsSelect[1]->setSelection(0);

   m_IndexCurrentTXPreferred = -1;
   bool bFoundPreferred = false;

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);

      if ( 1 == hardware_get_radio_interfaces_count() )
      {
         controllerRemoveCardDisabled(pNIC->szMAC);
         controllerSetCardTXRX(pNIC->szMAC);
      }

      if ( ! bFoundPreferred )
      if ( controllerIsCardTXPreferred(pNIC->szMAC) )
      {
         m_IndexCurrentTXPreferred = n;
         bFoundPreferred = true;
      }
   }

   m_pItemsSelect[1]->removeAllSelections();

   if ( pCS->nAutomaticTxCard )
   {
      m_pItemsSelect[1]->addSelection("Auto");
      m_pItemsSelect[1]->setSelection(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pMenuItems[m_IndexTXPreferred]->setEnabled(false);
      return;
   }

   m_pMenuItems[m_IndexTXPreferred]->setEnabled(true);
   m_pItemsSelect[1]->addSelection("None");

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);
      if ( controllerIsCardRXOnly(pNIC->szMAC) )
         continue;
      if ( controllerIsCardDisabled(pNIC->szMAC) )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);

      char szName[128];
      strcpy(szName, "NoName");

      char* szCardName = controllerGetCardUserDefinedName(pNIC->szMAC);
      if ( NULL != szCardName && 0 != szCardName[0] )
         strcpy(szName, szCardName);
      else if ( NULL != pCardInfo )
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );

      char szBuff[256];
      int iRadioLinkId = -1;
      iRadioLinkId = g_SM_RadioStats.radio_interfaces[n].assignedLocalRadioLinkId;
      if ( iRadioLinkId >= 0 )
         sprintf(szBuff, "%d: Link %d, Port %s, %s", n+1, iRadioLinkId+1, pNIC->szUSBPort, szName);
      else
         sprintf(szBuff, "%d: Port %s, %s", n+1, pNIC->szUSBPort, szName);
      m_pItemsSelect[1]->addSelection(szBuff);
      if ( m_IndexCurrentTXPreferred == n )
         m_pItemsSelect[1]->setSelection(m_pItemsSelect[1]->getSelectionsCount()-1);
   }
   m_pItemsSelect[1]->setIsEditable();

   if ( 1 == m_pItemsSelect[1]->getSelectionsCount() )
   {
      m_IndexCurrentTXPreferred = -1;
      m_pItemsSelect[1]->removeAllSelections();
      m_pItemsSelect[1]->addSelection("No cards are enabled for uplink!");
      m_pItemsSelect[1]->setSelection(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pMenuItems[m_IndexTXPreferred]->setEnabled(false);
   }
}


void MenuControllerRadioInterfaces::onShow()
{
   Menu::onShow();
}

void MenuControllerRadioInterfaces::Render()
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


void MenuControllerRadioInterfaces::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      if ( m_IndexStartNics[n] == m_SelectedIndex )
      {
         MenuControllerRadioInterface* pMenu = new MenuControllerRadioInterface(n);
         add_menu_to_stack(pMenu);
         return;
      }
   }

   if ( m_IndexAutoTx == m_SelectedIndex )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nAutomaticTxCard = m_pItemsSelect[0]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      pairing_stop();
      pairing_start_normal();
      return;
   }

   if ( m_IndexTXPreferred == m_SelectedIndex )
   {
      if ( -1 != m_IndexCurrentTXPreferred )
      {
         radio_hw_info_t* pNICOld = hardware_get_radio_info(m_IndexCurrentTXPreferred);
         controllerRemoveCardTXPreferred(pNICOld->szMAC);
      }
      int index = m_pItemsSelect[1]->getSelectedIndex();
      if ( 0 == index )
         return;

      m_IndexCurrentTXPreferred = index-1;
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(index-1);
      controllerSetCardTXPreferred(pNICInfo->szMAC);
      save_ControllerInterfacesSettings();
      valuesToUI();
      pairing_stop();
      pairing_start_normal();
      return;
   }

   if ( m_IndexTXInfo == m_SelectedIndex )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowVehicle = false;
      add_menu_to_stack(pMenu);
      return;
   }
}
