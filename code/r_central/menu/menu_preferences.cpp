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
#include "menu_preferences.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "../popup_log.h"
#include "osd_common.h"
#include "menu_preferences_ui.h"

MenuPreferences::MenuPreferences(void)
:Menu(MENU_ID_PREFERENCES, L("User Interface Messages"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   int c = 0;

   m_pItemsSelect[8] = new MenuItemSelect(L("Display Units"), L("Changes how the OSD displays data: in metric system or imperial system."));
   m_pItemsSelect[8]->addSelection("Metric (km/h)");
   m_pItemsSelect[8]->addSelection("Metric (m/s)");
   m_pItemsSelect[8]->addSelection("Imperial (mi/h)");
   m_pItemsSelect[8]->addSelection("Imperial (ft/s)");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexUnits = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect(L("Persistent Messages"), L("Keep the various messages and warnings longer on the screen."));
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexPersistentMessages = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[11] = new MenuItemSelect("Log Messages Window", "Shows the log messages window.");
   m_pItemsSelect[11]->addSelection("Off");
   m_pItemsSelect[11]->addSelection("On New Content");
   m_pItemsSelect[11]->addSelection("Always On");
   m_pItemsSelect[11]->setIsEditable();
   m_IndexLogWindow = addMenuItem(m_pItemsSelect[11]);

   c++;
}

void MenuPreferences::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( p->iUnits == prefUnitsMetric )
      m_pItemsSelect[8]->setSelection(0);
   if ( p->iUnits == prefUnitsMeters )
      m_pItemsSelect[8]->setSelection(1);
   if ( p->iUnits == prefUnitsImperial )
      m_pItemsSelect[8]->setSelection(2);
   if ( p->iUnits == prefUnitsFeets )
      m_pItemsSelect[8]->setSelection(3);

   m_pItemsSelect[9]->setSelection(p->iPersistentMessages);
   m_pItemsSelect[11]->setSelection(p->iShowLogWindow);
}

void MenuPreferences::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuPreferences::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuPreferences::onSelectItem()
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

   if ( m_IndexUnits == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      int nSel = m_pItemsSelect[8]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnits = prefUnitsMetric;
      if ( 1 == nSel )
         p->iUnits = prefUnitsMeters;
      if ( 2 == nSel )
         p->iUnits = prefUnitsImperial;
      if ( 3 == nSel )
         p->iUnits = prefUnitsFeets;
   }

   if ( m_IndexPersistentMessages == m_SelectedIndex )
      p->iPersistentMessages = m_pItemsSelect[9]->getSelectedIndex();

   if ( m_IndexLogWindow == m_SelectedIndex )
   {
      p->iShowLogWindow = m_pItemsSelect[11]->getSelectedIndex();
      popup_log_set_show_flag(p->iShowLogWindow);
      menu_invalidate_all();
      valuesToUI();
      return;
   }

   save_Preferences();
}
