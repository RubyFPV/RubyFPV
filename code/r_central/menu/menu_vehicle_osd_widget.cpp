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
#include "menu_vehicle_osd_widget.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd.h"
#include "../osd/osd_common.h"
#include "../osd/osd_widgets.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define WIDGET_MOVE_MARGIN 0.01

MenuVehicleOSDWidget::MenuVehicleOSDWidget(int iWidgetIndex)
:Menu(MENU_ID_VEHICLE_OSD_WIDGET, "Widget Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.22;
   m_nWidgetIndex = iWidgetIndex;
   m_nWidgetSettingsCount = 0;
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
   m_bIsResizingH = false;
   m_bIsResizingV = false;

   m_fMenuOrgAlpha = menu_getGlobalAlpha();

   removeAllItems();

   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;

   char szBuff[256];
   sprintf(szBuff, "Configure %s", pWidget->info.szName);
   setTitle(szBuff);

   for( int i=0; i<MAX_OSD_WIDGET_PARAMS; i++ )
      m_IndexSettings[i] = -1;

   m_nWidgetSettingsCount = pWidget->display_info[0][0].iParamsCount;
   if ( m_nWidgetSettingsCount >= MAX_OSD_WIDGET_PARAMS )
      m_nWidgetSettingsCount = MAX_OSD_WIDGET_PARAMS-1;

   addMenuItem(new MenuItemSection("Layout"));

   m_IndexMoveX = addMenuItem(new MenuItem("Move Horizontally", "Sets the horizontal position of the plugin on the screen."));
   m_IndexMoveY = addMenuItem(new MenuItem("Move Vertically", "Sets the horizontal position of the plugin on the screen."));
   m_IndexResize = addMenuItem(new MenuItem("Resize", "Sets the size of the plugin on the screen."));
   m_IndexResizeH = addMenuItem(new MenuItem("Resize Horizontal", "Sets the width of the plugin on the screen."));
   m_IndexResizeV = addMenuItem(new MenuItem("Resize Vertical", "Sets the height of the plugin on the screen."));
}

MenuVehicleOSDWidget::~MenuVehicleOSDWidget()
{
}

void MenuVehicleOSDWidget::valuesToUI()
{
   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;
}

void MenuVehicleOSDWidget::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}

void MenuVehicleOSDWidget::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;
   int iIndexModel = osd_widget_get_model_index(pWidget, g_pCurrentModel->uVehicleId);
   int iIndexProfile = osd_get_current_layout_index();
   if ( (iIndexModel < 0) || (iIndexProfile < 0) )
      return;

   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      type_osd_widget_display_info* pDisplayInfo = &(pWidget->display_info[iIndexModel][iIndexProfile]);

      double c[4] = {255,100,0,0.9};
      g_pRenderEngine->setColors(c);
      g_pRenderEngine->drawRect(pDisplayInfo->fXPos, pDisplayInfo->fYPos, pDisplayInfo->fWidth, pDisplayInfo->fHeight);

      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());

   }
}

void MenuVehicleOSDWidget::stopAction()
{
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
   m_bIsResizingH = false;
   m_bIsResizingV = false;
   menu_setGlobalAlpha(m_fMenuOrgAlpha);
   osd_enable_rendering();
   osd_widgets_save();
}

bool MenuVehicleOSDWidget::periodicLoop()
{
   return false;
}


void MenuVehicleOSDWidget::onMinusAction()
{
   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;
   int iIndexModel = osd_widget_get_model_index(pWidget, g_pCurrentModel->uVehicleId);
   int iIndexProfile = osd_get_current_layout_index();
   if ( (iIndexModel < 0) || (iIndexProfile < 0) )
      return;

   float fXPos = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
   float fYPos = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
   float fWidth = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
   float fHeight = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

   float fWPixel = g_pRenderEngine->getPixelWidth();
   float fHPixel = g_pRenderEngine->getPixelHeight();

   if ( m_bIsMovingH )
   {
      fXPos -= WIDGET_MOVE_MARGIN;
      if ( fXPos <= 0 )
         fXPos = fWPixel;
   }
   if ( m_bIsMovingV )
   {
      fYPos -= WIDGET_MOVE_MARGIN;
      if ( fYPos <= 0 )
        fYPos = fHPixel;
   }

   float fAspect = 1.0;
   if ( 0 < fHeight )
      fAspect = fWidth / fHeight;

   if ( m_bIsResizing )
   if ( fWidth > WIDGET_MOVE_MARGIN )
   if ( fHeight > WIDGET_MOVE_MARGIN/fAspect )
   {
      fWidth -= WIDGET_MOVE_MARGIN;
      fHeight -= WIDGET_MOVE_MARGIN/fAspect;

      fXPos += WIDGET_MOVE_MARGIN*0.5;
      fYPos += WIDGET_MOVE_MARGIN*0.5/fAspect;
   }

   if ( m_bIsResizingH && (fWidth > WIDGET_MOVE_MARGIN) )
   {
      fWidth -= WIDGET_MOVE_MARGIN;
      fXPos += WIDGET_MOVE_MARGIN*0.5;
   }

   if ( m_bIsResizingV && (fHeight > WIDGET_MOVE_MARGIN) )
   {
      fHeight -= WIDGET_MOVE_MARGIN;
      fYPos += WIDGET_MOVE_MARGIN*0.5;
   }

   pWidget->display_info[iIndexModel][iIndexProfile].fXPos = fXPos;
   pWidget->display_info[iIndexModel][iIndexProfile].fYPos = fYPos;
   pWidget->display_info[iIndexModel][iIndexProfile].fWidth = fWidth;
   pWidget->display_info[iIndexModel][iIndexProfile].fHeight = fHeight;
}

void MenuVehicleOSDWidget::onPlusAction()
{
   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;
   int iIndexModel = osd_widget_get_model_index(pWidget, g_pCurrentModel->uVehicleId);
   int iIndexProfile = osd_get_current_layout_index();
   if ( (iIndexModel < 0) || (iIndexProfile < 0) )
      return;

   float fXPos = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
   float fYPos = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
   float fWidth = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
   float fHeight = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;
   float fWPixel = g_pRenderEngine->getPixelWidth();
   float fHPixel = g_pRenderEngine->getPixelHeight();

   if ( m_bIsMovingH )
   {
      fXPos += WIDGET_MOVE_MARGIN;
      if ( fXPos + fWidth >= 1.0 )
         fXPos = 1.0 - fWidth - fWPixel;
   }

   if ( m_bIsMovingV )
   {
      fYPos += WIDGET_MOVE_MARGIN;
      if ( fYPos + fHeight >= 1.0 )
         fYPos = 1.0 - fHeight - fHPixel;
   }

   float fAspect = 1.0;
   if ( 0 < fHeight )
      fAspect = fWidth/fHeight;

   if ( m_bIsResizing )
   if ( fXPos + fWidth <= (1.0-WIDGET_MOVE_MARGIN) )
   if ( fYPos + fHeight <= 1.0-WIDGET_MOVE_MARGIN/fAspect )
   {
      fWidth += WIDGET_MOVE_MARGIN;
      fHeight += WIDGET_MOVE_MARGIN/fAspect;

      fXPos -= WIDGET_MOVE_MARGIN*0.5;
      if ( fXPos < 0 )
         fXPos = fWPixel;

      fYPos -= WIDGET_MOVE_MARGIN*0.5/fAspect;
      if ( fYPos < 0 )
         fYPos = fHPixel;
   }

   if ( m_bIsResizingH && (fWidth < 1.0) )
   {
      fWidth += WIDGET_MOVE_MARGIN;
      fXPos -= WIDGET_MOVE_MARGIN*0.5;
      if ( fXPos < 0 )
         fXPos = fWPixel;
      if ( fXPos + fWidth >= 1.0 )
         fXPos = 1.0 - fWidth - fWPixel;
   }

   if ( m_bIsResizingV && (fHeight < 1.0) )
   {
      fHeight += WIDGET_MOVE_MARGIN;
      fYPos -= WIDGET_MOVE_MARGIN*0.5;
      if ( fYPos < 0 )
         fYPos = fHPixel;
      if ( fYPos + fHeight >= 1.0 )
         fYPos = 1.0 - fHeight - fHPixel;
   }

   pWidget->display_info[iIndexModel][iIndexProfile].fXPos = fXPos;
   pWidget->display_info[iIndexModel][iIndexProfile].fYPos = fYPos;
   pWidget->display_info[iIndexModel][iIndexProfile].fWidth = fWidth;
   pWidget->display_info[iIndexModel][iIndexProfile].fHeight = fHeight;
}

void MenuVehicleOSDWidget::onMoveUp(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      Preferences* pP = get_Preferences();
      if ( bIgnoreReversion || (pP->iSwapUpDownButtonsValues == 0) )
         onMinusAction();
      else
         onPlusAction();
      return;
   }
   Menu::onMoveUp(bIgnoreReversion);
}

void MenuVehicleOSDWidget::onMoveDown(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      Preferences* pP = get_Preferences();
      if ( bIgnoreReversion || (pP->iSwapUpDownButtonsValues == 0) )
         onPlusAction();
      else
         onMinusAction();
      return;
   }

   Menu::onMoveDown(bIgnoreReversion);
}

void MenuVehicleOSDWidget::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      onMoveUp(bIgnoreReversion);
   }
   Menu::onMoveLeft(bIgnoreReversion);
}

void MenuVehicleOSDWidget::onMoveRight(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      onMoveDown(bIgnoreReversion);
   }
   Menu::onMoveRight(bIgnoreReversion);
}



int MenuVehicleOSDWidget::onBack()
{
   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return 1;
   int iIndexModel = osd_widget_get_model_index(pWidget, g_pCurrentModel->uVehicleId);
   int iIndexProfile = osd_get_current_layout_index();
   if ( (iIndexModel < 0) || (iIndexProfile < 0) )
      return 1;

   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      pWidget->display_info[iIndexModel][iIndexProfile].fXPos = m_fXOrg;
      pWidget->display_info[iIndexModel][iIndexProfile].fYPos = m_fYOrg;
      pWidget->display_info[iIndexModel][iIndexProfile].fWidth = m_fWOrg;
      pWidget->display_info[iIndexModel][iIndexProfile].fHeight = m_fHOrg;

      if ( pWidget->display_info[iIndexModel][iIndexProfile].fWidth < 0.02 )
         pWidget->display_info[iIndexModel][iIndexProfile].fWidth = 0.02;
      if ( pWidget->display_info[iIndexModel][iIndexProfile].fHeight < 0.02 )
         pWidget->display_info[iIndexModel][iIndexProfile].fHeight = 0.02;
      stopAction();
      return 1;
   }
   menu_setGlobalAlpha(1.0);
   osd_enable_rendering();
   return Menu::onBack();
}


void MenuVehicleOSDWidget::onSelectItem()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing || m_bIsResizingH || m_bIsResizingV )
   {
      stopAction();
      osd_widgets_save();
      return;
   }

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   type_osd_widget* pWidget = osd_widgets_get(m_nWidgetIndex);
   if ( NULL == pWidget )
      return;
   int iIndexModel = osd_widget_get_model_index(pWidget, g_pCurrentModel->uVehicleId);
   int iIndexProfile = osd_get_current_layout_index();
   if ( (iIndexModel < 0) || (iIndexProfile < 0) )
      return;

   if ( m_IndexMoveX == m_SelectedIndex )
   {
      m_fXOrg = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
      m_fYOrg = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
      m_fWOrg = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
      m_fHOrg = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

      m_bIsMovingH = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }
   if ( m_IndexMoveY == m_SelectedIndex )
   {
      m_fXOrg = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
      m_fYOrg = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
      m_fWOrg = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
      m_fHOrg = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

      m_bIsMovingV = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   if ( m_IndexResize == m_SelectedIndex )
   {
      m_fXOrg = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
      m_fYOrg = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
      m_fWOrg = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
      m_fHOrg = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

      m_bIsResizing = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   if ( m_IndexResizeH == m_SelectedIndex )
   {
      m_fXOrg = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
      m_fYOrg = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
      m_fWOrg = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
      m_fHOrg = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

      m_bIsResizingH = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   if ( m_IndexResizeV == m_SelectedIndex )
   {
      m_fXOrg = pWidget->display_info[iIndexModel][iIndexProfile].fXPos;
      m_fYOrg = pWidget->display_info[iIndexModel][iIndexProfile].fYPos;
      m_fWOrg = pWidget->display_info[iIndexModel][iIndexProfile].fWidth;
      m_fHOrg = pWidget->display_info[iIndexModel][iIndexProfile].fHeight;

      m_bIsResizingV = true;
      menu_setGlobalAlpha(0.0);
      osd_disable_rendering();
      return;
   }

   /*
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
   */
}
