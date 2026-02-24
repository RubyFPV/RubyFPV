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

#include "menu_item_select.h"
#include "menu_objects.h"
#include "menu.h"

MenuItemSelect::MenuItemSelect(const char* title)
:MenuItemSelectBase(title)
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
   m_bPopupSelectorCentered = true;
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
      m_szSelectionsTooltips[i] = NULL;

   m_iCountSeparators = 0;
}

MenuItemSelect::MenuItemSelect(const char* title, const char* tooltip)
:MenuItemSelectBase(title, tooltip)
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
   m_bPopupSelectorCentered = true;
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
      m_szSelectionsTooltips[i] = NULL;
   m_iCountSeparators = 0;
}

MenuItemSelect::~MenuItemSelect()
{
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
   {
      if ( NULL != m_szSelectionsTooltips[i] )
         free( m_szSelectionsTooltips[i] );
      m_szSelectionsTooltips[i] = NULL;
   }
}

void MenuItemSelect::setUseMultiViewLayout()
{
   m_bUseMultipleViewLayout = true;
   m_bUsePopupSelector = false;
}

void MenuItemSelect::setUsePopupSelector()
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
}

void MenuItemSelect::disablePopupSelector()
{
   m_bUsePopupSelector = false;
}

void MenuItemSelect::setPopupSelectorToRight()
{
   m_bPopupSelectorCentered = false;
}

void MenuItemSelect::setTooltipForSelection(int selectionIndex, const char* szTooltip)
{
   if ( selectionIndex < 0 || selectionIndex >= MAX_MENU_ITEM_SELECTIONS )
      return;

   if ( NULL != m_szSelectionsTooltips[selectionIndex] )
      free( m_szSelectionsTooltips[selectionIndex] );

   m_szSelectionsTooltips[selectionIndex] = (char*)malloc(strlen(szTooltip)+1);
   strcpy(m_szSelectionsTooltips[selectionIndex], szTooltip);
}

void MenuItemSelect::addSeparator()
{
   if ( 0 == m_SelectionsCount )
      return;
   m_SeparatorIndexes[m_iCountSeparators] = m_SelectionsCount-1;
   m_iCountSeparators++;
}


void MenuItemSelect::onKeyDown(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   bool bReverse = false;
   if ( m_bUsePopupSelector )
   {
      if ( p->iSwapUpDownButtonsValues != 0 )
         bReverse = true;
      //if ( p->iSwapUpDownButtons != 0 )
      //   bReverse = ! bReverse;
   }
   if ( bIgnoreReversion )
      bReverse = false;

   if ( bReverse )
   {
      m_SelectedIndex--;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = m_SelectionsCount-1;

      int k = m_SelectionsCount*2;
      while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
      {
         k--;
         m_SelectedIndex--;
         if ( m_SelectedIndex < 0 )
            m_SelectedIndex = m_SelectionsCount-1;
      }
   }
   else
   {
      m_SelectedIndex++;
      if ( m_SelectedIndex >= m_SelectionsCount )
         m_SelectedIndex = 0;

      int k = m_SelectionsCount*2;
      while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
      {
         k--;
         m_SelectedIndex++;
         if ( m_SelectedIndex >= m_SelectionsCount )
            m_SelectedIndex = 0;
      }
   }
}


void MenuItemSelect::onKeyUp(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   bool bReverse = false;
   if ( m_bUsePopupSelector )
   {
      if ( p->iSwapUpDownButtonsValues != 0 )
         bReverse = true;
      //if ( p->iSwapUpDownButtons != 0 )
      //   bReverse = ! bReverse;
   }

   if ( bIgnoreReversion )
      bReverse = false;

   if ( bReverse )
   {
      m_SelectedIndex++;
      if ( m_SelectedIndex >= m_SelectionsCount )
         m_SelectedIndex = 0;

      int k = m_SelectionsCount*2;
      while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
      {
         k--;
         m_SelectedIndex++;
         if ( m_SelectedIndex >= m_SelectionsCount )
            m_SelectedIndex = 0;
      }
   }
   else
   {
      m_SelectedIndex--;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = m_SelectionsCount-1;

      int k = m_SelectionsCount*2;
      while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
      {
         k--;
         m_SelectedIndex--;
         if ( m_SelectedIndex < 0 )
            m_SelectedIndex = m_SelectionsCount-1;
      }
   }
}


void MenuItemSelect::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_bUsePopupSelector && m_bIsEditing )
   {
      RenderPopupSelections(xPos, yPos, bSelected, fWidthSelection);
      return;
   }

   if ( ! m_bUseMultipleViewLayout )
      return MenuItemSelectBase::Render(xPos, yPos, bSelected, fWidthSelection);

   RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float fAlphaOrg = g_pRenderEngine->getGlobalAlfa();
   float fAlphaLow = 0.5;
   height_text = 0.9*height_text;

   float xEnd = xPos + m_pMenu->getUsableWidth();
   float x0 = xEnd;

   float dxPaddings = Menu::getSelectionPaddingX();
   float dyPaddings = 0.6*Menu::getSelectionPaddingY();
   float dyText = 0.14*(m_RenderHeight - m_RenderTitleHeight)-0.07*height_text;
   float dxItemPadding = dxPaddings*1.5;

   for( int i=m_SelectionsCount-1; i>=0; i-- )
   {
      float width_text = g_pRenderEngine->textWidth(g_idFontMenu, m_szSelections[i]);
      if ( m_SelectedIndex != i )
      {
         g_pRenderEngine->setGlobalAlfa(fAlphaLow);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else if ( ! m_bEnabled )
            g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->drawTextLeft(xEnd-dxItemPadding, yPos+dyText, g_idFontMenu, m_szSelections[i]);
      }
      else
      {
         g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
         double pC[4];
         memcpy(pC, get_Color_MenuItemSelectedBg(), 4*sizeof(double));
         if ( ! m_bEnabled )
            pC[3] = 0.1;
         g_pRenderEngine->setColors(pC);
         g_pRenderEngine->drawRoundRect(xEnd-width_text-2*dxItemPadding, yPos-dyPaddings, width_text+2*dxItemPadding, m_RenderHeight+2*dyPaddings - m_fExtraHeight, 0.05*Menu::getMenuPaddingY());
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
         g_pRenderEngine->drawTextLeft(xEnd-dxItemPadding, yPos+dyText, g_idFontMenu, m_szSelections[i]);
      }

      xEnd -= width_text+2*dxItemPadding;
      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);
      g_pRenderEngine->setStroke(get_Color_MenuText());
      if ( ! m_bEnabled )
         g_pRenderEngine->setStroke(get_Color_MenuItemDisabledText());
      if ( i != 0 )
         g_pRenderEngine->drawLine(xEnd, yPos, xEnd, yPos+m_RenderHeight - m_fExtraHeight);
      g_pRenderEngine->setColors(get_Color_MenuText(),1);
      g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   }

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setGlobalAlfa(fAlphaLow);
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xEnd, yPos-dyPaddings, (x0-xEnd), m_RenderHeight+2*dyPaddings - m_fExtraHeight, 0.05*Menu::getMenuPaddingY());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
}

void MenuItemSelect::RenderPopupSelections(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( ! m_bCondensedOnly )
      RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   if ( -1 == m_SelectedIndex )
      return;

   float width_text = 0.0;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      float f = g_pRenderEngine->textWidth(g_idFontMenu, m_szSelections[i]);
      if ( f > width_text )
         width_text = f;
   }
   float paddingH = Menu::getMenuPaddingX();
   float paddingV = Menu::getMenuPaddingY();
   float selectionPaddingH = Menu::getSelectionPaddingX();
   float selectionPaddingV = Menu::getSelectionPaddingY();
   float totalWidth = width_text + 2*paddingH;

   int columns = 1;
   if ( 0 < m_iCountSeparators )
   {
       columns = m_iCountSeparators+1;
       if ( 0 < m_iCountSeparators )
       if ( m_SeparatorIndexes[m_iCountSeparators-1] == m_SelectionsCount-1 )
          columns = m_iCountSeparators;
       totalWidth = width_text*columns + 2*paddingH + (columns-1)*paddingH;
   }
   float xValues = xPos + m_pMenu->getUsableWidth() - width_text + paddingH;
   if ( ! m_bPopupSelectorCentered )
      xValues = xPos;

   if ( xValues > m_pMenu->getRenderXPos() + m_pMenu->getRenderWidth() - totalWidth*0.6 )
      xValues = m_pMenu->getRenderXPos() + m_pMenu->getRenderWidth() - totalWidth*0.6;

   float heightPopup = 0.0;
   float hTmp = 0.0;
   int maxItemsInColumn = 0;
   int countInColumn = 0;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      hTmp += m_RenderHeight + MENU_ITEM_SPACING * height_text - m_fExtraHeight;
      countInColumn++;
      for( int c=0; c<m_iCountSeparators; c++ )
         if ( m_SeparatorIndexes[c] == i )
         {
            if ( hTmp > heightPopup )
               heightPopup = hTmp;
            if ( countInColumn > maxItemsInColumn )
               maxItemsInColumn = countInColumn;
            hTmp = 0.0;
            countInColumn = 0;
            break;
         }
   }
   if ( hTmp > heightPopup )
      heightPopup = hTmp;
   heightPopup += 2.0*paddingV;

   float yTopSelections = yPos - maxItemsInColumn * m_RenderHeight / 2;
   if ( yPos > 0.7 )
      yTopSelections = yPos - maxItemsInColumn * m_RenderHeight * 3 / 4;
   if ( yPos < 0.3 )
      yTopSelections = yPos - maxItemsInColumn * m_RenderHeight / 4;

   if ( heightPopup > 0.7 )
      yTopSelections = 0.5*(1.0-heightPopup);
   if ( yTopSelections + heightPopup >= 0.99 )
      yTopSelections = 0.99 - heightPopup;
   if ( yTopSelections < 0.01 )
      yTopSelections = 0.01;

   double pColor[4];
   memcpy(pColor, get_Color_MenuBg(), 4*sizeof(double));
   pColor[3] = 0.9;
   g_pRenderEngine->setColors(pColor, 1.5);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xValues, yTopSelections, totalWidth, heightPopup, 0.2*Menu::getMenuPaddingY());

   if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   float y = yTopSelections + paddingV;
   xValues += paddingH;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      if ( i == m_SelectedIndex )
      {
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
         g_pRenderEngine->drawRoundRect(xValues-selectionPaddingH, y - selectionPaddingV, width_text+2.0*selectionPaddingH , m_RenderHeight + 2.0*selectionPaddingV - m_fExtraHeight, 0.2*Menu::getMenuPaddingY());
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
         g_pRenderEngine->drawText(xValues, y, g_idFontMenu, m_szSelections[i]);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
      {
         if ( ! m_bEnabledItems[i] )
            g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
         if ( i == m_SelectedIndexBeforeEdit )
         {
            g_pRenderEngine->setFill(0,0,0,0);
            g_pRenderEngine->drawRoundRect(xValues-selectionPaddingH, y - selectionPaddingV + g_pRenderEngine->getPixelHeight(), width_text+2.0*selectionPaddingH , m_RenderHeight + 2.0*selectionPaddingV - 2.0 * g_pRenderEngine->getPixelHeight() - m_fExtraHeight, 0.2*Menu::getMenuPaddingY());
            if ( m_bCustomTextColor )
               g_pRenderEngine->setColors(&m_TextColor[0]);
            else
               g_pRenderEngine->setColors(get_Color_MenuText());
         }
         g_pRenderEngine->drawText(xValues, y, g_idFontMenu, m_szSelections[i]);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      y += m_RenderHeight - m_fExtraHeight;
      y += MENU_ITEM_SPACING * height_text;
      for( int c=0; c<m_iCountSeparators; c++ )
         if ( m_SeparatorIndexes[c] == i )
         {
            y = yTopSelections + paddingV;
            xValues += width_text + paddingH;
            break;
         }
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
}

