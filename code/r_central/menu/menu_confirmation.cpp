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

#include "../../base/base.h"
#include "../../base/config.h"
#include "menu.h"
#include "menu_confirmation.h"


MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION + id*1000, szTitle, NULL)
{
   m_bDisableStacking = true;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = false;
   m_bShowDoNotShowAgain = false;
   m_bDoNotShowAgain = false;
   m_bDisablePairingUIActions = false;
   m_bDefaultsToYes = false;
   m_uTimeoutMs = 0;
   m_uCloseOnTimeoutTime = 0;
   m_uIconId = 0;
   m_iUniqueId = 0;
   if ( NULL != szText )
      addTopLine(szText);

   m_szButtonOk[0] = 0;
   m_iIndexMenuOk = -1;
   m_iIndexMenuCancel = -1;
   m_iIndexMenuDoNotShow = -1;
   m_pCheckBox = NULL;
}

MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption)
:Menu(MENU_ID_CONFIRMATION + id*1000, szTitle, NULL)
{
   m_bDisableStacking = true;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = singleOption;
   m_bShowDoNotShowAgain = false;
   m_bDoNotShowAgain = false;
   m_bDisablePairingUIActions = false;
   m_bDefaultsToYes = false;
   m_uTimeoutMs = 0;
   m_uCloseOnTimeoutTime = 0;
   m_uIconId = 0;
   m_iUniqueId = 0;
   if ( NULL != szText )
      addTopLine(szText);

   m_szButtonOk[0] = 0;
   m_iIndexMenuOk = -1;
   m_iIndexMenuCancel = -1;
   m_iIndexMenuDoNotShow = -1;
   m_pCheckBox = NULL;
}

MenuConfirmation::~MenuConfirmation()
{
}

void MenuConfirmation::setOkActionText(const char* szText)
{
   if ( (NULL == szText) || (0 == szText[0]) )
      return;
   strncpy(m_szButtonOk, szText, sizeof(m_szButtonOk)/sizeof(m_szButtonOk[0])-1);
}

void MenuConfirmation::setDefaultActionPositive()
{
   m_bDefaultsToYes = true;
}

void MenuConfirmation::setIconId(u32 uIconId)
{
   m_uIconId = uIconId;
   invalidate();
}

void MenuConfirmation::setTimeoutMs(u32 uTimeoutMs)
{
   m_uTimeoutMs = uTimeoutMs;
   if ( m_uTimeoutMs > 0 )
      m_uCloseOnTimeoutTime = g_TimeNow + m_uTimeoutMs;
}


void MenuConfirmation::setUniqueId(int iUniqueId)
{
   m_iUniqueId = iUniqueId;
}

void MenuConfirmation::enableShowDoNotShowAgain()
{
   m_bShowDoNotShowAgain = true;
}

void MenuConfirmation::disablePairingUIActions()
{
   m_bDisablePairingUIActions = true;
}

void MenuConfirmation::onShow()
{
   removeAllItems();
   m_iIndexMenuOk = -1;
   m_iIndexMenuCancel = -1;
   m_iIndexMenuDoNotShow = -1;
   m_pCheckBox = NULL;

   if ( m_bSingleOption )
   {
      if ( 0 != m_szButtonOk[0] )
         m_iIndexMenuOk = addMenuItem(new MenuItem(m_szButtonOk));
      else
         m_iIndexMenuOk = addMenuItem(new MenuItem(L("Ok")));
      m_yPos = 0.45;
   }
   else
   {
      m_iIndexMenuCancel = addMenuItem(new MenuItem(L("No")));
      m_iIndexMenuOk = addMenuItem(new MenuItem(L("Yes")));
   }

   if ( m_bShowDoNotShowAgain && (m_iUniqueId > 0) && (getPreferencesDoNotShowAgain(m_iUniqueId) != -1) )
   {
      m_pMenuItems[m_ItemsCount-1]->setExtraHeight(g_pRenderEngine->textHeight(g_idFontMenu));
      m_pCheckBox = new MenuItemCheckbox(L("Do not show again"));
      m_iIndexMenuDoNotShow = addMenuItem(m_pCheckBox);

      m_bDoNotShowAgain = (bool)getPreferencesDoNotShowAgain(m_iUniqueId);
      m_pCheckBox->setChecked(m_bDoNotShowAgain);
   }

   if ( ! m_bSingleOption )
   if ( m_bDefaultsToYes )
   if ( m_iIndexMenuOk != -1 )
   {
      m_SelectedIndex = 1;
      onFocusedItemChanged();
   }
}

void MenuConfirmation::valuesToUI()
{

}

void MenuConfirmation::Render()
{
   RenderPrepare();

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   if ( m_uIconId != 0 )
      m_RenderWidth += iconWidth + m_sfMenuPaddingX;

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   if ( m_uIconId != 0 )
      m_RenderWidth -= iconWidth + m_sfMenuPaddingX;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   if ( m_uIconId != 0 )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.9);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth, m_RenderYPos + 0.5*(m_RenderHeight-iconHeight), iconWidth, iconHeight, m_uIconId);
   }
}

void MenuConfirmation::_saveDoNotShowFlag()
{
   if ( m_iUniqueId <= 0 )
      return;
   setPreferencesDoNotShowAgain(m_iUniqueId, (int)m_bDoNotShowAgain);
   save_Preferences();
}

bool MenuConfirmation::periodicLoop()
{
   if ( (m_uTimeoutMs != 0) && (m_uCloseOnTimeoutTime != 0) && (g_TimeNow > m_uCloseOnTimeoutTime) )
   {
      m_uTimeoutMs = 0;
      m_uCloseOnTimeoutTime = 0;
      menu_stack_pop(1);
      if ( (! m_bDisablePairingUIActions) && onEventPairingTookUIActions() )
         onEventFinishedPairUIAction();
      return true;
   }
   return Menu::periodicLoop();
}

int MenuConfirmation::onBack()
{
   int iRet = Menu::onBack();
   _saveDoNotShowFlag();
   if ( (! m_bDisablePairingUIActions) && onEventPairingTookUIActions() )
      onEventFinishedPairUIAction();
   log_line("Menu Confirmation: will close on Back, return value: %d, confirmation id: %d (%d)", iRet, m_MenuId, (m_MenuId - MENU_ID_CONFIRMATION)/1000);
   return iRet;
}

void MenuConfirmation::onSelectItem()
{
   log_line("Menu Confirmation: selected item: %d", m_SelectedIndex);

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( (m_iIndexMenuOk != -1) && (m_iIndexMenuOk == m_SelectedIndex) )
   if ( (m_MenuId%1000) != MENU_ID_SIMPLE_MESSAGE ) // if it's a simple message menu, it was poped up by menu base.
   {
      log_line("Menu Confirmation: will close with Ok, confirmation id: %d (%d)", m_MenuId, (m_MenuId - MENU_ID_CONFIRMATION)/1000);
      _saveDoNotShowFlag();
      menu_stack_pop(1);
      if ( (! m_bDisablePairingUIActions) && onEventPairingTookUIActions() )
         onEventFinishedPairUIAction();
      return;
   }

   if ( (m_iIndexMenuCancel != -1) && (m_iIndexMenuCancel == m_SelectedIndex) )
   {
      log_line("Menu Confirmation: will close with Cancel, confirmation id: %d (%d)", m_MenuId, (m_MenuId - MENU_ID_CONFIRMATION)/1000);
      _saveDoNotShowFlag();
      menu_stack_pop(0);
      if ( (! m_bDisablePairingUIActions) && onEventPairingTookUIActions() )
         onEventFinishedPairUIAction();
      return;
   }

   if ( (m_iIndexMenuDoNotShow != -1) && (m_iIndexMenuDoNotShow == m_SelectedIndex) )
   {
      m_bDoNotShowAgain = !m_bDoNotShowAgain;
      if ( NULL != m_pCheckBox )
         m_pCheckBox->setChecked(m_bDoNotShowAgain);
      _saveDoNotShowFlag();
   }
}