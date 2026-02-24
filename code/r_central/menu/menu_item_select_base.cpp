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

#include "menu_item_select_base.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemSelectBase::MenuItemSelectBase(const char* title)
:MenuItem(title)
{
   m_SelectionsCount = 0;
   m_SelectedIndex = -1;
   m_SelectedIndexBeforeEdit = 0;
   m_bDisabledClick = false;
}

MenuItemSelectBase::MenuItemSelectBase(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_SelectionsCount = 0;
   m_SelectedIndex = -1;
   m_SelectedIndexBeforeEdit = 0;
   m_bDisabledClick = false;
}

MenuItemSelectBase::~MenuItemSelectBase()
{
   removeAllSelections();
}


void MenuItemSelectBase::removeAllSelections()
{
   for( int i=0; i<m_SelectionsCount; i++ )
      free( m_szSelections[i] );
   m_SelectionsCount = 0;
}

int MenuItemSelectBase::addSelection(const char* szText)
{
   if ( NULL == szText || m_SelectionsCount >= MAX_MENU_ITEM_SELECTIONS - 1 )
      return -1;

   if ( -1 == m_SelectedIndex )
      m_SelectedIndex = 0;

   m_szSelections[m_SelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_SelectionsCount], szText);
   m_bEnabledItems[m_SelectionsCount] = true;
   m_SelectionsCount++;
   return m_SelectionsCount-1;
}

int MenuItemSelectBase::addSelection(const char* szText, bool bEnabled)
{
   int i = addSelection(szText);
   if ( ! bEnabled )
      setSelectionIndexDisabled(m_SelectionsCount-1);
   return i;
}

void MenuItemSelectBase::updateSelectionText(int iIndex, const char* szText)
{
   if ( (iIndex < 0) || (iIndex >= m_SelectionsCount) )
      return;
   if ( NULL == szText )
      return;

   if ( NULL != m_szSelections[iIndex] )
      free(m_szSelections[iIndex]);
   m_szSelections[iIndex] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[iIndex], szText);
}

void MenuItemSelectBase::setSelection(int index)
{
   if ( (index >= 0) && (index < m_SelectionsCount) )
      m_SelectedIndex = index;
   else
      m_SelectedIndex = 0;
}

void MenuItemSelectBase::setSelectedIndex(int index)
{
   setSelection(index);
}

void MenuItemSelectBase::setSelectionIndexDisabled(int index)
{
   m_bEnabledItems[index] = false;
}

void MenuItemSelectBase::setSelectionIndexEnabled(int index)
{
   m_bEnabledItems[index] = true;
}

int MenuItemSelectBase::getSelectedIndex()
{
   return m_SelectedIndex;
}

int MenuItemSelectBase::getSelectionsCount()
{
   return m_SelectionsCount;
}

void MenuItemSelectBase::restoreSelectedIndex()
{
   m_SelectedIndex = m_SelectedIndexBeforeEdit;
}

char* MenuItemSelectBase::getSelectionIndexText(int iSelectionIndex)
{
   if ( (iSelectionIndex < 0) || (iSelectionIndex >= m_SelectionsCount) )
      return NULL;
   return m_szSelections[iSelectionIndex];
}

void MenuItemSelectBase::disableClick()
{
   m_bDisabledClick = true;
}

void MenuItemSelectBase::beginEdit()
{
   m_SelectedIndexBeforeEdit = m_SelectedIndex;

   MenuItem::beginEdit();
}

void MenuItemSelectBase::endEdit(bool bCanceled)
{
   if ( bCanceled )
      restoreSelectedIndex();
   MenuItem::endEdit(bCanceled);
}

void MenuItemSelectBase::onClick()
{
   if ( m_bDisabledClick )
      return;
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


void MenuItemSelectBase::onKeyUp(bool bIgnoreReversion)
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

void MenuItemSelectBase::onKeyDown(bool bIgnoreReversion)
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

void MenuItemSelectBase::onKeyLeft(bool bIgnoreReversion)
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

void MenuItemSelectBase::onKeyRight(bool bIgnoreReversion)
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

void MenuItemSelectBase::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( -1 == m_SelectedIndex )
   {
      RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);
      return;
   }

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float width_value = g_pRenderEngine->textWidth(g_idFontMenu, m_szSelections[m_SelectedIndex]);
   float paddingH = Menu::getSelectionPaddingX();
   float paddingV = Menu::getSelectionPaddingY();
   float triSize = 0.3 * height_text;
   float triMargin = 0.2 * height_text;
   float totalWidthValue = width_value + 2.0*triSize + 2.0*triMargin;
   float xValue = xPos + m_pMenu->getUsableWidth() - totalWidthValue;

   if ( xPos + fWidthSelection > xValue - m_pMenu->getMenuPaddingX() )
   //if ( fWidthSelection > width_title )
   {
     float dOverlapWidth = xPos + fWidthSelection - (xValue-m_pMenu->getMenuPaddingX());
     //if ( dOverlapWidth < fWidthSelection - width_title )
        fWidthSelection -= dOverlapWidth;
     //else
     //   fWidthSelection = width_value;
   }

   RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xValue-paddingH, yPos-paddingV, totalWidthValue + 2.0*paddingH , m_RenderHeight + 2.0*paddingV, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   else
      g_pRenderEngine->setColors(get_Color_MenuText());
   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());

   g_pRenderEngine->drawText(xValue + triMargin + triSize, yPos, g_idFontMenu, m_szSelections[m_SelectedIndex]);

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());

   float yT = yPos+height_text*0.5;
   g_pRenderEngine->fillTriangle(xValue, yT, xValue+triSize, yT+triSize, xValue+triSize, yT-triSize);
   xValue += totalWidthValue;
   g_pRenderEngine->fillTriangle(xValue, yT, xValue-triSize, yT+triSize, xValue-triSize, yT-triSize);
}

void MenuItemSelectBase::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float width_value = g_pRenderEngine->textWidth(g_idFontMenu, m_szSelections[m_SelectedIndex]);
   float paddingH = Menu::getSelectionPaddingX();
   float paddingV = Menu::getSelectionPaddingY();
   float triSize = 0.3*height_text;
   float triMargin = 0.2 * height_text;
   float totalWidthValue = width_value + 2*triSize + 2*triMargin;

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, 2.0*paddingH + totalWidthValue, m_RenderHeight + 2.0*paddingV - m_fExtraHeight, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, 2.0*paddingH + totalWidthValue , m_RenderHeight + 2.0*paddingV - m_fExtraHeight, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   else if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());
   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());

   g_pRenderEngine->drawText(xPos + triMargin + triSize, yPos, g_idFontMenu, m_szSelections[m_SelectedIndex]);

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   else if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
   float yT = yPos+height_text*0.5;
   g_pRenderEngine->fillTriangle(xPos, yT, xPos+triSize, yT+triSize, xPos+triSize, yT-triSize);
   xPos += totalWidthValue;
   g_pRenderEngine->fillTriangle(xPos, yT, xPos-triSize, yT+triSize, xPos-triSize, yT-triSize);
}
