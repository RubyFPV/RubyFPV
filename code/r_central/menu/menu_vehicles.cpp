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
#include "menu_vehicles.h"
#include "menu_confirmation.h"
#include "menu_vehicle_import.h"
#include "menu_vehicle_selector.h"

#include "../osd/osd_common.h"
#include "../../common/favorites.h"
#include "../../base/hardware_files.h"

//const char* s_textTitle[] = { "My Vehicles",  NULL };
const char* s_szVehicleNone1 = L("No vehicles defined. To add a vehicle:");
const char* s_szVehicleNone2 = L("* Search for a vehicle and then connect to it as controller;");
const char* s_szVehicleNone3 = L("* Or import a previously saved vehicle from a USB memory stick.");

MenuVehicles::MenuVehicles(void)
:Menu(MENU_ID_VEHICLES, L("My Vehicles"), NULL)
{
   if ( g_pControllerSettings->iDeveloperMode )
      m_Width = 0.38;
   else
      m_Width = 0.32;

   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   m_iLastSelectedVehicle = -1;

   log_line("[Menu] MenuVehicles: On open, this is the current radio interfaces configuration:");
   hardware_load_radio_info();

   m_bDisableStacking = true;
   m_bDisableBackgroundAlpha = true;
   m_bShowSpectator = true;
}

void MenuVehicles::onShow()
{
   m_Height = 0.0;
   
   if ( (NULL != g_pCurrentModel) && ( 0 != g_uActiveControllerModelVID) )
      log_line("[Menu] MenuVehicles: Current vehicle id: %u (%u)", g_pCurrentModel->uVehicleId, g_uActiveControllerModelVID);
   else
      log_line("[Menu] MenuVehicles: No current vehicle.");
   removeAllTopLines();
   removeAllItems();

   log_line("[Menu] MenuVehicles: %d controller vehicles, %d spectator vehicles", getControllerModelsCount(), getControllerModelsSpectatorCount());
   log_line("[Menu] MenuVehicles: Last selected vehicle index: %d", m_iLastSelectedVehicle);

   addTopLine("Select the vehicle to control:");
   bool bCurrentVehicleFound = false;
   int iItemIndexToSelect = -1;

   int iCountVehicles = getControllerModelsCount();
   if ( m_bShowSpectator )
      iCountVehicles += getControllerModelsSpectatorCount();
   if ( m_iLastSelectedVehicle == iCountVehicles )
      m_iLastSelectedVehicle--;
     
   for( int i=0; i<getControllerModelsCount(); i++ )
   {
      Model *p = getModelAtIndex(i);
      log_line("[Menu] MenuVehicles: Iterating vehicles %d: id: %u", i, p->uVehicleId);
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
      pItem->setVehicleIndex(i, false);
      int iIndexItem = addMenuItem( pItem );

      if ( (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) )
      if ( (g_uActiveControllerModelVID == p->uVehicleId) && (g_pCurrentModel->uVehicleId == p->uVehicleId) )
      {
         log_line("[Menu] MenuVehicles: Found current vehicle in the list at position %d. Added as menu item index %d.", i, iIndexItem);
         bCurrentVehicleFound = true;
         if ( -1 == m_iLastSelectedVehicle )
            m_iLastSelectedVehicle = i;
      }

      if ( -1 != m_iLastSelectedVehicle )
      if ( i == m_iLastSelectedVehicle )
      {
         log_line("[MenuVehicles] Set selected menu item index to %d, for vehicle index %d", iIndexItem, i);
         iItemIndexToSelect = iIndexItem;
      }
   }

   if ( m_bShowSpectator )
   for( int i=0; i<getControllerModelsSpectatorCount(); i++ )
   {
      Model *p = getSpectatorModel(i);
      log_line("[Menu] MenuVehicles: Iterating spectator vehicles: id: %u", p->uVehicleId);
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
      strcat(szBuff, L(" (Spectator)"));
      MenuItemVehicle* pItem = new MenuItemVehicle(szBuff);
      pItem->setVehicleIndex(i, true);
      int iIndexItem = addMenuItem( pItem );

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->is_spectator) )
      if ( (g_uActiveControllerModelVID == p->uVehicleId) && (g_pCurrentModel->uVehicleId == p->uVehicleId) )
      {
         log_line("[Menu] MenuVehicles: Found current spectator vehicle in the list at position %d. Added as menu item index %d.", i, iIndexItem);
         bCurrentVehicleFound = true;
         if ( -1 == m_iLastSelectedVehicle )
            m_iLastSelectedVehicle = i + getControllerModelsCount();
      }

      if ( -1 != m_iLastSelectedVehicle )
      if ( (i + getControllerModelsCount()) == m_iLastSelectedVehicle )
      {
         log_line("[MenuVehicles] Set selected menu item index to %d, for spectator vehicle index %d", iIndexItem, i);
         iItemIndexToSelect = iIndexItem;
      }
   }

   if ( ! bCurrentVehicleFound )
      log_softerror_and_alarm("[Menu] MenuVehicles: Current vehicle not found in the vehicles list!");
   if ( 0 == iCountVehicles )
   {
      removeAllTopLines();
      addTopLine(s_szVehicleNone1);
      addTopLine(s_szVehicleNone2);
      addTopLine(s_szVehicleNone3);
   }

   addSeparator();
   m_IndexImport = addMenuItem(new MenuItem("Import Vehicle", "Imports a new vehicle from a model file on a USB stick."));
   m_IndexDeleteAll = -1;
   if ( iCountVehicles > 0 )
      m_IndexDeleteAll = addMenuItem(new MenuItem("Delete All Vehicles", "Deletes all models stored in memory."));

   Menu::onShow();

   if ( -1 != iItemIndexToSelect )
      m_SelectedIndex = iItemIndexToSelect;
}


void MenuVehicles::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   // Delete all
   if ( (1 == returnValue) && (1 == iChildMenuId/1000) )
   {
      render_all(get_current_timestamp_ms(), true, false);
      pairing_stop();

      ruby_set_active_model_id(0);
      g_pCurrentModel = NULL;

      int iModelsCount = getControllerModelsCount();
      Model* pModels[MAX_MODELS];

      for( int i=0; i<getControllerModelsCount(); i++ )
      {
         pModels[i] = getModelAtIndex(i);
         if ( NULL == pModels[i] )
            continue;
         deletePluginModelSettings(pModels[i]->uVehicleId);
      }
      save_PluginsSettings();

      for( int i=0; i<iModelsCount; i++ )
      {
         if ( NULL != pModels[i] )
            deleteModel(pModels[i]);
         log_line("[Menu] Deleted model %d of %d", i+1, iModelsCount);
      }

      if ( m_bShowSpectator )
      {
         iModelsCount = getControllerModelsSpectatorCount();
         
         for( int i=0; i<getControllerModelsSpectatorCount(); i++ )
         {
            pModels[i] = getSpectatorModel(i);
            if ( NULL == pModels[i] )
               continue;
            deletePluginModelSettings(pModels[i]->uVehicleId);
         }
         save_PluginsSettings();

         for( int i=0; i<iModelsCount; i++ )
         {
            if ( NULL != pModels[i] )
               deleteModel(pModels[i]);
            log_line("[Menu] Deleted model %d of %d", i+1, iModelsCount);
         }
      }

      menu_invalidate_all();
      menu_refresh_all_menus();
      menu_update_ui_all_menus();
      log_line("[Menu] Vehicle deeted all models");
   }
   invalidate();
}

void MenuVehicles::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float dy = 0.2*height_text;
   float fFavoriteHeight = 1.2*g_pRenderEngine->textHeight(g_idFontMenu);
   float fFavoriteWidth = fFavoriteHeight / g_pRenderEngine->getAspectRatio();

   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();

   int iCountVehicles = getControllerModelsCount();
   if ( m_bShowSpectator )
      iCountVehicles += getControllerModelsSpectatorCount();

   for( int i=0; i<m_ItemsCount; i++ )
   {
      float y0 = y;
      y += RenderItem(i,y);
      if ( i >= iCountVehicles )
         continue;

      Model *pModel = NULL;
      if ( i < getControllerModelsCount() )
         pModel = getModelAtIndex(i);
      else
         pModel = getSpectatorModel(i-getControllerModelsCount());
      if ( NULL == pModel )
         continue;
      if ( vehicle_is_favorite(pModel->uVehicleId) )
      {
         float xFav = m_RenderXPos + getUsableWidth() - m_pMenuItems[i]->m_fMarginX - 2.0*fFavoriteWidth;
         if ( (NULL != g_pCurrentModel) && (pModel->uVehicleId == g_pCurrentModel->uVehicleId) )
            g_pRenderEngine->drawIcon(xFav, y0-dy+0.6*height_text, fFavoriteWidth, fFavoriteHeight, g_idIconFavorite);
         else
            g_pRenderEngine->drawIcon(xFav, y0-dy, fFavoriteWidth, fFavoriteHeight, g_idIconFavorite);
      }
   }
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
   RenderEnd(yTop);
}


void MenuVehicles::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_IndexImport == m_SelectedIndex )
   {
      int iMountRes = hardware_try_mount_usb();
      if ( 1 != iMountRes )
      {
         log_line("[Menu] MenuVehicles: No USB memory stick available.");
         Popup* p = new Popup("Please insert a USB memory stick to import the vehicle from.",0.28, 0.32, 0.32, 3);
         if ( 0 != iMountRes )
            p->setTitle("USB memory stick invalid. Please try again.");
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }

      MenuVehicleImport* pM = new MenuVehicleImport();
      pM->setCreateNew();
      pM->buildSettingsFileList();
      if ( 0 == pM->getSettingsFilesCount() )
      {
         hardware_unmount_usb();
         ruby_signal_alive();
         sync();
         ruby_signal_alive();
         delete pM;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+10*1000,"No vehicle settings files",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("There are no vehicle settings files on the USB stick.");
         add_menu_to_stack(pm);
         return;
      }
      add_menu_to_stack(pM);
      return;
   }

   if ( (-1 != m_IndexDeleteAll) && (m_IndexDeleteAll == m_SelectedIndex) )
   {
      if ( (NULL != g_pCurrentModel) )
      if ( checkIsArmed() )
      {
         //addMessage("Can't delete all models as current vehicle is armed.");
         return;
      }
      char szBuff[64];
      sprintf(szBuff, "Are you sure you want to delete all models?");
      add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 1));
      return;
   }

   int iCountVehicles = getControllerModelsCount();
   if ( m_bShowSpectator )
      iCountVehicles += getControllerModelsSpectatorCount();

   if ( 0 == iCountVehicles )
   {
      menu_stack_pop(0);
      return;
   }

   Model *pModel = NULL;
   if ( m_SelectedIndex < getControllerModelsCount() )
      pModel = getModelAtIndex(m_SelectedIndex);
   else
      pModel = getSpectatorModel(m_SelectedIndex-getControllerModelsCount());

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[Menu] MenuVehicles: NULL model for vehicle: %s.", m_pMenuItems[m_SelectedIndex]->getTitle());
      return;
   }

   m_iLastSelectedVehicle = m_SelectedIndex;
   log_line("[Menu] MenuVehicles: Adding menu selector for vehicle index %d", m_iLastSelectedVehicle);
   
   MenuVehicleSelector* pMenu = new MenuVehicleSelector();
   pMenu->m_IndexSelectedVehicle = m_SelectedIndex;
   pMenu->m_bSpectatorMode = false;
   if ( m_SelectedIndex >= getControllerModelsCount() )
   {
      pMenu->m_bSpectatorMode = true;
      pMenu->m_IndexSelectedVehicle = m_SelectedIndex - getControllerModelsCount();
   }
   pMenu->m_yPos = m_pMenuItems[m_SelectedIndex]->getItemRenderYPos() - g_pRenderEngine->textHeight(g_idFontMenu);
   add_menu_to_stack(pMenu);
}
