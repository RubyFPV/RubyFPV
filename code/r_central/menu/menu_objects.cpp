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

#include "menu_objects.h"
#include "menu_search.h"
#include "menu_item_section.h"

#include "../../base/utils.h"
#include "../../base/hardware_files.h"
#include "../../radio/radiolink.h"
#include "../osd/osd_common.h"
#include "menu.h"
#include <math.h>
#include "menu_preferences_buttons.h"
#include "menu_preferences_ui.h"
#include "menu_confirmation.h"
#include "../process_router_messages.h"
#include "../render_commands.h"
#include "../keyboard.h"
#include "../link_watch.h"
#include "../../base/tx_powers.h"

#define MENU_ALFA_WHEN_IN_BG 0.5

#define MENU_MARGINS 0.8
// As percentage of current font height


float MENU_TEXTLINE_SPACING = 0.1; // percent of item text height
float MENU_ITEM_SPACING = 0.28;  // percent of item text height

static bool s_bDebugMenuBoundingBoxes = false;

static bool s_bMenuObjectsRenderEndItems = false;

float Menu::m_sfMenuPaddingX = 0.0;
float Menu::m_sfMenuPaddingY = 0.0;
float Menu::m_sfSelectionPaddingX = 0.0;
float Menu::m_sfSelectionPaddingY = 0.0;
float Menu::m_sfScaleFactor = 1.0;
int Menu::m_siRenderMode = 0;

int Menu::getRenderMode()
{
   return m_siRenderMode;
}

void Menu::setRenderMode(int iMode)
{
   m_siRenderMode = iMode;
}


Menu::Menu(int id, const char* title, const char* subTitle)
{
   m_pParent = NULL;
   m_MenuId = id;
   m_MenuDepth = 0;
   m_iColumnsCount = 1;
   m_bIsModal = false;
   m_bEnableColumnSelection = true;
   m_bIsSelectingInsideColumn = false;
   m_bFullWidthSelection = false;
   m_bFirstShow = true;
   m_bDisableBackAction = false;
   m_ItemsCount = 0;
   m_SelectedIndex = 0;
   m_Height = 0.0;
   m_RenderTotalHeight = 0;
   m_RenderHeight = 0;
   m_RenderWidth = 0;
   m_RenderHeaderHeight = 0;
   m_RenderTitleHeight = 0.0;
   m_RenderSubtitleHeight = 0.0;
   m_RenderFooterHeight = 0.0;
   m_RenderTooltipHeight = 0.0;
   m_RenderMaxFooterHeight = 0.0;
   m_RenderMaxTooltipHeight = 0.0;
   m_fRenderScrollBarsWidth = 0.015;

   m_fRenderItemsTotalHeight = 0.0;
   m_fRenderItemsVisibleHeight = 0.0;
   m_fRenderItemsStartYPos = 0.0;
   m_fRenderItemsStopYPos = 0.0;

   m_bRenderedLastItem = false;
   m_ThisRenderCycleStartRenderItemIndex = -1;
   m_ThisRenderCycleEndRenderItemIndex = -1;

   m_bEnableScrolling = true;
   m_bHasScrolling = false;
   m_iIndexFirstVisibleItem = 0;
   m_fSelectionWidth = 0;

   m_fExtraHeightStart = 0.0;
   m_fExtraHeightEnd = 0.0;
   
   m_bDisableStacking = false;
   m_bDisableBackgroundAlpha = false;
   m_fAlfaWhenInBackground = MENU_ALFA_WHEN_IN_BG;
   Preferences* pP = get_Preferences();
   if ( (NULL != pP) && (! pP->iMenusStacked) )
      m_fAlfaWhenInBackground = MENU_ALFA_WHEN_IN_BG*1.1;
   if ( (NULL != pP) && ( pP->iMenuStyle == 1) )
      m_fAlfaWhenInBackground = 0.95;

   m_szCurrentTooltip[0] = 0;
   m_szTitle[0] = 0;
   m_szSubTitle[0] = 0;
   if ( NULL != title )
      strcpy( m_szTitle, title );
   if ( NULL != subTitle )
      strcpy( m_szSubTitle, subTitle );

   m_TopLinesCount = 0;
   m_bInvalidated = true;
   m_uIconId = 0;
   m_fIconSize = 0.0;

   m_uOnShowTime = g_TimeNow;
   m_uOnChildAddTime = 0;
   m_uOnChildCloseTime = 0;
   
   m_bIsAnimationInProgress = false;
   m_uAnimationStartTime = 0;
   m_uAnimationLastStepTime = 0;
   m_uAnimationTotalDuration = 200;
   m_fAnimationStartXPos = m_xPos;
   m_fAnimationTargetXPos = -10.0;
   m_RenderXPos = -10.0;
   m_iLoopCounter = 0;
}

Menu::~Menu()
{
   removeAllItems();
   removeAllTopLines();
}

float Menu::getMenuPaddingX()
{
   return m_sfMenuPaddingX;
}

float Menu::getMenuPaddingY()
{
   return m_sfMenuPaddingY;
}

float Menu::getSelectionPaddingX()
{
   return m_sfSelectionPaddingX;
}

float Menu::getSelectionPaddingY()
{
   return m_sfSelectionPaddingY;
}

float Menu::getScaleFactor()
{
   return m_sfScaleFactor;
}

int Menu::getId()
{
   return m_MenuId;
}

void Menu::setId(int iId)
{
   m_MenuId = iId;
}


void Menu::setParent(Menu* pParent)
{
   m_pParent = pParent;
}

void Menu::invalidate()
{
   valuesToUI();
   m_bInvalidated = true;
   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->invalidate();
}

void Menu::setModal(bool bModal)
{
   m_bIsModal = bModal;
}

bool Menu::isModal()
{
   return m_bIsModal;
}

void Menu::disableScrolling()
{
   m_bEnableScrolling = false;
   m_bHasScrolling = false;
   invalidate();
}

char* Menu::getTitle()
{
   return m_szTitle;
}

void Menu::setTitle(const char* szTitle)
{
   if ( NULL == szTitle )
      m_szTitle[0] = 0;
   else
      strncpy(m_szTitle, szTitle, sizeof(m_szTitle)/sizeof(m_szTitle[0]));
   m_szTitle[sizeof(m_szTitle)/sizeof(m_szTitle[0]) - 1] = 0;
   m_bInvalidated = true;
}

void Menu::setSubTitle(const char* szSubTitle)
{
   if ( NULL == szSubTitle )
      m_szSubTitle[0] = 0;
   else
      strncpy(m_szSubTitle, szSubTitle, sizeof(m_szSubTitle)/sizeof(m_szSubTitle[0]));
   m_szSubTitle[sizeof(m_szSubTitle)/sizeof(m_szSubTitle[0]) - 1] = 0;
   m_bInvalidated = true;
}

void Menu::removeAllTopLines()
{
   for( int i=0; i<m_TopLinesCount; i++ )
   {
      if ( NULL != m_szTopLines[i] )
         free(m_szTopLines[i]);
   }
   m_TopLinesCount = 0;
   m_bInvalidated = true;
}

void Menu::addTopLine(const char* szLine, float dx)
{
   if ( (m_TopLinesCount >= MENU_MAX_TOP_LINES-1) || (NULL == szLine) )
      return;
   m_fTopLinesDX[m_TopLinesCount] = dx;
   m_szTopLines[m_TopLinesCount] = (char*)malloc(strlen(szLine)+1);
   strcpy(m_szTopLines[m_TopLinesCount], szLine);

   if ( strlen(m_szTopLines[m_TopLinesCount]) > 1 )
   if ( m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 10 || 
        m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 13 )
      m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] = 0;
   if ( strlen(m_szTopLines[m_TopLinesCount]) > 1 )
   if ( m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 10 || 
        m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 13 )
      m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] = 0;

   m_TopLinesCount++;
   m_bInvalidated = true;
}

void Menu::setColumnsCount(int count)
{
   m_iColumnsCount = count;
}

void Menu::enableColumnSelection(bool bEnable)
{
   m_bEnableColumnSelection = bEnable;
}

void Menu::removeAllItems()
{
   for( int i=0; i<m_ItemsCount; i++ )
      if ( NULL != m_pMenuItems[i] )
         delete m_pMenuItems[i];
   m_ItemsCount = 0;
   m_iIndexFirstVisibleItem = 0;
   m_bInvalidated = true;
}

void Menu::removeMenuItem(MenuItem* pItem)
{
   if ( NULL == pItem )
      return;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i] == pItem )
      {
         for( int k=i; k<m_ItemsCount-1; k++ )
         {
            m_pMenuItems[k] = m_pMenuItems[k+1];
            m_bHasSeparatorAfter[k] = m_bHasSeparatorAfter[k+1];   
         }
         m_ItemsCount--;
         if ( m_SelectedIndex >= m_ItemsCount )
            m_SelectedIndex--;
         else if ( (m_SelectedIndex >= i) && (m_SelectedIndex > 0) )
            m_SelectedIndex--;
         m_iIndexFirstVisibleItem = 0;
         m_bInvalidated = true;
         return;
      }
   }
}

int Menu::addMenuItem(MenuItem* item)
{
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i] == item )
         return -1;
   }

   m_pMenuItems[m_ItemsCount] = item;
   m_bHasSeparatorAfter[m_ItemsCount] = false;
   m_pMenuItems[m_ItemsCount]->m_pMenu = this;
   m_ItemsCount++;
   m_bInvalidated = true;
   if ( -1 == m_SelectedIndex )
   if ( item->isEnabled() )
      m_SelectedIndex = 0;
   return m_ItemsCount-1;
}

int Menu::insertMenuItem(MenuItem* pItem, int iPosition)
{
   if ( (NULL == pItem) || (iPosition < 0) || (iPosition > m_ItemsCount) )
      return -1;

   for( int i=m_ItemsCount-1; i>= iPosition; i-- )
   {
      m_pMenuItems[i+1] = m_pMenuItems[i];
      m_bHasSeparatorAfter[i+1] = m_bHasSeparatorAfter[i];   
   }
   m_pMenuItems[iPosition] = pItem;
   m_bHasSeparatorAfter[iPosition] = false;
   m_pMenuItems[iPosition]->m_pMenu = this;

   if ( m_SelectedIndex >= iPosition )
      m_SelectedIndex++;
   m_ItemsCount++;
   m_bInvalidated = true;
   return iPosition;
}

int Menu::getSelectedMenuItemIndex()
{
   return m_SelectedIndex;
}

void Menu::addSeparator()
{
   if ( 0 == m_ItemsCount )
      return;
   m_bHasSeparatorAfter[m_ItemsCount-1] = true;
}

int Menu::addSection(const char* szSectionName)
{
   if ( (NULL == szSectionName) || (0 == szSectionName[0]) )
      return -1;
   return addMenuItem(new MenuItemSection(szSectionName));
}

void Menu::setTooltip(const char* szTooltip)
{
   if ( NULL == szTooltip || 0 == szTooltip[0] )
      m_szCurrentTooltip[0] = 0;
   else
      strcpy(m_szCurrentTooltip, szTooltip);
   m_bInvalidated = true;
}

void Menu::setIconId(u32 uIconId)
{
   m_uIconId = uIconId;
   m_bInvalidated = true;
}

void Menu::enableMenuItem(int index, bool enable)
{
   if ( (index < 0) || (index >= m_ItemsCount) )
      return;
   if ( NULL != m_pMenuItems[index] )
        m_pMenuItems[index]->setEnabled(enable);
   m_bInvalidated = true;
}


u32 Menu::getOnShowTime()
{
   return m_uOnShowTime;
}

u32 Menu::getOnChildAddTime()
{
   return m_uOnChildAddTime;
}

u32 Menu::getOnReturnFromChildTime()
{
   return m_uOnChildCloseTime;
}

float Menu::getMenuFontHeight()
{
   return g_pRenderEngine->textHeight(g_idFontMenu);
}

bool Menu::isEditingItem()
{
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( NULL != m_pMenuItems[i] )
      if ( m_pMenuItems[i]->isEditing() )
         return true;
   }
   return false;
}

void Menu::addExtraHeightAtStart(float fExtraH)
{
   m_fExtraHeightStart = fExtraH;
}

void Menu::addExtraHeightAtEnd(float fExtraH)
{
   m_fExtraHeightEnd = fExtraH;
}

void Menu::disableBackAction()
{
   m_bDisableBackAction = true;
}

void Menu::enableBackAction()
{
   m_bDisableBackAction = false; 
}


bool Menu::hasDisabledBackAction()
{
   return m_bDisableBackAction;
} 

float Menu::getRenderWidth()
{
   return m_RenderWidth;
}

float Menu::getRenderXPos()
{
   if ( (m_RenderXPos < -8.0) || m_bDisableStacking )
     m_RenderXPos = m_xPos;
   return m_RenderXPos;
}

void Menu::resetRenderXPos()
{
   m_RenderXPos = m_xPos;
   m_fAnimationTargetXPos = m_xPos;
   m_uAnimationStartTime = 0;
   m_bIsAnimationInProgress = false;
}

void Menu::onAddToStack()
{
  log_line("[Menu] (loop %d) Menu (%s) was added to stack, menu id: %d",
     menu_get_loop_counter()%1000, m_szTitle, m_MenuId);
}

void Menu::onShow()
{
   if ( m_bDisableStacking )
      m_RenderXPos = m_xPos;
   log_line("[Menu] (loop %u) [%s] onShow (id: %d, title: %s, ptr: 0x%X): xPos: %.2f, xRenderPos: %.2f",
      menu_get_loop_counter()%1000, m_bFirstShow? "first show":"not first show", 
      m_MenuId, m_szTitle, this, m_xPos, m_RenderXPos);

   m_bFirstShow = false;
   if ( 0 == m_uOnShowTime )
      m_uOnShowTime = g_TimeNow;

   if ( (m_MenuId%1000) == MENU_ID_SIMPLE_MESSAGE )
   {
      removeAllItems();
      addMenuItem(new MenuItem("OK",""));
   }
   valuesToUI();

   m_SelectedIndex = 0;
   if ( 0 == m_ItemsCount )
      m_SelectedIndex = -1;
   else
   {
      while ( (m_SelectedIndex < m_ItemsCount) && (NULL != m_pMenuItems[m_SelectedIndex]) && ( ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) )
         m_SelectedIndex++;
   }
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = -1;
   log_line("[Menu] onShow (%s): set selected item index to: %d (out of %d items)", m_szTitle, m_SelectedIndex, m_ItemsCount);
   onFocusedItemChanged();
   m_bInvalidated = true;
   log_line("[Menu] onShow (%s): finished handling onShow event.", m_szTitle);
}

bool Menu::periodicLoop()
{
   m_iLoopCounter++;
   return false;
}

float Menu::getSelectionWidth()
{
   return m_fSelectionWidth;
}

float Menu::getUsableWidth()
{
   m_fIconSize = 0.0;

   m_RenderWidth = m_Width;

   // Account for 4:3 screens or screen that do not have 16:9 aspect ratio
   if ( g_pRenderEngine->getAspectRatio() < 1.7 )
      m_RenderWidth *= 1.0 + 0.6*(1.7 - g_pRenderEngine->getAspectRatio());

   Preferences* pP = get_Preferences();
   m_sfScaleFactor = 1.0;

   // Same scaling must be done in font scaling computation. Search for AABBCC marker
   if ( NULL != pP )
   {
      if ( pP->iScaleMenus < 0 )
         m_sfScaleFactor = (1.0 + 0.15*pP->iScaleMenus);
      if ( pP->iScaleMenus > 0 )
         m_sfScaleFactor = (1.0 + 0.1*pP->iScaleMenus);
      if ( pP->iMenuStyle == 1 )
         m_sfScaleFactor *= 1.2;
   }
   
   m_RenderWidth *= m_sfScaleFactor;

   m_fRenderScrollBarsWidth = 0.015 * m_sfScaleFactor;

   if ( (m_iColumnsCount > 1) && (!m_bEnableColumnSelection) )
      return (m_RenderWidth - m_fIconSize)/(float)(m_iColumnsCount) - 2*m_sfMenuPaddingX;
   else
      return (m_RenderWidth - m_fIconSize - 2*m_sfMenuPaddingX);
}

float Menu::_computeRenderTopHeaderSize()
{
   m_RenderHeaderHeight = 0;
   m_RenderTitleHeight = 0;
   m_RenderSubtitleHeight = 0;
   if ( 0 != m_szTitle[0] )
   {
      m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
      m_RenderHeaderHeight += m_RenderTitleHeight;
      m_RenderHeaderHeight += m_sfMenuPaddingY;
   }
   if ( 0 != m_szSubTitle[0] )
   {
      m_RenderSubtitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
      m_RenderHeaderHeight += m_RenderSubtitleHeight;
      m_RenderHeaderHeight += 0.4*m_sfMenuPaddingY;
   }
   return m_RenderHeaderHeight;
}


float Menu::_computeRenderTopPartSize()
{
   float fHeight = 0.0;
   fHeight += m_fExtraHeightStart;
   fHeight += 0.5*m_sfMenuPaddingY;

   for (int i = 0; i<m_TopLinesCount; i++)
   {
       if ( 0 == strlen(m_szTopLines[i]) )
          fHeight += g_pRenderEngine->textHeight(g_idFontMenu)*(1.0 + MENU_TEXTLINE_SPACING);
       else
       {
          fHeight += g_pRenderEngine->getMessageHeight(m_szTopLines[i], MENU_TEXTLINE_SPACING, getUsableWidth() - m_fTopLinesDX[i], g_idFontMenu);
          fHeight += g_pRenderEngine->textHeight(g_idFontMenu)*MENU_TEXTLINE_SPACING;
       }
   }

   fHeight += 0.3*m_sfMenuPaddingY;
   if ( 0 < m_TopLinesCount )
      fHeight += 0.3*m_sfMenuPaddingY;
   return fHeight;
}

float Menu::_computeRenderMaxBottomFooterSize()
{
   m_RenderTooltipHeight = 0.0;
   m_RenderMaxTooltipHeight = 0.0;

   if ( 0 != m_szCurrentTooltip[0] )
   {
       m_RenderTooltipHeight = g_pRenderEngine->getMessageHeight(m_szCurrentTooltip, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenuSmall);
       m_RenderMaxTooltipHeight = m_RenderTooltipHeight;
   }

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( (NULL == m_pMenuItems[i]) || (m_pMenuItems[i]->isHidden()) )
         continue;
     
      if ( NULL != m_pMenuItems[i]->getTooltip() )
      {
         float fHTooltip = g_pRenderEngine->getMessageHeight(m_pMenuItems[i]->getTooltip(), MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
         if ( fHTooltip > m_RenderMaxTooltipHeight )
            m_RenderMaxTooltipHeight = fHTooltip;
      }
   }

   m_RenderFooterHeight = 0.4*m_sfMenuPaddingY;
   m_RenderFooterHeight += m_RenderTooltipHeight;
   m_RenderFooterHeight += 0.2*m_sfMenuPaddingY;

   m_RenderMaxFooterHeight = 0.4*m_sfMenuPaddingY;
   m_RenderMaxFooterHeight += m_RenderMaxTooltipHeight;
   m_RenderMaxFooterHeight += 0.2*m_sfMenuPaddingY;

   return m_RenderMaxFooterHeight;
}


void Menu::computeRenderSizes()
{
   m_fIconSize = 0.0;

   m_RenderYPos = m_yPos;
   if ( m_siRenderMode == 1 )
   {
      m_RenderYPos = 0.0;
      m_xPos = menu_get_XStartPos(this, m_Width);
      m_RenderXPos = m_xPos;
   }

   getUsableWidth(); // Computes m_RenderWidth
   
   m_RenderHeight = m_Height;
   if ( m_siRenderMode == 1 )
      m_RenderHeight = 1.0;

   m_sfMenuPaddingY = g_pRenderEngine->textHeight(g_idFontMenu) * MENU_MARGINS;
   m_sfMenuPaddingX = 1.2*m_sfMenuPaddingY / g_pRenderEngine->getAspectRatio();

   m_sfSelectionPaddingY = 0.54 * MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
   m_sfSelectionPaddingX = 0.3* m_sfMenuPaddingX;


   m_RenderTotalHeight = _computeRenderTopHeaderSize();
   m_RenderTotalHeight += _computeRenderTopPartSize();
  
   m_fRenderItemsStartYPos = m_RenderYPos + m_RenderTotalHeight;
   
   m_fRenderItemsTotalHeight = 0.0;
   
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( (NULL == m_pMenuItems[i]) || (m_pMenuItems[i]->isHidden()) )
         continue;
      float fItemTotalHeight = 0.0;
      if ( m_iColumnsCount < 2 )
      {
         fItemTotalHeight += m_pMenuItems[i]->getItemHeight(getUsableWidth() - m_pMenuItems[i]->m_fMarginX);
         fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
         if ( m_bHasSeparatorAfter[i] )
            fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
      }
      else if ( (i%m_iColumnsCount) == 0 )
      {
         fItemTotalHeight += m_pMenuItems[i]->getItemHeight(getUsableWidth() - m_pMenuItems[i]->m_fMarginX);
         fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
         if ( m_bHasSeparatorAfter[i] )
            fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
      }

      m_RenderTotalHeight += fItemTotalHeight;
      m_fRenderItemsTotalHeight += fItemTotalHeight;

   }
   m_RenderTotalHeight += 0.5*m_sfMenuPaddingY;
   
   m_fRenderItemsVisibleHeight = m_fRenderItemsTotalHeight;
   m_fRenderItemsStopYPos = m_RenderYPos + m_RenderTotalHeight;

   // End: compute items total height and render total height

   _computeRenderMaxBottomFooterSize();


   m_RenderTotalHeight += 0.4*m_sfMenuPaddingY;
   m_RenderTotalHeight += m_RenderMaxFooterHeight;
   
   m_bHasScrolling = false;
   m_RenderHeight = m_RenderTotalHeight;
   

   // Detect if scrolling is needed
    
   if ( m_Height < 0.001 )
   {
       if ( m_bEnableScrolling )
       if ( (m_iColumnsCount < 2) && (m_RenderYPos + m_RenderTotalHeight > 0.96) )
       {
          if ( m_TopLinesCount > 10 )
          {
             float fDelta = m_RenderYPos - 0.1;
             m_RenderYPos -= fDelta;
             m_fRenderItemsStartYPos -= fDelta;
             m_fRenderItemsStopYPos -= fDelta;
          }
          else
             m_bHasScrolling = true;
       }

       float fOverflowHeight = (m_RenderYPos + m_RenderTotalHeight) - 0.96;
          
       // Just move up if possible

       if ( m_bHasScrolling )
       if ( m_RenderYPos >= 0.14 )
       if ( fOverflowHeight <= (m_RenderYPos-0.14) )
       {
          m_RenderYPos -= fOverflowHeight;
          m_fRenderItemsStartYPos -= fOverflowHeight;
          m_fRenderItemsStopYPos -= fOverflowHeight;
          m_bHasScrolling = false;
       }

       // Move up as much as possible

       if ( m_bHasScrolling )
       {
          if ( m_RenderYPos > 0.16 )
          {
             float dMoveUp = m_RenderYPos - 0.16;
             m_RenderYPos -= dMoveUp;
             m_fRenderItemsStartYPos -= dMoveUp;
             m_fRenderItemsStopYPos -= dMoveUp;
             
             fOverflowHeight -= dMoveUp;
          }
          if ( fOverflowHeight <= 0.001 )
             m_bHasScrolling = false;
      }

      if ( m_bHasScrolling )
      {
         // Reduce the menu height to fit the screen and enable scrolling
         float fDeltaH = m_RenderHeight - (0.96 - m_RenderYPos);
         m_RenderHeight -= fDeltaH;
         m_fRenderItemsStopYPos -= fDeltaH;
      }
   }
   else
      m_RenderHeight = m_Height * m_sfScaleFactor;

   m_RenderHeight -= m_RenderMaxFooterHeight;
   m_RenderHeight += m_RenderFooterHeight;
   m_RenderHeight += m_fExtraHeightEnd;
   
   m_fSelectionWidth = 0;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      float w = 0;
      if ( (NULL != m_pMenuItems[i]) && (! m_pMenuItems[i]->isHidden()) )
      {
         if ( m_pMenuItems[i]->isSelectable() )
            w = m_pMenuItems[i]->getTitleWidth(getUsableWidth() - m_pMenuItems[i]->m_fMarginX);
         else
            m_pMenuItems[i]->getTitleWidth(getUsableWidth() - m_pMenuItems[i]->m_fMarginX);
         w += m_pMenuItems[i]->m_fMarginX;
         m_pMenuItems[i]->getValueWidth(getUsableWidth());
      }
      if ( m_fSelectionWidth < w )
         m_fSelectionWidth = w;
   }

   if ( 1 == m_siRenderMode)
   {
      m_RenderHeight = 1.0;
   }

   if ( m_bFullWidthSelection )
      m_fSelectionWidth = getUsableWidth();
   m_bInvalidated = false;
}


void Menu::RenderPrepare()
{
   m_ThisRenderCycleStartRenderItemIndex = -1;
   m_ThisRenderCycleEndRenderItemIndex = -1;

   if ( m_bInvalidated )
   {
      computeRenderSizes();
      m_bInvalidated = false;
   }

   if ( (m_RenderXPos < -8.0) || m_bDisableStacking )
      m_RenderXPos = m_xPos;

   // Adjust render x position if needed

   Preferences* pP = get_Preferences();
   if ( (NULL == pP) || (! pP->iMenusStacked) )
      return;

   if ( ! m_bIsAnimationInProgress )
      return;
   
   if ( m_bDisableStacking || (pP->iMenuStyle == 1) )
      return;

   float f = (g_TimeNow-m_uAnimationStartTime)/(float)m_uAnimationTotalDuration;

   if ( f < 0.0 )
      f = 0.0;

   if ( f < 1.0 )
      m_RenderXPos = m_fAnimationStartXPos +(m_fAnimationTargetXPos - m_fAnimationStartXPos)*f;
   else
   {
      f = 1.0;
      m_bIsAnimationInProgress = false;
      log_line("[Menu] Finished animation for menu id %d", m_MenuId);
      m_RenderXPos = m_fAnimationTargetXPos;
      m_fAnimationTargetXPos = -10.0;
      if ( m_uOnChildCloseTime > m_uOnChildAddTime )
         m_uOnChildCloseTime = 0;
   }
}

void Menu::Render()
{
   RenderPrepare();

   bool bAlpha = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->disableAlphaBlending();
   
   float yTop = RenderFrameAndTitle();
   float yPos = yTop;

   s_bMenuObjectsRenderEndItems = false;
   for( int i=0; i<m_ItemsCount; i++ )
      yPos += RenderItem(i,yPos);

   g_pRenderEngine->setAlphaBlendingEnabled(bAlpha);

   RenderEnd(yTop);
   g_pRenderEngine->setColors(get_Color_MenuText());
}

void Menu::RenderEnd(float yPos)
{
   bool bAlpha = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->disableAlphaBlending();

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isEditing() )
      {
         s_bMenuObjectsRenderEndItems = true;
         RenderItem(i,m_pMenuItems[i]->m_RenderLastY, m_pMenuItems[i]->m_RenderLastX - (m_RenderXPos + m_sfMenuPaddingX));
         s_bMenuObjectsRenderEndItems = false;
      }
   }
   g_pRenderEngine->setAlphaBlendingEnabled(bAlpha);
}

float Menu::RenderFrameAndTitle()
{
   if ( m_bInvalidated )
   {
      computeRenderSizes();
      m_bInvalidated = false;
   }

   if ( m_siRenderMode == 1 )
      return RenderFrameAndTitleSticky();

   float fExtraWidth = 0.0;
   if ( m_bEnableScrolling && m_bHasScrolling )
      fExtraWidth = m_fRenderScrollBarsWidth;


   // Draw backgrounds

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(0,0,0,0);
   g_pRenderEngine->drawRoundRectMenu(m_RenderXPos, m_RenderYPos, m_RenderWidth + fExtraWidth, m_RenderHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

   g_pRenderEngine->setColors(get_Color_MenuBgTitle());
   g_pRenderEngine->setStroke(0,0,0,0);
   g_pRenderEngine->drawRoundRectMenu(m_RenderXPos, m_RenderYPos, m_RenderWidth + fExtraWidth, m_RenderHeaderHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

   if ( 0 != m_szCurrentTooltip[0] )
   {
      g_pRenderEngine->setColors(get_Color_MenuBgTooltip());
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->drawRoundRectMenu(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderWidth + fExtraWidth, m_RenderFooterHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   }

   // Draw outlines

   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());

   g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos, m_RenderWidth + fExtraWidth, m_RenderHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   g_pRenderEngine->drawLine(m_RenderXPos, m_RenderYPos + m_RenderHeaderHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderYPos + m_RenderHeaderHeight);
   
   if ( 0 != m_szCurrentTooltip[0] )
      g_pRenderEngine->drawLine(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight);

   // Draw texts

   g_pRenderEngine->setColors(get_Color_MenuText());

   float yPos = m_RenderYPos + 0.56*m_sfMenuPaddingY;
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX, yPos, g_idFontMenu, m_szTitle);
   yPos += m_RenderTitleHeight;

   if ( 0 != m_szSubTitle[0] )
   {
      yPos += 0.4*m_sfMenuPaddingY;
      g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX, yPos, g_idFontMenu, m_szSubTitle);
      yPos += m_RenderSubtitleHeight;
   }

   // Render top lines
   
   yPos = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY;
   yPos += m_fExtraHeightStart;
   for (int i=0; i<m_TopLinesCount; i++)
   {
      if ( 0 == strlen(m_szTopLines[i]) )
        yPos += g_pRenderEngine->textHeight(g_idFontMenu)*(1.0 + MENU_TEXTLINE_SPACING);
      else
      {
         if ( NULL != strstr(m_szTopLines[i], "---") )
         {
            g_pRenderEngine->setColors(get_Color_MenuBg());
            g_pRenderEngine->setStroke(get_Color_MenuBorder());
            g_pRenderEngine->drawLine(m_RenderXPos+m_sfMenuPaddingX, yPos + 0.5*g_pRenderEngine->textHeight(g_idFontMenu), m_RenderXPos+m_RenderWidth - m_sfMenuPaddingX, yPos + 0.5*g_pRenderEngine->textHeight(g_idFontMenu));

            g_pRenderEngine->setColors(get_Color_MenuText());
            g_pRenderEngine->setStrokeSize(0);
            yPos += g_pRenderEngine->textHeight(g_idFontMenu)*(1+MENU_TEXTLINE_SPACING);
         }
         else
         {
            yPos += g_pRenderEngine->drawMessageLines(m_RenderXPos + m_sfMenuPaddingX + m_fIconSize + m_fTopLinesDX[i], yPos, m_szTopLines[i], MENU_TEXTLINE_SPACING, getUsableWidth()-m_fTopLinesDX[i], g_idFontMenu);
            yPos += g_pRenderEngine->textHeight(g_idFontMenu)*MENU_TEXTLINE_SPACING;
         }
      }
   }

   yPos += 0.3*m_sfMenuPaddingY;
   if ( 0 < m_TopLinesCount )
      yPos += 0.3*m_sfMenuPaddingY;

   if ( m_bEnableScrolling && m_bHasScrolling )
   {
      float wPixel = g_pRenderEngine->getPixelWidth();
      float fScrollBarHeight = m_fRenderItemsStopYPos - m_fRenderItemsStartYPos - m_sfMenuPaddingY;
      float fScrollButtonHeight = fScrollBarHeight * (fScrollBarHeight/m_fRenderItemsTotalHeight);
      float xPos = m_RenderXPos + m_RenderWidth + fExtraWidth - fExtraWidth*0.8;
      
      float yButton = 0.0;
      for( int k=0; k<m_iIndexFirstVisibleItem; k++ )
      {
         if ( m_pMenuItems[k]->isHidden() )
            continue;
         float fItemTotalHeight = 0.0;
         if ( m_iColumnsCount < 2 )
         {
            fItemTotalHeight += m_pMenuItems[k]->getItemHeight(getUsableWidth() - m_pMenuItems[k]->m_fMarginX);
            fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
            if ( m_bHasSeparatorAfter[k] )
               fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
         }
         else if ( (k%m_iColumnsCount) == 0 )
         {
            fItemTotalHeight += m_pMenuItems[k]->getItemHeight(getUsableWidth() - m_pMenuItems[k]->m_fMarginX);
            fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
            if ( m_bHasSeparatorAfter[k] )
               fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
         }
         yButton += fItemTotalHeight;
      }

      yButton = yPos + yButton*(fScrollBarHeight/m_fRenderItemsTotalHeight);
      
      if ( yButton + fScrollButtonHeight > m_fRenderItemsStopYPos )
         yButton = m_fRenderItemsStopYPos - fScrollButtonHeight;

      const double* pC = get_Color_MenuText();
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.18);
      g_pRenderEngine->drawRect( xPos - wPixel*2.0, yPos, wPixel*4.0, fScrollBarHeight);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.5);
      
      g_pRenderEngine->drawRoundRect(xPos - 0.24*fExtraWidth, yButton, fExtraWidth*0.48, fScrollButtonHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   if ( 0 != m_szCurrentTooltip[0] )
   {
      float yTooltip = m_RenderYPos+m_RenderHeight-m_RenderFooterHeight;
      yTooltip += 0.4 * m_sfMenuPaddingY;

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawMessageLines( m_RenderXPos+m_sfMenuPaddingX, yTooltip, m_szCurrentTooltip, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenuSmall);
   }


   if ( s_bDebugMenuBoundingBoxes )
      _debugDrawBoundingBoxes(yPos);
   return yPos;
}

float Menu::RenderFrameAndTitleSticky()
{
   float fExtraWidth = 0.0;
   if ( m_bEnableScrolling && m_bHasScrolling )
      fExtraWidth = m_fRenderScrollBarsWidth;

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(get_Color_MenuBg());
   g_pRenderEngine->drawRect(m_RenderXPos, 0, m_RenderWidth + fExtraWidth, m_RenderHeight);

   g_pRenderEngine->setColors(get_Color_MenuBgTitle());
   g_pRenderEngine->drawRect(m_RenderXPos, 0, m_RenderWidth + fExtraWidth, m_RenderHeaderHeight);


   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawLine(m_RenderXPos, m_RenderHeaderHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderHeaderHeight);
   g_pRenderEngine->drawLine(m_RenderXPos + m_RenderWidth + fExtraWidth, 0, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderHeight);


   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yPos = m_RenderYPos + 0.56*m_sfMenuPaddingY;
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX, yPos, g_idFontMenu, m_szTitle);
   yPos += m_RenderTitleHeight;

   if ( 0 != m_szSubTitle[0] )
   {
      yPos += 0.4*m_sfMenuPaddingY;
      g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX, yPos, g_idFontMenu, m_szSubTitle);
      yPos += m_RenderSubtitleHeight;
   }

   yPos = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY;
   yPos += m_fExtraHeightStart;
   for (int i=0; i<m_TopLinesCount; i++)
   {
      if ( 0 == strlen(m_szTopLines[i]) )
        yPos += g_pRenderEngine->textHeight(g_idFontMenu)*(1.0 + MENU_TEXTLINE_SPACING);
      else
      {
         if ( NULL != strstr(m_szTopLines[i], "---") )
         {
            g_pRenderEngine->setColors(get_Color_MenuBg());
            g_pRenderEngine->setStroke(get_Color_MenuBorder());
            g_pRenderEngine->drawLine(m_RenderXPos+m_sfMenuPaddingX, yPos + 0.5*g_pRenderEngine->textHeight(g_idFontMenu), m_RenderXPos+m_RenderWidth - m_sfMenuPaddingX, yPos + 0.5*g_pRenderEngine->textHeight(g_idFontMenu));

            g_pRenderEngine->setColors(get_Color_MenuText());
            g_pRenderEngine->setStrokeSize(0);
            yPos += g_pRenderEngine->textHeight(g_idFontMenu)*(1+MENU_TEXTLINE_SPACING);
         }
         else
         {
            yPos += g_pRenderEngine->drawMessageLines(m_RenderXPos + m_sfMenuPaddingX + m_fIconSize + m_fTopLinesDX[i], yPos, m_szTopLines[i], MENU_TEXTLINE_SPACING, getUsableWidth()-m_fTopLinesDX[i], g_idFontMenu);
            yPos += g_pRenderEngine->textHeight(g_idFontMenu)*MENU_TEXTLINE_SPACING;
         }
      }
   }

   yPos += 0.3*m_sfMenuPaddingY;
   if ( 0 < m_TopLinesCount )
      yPos += 0.3*m_sfMenuPaddingY;

   if ( m_bEnableScrolling && m_bHasScrolling )
   {
      float wPixel = g_pRenderEngine->getPixelWidth();
      float fScrollBarHeight = m_fRenderItemsStopYPos - m_fRenderItemsStartYPos - m_sfMenuPaddingY;
      float fScrollButtonHeight = fScrollBarHeight * (fScrollBarHeight/m_fRenderItemsTotalHeight);
      float xPos = m_RenderXPos + m_RenderWidth + fExtraWidth - fExtraWidth*0.8;
      
      float yButton = 0.0;
      for( int k=0; k<m_iIndexFirstVisibleItem; k++ )
      {
         if ( m_pMenuItems[k]->isHidden() )
            continue;
         float fItemTotalHeight = 0.0;
         if ( m_iColumnsCount < 2 )
         {
            fItemTotalHeight += m_pMenuItems[k]->getItemHeight(getUsableWidth() - m_pMenuItems[k]->m_fMarginX);
            fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
            if ( m_bHasSeparatorAfter[k] )
               fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
         }
         else if ( (k%m_iColumnsCount) == 0 )
         {
            fItemTotalHeight += m_pMenuItems[k]->getItemHeight(getUsableWidth() - m_pMenuItems[k]->m_fMarginX);
            fItemTotalHeight += MENU_ITEM_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
            if ( m_bHasSeparatorAfter[k] )
               fItemTotalHeight += g_pRenderEngine->textHeight(g_idFontMenu) * MENU_SEPARATOR_HEIGHT;
         }
         yButton += fItemTotalHeight;
      }

      yButton = yPos + yButton*(fScrollBarHeight/m_fRenderItemsTotalHeight);
      
      if ( yButton + fScrollButtonHeight > m_fRenderItemsStopYPos )
         yButton = m_fRenderItemsStopYPos - fScrollButtonHeight;

      const double* pC = get_Color_MenuText();
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.18);
      g_pRenderEngine->drawRect( xPos - wPixel*2.0, yPos, wPixel*4.0, fScrollBarHeight);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.5);
      
      g_pRenderEngine->drawRoundRect(xPos - 0.24*fExtraWidth, yButton, fExtraWidth*0.48, fScrollButtonHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   if ( 0 != m_szCurrentTooltip[0] )
   {
      g_pRenderEngine->setColors(get_Color_MenuBgTooltip());
      g_pRenderEngine->drawRect(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderWidth + fExtraWidth, m_RenderFooterHeight);

      float yTooltip = m_RenderYPos+m_RenderHeight-m_RenderFooterHeight;
      yTooltip += 0.4 * m_sfMenuPaddingY;

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawMessageLines( m_RenderXPos+m_sfMenuPaddingX, yTooltip, m_szCurrentTooltip, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenuSmall);

      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->drawLine(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight);
   }


   if ( s_bDebugMenuBoundingBoxes )
      _debugDrawBoundingBoxes(yPos);
   return yPos;
}

float Menu::RenderItem(int index, float yPos, float dx)
{
   m_bRenderedLastItem = false;
   if ( (NULL == m_pMenuItems[index]) || m_pMenuItems[index]->isHidden() )
      return 0.0;

   if ( m_bHasScrolling )
   if ( index < m_iIndexFirstVisibleItem )
      return 0.0;

   if ( -1 == m_ThisRenderCycleStartRenderItemIndex )
      m_ThisRenderCycleStartRenderItemIndex = index;
   else if ( index < m_ThisRenderCycleStartRenderItemIndex )
      m_ThisRenderCycleStartRenderItemIndex = index;
   
   if ( m_bHasScrolling )
   if ( -1 != m_ThisRenderCycleEndRenderItemIndex )
   if ( index > m_ThisRenderCycleEndRenderItemIndex )
      return 0.0;

   //dx += m_pMenuItems[index]->m_fMarginX;
   m_pMenuItems[index]->setLastRenderPos(m_RenderXPos + m_sfMenuPaddingX + dx + m_pMenuItems[index]->m_fMarginX, yPos);
   
   float fHeightFont = g_pRenderEngine->textHeight(g_idFontMenu);
   float hItem = m_pMenuItems[index]->getItemHeight(getUsableWidth() - m_pMenuItems[index]->m_fMarginX);
   
   float fTotalHeight = hItem;
   if ( m_bHasScrolling )
   if ( yPos + fTotalHeight > m_fRenderItemsStopYPos + 0.001 )
   {
      m_ThisRenderCycleEndRenderItemIndex = index;
      return 0.0;
   }

   fTotalHeight += fHeightFont * MENU_ITEM_SPACING;
   if ( m_bHasSeparatorAfter[index] )
      fTotalHeight += fHeightFont * MENU_SEPARATOR_HEIGHT;

   /*
   if ( m_bEnableScrolling && m_bHasScrolling )
   {
      if ( yPos < m_fRenderItemsStartYPos-0.01 )
         return fTotalHeight;
      if ( yPos + m_pMenuItems[index]->getItemHeight(getUsableWidth() - m_pMenuItems[index]->m_fMarginX) > m_fRenderItemsStopYPos + 0.001 )
         return fTotalHeight;
   }
   */

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( s_bDebugMenuBoundingBoxes )
   {
      g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
      g_pRenderEngine->setStroke(get_Color_IconSucces());
      g_pRenderEngine->drawRect( m_RenderXPos + m_sfMenuPaddingX + dx + g_pRenderEngine->getPixelWidth(),
          yPos + g_pRenderEngine->getPixelHeight(),
          getUsableWidth() - 2.0 * g_pRenderEngine->getPixelWidth(), hItem - 2.0*g_pRenderEngine->getPixelHeight()
          );

      g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
      g_pRenderEngine->setStroke(get_Color_IconError());
      g_pRenderEngine->drawRect( m_RenderXPos + m_sfMenuPaddingX + dx + 2.0*g_pRenderEngine->getPixelWidth(),
          yPos + 2.0*g_pRenderEngine->getPixelHeight(),
          m_fSelectionWidth - 4.0 * g_pRenderEngine->getPixelWidth(), hItem - 4.0*g_pRenderEngine->getPixelHeight()
          );


      if ( m_pMenuItems[index]->getExtraHeight() > 0.0001 )
      {
         g_pRenderEngine->setStroke(50,50,255,1);
         g_pRenderEngine->drawLine(m_RenderXPos + m_RenderWidth*0.5, yPos + hItem - m_pMenuItems[index]->getExtraHeight(), m_RenderXPos + m_RenderWidth*0.5, yPos + hItem);
      }

      g_pRenderEngine->setStroke(get_Color_MenuBorder());
   }

   m_pMenuItems[index]->Render(m_RenderXPos + m_sfMenuPaddingX + dx, yPos, index == m_SelectedIndex, m_fSelectionWidth);
   
   if ( m_bHasSeparatorAfter[index] && (!s_bMenuObjectsRenderEndItems) )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuText());
      float yLine = yPos+hItem+0.55*(fHeightFont * MENU_ITEM_SPACING + fHeightFont * MENU_SEPARATOR_HEIGHT);
      g_pRenderEngine->drawLine(m_RenderXPos+m_sfMenuPaddingX+dx, yLine, m_RenderXPos+m_RenderWidth - m_sfMenuPaddingX, yLine);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(0);
   }

   g_pRenderEngine->setColors(get_Color_MenuText());

   m_bRenderedLastItem = true;
   return fTotalHeight;
}

bool Menu::didRenderedLastItem()
{
   return m_bRenderedLastItem;
}

float Menu::_getMenuItemTotalRenderHeight(int iMenuItemIndex)
{
   if ( (iMenuItemIndex < 0) || (iMenuItemIndex >= m_ItemsCount) )
      return 0.0;
   if ( (NULL == m_pMenuItems[iMenuItemIndex]) || m_pMenuItems[iMenuItemIndex]->isHidden() )
      return 0.0;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float fItemTotalHeight = 0.0;
   if ( m_iColumnsCount < 2 )
   {
      fItemTotalHeight += m_pMenuItems[iMenuItemIndex]->getItemHeight(getUsableWidth() - m_pMenuItems[iMenuItemIndex]->m_fMarginX);
      fItemTotalHeight += height_text * MENU_ITEM_SPACING;
      if ( m_bHasSeparatorAfter[iMenuItemIndex] )
         fItemTotalHeight += height_text * MENU_SEPARATOR_HEIGHT;
   }
   else if ( (iMenuItemIndex%m_iColumnsCount) == 0 )
   {
      fItemTotalHeight += m_pMenuItems[iMenuItemIndex]->getItemHeight(getUsableWidth() - m_pMenuItems[iMenuItemIndex]->m_fMarginX);
      fItemTotalHeight += height_text * MENU_ITEM_SPACING;
      if ( m_bHasSeparatorAfter[iMenuItemIndex] )
         fItemTotalHeight += height_text * MENU_SEPARATOR_HEIGHT;
   }
   return fItemTotalHeight;
}

void Menu::_debugDrawBoundingBoxes(float yPos)
{
   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
   g_pRenderEngine->setStroke(get_Color_IconError());
   g_pRenderEngine->drawRect( m_RenderXPos - 6.0 * g_pRenderEngine->getPixelWidth(),
       yPos - 6.0*g_pRenderEngine->getPixelHeight(),
       m_RenderWidth + 12.0 * g_pRenderEngine->getPixelWidth(),
       m_fRenderItemsTotalHeight  + 12.0*g_pRenderEngine->getPixelHeight()
       );

   double col[4] = {255,255,0,1};
   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
   g_pRenderEngine->setStroke(col);
   g_pRenderEngine->drawRect( m_RenderXPos - 4.0 * g_pRenderEngine->getPixelWidth(),
       yPos - 4.0*g_pRenderEngine->getPixelHeight(),
       m_RenderWidth + 8.0 * g_pRenderEngine->getPixelWidth(),
       m_fRenderItemsVisibleHeight + 8.0*g_pRenderEngine->getPixelHeight()
       );

   double col2[4] = {50,105,255,1};
   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
   g_pRenderEngine->setStroke(col2);
   g_pRenderEngine->drawRect( m_RenderXPos - 2.0 * g_pRenderEngine->getPixelWidth(),
       m_fRenderItemsStartYPos - 2.0*g_pRenderEngine->getPixelHeight(),
       m_RenderWidth + 4.0 * g_pRenderEngine->getPixelWidth(),
       (m_fRenderItemsStopYPos - m_fRenderItemsStartYPos) + 4.0*g_pRenderEngine->getPixelHeight()
       );

   double col3[4] = {100,255,100,1};
   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.0);
   g_pRenderEngine->setStroke(col3);
   g_pRenderEngine->drawRect( m_RenderXPos - 12.0 * g_pRenderEngine->getPixelWidth(),
       m_RenderYPos, 3.0 * g_pRenderEngine->getPixelWidth(), m_RenderTotalHeight);

   g_pRenderEngine->drawRect( m_RenderXPos - 18.0 * g_pRenderEngine->getPixelWidth(),
       m_RenderYPos, 3.0 * g_pRenderEngine->getPixelWidth(), m_RenderHeaderHeight);

   g_pRenderEngine->drawRect( m_RenderXPos - 24.0 * g_pRenderEngine->getPixelWidth(),
       m_fRenderItemsStopYPos, 3.0 * g_pRenderEngine->getPixelWidth(), m_RenderMaxFooterHeight);

   g_pRenderEngine->drawRect( m_RenderXPos - 30.0 * g_pRenderEngine->getPixelWidth(),
       m_fRenderItemsStopYPos, 3.0 * g_pRenderEngine->getPixelWidth(), m_RenderMaxTooltipHeight);
   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(get_Color_MenuText());
}

void Menu::updateScrollingOnSelectionChange()
{
   if ( (! m_bEnableScrolling) || (! m_bHasScrolling) )
      return;

   // -------------------------------------
   // Begin - Scroll up

   int countItemsNonSelectableJustBeforeSelection = 0;
   while ( m_SelectedIndex - countItemsNonSelectableJustBeforeSelection - 1 >= 0 )
   {
      if ( ! m_pMenuItems[m_SelectedIndex - countItemsNonSelectableJustBeforeSelection - 1]->isHidden() )
      if ( m_pMenuItems[m_SelectedIndex - countItemsNonSelectableJustBeforeSelection - 1]->m_bEnabled )
      if ( m_pMenuItems[m_SelectedIndex - countItemsNonSelectableJustBeforeSelection - 1]->isSelectable() )
         break;
      countItemsNonSelectableJustBeforeSelection++;
   }

   if ( s_bDebugMenuBoundingBoxes )
      log_line("Menu-Scroll: Count non selectable top: %d, sel index: %d, first visible index: %d",
         countItemsNonSelectableJustBeforeSelection, m_SelectedIndex, m_iIndexFirstVisibleItem);

   if ( m_SelectedIndex < m_iIndexFirstVisibleItem )
   {
      m_iIndexFirstVisibleItem = m_SelectedIndex;
      m_iIndexFirstVisibleItem -= countItemsNonSelectableJustBeforeSelection;
      if ( m_iIndexFirstVisibleItem < 0 )
         m_iIndexFirstVisibleItem = 0;
      return;
   }

   // End - Scroll up
   // -------------------------------------

   // -------------------------------------
   // Begin - Scroll down

   // Compute total height of visible items to render
   // From first visible index up to current selected item

   float fTotalVisibleItemsHeight = 0.0;
   for( int i=m_iIndexFirstVisibleItem; i<=m_SelectedIndex; i++ )
   {
      fTotalVisibleItemsHeight += _getMenuItemTotalRenderHeight(i);
   }

   // End - computed total items height up to, including, selected item

   if ( s_bDebugMenuBoundingBoxes )
      log_line("Menu-Scroll: Total height from first visible item (%d) to current item (%d) inclusive: %.2f, menu visible usable items container height: %.2f",
          m_iIndexFirstVisibleItem, m_SelectedIndex, fTotalVisibleItemsHeight, m_fRenderItemsStopYPos - m_fRenderItemsStartYPos);

   // Move displayed items range down (that is first visible item index) untill the selected item becomes visible

   while ( fTotalVisibleItemsHeight >= m_fRenderItemsStopYPos - m_fRenderItemsStartYPos - 0.001 * m_sfScaleFactor)
   {
      fTotalVisibleItemsHeight -= _getMenuItemTotalRenderHeight(m_iIndexFirstVisibleItem);
      m_iIndexFirstVisibleItem++;
      if ( m_iIndexFirstVisibleItem >= m_ItemsCount-1 )
         break;

      if ( m_pMenuItems[m_iIndexFirstVisibleItem]->isHidden() )
         continue;
   }
}


int Menu::onBack()
{
   log_line("[Menu] (loop %u): onBack: id %d-%d, name [%s]",
       menu_get_loop_counter()%1000, m_MenuId%1000, m_MenuId/1000, m_szTitle);

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( ! m_pMenuItems[i]->isHidden() )
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         log_line("[Menu] menu id %d, end edit item %d", m_MenuId, m_SelectedIndex);
         m_pMenuItems[i]->endEdit(true);
         onItemValueChanged(i);
         onItemEndEdit(i);
         return 1;
      }
   }
   if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      m_bIsSelectingInsideColumn = false;
      m_SelectedIndex = ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      onFocusedItemChanged();
      return 1;
   }

   if ( ! m_bDisableBackAction )
   if ( ! m_bIsModal )
      menu_stack_pop(0);
   return 0;
}

void Menu::onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded)
{
   log_line("[Menu] (loop %u): id %d-%d, name [%s], vehicle command finished: command type: %s, succeeded? %s",
       menu_get_loop_counter()%1000, m_MenuId%1000, m_MenuId/1000, m_szTitle,
       commands_get_description(uCommandType), bSucceeded?"yes":"no");
}

void Menu::onSelectItem()
{
   log_line("[Menu] (loop %d) onSelectItem: menu id: %d, selected item index: %d", menu_get_loop_counter()%1000, m_MenuId, m_SelectedIndex);
   if ( (m_MenuId%1000) == MENU_ID_SIMPLE_MESSAGE ) // simple message menu? just pop it and return.
   {
      log_line("[Menu] onSelectItem: it's a simple message (MENU_ID_SIMPLE_MESSAGE), just pop it.");
      menu_stack_pop(0);
      return;
   }
   if ( (m_SelectedIndex < 0) || (m_SelectedIndex >= m_ItemsCount) )
      return;
   if ( m_pMenuItems[m_SelectedIndex]->isHidden() )
      return;
   if ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() )
      return;

   if ( m_pMenuItems[m_SelectedIndex]->isEditable() )
   {
      log_line("[Menu] (loop %d) Selected editable menu item.", menu_get_loop_counter()%1000);
      MenuItem* pSelectedItem = m_pMenuItems[m_SelectedIndex];
      if ( pSelectedItem->isEditing() )
      {
         if ( pSelectedItem->isEndEditOnBackOnly() )
         {
            // On press ok, move to next edit char, not end edit.
            // beginEdit() will move to next char if already editing.
            log_line("[Menu] (loop %d) Continue edit item %d", menu_get_loop_counter()%1000, m_SelectedIndex);
            pSelectedItem->beginEdit();
         }
         else
         {
            log_line("[Menu] (loop %d) End edit item %d", menu_get_loop_counter()%1000, m_SelectedIndex);
            pSelectedItem->endEdit(false);
            onItemEndEdit(m_SelectedIndex);
         }
      }
      else
      {
         log_line("[Menu] (loop %d) Begin edit item %d", menu_get_loop_counter()%1000, m_SelectedIndex);
         pSelectedItem->beginEdit();
      }
   }
   else
   {
      if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && (!m_bIsSelectingInsideColumn) )
      {
         m_bIsSelectingInsideColumn = true;
         m_SelectedIndex++;
         onFocusedItemChanged();
      }
      else
      {
         log_line("[Menu] (loop %d) Selected menu item %d", menu_get_loop_counter()%1000, m_SelectedIndex);
         m_pMenuItems[m_SelectedIndex]->onClick();
      }
   }
}


void Menu::onMoveUp(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop(0);
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   // Check for edit first
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isHidden() )
         continue;
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         if ( bIgnoreReversion || p->iSwapUpDownButtonsValues == 0 )
         {
            m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            //if ( isKeyMinusLongPressed() )
            //   for(int k=0; k<0; k++ )
            //      m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
         }
         else
         {
            m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            //if ( isKeyMinusLongPressed() )
            //   for(int k=0; k<0; k++ )
            //      m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
         }
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected item wants to preprocess up/down/left/right
   if ( (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) )
   if ( (NULL != m_pMenuItems[m_SelectedIndex]) && (m_pMenuItems[m_SelectedIndex]->isEnabled()) )
   if ( ! m_pMenuItems[m_SelectedIndex]->isHidden() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyUp() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

   int loopCount = 0;

   if ( (m_iColumnsCount > 1) && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      int indexStartColumn = 1 + ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      int indexEndColumn = indexStartColumn + m_iColumnsCount - 2;
      do
      {
         loopCount++;
         if ( m_SelectedIndex > indexStartColumn )
            m_SelectedIndex--;
         else
            m_SelectedIndex = indexEndColumn;
      }
      while ( (loopCount < (m_iColumnsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && ( ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
   }
   else
   {
      if ( m_bEnableColumnSelection )
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex >= m_iColumnsCount )
               m_SelectedIndex -= m_iColumnsCount;
            else
            {
               m_SelectedIndex = m_ItemsCount-1;
               m_SelectedIndex = ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
            }
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && (( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
      }
      else
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex >= 1 )
               m_SelectedIndex--;
            else
               m_SelectedIndex = m_ItemsCount-1;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && (( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
      }
   }
   if ( (! m_pMenuItems[m_SelectedIndex]->isSelectable()) || (m_pMenuItems[m_SelectedIndex]->isHidden()) )
      m_SelectedIndex = -1;
   onFocusedItemChanged();
}

void Menu::onMoveDown(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop(0);
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   // Check for edit first

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isHidden() )
         continue;
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         if ( bIgnoreReversion || p->iSwapUpDownButtonsValues == 0 )
         {
            m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            //if ( isKeyPlusLongPressed() )
            //   for(int k=0; k<0; k++ )
            //      m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
         }
         else
         {
            m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            //if ( isKeyPlusLongPressed() )
            //   for(int k=0; k<0; k++ )
            //      m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
         }
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected item wants to preprocess up/down/left/right
   if ( (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) )
   if ( (NULL != m_pMenuItems[m_SelectedIndex]) && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( ! m_pMenuItems[m_SelectedIndex]->isHidden() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyDown() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

   int loopCount = 0;

   if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      int indexStartColumn = 1 + ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      int indexEndColumn = indexStartColumn + m_iColumnsCount - 2;
      do
      {
         loopCount++;
         if ( m_SelectedIndex < indexEndColumn )
            m_SelectedIndex++;
         else
            m_SelectedIndex = indexStartColumn;
      }
      while ( (loopCount < (m_iColumnsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && ( ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
   }
   else
   {
      if ( m_bEnableColumnSelection )
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex < m_ItemsCount-m_iColumnsCount )
               m_SelectedIndex += m_iColumnsCount;
            else
               m_SelectedIndex = 0;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && ( ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
      }
      else
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex < m_ItemsCount-1 )
               m_SelectedIndex++;
            else
               m_SelectedIndex = 0;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && (NULL != m_pMenuItems[m_SelectedIndex]) && ( ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) || m_pMenuItems[m_SelectedIndex]->isHidden()) );
      }

   }
   if ( (! m_pMenuItems[m_SelectedIndex]->isSelectable()) || m_pMenuItems[m_SelectedIndex]->isHidden() )
      m_SelectedIndex = -1;
   onFocusedItemChanged();
}

void Menu::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop(0);
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isHidden() )
         continue;
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         m_pMenuItems[i]->onKeyLeft(bIgnoreReversion);
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected item wants to preprocess up/down/left/right
   if ( (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) )
   if ( (NULL != m_pMenuItems[m_SelectedIndex]) && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( ! m_pMenuItems[m_SelectedIndex]->isHidden() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyLeft() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

}

void Menu::onMoveRight(bool bIgnoreReversion)
{
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop(0);
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isHidden() )
         continue;
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         m_pMenuItems[i]->onKeyRight(bIgnoreReversion);
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected item wants to preprocess up/down/left/right
   if ( (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) )
   if ( (NULL != m_pMenuItems[m_SelectedIndex]) && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( ! m_pMenuItems[m_SelectedIndex]->isHidden() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyRight() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }
}


void Menu::onFocusedItemChanged()
{
   if ( (0 < m_ItemsCount) && (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) )
   if ( NULL != m_pMenuItems[m_SelectedIndex] )
   if ( ! m_pMenuItems[m_SelectedIndex]->isHidden() )
      setTooltip( m_pMenuItems[m_SelectedIndex]->getTooltip() );

   updateScrollingOnSelectionChange();
}

void Menu::onItemValueChanged(int itemIndex)
{
}

void Menu::onItemEndEdit(int itemIndex)
{
}

void Menu::onReturnFromChild(int iChildMenuId, int returnValue)
{
   log_line("[Menu] (loop %u): id %d-%d, name [%s], on return from child id %d-%d, result %d",
       menu_get_loop_counter()%1000, m_MenuId%1000, m_MenuId/1000, m_szTitle, iChildMenuId%1000, iChildMenuId/1000, returnValue);

   m_bInvalidated = true;
   m_bIsAnimationInProgress = false;

   m_uOnChildAddTime = 0;
   m_uOnChildCloseTime = 0;
      
   Preferences* pP = get_Preferences();
   if ( (NULL == pP) || (! pP->iMenusStacked) )
      return;

   if ( menu_had_disable_stacking(iChildMenuId) )
      return;
   if ( m_bDisableStacking || menu_hasAnyDisableStackingMenu() )
      return;

   m_uOnChildCloseTime = g_TimeNow;
   log_line("[Menu] On return from child will start animating, disable stacking: %d", (int)m_bDisableStacking);
   menu_startAnimationOnChildMenuClosed(this);
}

// It is called just before the new child menu is added to the stack.
// So it's not yet actually present in the stack.

void Menu::onChildMenuAdd(Menu* pChildMenu)
{
   log_line("[Menu] (loop %u) this menu %d-%d [%s]: add child menu: %d-%d [%s]",
      menu_get_loop_counter()%1000, m_MenuId%1000, m_MenuId/1000,
      m_szTitle, 
      pChildMenu->m_MenuId%1000, pChildMenu->m_MenuId/1000,
      pChildMenu->m_szTitle);

   m_uOnChildAddTime = g_TimeNow;
   m_uOnChildCloseTime = 0;
   m_bInvalidated = true;
   
   // Animate menus
   
   m_bIsAnimationInProgress = false;

   Preferences* pP = get_Preferences();
   if ( (NULL == pP) || (! pP->iMenusStacked) )
      return;

   if ( m_bDisableStacking || pChildMenu->m_bDisableStacking )
      return;
   if ( menu_hasAnyDisableStackingMenu() )
      return;

   menu_startAnimationOnChildMenuAdd(this);
}

void Menu::startAnimationOnChildMenuAdd()
{
   if ( m_RenderXPos < -8.0 )
      m_RenderXPos = m_xPos;

   if ( m_siRenderMode == 1 )
   {
      m_RenderXPos = m_xPos;
      return;
   }
   m_bIsAnimationInProgress = true;
   m_uAnimationStartTime = g_TimeNow;
   m_uAnimationLastStepTime = g_TimeNow;
   m_uAnimationTotalDuration = 280;

   m_fAnimationStartXPos = m_RenderXPos;
   if ( m_fAnimationTargetXPos < -8.0 )
      m_fAnimationTargetXPos = m_RenderXPos;
   m_fAnimationTargetXPos -= m_RenderWidth*0.4;

   log_line("[Menu] Start open menu animation for menu id %d, from xpos %.2f to xpos %.2f (original pos: %.2f, render pos now: %.2f)",
      m_MenuId, m_fAnimationStartXPos, m_fAnimationTargetXPos,
      m_xPos, m_RenderXPos);
}

void Menu::startAnimationOnChildMenuClosed()
{
   if ( m_RenderXPos < -8.0 )
      m_RenderXPos = m_xPos;

   if ( m_siRenderMode == 1 )
   {
      m_RenderXPos = m_xPos;
      return;
   }

   m_bIsAnimationInProgress = true;
   m_uAnimationStartTime = g_TimeNow;
   m_uAnimationLastStepTime = g_TimeNow;
   m_uAnimationTotalDuration = 200;

   m_fAnimationStartXPos = m_RenderXPos;

   if ( menu_is_menu_on_top(this) )
      m_fAnimationTargetXPos = m_xPos;
   else
   {
      if ( m_fAnimationTargetXPos < -8.0 )
         m_fAnimationTargetXPos = m_RenderXPos;
      m_fAnimationTargetXPos += m_RenderWidth*0.4;
   }

   log_line("[Menu] (loop %u) Start close menu animation for menu id %d, from xpos %.2f to xpos %.2f (original pos: %.2f, render pos now: %.2f)",
      menu_get_loop_counter()%1000, m_MenuId, m_fAnimationStartXPos, m_fAnimationTargetXPos,
      m_xPos, m_RenderXPos);

}
     
bool Menu::checkIsArmed()
{
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
   {
      char szText[256];
      if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
      {
         char szModelType[64];
         strcpy(szModelType, g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         sprintf(szText, "Your %s is armed, you can't perform this operation now. Please stop/disarm your %s first.", szModelType, szModelType);
      }
      else
         strcpy(szText, L("Your vehicle is armed, you can't perform this operation now. Please stop/disarm your vehicle first."));
      Popup* p = new Popup(szText, 0.3, 0.3, 0.5, 6 );
      p->setCentered();
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return true;
   }
   return false;
}

void Menu::addMessageWithTitle(int iId, const char* szTitle, const char* szMessage)
{
   Menu* pm = new MenuConfirmation(szTitle, szMessage, MENU_ID_SIMPLE_MESSAGE + iId*1000, true);
   pm->setId(MENU_ID_SIMPLE_MESSAGE + iId*1000);
   pm->m_xPos = 0.32; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm); 
}

void Menu::addMessage(const char* szMessage)
{
   addMessage(0, szMessage);
}

void Menu::addMessage(int iId, const char* szMessage)
{
   Menu* pm = new MenuConfirmation(L("Info"), szMessage, MENU_ID_SIMPLE_MESSAGE + iId*1000, true);
   pm->setId(MENU_ID_SIMPLE_MESSAGE + iId*1000);
   pm->m_xPos = 0.32; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm);
}

void Menu::addMessage2(int iId, const char* szMessage, const char* szLine2)
{
   Menu* pm = new MenuConfirmation(L("Info"), szMessage, MENU_ID_SIMPLE_MESSAGE + iId*1000, true);
   pm->setId(MENU_ID_SIMPLE_MESSAGE + iId*1000);
   pm->m_xPos = 0.32; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->m_bDisableStacking = true;
   if ( NULL != szLine2 )
      pm->addTopLine(szLine2);
   add_menu_to_stack(pm);
}

void Menu::addMessageWithTitleAndIcon(int iId, const char* szTitle, const char* szMessage, u32 uIconId)
{
   Menu* pm = new MenuConfirmation(szTitle, szMessage, MENU_ID_SIMPLE_MESSAGE + iId*1000, true);
   pm->setId(MENU_ID_SIMPLE_MESSAGE + iId*1000);
   pm->m_xPos = 0.32; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->m_bDisableStacking = true;
   pm->setIconId(uIconId);
   add_menu_to_stack(pm); 
}

bool Menu::checkCancelUpload()
{
   int iCount = 5;
   bool bBackPressed = false;
   while ( iCount > 0 )
   {
      int iEvent = keyboard_get_next_input_event();
      if ( iEvent == 0 )
         break;
      if ( iEvent == INPUT_EVENT_PRESS_BACK )
         bBackPressed = true; 
      iCount--;
   }
   
   if ( ! bBackPressed )
      return false;

   g_bUpdateInProgress = false;
   render_commands_set_custom_status(NULL);
   log_line("The software update was canceled by user.");
   hardware_sleep_ms(50);
   addMessage(L("Update was canceled."));
   g_nFailedOTAUpdates++;
   return true;
}

static int s_iThreadGenerateUploadCounter = 0;
static bool s_bThreadGenerateUploadError = false;
static char s_szThreadGenerateUploadErrorString[256];

static void * _thread_generate_upload(void *argument)
{
   char szComm[512];
   char* szFileNameArchive = (char*)argument;
   if ( NULL == argument )
      return NULL;

   hw_log_current_thread_attributes("generate upload arch");
   log_line("ThreadGenerateUpload started, counter %d, archive file to generate: %s", s_iThreadGenerateUploadCounter, szFileNameArchive);

   // Check and create update info file if missing
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_INFO_LAST_UPDATE);

   if( access(szFile, R_OK) == -1 )
   {
      char szVersionFile[MAX_FILE_PATH_SIZE];
      FILE* fd = try_open_base_version_file(szVersionFile);
      if ( NULL != fd )
      {
         fclose(fd);
         sprintf(szComm, "cp %s %s%s 2>/dev/null", szVersionFile, FOLDER_CONFIG, FILE_INFO_LAST_UPDATE);
         hw_execute_bash_command(szComm, NULL);
      }
      else
      {
         log_line("ThreadGenerateUpload: can't access base version file. Generate new update version file...");
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_INFO_LAST_UPDATE);
         FILE* fd = fopen(szFile, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%d.%d\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
            fclose(fd);
         }
      }
   }
   // Done - Check and create update info file if missing

   char szFullPathOutputArchive[MAX_FILE_PATH_SIZE];
   strcpy(szFullPathOutputArchive, FOLDER_UPDATES);
   strcat(szFullPathOutputArchive, szFileNameArchive);

   sprintf(szComm, "rm -rf %s 2>/dev/null", szFullPathOutputArchive);
   hw_execute_bash_command(szComm, NULL);

   char szPathTempUpload[MAX_FILE_PATH_SIZE];
   strcpy(szPathTempUpload, FOLDER_RUBY_TEMP);
   strcat(szPathTempUpload, "tempUploadFiles/");
   sprintf(szComm, "mkdir -p %s", szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   // Add binaries to upload archive

   char szFolderLocalUpdateBinaries[MAX_FILE_PATH_SIZE];

   strcpy(szFolderLocalUpdateBinaries, FOLDER_UPDATES);
   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
      strcat(szFolderLocalUpdateBinaries, SUBFOLDER_UPDATES_OIPC);
   else if ( hardware_board_is_raspberry(g_pCurrentModel->hwCapabilities.uBoardType) )
      strcat(szFolderLocalUpdateBinaries, SUBFOLDER_UPDATES_PI);
   else if ( hardware_board_is_radxa(g_pCurrentModel->hwCapabilities.uBoardType) )
      strcat(szFolderLocalUpdateBinaries, SUBFOLDER_UPDATES_RADXA);
   else
      log_error_and_alarm("ThreadGenerateUpload: Invalid vehicle board type: none of the known ones.");

   strcpy(szFile, szFolderLocalUpdateBinaries);
   strcat(szFile, "ruby_start");
   if ( ! hardware_file_check_and_fix_access(szFile) )
   {
      log_softerror_and_alarm("ThreadGenerateUpload: vehicle update binaries are missing from folder: %s", szFolderLocalUpdateBinaries);
      s_bThreadGenerateUploadError = true;
      snprintf(s_szThreadGenerateUploadErrorString, sizeof(s_szThreadGenerateUploadErrorString)/sizeof(s_szThreadGenerateUploadErrorString[0]),
        "Error: Failed to access file (%s)", szFile);
      return NULL;
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s* 2>/dev/null", szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s %s 2>/dev/null", FOLDER_CONFIG, FILE_INFO_LAST_UPDATE, szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   // This is added for updating vehicles older than 11.6, they look for ruby_update_vehicle to execute on update
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sruby_update %sruby_update_vehicle", szFolderLocalUpdateBinaries, szFolderLocalUpdateBinaries);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s/ruby_update_vehicle", szFolderLocalUpdateBinaries);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s* %s 2>/dev/null", szFolderLocalUpdateBinaries, szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   // Drivers not needed for now to be uploaded
   /*
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s 2>/dev/null", FOLDER_DRIVERS, szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   // Remove unnecessary drivers
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*pi*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*radxa*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
   }
   else if ( g_pCurrentModel->isRunningOnPiHardware() )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*oipc*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*radxa*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
   }
   else if ( g_pCurrentModel->isRunningOnRadxaHardware() )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*pi*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*oipc*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
   }
   */
   
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %smaj* %s 2>/dev/null", szFolderLocalUpdateBinaries, szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
   }
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %scustom* %s 2>/dev/null", szFolderLocalUpdateBinaries, szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);

   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
      sprintf(szComm, "tar -C %s -cf %s . 2>&1", szPathTempUpload, szFullPathOutputArchive);
   else
      sprintf(szComm, "tar -czf %s -C %s . 2>&1", szFullPathOutputArchive, szPathTempUpload);
   hw_execute_bash_command(szComm, NULL);
   
   if ( 0 < strlen(szPathTempUpload) )
   {
      sprintf(szComm, "rm -rf %s*", szPathTempUpload);
      hw_execute_bash_command(szComm, NULL);
   }
   sprintf(szComm, "chmod 777 %s 2>&1", szFullPathOutputArchive);
   hw_execute_bash_command(szComm, NULL);

   char szOutput[4096];
   szOutput[0] = 0;
   sprintf(szComm, "tar -tvf %s 2>&1", szFullPathOutputArchive);
   hw_execute_bash_command(szComm, szOutput);

   log_line("ThreadGenerateUpload: generated tar achive contains: [[[%s]]]", szOutput);
   log_line("ThreadGenerateUpload finished, counter %d", s_iThreadGenerateUploadCounter);
   s_iThreadGenerateUploadCounter--;
   return NULL;
}

bool Menu::_generate_upload_archive(char* szArchiveName)
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();

   s_bThreadGenerateUploadError = false;
   s_szThreadGenerateUploadErrorString[0] = 0;
   pthread_t pThread;
   if ( 0 != pthread_create(&pThread, NULL, &_thread_generate_upload, szArchiveName) )
   {
      render_commands_set_progress_percent(-1, true);
      ruby_resume_watchdog("upload generate failed");
      g_bUpdateInProgress = false;
      addMessage(L("There was an error generating software upload file."));
      return false;
   }
   else
      pthread_detach(pThread);
   s_iThreadGenerateUploadCounter++;

   // Wait for the thread to finish
   u32 uTimeLastRender = 0;
   while ( (s_iThreadGenerateUploadCounter > 0) && (!s_bThreadGenerateUploadError) )
   {
      try_read_messages_from_router(5);
      hardware_sleep_ms(5);
      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      if ( checkCancelUpload() )
      {
         render_commands_set_progress_percent(-1, true);
         ruby_resume_watchdog("upload canceled");
         g_bUpdateInProgress = false;
         return false;
      }

      if ( g_TimeNow > (uTimeLastRender+100) )
      {
         uTimeLastRender = g_TimeNow;
         render_commands_set_progress_percent(0, true);
         g_pRenderEngine->startFrame();
         //osd_render();
         render_background_and_paddings(false);
         popups_render();
         //menu_render();
         render_commands();
         popups_render_topmost();
         g_pRenderEngine->endFrame();
      }
   }
   if ( s_iThreadGenerateUploadCounter < 0 )
      s_iThreadGenerateUploadCounter = 0;
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();

   if ( s_bThreadGenerateUploadError )
   {
      render_commands_set_progress_percent(-1, true);
      ruby_resume_watchdog("upload thread error");
      g_bUpdateInProgress = false;
      return false;
   }
   return true;
}

u8 s_uOTAStatus = 0;
u32 s_uOTACounter = 0;
u32 s_uTimeLastOTACounterChanged = 0;

void Menu::updateOTAStatus(u8 uOTAStatus, u32 uOTACounter)
{
   s_uOTAStatus = uOTAStatus;
   s_uOTACounter = uOTACounter;
   s_uTimeLastOTACounterChanged = g_TimeNow;
}

bool Menu::uploadSoftware()
{
   log_line("Menu: Start upload procedure for vehicle software version %d.%d (mode: %s)...", get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel), g_pCurrentModel->is_spectator?"spectator mode":"control mode");

   ruby_pause_watchdog("uploading software to vehicle");
   render_commands_init();
   g_bUpdateInProgress = true;
   render_commands_set_progress_percent(0, true);
   g_pRenderEngine->startFrame();
   render_background_and_paddings(false);
   popups_render();
   render_commands();
   popups_render_topmost();
   g_pRenderEngine->endFrame();


   char szArchiveToUpload[MAX_FILE_PATH_SIZE];
   strcpy(szArchiveToUpload, "last_uploaded_archive.tar");

   render_commands_set_custom_status("Generating update archive to upload. Please wait.");
   if ( ! _generate_upload_archive(szArchiveToUpload) )
   {
      render_commands_set_progress_percent(-1, true);
      ruby_resume_watchdog("upload failed");
      g_bUpdateInProgress = false;
      if ( s_bThreadGenerateUploadError )
         addMessage2(0, L("Vehicle update binary files are missing or update procedure changed. Please update (again) your controller."), s_szThreadGenerateUploadErrorString);
      else
         addMessage(L("There was an error generating upload software archive."));
      return false;
   }
   render_commands_set_custom_status(NULL);

   log_line("Generated update archive to upload to vehicle (%s).", szArchiveToUpload);

   s_uOTAStatus = 0;
   s_uOTACounter = 0;
   s_uTimeLastOTACounterChanged = 0;

   render_commands_set_custom_status("Uploading software. Please wait.");
   if ( ! _uploadVehicleUpdate(szArchiveToUpload) )
   {
      render_commands_set_progress_percent(-1, true);
      ruby_resume_watchdog("upload failed");
      g_bUpdateInProgress = false;
      addMessage(L("There was an error updating your vehicle."));
      return false;
   }
   render_commands_set_custom_status(NULL);

  
   log_line("Successfully sent software package to vehicle.");
   bool bProcessingFailed = false;
   char szProcessingError[256];
   szProcessingError[0] = 0;

   render_commands_set_progress_percent(-1, true);
   render_commands_set_custom_status("Processing update on vehicle");

   u32 uTimeLastRender = 0;
   u32 uTimeStartProcessing = g_TimeNow;

   while ( true )
   {
      hardware_sleep_ms(100);
      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();
      if ( checkCancelUpload() )
      {
         log_line("Update was canceled by user.");
         bProcessingFailed = true;
         break;
      }

      try_read_messages_from_router(50);
      
      bool bTimedOut = false;
      if ( g_TimeNow > uTimeStartProcessing + 1000*300 )
         bTimedOut = true;
      if ( 0 != s_uTimeLastOTACounterChanged )
      if ( g_TimeNow > s_uTimeLastOTACounterChanged + 1000*20 )
         bTimedOut = true;

      if ( bTimedOut )
      {
         log_line("Update has timedout.");
         bProcessingFailed = true;
         break;          
      }

      if ( s_uOTAStatus == OTA_UPDATE_STATUS_START_PROCESSING )
         render_commands_set_custom_status("Start processing the update on the vehicle");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_UNPACK )
         render_commands_set_custom_status("Unpacking update");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_UPDATING )
         render_commands_set_custom_status("Updating vehicle");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_POST_UPDATING )
         render_commands_set_custom_status("Post update");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_COMPLETED )
         render_commands_set_custom_status("Finished. Cleaning up");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_REBOOT )
         render_commands_set_custom_status("Finished. Vehicle rebooting");
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_FAILED )
      {
         strcpy(szProcessingError, "Vehicle failed to process the update. Disk error.");
         render_commands_set_custom_status(szProcessingError);
         bProcessingFailed = true;
         break;
      }
      if ( s_uOTAStatus == OTA_UPDATE_STATUS_FAILED_DISK_SPACE )
      {
         strcpy(szProcessingError, "Vehicle failed to process the update. Not enough space on device.");
         render_commands_set_custom_status(szProcessingError);
         break;
      }
      if ( g_TimeNow > (uTimeLastRender+100) )
      {
         uTimeLastRender = g_TimeNow;
         g_pRenderEngine->startFrame();
         render_background_and_paddings(false);
         popups_render();
         render_commands();
         popups_render_topmost();
         g_pRenderEngine->endFrame();
      }

      if ( s_uOTAStatus == OTA_UPDATE_STATUS_COMPLETED )
         break;
   }

   log_line("Finished software upload part, status: %d", s_uOTAStatus);
   if ( (s_uOTAStatus != OTA_UPDATE_STATUS_COMPLETED) || bProcessingFailed )
   {
      render_commands_set_progress_percent(-1, true);
      render_commands_set_custom_status(NULL);
   }
   if ( bProcessingFailed )
   {
      ruby_resume_watchdog("upload processing failed");
      g_bUpdateInProgress = false;
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
      if ( 0 != szProcessingError[0] )
         addMessage(szProcessingError);
      return false;
   }

   g_nSucceededOTAUpdates++;
   g_bDidAnUpdate = true;
   
   log_line("Upload software: wait for vehicle to reboot...");
   render_commands_set_custom_status("Waiting vehicle to reboot");
   if ( NULL != g_pCurrentModel )
   {
      u32 uTimeLastRender = g_TimeNow = get_current_timestamp_ms();
      while ( s_uTimeLastOTACounterChanged > g_TimeNow - 2000 )
      {
         g_TimeNow = get_current_timestamp_ms();
         try_read_messages_from_router(50);
         ruby_signal_alive();
         if ( g_TimeNow > (uTimeLastRender+100) )
         {
            uTimeLastRender = g_TimeNow;
            g_pRenderEngine->startFrame();
            render_background_and_paddings(false);
            popups_render();
            render_commands();
            popups_render_topmost();
            g_pRenderEngine->endFrame();
         }
      }
      log_line("No more OTA messages from vehicle. Now wait for vehicle reboot...");
      u32 uTimeStartWait = g_TimeNow = get_current_timestamp_ms();
      while ( link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         g_TimeNow = get_current_timestamp_ms();
         if ( g_TimeNow > uTimeStartWait + 20000 )
         {
            log_softerror_and_alarm("Failed to wait for vehicle to go offline after %u ms. Proceed with finishing upload.", g_TimeNow - uTimeStartWait);
            break;
         }
         try_read_messages_from_router(50);
         ruby_signal_alive();
         if ( g_TimeNow > (uTimeLastRender+100) )
         {
            uTimeLastRender = g_TimeNow;
            g_pRenderEngine->startFrame();
            render_background_and_paddings(false);
            popups_render();
            render_commands();
            popups_render_topmost();
            g_pRenderEngine->endFrame();
         }
      }
      log_line("Finishing upload: Mark vehicle to sync settings.");
      g_pCurrentModel->b_mustSyncFromVehicle = true;
      g_bSyncModelSettingsOnLinkRecover = true;
   }
   log_line("Upload software: Finished waiting for vehicle to go offline.");

   render_commands_set_progress_percent(-1, true);
   render_commands_set_custom_status(NULL);

   g_bAskedForNegociateRadioLink = false;
   g_bUpdateInProgress = false;
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
   ruby_resume_watchdog("update finished");

   log_line("Finished software upload procedure.");
   return true;
}

MenuItemSelect* Menu::createMenuItemCardModelSelector(const char* szTitle)
{
   MenuItemSelect* pItem = new MenuItemSelect(szTitle, L("Sets the radio interface model."));
   pItem->addSelection("Autodetected");
   pItem->addSelection("Generic");
   for( int i=1; i<50; i++ )
   {
      const char* szDesc = str_get_radio_card_model_string(i);
      if ( (NULL != szDesc) && (0 != szDesc[0]) )
      if ( NULL == strstr(szDesc, "NONAME") )
      if ( NULL == strstr(szDesc, "Generic") )
         pItem->addSelection(szDesc);
   }
   pItem->setIsEditable();
   return pItem;
}

void Menu::setSelectionForCardModelSelector(MenuItemSelect* pItemSelect, int iCardModel)
{
   if ( NULL == pItemSelect )
      return;
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   pItemSelect->setSelectedIndex(1);
   if ( 0 == iCardModel )
      return;
   for( int i=1; i<50; i++ )
   {
      const char* szDesc = str_get_radio_card_model_string(i);
      if ( NULL != szDesc && 0 != szDesc[0] )
      if ( NULL == strstr(szDesc, "NONAME") )
      if ( NULL == strstr(szDesc, "Generic") )
      if ( i == iCardModel )
      {
         pItemSelect->setSelectedIndex(i+1);
         return;
      }
   }
}

u32 Menu::getSelectedCardForCardModelSelector(MenuItemSelect* pItemSelect)
{
   if ( NULL == pItemSelect )
      return 0xFF;
   int iSelectedIndex = pItemSelect->getSelectedIndex();

   if ( 0 == iSelectedIndex )
      return 0xFF;
   if ( 1 == iSelectedIndex )
      return 0;

   int iSelection = 2;
   for( int i=1; i<50; i++ )
   {
      const char* szDesc = str_get_radio_card_model_string(i);
      if ( NULL != szDesc && 0 != szDesc[0] )
      if ( NULL == strstr(szDesc, "NONAME") )
      if ( NULL == strstr(szDesc, "Generic") )
      {
         if ( iSelection == iSelectedIndex )
            return (u32)i;
         iSelection++;
      }
   }
   return 0;
}

MenuItemSelect* Menu::createMenuItemTxPowers(const char* szTitle, bool bAddAutoOption, bool bBooster2W, bool bBooster4W, int iMaxUsablePowerMw)
{
   int iPowerLevelsCount = 0;
   const int* piPowerLevelsUIMw = tx_powers_get_ui_levels_mw(&iPowerLevelsCount);

   MenuItemSelect* pItem = new MenuItemSelect(szTitle, L("Sets the radio Tx power level."));
   
   char szText[128];
   szText[0] = 0;

   if ( bAddAutoOption )
      pItem->addSelection("Auto");

   int iPrevValue = 0;
   for( int i=0; i<iPowerLevelsCount; i++ )
   {
      if ( piPowerLevelsUIMw[i] > (iMaxUsablePowerMw * (100 + MAX_TX_POWER_UI_DEVIATION_FROM_STANDARD)) / 100 )
         break;
      if ( (piPowerLevelsUIMw[i] >= 100) && (iPrevValue < 100) )
         pItem->addSeparator();
      sprintf(szText, "%d mW", piPowerLevelsUIMw[i]);
      if ( bBooster2W || bBooster4W )
      {
         if ( piPowerLevelsUIMw[i] < MIN_BOOST_INPUT_SIGNAL )
            sprintf(szText, "%d mW -> 0!", piPowerLevelsUIMw[i]);
         else if ( piPowerLevelsUIMw[i] > MAX_BOOST_INPUT_SIGNAL )
            sprintf(szText, "%d mW -> !!!", piPowerLevelsUIMw[i]);
         else if ( bBooster2W )
         {
            if ( tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 1, 0) >= 1000 )
               sprintf(szText, "%d -> %.1f W", piPowerLevelsUIMw[i], (float)tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 1, 0)/(float)1000.0);
            else
               sprintf(szText, "%d -> %d mW", piPowerLevelsUIMw[i], tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 1, 0));
         }
         else if ( bBooster4W )
         {
            if ( tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 1, 0) >= 1000 )
               sprintf(szText, "%d -> %.1f W", piPowerLevelsUIMw[i], (float)tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 0, 1)/(float)1000.0);
            else
               sprintf(szText, "%d -> %d mW", piPowerLevelsUIMw[i], tx_powers_get_mw_boosted_value_from_mw(piPowerLevelsUIMw[i], 0, 1));
         }
      }
      pItem->addSelection(szText);

      iPrevValue = piPowerLevelsUIMw[i];
   }
   pItem->addSelection("Custom");
   pItem->setIsEditable();
   return pItem;
}

void Menu::selectMenuItemTxPowersValue(MenuItemSelect* pMenuItem, bool bHasAutoOption, bool bBooster2W, bool bBooster4W, int* piCardsCurrentPowerLevelsMw, int iNumCards, int iMaxUsablePowerMw)
{
   if ( (NULL == pMenuItem) || (NULL == piCardsCurrentPowerLevelsMw) || (0 == iNumCards) )
      return;

   int iPowerLevelsUICount = 0;
   const int* piPowerLevelsUIMw = tx_powers_get_ui_levels_mw(&iPowerLevelsUICount);

   int iMaxCurrentPowerMw = 0;
   for( int i=0; i<iNumCards; i++ )
   {
      if ( piCardsCurrentPowerLevelsMw[i] > iMaxCurrentPowerMw )
         iMaxCurrentPowerMw = piCardsCurrentPowerLevelsMw[i];
   }
   if ( iMaxCurrentPowerMw > iMaxUsablePowerMw )
      iMaxCurrentPowerMw = iMaxUsablePowerMw;
   log_line("Menu: Select txpower item: Current max set tx power for all input cards: %d mw", iMaxCurrentPowerMw);
   
   int iMinDiffMw = 10000;
   int iMinDiffMwIndexUI = -1;
   for( int i=0; i<iPowerLevelsUICount; i++ )
   {
      if ( piPowerLevelsUIMw[i] > (iMaxUsablePowerMw * (100 + MAX_TX_POWER_UI_DEVIATION_FROM_STANDARD))/100 )
         break;
      int iDiffMw = piPowerLevelsUIMw[i] - iMaxCurrentPowerMw;
      if ( iDiffMw < 0 )
         iDiffMw = -iDiffMw;
      if ( iDiffMw < iMinDiffMw )
      {
         iMinDiffMw = iDiffMw;
         iMinDiffMwIndexUI = i;
      }
   }

   log_line("Menu: Matching tx power mw index: %d (value: %d mW), diff: %d mW (has auto option: %d)", iMinDiffMwIndexUI, (iMinDiffMwIndexUI != -1)?(piPowerLevelsUIMw[iMinDiffMwIndexUI]):-1, iMinDiffMw, bHasAutoOption);
   if ( iMinDiffMwIndexUI == -1 )
   {
      pMenuItem->setSelectedIndex(0);
      return;
   }

   if ( bHasAutoOption )
      pMenuItem->setSelectedIndex(iMinDiffMwIndexUI+1);
   else
      pMenuItem->setSelectedIndex(iMinDiffMwIndexUI);
   
   bool bBigDeviation = false;
   
   if ( iMinDiffMw > (piPowerLevelsUIMw[iMinDiffMwIndexUI] * MAX_TX_POWER_UI_DEVIATION_FROM_STANDARD) / 100 )
   //if ( piPowerLevelsUIMw[iMinDiffMwIndexUI] >= 20 )
      bBigDeviation = true;

   log_line("Menu: Matching tx power is big deviation: %d", bBigDeviation);

   if ( bBigDeviation )
   {
      int iIndex = pMenuItem->getSelectionsCount()-1;
      char szText[128];
      sprintf(szText, "Custom (%d mW)", iMaxCurrentPowerMw);
      if ( bBooster2W )
      {
         if ( tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 1, 0) >= 1000 )
            sprintf(szText, "Custom (%d mW -> %.1f W)", iMaxCurrentPowerMw, (float)tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 1, 0)/(float)1000.0);
         else
            sprintf(szText, "Custom (%d mW -> %d mW)", iMaxCurrentPowerMw, tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 1, 0));
      }
      if ( bBooster4W )
      {
         if ( tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 1, 0) >= 1000 )
            sprintf(szText, "Custom (%d mW -> %.1f W)", iMaxCurrentPowerMw, (float)tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 0, 1)/(float)1000.0);
         else
            sprintf(szText, "Custom (%d mW -> %d mW)", iMaxCurrentPowerMw, tx_powers_get_mw_boosted_value_from_mw(iMaxCurrentPowerMw, 0, 1));
      }
      pMenuItem->updateSelectionText(iIndex, szText);
      pMenuItem->setSelectedIndex(iIndex);
   }
   else
   {
      bBigDeviation = false;
      if ( piPowerLevelsUIMw[iMinDiffMwIndexUI] < 10 )
      if ( iMinDiffMw > 2 )
         bBigDeviation = true;

      if ( piPowerLevelsUIMw[iMinDiffMwIndexUI] >= 20 )
      if ( iMinDiffMw > (piPowerLevelsUIMw[iMinDiffMwIndexUI] * 5) / 100 )
         bBigDeviation = true;
      log_line("Menu: Matching tx power is big deviation (2): %d", bBigDeviation);
      if ( bBigDeviation )
      {
         int iIndex = iMinDiffMwIndexUI;
         if ( bHasAutoOption )
            iIndex++;
         char szText[128];
         strcpy(szText, "~");
         strcat(szText, pMenuItem->getSelectionIndexText(iIndex));
         pMenuItem->updateSelectionText(iIndex, szText);
      }
   }
}

bool Menu::_uploadVehicleUpdate(const char* szArchiveToUpload)
{
   command_packet_sw_package cpswp_cancel;
   cpswp_cancel.type = 1; // 0 - zip, 1 - tar
   cpswp_cancel.total_size = 0;
   cpswp_cancel.file_block_index = MAX_U32;
   cpswp_cancel.is_last_block = false;
   cpswp_cancel.block_length = 0;

   g_bUpdateInProgress = true;
   long lSize = 0;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_UPDATES);
   strcat(szFile, szArchiveToUpload);
   FILE* fd = fopen(szFile, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSize = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
   }

   log_line("Sending to vehicle the update archive (method 6.3): [%s], size: %d bytes", szFile, (int)lSize);

   fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      addMessage(L("There was an error generating the software package."));
      g_bUpdateInProgress = false;
      return false;
   }

   u32 blockSize = 1100;
   u32 nPackets = ((u32)lSize) / blockSize;
   if ( lSize > (int)(nPackets * blockSize) )
      nPackets++;

   u8** pPackets = (u8**) malloc(nPackets*sizeof(u8*));
   if ( NULL == pPackets )
   {
      addMessage(L("There was an error generating the upload package."));
      g_bUpdateInProgress = false;
      return false;
   }

   // Read packets in memory

   u32 nTotalPackets = 0;
   for( long l=0; l<lSize; )
   {
      u8* pPacket = (u8*) malloc(1500);
      if ( NULL == pPacket )
      {
         //free(pPackets);
         addMessage("There was an error generating the upload package.");
         g_bUpdateInProgress = false;
         return false;
      }
      command_packet_sw_package* pcpsp = (command_packet_sw_package*)pPacket;

      int nRead = fread(pPacket+sizeof(command_packet_sw_package), 1, blockSize, fd);
      if ( nRead < 0 )
      {
         //free((u8*)pPackets);
         addMessage(L("There was an error generating the upload package."));
         g_bUpdateInProgress = false;
         return false;
      }

      pcpsp->block_length = nRead;
      l += nRead;
      pcpsp->total_size = (u32)lSize;
      pcpsp->file_block_index = nTotalPackets;
      pcpsp->is_last_block = ((l == lSize)?true:false);
      pcpsp->type = 1; // 0 - zip, 1 - tar
      pPackets[nTotalPackets] = pPacket;
      nTotalPackets++;
   }

   log_line("Uploading %d sw segments", nTotalPackets);
   ruby_signal_alive();

   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED,0);

   g_TimeNow = get_current_timestamp_ms();
   g_TimeNowMicros = get_current_timestamp_micros();
   u32 uTimeLastRender = 0;

   static int s_iAckFrequency = DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;

   int iLastAcknowledgedPacket = -1;
   int iPacketToSend = 0;
   int iCountMaxRetriesForCurrentSegments = 10;
   while ( iPacketToSend < (int)nTotalPackets )
   {
      u8* pPacket = pPackets[iPacketToSend];
      command_packet_sw_package* pcpsp = (command_packet_sw_package*)pPacket;

      bool bWaitAck = true;
      if ( (! pcpsp->is_last_block) && ((iPacketToSend % s_iAckFrequency) != 0 ) )
         bWaitAck = false;
      
      if ( ! bWaitAck )
      {
         log_line("Send sw package block %d of %d", iPacketToSend + 1, nTotalPackets);

         g_TimeNow = get_current_timestamp_ms();
         g_TimeNowMicros = get_current_timestamp_micros();
         ruby_signal_alive();

         for( int k=0; k<2; k++ )
            handle_commands_send_single_oneway_command(0, COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, bWaitAck, pPacket, pcpsp->block_length+sizeof(command_packet_sw_package));
         hardware_sleep_ms(2);
         iPacketToSend++;
         continue;
      }

      u32 commandUID = handle_commands_increment_command_counter();
      u8 resendCounter = 0;
      int waitReplyTime = 100;
      bool gotResponse = false;
      bool responseOk = false;

      // Send packet and wait for acknoledge

      if ( iPacketToSend == (int)nTotalPackets - 1 )
         log_line("Send last sw package with ack, segment %d of %d", iPacketToSend + 1, nTotalPackets);
      else
         log_line("Send sw package with ack, segment %d of %d", iPacketToSend + 1, nTotalPackets);

      do
      {
         g_TimeNow = get_current_timestamp_ms();
         g_TimeNowMicros = get_current_timestamp_micros();
         ruby_signal_alive();

         if ( ! handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, resendCounter, bWaitAck, pPacket, pcpsp->block_length+sizeof(command_packet_sw_package)) )
         {
            addMessage(L("There was an error uploading the software package."));
            fclose(fd);
            g_nFailedOTAUpdates++;
            for( int i=0; i<5; i++ )
            {
               handle_commands_increment_command_counter();
               handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
               hardware_sleep_ms(20);
            }
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
            //for( int i=0; i<nTotalPackets; i++ )
            //   free((u8*)pPackets[i]);
            //free((u8*)pPackets);
            g_bUpdateInProgress = false;
            if ( s_iAckFrequency >= 2 )
               s_iAckFrequency /= 2;
            return false;
         }

         resendCounter++;
         gotResponse = false;
         u32 timeToWaitReply = g_TimeNow + waitReplyTime;
         log_line("Waiting for ACK for SW package segment %d, for %d ms, on retry: %d", iPacketToSend + 1, waitReplyTime, resendCounter-1);

         while ( g_TimeNow < timeToWaitReply && (! gotResponse) )
         {
            if ( checkCancelUpload() )
            {
               fclose(fd);
               for( int i=0; i<5; i++ )
               {
                  handle_commands_increment_command_counter();
                  handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
                  hardware_sleep_ms(20);
               }
               send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
               //for( int i=0; i<nTotalPackets; i++ )
               //   free((u8*)pPackets[i]);
               //free((u8*)pPackets);
               g_bUpdateInProgress = false;

               if ( s_iAckFrequency >= 2 )
                  s_iAckFrequency /= 2;
               return false;
            }

            g_TimeNow = get_current_timestamp_ms();
            g_TimeNowMicros = get_current_timestamp_micros();
            if ( try_read_messages_from_router(waitReplyTime) )
            {
               if ( handle_commands_get_last_command_id_response_received() == commandUID )
               {
                  gotResponse = true;
                  responseOk = handle_commands_last_command_succeeded();
                  log_line("Did got an ACK. Succeded: %s", responseOk?"yes":"no");
                  break;
               }
            }
            log_line("Did not get an ACK from vehicle. Keep waiting...");
         } // finised waiting for ACK

         if ( ! gotResponse )
         {
            log_line("Did not get an ACK from vehicle. Increase wait time.");
            waitReplyTime += 50;
            if ( waitReplyTime > 500 )
               waitReplyTime = 500;
         }
      }
      while ( (resendCounter < 15) && (! gotResponse) );

      // Finished sending and waiting for packet with ACK wait

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();

      if ( ! gotResponse )
      {
         log_softerror_and_alarm("Did not get a confirmation from vehicle about the software upload.");
         g_nFailedOTAUpdates++;
         fclose(fd);
         for( int i=0; i<5; i++ )
         {
            handle_commands_increment_command_counter();
            handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
            hardware_sleep_ms(20);
         }
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);

         //for( int i=0; i<nTotalPackets; i++ )
         //   free((u8*)pPackets[i]);
         //free((u8*)pPackets);
         g_bUpdateInProgress = false;
         return false;
      }

      if ( gotResponse && (!responseOk) )
      {
         // Restart from last confirmed packet
         log_softerror_and_alarm("The software package block (segment index %d) was rejected by the vehicle.", iPacketToSend+1);
         iPacketToSend = iLastAcknowledgedPacket; // will be +1 down below
         iCountMaxRetriesForCurrentSegments--;
         if ( iCountMaxRetriesForCurrentSegments < 0 )
         {
            g_nFailedOTAUpdates++;
            fclose(fd);
            for( int i=0; i<5; i++ )
            {
               handle_commands_increment_command_counter();
               handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
               hardware_sleep_ms(20);
            }
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);

            g_bUpdateInProgress = false;
            if ( s_iAckFrequency >= 2 )
               s_iAckFrequency /= 2;
            return false;
         }
      }

      if ( gotResponse && responseOk )
      {
         iCountMaxRetriesForCurrentSegments = 10;
         iLastAcknowledgedPacket = iPacketToSend;
         log_line("Got ACK for segment %d", iPacketToSend+1);
      }
      int percent = pcpsp->file_block_index*100/(pcpsp->total_size/blockSize);

      if ( checkCancelUpload() )
      {
         fclose(fd);
         for( int i=0; i<5; i++ )
         {
            handle_commands_increment_command_counter();
            handle_commands_send_command_once_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
            hardware_sleep_ms(20);
         }
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
         //for( int i=0; i<nTotalPackets; i++ )
         //   free((u8*)pPackets[i]);
         //free((u8*)pPackets);
         g_bUpdateInProgress = false;
         if ( s_iAckFrequency >= 2 )
            s_iAckFrequency /= 2;
         return false;
      }

      if ( g_TimeNow > (uTimeLastRender+100) )
      {
         uTimeLastRender = g_TimeNow;
         render_commands_set_progress_percent(percent, true);
         g_pRenderEngine->startFrame();
         //osd_render();
         render_background_and_paddings(false);
         popups_render();
         //menu_render();
         render_commands();
         popups_render_topmost();
         g_pRenderEngine->endFrame();
      }
      iPacketToSend++;
   }

   //for( int i=0; i<nTotalPackets; i++ )
   //   free((u8*)pPackets[i]);
   //free((u8*)pPackets);

   return true;
}

void Menu::addMessageNeedsVehcile(const char* szMessage, int iConfirmationId)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+iConfirmationId*1000, L("Not connected"), NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm);
}

void Menu::addUnsupportedMessageOpenIPC(const char* szMessage)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Functionality not supported"), NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   if ( (NULL != szMessage) && (0 != szMessage[0]) )
      pm->addTopLine(szMessage);
   else
      pm->addTopLine("This functionality is not supported on OpenIPC camera hardware.");
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm);
}

void Menu::addUnsupportedMessageOpenIPCGoke(const char* szMessage)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Functionality not supported"), NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   if ( (NULL != szMessage) && (0 != szMessage[0]) )
      pm->addTopLine(szMessage);
   else
      pm->addTopLine("This functionality is not supported on OpenIPC Goke camera hardware.");
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm);
}

void Menu::addUnsupportedMessageOpenIPCSigmaster(const char* szMessage)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Functionality not supported"), NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   if ( (NULL != szMessage) && (0 != szMessage[0]) )
      pm->addTopLine(szMessage);
   else
      pm->addTopLine("This functionality is not supported on OpenIPC Sigmaster SSC335 camera hardware.");
   pm->m_bDisableStacking = true;
   add_menu_to_stack(pm);
}
