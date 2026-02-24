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
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/hardware_procs.h"
#include "../base/hdmi.h"
#include "../osd/osd_common.h"

#define MAX_FONTS_SIZES 50

bool s_bFontsInitialized = false;

u32 s_ListOSDFontSizes[50];
int s_iListOSDFontSizesCount = 0;

u32 s_ListMenuFontSizes[50];
int s_iListMenuFontSizesCount = 0;

u32 g_idFontOSD = 0;
u32 g_idFontOSDBig = 0;
u32 g_idFontOSDSmall = 0;
u32 g_idFontOSDExtraSmall = 0;

u32 g_idFontOSDWarnings = 0;
u32 g_idFontStats = 0;
u32 g_idFontStatsSmall = 0;

u32 g_idFontMenu = 0;
u32 g_idFontMenuSmall = 0;
u32 g_idFontMenuLarge = 0;

// Exported to end user
extern u32 s_uRenderEngineUIFontIdSmall;
extern u32 s_uRenderEngineUIFontIdRegular;
extern u32 s_uRenderEngineUIFontIdBig;

#include "fonts.h"
#include "shared_vars.h"

u32 _getBestMatchingFontHeight(u32* pFontList, int iFontCount, float fPixelsHeight)
{
   if (NULL == pFontList )
      return 0;

   float hScreen = hdmi_get_current_resolution_height();

   for( int i=0; i<iFontCount-1; i++ )
   {
      float fFontHeightPx = g_pRenderEngine->getRawFontHeight(pFontList[i]) * hScreen;
      float fFontHeightPxNext = g_pRenderEngine->getRawFontHeight(pFontList[i+1]) * hScreen;

      if ( fPixelsHeight <= fFontHeightPx )
      {
         log_line("Best font match for %d pixels height: default font index %d, font id: %u (h: %d pixels)", (int)fPixelsHeight, i, pFontList[i], (int)fFontHeightPx);
         return pFontList[i];
      }
      if ( fPixelsHeight <= (fFontHeightPx + fFontHeightPxNext)/2.0 )
      {
         log_line("Best font match for %d pixels height: default font index %d, font id: %u (h: %d pixels)", (int)fPixelsHeight, i, pFontList[i], (int)fFontHeightPx);
         return pFontList[i];
      }
   }
   int iIndex = iFontCount/2;
   log_line("Best font match for %d pixels height: use a default font index: %d, font id: %u", (int)fPixelsHeight, iIndex, pFontList[iIndex]);
   return pFontList[iIndex];
}

bool _loadFontFamily(int iFamilyId, const char* szName, int iBold, u32* pOutputList, int* pCountOutput)
{
   if ( (NULL == szName) || (0 == szName[0]) || (NULL == pOutputList) || (NULL == pCountOutput) )
      return false;

   log_line("Loading font familiy %s ...", szName);

   *pCountOutput = 0;

   char szFontFile[128];
   for( int iSize = 10; iSize < 62; iSize += 2 )
   {
      sprintf(szFontFile, "res/font_%s_%d.dsc", szName, iSize);
      if ( access( szFontFile, R_OK ) == -1 )
         continue;

      int iResult = g_pRenderEngine->loadRawFont(iFamilyId, szFontFile, iBold);
      if ( iResult > 0 )
      {
         pOutputList[(*pCountOutput)] = (u32) iResult;
         *pCountOutput = (*pCountOutput) + 1;
      }
      else
         log_line("Can't load font %s, error code: %d", szFontFile, iResult);
   }
   log_line("Loaded %d sizes for font family %s", *pCountOutput, szName);
   return true;
}

bool loadAllFonts(bool bReloadMenuFonts)
{
   float hScreen = hdmi_get_current_resolution_height();
   log_line("Loading fonts for screen surface height: %d px (hdmi height: %d px), reload menu fonts: %s", g_pRenderEngine->getScreenHeight(), (int)hScreen, bReloadMenuFonts?"yes":"no");

   if ( s_bFontsInitialized )
   {
      if ( bReloadMenuFonts )
      {
         log_line("Cleaning up previous fonts for menus (%d fonts)...", s_iListMenuFontSizesCount);
         for( int i=0; i<s_iListMenuFontSizesCount; i++ )
            g_pRenderEngine->freeRawFont(s_ListMenuFontSizes[i]);
         s_iListMenuFontSizesCount = 0;
      }

      log_line("Cleaning up previous fonts for OSD (%d fonts)...", s_iListOSDFontSizesCount);
      for( int i=0; i<s_iListOSDFontSizesCount; i++ )
         g_pRenderEngine->freeRawFont(s_ListOSDFontSizes[i]);
      s_iListOSDFontSizesCount = 0;
   }

   s_bFontsInitialized = true;

   if ( bReloadMenuFonts )
   {
      log_line("Loading menu fonts...");
      _loadFontFamily(0, "raw_bold", false, s_ListMenuFontSizes, &s_iListMenuFontSizesCount );
   }

   Preferences* p = get_Preferences();
   char szFont[32];
   strcpy(szFont, "raw_bold");
   if ( p->iOSDFont == 0 )
      strcpy(szFont, "raw_bold");
   if ( p->iOSDFont == 1 )
      strcpy(szFont, "rawobold");
   if ( p->iOSDFont == 2 )
      strcpy(szFont, "ariobold");
   if ( p->iOSDFont == 3 )
      strcpy(szFont, "bt_bold");

    log_line("Loading OSD fonts...");
   _loadFontFamily(p->iOSDFont, szFont, p->iOSDFontBold, s_ListOSDFontSizes, &s_iListOSDFontSizesCount );

   for( int i=0; i<s_iListOSDFontSizesCount; i++ )
      g_pRenderEngine->setFontOutlineColor(s_ListOSDFontSizes[i], p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], p->iColorOSDOutline[3]);

   applyFontScaleChanges();

   log_line("Done loading fonts.");
   return true;
}

void free_all_fonts()
{
   log_line("Cleaning up previous fonts for menus (%d fonts)...", s_iListMenuFontSizesCount);

   for( int i=0; i<s_iListMenuFontSizesCount; i++ )
      g_pRenderEngine->freeRawFont(s_ListMenuFontSizes[i]);
   s_iListMenuFontSizesCount = 0;

   for( int i=0; i<s_iListOSDFontSizesCount; i++ )
      g_pRenderEngine->freeRawFont(s_ListOSDFontSizes[i]);
   s_iListOSDFontSizesCount = 0;
}

void _applyNewFontToExistingPopups(u32 uFontIdOld, u32 uFontIdNew)
{
   for( int i=0; i<popups_get_count(); i++ )
   {
      Popup* p = popups_get_at(i);
      if ( p && ( p->getFontId() == uFontIdOld) )
         p->setFont(uFontIdNew);

   }

   for( int i=0; i<popups_get_topmost_count(); i++ )
   {
      Popup* p = popups_get_topmost_at(i);
      if ( p && ( p->getFontId() == uFontIdOld) )
         p->setFont(uFontIdNew);
   }
}

void applyFontScaleChanges()
{
   log_line("Applying font scale updates...");
   Preferences* pP = get_Preferences();
   float hScreen = hdmi_get_current_resolution_height();

   // Percente of screen height
   float fMenuFontSize = 0.026;
   float fOSDFontSize = 0.024;
   float fOSDScale = osd_getScaleOSD();
   float fOSDStatsScale = osd_getScaleOSDStats();

   // Same scaling must be done in menu render width computation. Search for AABBCC marker
   if ( NULL != pP )
   {
      if ( pP->iScaleMenus < 0 )
         fMenuFontSize *= (1.0 + 0.15*pP->iScaleMenus);
      if ( pP->iScaleMenus > 0 )
         fMenuFontSize *= (1.0 + 0.1*pP->iScaleMenus);

      if ( pP->iMenuStyle == 1 )
         fMenuFontSize *= 1.2;
   }

   log_line("Applying menu font size of: %d pixels (screen height: %d pixels)", (int)(hScreen * fMenuFontSize), (int) hScreen);
   u32 uFontId = 0;
   uFontId = _getBestMatchingFontHeight(s_ListMenuFontSizes, s_iListMenuFontSizesCount, hScreen * fMenuFontSize );
   _applyNewFontToExistingPopups(g_idFontMenu, uFontId);
   log_line("Switching font menu from font id %u to font id %u", g_idFontMenu, uFontId);
   g_idFontMenu = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListMenuFontSizes, s_iListMenuFontSizesCount, hScreen * fMenuFontSize * 0.8 );
   _applyNewFontToExistingPopups(g_idFontMenuSmall, uFontId);
   log_line("Switching font menu small from font id %u to font id %u", g_idFontMenuSmall, uFontId);
   g_idFontMenuSmall = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListMenuFontSizes, s_iListMenuFontSizesCount, hScreen * fMenuFontSize * 1.24 );
   _applyNewFontToExistingPopups(g_idFontMenuLarge, uFontId);
   log_line("Switching font menu big from font id %u to font id %u", g_idFontMenuLarge, uFontId);
   g_idFontMenuLarge = uFontId;

   log_line("Applying OSD font size of: %d pixels (screen height: %d pixels)", (int)(hScreen * fOSDFontSize), (int) hScreen);

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDScale );
   _applyNewFontToExistingPopups(g_idFontOSD, uFontId);
   g_idFontOSD = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDScale * 1.25);
   _applyNewFontToExistingPopups(g_idFontOSDBig, uFontId);
   g_idFontOSDBig = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDScale * 0.74);
   _applyNewFontToExistingPopups(g_idFontOSDSmall, uFontId);
   g_idFontOSDSmall = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDScale * 0.5);
   _applyNewFontToExistingPopups(g_idFontOSDExtraSmall, uFontId);
   g_idFontOSDExtraSmall = uFontId;


   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDScale );
   _applyNewFontToExistingPopups(g_idFontOSDWarnings, uFontId);
   g_idFontOSDWarnings = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDStatsScale * 0.8 );
   _applyNewFontToExistingPopups(g_idFontStats, uFontId);
   g_idFontStats = uFontId;

   uFontId = _getBestMatchingFontHeight(s_ListOSDFontSizes, s_iListOSDFontSizesCount, hScreen * fOSDFontSize * fOSDStatsScale * 0.8 * 0.75);
   _applyNewFontToExistingPopups(g_idFontStatsSmall, uFontId);
   g_idFontStatsSmall = uFontId;

   s_uRenderEngineUIFontIdRegular = g_idFontOSD;
   s_uRenderEngineUIFontIdSmall = g_idFontOSDSmall;
   s_uRenderEngineUIFontIdBig = g_idFontOSDBig;

}