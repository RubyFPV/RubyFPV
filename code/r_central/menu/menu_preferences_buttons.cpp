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
#include "menu_preferences_buttons.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "../popup_log.h"
#include "osd_common.h"
#include "menu_preferences_ui.h"

MenuButtons::MenuButtons(void)
:Menu(MENU_ID_PREFERENCES_BUTTONS, L("User Interface Buttons"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   int c = 0;

   m_pItemsSelect[c] = new MenuItemSelect(L("Swap +/- Buttons"), L("Swaps the function of + and - buttons."));
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Swap +/- Edit Buttons"), L("Swaps the function of + and - buttons for editing values in UI."));
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Quick Action Button 1"), L("Change what happens when you press the quick action button 1."));
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD screen"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD size"));
   m_pItemsSelect[c]->addSelection(L("Take Picture"));
   m_pItemsSelect[c]->addSelection(L("Video Recording"));
   m_pItemsSelect[c]->addSelection(L("Toggle OSD Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle Stats Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle All Off"));
   m_pItemsSelect[c]->addSelection(L("Relay Switch"));
   m_pItemsSelect[c]->addSelection(L("Switch Camera Profile"));
   m_pItemsSelect[c]->addSelection(L("RC Output On/Off"));
   m_pItemsSelect[c]->addSelection(L("Rotary Encoder Function"));
   m_pItemsSelect[c]->addSelection(L("Freeze OSD"));
   m_pItemsSelect[c]->addSelection(L("Cycle Favorite Vehicles"));
   m_pItemsSelect[c]->addSelection(L("PIT Mode"));
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Quick Action Button 2"), L("Change what happens when you press the quick action button 2."));
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD screen"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD size"));
   m_pItemsSelect[c]->addSelection(L("Take Picture"));
   m_pItemsSelect[c]->addSelection(L("Video Recording"));
   m_pItemsSelect[c]->addSelection(L("Toggle OSD Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle Stats Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle All Off"));
   m_pItemsSelect[c]->addSelection(L("Relay Switch"));
   m_pItemsSelect[c]->addSelection(L("Switch Camera Profile"));
   m_pItemsSelect[c]->addSelection(L("RC Output On/Off"));
   m_pItemsSelect[c]->addSelection(L("Rotary Encoder Function"));
   m_pItemsSelect[c]->addSelection(L("Freeze OSD"));
   m_pItemsSelect[c]->addSelection(L("Cycle Favorite Vehicles"));
   m_pItemsSelect[c]->addSelection(L("PIT Mode"));
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Quick Action Button 3"), L("Change what happens when you press the quick action button 3."));
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD screen"));
   m_pItemsSelect[c]->addSelection(L("Cycle OSD size"));
   m_pItemsSelect[c]->addSelection(L("Take Picture"));
   m_pItemsSelect[c]->addSelection(L("Video Recording"));
   m_pItemsSelect[c]->addSelection(L("Toggle OSD Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle Stats Off"));
   m_pItemsSelect[c]->addSelection(L("Toggle All Off"));
   m_pItemsSelect[c]->addSelection(L("Relay Switch"));
   m_pItemsSelect[c]->addSelection(L("Switch Camera Profile"));
   m_pItemsSelect[c]->addSelection(L("RC Output On/Off"));
   m_pItemsSelect[c]->addSelection(L("Rotary Encoder Function"));
   m_pItemsSelect[c]->addSelection(L("Freeze OSD"));
   m_pItemsSelect[c]->addSelection(L("Cycle Favorite Vehicles"));
   m_pItemsSelect[c]->addSelection(L("PIT Mode"));
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Rotary Encoder Function"), L("Sets the function of the optional rotary encoder. Long press the rotary encoder for 10 seconds to revert to Menu function."));
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("Menu Navigation"));
   m_pItemsSelect[c]->addSelection(L("Camera Adjustment"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotaryEncoder = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Rotary Encoder Speed"), L("Sets the speed of the optional rotary encoder."));
   m_pItemsSelect[c]->addSelection(L("Normal"));
   m_pItemsSelect[c]->addSelection(L("Slower"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotarySpeed = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Rotary Encoder 2 Function"), L("Sets the function of the secondary optional rotary encoder. Long press the secondary rotary encoder for 10 seconds to revert to Menu function."));
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("Menu Navigation"));
   m_pItemsSelect[c]->addSelection(L("Camera Adjustment"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotaryEncoder2 = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Rotary Encoder 2 Speed"), L("Sets the speed of the secondary optional rotary encoder."));
   m_pItemsSelect[c]->addSelection(L("Normal"));
   m_pItemsSelect[c]->addSelection(L("Slower"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotarySpeed2 = addMenuItem(m_pItemsSelect[c]);
   c++;
}

void MenuButtons::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   m_pItemsSelect[0]->setSelection(p->iSwapUpDownButtons);
   m_pItemsSelect[1]->setSelection(p->iSwapUpDownButtonsValues);
   m_pItemsSelect[2]->setSelection(p->iActionQuickButton1);
   m_pItemsSelect[3]->setSelection(p->iActionQuickButton2);
   m_pItemsSelect[4]->setSelection(p->iActionQuickButton3);

   m_pItemsSelect[5]->setSelection(pCS->nRotaryEncoderFunction);
   m_pItemsSelect[6]->setSelection(pCS->nRotaryEncoderSpeed);

   m_pItemsSelect[7]->setSelection(pCS->nRotaryEncoderFunction2);
   m_pItemsSelect[8]->setSelection(pCS->nRotaryEncoderSpeed2);
}

void MenuButtons::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuButtons::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuButtons::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      p->iSwapUpDownButtons = m_pItemsSelect[0]->getSelectedIndex();
      hardware_swap_buttons(p->iSwapUpDownButtons);
   }
   if ( 1 == m_SelectedIndex )
   {
      p->iSwapUpDownButtonsValues = m_pItemsSelect[1]->getSelectedIndex();
      hardware_swap_buttons(p->iSwapUpDownButtons);
   }

   if ( 2 == m_SelectedIndex )
   {
      p->iActionQuickButton1 = m_pItemsSelect[2]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton1 == quickActionRCEnable )
      {
         Popup* p = new Popup( L("Enabling/disabling RC link during flight is dangerous. Proceed with caution."), 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
      g_bFreezeOSD = false;
   }

   if ( 3 == m_SelectedIndex )
   {
      p->iActionQuickButton2 = m_pItemsSelect[3]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton2 == quickActionRCEnable )
      {
         Popup* p = new Popup(L("Enabling/disabling RC link during flight is dangerous. Proceed with caution."), 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
      g_bFreezeOSD = false;
   }

   if ( 4 == m_SelectedIndex )
   {
      p->iActionQuickButton3 = m_pItemsSelect[4]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton3 == quickActionRCEnable )
      {
         Popup* p = new Popup(L("Enabling/disabling RC link during flight is dangerous. Proceed with caution."), 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
      g_bFreezeOSD = false;
   }

   if ( m_IndexRotaryEncoder == m_SelectedIndex )
   {
      pCS->nRotaryEncoderFunction = m_pItemsSelect[5]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexRotarySpeed == m_SelectedIndex )
   {
      pCS->nRotaryEncoderSpeed = m_pItemsSelect[6]->getSelectedIndex();
      save_ControllerSettings();
      char szBuff[128];
      sprintf(szBuff, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_I2C_UPDATED);
      hw_execute_bash_command_silent(szBuff, NULL);
      valuesToUI();
      return;
   }

   if ( m_IndexRotaryEncoder2 == m_SelectedIndex )
   {
      pCS->nRotaryEncoderFunction2 = m_pItemsSelect[7]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexRotarySpeed2 == m_SelectedIndex )
   {
      pCS->nRotaryEncoderSpeed2 = m_pItemsSelect[8]->getSelectedIndex();
      save_ControllerSettings();
      char szBuff[128];
      sprintf(szBuff, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_I2C_UPDATED);
      hw_execute_bash_command_silent(szBuff, NULL);
      valuesToUI();
      return;
   }

   save_Preferences();
   hardware_swap_buttons(p->iSwapUpDownButtons);
}
