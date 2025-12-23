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
#include "menu_preferences_ui.h"

#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_color_picker.h"
#include "../osd/osd_common.h"
#include "../popup_log.h"
#include "../fonts.h"


MenuPreferencesUI::MenuPreferencesUI(bool bShowOnlyOSD)
:Menu(MENU_ID_PREFERENCES_UI, L("Controller User Interface"), NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   m_bShowOnlyOSD = bShowOnlyOSD;
}

void MenuPreferencesUI::onShow()
{
   if ( m_bShowOnlyOSD )
      setTitle(L("OSD Size, Color, Fonts"));
   else
      setTitle(L("Controller User Interface"));

   int iTmp = getSelectedMenuItemIndex();
   addItems();
   Menu::onShow();

   if ( iTmp >= 1 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuPreferencesUI::addItems()
{
   removeAllItems();
   removeAllTopLines();

   m_IndexScaleMenu = -1;
   m_IndexMenuStacked = -1;
   m_IndexMenusCompact = -1;

   m_IndexUnits = -1;
   m_IndexUnitsHeight = -1;
   m_IndexPersistentMessages = -1;
   m_IndexLogWindow = -1;
   m_IndexMonitor = -1;
   m_IndexLanguage = -1;

   m_IndexInvertColors = -1;
   m_IndexColorPickerOSD = -1;
   m_IndexColorPickerOSDOutline = -1;
   m_IndexColorPickerAHI = -1;
   m_IndexOSDOutlineThickness = -1;
   m_IndexOSDSize = -1;
   m_IndexOSDFlip = -1;
   m_IndexOSDFont = -1;
   m_IndexOSDFontBold = -1;

   m_IndexMSPOSDFont = -1;
   m_IndexMSPOSDSize = -1;
   m_IndexMSPOSDDeltaX = -1;
   m_IndexMSPOSDDeltaY = -1;

   if ( ! m_bShowOnlyOSD )
   {
      addMenuItem(new MenuItemSection(L("Menus")));

      m_pItemsSelect[1] = new MenuItemSelect(L("Menus layout"), L("Changes how the menus appear on screen."));  
      m_pItemsSelect[1]->addSelection(L("Side by side"));
      m_pItemsSelect[1]->addSelection(L("Stacked"));
      m_pItemsSelect[1]->addSelection(L("Sticky Left"), false);
      m_pItemsSelect[1]->setIsEditable();
      m_IndexMenuStacked = addMenuItem(m_pItemsSelect[1]);

      m_pItemsSelect[9] = new MenuItemSelect(L("Compact Menus"), L("Show compact menus for ease of navigation. Show full settings only on demand, for each menu."));
      m_pItemsSelect[9]->addSelection(L("No"));
      m_pItemsSelect[9]->addSelection(L("Yes"));
      m_pItemsSelect[9]->setIsEditable();
      m_IndexMenusCompact = addMenuItem(m_pItemsSelect[9]);

      m_pItemsSelect[0] = new MenuItemSelect(L("Menu font size"), L("Change how big the menus appear on screen."));  
      m_pItemsSelect[0]->addSelection(L("X-Small"));
      m_pItemsSelect[0]->addSelection(L("Small"));
      m_pItemsSelect[0]->addSelection(L("Normal"));
      m_pItemsSelect[0]->addSelection(L("Large"));
      m_pItemsSelect[0]->addSelection(L("X-Large"));
      m_pItemsSelect[0]->setIsEditable();
      m_IndexScaleMenu = addMenuItem(m_pItemsSelect[0]);
      
   }

   if ( m_bShowOnlyOSD )
   {
      addMenuItem(new MenuItemSection("OSD"));

      m_pItemsSelect[2] = new MenuItemSelect(L("Invert colors"), L("Invert colors on OSD and menus."));
      m_pItemsSelect[2]->addSelection(L("Normal"));
      m_pItemsSelect[2]->addSelection(L("Inverted"));
      m_pItemsSelect[2]->setIsEditable();
      m_IndexInvertColors = addMenuItem(m_pItemsSelect[2]);

      m_IndexColorPickerOSD = addMenuItem(new MenuItem(L("OSD text color"), L("Change color of the text in the OSD."))); 
      m_IndexColorPickerOSDOutline = addMenuItem(new MenuItem(L("OSD outline color"), L("Change color of the outline in the OSD."))); 

      /*
      m_pItemsSelect[3] = new MenuItemSelect(L("OSD outline thickness"), L("Increase/decrease OSD outline thickness."));
      m_pItemsSelect[3]->addSelection(L("None"));
      m_pItemsSelect[3]->addSelection(L("Smallest"));
      m_pItemsSelect[3]->addSelection(L("Small"));
      m_pItemsSelect[3]->addSelection(L("Normal"));
      m_pItemsSelect[3]->addSelection(L("Large"));
      m_pItemsSelect[3]->addSelection(L("Larger"));
      m_pItemsSelect[3]->addSelection(L("Largest"));
      m_IndexOSDOutlineThickness = addMenuItem(m_pItemsSelect[3]);
      */

      m_pItemsSelect[6] = new MenuItemSelect(L("OSD screen size"), L("Change how big is the OSD relative to the screen."));  
      m_pItemsSelect[6]->addSelection("100%");
      m_pItemsSelect[6]->addSelection("98%");
      m_pItemsSelect[6]->addSelection("96%");
      m_pItemsSelect[6]->addSelection("94%");
      m_pItemsSelect[6]->addSelection("92%");
      m_pItemsSelect[6]->addSelection("90%");
      m_pItemsSelect[6]->addSelection("88%");
      m_pItemsSelect[6]->addSelection("86%");
      m_IndexOSDSize = addMenuItem(m_pItemsSelect[6]);

      m_pItemsSelect[7] = new MenuItemSelect(L("OSD flip vertical"), L("Flips the OSD info vertically for rotated displays."));  
      m_pItemsSelect[7]->addSelection(L("No"));
      m_pItemsSelect[7]->addSelection(L("Yes"));
      m_pItemsSelect[7]->setIsEditable();
      m_IndexOSDFlip = addMenuItem(m_pItemsSelect[7]);

      m_IndexColorPickerAHI = addMenuItem(new MenuItem(L("Instruments color"), L("Change color of the instruments/gauges."))); 

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
         m_pItemsSelect[10] = new MenuItemSelect(L("Ruby OSD font"), L("Changes the OSD font used for Ruby OSD elements."));  
      else
         m_pItemsSelect[10] = new MenuItemSelect(L("OSD font"), L("Changes the OSD font."));  

      #if defined (HW_PLATFORM_RASPBERRY)
      m_pItemsSelect[10]->addSelection("Font 1 Bold");
      m_pItemsSelect[10]->addSelection("Font 1 Outlined");
      m_pItemsSelect[10]->addSelection("Font 2");
      m_pItemsSelect[10]->addSelection("Font 3 Bold");
      m_pItemsSelect[10]->setIsEditable();
      m_IndexOSDFont = addMenuItem(m_pItemsSelect[10]);
      #else
      m_pItemsSelect[10]->addSelection("Font 1");
      m_pItemsSelect[10]->addSelection("Font 2");
      m_pItemsSelect[10]->setIsEditable();
      m_IndexOSDFont = addMenuItem(m_pItemsSelect[10]);

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
         m_pItemsSelect[5] = new MenuItemSelect(L("Ruby OSD font style"), L("Changes the OSD font style used for Ruby OSD elements."));  
      else
         m_pItemsSelect[5] = new MenuItemSelect(L("OSD font style"), L("Changes the OSD font style."));  
      m_pItemsSelect[5]->addSelection(L("Regular"));
      m_pItemsSelect[5]->addSelection(L("Bold"));
      m_pItemsSelect[5]->setIsEditable();
      m_IndexOSDFontBold = addMenuItem(m_pItemsSelect[5]);
      #endif

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      {
         addMenuItem(new MenuItemSection(L("MSP OSD")));

         m_pItemsSelect[14] = new MenuItemSelect(L("MSP OSD font"), L("Changes the OSD font used for MSP OSD."));  
         m_pItemsSelect[14]->addSelection(L("Auto"));
         m_pItemsSelect[14]->addSelection("Betaflight");
         m_pItemsSelect[14]->addSelection("INAV");
         m_pItemsSelect[14]->addSelection("Ardupilot");
         m_pItemsSelect[14]->addSelection("PitLab");
         m_pItemsSelect[14]->setIsEditable();
         m_IndexMSPOSDFont = addMenuItem(m_pItemsSelect[14]);

         float fSliderWidth = 0.10 * Menu::getScaleFactor();
         m_pItemsSlider[0] = new MenuItemSlider(L("MSP OSD Scale"), L("Sets the relative size of the MSP OSD screen."), 40,140,100, fSliderWidth);
         m_IndexMSPOSDSize = addMenuItem(m_pItemsSlider[0]);

         m_pItemsSlider[1] = new MenuItemSlider(L("MSP OSD Position (H)"), L("Sets the relative horizontal position of the MSP OSD screen."), -30,30,0, fSliderWidth);
         m_IndexMSPOSDDeltaX = addMenuItem(m_pItemsSlider[1]);

         m_pItemsSlider[2] = new MenuItemSlider(L("MSP OSD Position (V)"), L("Sets the relative vertical position of the MSP OSD screen."), -30,30,0, fSliderWidth);
         m_IndexMSPOSDDeltaY = addMenuItem(m_pItemsSlider[2]);
      }
   }

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iDeveloperMode )
   {
      m_pItemsSelect[12] = new MenuItemSelect("Show loop monitor", "Shows a graphical spinner (bottom left of the screen) for monitoring the controller processes.");  
      m_pItemsSelect[12]->addSelection(L("No"));
      m_pItemsSelect[12]->addSelection(L("Yes"));
      m_pItemsSelect[12]->setIsEditable();
      m_pItemsSelect[12]->setTextColor(get_Color_Dev());
      m_IndexMonitor = addMenuItem(m_pItemsSelect[12]);
   }

   if ( ! m_bShowOnlyOSD )
   {
      addMenuItem(new MenuItemSection(L("General")));

      m_pItemsSelect[18] = new MenuItemSelect(L("Language"), L("Change the user interface language."));
      for( int i=0; i<getLanguagesCount(); i++ )
      {
         bool bEnabled = true;
         #if defined (HW_PLATFORM_RASPBERRY)
         // Disable non latin charsets (raster fonts)
         if ( (i == 0) || (i == 4) || (i == 5) )
            bEnabled = false;
         #endif
         if ( i == 4 )
            bEnabled = false;
         m_pItemsSelect[18]->addSelection(L(getLanguageName(i)), bEnabled);
      }
      m_pItemsSelect[18]->setIsEditable();
      m_IndexLanguage = addMenuItem(m_pItemsSelect[18]);
    
      /*
      m_pItemsSelect[15] = new MenuItemSelect(L("Display units"), L("Changes how the OSD displays data: in metric system or imperial system."));  
      m_pItemsSelect[15]->addSelection("Metric (km/h)");
      m_pItemsSelect[15]->addSelection("Metric (m/s)");
      m_pItemsSelect[15]->addSelection("Imperial (mi/h)");
      m_pItemsSelect[15]->addSelection("Imperial (ft/s)");
      m_pItemsSelect[15]->setIsEditable();
      m_IndexUnits = addMenuItem(m_pItemsSelect[15]);

      m_pItemsSelect[4] = new MenuItemSelect(L("Display units (Heights)"), L("Changes how the OSD displays heights: in metric system or imperial system."));  
      //m_pItemsSelect[4]->addSelection("Metric (km)");
      m_pItemsSelect[4]->addSelection("Metric (m)");
      //m_pItemsSelect[4]->addSelection("Imperial (mi)");
      m_pItemsSelect[4]->addSelection("Imperial (ft)");
      m_pItemsSelect[4]->setIsEditable();
      m_IndexUnitsHeight = addMenuItem(m_pItemsSelect[4]);
      */

      m_pItemsSelect[16] = new MenuItemSelect(L("Persist messages longer"), L("Keep the various messages and warnings longer on the screen."));  
      m_pItemsSelect[16]->addSelection(L("No"));
      m_pItemsSelect[16]->addSelection(L("Yes"));
      m_pItemsSelect[16]->setIsEditable();
      m_IndexPersistentMessages = addMenuItem(m_pItemsSelect[16]);

      m_pItemsSelect[17] = new MenuItemSelect(L("Log messages window"), L("Shows the log messages window."));  
      m_pItemsSelect[17]->addSelection(L("Off"));
      m_pItemsSelect[17]->addSelection(L("On New Content"));
      m_pItemsSelect[17]->addSelection(L("Always On"));
      m_pItemsSelect[17]->setIsEditable();
      m_IndexLogWindow = addMenuItem(m_pItemsSelect[17]);
   }
   valuesToUI();
}

void MenuPreferencesUI::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( ! m_bShowOnlyOSD )
   {
      m_pItemsSelect[0]->setSelection(p->iScaleMenus+2);

      if ( p->iMenuStyle == 1 )
         m_pItemsSelect[1]->setSelection(2);
      else if ( p->iMenusStacked )
         m_pItemsSelect[1]->setSelection(1);
      else
         m_pItemsSelect[1]->setSelection(0);

      m_pItemsSelect[9]->setSelectedIndex(p->iShowCompactMenus);
   }

   if ( -1 != m_IndexInvertColors )
      m_pItemsSelect[2]->setSelection(p->iInvertColorsOSD);
   if ( -1 != m_IndexOSDOutlineThickness )
      m_pItemsSelect[3]->setSelection(p->iOSDOutlineThickness+3);
   if ( -1 != m_IndexOSDSize )
      m_pItemsSelect[6]->setSelection(p->iOSDScreenSize);
   if ( -1 != m_IndexOSDFlip )
      m_pItemsSelect[7]->setSelection(p->iOSDFlipVertical);

   if ( -1 != m_IndexOSDFont )
      m_pItemsSelect[10]->setSelectedIndex(p->iOSDFont);
   if ( -1 != m_IndexOSDFontBold )
      m_pItemsSelect[5]->setSelectedIndex(p->iOSDFontBold);

   if ( -1 != m_IndexMonitor )
      m_pItemsSelect[12]->setSelection(p->iShowProcessesMonitor);

   if ( -1 != m_IndexUnits )
   {
      if ( p->iUnits == prefUnitsMetric )
         m_pItemsSelect[15]->setSelection(0);
      if ( p->iUnits == prefUnitsMeters )
         m_pItemsSelect[15]->setSelection(1);
      if ( p->iUnits == prefUnitsImperial )
         m_pItemsSelect[15]->setSelection(2);
      if ( p->iUnits == prefUnitsFeets )
         m_pItemsSelect[15]->setSelection(3);
   }
   if ( -1 != m_IndexUnitsHeight )
   {
      if ( p->iUnitsHeight == prefUnitsMetric )
         m_pItemsSelect[4]->setSelection(0);
      if ( p->iUnitsHeight == prefUnitsMeters )
         m_pItemsSelect[4]->setSelection(0);
      if ( p->iUnitsHeight == prefUnitsImperial )
         m_pItemsSelect[4]->setSelection(1);
      if ( p->iUnitsHeight == prefUnitsFeets )
         m_pItemsSelect[4]->setSelection(1);
   }
   if ( -1 != m_IndexPersistentMessages )
      m_pItemsSelect[16]->setSelection(p->iPersistentMessages);
   if ( -1 != m_IndexLogWindow )
      m_pItemsSelect[17]->setSelection(p->iShowLogWindow);

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( -1 != m_IndexMSPOSDFont )
   {
      int iFont = g_pCurrentModel->osd_params.uFlags & OSD_BIT_FLAGS_MASK_MSPOSD_FONT;
      m_pItemsSelect[14]->setSelectedIndex(iFont);
   }

   if ( -1 != m_IndexLanguage )
      m_pItemsSelect[18]->setSelectedIndex(p->iLanguage);

   if ( -1 != m_IndexMSPOSDSize )
      m_pItemsSlider[0]->setCurrentValue(p->iMSPOSDSize);
   if ( -1 != m_IndexMSPOSDDeltaX )
      m_pItemsSlider[1]->setCurrentValue(p->iMSPOSDDeltaX);
   if ( -1 != m_IndexMSPOSDDeltaY )
      m_pItemsSlider[2]->setCurrentValue(p->iMSPOSDDeltaY);
}

void MenuPreferencesUI::Render()
{
   Preferences* p = get_Preferences();
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( (-1 != m_IndexColorPickerOSD) && (i == m_IndexColorPickerOSD) )
      {
         double color[4];
         color[0] = p->iColorOSD[0];
         color[1] = p->iColorOSD[1];
         color[2] = p->iColorOSD[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      if ( (-1 != m_IndexColorPickerOSDOutline) && (i == m_IndexColorPickerOSDOutline) )
      {
         double color[4];
         color[0] = p->iColorOSDOutline[0];
         color[1] = p->iColorOSDOutline[1];
         color[2] = p->iColorOSDOutline[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      if ( (-1 != m_IndexColorPickerAHI) && (i == m_IndexColorPickerAHI) )
      {
         double color[4];
         color[0] = p->iColorAHI[0];
         color[1] = p->iColorAHI[1];
         color[2] = p->iColorAHI[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      y += RenderItem(i,y);

      if ( i == m_IndexOSDSize-1 )
      {
         g_pRenderEngine->setColors(get_Color_MenuBg());
         g_pRenderEngine->setStroke(get_Color_MenuText());
         Preferences* p = get_Preferences();
         float h = 2*(1+MENU_TEXTLINE_SPACING)*g_pRenderEngine->textHeight(g_idFontMenu);
         float hs = h*0.7;
         float ws = hs;
         float ho = hs;
         float wo = ws;
         float x = m_xPos+m_RenderWidth-2.8*m_sfMenuPaddingX-ws;
         x -= 0.04*m_sfScaleFactor;
         if ( NULL != p && p->iOSDScreenSize == 1 )
         { ho = hs*0.9; wo = ws*0.9; }
         if ( NULL != p && p->iOSDScreenSize == 2 )
         { ho = hs*0.8; wo = ws*0.84; }
         if ( NULL != p && p->iOSDScreenSize == 3 )
         { ho = hs*0.7; wo = ws*0.78; }
         if ( NULL != p && p->iOSDScreenSize == 4 )
         { ho = hs*0.6; wo = ws*0.72; }
         if ( NULL != p && p->iOSDScreenSize == 5 )
         { ho = hs*0.5; wo = ws*0.66; }
         if ( NULL != p && p->iOSDScreenSize == 6 )
         { ho = hs*0.5; wo = ws*0.60; }
         if ( NULL != p && p->iOSDScreenSize == 7 )
         { ho = hs*0.5; wo = ws*0.54; }
         g_pRenderEngine->drawRoundRect(x, y+0.5*(h-hs)-0.3*h, ws, hs, 0.002*m_sfMenuPaddingY);
         g_pRenderEngine->drawRoundRect(x+0.5*(ws-wo), y+0.5*(h-ho)-0.3*h, wo, ho,0.002*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
         //y -= h;
      }
   }
   RenderEnd(yTop);
}

void MenuPreferencesUI::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   Preferences* pP = get_Preferences();

   if ( (m_IndexMSPOSDSize != -1) && (m_IndexMSPOSDSize == itemIndex))
   {
      pP->iMSPOSDSize = m_pItemsSlider[0]->getCurrentValue();
      save_Preferences();
      return;
   }
   if ( (-1 != m_IndexMSPOSDDeltaX) && (m_IndexMSPOSDDeltaX == itemIndex) )
   {
      pP->iMSPOSDDeltaX = m_pItemsSlider[1]->getCurrentValue();
      save_Preferences();
      return;
   }
   if ( (-1 != m_IndexMSPOSDDeltaY) && (m_IndexMSPOSDDeltaY == itemIndex) )
   {
      pP->iMSPOSDDeltaY = m_pItemsSlider[2]->getCurrentValue();
      save_Preferences();
      return;
   }
}

void MenuPreferencesUI::onSelectItem()
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

   if ( ! m_bShowOnlyOSD )
   if ( (-1 != m_IndexScaleMenu) && (m_IndexScaleMenu == m_SelectedIndex) )
   {
      p->iScaleMenus = m_pItemsSelect[0]->getSelectedIndex()-2;
      if ( render_engine_uses_raw_fonts() )
      {
         save_Preferences();
         applyFontScaleChanges();
         menu_invalidate_all();
         popups_invalidate_all();
      }
   }

   if ( ! m_bShowOnlyOSD )
   if ( (-1 != m_IndexMenuStacked) && (m_IndexMenuStacked == m_SelectedIndex) )
   {
      int iIndex = m_pItemsSelect[1]->getSelectedIndex();
      if ( 2 == iIndex )
         p->iMenuStyle = 1;
      else
      {
         p->iMenuStyle = 0;
         if ( 0 == iIndex )
            p->iMenusStacked = 0;
         else
            p->iMenusStacked = 1;
      }
      Menu::setRenderMode(p->iMenuStyle);
      save_Preferences();
      applyFontScaleChanges();
      menu_invalidate_all();
      popups_invalidate_all();

      menu_rearrange_all_menus_xpos_no_animation();
      menu_loop(true);
      menu_invalidate_all();
      popups_invalidate_all();

      return;
   }

   if ( ! m_bShowOnlyOSD )
   if ( (-1 != m_IndexMenusCompact) && (m_IndexMenusCompact == m_SelectedIndex) )
   {
      p->iShowCompactMenus = m_pItemsSelect[9]->getSelectedIndex();
      save_Preferences();
      menu_invalidate_all();
      return;
   }

   if ( (-1 != m_IndexInvertColors) && (m_IndexInvertColors == m_SelectedIndex) )
   {
      p->iInvertColorsOSD = m_pItemsSelect[2]->getSelectedIndex();
      p->iInvertColorsMenu = m_pItemsSelect[2]->getSelectedIndex();
      menu_invalidate_all();
   }

   if ( (-1 != m_IndexOSDFont) && (m_IndexOSDFont == m_SelectedIndex) )
   {
      p->iOSDFont = m_pItemsSelect[10]->getSelectedIndex();
      save_Preferences();
      loadAllFonts(false);
   }

   if ( (-1 != m_IndexOSDFontBold) && (m_IndexOSDFontBold == m_SelectedIndex) )
   {
      p->iOSDFontBold = m_pItemsSelect[5]->getSelectedIndex();
      save_Preferences();
      loadAllFonts(false);    
   }

   if ( (-1 != m_IndexMSPOSDSize) && (m_IndexMSPOSDSize == m_SelectedIndex) )
   {
      p->iMSPOSDSize = m_pItemsSlider[0]->getCurrentValue();
      save_Preferences();
      return;
   }
   if ( (-1 != m_IndexMSPOSDDeltaX) && (m_IndexMSPOSDDeltaX == m_SelectedIndex) )
   {
      p->iMSPOSDDeltaX = m_pItemsSlider[1]->getCurrentValue();
      save_Preferences();
      return;
   }
   if ( (-1 != m_IndexMSPOSDDeltaY) && (m_IndexMSPOSDDeltaY == m_SelectedIndex) )
   {
      p->iMSPOSDDeltaY = m_pItemsSlider[2]->getCurrentValue();
      save_Preferences();
      return;
   }

   if ( (-1 != m_IndexColorPickerOSD) && (m_IndexColorPickerOSD == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuColorPicker());
      return;
   }

   if ( (-1 != m_IndexColorPickerOSDOutline) && (m_IndexColorPickerOSDOutline == m_SelectedIndex) )
   {
      MenuColorPicker* mp = new MenuColorPicker();
      mp->m_ColorType = COLORPICKER_TYPE_OSD_OUTLINE;
      add_menu_to_stack(mp);
      return;
   }

   if ( (-1 != m_IndexColorPickerAHI) && (m_IndexColorPickerAHI == m_SelectedIndex) )
   {
      MenuColorPicker* mp = new MenuColorPicker();
      mp->m_ColorType = COLORPICKER_TYPE_AHI;
      add_menu_to_stack(mp);
      return;
   }

   if ( (-1 != m_IndexOSDOutlineThickness) && (m_IndexOSDOutlineThickness == m_SelectedIndex) )
   {
      p->iOSDOutlineThickness = m_pItemsSelect[3]->getSelectedIndex()-3;
      save_Preferences();
      osd_apply_preferences();
   }

   if ( (-1 != m_IndexOSDSize) && (m_IndexOSDSize == m_SelectedIndex) )
      p->iOSDScreenSize = m_pItemsSelect[6]->getSelectedIndex();

   if ( (-1 != m_IndexOSDFlip) && (m_IndexOSDFlip == m_SelectedIndex) )
      p->iOSDFlipVertical = m_pItemsSelect[7]->getSelectedIndex();

   if ( (-1 != m_IndexMonitor) && (m_IndexMonitor == m_SelectedIndex) )
   {
      p->iShowProcessesMonitor = m_pItemsSelect[12]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexUnits) && (m_IndexUnits == m_SelectedIndex) )
   {
      int nSel = m_pItemsSelect[15]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnits = prefUnitsMetric;
      if ( 1 == nSel )
         p->iUnits = prefUnitsMeters;
      if ( 2 == nSel )
         p->iUnits = prefUnitsImperial;
      if ( 3 == nSel )
         p->iUnits = prefUnitsFeets;
   }

   if ( (-1 != m_IndexUnitsHeight) && (m_IndexUnitsHeight == m_SelectedIndex) )
   {
      int nSel = m_pItemsSelect[4]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnitsHeight = prefUnitsMeters;
      if ( 1 == nSel )
         p->iUnitsHeight = prefUnitsFeets;
   }

   if ( (-1 != m_IndexPersistentMessages) && (m_IndexPersistentMessages == m_SelectedIndex) )
      p->iPersistentMessages = m_pItemsSelect[16]->getSelectedIndex();

   if ( (-1 != m_IndexLogWindow) && (m_IndexLogWindow == m_SelectedIndex) )
   {
      p->iShowLogWindow = m_pItemsSelect[17]->getSelectedIndex();
      popup_log_set_show_flag(p->iShowLogWindow);
      menu_invalidate_all();
      valuesToUI();
      save_Preferences();
      return;
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( (-1 != m_IndexMSPOSDFont) && (m_IndexMSPOSDFont == m_SelectedIndex) )
   {
      u32 uFont = m_pItemsSelect[14]->getSelectedIndex();
      g_pCurrentModel->osd_params.uFlags &= ~OSD_BIT_FLAGS_MASK_MSPOSD_FONT;
      g_pCurrentModel->osd_params.uFlags |= (uFont & OSD_BIT_FLAGS_MASK_MSPOSD_FONT);
   }

   if ( (-1 != m_IndexLanguage) && (m_IndexLanguage == m_SelectedIndex) )
   {
      p->iLanguage = m_pItemsSelect[18]->getSelectedIndex();
      save_Preferences();
      setActiveLanguage(p->iLanguage); 
      valuesToUI();
      menu_refresh_all_menus();
      return;
   }
   save_Preferences();
}
