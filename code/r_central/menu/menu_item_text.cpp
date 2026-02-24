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

#include "menu_item_text.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemText::MenuItemText(const char* title)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsSelectable = false;
   m_bIsEditable = false;
   m_bUseSmallText = false;
   m_fScale = 1.0;
   m_fMarginX = 0;// 0.5 * Menu::getMenuPaddingX();
}

MenuItemText::MenuItemText(const char* title, bool bUseSmallText)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsSelectable = false;
   m_bIsEditable = false;
   m_bUseSmallText = bUseSmallText;
   m_fScale = 1.0;
   m_fMarginX = 0; //0.5 * Menu::getMenuPaddingX();
}

MenuItemText::MenuItemText(const char* title,  bool bUseSmallText, float fMargin)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsSelectable = false;
   m_bIsEditable = false;
   m_bUseSmallText = bUseSmallText;
   m_fScale = 1.0;
   m_fMarginX = fMargin;
}

MenuItemText::~MenuItemText()
{
}

bool MenuItemText::isSelectable()
{
   return m_bIsSelectable;
}

void MenuItemText::makeSelectable()
{
   m_bIsSelectable = true;
}

void MenuItemText::setSmallText()
{
   m_bUseSmallText = true;
}

float MenuItemText::getItemHeight(float maxWidth)
{
   if ( m_bUseSmallText )
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, g_idFontMenuSmall);
   else
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, g_idFontMenu);
   m_RenderHeight += m_fExtraHeight;
   return m_RenderHeight;
}

float MenuItemText::getTitleWidth(float maxWidth)
{
   return 0.0;
   //return MenuItem::getTitleWidth(maxWidth);
}


void MenuItemText::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;

   if ( m_bEnabled )
   {
      if ( m_bCustomTextColor )
         g_pRenderEngine->setColors(&m_TextColor[0]);
      else
         g_pRenderEngine->setColors(get_Color_MenuText());
   }
   else
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());

   if ( m_bUseSmallText )
      g_pRenderEngine->drawMessageLines(xPos + m_fMarginX, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_pMenu->getUsableWidth()-m_fMarginX, g_idFontMenuSmall);
   else
      g_pRenderEngine->drawMessageLines(xPos + m_fMarginX, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_pMenu->getUsableWidth()-m_fMarginX, g_idFontMenu);

   if ( m_bHighlightFirstWord )
   {
      char szTmp[1024];
      strncpy(szTmp, m_pszTitle, 1023);
      szTmp[1023] = 0;
      char *p = szTmp;
      while ( *p )
      {
         if ( *p == ' ' )
         {
            *p = 0;
            break;
         }
         p++;
      }
      g_pRenderEngine->setColors(get_Color_IconError());
      if ( m_bUseSmallText )
         g_pRenderEngine->drawText(xPos + m_fMarginX, yPos, g_idFontMenuSmall, szTmp);
      else
         g_pRenderEngine->drawText(xPos + m_fMarginX, yPos, g_idFontMenu, szTmp);
   }
}
