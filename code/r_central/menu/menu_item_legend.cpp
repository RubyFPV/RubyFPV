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

#include "menu_item_legend.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemLegend::MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth)
:MenuItem(szTitle, szDesc)
{
   m_bEnabled = false;
   m_bIsEditable = false;
   m_bSmall = false;
   m_fMarginX = Menu::getMenuPaddingX();
   m_fMaxWidth = maxWidth;
   m_pszTitle = (char*)malloc(strlen(szTitle)+7);
   //strcpy(m_pszTitle, "• ");
   //strcat(m_pszTitle, szTitle);
   strcpy(m_pszTitle, szTitle);
   if ( NULL != szDesc && 0 < strlen(szDesc) )
      strcat(m_pszTitle, ":");
}


MenuItemLegend::MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth, bool bSmall)
:MenuItem(szTitle, szDesc)
{
   m_bEnabled = false;
   m_bIsEditable = false;
   m_bSmall = bSmall;
   m_fMarginX = Menu::getMenuPaddingX();
   m_fMaxWidth = maxWidth;
   m_pszTitle = (char*)malloc(strlen(szTitle)+7);
   //strcpy(m_pszTitle, "• ");
   //strcat(m_pszTitle, szTitle);
   strcpy(m_pszTitle, szTitle);
   if ( NULL != szDesc && 0 < strlen(szDesc) )
      strcat(m_pszTitle, ":");
}

MenuItemLegend::~MenuItemLegend()
{
}

float MenuItemLegend::getItemHeight(float maxWidth)
{
   int iFont = g_idFontMenu;
   if ( m_bSmall )
      iFont = g_idFontMenuSmall;

   m_RenderTitleHeight = g_pRenderEngine->textHeight(iFont);
   m_RenderHeight = m_RenderTitleHeight + m_fExtraHeight;

   getTitleWidth(maxWidth);
   if ( maxWidth > 0.0001 )
      getValueWidth(maxWidth-m_RenderTitleWidth-Menu::getMenuPaddingX());
   else
      getValueWidth(0);
   float h = 0.0;
   if ( (NULL != m_pszTooltip) && (0 != m_pszTooltip[0]) )
   {
      //h = g_pRenderEngine->getMessageHeight(m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, iFont);

      char szTokens[256];
      strncpy(szTokens, m_pszTooltip, sizeof(szTokens)/sizeof(szTokens[0]));
      char* pToken = strtok(szTokens, "\n");
      while ( NULL != pToken )
      {
         h += g_pRenderEngine->getMessageHeight(pToken, MENU_TEXTLINE_SPACING, m_RenderValueWidth, iFont);
         pToken = strtok(NULL, "\n");
      }
   }
   if ( h > m_RenderHeight )
      m_RenderHeight = h;

   m_RenderHeight += 0.3 * g_pRenderEngine->textHeight(iFont);
   return m_RenderHeight + m_fExtraHeight;
}

float MenuItemLegend::getTitleWidth(float maxWidth)
{
   int iFont = g_idFontMenu;
   if ( m_bSmall )
      iFont = g_idFontMenuSmall;

   if ( m_fMaxWidth > 0.001 )
   {
      if ( m_RenderTitleWidth > 0.001 )
         return m_RenderTitleWidth;

      m_RenderTitleWidth = g_pRenderEngine->textWidth(iFont, m_pszTitle);

      if ( m_bShowArrow )
         m_RenderTitleWidth += 0.66*g_pRenderEngine->textHeight(iFont);
      return m_RenderTitleWidth;
   }

   m_RenderTitleWidth = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, iFont);
   return m_RenderTitleWidth;
}

float MenuItemLegend::getValueWidth(float maxWidth)
{
   int iFont = g_idFontMenu;
   if ( m_bSmall )
      iFont = g_idFontMenuSmall;
   m_RenderTitleWidth = g_pRenderEngine->textWidth(iFont, m_pszTitle);
   m_RenderValueWidth = maxWidth - m_RenderTitleWidth;
   return m_RenderValueWidth;
}


void MenuItemLegend::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;

   g_pRenderEngine->setColors(get_Color_MenuText());

   int iFont = g_idFontMenu;
   if ( m_bSmall )
      iFont = g_idFontMenuSmall;

   g_pRenderEngine->drawText(xPos, yPos, iFont, m_pszTitle);
   //g_pRenderEngine->drawMessageLines(xPos, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_RenderTitleWidth, iFont);

   m_RenderTitleWidth = g_pRenderEngine->textWidth(iFont, m_pszTitle);

   xPos += m_RenderTitleWidth + Menu::getMenuPaddingX();

   if ( (NULL != m_pszTooltip) && (0 != m_pszTooltip[0]) )
   {
      //g_pRenderEngine->drawMessageLines(xPos, yPos, m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, iFont);
      char szTokens[256];
      strncpy(szTokens, m_pszTooltip, sizeof(szTokens)/sizeof(szTokens[0]));
      char* pToken = strtok(szTokens, "\n");
      while ( NULL != pToken )
      {
         float h = g_pRenderEngine->drawMessageLines(xPos, yPos, pToken, MENU_TEXTLINE_SPACING, m_RenderValueWidth, iFont);
         yPos += h;
         pToken = strtok(NULL, "\n");
      }
   }
}
