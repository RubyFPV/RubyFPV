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

#include "menu_item_range.h"
#include "menu_objects.h"
#include "menu.h"

MenuItemRange::MenuItemRange(const char* title, float min, float max, float current, float step)
:MenuItem(title)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueCurrent = current;
   m_ValueStep = step;
   m_bIsEditable = true;
   m_szSufix[0] = 0;
   m_szPrefix[0] = 0;
}

MenuItemRange::MenuItemRange(const char* title, const char* tooltip, float min, float max, float current, float step)
:MenuItem(title, tooltip)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueCurrent = current;
   m_ValueStep = step;
   m_bIsEditable = true;
   m_szSufix[0] = 0;
   m_szPrefix[0] = 0;
}

MenuItemRange::~MenuItemRange()
{
}

void MenuItemRange::setSufix(const char* sufix)
{
   if ( NULL == sufix )
      m_szSufix[0] = 0;
   else
      strcpy(m_szSufix, sufix);
}

void MenuItemRange::setPrefix(const char* szPrefix)
{
   if ( NULL == szPrefix )
      m_szPrefix[0] = 0;
   else
      strcpy(m_szPrefix, szPrefix);
}

float MenuItemRange::getCurrentValue()
{
   return m_ValueCurrent;
}

void MenuItemRange::setCurrentValue(float val)
{
   m_ValueCurrent = val;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyUp(bool bIgnoreReversion)
{
   m_ValueCurrent -= m_ValueStep;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyDown(bool bIgnoreReversion)
{
   m_ValueCurrent += m_ValueStep;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyLeft(bool bIgnoreReversion)
{
   m_ValueCurrent -= m_ValueStep;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyRight(bool bIgnoreReversion)
{
   m_ValueCurrent += m_ValueStep;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

float MenuItemRange::getValueWidth(float maxWidth)
{
   //if ( (!m_bIsEditing) && m_RenderValueWidth > 0.01 )
   //   return m_RenderValueWidth;

   char szBuff[128];
   sprintf(szBuff, "%.1f", m_ValueCurrent);
   if ( szBuff[strlen(szBuff)-1] == '0' )
   if ( szBuff[strlen(szBuff)-2] == '.' )
      szBuff[strlen(szBuff)-2] = 0;
   if ( 0 != m_szPrefix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szPrefix);
   }
   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }
   m_RenderValueWidth = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   return m_RenderValueWidth;
}

void MenuItemRange::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_bCondensedOnly )
      return;

   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);

   float paddingX = Menu::getSelectionPaddingX();
   float paddingY = Menu::getSelectionPaddingY();

   char szBuff[128];
   szBuff[0] = 0;
   if ( 0 != m_szPrefix[0] )
   {
      strcat(szBuff, m_szPrefix);
      strcat(szBuff, " ");
   }

   char szValue[32];
   sprintf(szValue, "%.1f", m_ValueCurrent);
   if ( szValue[strlen(szValue)-1] == '0' )
   if ( szValue[strlen(szValue)-2] == '.' )
      szValue[strlen(szValue)-2] = 0;
   strcat(szBuff, szValue);

   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }

   m_RenderValueWidth = getValueWidth(m_pMenu->getUsableWidth());
   float width = m_RenderValueWidth + 2.0*paddingX;

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth-paddingX, yPos-paddingY, width, m_RenderTitleHeight + 2.0*paddingY, 0.1*Menu::getMenuPaddingY());
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
   g_pRenderEngine->drawText(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth, yPos, g_idFontMenu, szBuff);
}

void MenuItemRange::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;

   float paddingX = Menu::getSelectionPaddingX();
   float paddingY = Menu::getSelectionPaddingY();

   char szBuff[32];
   sprintf(szBuff, "%.1f", m_ValueCurrent);
   if ( szBuff[strlen(szBuff)-1] == '0' )
   if ( szBuff[strlen(szBuff)-2] == '.' )
      szBuff[strlen(szBuff)-2] = 0;
   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }

   if ( m_bIsEditing )
      m_RenderValueWidth = getValueWidth(m_pMenu->getUsableWidth());
   float width = m_RenderValueWidth + 2.0*paddingX;

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-paddingX, yPos-paddingY, width, m_RenderTitleHeight + 2.0*paddingY, 0.1*Menu::getMenuPaddingY());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-paddingX, yPos-paddingY, width, m_RenderTitleHeight + 2.0*paddingY, 0.1*Menu::getMenuPaddingY());
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
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenu, szBuff);
}
