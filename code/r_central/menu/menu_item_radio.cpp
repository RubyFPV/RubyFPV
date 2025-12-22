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

#include "menu_item_radio.h"
#include "menu_objects.h"
#include "menu.h"

MenuItemRadio::MenuItemRadio(const char* title)
:MenuItem(title)
{
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_fSelectorWidth = 0.9*height_text;
   m_fLegendDx = 0.5 * Menu::getMenuPaddingX();
   m_bSmallLegend = false;
}

MenuItemRadio::MenuItemRadio(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_fSelectorWidth = 0.9*height_text;
   m_fLegendDx = 0.5 * Menu::getMenuPaddingX();
   m_bSmallLegend = false;
}

MenuItemRadio::~MenuItemRadio()
{
   removeAllSelections();
}

void MenuItemRadio::removeAllSelections()
{
   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      free( m_szSelections[i] );
      if ( NULL != m_szSelectionsLegend[i] )
         free( m_szSelectionsLegend[i] );
   }
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
}

void MenuItemRadio::addSelection(const char* szText)
{
   if ( NULL == szText || m_nSelectionsCount >= MAX_MENU_ITEM_RADIO_SELECTIONS - 1 )
      return;

   if ( -1 == m_nSelectedIndex )
      m_nSelectedIndex = 0;

   m_szSelections[m_nSelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_nSelectionsCount], szText);
   m_szSelectionsLegend[m_nSelectionsCount] = NULL;
   m_bEnabledSelections[m_nSelectionsCount] = true;
   m_nSelectionsCount++;
}

void MenuItemRadio::addSelection(const char* szText, const char* szLegend)
{
   if ( NULL == szText || m_nSelectionsCount >= MAX_MENU_ITEM_RADIO_SELECTIONS - 1 )
      return;

   if ( -1 == m_nSelectedIndex )
      m_nSelectedIndex = 0;

   m_szSelections[m_nSelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_nSelectionsCount], szText);
   m_szSelectionsLegend[m_nSelectionsCount] = NULL;
   if ( NULL != szLegend && 0 != szLegend[0] )
   {
      bool bAddSeparator = false;
      int len = strlen(szLegend)+1;
      if ( szLegend[len-2] != '.' && szLegend[len-2] != ';' )
      {
         len++;
         bAddSeparator = true;
      }
      m_szSelectionsLegend[m_nSelectionsCount] = (char*)malloc(len);
      strcpy(m_szSelectionsLegend[m_nSelectionsCount], szLegend);
      if ( bAddSeparator )
         strcat(m_szSelectionsLegend[m_nSelectionsCount], ".");
   }
   m_bEnabledSelections[m_nSelectionsCount] = true;
   m_nSelectionsCount++;
}

void MenuItemRadio::setSelectedIndex(int index)
{
   if ( index >= 0 && index < m_nSelectionsCount )
      m_nSelectedIndex = index;
   else
      m_nSelectedIndex = 0;
}


void MenuItemRadio::enableSelectionIndex(int index, bool bEnable)
{
   if ( index >= 0 && index < m_nSelectionsCount )
   {
      if ( m_nFocusedIndex == index && (!bEnable) )
         setNextAvailableFocusableItem();

      m_bEnabledSelections[index] = bEnable;
   }
}

void MenuItemRadio::setFocusedIndex(int index)
{
   if ( index >= 0 && index < m_nSelectionsCount )
   {
      m_nFocusedIndex = index;
      if ( ! m_bEnabledSelections[m_nFocusedIndex] )
         setNextAvailableFocusableItem();
   }
}

int MenuItemRadio::getFocusedIndex()
{
   return m_nFocusedIndex;
}

int MenuItemRadio::getSelectedIndex()
{
   return m_nSelectedIndex;
}

int MenuItemRadio::getSelectionsCount()
{
   return m_nSelectionsCount;
}

void MenuItemRadio::useSmallLegend(bool bSmall)
{
   m_bSmallLegend = bSmall;
}

bool MenuItemRadio::setPrevAvailableFocusableItem()
{
   while ( m_nFocusedIndex >= 0 )
   {
      m_nFocusedIndex--;
      if ( m_nFocusedIndex < 0 )
         break;
      if ( m_bEnabledSelections[m_nFocusedIndex] )
         return true;
   }

   if ( m_nFocusedIndex < 0 )
      m_nFocusedIndex = 0;
   return false;
}

bool MenuItemRadio::setNextAvailableFocusableItem()
{
   while ( m_nFocusedIndex < m_nSelectionsCount )
   {
      m_nFocusedIndex++;
      if ( m_nFocusedIndex >= m_nSelectionsCount )
         break;
      if ( m_bEnabledSelections[m_nFocusedIndex] )
         return true;
   }

   if ( m_nFocusedIndex >= m_nSelectionsCount )
      m_nFocusedIndex = m_nSelectionsCount-1;
   return false;
}

bool MenuItemRadio::preProcessKeyUp()
{
   return setPrevAvailableFocusableItem();
}

bool MenuItemRadio::preProcessKeyDown()
{
   return setNextAvailableFocusableItem();
}

void MenuItemRadio::onClick()
{
   m_nSelectedIndex = m_nFocusedIndex;
}


void MenuItemRadio::onKeyUp(bool bIgnoreReversion)
{
   setPrevAvailableFocusableItem();
}

void MenuItemRadio::onKeyDown(bool bIgnoreReversion)
{
   setNextAvailableFocusableItem();
}

void MenuItemRadio::onKeyLeft(bool bIgnoreReversion)
{
   setPrevAvailableFocusableItem();
}

void MenuItemRadio::onKeyRight(bool bIgnoreReversion)
{
   setNextAvailableFocusableItem();
}


float MenuItemRadio::getItemHeight(float maxWidth)
{
   maxWidth = m_pMenu->getUsableWidth();
   //float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float fHeight = 0.0;

   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      fHeight += g_pRenderEngine->getMessageHeight(m_szSelections[i], MENU_TEXTLINE_SPACING, maxWidth - m_fSelectorWidth, g_idFontMenu);
      if ( NULL != m_szSelectionsLegend[i] && 0 != m_szSelectionsLegend[i][0] )
      {
         if ( m_bSmallLegend )
         {
            fHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, maxWidth-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenuSmall);
            fHeight += height_text_small*0.5;
         }
         else
            fHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, maxWidth-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenu) + height_text_small*0.2;
      }
      //fHeight += height_text * MENU_ITEM_SPACING;
      fHeight += 2.0 * Menu::getSelectionPaddingY();
   }

   //if ( m_nSelectionsCount > 1 )
   //   fHeight += (float)(m_nSelectionsCount-1) * MENU_ITEM_SPACING * height_text;

   fHeight += m_fExtraHeight;
   return fHeight;
}

float MenuItemRadio::getTitleWidth(float maxWidth)
{
   return 0.0;
   //return maxWidth;
}

float MenuItemRadio::getValueWidth(float maxWidth)
{
   return 0.0;
   //return maxWidth;
}


void MenuItemRadio::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   fWidthSelection = m_pMenu->getUsableWidth();
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   float y = yPos;

   if ( bSelected )
   {
      if ( -1 == m_nFocusedIndex )
         m_nFocusedIndex = 0;
   }
   else
      m_nFocusedIndex = -1;

   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      float yItem = y;

      if ( bSelected )
      if ( i == m_nFocusedIndex )
      {
         double pC[4];
         memcpy(pC, get_Color_MenuBg(), 4*sizeof(double));
         pC[0] += 40; pC[1] += 35; pC[2] += 30;
         g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]);
         g_pRenderEngine->setStrokeSize(2);

         float fSelHeight = g_pRenderEngine->getMessageHeight(m_szSelections[i], MENU_TEXTLINE_SPACING, fWidthSelection - m_fSelectorWidth, g_idFontMenu);
         if ( (NULL != m_szSelectionsLegend[i]) && (0 != m_szSelectionsLegend[i][0]) )
         {
            if ( m_bSmallLegend )
            {
               fSelHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenuSmall);
               fSelHeight += height_text_small*0.5;
            }
            else
               fSelHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenu) + height_text_small*0.2;
         }
         fSelHeight += 2.0 * Menu::getSelectionPaddingY();

         g_pRenderEngine->drawRoundRect(xPos-Menu::getSelectionPaddingX(), yItem-Menu::getSelectionPaddingY(), fWidthSelection + 2.0 * Menu::getSelectionPaddingX(), fSelHeight, 0.01);
      }

      // Draw circle
      
      if ( ! m_bEnabledSelections[i] )
         g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
      else
         g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(1.0);
      //g_pRenderEngine->drawLine(xPos - m_fSelectorWidth, y, xPos + m_fSelectorWidth, y);
      //g_pRenderEngine->drawCircle(xPos + m_fSelectorWidth*0.4 + 0.03, y + m_fSelectorWidth*0.4*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.6);
      //g_pRenderEngine->fillCircle(xPos + m_fSelectorWidth*0.4+0.06, y + m_fSelectorWidth*0.4*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.4);
      

      g_pRenderEngine->drawCircle(xPos + m_fSelectorWidth*0.4, y + m_fSelectorWidth*0.4*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.6);
      if ( i == m_nSelectedIndex )
         g_pRenderEngine->fillCircle(xPos + m_fSelectorWidth*0.4, y + m_fSelectorWidth*0.4*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.4);
      // Draw circle - end

      y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth, y, m_szSelections[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth, g_idFontMenu);
      if ( NULL != m_szSelectionsLegend[i] && 0 != m_szSelectionsLegend[i][0] )
      {
         if ( m_bSmallLegend )
         {
            y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth + m_fLegendDx, y + 0.3*height_text_small, m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenuSmall);
            y += height_text_small*0.5;
         }
         else
            y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth + m_fLegendDx, y + height_text_small*0.2, m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx - Menu::getSelectionPaddingX(), g_idFontMenu) + height_text_small*0.2;
      }

      y += MENU_ITEM_SPACING * height_text;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);
}

void MenuItemRadio::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   return;
}
