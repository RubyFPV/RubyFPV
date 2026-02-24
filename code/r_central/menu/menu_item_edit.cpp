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

#include "menu_item_edit.h"
#include "menu_objects.h"
#include "menu.h"

MenuItemEdit::MenuItemEdit(const char* title, const char* szValue)
:MenuItem(title)
{
   m_bOnlyLetters = false;
   m_bIsEditable = true;
   m_bEndEditOnBackOnly = true;
   m_szLastEditChar = 0;
   m_bCurrentEditCharFirstChange = true;
   memset(m_szValue, 0, MAX_EDIT_LENGTH+2);
   memset(m_szValueOriginal, 0, MAX_EDIT_LENGTH+2);
   if ( NULL == szValue )
   {
      m_szValue[0] = 0;
      m_szValueOriginal[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
      strcpy(m_szValueOriginal, szValue);
   }
   m_EditPos = 0;
   m_MaxLength = MAX_EDIT_LENGTH;
}

MenuItemEdit::MenuItemEdit(const char* title, const char* tooltip, const char* szValue)
:MenuItem(title, tooltip)
{
   m_bOnlyLetters = false;
   m_bIsEditable = true;
   m_bEndEditOnBackOnly = true;
   m_szLastEditChar = 0;
   m_bCurrentEditCharFirstChange = true;
   memset(m_szValue, 0, MAX_EDIT_LENGTH+2);
   memset(m_szValueOriginal, 0, MAX_EDIT_LENGTH+2);

   if ( NULL == szValue )
   {
      m_szValue[0] = 0;
      m_szValueOriginal[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
      strcpy(m_szValueOriginal, szValue);
   }
   m_EditPos = 0;
   m_MaxLength = MAX_EDIT_LENGTH;
}

MenuItemEdit::~MenuItemEdit()
{
}

void MenuItemEdit::setOnlyLetters()
{
   m_bOnlyLetters = true;
}

void MenuItemEdit::setMaxLength(int l)
{
   m_MaxLength = l;
}

void MenuItemEdit::setCurrentValue(const char* szValue)
{
   memset(m_szValue, 0, MAX_EDIT_LENGTH+2);
   memset(m_szValueOriginal, 0, MAX_EDIT_LENGTH+2);

   if ( NULL == szValue || 0 == szValue[0] )
   {
      m_szValue[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
   }
   strcpy(m_szValueOriginal, m_szValue);
   invalidate();
   m_RenderValueWidth = 0.0;
}

const char* MenuItemEdit::getCurrentValue()
{
   return m_szValue;
}

void MenuItemEdit::beginEdit()
{
   if ( ! m_bIsEditing )
   {
      MenuItem::beginEdit();
      m_EditPos = 0;
      return;
   }

   // It's already editing, move to next position

   if ( m_szValue[m_EditPos] == 0 )
      m_szValue[m_EditPos] = ' ';

   m_szLastEditChar = m_szValue[m_EditPos];
   m_bCurrentEditCharFirstChange = true;
   m_EditPos++;

   if ( m_EditPos >= m_MaxLength )
      m_EditPos = 0;

   if ( 0 == m_szValue[m_EditPos] )
     m_szValue[m_EditPos] = ' ';
   else
     m_bCurrentEditCharFirstChange = false;

   m_RenderValueWidth = 0.0;
}


void MenuItemEdit::onKeyUp(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;

   if ( m_bCurrentEditCharFirstChange )
   {
      m_bCurrentEditCharFirstChange = false;
      m_szValue[m_EditPos] = m_szLastEditChar;
   }

   if ( 0 == m_szValue[m_EditPos] )
   {
      m_szValue[m_EditPos] = 65;
      m_szValue[m_EditPos+1] = 0;
   }
   else
   {
      m_szValue[m_EditPos]--;
      if ( m_bOnlyLetters )
      {
         if ( m_szValue[m_EditPos] == 64 )
            m_szValue[m_EditPos] = 32;
         if ( m_szValue[m_EditPos] < 32 )
            m_szValue[m_EditPos] = 90;
      }
      if ( (!m_bOnlyLetters) && m_szValue[m_EditPos] < 32 )
         m_szValue[m_EditPos] = 122;
   }

   m_RenderValueWidth = 0.0;
}

void MenuItemEdit::onKeyDown(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;

   if ( m_bCurrentEditCharFirstChange )
   {
      m_bCurrentEditCharFirstChange = false;
      m_szValue[m_EditPos] = m_szLastEditChar;
   }

   if ( 0 == m_szValue[m_EditPos] )
   {
      m_szValue[m_EditPos] = 65;
      m_szValue[m_EditPos+1] = 0;
   }
   else
   {
      m_szValue[m_EditPos]++;
      if ( m_bOnlyLetters )
      {
         if ( m_szValue[m_EditPos] == 33 )
            m_szValue[m_EditPos] = 65;
         if ( m_szValue[m_EditPos] > 90 )
            m_szValue[m_EditPos] = 32;
      }
      if ( (!m_bOnlyLetters) && m_szValue[m_EditPos] > 122 )
         m_szValue[m_EditPos] = 32;
   }

   m_RenderValueWidth = 0.0;
}

void MenuItemEdit::onKeyLeft(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;
}

void MenuItemEdit::onKeyRight(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;
}

float MenuItemEdit::getValueWidth(float maxWidth)
{
   if ( m_RenderValueWidth > 0.01 )
      return m_RenderValueWidth;

   if ( (0 == m_szValue[0]) || ((' ' == m_szValue[0]) && (0 == m_szValue[1])) )
      m_RenderValueWidth = g_pRenderEngine->textWidth(g_idFontMenu, "[Not set]");
   else
      m_RenderValueWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_szValue);
   return m_RenderValueWidth;
}

void MenuItemEdit::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
   getValueWidth(0);
   float padding = Menu::getSelectionPaddingY();
   float paddingH = Menu::getSelectionPaddingX();

   if ( ! m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      if ( (0 == m_szValue[0]) || ((' ' == m_szValue[0]) && (0 == m_szValue[1])) )
         g_pRenderEngine->drawText(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth, yPos, g_idFontMenu, "[Not set]");
      else
         g_pRenderEngine->drawText(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth, yPos, g_idFontMenu, m_szValue);
      return;
   }

   // Editing

   g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
   g_pRenderEngine->drawRoundRect(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth - paddingH, yPos-0.8*padding, m_RenderValueWidth + 2*paddingH, m_RenderTitleHeight + 2*padding, 0.01*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());

   u32 timeNow = get_current_timestamp_ms();
   bool flash = false;
   if ( (timeNow/400)%2 )
      flash = true;

   float x = xPos + m_pMenu->getUsableWidth()-m_RenderValueWidth;
   char szBuff[16];
   double cInv[4] = { 255,255,255,0.8 };
   for( int i=0; i<m_MaxLength; i++ )
   {
      szBuff[0] = m_szValue[i];
      szBuff[1] = 0;
      float w = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      if ( flash && (i == m_EditPos))
      {
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
         g_pRenderEngine->drawRect(x-0.001*Menu::getMenuPaddingX(), yPos-0.003*Menu::getMenuPaddingY(), w + 0.002*Menu::getMenuPaddingX(), m_RenderTitleHeight + 2*0.003*Menu::getMenuPaddingY());
         g_pRenderEngine->setColors(cInv);
      }
      else
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());

      if ( 0 != szBuff[0] )
         g_pRenderEngine->drawText(x, yPos, g_idFontMenu, szBuff);
      if ( szBuff[0] == 0 )
         break;
      x += w;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
}
