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

#include "../base/base.h"
#include "popup.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "osd_common.h"
#include "menu.h"
#include "pairing.h"
#include "link_watch.h"
#include "shared_vars.h"
#include "timers.h"

float POPUP_LINE_SPACING = 0.5; // percentage of actual line height
float POPUP_ALPHA_WHEN_MENU_ON = 0.8;

Popup* sPopups[MAX_POPUPS];
int countPopups = 0;

Popup* sPopupsTopmost[MAX_POPUPS];
int countPopupsTopmost = 0;

Popup* sPopupsBottom[MAX_POPUPS];
int countPopupsBottom = 0;


int popups_get_count() { return countPopups; }
int popups_get_topmost_count() { return countPopupsTopmost; };

Popup* popups_get_at(int index)
{
   if ( index >= 0 && index < countPopups )
      return sPopups[index];
   return NULL;
}

Popup* popups_get_topmost_at(int index)
{
   if ( index >= 0 && index < countPopupsTopmost )
      return sPopupsTopmost[index];
   return NULL;
}

void popups_add(Popup* p)
{
   if ( NULL == p )
   {
      log_softerror_and_alarm("Tried to add a NULL popup to the stack");
      return;      
   }

   log_line("Added popup: %s", p->getTitle());

   for( int i=0; i<countPopups; i++ )
      if ( sPopups[i] == p )
      {
         p->onShow();
         return;
      }

   if ( countPopups < MAX_POPUPS )
   {
      p->onShow();
      sPopups[countPopups] = p;
      countPopups++;
   }
}

void popups_add_bottom(Popup* p)
{
   if ( NULL == p )
   {
      log_softerror_and_alarm("Tried to add a NULL popup to the stack");
      return;      
   }

   for( int i=0; i<countPopupsBottom; i++ )
   {
      if ( sPopupsBottom[i] == p )
      {
         p->onShow();
         return;
      }
   }
   if ( countPopupsBottom < MAX_POPUPS )
   {
      p->onShow();
      sPopupsBottom[countPopupsBottom] = p;
      countPopupsBottom++;
      log_line("Added bottom popup: [%s]", p->getTitle());
   }
}

void popups_add_topmost(Popup* p)
{
   if ( NULL == p )
   {
      log_softerror_and_alarm("Tried to add a NULL popup to the stack");
      return;      
   }

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( sPopupsTopmost[i] == p )
      {
         p->onShow();
         return;
      }

   if ( countPopupsTopmost < MAX_POPUPS )
   {
      p->onShow();
      sPopupsTopmost[countPopupsTopmost] = p;
      countPopupsTopmost++;
      log_line("Added topmost popup: [%s]", p->getTitle());
   }
}

bool popups_has_popup(Popup* p)
{
   if ( NULL == p )
      return false;

   for( int i=0; i<countPopups; i++ )
      if ( sPopups[i] == p )
         return true;

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( sPopupsTopmost[i] == p )
         return true;

   for( int i=0; i<countPopupsBottom; i++ )
      if ( sPopupsBottom[i] == p )
         return true;
   return false;
}

void popups_remove(Popup* p)
{
   if ( NULL == p )
      return;
   int i = 0;
   for( ; i<countPopups; i++ )
      if ( (NULL != sPopups[i]) && (sPopups[i] == p) )
         break;

   if ( (i < countPopups) && (sPopups[i] == p) )
   {
      for( ; i<countPopups-1; i++ )
         sPopups[i] = sPopups[i+1];
      countPopups--;
   }

   i = 0;
   for( ; i<countPopupsTopmost; i++ )
      if ( (NULL != sPopupsTopmost[i]) && (sPopupsTopmost[i] == p) )
         break;

   if ( (i < countPopupsTopmost) && (sPopupsTopmost[i] == p) )
   {
      log_line("Removed topmost popup: [%s]", p->getTitle());
      for( ; i<countPopupsTopmost-1; i++ )
         sPopupsTopmost[i] = sPopupsTopmost[i+1];
      countPopupsTopmost--;
   }

   i = 0;
   for( ; i<countPopupsBottom; i++ )
      if ( (NULL != sPopupsBottom[i]) && (sPopupsBottom[i] == p) )
         break;

   if ( (i < countPopupsBottom) && (sPopupsBottom[i] == p) )
   {
      log_line("Removed bottom popup: [%s]", p->getTitle());
      for( ; i<countPopupsBottom-1; i++ )
         sPopupsBottom[i] = sPopupsBottom[i+1];
      countPopupsBottom--;
   }
}

void popups_remove_all(Popup* pExceptionPopup)
{
   int iCountExceptions = 0;
   for( int i=0; i<countPopups; i++ )
   {
      if ( NULL != sPopups[i] )
      if ( (sPopups[i] == pExceptionPopup) || sPopups[i]->hasDisabledAutoRemove() )
      {
         sPopups[iCountExceptions] = sPopups[i];
         iCountExceptions++;
      }
   }

   countPopups = iCountExceptions;

   iCountExceptions = 0;
   for( int i=0; i<countPopupsTopmost; i++ )
   {
      if ( NULL != sPopupsTopmost[i] )
      if ( (sPopupsTopmost[i] == pExceptionPopup) || sPopupsTopmost[i]->hasDisabledAutoRemove() )
      {
         sPopupsTopmost[iCountExceptions] = sPopupsTopmost[i];
         iCountExceptions++;
      }
   }

   countPopupsTopmost = iCountExceptions;

   iCountExceptions = 0;
   for( int i=0; i<countPopupsBottom; i++ )
   {
      if ( NULL != sPopupsBottom[i] )
      if ( (sPopupsBottom[i] == pExceptionPopup) || sPopupsBottom[i]->hasDisabledAutoRemove() )
      {
         sPopupsBottom[iCountExceptions] = sPopupsBottom[i];
         iCountExceptions++;
      }
   }

   countPopupsBottom = iCountExceptions;
}

void popups_render()
{
   if ( render_engine_uses_raw_fonts() )
      POPUP_LINE_SPACING = 0.2;

   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   float fAlfa = g_pRenderEngine->getGlobalAlfa();

   g_pRenderEngine->disableAlphaBlending();

   if ( isMenuOn() )
   {
      g_pRenderEngine->enableAlphaBlending();
      g_pRenderEngine->setGlobalAlfa(POPUP_ALPHA_WHEN_MENU_ON);
   }

   for( int i=0; i<countPopups; i++ )
   {
      if ( NULL != sPopups[i] )
      {
         sPopups[i]->Render();
         if ( sPopups[i]->isExpired() )
         {
            sPopups[i] = NULL;
         }
      }
   }
   g_pRenderEngine->setGlobalAlfa(fAlfa);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);

   // Remove expired ones (NULL)
   int index = 0;
   int skip = 0;
   while( index < countPopups )
   {
       if ( sPopups[index+skip] == NULL )
       {
          skip++;
          countPopups--;
          continue;
       }
       sPopups[index] = sPopups[index+skip];
       index++;
   }
}

void popups_render_topmost()
{
   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   float fAlfa = g_pRenderEngine->setGlobalAlfa(1.0);

   g_pRenderEngine->disableAlphaBlending();

   if ( isMenuOn() )
   {
      g_pRenderEngine->enableAlphaBlending();
      g_pRenderEngine->setGlobalAlfa(POPUP_ALPHA_WHEN_MENU_ON);
   }

   for( int i=0; i<countPopupsTopmost; i++ )
   {
      if ( NULL != sPopupsTopmost[i] )
      {
         sPopupsTopmost[i]->Render();
         if ( sPopupsTopmost[i]->isExpired() )
            sPopupsTopmost[i] = NULL;
      }
   }

   g_pRenderEngine->setGlobalAlfa(fAlfa);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);

   // Remove expired ones (NULL)
   int index = 0;
   int skip = 0;
   while( index < countPopupsTopmost )
   {
       if ( sPopupsTopmost[index+skip] == NULL )
       {
          skip++;
          countPopupsTopmost--;
          continue;
       }
       sPopupsTopmost[index] = sPopupsTopmost[index+skip];
       index++;
   }
}


void popups_render_bottom()
{
   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   float fAlfa = g_pRenderEngine->getGlobalAlfa();

   g_pRenderEngine->disableAlphaBlending();

   if ( isMenuOn() )
      g_pRenderEngine->setGlobalAlfa(POPUP_ALPHA_WHEN_MENU_ON);

   for( int i=0; i<countPopupsBottom; i++ )
   {
      if ( NULL != sPopupsBottom[i] )
      {
         sPopupsBottom[i]->Render();
         if ( sPopupsBottom[i]->isExpired() )
            sPopupsBottom[i] = NULL;
      }
   }

   g_pRenderEngine->setGlobalAlfa(fAlfa);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);

   // Remove expired ones (NULL)
   int index = 0;
   int skip = 0;
   while( index < countPopupsBottom )
   {
       if ( sPopupsBottom[index+skip] == NULL )
       {
          skip++;
          countPopupsBottom--;
          continue;
       }
       sPopupsBottom[index] = sPopupsBottom[index+skip];
       index++;
   }
}

void popups_invalidate_all()
{
   for( int i=0; i<countPopups; i++ )
      if ( NULL != sPopups[i] )
         sPopups[i]->invalidate();

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( NULL != sPopupsTopmost[i] )
         sPopupsTopmost[i]->invalidate();

   for( int i=0; i<countPopupsBottom; i++ )
      if ( NULL != sPopupsBottom[i] )
         sPopupsBottom[i]->invalidate();
}


Popup::Popup(const char* title, float x, float y, float timeoutSec)
{
   m_bDisableAutoRemove = false;
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   int pos = strlen(m_szTitle)-1;
   while (pos >= 0 && m_szTitle[pos] == ' ' )
   {
      m_szTitle[pos] = 0;
      pos--;
   }
   for( int i=0; i<MAX_POPUPS; i++ )
      m_szLines[i] = NULL;

   m_xPos = x;
   m_yPos = y;
   m_fTimeoutSeconds = timeoutSec;
   m_uTimeoutEndTime = 0;
   if ( timeoutSec > 0.001 )
      m_uTimeoutEndTime = g_TimeNow + 1000 * timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = 0.8;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bShowTimeoutBar = false;
   m_bBelowMenu = false;
   m_bTopmost = false;
   m_uIdFont = g_idFontMenu;
   m_idIcon = 0;
   m_idIcon2 = 0;
   m_ColorIcon[0] = m_ColorIcon[1] = m_ColorIcon[2] = 0.0;
   m_ColorIcon[3] = 1.0;
   m_ColorIcon2[0] = m_ColorIcon2[1] = m_ColorIcon2[2] = 0.0;
   m_ColorIcon2[3] = 1.0;
   m_fIconWidth = 0;
   m_fIconWidth2 = 0;
   m_fIconHeight = 0;
   m_fIconHeight2 = 0;
   m_bCentered = false;
   m_bCenterTexts = false;
   m_bBottomAlign = false;
   m_bSmallLines = false;
   m_bNoBackground = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;
   m_fPaddingX = 0.0;
   m_fPaddingY = 0.0;
   m_uCreatedTime = g_TimeNow;
}

Popup::Popup(const char* title, float x, float y, float maxWidth, float timeoutSec)
{
   m_bDisableAutoRemove = false;
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   int pos = strlen(m_szTitle)-1;
   while (pos >= 0 && m_szTitle[pos] == ' ' )
   {
      m_szTitle[pos] = 0;
      pos--;
   }
   for( int i=0; i<MAX_POPUPS; i++ )
      m_szLines[i] = NULL;

   m_xPos = x;
   m_yPos = y;
   m_fTimeoutSeconds = timeoutSec;
   m_uTimeoutEndTime = 0;
   if ( timeoutSec > 0.001 )
      m_uTimeoutEndTime = g_TimeNow + 1000 * timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = maxWidth;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bShowTimeoutBar = false;
   m_bBelowMenu = false;
   m_bTopmost = false;
   m_uIdFont = g_idFontMenu;
   m_idIcon = 0;
   m_idIcon2 = 0;
   m_ColorIcon[0] = m_ColorIcon[1] = m_ColorIcon[2] = 0.0;
   m_ColorIcon[3] = 1.0;
   m_ColorIcon2[0] = m_ColorIcon2[1] = m_ColorIcon2[2] = 0.0;
   m_ColorIcon2[3] = 1.0;
   m_fIconWidth = 0;
   m_fIconWidth2 = 0;
   m_fIconHeight = 0;
   m_fIconHeight2 = 0;
   m_bCentered = false;
   m_bCenterTexts = false;
   m_bBottomAlign = false;
   m_bSmallLines = false;
   m_bNoBackground = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;
   m_fPaddingX = 0.0;
   m_fPaddingY = 0.0;
   m_uCreatedTime = g_TimeNow;
}


Popup::Popup(bool bCentered, const char* title, float timeoutSec)
{
   m_bDisableAutoRemove = false;
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   for( int i=0; i<MAX_POPUPS; i++ )
      m_szLines[i] = NULL;

   m_xPos = 0;
   m_yPos = 0;
   m_fTimeoutSeconds = timeoutSec;
   m_uTimeoutEndTime = 0;
   if ( timeoutSec > 0.001 )
      m_uTimeoutEndTime = g_TimeNow + 1000 * timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = 0.8;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bShowTimeoutBar = false;
   m_bBelowMenu = false;
   m_bTopmost = false;
   m_uIdFont = g_idFontMenu;
   m_idIcon = 0;
   m_idIcon2 = 0;
   m_ColorIcon[0] = m_ColorIcon[1] = m_ColorIcon[2] = 0.0;
   m_ColorIcon[3] = 1.0;
   m_ColorIcon2[0] = m_ColorIcon2[1] = m_ColorIcon2[2] = 0.0;
   m_ColorIcon2[3] = 1.0;
   m_fIconWidth = 0;
   m_fIconWidth2 = 0;
   m_fIconHeight = 0;
   m_fIconHeight2 = 0;
   m_bCentered = bCentered;
   m_bCenterTexts = false;
   m_bNoBackground = false;
   m_bSmallLines = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;
   m_fPaddingX = 0.0;
   m_fPaddingY = 0.0;

   if ( m_bCentered )
      m_bBottomAlign = false;

   m_uCreatedTime = g_TimeNow;
}


Popup::~Popup()
{
   removeAllLines();
}

const char* Popup::getTitle()
{
   return m_szTitle;
}

void Popup::setFont(u32 idFont)
{
   m_uIdFont = idFont;
   m_bInvalidated = true;
   setCustomPadding(m_fPadding);
}

u32 Popup::getFontId()
{
   return m_uIdFont;
}

void Popup::setCenteredTexts()
{
   m_bCenterTexts = true;
   m_bInvalidated = true;
}

bool Popup::isRenderBelowMenu()
{
   return m_bBelowMenu;
}

void Popup::setMaxWidth(float width)
{
   m_fMaxWidth = width;
   m_bInvalidated = true;
}

void Popup::setFixedWidth(float width)
{
   m_fFixedWidth = width;
   m_bInvalidated = true;
}

void Popup::setXPos(float xPos)
{
   m_xPos = xPos;
   m_bInvalidated = true;
}

void Popup::setYPos(float yPos)
{
   m_yPos = yPos;
   m_bInvalidated = true; 
}

void Popup::setBottomAlign(bool b)
{
   m_bBottomAlign = b;
   m_bInvalidated = true;
}

void Popup::setBottomMargin(float dymargin)
{
   m_fBottomMargin = dymargin;
   m_bInvalidated = true;
}

float Popup::getBottomMargin()
{
   return m_fBottomMargin;
}

void Popup::useSmallLines(bool bSmallLines)
{
   m_bSmallLines = bSmallLines;
   m_bInvalidated = true;
}

void Popup::setNoBackground()
{
   m_bNoBackground = true;
   m_bInvalidated = true;
}

void Popup::setBackgroundAlpha(float fAlpha)
{
   m_fBackgroundAlpha = fAlpha;
}

void Popup::setCustomPadding(float fPadding)
{
   m_fPadding = fPadding;
   
   m_fPaddingX = POPUP_MARGINS*g_pRenderEngine->textHeight(m_uIdFont)/g_pRenderEngine->getAspectRatio();
   m_fPaddingY = POPUP_MARGINS*g_pRenderEngine->textHeight(m_uIdFont);

   if ( m_fPadding > 0.0001 )
   {
      m_fPaddingX = g_pRenderEngine->textHeight(m_uIdFont) * m_fPadding/g_pRenderEngine->getAspectRatio();
      m_fPaddingY = g_pRenderEngine->textHeight(m_uIdFont) * m_fPadding;
   }
   m_bInvalidated = true;
}

int Popup::isExpired()
{
   if ( (m_fTimeoutSeconds > 0.001) && (m_uTimeoutEndTime !=0) )
   if (g_TimeNow > m_uTimeoutEndTime)
      return 1;
   return 0;
}

void Popup::resetTimer()
{
   m_StartTime = g_TimeNow;
   m_bInvalidated = true;
}

void Popup::setTimeout(float timeoutSeconds)
{
   m_fTimeoutSeconds = timeoutSeconds;
   m_uTimeoutEndTime = 0;
   if ( m_fTimeoutSeconds > 0.001 )
   {
      m_uTimeoutEndTime = g_TimeNow + 1000 * m_fTimeoutSeconds;
      log_line("Popup (%s) set timeout to %d ms from now", m_szTitle, m_uTimeoutEndTime-g_TimeNow);
   }
   m_bInvalidated = true;
}

float Popup::getTimeout()
{
   return m_fTimeoutSeconds;
}


void Popup::setIconId(u32 idIcon, const double* pColor)
{
   m_idIcon = idIcon;
   if ( NULL != pColor )
      memcpy((u8*)&m_ColorIcon, pColor, 4*sizeof(double));
   else
   {
      m_ColorIcon[0] = m_ColorIcon[1] = m_ColorIcon[2] = 0.0;
      m_ColorIcon[3] = 1.0;
   }
   m_bInvalidated = true;
}

void Popup::setIconId2(u32 idIcon2, const double* pColor2)
{
   m_idIcon2 = idIcon2;
   if ( NULL != pColor2 )
      memcpy((u8*)&m_ColorIcon2, pColor2, 4*sizeof(double));
   else
   {
      m_ColorIcon2[0] = m_ColorIcon2[1] = m_ColorIcon2[2] = 0.0;
      m_ColorIcon2[3] = 1.0;
   }
   m_bInvalidated = true; 
}

void Popup::invalidate()
{
   m_bInvalidated = true;
}

void Popup::refresh()
{
   m_bInvalidated = true;
}

void Popup::setRenderBelowMenu(bool bBelowMenu)
{
   m_bBelowMenu = bBelowMenu;
}

void Popup::showTimeoutProgress()
{
   m_bShowTimeoutBar = true;
}

void Popup::setCentered()
{
   m_bCentered = true;
   m_bBottomAlign = false;
}

void Popup::setTitle(const char* szTitle)
{
   if ( NULL == szTitle )
      m_szTitle[0] = 0;
   else
      strcpy(m_szTitle, szTitle);
   m_bInvalidated = true;
}

void Popup::addLine(const char* szLine)
{
   if ( NULL == szLine )
      return;
   if ( m_LinesCount < MAX_POPUP_LINES - 1 )
   {
      m_szLines[m_LinesCount] = (char*)malloc(strlen(szLine)+1);
      strcpy(m_szLines[m_LinesCount], szLine);
      m_LinesCount++;
   }
   else
   {
      free (m_szLines[0]);
      for( int i=0; i<MAX_POPUP_LINES-1; i++ )
         m_szLines[i] = m_szLines[i+1];
      m_szLines[MAX_POPUP_LINES-1] = (char*)malloc(strlen(szLine)+1);
      strcpy(m_szLines[MAX_POPUP_LINES-1], szLine);
   }
   m_bInvalidated = true;
}

u32 Popup::getCreationTime()
{
   return m_uCreatedTime;
}

bool Popup::hasDisabledAutoRemove()
{
   return m_bDisableAutoRemove;
}

void Popup::disableAutoRemove()
{
   m_bDisableAutoRemove = true;
}


void Popup::setLine(int iLine, const char* szLine)
{
   if ( (iLine<0) || (iLine >= m_LinesCount) )
      return;
   if ( NULL == szLine )
      return;
   if ( NULL != m_szLines[iLine] )
      free(m_szLines[iLine]);  

   m_szLines[iLine] = (char*)malloc(strlen(szLine)+1);
   strcpy(m_szLines[iLine], szLine);
}

char* Popup::getLine(int iLine)
{
   if ( (iLine < 0 ) || (iLine >= m_LinesCount) )
      return NULL;
   return m_szLines[iLine];
}

int Popup::getLinesCount()
{
   return m_LinesCount;
}


void Popup::removeLastLine()
{
   if ( m_LinesCount > 0 )
   {
      free ( m_szLines[m_LinesCount-1] );
      m_szLines[m_LinesCount-1] = NULL;
      m_LinesCount--;
   }
   m_bInvalidated = true;
}

void Popup::removeAllLines()
{
   for( int i=0; i<m_LinesCount; i++ )
   {
      free ( m_szLines[i] );
      m_szLines[i] = NULL;
   }
   m_LinesCount = 0;
   m_bInvalidated = true;
}

float Popup::getRenderHeight()
{
   if ( m_bInvalidated )
      computeSize();
   return m_RenderHeight;
}

void Popup::onShow()
{
   setCustomPadding(m_fPadding);
   resetTimer();
   m_bInvalidated = true;
}

float Popup::computeIconsSizes()
{
   float height_text = g_pRenderEngine->textHeight(m_uIdFont);
   m_fIconWidth = 0;
   m_fIconWidth2 = 0;
   m_fIconHeight = 0;
   m_fIconHeight2 = 0;
   float fDeltaWidthIcons = 0.0;
   if ( 0 != m_idIcon )
   {
      m_fIconHeight = height_text*1.4;
      if ( m_RenderHeight-2.0*m_fPaddingY > 2.0*height_text )
         m_fIconHeight = height_text*2.2;
      if ( m_RenderHeight-2.0*m_fPaddingY > 3.6*height_text )
         m_fIconHeight = height_text*3.2;
      if ( m_RenderHeight-2.0*m_fPaddingY >= 4.6*height_text )
         m_fIconHeight = height_text*3.8;

      if ( m_fIconHeight < height_text*1.8 )
      if ( m_idIcon == g_idIconController )
         m_fIconHeight = height_text*2.0;

      m_fIconWidth = m_fIconHeight / g_pRenderEngine->getAspectRatio();
      fDeltaWidthIcons += m_fIconWidth;
   }

   if ( 0 != m_idIcon2 )
   {
      m_fIconHeight2 = height_text*1.4;
      if ( m_RenderHeight-2.0*m_fPaddingY > 2.0*height_text )
         m_fIconHeight2 = height_text*2.2;
      if ( m_RenderHeight-2.0*m_fPaddingY > 3.6*height_text )
         m_fIconHeight2 = height_text*3.2;
      if ( m_RenderHeight-2.0*m_fPaddingY >= 4.6*height_text )
         m_fIconHeight2 = height_text*3.8;

      if ( m_fIconHeight2 < height_text*1.8 )
      if ( m_idIcon2 == g_idIconController )
         m_fIconHeight2 = height_text*2.0;

      m_fIconHeight *= 1.1;
      m_fIconWidth2 = m_fIconHeight2 / g_pRenderEngine->getAspectRatio();
      fDeltaWidthIcons += m_fIconWidth2;
      if ( 0 != m_idIcon )
         fDeltaWidthIcons += height_text*0.4;
   }

   if ( (0 != m_idIcon) || (0 != m_idIcon2) )
      fDeltaWidthIcons += height_text*0.4;
   return fDeltaWidthIcons;
}

void Popup::computeSize()
{
   float height_text = g_pRenderEngine->textHeight(m_uIdFont);

   if ( m_fMaxWidth > 0.8 )
      m_fMaxWidth = 0.8;
   if ( m_fMaxWidth < 0.1 )
      m_fMaxWidth = 0.1;

   // Compute width

   float fMaxTextWidth = 0.0;
      
   if ( m_fFixedWidth > 0.01 )
      fMaxTextWidth = m_fFixedWidth - 2.0 * m_fPaddingX;
   else
   {
      if ( 0 != m_szTitle[0] )
      {
         //fMaxTextWidth = g_pRenderEngine->textWidth(m_uIdFont, m_szTitle);
         fMaxTextWidth = g_pRenderEngine->getMessageWidth(m_szTitle, m_fMaxWidth, m_uIdFont );
      }  
      for (int i = 0; i<m_LinesCount; i++)
      {
         //float fTextWidth = g_pRenderEngine->textWidth(m_uIdFont, (const char*)(m_szLines[i]));
         //if ( m_bSmallLines )
         //   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenuSmall, (const char*)(m_szLines[i]));
         float fTextWidth = g_pRenderEngine->getMessageWidth((const char*)(m_szLines[i]), m_fMaxWidth, m_uIdFont);
         if ( m_bSmallLines )
            fTextWidth = g_pRenderEngine->getMessageWidth((const char*)(m_szLines[i]), m_fMaxWidth, g_idFontMenuSmall);
         
         if ( fTextWidth > fMaxTextWidth )
            fMaxTextWidth = fTextWidth;
      }

      if ( fMaxTextWidth > m_fMaxWidth - 2.0 * m_fPaddingX )
         fMaxTextWidth = m_fMaxWidth - 2.0 * m_fPaddingX;
   }
   m_RenderWidth = fMaxTextWidth + 2.0 * m_fPaddingX;

   // Compute height

   m_RenderHeight = 2.0 * m_fPaddingY;
   
   if ( 0 != m_szTitle[0] )
   {
      float fHeight = g_pRenderEngine->getMessageHeight(m_szTitle, POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
      m_RenderHeight += fHeight;
   }

   for (int i = 0; i<m_LinesCount; i++)
   {
      m_RenderHeight += height_text*POPUP_LINE_SPACING;
      
      float fHeight = g_pRenderEngine->getMessageHeight((const char*)(m_szLines[i]), POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
      if ( m_bSmallLines )
         fHeight = g_pRenderEngine->getMessageHeight((const char*)(m_szLines[i]), POPUP_LINE_SPACING, fMaxTextWidth, g_idFontMenuSmall);
      m_RenderHeight += fHeight;
   }

   float fDeltaWidthIcons = computeIconsSizes();
   if ( m_bCentered )
   {
      m_RenderXPos = (1.0-m_RenderWidth)/2.0 - fDeltaWidthIcons;
      m_RenderYPos = (1.0-m_RenderHeight)/2.0;
   }
   else
   {
      m_RenderXPos = m_xPos;
      m_RenderYPos = m_yPos;
   }

   if ( m_bBottomAlign )
   {
      //m_RenderYPos = 0.84 - m_RenderHeight;
      m_RenderYPos = 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() - m_RenderHeight;
   }
   m_RenderYPos -= m_fBottomMargin;

   if ( g_pRenderEngine->isFrameStarted() )
      m_bInvalidated = false;
}

void Popup::Render()
{
   if ( m_bInvalidated )
      computeSize();

   if ( (m_fTimeoutSeconds > 0.001) && (m_uTimeoutEndTime != 0) )
   if ( g_TimeNow > m_uTimeoutEndTime )
      return;

   float height_text = g_pRenderEngine->textHeight(m_uIdFont);

   float alfaOrg = g_pRenderEngine->getGlobalAlfa();
   float alfa = alfaOrg;

   float fFadeInterval = 1500;
   if ( (m_fTimeoutSeconds > 1.0) && (m_uTimeoutEndTime != 0) )
   if ( g_TimeNow > m_uTimeoutEndTime - fFadeInterval )
   {
      float fDelta = g_TimeNow - (m_uTimeoutEndTime-fFadeInterval);
      fDelta = fDelta/fFadeInterval;
      alfa = alfaOrg * (1 - fDelta);
   }
   if ( alfa < 0 ) alfa = 0;
   if ( alfa > alfaOrg ) alfa = alfaOrg;

   g_pRenderEngine->setGlobalAlfa(alfa);

   double colorBg[4];
   memcpy(&colorBg, get_Color_PopupBg(), 4*sizeof(double));
   colorBg[3] = colorBg[3] * m_fBackgroundAlpha;
   if ( colorBg[3] > 1.0 )
      colorBg[3] = 1.0;
   
   if ( m_fBackgroundAlpha < 0.99 )
      g_pRenderEngine->setColors(colorBg);
   else
   {
      g_pRenderEngine->setColors(get_Color_PopupBg());
      g_pRenderEngine->setStroke(get_Color_PopupBorder());
   }
   
   float fDeltaWidthIcons = computeIconsSizes();
   if ( ! m_bNoBackground )
      g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos, m_RenderWidth + fDeltaWidthIcons, m_RenderHeight, POPUP_ROUND_MARGIN);


   if ( m_bShowTimeoutBar )
   if ( (m_fTimeoutSeconds > 0.001) && (m_uTimeoutEndTime != 0) )
   if ( g_TimeNow <= m_uTimeoutEndTime )
   {
      float fBarPercentage = 1.0 - (float)(m_uTimeoutEndTime - g_TimeNow)/(1000.0*m_fTimeoutSeconds);
      if ( fBarPercentage < 0.001 )
         fBarPercentage = 0.001;
      if ( fBarPercentage > 1.0 )
         fBarPercentage = 1.0;

      float fBarHeight = height_text*0.5;

      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos + m_RenderHeight - fBarHeight, m_RenderWidth + fDeltaWidthIcons, fBarHeight, POPUP_ROUND_MARGIN);

      g_pRenderEngine->setColors(get_Color_PopupText());
      g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos + m_RenderHeight - fBarHeight, fBarPercentage * (m_RenderWidth + fDeltaWidthIcons), fBarHeight, POPUP_ROUND_MARGIN);
   }

   float xTextStart = m_RenderXPos+m_fPaddingX;
   float yTextStart = m_RenderYPos+m_fPaddingY;
   
   if ( 0 != m_idIcon2 )
   {
      if ( m_ColorIcon2[3] > 0.0001 )
         g_pRenderEngine->setColors(&m_ColorIcon2[0]);
      float yIcon = m_RenderYPos + 0.5*(m_RenderHeight - m_fIconHeight2);
      g_pRenderEngine->drawIcon(xTextStart, yIcon, m_fIconWidth2, m_fIconHeight2, m_idIcon2);  
      xTextStart += m_fIconWidth2;
   }

   if ( 0 != m_idIcon )
   {
      if ( 0 != m_idIcon2 )
         xTextStart += height_text * 0.4;
      if ( m_ColorIcon[3] > 0.0001 )
         g_pRenderEngine->setColors(&m_ColorIcon[0]);
      float yIcon = m_RenderYPos + 0.5*(m_RenderHeight - m_fIconHeight);
      g_pRenderEngine->drawIcon(xTextStart, yIcon, m_fIconWidth, m_fIconHeight, m_idIcon);  
      xTextStart += m_fIconWidth;
   }

   if ( (0 != m_idIcon) || (0 != m_idIcon2) )
      xTextStart += height_text*0.4;
   
   g_pRenderEngine->setColors(get_Color_PopupText());
   
   float fMaxTextWidth = m_RenderWidth - 2.0*m_fPaddingX;
   
   if ( 0 != m_szTitle[0] )
   {
      float fHeightText = 0.0;
      if ( m_bCenterTexts )
      {
         float fTextWidth = g_pRenderEngine->textWidth(m_uIdFont, m_szTitle);
         if ( fTextWidth > m_RenderWidth - 2.0 * m_fPaddingX )
            fTextWidth = m_RenderWidth - 2.0 * m_fPaddingX;
         fHeightText = g_pRenderEngine->drawMessageLines(xTextStart + 0.5*(fMaxTextWidth-fTextWidth), yTextStart, m_szTitle, POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
      }
      else
         fHeightText = g_pRenderEngine->drawMessageLines(xTextStart, yTextStart, m_szTitle, POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
      yTextStart += fHeightText;
   }
   
   for (int i = 0; i<m_LinesCount; i++)
   {
      yTextStart += height_text*POPUP_LINE_SPACING;
      float fHeightText = 0.0;
      if ( m_bSmallLines )
      {
         if ( m_bCenterTexts )
         {
            float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenuSmall, (const char*)(m_szLines[i]));
            if ( fTextWidth > m_RenderWidth - 2.0 * m_fPaddingX )
               fTextWidth = m_RenderWidth - 2.0 * m_fPaddingX;
            fHeightText = g_pRenderEngine->drawMessageLines(xTextStart + 0.5*(fMaxTextWidth - fTextWidth), yTextStart, m_szLines[i], POPUP_LINE_SPACING, fMaxTextWidth, g_idFontMenuSmall);
         }
         else
            fHeightText = g_pRenderEngine->drawMessageLines(xTextStart, yTextStart, m_szLines[i], POPUP_LINE_SPACING, fMaxTextWidth, g_idFontMenuSmall);
      }
      else
      {
         if ( m_bCenterTexts )
         {
            float fTextWidth = g_pRenderEngine->textWidth(m_uIdFont, (const char*)(m_szLines[i]));
            if ( fTextWidth > m_RenderWidth - 2.0 * m_fPaddingX )
               fTextWidth = m_RenderWidth - 2.0 * m_fPaddingX;
            fHeightText = g_pRenderEngine->drawMessageLines(xTextStart + 0.5*(fMaxTextWidth-fTextWidth), yTextStart, m_szLines[i], POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
         }
         else
            fHeightText = g_pRenderEngine->drawMessageLines(xTextStart, yTextStart, m_szLines[i], POPUP_LINE_SPACING, fMaxTextWidth, m_uIdFont);
      }
      yTextStart += fHeightText;
   }
   g_pRenderEngine->setGlobalAlfa(alfaOrg);
}

