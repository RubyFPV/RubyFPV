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
#include "menu_quick_menu.h"
#include "menu_quick_menu_settings.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuQuickMenuSettings::MenuQuickMenuSettings(void)
:Menu(MENU_ID_QUICK_MENU_SETTINGS, L("Quick Menu Settings"), NULL)
{
   m_Width = 0.42;
   m_yPos = 0.28;
   m_xPos = menu_get_XStartPos(m_Width);

   m_pItemsSelect[0] = new MenuItemSelect(L("Quick Menu"), L("Enable,  Disable or Customize which quick-actions are displayed in the Quick Menu. Customization can start from all disabled and activate a few, or all enabled and deactivate a few"));
   m_pItemsSelect[0]->addSelection(L("Disabled"));
   m_pItemsSelect[0]->addSelection(L("Enabled"));
   m_pItemsSelect[0]->addSelection(L("Custom"));
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexAllQuickMenu = addMenuItem( m_pItemsSelect[0]);

   addSeparator();

   m_pItemsSelect[3] = new MenuItemSelect(L("Take Picture"), L("Take Picture."));
   m_pItemsSelect[3]->addSelection(L("Disabled"));
   m_pItemsSelect[3]->addSelection(L("Enabled"));
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexTakePicture = addMenuItem( m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Video Recording"), L("Toggle Video Recording on/off."));
   m_pItemsSelect[4]->addSelection(L("Disabled"));
   m_pItemsSelect[4]->addSelection(L("Enabled"));
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexVideoRecording = addMenuItem( m_pItemsSelect[4]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Cycle OSD screen"), L("Cycle OSD screen profile."));
   m_pItemsSelect[1]->addSelection(L("Disabled"));
   m_pItemsSelect[1]->addSelection(L("Enabled"));
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexCycleOSDScreen = addMenuItem( m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Cycle OSD size"), L("Cycle OSD size."));
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexCycleOSDSize = addMenuItem( m_pItemsSelect[2]);

   m_pItemsSelect[5] = new MenuItemSelect(L("Toggle OSD"), L("Toggle OSD on/off."));
   m_pItemsSelect[5]->addSelection(L("Disabled"));
   m_pItemsSelect[5]->addSelection(L("Enabled"));
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexToggleOSD = addMenuItem( m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Toggle Stats"), L("Toggle the Statistics on/off."));
   m_pItemsSelect[6]->addSelection(L("Disabled"));
   m_pItemsSelect[6]->addSelection(L("Enabled"));
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexToggleStatsOff = addMenuItem( m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Toggle All"), L("Toggle All OSD and Statistics on/off."));
   m_pItemsSelect[7]->addSelection(L("Disabled"));
   m_pItemsSelect[7]->addSelection(L("Enabled"));
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexToggleAllOff = addMenuItem( m_pItemsSelect[7]);

   m_pItemsSelect[8] = new MenuItemSelect(L("Relay Switch"), L("Relay Switch."));
   m_pItemsSelect[8]->addSelection(L("Disabled"));
   m_pItemsSelect[8]->addSelection(L("Enabled"));
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexRelaySwitch = addMenuItem( m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect(L("Switch Camera Profile"), L("Switch Camera Profile."));
   m_pItemsSelect[9]->addSelection(L("Disabled"));
   m_pItemsSelect[9]->addSelection(L("Enabled"));
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexSwitchCameraProfile = addMenuItem( m_pItemsSelect[9]);

    #ifdef FEATURE_ENABLE_RC
   m_pItemsSelect[10] = new MenuItemSelect(L("RC Output On/Off"), L("Toggle RC Output on/off."));
   m_pItemsSelect[10]->addSelection(L("Disabled"));
   m_pItemsSelect[10]->addSelection(L("Enabled"));
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexRCOutputOnOff = addMenuItem( m_pItemsSelect[10]);
   #endif

   m_pItemsSelect[11] = new MenuItemSelect(L("Rotary Encoder Function"), L("Rotary Encoder Function."));
   m_pItemsSelect[11]->addSelection(L("Disabled"));
   m_pItemsSelect[11]->addSelection(L("Enabled"));
   m_pItemsSelect[11]->setUseMultiViewLayout();
   m_IndexRotaryEncoderFunction = addMenuItem( m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect(L("Freeze OSD"), L("Freeze OSD."));
   m_pItemsSelect[12]->addSelection(L("Disabled"));
   m_pItemsSelect[12]->addSelection(L("Enabled"));
   m_pItemsSelect[12]->setUseMultiViewLayout();
   m_IndexFreezeOSD = addMenuItem( m_pItemsSelect[12]);

   m_pItemsSelect[13] = new MenuItemSelect(L("Cycle Favorite Vehicles"), L("Cycle Favorite Vehicles."));
   m_pItemsSelect[13]->addSelection(L("Disabled"));
   m_pItemsSelect[13]->addSelection(L("Enabled"));
   m_pItemsSelect[13]->setUseMultiViewLayout();
   m_IndexCycleFavoriteVehicles = addMenuItem( m_pItemsSelect[13]);

   m_pItemsSelect[14] = new MenuItemSelect(L("PIT Mode"), L("PIT Mode enable/disable."));
   m_pItemsSelect[14]->addSelection(L("Disabled"));
   m_pItemsSelect[14]->addSelection(L("Enabled"));
   m_pItemsSelect[14]->setUseMultiViewLayout();
   m_IndexPITMode = addMenuItem( m_pItemsSelect[14]);

   Preferences* pP = get_Preferences();

   m_bMenuQuickMenuSettingsIsOnCustomOption = true;
   if ( (pP->uEnabledQuickMenu == 0xFFFFFFFF) || (pP->uEnabledQuickMenu == 0) )
      m_bMenuQuickMenuSettingsIsOnCustomOption = false;

}

void MenuQuickMenuSettings::valuesToUI()
{
   
   Preferences* pP = get_Preferences();

   if ( m_bMenuQuickMenuSettingsIsOnCustomOption )
      m_pItemsSelect[0]->setSelectedIndex(2);
   else if ( 0 == pP->uEnabledQuickMenu )
      m_pItemsSelect[0]->setSelectedIndex(0);
   else if ( 0xFFFFFFFF == pP->uEnabledQuickMenu )
      m_pItemsSelect[0]->setSelectedIndex(1);
   else
      m_pItemsSelect[0]->setSelectedIndex(2);

   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[1]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDScreen)?1:0);

   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[2]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDSize)?1:0);

   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[3]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::TakePicture)?1:0);

   m_pItemsSelect[4]->setEnabled(true);
   m_pItemsSelect[4]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::VideoRecording)?1:0);

   m_pItemsSelect[5]->setEnabled(true);
   m_pItemsSelect[5]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::ToggleOSD)?1:0);

   m_pItemsSelect[6]->setEnabled(true);
   m_pItemsSelect[6]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::ToggleStatsOff)?1:0);

   m_pItemsSelect[7]->setEnabled(true);
   m_pItemsSelect[7]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::ToggleAllOff)?1:0);

   m_pItemsSelect[8]->setEnabled(true);
   m_pItemsSelect[8]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::RelaySwitch)?1:0);

   m_pItemsSelect[9]->setEnabled(true);
   m_pItemsSelect[9]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::SwitchCameraProfile)?1:0);

   m_pItemsSelect[10]->setEnabled(true);
   m_pItemsSelect[10]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::RCOutputOnOff)?1:0);

   m_pItemsSelect[11]->setEnabled(true);
   m_pItemsSelect[11]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::RotaryEncoderFunction)?1:0);

   m_pItemsSelect[12]->setEnabled(true);
   m_pItemsSelect[12]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::FreezeOSD)?1:0);

   m_pItemsSelect[13]->setEnabled(true);
   m_pItemsSelect[13]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::CycleFavoriteVehicles)?1:0);

   m_pItemsSelect[14]->setEnabled(true);
   m_pItemsSelect[14]->setSelectedIndex((pP->uEnabledQuickMenu & MenuQuickMenu::PITMode)?1:0);


   if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
   {
      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSelect[3]->setSelectedIndex(0);
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSelect[5]->setSelectedIndex(0);
      m_pItemsSelect[6]->setSelectedIndex(0);
      m_pItemsSelect[7]->setSelectedIndex(0);
      m_pItemsSelect[8]->setSelectedIndex(0);
      m_pItemsSelect[9]->setSelectedIndex(0);
      m_pItemsSelect[10]->setSelectedIndex(0);
      m_pItemsSelect[11]->setSelectedIndex(0);
      m_pItemsSelect[12]->setSelectedIndex(0);
      m_pItemsSelect[13]->setSelectedIndex(0);
      m_pItemsSelect[14]->setSelectedIndex(0);
   }
}

void MenuQuickMenuSettings::onShow()
{
   Menu::onShow();
}


void MenuQuickMenuSettings::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);
}

void MenuQuickMenuSettings::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   Preferences* pP = get_Preferences();

   if ( m_IndexAllQuickMenu == m_SelectedIndex )
   {
      m_bMenuQuickMenuSettingsIsOnCustomOption = false;
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledQuickMenu = 0;
      else if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledQuickMenu = 0xFFFFFFFF;
      else
        m_bMenuQuickMenuSettingsIsOnCustomOption = true;
   }

   MenuQuickMenu::t_quick_menu_actions action = MenuQuickMenu::None;
   int selectionState = 0;

   if ( m_IndexCycleOSDScreen == m_SelectedIndex )
   {
      action = MenuQuickMenu::CycleOSDScreen;
      selectionState = m_pItemsSelect[1]->getSelectedIndex();
   }
   if ( m_IndexCycleOSDSize == m_SelectedIndex )
   {
      action = MenuQuickMenu::CycleOSDSize;
      selectionState = m_pItemsSelect[2]->getSelectedIndex();
   }
   if ( m_IndexTakePicture == m_SelectedIndex )
   {
      action = MenuQuickMenu::TakePicture;
      selectionState = m_pItemsSelect[3]->getSelectedIndex();
   }
   if ( m_IndexVideoRecording == m_SelectedIndex )
   {
      action = MenuQuickMenu::VideoRecording;
      selectionState = m_pItemsSelect[4]->getSelectedIndex();
   }
   if ( m_IndexToggleOSD == m_SelectedIndex )
   {
      action = MenuQuickMenu::ToggleOSD;
      selectionState = m_pItemsSelect[5]->getSelectedIndex();
   }
   if ( m_IndexToggleStatsOff == m_SelectedIndex )
   {
      action = MenuQuickMenu::ToggleStatsOff;
      selectionState = m_pItemsSelect[6]->getSelectedIndex();
   }
   if ( m_IndexToggleAllOff == m_SelectedIndex )
   {
      action = MenuQuickMenu::ToggleAllOff;
      selectionState = m_pItemsSelect[7]->getSelectedIndex();
   }
   if ( m_IndexRelaySwitch == m_SelectedIndex )
   {
      action = MenuQuickMenu::RelaySwitch;
      selectionState = m_pItemsSelect[8]->getSelectedIndex();
   }
   if ( m_IndexSwitchCameraProfile == m_SelectedIndex )
   {
      action = MenuQuickMenu::SwitchCameraProfile;
      selectionState = m_pItemsSelect[9]->getSelectedIndex();
   }
   if ( m_IndexRCOutputOnOff == m_SelectedIndex )
   {
      action = MenuQuickMenu::RCOutputOnOff;
      selectionState = m_pItemsSelect[10]->getSelectedIndex();
   }
   if ( m_IndexRotaryEncoderFunction == m_SelectedIndex )
   {
      action = MenuQuickMenu::RotaryEncoderFunction;
      selectionState = m_pItemsSelect[11]->getSelectedIndex();
   }
   if ( m_IndexFreezeOSD == m_SelectedIndex )
   {
      action = MenuQuickMenu::FreezeOSD;
      selectionState = m_pItemsSelect[12]->getSelectedIndex();
   }
   if ( m_IndexCycleFavoriteVehicles == m_SelectedIndex )
   {
      action = MenuQuickMenu::CycleFavoriteVehicles;
      selectionState = m_pItemsSelect[13]->getSelectedIndex();
   }
   if ( m_IndexPITMode == m_SelectedIndex )
   {
      action = MenuQuickMenu::PITMode;
      selectionState = m_pItemsSelect[14]->getSelectedIndex();
   }

   if ( 0 == selectionState )
      pP->uEnabledQuickMenu &= ~action;
   else
      pP->uEnabledQuickMenu |= action;

   MenuQuickMenu::iPrevSelectedItem = 0; //reset the quick menu previously selected item

   save_Preferences();
   valuesToUI();
}

