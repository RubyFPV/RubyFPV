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
#include "menu_controller_radio_interface_sik.h"
#include "menu_text.h"
#include "menu_tx_raw_power.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include "menu_item_text.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuControllerRadioInterfaceSiK::MenuControllerRadioInterfaceSiK(int iInterfaceIndex)
:Menu(MENU_ID_CONTROLLER_RADIO_INTERFACE_SIK, "Controller Radio Interface (SiK) Settings", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_pPopupProgress = NULL;
   m_iInterfaceIndex = iInterfaceIndex;
   
   load_ControllerInterfacesSettings();

   m_IndexName = -1;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      addTopLine("No radio interfaces detected on the system!");
      return;
   }

   m_pItemsSelect[2] = new MenuItemSelect("Enabled", "Enables or disables this card.");
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[6] = new MenuItemSelect("Card Location", "Marks this card as internal or external.");
   m_pItemsSelect[6]->addSelection("External");
   m_pItemsSelect[6]->addSelection("Internal");
   m_pItemsSelect[6]->setIsEditable();
   m_IndexInternal = addMenuItem(m_pItemsSelect[6]);

   m_pItemsEdit[0] = new MenuItemEdit("Custom Name", "Sets a custom name for this unique physical radio interface; This name is to be displayed in the OSD and menus, to easily identify and distinguish each physical radio interface from the others.", "");
   m_pItemsEdit[0]->setMaxLength(48);
   m_IndexName = addMenuItem(m_pItemsEdit[0]);
}

void MenuControllerRadioInterfaceSiK::valuesToUI()
{
   if ( 0 == hardware_get_radio_interfaces_count() || m_iInterfaceIndex < 0 || m_iInterfaceIndex >= hardware_get_radio_interfaces_count() )
       return;

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
      
   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      
   if ( NULL == pCardInfo )
      return;

   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[2]->setSelection(1);
   if ( controllerIsCardDisabled(pNIC->szMAC) )
      m_pItemsSelect[2]->setSelection(0);

   if ( 1 == hardware_get_radio_interfaces_count() )
   {
      m_pItemsSelect[2]->setSelection(1);
      m_pItemsSelect[2]->setEnabled(false);
      controllerRemoveCardDisabled(pNIC->szMAC);
      controllerSetCardTXRX(pNIC->szMAC);
   }

   m_pItemsSelect[6]->setSelectedIndex(controllerIsCardInternal(pNIC->szMAC));

   if ( NULL != m_pItemsEdit[0] )
      m_pItemsEdit[0]->setCurrentValue(controllerGetCardUserDefinedName(pNIC->szMAC));
}


void MenuControllerRadioInterfaceSiK::onShow()
{
   Menu::onShow();
}

void MenuControllerRadioInterfaceSiK::Render()
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

void MenuControllerRadioInterfaceSiK::showProgressInfo()
{
   ruby_pause_watchdog("controller update sik radio interface progress");
   m_pPopupProgress = new Popup("Updating SiK Radio Configuration. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(m_pPopupProgress);

   g_pRenderEngine->startFrame();
   render_background_and_paddings(false);
   popups_render();
   popups_render_topmost();
   g_pRenderEngine->endFrame();

}

void MenuControllerRadioInterfaceSiK::hideProgressInfo()
{
   popups_remove(m_pPopupProgress);
   m_pPopupProgress = NULL;
}

bool MenuControllerRadioInterfaceSiK::checkFlagsConsistency()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   int enabled = m_pItemsSelect[2]->getSelectedIndex();

   if ( 0 == enabled )
   {
      bool anyEnabled = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( i != m_iInterfaceIndex )
      {
         radio_hw_info_t* pNIC2 = hardware_get_radio_info(i);
         if ( NULL == pNIC2 )
            break;
         u32 flags = controllerGetCardFlags(pNIC2->szMAC);
         if ( !(flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         {
            anyEnabled = true;
            break;
         }
      }
      if ( ! anyEnabled )
      {
         addMessage("You can't disable all the radio interfaces!");
         return false;
      }
   }
   return true;
}

bool MenuControllerRadioInterfaceSiK::setCardFlags()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   if ( ! checkFlagsConsistency() )
      return false;

   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   int enabled = m_pItemsSelect[2]->getSelectedIndex();
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

   cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   controllerSetCardTXRX(pNIC->szMAC);
   
   cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   
   controllerSetCardFlags(pNIC->szMAC, cardFlags);
   save_ControllerInterfacesSettings();
   return true;
}

int MenuControllerRadioInterfaceSiK::onBack()
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


void MenuControllerRadioInterfaceSiK::onSelectItem()
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

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return;

   if ( m_IndexEnabled == m_SelectedIndex )
   {
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
      controllerSetCardInternal(pNIC->szMAC, (bool)m_pItemsSelect[6]->getSelectedIndex());
      save_ControllerInterfacesSettings();
      valuesToUI();
      return;
   }
}
