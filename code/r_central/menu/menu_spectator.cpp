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
#include "menu_spectator.h"
#include "menu_search.h"
#include "menu_item_text.h"
#include "menu_vehicle_selector.h"

#include "../../common/favorites.h"
#include "../osd/osd_common.h"

const char* s_szNoSpectator = "No recently viewed vehicles in spectator mode.";

MenuSpectator::MenuSpectator(void)
:Menu(MENU_ID_SPECTATOR, "Spectator Mode Vehicles", NULL)
{
   addTopLine("Select a recently viewed vehicle to connect to in spectator mode:");
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.25;
   m_IndexSelectedVehicle = -1;
}


void MenuSpectator::onShow()
{
   m_Height = 0.0;

   removeAllItems();
   m_IndexSelectedVehicle = -1;

   for( int i=0; i<getControllerModelsSpectatorCount(); i++ )
   {
      Model *p = getSpectatorModel(i);
      log_line("MenuSpectator: Iterating vehicles: id: %u", p->uVehicleId);
      char szBuff[256];
      if ( 1 == p->radioLinksParams.links_count )
         sprintf(szBuff, "%s, %s", p->getLongName(), str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
      else if ( 2 == p->radioLinksParams.links_count )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(p->radioLinksParams.link_frequency_khz[1]));
         sprintf(szBuff, "%s, %s/%s", p->getLongName(), szFreq1, szFreq2);
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(p->radioLinksParams.link_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(p->radioLinksParams.link_frequency_khz[2]));
         sprintf(szBuff, "%s, %s/%s/%s", p->getLongName(), szFreq1, szFreq2, szFreq3);
      }

      MenuItemVehicle* pItem = new MenuItemVehicle(szBuff);
      pItem->setVehicleIndex(i, true);
      addMenuItem( pItem );

      //MenuItem* pMI = new MenuItem(szBuff);
      //addMenuItem( pMI );
      //if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator && g_pCurrentModel->uVehicleId == p->uVehicleId )
      //   pMI->setExtraHeight((1+MENU_TEXTLINE_SPACING)*g_pRenderEngine->textHeight(g_idFontMenu));

   }
   if ( 0 == getControllerModelsSpectatorCount() )
   {
      MenuItem* pMI = new MenuItemText(s_szNoSpectator);
      pMI->setEnabled(false);
      addMenuItem(pMI);
   }
   else
      addSeparator();
   addMenuItem( new MenuItem("Search for vehicles") );
   Menu::onShow();
}

void MenuSpectator::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float dy = 0.2*g_pRenderEngine->textHeight(g_idFontMenu);
   float fFavoriteHeight = 1.2*g_pRenderEngine->textHeight(g_idFontMenu);
   float fFavoriteWidth = fFavoriteHeight / g_pRenderEngine->getAspectRatio();

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( i == m_ItemsCount-1 )
         y += 0.02*m_sfScaleFactor;

      float y0 = y;
      y += RenderItem(i,y);
      if ( i >= getControllerModelsSpectatorCount() )
         continue;

      Model *pModel = getSpectatorModel(i);
      if ( NULL == pModel )
         continue;
      if ( vehicle_is_favorite(pModel->uVehicleId) )
         g_pRenderEngine->drawIcon(m_xPos + m_RenderWidth - m_sfMenuPaddingX - fFavoriteWidth, y0-dy, fFavoriteWidth, fFavoriteHeight, g_idIconFavorite);
   }
   RenderEnd(yTop);
}


void MenuSpectator::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   invalidate();
}

void MenuSpectator::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;
   if ( m_SelectedIndex == m_ItemsCount-1 )
   {
      MenuSearch* pMenuSearchSpectator = new MenuSearch();
      pMenuSearchSpectator->setSpectatorOnly();
      add_menu_to_stack(pMenuSearchSpectator);
      return;
   }

   if ( 0 == getControllerModelsSpectatorCount() )
   {
       return;
   }

   Model *pModel = getSpectatorModel(m_SelectedIndex);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for spectator mode vehicle: %s.", m_pMenuItems[m_SelectedIndex]->getTitle());
      return;
   }

   m_IndexSelectedVehicle = m_SelectedIndex;
   log_line("Adding menu for vehicle index %d", m_IndexSelectedVehicle);

   MenuVehicleSelector* pMenu = new MenuVehicleSelector();
   pMenu->m_IndexSelectedVehicle = m_SelectedIndex;
   pMenu->m_bSpectatorMode = true;
   pMenu->m_yPos = m_pMenuItems[m_SelectedIndex]->getItemRenderYPos() - g_pRenderEngine->textHeight(g_idFontMenu);
   add_menu_to_stack(pMenu);
}