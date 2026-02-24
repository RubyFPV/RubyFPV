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
#include "menu_item_checkbox.h"
#include "menu_objects.h"


MenuItemCheckbox::MenuItemCheckbox(const char* title)
:MenuItem(title)
{
   m_bChecked = false;
}

MenuItemCheckbox::MenuItemCheckbox(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_bChecked = false;
}

MenuItemCheckbox::~MenuItemCheckbox()
{
}

bool MenuItemCheckbox::isChecked()
{
   return m_bChecked;
}

void MenuItemCheckbox::setChecked(bool bChecked)
{
   m_bChecked = bChecked;
}


void MenuItemCheckbox::beginEdit()
{
   MenuItem::beginEdit();
}

void MenuItemCheckbox::endEdit(bool bCanceled)
{
   MenuItem::endEdit(bCanceled);
}

void MenuItemCheckbox::onClick()
{
   m_bChecked = ! m_bChecked;
   invalidate();
}

float MenuItemCheckbox::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);

   return m_RenderTitleWidth;
}

float MenuItemCheckbox::getValueWidth(float maxWidth)
{
   return getItemHeight(0.0) + Menu::getMenuPaddingY();
}


void MenuItemCheckbox::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float fCheckWidth = getValueWidth(0.0);
   RenderCondensed(xPos, yPos, bSelected, fWidthSelection);
   RenderBaseTitle(xPos+fCheckWidth, yPos, bSelected, fWidthSelection);
}


void MenuItemCheckbox::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;

   float paddingV = Menu::getSelectionPaddingY();
   float paddingH = Menu::getSelectionPaddingX();
   float width = m_RenderTitleHeight/g_pRenderEngine->getAspectRatio();

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, width + 2.0*paddingH, m_RenderTitleHeight + 2.0*paddingV, 0.01*Menu::getMenuPaddingY());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, width + 2.0*paddingH, m_RenderTitleHeight + 2.0*paddingV, 0.01*Menu::getMenuPaddingY());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   else
   {
      if ( m_bEnabled )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else
         g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
   }

   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->setStrokeSize(2);
   float corner = 0.1*Menu::getMenuPaddingY();
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, m_RenderTitleHeight, corner);
   corner = 0.4*corner;
   float cornerH = corner/g_pRenderEngine->getAspectRatio();
   if ( m_bChecked )
   {
      g_pRenderEngine->drawLine(xPos+cornerH, yPos+corner, xPos+width-cornerH, yPos+m_RenderTitleHeight-corner);
      g_pRenderEngine->drawLine(xPos+cornerH, yPos+m_RenderTitleHeight-corner, xPos+width-cornerH, yPos+corner);
   }
}
