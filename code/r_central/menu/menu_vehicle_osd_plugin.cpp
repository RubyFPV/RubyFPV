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

#include "../../public/settings_info.h"
#include "menu.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd.h"
#include "../osd/osd_common.h"
#include "../osd/osd_plugins.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define PLUGIN_MOVE_MARGIN 0.01

extern bool s_bMenuOSDPluginsPluginDeleted;

MenuVehicleOSDPlugin::MenuVehicleOSDPlugin(int pluginIndex)
:Menu(MENU_ID_VEHICLE_OSD_PLUGIN, "Plugin Settings", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.22;
   m_nPluginIndex = pluginIndex;
   m_nPluginSettingsCount = 0;
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;

   m_fMenuOrgAlpha = menu_getGlobalAlpha();
   readPlugin();
}

MenuVehicleOSDPlugin::~MenuVehicleOSDPlugin()
{
}

void MenuVehicleOSDPlugin::readPlugin()
{
   removeAllItems();

   char* szName = osd_plugins_get_name(m_nPluginIndex);
   char szBuff[256];
   sprintf(szBuff, "Configure %s", szName);
   setTitle(szBuff);

   int iMaxStringLenght = 0;
   int iMaxStringLenght2 = 0;

   for( int i=0; i<MAX_PLUGIN_SETTINGS; i++ )
      m_IndexSettings[i] = -1;

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount >= MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS-1;

   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      int type = osd_plugin_get_setting_type(m_nPluginIndex, i);
      char* szName = osd_plugin_get_setting_name(m_nPluginIndex,i);
      if ( (int)strlen(szName) > iMaxStringLenght )
         iMaxStringLenght = strlen(szName);
      if ( type == PLUGIN_SETTING_TYPE_BOOL )
      {
         m_pItemsSelect[i+1] = new MenuItemSelect(szName);
         m_pItemsSelect[i+1]->addSelection("No");
         m_pItemsSelect[i+1]->addSelection("Yes");
         //m_pItemsSelect[i+1]->setIsEditable();
         //m_pItemsSelect[i+1]->setUseMultiViewLayout();
         m_IndexSettings[i] = addMenuItem(m_pItemsSelect[i+1]);
      }
      if ( type == PLUGIN_SETTING_TYPE_ENUM )
      {
         m_pItemsSelect[i+1] = new MenuItemSelect(szName);
         int c = osd_plugin_get_setting_options_count(m_nPluginIndex, i);
         for( int k=0; k<c; k++ )
         {
            char* szOptName = osd_plugin_get_setting_option_name(m_nPluginIndex, i, k);
            if ( NULL != szOptName )
            {
               if ( (int)strlen(szOptName) > iMaxStringLenght2 )
                  iMaxStringLenght2 = strlen(szOptName);
               m_pItemsSelect[i+1]->addSelection(szOptName);
            }
         }
         m_pItemsSelect[i+1]->setIsEditable();
         m_IndexSettings[i] = addMenuItem(m_pItemsSelect[i+1]);
      }
      if ( type == PLUGIN_SETTING_TYPE_INT )
      {
         int min = osd_plugin_get_setting_minvalue(m_nPluginIndex,i);
         int max = osd_plugin_get_setting_maxvalue(m_nPluginIndex,i);
         int def = osd_plugin_get_setting_defaultvalue(m_nPluginIndex,i);
         m_pItemsRange[i+1] = new MenuItemRange(szName, min, max, def, 1 );
         m_pItemsRange[i+1]->setSufix("");
         m_IndexSettings[i] = addMenuItem(m_pItemsRange[i+1]);
      }
   }

   addMenuItem(new MenuItemSection("Layout"));

   m_IndexMoveX = addMenuItem(new MenuItem("Move Horizontally", "Sets the horizontal position of the plugin on the screen."));
   m_IndexMoveY = addMenuItem(new MenuItem("Move Vertically", "Sets the horizontal position of the plugin on the screen."));
   m_IndexResize = addMenuItem(new MenuItem("Resize", "Sets the size of the plugin on the screen."));


   iMaxStringLenght += iMaxStringLenght2;
   for( int i=0; i<iMaxStringLenght; i++ )
      szBuff[i] = 'A';
   szBuff[iMaxStringLenght] = 0;

   float fMaxWidth = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);

   fMaxWidth -= m_sfMenuPaddingX;
   if ( fMaxWidth > 0.6 )
      fMaxWidth = 0.6;
   if ( fMaxWidth > m_Width )
   {
      m_Width = fMaxWidth;
      invalidate();
   }
}

void MenuVehicleOSDPlugin::valuesToUI()
{
   char* szUID = osd_plugins_get_uid(m_nPluginIndex);
   SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
   if ( NULL == pPlugin )
      return;

   int iModelIndex = getPluginModelSettingsIndex(pPlugin, g_pCurrentModel);
   int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount > MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS;

   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      int type = osd_plugin_get_setting_type(m_nPluginIndex, i);
      if ( type == PLUGIN_SETTING_TYPE_BOOL )
      {
         m_pItemsSelect[i+1]->setSelectedIndex( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
      if ( type == PLUGIN_SETTING_TYPE_ENUM )
      {
         m_pItemsSelect[i+1]->setSelectedIndex( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
      if ( type == PLUGIN_SETTING_TYPE_INT )
      {
         m_pItemsRange[i+1]->setCurrentValue( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
   }
}

void MenuVehicleOSDPlugin::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}

void MenuVehicleOSDPlugin::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleOSDPlugin::stopAction()
{
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
   menu_setGlobalAlpha(m_fMenuOrgAlpha);
   osd_enable_rendering();

   plugin_osd_t* pPlugin = osd_plugins_get(m_nPluginIndex);
   pPlugin->bBoundingBox = false;
   pPlugin->bHighlight = false;

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      plugin_osd_t* pPlugin = osd_plugins_get(i);
      if ( NULL != pPlugin )
         pPlugin->bBoundingBox = false;
   }
   save_PluginsSettings();
}

bool MenuVehicleOSDPlugin::periodicLoop()
{
   return false;
}

void MenuVehicleOSDPlugin::onMinusAction()
{
   plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
   if ( NULL == pPluginOSD )
      return;

   SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
   if ( NULL == pPluginSettings )
      return;

   int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

   if ( m_bIsMovingH )
   if ( pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] >= PLUGIN_MOVE_MARGIN )
      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;

   if ( m_bIsMovingV )
   if ( pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] >= PLUGIN_MOVE_MARGIN )
      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;

   float fAspect = 1.0;
   if ( 0 < pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] )
      fAspect = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] / pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];

   if ( m_bIsResizing )
   if ( pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] > PLUGIN_MOVE_MARGIN )
   if ( pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] > PLUGIN_MOVE_MARGIN/fAspect )
   {
      pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;
      pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN/fAspect;

      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN*0.5;
      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN*0.5/fAspect;
   }
}

void MenuVehicleOSDPlugin::onPlusAction()
{
   plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
   if ( NULL == pPluginOSD )
      return;

   SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
   if ( NULL == pPluginSettings )
      return;

   int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

   if ( m_bIsMovingH )
   if ( osd_getMarginX() + pPluginSettings->fXPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginX()) + pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] <= 1.0-osd_getMarginX()-PLUGIN_MOVE_MARGIN )
      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;

   if ( m_bIsMovingV )
   if ( osd_getMarginY() + pPluginSettings->fYPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginY()) + pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] <= 1.0-osd_getMarginY()-PLUGIN_MOVE_MARGIN )
      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;

   float fAspect = 1.0;
   if ( 0 < pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] )
      fAspect = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] / pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];

   if ( m_bIsResizing )
   if ( osd_getMarginX() + pPluginSettings->fXPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginX()) + pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] < (1.0-PLUGIN_MOVE_MARGIN) )
   if ( osd_getMarginY() + pPluginSettings->fYPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginY()) + pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] < 1.0-PLUGIN_MOVE_MARGIN/fAspect )
   {
      pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;
      pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN/fAspect;

      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN*0.5;
      if ( pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] < PLUGIN_MOVE_MARGIN )
         pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] = PLUGIN_MOVE_MARGIN;

      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN*0.5/fAspect;
      if ( pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] < PLUGIN_MOVE_MARGIN )
         pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] = PLUGIN_MOVE_MARGIN;
   }
}

void MenuVehicleOSDPlugin::onMoveUp(bool bIgnoreReversion)
{
   Preferences* pP = get_Preferences();

   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      if ( bIgnoreReversion || (pP->iSwapUpDownButtonsValues == 0) )
         onMinusAction();
      else
         onPlusAction();

      return;
   }
   Menu::onMoveUp(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveDown(bool bIgnoreReversion)
{
   Preferences* pP = get_Preferences();

   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      if ( bIgnoreReversion || (pP->iSwapUpDownButtonsValues == 0) )
         onPlusAction();
      else
         onMinusAction();

      return;
   }

   Menu::onMoveDown(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveUp(bIgnoreReversion);
   }
   Menu::onMoveLeft(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveRight(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveDown(bIgnoreReversion);
   }
   Menu::onMoveRight(bIgnoreReversion);
}



int MenuVehicleOSDPlugin::onBack()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return 0;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return 0;

      int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] = m_fXOrg;
      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] = m_fYOrg;
      pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] = m_fWOrg;
      pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] = m_fHOrg;

      if ( pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] < 0.02 )
         pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] = 0.02;
      if ( pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] < 0.02 )
         pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] = 0.02;
      stopAction();
      return 1;
   }
   menu_setGlobalAlpha(1.0);
   osd_enable_rendering();
   return Menu::onBack();
}


void MenuVehicleOSDPlugin::onSelectItem()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      stopAction();
      save_PluginsSettings();
      return;
   }

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_IndexMoveX == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }

      m_bIsMovingH = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }
   if ( m_IndexMoveY == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }

      m_bIsMovingV = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   if ( m_IndexResize == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }
      m_bIsResizing = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount > MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS;

   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      if ( m_SelectedIndex == m_IndexSettings[i] )
      {
         char* szUID = osd_plugins_get_uid(m_nPluginIndex);
         SinglePluginSettings* pPluginSettings = pluginsGetByUID(szUID);
         if ( NULL == pPluginSettings )
            return;

         int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
         int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

         int type = osd_plugin_get_setting_type(m_nPluginIndex, i);

         if ( type == PLUGIN_SETTING_TYPE_BOOL )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsSelect[i+1]->getSelectedIndex();
         }
         if ( type == PLUGIN_SETTING_TYPE_ENUM )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsSelect[i+1]->getSelectedIndex();
         }
         if ( type == PLUGIN_SETTING_TYPE_INT )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsRange[i+1]->getCurrentValue();
         }
         save_PluginsSettings();
         valuesToUI();
         return;
      }
   }
}
