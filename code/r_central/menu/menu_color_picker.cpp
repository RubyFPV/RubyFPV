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
#include "../../base/models.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "../colors.h"
#include "../fonts.h"
#include "../osd/osd_common.h"
#include "../../renderer/render_engine.h"
#include "menu.h"

#include "menu_color_picker.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

#include "../shared_vars.h"
#include "../ruby_central.h"


MenuColorPicker::MenuColorPicker(void)
:Menu(MENU_ID_COLOR_PICKER, L("Color Picker"), NULL)
{
   m_Width = 0.1;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.40;

   m_pItemsRange[0] = new MenuItemRange("R:", "", 1, 255, 255, 1 );
   m_pItemsRange[0]->setSufix("");
   m_IndexColor[0] = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("G:", "", 1, 255, 255, 1 );
   m_pItemsRange[1]->setSufix("");
   m_IndexColor[1] = addMenuItem(m_pItemsRange[1]);

   m_pItemsRange[2] = new MenuItemRange("B:", "", 1, 255, 255, 1 );
   m_pItemsRange[2]->setSufix("");
   m_IndexColor[2] = addMenuItem(m_pItemsRange[2]);

   m_ColorType = COLORPICKER_TYPE_OSD;
}

MenuColorPicker::~MenuColorPicker()
{
}

void MenuColorPicker::valuesToUI()
{
   Preferences* p = get_Preferences();

   if ( m_ColorType == COLORPICKER_TYPE_OSD )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorOSD[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorOSD[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorOSD[2]);
   }
   if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorOSDOutline[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorOSDOutline[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorOSDOutline[2]);
   }
   if ( m_ColorType == COLORPICKER_TYPE_AHI )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorAHI[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorAHI[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorAHI[2]);
   }

   m_Color[0] = m_pItemsRange[0]->getCurrentValue();
   m_Color[1] = m_pItemsRange[1]->getCurrentValue();
   m_Color[2] = m_pItemsRange[2]->getCurrentValue();
   m_Color[3] = 1.0;
}


void MenuColorPicker::Render()
{
   RenderPrepare();
   float sizeRectV = m_RenderHeight - m_RenderHeaderHeight - 2*m_sfMenuPaddingY;
   float sizeRectH = sizeRectV/g_pRenderEngine->getAspectRatio();

   m_RenderWidth += sizeRectH + m_sfMenuPaddingX;
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   m_RenderWidth -= sizeRectH + m_sfMenuPaddingX;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   g_pRenderEngine->setColors(&m_Color[0], 1.0);
   g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth, yTop, sizeRectH, sizeRectV, 0.003);
}

void MenuColorPicker::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
   m_Color[0] = m_pItemsRange[0]->getCurrentValue();
   m_Color[1] = m_pItemsRange[1]->getCurrentValue();
   m_Color[2] = m_pItemsRange[2]->getCurrentValue();
   m_Color[3] = 1.0;
}

void MenuColorPicker::onSelectItem()
{
   Menu::onSelectItem();
   Preferences* p = get_Preferences();
   bool bChanged = false;
   if ( (m_IndexColor[0] == m_SelectedIndex) && (! m_pMenuItems[m_IndexColor[0]]->isEditing()) )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[0] = m_pItemsRange[0]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[0] = m_pItemsRange[0]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[0] = m_pItemsRange[0]->getCurrentValue();
      save_Preferences();
      bChanged = true;
   }
   if ( (m_IndexColor[1] == m_SelectedIndex) && (! m_pMenuItems[m_IndexColor[1]]->isEditing()) )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[1] = m_pItemsRange[1]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[1] = m_pItemsRange[1]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[1] = m_pItemsRange[1]->getCurrentValue();
      save_Preferences();
      bChanged = true;
   }
   if ( (m_IndexColor[2] == m_SelectedIndex) && (! m_pMenuItems[m_IndexColor[2]]->isEditing()) )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[2] = m_pItemsRange[2]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[2] = m_pItemsRange[2]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[2] = m_pItemsRange[2]->getCurrentValue();
      save_Preferences();
      bChanged = true;
   }

   if ( bChanged )
   if ( (m_ColorType == COLORPICKER_TYPE_OSD) || (m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE) )
   {
      osd_reload_msp_resources();
      loadAllFonts(true);
   }
}