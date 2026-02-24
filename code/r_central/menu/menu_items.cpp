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

#include <math.h>
#include "menu_items.h"
#include "menu_objects.h"
#include "menu.h"

MenuItem::MenuItem(const char* title)
{
   m_ItemType = MENU_ITEM_TYPE_SIMPLE;
   m_bEnabled = true;
   m_bHidden = false;
   m_bIsEditable = false;
   m_bHighlightFirstWord = false;
   m_bEndEditOnBackOnly = false;
   m_bIsEditing = false;
   m_bShowArrow = false;
   m_bCondensedOnly = false;
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   m_RenderLastY = 0.0;
   m_RenderLastX = 0.0;

   m_fExtraHeight = 0.0;
   m_bCustomTextColor = false;
   m_pMenu = NULL;
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
   m_pszValue = NULL;
   m_fMarginX = 0.0;
   setTitle(title);
   setTooltip("");
}

MenuItem::MenuItem(const char* title, const char* tooltip)
{
   m_ItemType = MENU_ITEM_TYPE_SIMPLE;
   m_bEnabled = true;
   m_bHidden = false;
   m_bHighlightFirstWord = false;
   m_bIsEditable = false;
   m_bEndEditOnBackOnly = false;
   m_bIsEditing = false;
   m_bShowArrow = false;
   m_bCondensedOnly = false;
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   m_RenderLastY = 0.0;
   m_RenderLastX = 0.0;

   m_fExtraHeight = 0.0;
   m_bCustomTextColor = false;
   m_pMenu = NULL;
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
   m_pszValue = NULL;
   m_fMarginX = 0.0;
   setTitle(title);
   setTooltip(tooltip);
}


MenuItem::~MenuItem()
{
   if ( NULL != m_pszTitle )
      free(m_pszTitle);
   if ( NULL != m_pszTooltip )
      free(m_pszTooltip);
   if ( NULL != m_pszValue )
      free(m_pszValue);
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
   m_pszValue = NULL;
}

bool MenuItem::isEnabled()
{
   return m_bEnabled;
}

bool MenuItem::isSelectable()
{
   return m_bEnabled;
}

void MenuItem::setEnabled(bool enabled)
{
   m_bEnabled = enabled;
}

void MenuItem::setIsEditable() { m_bIsEditable = true; }
void MenuItem::setNotEditable() { m_bIsEditable = false; }
void MenuItem::beginEdit() { if ( m_bIsEditable ) m_bIsEditing = true; }
void MenuItem::endEdit(bool bCanceled) { if ( m_bIsEditable ) m_bIsEditing = false; }
bool MenuItem::isEditing() { return m_bIsEditing; }
bool MenuItem::isEditable() { return m_bIsEditable; }
bool MenuItem::isEndEditOnBackOnly() { return m_bEndEditOnBackOnly; }

void MenuItem::showArrow() { m_bShowArrow = true; }
void MenuItem::setCondensedOnly() { m_bCondensedOnly = true; }

void MenuItem::setHidden(bool bHidden)
{
   m_bHidden = bHidden;
}

bool MenuItem::isHidden()
{
   return m_bHidden;
}

void MenuItem::highlightFirstWord(bool bHighlight)
{
   m_bHighlightFirstWord = bHighlight;
}

void MenuItem::setExtraHeight(float fExtraHeight)
{
   m_fExtraHeight = fExtraHeight;
}

float MenuItem::getExtraHeight()
{
   return m_fExtraHeight;
}


void MenuItem::setMargin(float fMargin)
{
   m_fMarginX = fMargin;
}

void MenuItem::setTitle( const char* title )
{
   if ( NULL != m_pszTitle )
      free(m_pszTitle);

   if ( NULL == title || 0 == title[0] )
   {
      m_pszTitle = (char*)malloc(2);
      m_pszTitle[0] = 0;
      return;
   }
   m_pszTitle = (char*)malloc(strlen(title)+1);
   strcpy(m_pszTitle, title);

   invalidate();

   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   //if ( NULL != m_pMenu )
   //   m_pMenu->invalidate();
}

char* MenuItem::getTitle()
{
   return m_pszTitle;
}

void MenuItem::setValue(const char* szValue)
{
   if ( NULL != m_pszValue )
      free(m_pszValue);

   if ( NULL == szValue || 0 == szValue[0] )
   {
      m_pszValue = (char*)malloc(2);
      m_pszValue[0] = 0;
      return;
   }
   m_pszValue = (char*)malloc(strlen(szValue)+1);
   strcpy(m_pszValue, szValue);

   invalidate();
}

void MenuItem::setTooltip( const char* tooltip )
{
   if ( NULL != m_pszTooltip )
      free(m_pszTooltip);

   if ( NULL == tooltip || 0 == tooltip[0])
   {
      m_pszTooltip = (char*)malloc(1);
      m_pszTooltip[0] = 0;
      return;
   }
   m_pszTooltip = (char*)malloc(strlen(tooltip)+1);
   strcpy(m_pszTooltip, tooltip);
}

char* MenuItem::getTooltip()
{
   return m_pszTooltip;
}

void MenuItem::setTextColor(const double* pColor)
{
   if ( NULL == pColor )
      return;

   m_bCustomTextColor = true;
   memcpy(&m_TextColor[0], pColor, 4*sizeof(double));
}

void MenuItem::invalidate()
{
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
}

bool MenuItem::preProcessKeyUp()
{
   return false;
}

bool MenuItem::preProcessKeyDown()
{
   return false;
}

bool MenuItem::preProcessKeyLeft()
{
   return preProcessKeyUp();
}

bool MenuItem::preProcessKeyRight()
{
   return preProcessKeyDown();
}

void MenuItem::onClick()
{
}

float MenuItem::getItemRenderYPos()
{
   return m_RenderLastY;
}

float MenuItem::getItemHeight(float maxWidth)
{
   m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderHeight = m_RenderTitleHeight + m_fExtraHeight;
   return m_RenderHeight;
}

float MenuItem::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);

   if ( m_bShowArrow )
      m_RenderTitleWidth += 0.66*g_pRenderEngine->textHeight(g_idFontMenu);
   return m_RenderTitleWidth;
}

float MenuItem::getValueWidth(float maxWidth)
{
   return 0.0;
}

void MenuItem::RenderBaseTitle(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float width_text = m_RenderTitleWidth;

   float selectionMarginX = Menu::getSelectionPaddingX();
   float selectionMarginY = Menu::getSelectionPaddingY();

   if ( bSelected && (!m_bIsEditing) )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-selectionMarginX, yPos-selectionMarginY, fWidthSelection+2.0*selectionMarginX, height_text+2.0*selectionMarginY + g_pRenderEngine->getPixelHeight(), selectionMarginY);
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   }
   else
   {
      if ( m_bEnabled )
      {
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
         g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
   }
   g_pRenderEngine->drawText(xPos + m_fMarginX, yPos+g_pRenderEngine->getPixelHeight()*0.2, g_idFontMenu, m_pszTitle);

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
      g_pRenderEngine->drawText(xPos + m_fMarginX, yPos+g_pRenderEngine->getPixelHeight()*0.2, g_idFontMenu, szTmp);
   }

   float xEnd = xPos + width_text - 2.0*g_pRenderEngine->getPixelWidth();
   float yEnd = yPos + height_text*0.5;

   if ( m_bShowArrow )
   {
      if ( (!bSelected) )
      {
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
      if ( ! m_bEnabled )
         g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());

      float size = height_text*0.28;

      //g_pRenderEngine->drawTriangle(xEnd,yEnd, xEnd-size, yEnd+size, xEnd-size, yEnd-size);
      g_pRenderEngine->fillTriangle(xEnd,yEnd, xEnd-size, yEnd+size, xEnd-size, yEnd-size);
      g_pRenderEngine->setColors(get_Color_MenuText());
      xEnd -= size;
   }

   if ( (NULL != m_pszValue) && (0 != m_pszValue[0]) )
   {
      if ( NULL != m_pMenu )
         xEnd = m_pMenu->getRenderXPos() + m_pMenu->getRenderWidth() - m_fMarginX - Menu::getMenuPaddingX();
      g_pRenderEngine->drawTextLeft(xEnd, yPos+g_pRenderEngine->getPixelHeight()*0.2, g_idFontMenu, m_pszValue);
   }
}

void MenuItem::setLastRenderPos(float xPos, float yPos)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
}

void MenuItem::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
}

void MenuItem::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
}
