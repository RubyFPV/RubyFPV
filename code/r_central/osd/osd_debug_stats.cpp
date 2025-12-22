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

#include "../../base/base.h"
#include "../../base/utils.h"
#include "../../base/ctrl_preferences.h"
#include "../../base/ctrl_settings.h"
#include "../../common/string_utils.h"
#include <math.h>
#include "osd_debug_stats.h"
#include "osd_common.h"
#include "osd_stats.h"
#include "../colors.h"
#include "../shared_vars.h"
#include "../timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

bool s_bDebugStatsControllerInfoFreeze = false;
int s_iDebugStatsControllerInfoZoom = 0;
controller_runtime_info s_ControllerRTInfoFreeze;
controller_debug_video_runtime_info s_ControllerDebugRTInfoFreeze;

void osd_debug_stats_toggle_zoom(bool bIncrease)
{
   if ( bIncrease )
   {
      s_iDebugStatsControllerInfoZoom++;
      if ( s_iDebugStatsControllerInfoZoom > 3 )
         s_iDebugStatsControllerInfoZoom = 0;
   }
   else
   {
      if ( s_iDebugStatsControllerInfoZoom > 0 )
         s_iDebugStatsControllerInfoZoom--;
      else
         s_iDebugStatsControllerInfoZoom = 3;
   }
}

void osd_debug_stats_toggle_freeze()
{
   s_bDebugStatsControllerInfoFreeze = ! s_bDebugStatsControllerInfoFreeze;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
      memcpy(&s_ControllerRTInfoFreeze, &g_SMControllerRTInfo, sizeof(controller_runtime_info));
      memcpy(&s_ControllerDebugRTInfoFreeze, &g_SMControllerDebugVideoRTInfo, sizeof(controller_debug_video_runtime_info));
   }
}


float _osd_render_debug_stats_min_max_graph_lines(float xPos, float yPos, float hGraph, float fWidth, int* pValuesMin, int* pValuesMax, int* pValuesLine1, int* pValuesLine2, int iCountValues, int iCurrentIndex, bool bZeroBased)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);
   float hPixel = g_pRenderEngine->getPixelHeight();

   int iMax = -1000;
   int iMin = 1000;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;

      if ( NULL != pValuesMin )
      if ( (pValuesMin[i] < 1000) && (pValuesMin[i] > -1000) )
      if ( pValuesMin[i] > iMax )
         iMax = pValuesMin[i];

      if ( NULL != pValuesMax )
      if ( (pValuesMax[i] < 1000) && (pValuesMax[i] > -1000) )
      if ( pValuesMax[i] > iMax )
         iMax = pValuesMax[i];

      if ( NULL != pValuesLine1 )
      if ( (pValuesLine1[i] < 1000) && (pValuesLine1[i] > -1000) )
      if ( pValuesLine1[i] > iMax )
         iMax = pValuesLine1[i];

      if ( NULL != pValuesLine2 )
      if ( (pValuesLine2[i] < 1000) && (pValuesLine2[i] > -1000) )
      if ( pValuesLine2[i] > iMax )
         iMax = pValuesLine2[i];

      if ( NULL != pValuesMin )
      if ( (pValuesMin[i] < 1000) && (pValuesMin[i] > -1000) )
      if ( pValuesMin[i] < iMin )
         iMin = pValuesMin[i];

      if ( NULL != pValuesMax )
      if ( (pValuesMax[i] < 1000) && (pValuesMax[i] > -1000) )
      if ( pValuesMax[i] < iMin )
         iMin = pValuesMax[i];

      if ( NULL != pValuesLine1 )
      if ( (pValuesLine1[i] < 1000) && (pValuesLine1[i] > -1000) )
      if ( pValuesLine1[i] < iMin )
         iMin = pValuesLine1[i];

      if ( NULL != pValuesLine2 )
      if ( (pValuesLine2[i] < 1000) && (pValuesLine2[i] > -1000) )
      if ( pValuesLine2[i] < iMin )
         iMin = pValuesLine2[i];
   }
   
   if ( bZeroBased )
   if ( iMin > 0 )
      iMin = 0;

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   sprintf(szBuff, "%d", iMin);
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", (iMax+iMin)/2);
   g_pRenderEngine->drawText(xPos, yPos +hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;

   if ( bZeroBased )
   {
      g_pRenderEngine->setStroke(250,250,250,0.8);
      g_pRenderEngine->drawLine(xPos, yPos + hGraph, xPos + fWidth, yPos+hGraph);
   }

   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;
   float hPrevPoint = 0.0;
   float hPrevPointLine1 = 0.0;
   float hPrevPointLine2 = 0.0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      float hPoint = 0.0;
      float hPoint2 = 0.0;
      if ( NULL != pValuesMin )
      {
         if ( (pValuesMin[i] < 1000) && (pValuesMin[i] > -1000) )
         {
            hPoint = hGraph * (float)(pValuesMin[i]-(float)iMin) / ((float)iMax-(float)iMin);
            hPrevPoint = hPoint;
         }
         else
            hPoint = hPrevPoint;

         g_pRenderEngine->setStroke(100, 250, 100, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(100, 250, 100, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint - hPixel, xBar+fWidthBar, yPos + hGraph - hPoint - hPixel);

         if ( NULL != pValuesMax )
         if ( (pValuesMax[i] < 1000) && (pValuesMax[i] > -1000) )
         {
            hPoint2 = hGraph * (float)(pValuesMax[i]-(float)iMin) / ((float)iMax-(float)iMin);
            g_pRenderEngine->setStroke(100, 150, 100, OSD_STRIKE_WIDTH);
            g_pRenderEngine->setFill(100, 150, 100, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBar, yPos + hGraph - hPoint2, fWidthBar, (hPoint2-hPoint - hPixel));
         }
      }

      if ( NULL != pValuesLine1 )
      {
         if ( (pValuesLine1[i] < 1000) && (pValuesLine1[i] > -1000) )
         {
            hPoint = hGraph * (float)(pValuesLine1[i]-(float)iMin) / ((float)iMax-(float)iMin);
            hPrevPointLine1 = hPoint;
         }
         else
            hPoint = hPrevPointLine1;
         g_pRenderEngine->setStroke(250, 250, 100, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(250, 250, 100, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint - hPixel, xBar+fWidthBar, yPos + hGraph - hPoint - hPixel);
      }

      if ( NULL != pValuesLine2 )
      {
         if ( (pValuesLine2[i] < 1000) && (pValuesLine2[i] > -1000) )
         {
            hPoint = hGraph * (float)(pValuesLine2[i]-(float)iMin) / ((float)iMax-(float)iMin);
            hPrevPointLine2 = hPoint;
         }
         else
            hPoint = hPrevPointLine2;
         g_pRenderEngine->setStroke(250, 100, 100, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(250, 100, 100, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint - hPixel, xBar+fWidthBar, yPos + hGraph - hPoint - hPixel);
      }

      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}


float _osd_render_debug_stats_graph_lines(float xPos, float yPos, float hGraph, float fWidth, int* pValues, int* pValuesMin, int* pValuesMax, int* pValuesAvg, int iCountValues, int iCurrentIndex)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   int iMax = -1000;
   int iMin = 1000;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;
      if ( NULL != pValues )
      if ( (pValues[i] < 1000) && (pValues[i] > -1000) )
      if ( pValues[i] > iMax )
         iMax = pValues[i];

      if ( NULL != pValuesMax )
      if ( (pValuesMax[i] < 1000) && (pValuesMax[i] > -1000) )
      if ( pValuesMax[i] > iMax )
         iMax = pValuesMax[i];

      if ( NULL != pValues )
      if ( (pValues[i] < 1000) && (pValues[i] > -1000) )
      if ( pValues[i] < iMin )
         iMin = pValues[i];

      if ( NULL != pValuesMin )
      if ( (pValuesMin[i] < 1000) && (pValuesMin[i] > -1000) )
      if ( pValuesMin[i] < iMin )
         iMin = pValuesMin[i];
   }
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   sprintf(szBuff, "%d", iMin);
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", (iMax+iMin)/2);
   g_pRenderEngine->drawText(xPos, yPos +hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;

   g_pRenderEngine->setStroke(250,250,250,0.5);
   for( float xx=xPos; xx<xPos+fWidth; xx += 0.02 )
   {
      g_pRenderEngine->drawLine(xx, yPos, xx + 0.008, yPos);
      g_pRenderEngine->drawLine(xx, yPos + hGraph*0.5, xx + 0.008, yPos+hGraph*0.5);
   }

   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;
   float hPoint = 0;
   float hPointPrev = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      if ( NULL != pValuesMin )
      if ( pValuesMin[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesMin[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( (i > 0) && (i != (iCurrentIndex-1)) )
         if ( (pValuesMin[i-1] < 1000) && (pValuesMin[i-1] > -1000) )
         {
            hPointPrev = hGraph * (float)(pValuesMin[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
         }
      }
      
      if ( NULL != pValuesMin )
      if ( pValuesMax[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesMax[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( (i > 0) && (i != (iCurrentIndex-1)) )
         if ( (pValuesMax[i-1] < 1000) && (pValuesMax[i-1] > -1000) )
         {
            hPointPrev = hGraph * (float)(pValuesMax[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
         }
      }

      if ( NULL != pValuesAvg )
      if ( pValuesAvg[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesAvg[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(100, 100, 250, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(100, 100, 250, s_fOSDStatsGraphLinesAlpha);
         //g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( (i > 0) && (i != (iCurrentIndex-1)) )
         if ( (pValuesAvg[i-1] < 1000) && (pValuesAvg[i-1] > -1000) )
         {
            hPointPrev = hGraph * (float)(pValuesAvg[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            //g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
            g_pRenderEngine->drawLine(xBar-fWidthBar, yPos + hGraph - hPointPrev, xBar, yPos + hGraph - hPoint);
         }
      }

      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_graph_rxtx_bars(float xPos, float yPos, float hGraph, float fWidth, u8* pValuesRx1, u8* pValuesRx2, u8* pValuesRx3, u8* pValuesRxDelta, u8* pValuesTx1, u8* pValuesTx2, u8* pValuesTxDeltaMin, u8* pValuesTxDeltaMax, int iCountValues, int iCurrentIndex, bool bUseLogScale)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   float hGraphTop = hGraph * 0.84;
   float hGraphBottom = 1.2*(hGraph - hGraphTop);

   controller_runtime_info* pCRTInfo = &g_SMControllerRTInfo;
   controller_debug_video_runtime_info* pCDebugRTInfo = &g_SMControllerDebugVideoRTInfo;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
       pCRTInfo = &s_ControllerRTInfoFreeze;
       pCDebugRTInfo = &s_ControllerDebugRTInfoFreeze;
   }
   int iMaxValue = 0;
   int iMaxValueBottom = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;

      if ( pValuesRx1[i] > iMaxValue )
         iMaxValue = pValuesRx1[i];
      if ( NULL != pValuesRx2 )
      if ( (int)pValuesRx1[i] + (int)pValuesRx2[i] > iMaxValue )
         iMaxValue = (int)pValuesRx1[i] + (int)pValuesRx2[i];
      if ( NULL != pValuesRx3 )
      if ( (int)pValuesRx1[i] + (int)pValuesRx3[i] > iMaxValue )
         iMaxValue = (int)pValuesRx1[i] + (int)pValuesRx3[i];

      if ( (NULL != pValuesRx2) && (NULL != pValuesRx3) )
      if ( (int)pValuesRx1[i] + (int)pValuesRx2[i] + (int)pValuesRx3[i] > iMaxValue )
         iMaxValue = (int)pValuesRx1[i] + (int)pValuesRx2[i] + (int)pValuesRx3[i];

      if ( (NULL != pValuesTx1) && (pValuesTx1[i] > iMaxValueBottom) )
         iMaxValueBottom = pValuesTx1[i];
      if ( (NULL != pValuesTx2) && (pValuesTx2[i] > iMaxValueBottom) )
         iMaxValueBottom = pValuesTx2[i];

      if ( (NULL != pValuesTx1) && (NULL != pValuesTx2) )
      if ( (pValuesTx1[i] + pValuesTx2[i] > iMaxValueBottom) )
         iMaxValueBottom = pValuesTx1[i] + pValuesTx2[i];
   }

   float fMaxLog = logf((float)iMaxValue);
   float fMaxLogBottom = logf((float)iMaxValueBottom);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos + hGraphTop - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMaxValue);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMaxValue/2);
   g_pRenderEngine->drawText(xPos, yPos + hGraphTop*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   sprintf(szBuff, "%d", iMaxValueBottom);
   g_pRenderEngine->drawText(xPos, yPos + hGraphTop + hGraphBottom - height_text_small*0.3, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();
   float fHeightPixel = g_pRenderEngine->getPixelHeight();
   float fWidthBar = fWidth / (float)iCountValues;

   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos, yPos-g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos-g_pRenderEngine->getPixelHeight());
   g_pRenderEngine->drawLine(xPos, yPos+hGraphTop+g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos+hGraphTop+g_pRenderEngine->getPixelHeight());

   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      float hBar1 = hGraphTop * (float)pValuesRx1[i] / (float)iMaxValue;
      float hBar2 = 0;
      float hBar3 = 0;
      float hBarTx1 = 0;
      float hBarTx2 = 0;
      if ( NULL != pValuesRx2 )
         hBar2 = hGraphTop * (float)pValuesRx2[i] / (float)iMaxValue;
      if ( NULL != pValuesRx3 )
         hBar3 = hGraphTop * (float)pValuesRx3[i] / (float)iMaxValue;

      if ( NULL != pValuesTx1 )
         hBarTx1 = hGraphBottom * (float)pValuesTx1[i] / (float)iMaxValueBottom;
      if ( NULL != pValuesTx2 )
         hBarTx2 = hGraphBottom * (float)pValuesTx2[i] / (float)iMaxValueBottom;

      if ( hBarTx1 < hGraphBottom * 0.2 )
         hBarTx1 = hGraphBottom * 0.2;

      if ( hBarTx2 < hGraphBottom * 0.2 )
         hBarTx2 = hGraphBottom * 0.2;

      if ( bUseLogScale )
      {
         float fAllSum = pValuesRx1[i];
         if ( NULL != pValuesRx2 )
            fAllSum += pValuesRx2[i];
         if ( NULL != pValuesRx3 )
            fAllSum += pValuesRx3[i];

         if ( pValuesRx1[i] > 0 )
            hBar1 = hGraphTop * logf((float)pValuesRx1[i]) / fMaxLog;

         if ( (NULL != pValuesRx2) && (pValuesRx2[i] > 0) )
            hBar2 = hGraphTop * logf(fAllSum) / fMaxLog - hBar1;

         if ( (NULL != pValuesRx3) && (pValuesRx3[i] > 0) )
            hBar3 = hGraphTop * logf(fAllSum) / fMaxLog - hBar1;

         if ( (NULL != pValuesRx2) && (pValuesRx2[i] > 0) )
         if ( (NULL != pValuesRx3) && (pValuesRx3[i] > 0) )
         {
            float hBar23 = hGraphTop * logf(fAllSum) / fMaxLog - hBar1;
            hBar2 = hBar23 * pValuesRx2[i]/(pValuesRx2[i] + pValuesRx3[i]);
            hBar3 = hBar23 - hBar2;
         }
      }
      else
      {
         if ( (NULL != pValuesRx2) && (pValuesRx2[i] > 0) )
         if ( hBar2 < 0.15 * hGraphTop )
            hBar2 = 0.15 * hGraphTop;
         if ( (NULL != pValuesRx3) && (pValuesRx3[i] > 0) )
         if ( hBar3 < 0.2 * hGraphTop )
            hBar3 = 0.2 * hGraphTop;
      }

      float yBar = yPos + hGraphTop;
      //float w = fWidthBar * (float)(pValuesRxDelta[i]) / (float) pCRTInfo->uUpdateIntervalMs;
      float w = fWidthBar - fWidthPixel*2.0;
      if ( w < fWidthBar * 0.2 )
         w = fWidthBar * 0.2;
      if ( w < fWidthPixel * 1.01 )
         w = fWidthPixel* 1.01;

      if ( pValuesRx1[i] > 0 )
      {
         yBar -= hBar1;
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, w, hBar1);
      }
      if ( (NULL != pValuesRx2) && (pValuesRx2[i] > 0) )
      {
         yBar -= hBar2;
         g_pRenderEngine->setStroke(0, 200, 0, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(0, 200, 0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, w, hBar2);
      }

      if ( (NULL != pValuesRx3) && (pValuesRx3[i] > 0) )
      {
         yBar -= hBar3;
         g_pRenderEngine->setStroke(90, 80, 255, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(90, 80, 255, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, w, hBar3);
      }

      if ( pValuesRx1[i] == 0 )
      if ( (NULL == pValuesRx2) || (0 == pValuesRx2[i]) )
      if ( (NULL == pValuesRx3) || (0 == pValuesRx3[i]) )
      {
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         float fWBox = fWidthPixel;
         float fHBox = fHeightPixel;
         if ( s_iDebugStatsControllerInfoZoom > 1 )
         {
            fWBox *= 2;
            fHBox *= 2;
         }
         g_pRenderEngine->drawRect(xBar + fWidthBar*0.5 - fWBox, yPos + 0.5*hGraphTop - fHBox, 2.0*fWBox, 2.0*fHBox);
      }

      yBar = yPos + hGraphTop + 3.0 * fHeightPixel;
      //w = fWidthBar * (float)(pValuesTxDeltaMax[i]) / (float) pCRTInfo->uUpdateIntervalMs;
      w = fWidthBar - fWidthPixel*2.0;
      if ( w < fWidthBar * 0.2 )
         w = fWidthBar * 0.2;
      if ( w < fWidthPixel * 1.01 )
         w = fWidthPixel* 1.01;

      if ( (NULL != pValuesTx1) && (pValuesTx1[i] > 0) )
      {
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, w, hBarTx1);
         yBar += hBarTx1;
      }

      if ( (NULL != pValuesTx2) && (pValuesTx2[i] > 0) )
      {
         g_pRenderEngine->setStroke(90, 80, 255, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(90, 80, 255, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, w, hBarTx2);
         yBar += hBarTx2;
      }

      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}


float _osd_render_debug_stats_graph_bars(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, u8* pValues2, u8* pValues3, int iCountValues, int iCurrentIndex, bool bUseLogScale)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   int iMaxValue = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;
      if ( pValues[i] > iMaxValue )
         iMaxValue = pValues[i];
      if ( NULL != pValues2 )
      if ( (int)pValues[i] + (int)pValues2[i] > iMaxValue )
         iMaxValue = (int)pValues[i] + (int)pValues2[i];
      if ( NULL != pValues3 )
      if ( (int)pValues[i] + (int)pValues3[i] > iMaxValue )
         iMaxValue = (int)pValues[i] + (int)pValues3[i];

      if ( NULL != pValues2 )
      if ( NULL != pValues3 )
      if ( (int)pValues[i] + (int)pValues2[i] + (int)pValues3[i] > iMaxValue )
         iMaxValue = (int)pValues[i] + (int)pValues2[i] + (int)pValues3[i];
   }
   float fMaxLog = logf((float)iMaxValue);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMaxValue);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMaxValue/2);
   g_pRenderEngine->drawText(xPos, yPos +hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();
   float fHeightPixel = g_pRenderEngine->getPixelHeight();
   float fWidthBar = fWidth / iCountValues;

   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos, yPos-g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos-g_pRenderEngine->getPixelHeight());
   g_pRenderEngine->drawLine(xPos, yPos+hGraph+g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos+hGraph+g_pRenderEngine->getPixelHeight());

   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      float hBar1 = hGraph * (float)pValues[i] / (float)iMaxValue;
      float hBar2 = 0;
      float hBar3 = 0;
      if ( NULL != pValues2 )
         hBar2 = hGraph * (float)pValues2[i] / (float)iMaxValue;
      if ( NULL != pValues3 )
         hBar3 = hGraph * (float)pValues3[i] / (float)iMaxValue;

      if ( bUseLogScale )
      {
         float fAllSum = pValues[i];
         if ( NULL != pValues2 )
            fAllSum += pValues2[i];
         if ( NULL != pValues3 )
            fAllSum += pValues3[i];

         if ( pValues[i] > 0 )
            hBar1 = hGraph * logf((float)pValues[i]) / fMaxLog;

         if ( NULL != pValues2 )
         if ( pValues2[i] > 0 )
            hBar2 = hGraph * logf(fAllSum) / fMaxLog - hBar1;

         if ( NULL != pValues3 )
         if ( pValues3[i] > 0 )
            hBar3 = hGraph * logf(fAllSum) / fMaxLog - hBar1;

         if ( NULL != pValues2 )
         if ( pValues2[i] > 0 )
         if ( NULL != pValues3 )
         if ( pValues3[i] > 0 )
         {
            float hBar23 = hGraph * logf(fAllSum) / fMaxLog - hBar1;
            hBar2 = hBar23 * pValues2[i]/(pValues2[i] + pValues3[i]);
            hBar3 = hBar23 - hBar2;
         }
      }

      float yBar = yPos + hGraph;
      if ( pValues[i] > 0 )
      {
         yBar -= hBar1;
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar1);
      }
      if ( NULL != pValues2 )
      if ( pValues2[i] > 0 )
      {
         yBar -= hBar2;
         g_pRenderEngine->setStroke(0, 200, 0, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(0, 200, 0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar2);
      }

      if ( NULL != pValues3 )
      if ( pValues3[i] > 0 )
      {
         yBar -= hBar3;
         g_pRenderEngine->setStroke(0, 0, 250, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(0, 0, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar3);
      }

      if ( pValues[i] == 0 )
      if ( (NULL == pValues2) || (0 == pValues2[i]) )
      if ( (NULL == pValues3) || (0 == pValues3[i]) )
      {
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         float fWBox = fWidthPixel;
         float fHBox = fHeightPixel;
         if ( s_iDebugStatsControllerInfoZoom > 1 )
         {
            fWBox *= 2;
            fHBox *= 2;
         }
         g_pRenderEngine->drawRect(xBar + fWidthBar*0.5 - fWBox, yPos + 0.5*hGraph - fHBox, 2.0*fWBox, 2.0*fHBox);
      }
      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_output_video(float xPos, float yPos, float hGraph, float fWidth, u32* pValues, u8* pValuesSkipped, int iCountValues, int iCurrentIndex, double* pColorP, double* pColorI, double* pColorO)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);
   float fWidthPixel = g_pRenderEngine->getPixelWidth();
   float fHeightPixel = g_pRenderEngine->getPixelHeight();

   controller_runtime_info* pCRTInfo = &g_SMControllerRTInfo;
   controller_debug_video_runtime_info* pCDebugRTInfo = &g_SMControllerDebugVideoRTInfo;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
       pCRTInfo = &s_ControllerRTInfoFreeze;
       pCDebugRTInfo = &s_ControllerDebugRTInfoFreeze;
   }
   int iMaxValue = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;
      u32 uBytes = (pValues[i] >> 16);
      if ( uBytes > iMaxValue )
         iMaxValue = uBytes;
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMaxValue);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;
   float fSpacingX = g_pRenderEngine->getPixelWidth()*2.0;
   if ( fSpacingX > fWidthBar * 0.4 )
      fSpacingX = fWidthBar * 0.4;

   bool bPrevValueWasEndOfFrame = false;
   bool bValueIsEndOfFrame = false;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      u32 uDeltaMS = pValues[i] & 0xFF;
      u32 uBytes = (pValues[i] >> 16);
      u32 uNALFlags = (pValues[i] & 0x0000FF00);
      if ( 0 == uBytes )
      {
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         float fWBox = fWidthPixel;
         float fHBox = fHeightPixel;
         if ( s_iDebugStatsControllerInfoZoom > 1 )
         {
            fWBox *= 2;
            fHBox *= 2;
         }
         g_pRenderEngine->drawRect(xBar + fWidthBar*0.5 - fWBox, yPos + 0.5*hGraph - fHBox, 2.0*fWBox, 2.0*fHBox);

         xBar += fWidthBar;
         continue;
      }

      bPrevValueWasEndOfFrame = false;
      bValueIsEndOfFrame = false;
      if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_NAL_END )
         bValueIsEndOfFrame = true;

      if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_NAL_P )
      {
         g_pRenderEngine->setStroke(pColorP[0], pColorP[1], pColorP[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColorP[0], pColorP[1], pColorP[2], s_fOSDStatsGraphLinesAlpha);
      }
      if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_NAL_I )
      {
         g_pRenderEngine->setStroke(pColorI[0], pColorI[1], pColorI[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColorI[0], pColorI[1], pColorI[2], s_fOSDStatsGraphLinesAlpha);
      }

      float hBar = hGraph * (float)uBytes / (float)iMaxValue;
      if ( hBar < 0.1 * hGraph )
         hBar = 0.1 * hGraph;
      float dyBar = 0.0;
      float x = xBar;
      float w = fWidthBar * (float)uDeltaMS/(float)pCRTInfo->uUpdateIntervalMs;
      if ( w < fWidthBar * 0.2 )
         w = fWidthBar * 0.2;
      if ( w < fWidthPixel * 1.01 )
         w = fWidthPixel * 1.01;
      if ( bPrevValueWasEndOfFrame )
      {
         x += fSpacingX;
         w -= fSpacingX;
      }
      if ( bValueIsEndOfFrame )
         w -= fSpacingX;


      if ( (NULL != pValuesSkipped) && (pValuesSkipped[i] != 0) )
      {
         g_pRenderEngine->setStroke(255,50,50, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(255,50,50, s_fOSDStatsGraphLinesAlpha*0.2 + 0.8);
         g_pRenderEngine->drawRect(xBar-2.0*fWidthPixel, yPos-3.0*fHeightPixel, fWidthBar + 4.0*fWidthPixel, hGraph + 6.0 * fHeightPixel);
      }
      else
      {
         g_pRenderEngine->drawRect(x, yPos + hGraph - hBar - dyBar, w, hBar);
         if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME )
         {
            g_pRenderEngine->setStroke(pColorO[0], pColorO[1], pColorO[2], OSD_STRIKE_WIDTH);
            float dx = fWidthBar * (float)uDeltaMS/(float)pCRTInfo->uUpdateIntervalMs;
            g_pRenderEngine->drawLine(xBar + dx, yPos, xBar + dx, yPos+hGraph);
         }
      }
      bPrevValueWasEndOfFrame = bValueIsEndOfFrame;
      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}


float _osd_render_debug_stats_graph_values_c(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, int iCountValues, int iCurrentIndex, double* pColor1, double* pColor2, double* pColor3, double* pColor4, double* pColor5)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   double cRed[] = {220, 0, 0, 1};
   double cRose[] = {250, 130, 130, 1};
   double cBlue[] = {80, 80, 250, 1};
   double cYellow[] = {250, 250, 50, 1};
   double cWhite[] = {250, 250, 250, 1};

   if ( NULL == pColor1 )
      pColor1 = &cRed[0];
   if ( NULL == pColor2 )
      pColor2 = &cRose[0];
   if ( NULL == pColor3 )
      pColor3 = &cBlue[0];
   if ( NULL == pColor4 )
      pColor4 = &cYellow[0];
   if ( NULL == pColor5 )
      pColor5 = &cWhite[0];

   int iMax = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
         continue;
      if ( pValues[i] > iMax )
         iMax = pValues[i];
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;

   for( int i=0; i<iCountValues; i++ )
   {
      if ( i == iCurrentIndex )
      {
         xBar += fWidthBar;
         continue;
      }

      float hBar = hGraph*0.5;
      float dyBar = 0.0;
      if ( pValues[i] == 0 )
      {
         xBar += fWidthBar;
         continue;
      }
      if ( pValues[i] == 1 )
      {
         g_pRenderEngine->setStroke(pColor1[0], pColor1[1], pColor1[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor1[0], pColor1[1], pColor1[2], s_fOSDStatsGraphLinesAlpha);
         hBar = hGraph * 0.7;
      }
      else if ( pValues[i] == 2 )
      {
         g_pRenderEngine->setStroke(pColor2[0], pColor2[1], pColor2[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor2[0], pColor2[1], pColor2[2], s_fOSDStatsGraphLinesAlpha);
         dyBar = 0.5*hGraph;
      }
      else if ( pValues[i] == 3 )
      {
         g_pRenderEngine->setStroke(pColor3[0], pColor3[1], pColor3[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor3[0], pColor3[1], pColor3[2], s_fOSDStatsGraphLinesAlpha);
         dyBar = 0.5*hGraph;
      }
      else if ( pValues[i] == 4 )
      {
         hBar = hGraph;
         g_pRenderEngine->setStroke(pColor4[0], pColor4[1], pColor4[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor4[0], pColor4[1], pColor4[2], s_fOSDStatsGraphLinesAlpha);
      }
      else if ( pValues[i] > 4 )
      {
         hBar = hGraph;
         g_pRenderEngine->setStroke(pColor5[0], pColor5[1], pColor5[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor5[0], pColor5[1], pColor5[2], s_fOSDStatsGraphLinesAlpha);
      }
      g_pRenderEngine->drawRect(xBar, yPos + hGraph - hBar - dyBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar);
      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_graph_values(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, int iCountValues, int iCurrentIndex)
{
   double cRed[] = {220, 0, 0, 1};
   double cRose[] = {250, 130, 130, 1};
   double cBlue[] = {80, 80, 250, 1};
   double cYellow[] = {250, 250, 50, 1};
   double cWhite[] = {250, 250, 250, 1};
   return _osd_render_debug_stats_graph_values_c(xPos, yPos, hGraph, fWidth, pValues, iCountValues, iCurrentIndex, cRed, cRose, cBlue, cYellow, cWhite);
}


float _osd_render_ack_time_hist(controller_runtime_info_vehicle* pRTInfoVehicle, float xPos, float fGraphXStart, float yPos, float hGraph, float fWidthGraph )
{
   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

   for( int iLink=0; iLink<g_SM_RadioStats.countLocalRadioLinks; iLink++ )
   {
      int iMaxValue = pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iLink]][iLink];
      int iMinValue = 1000;
      int iAvgValueSum = 0;
      int iAvgCount = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/4; i++ )
      {
         if ( pRTInfoVehicle->uAckTimes[i][iLink] > iMaxValue )
            iMaxValue = pRTInfoVehicle->uAckTimes[i][iLink];
         if ( pRTInfoVehicle->uAckTimes[i][iLink] != 0 )
         {
            iAvgValueSum += pRTInfoVehicle->uAckTimes[i][iLink];
            iAvgCount++;
            if ( pRTInfoVehicle->uAckTimes[i][iLink] < iMinValue )
               iMinValue = pRTInfoVehicle->uAckTimes[i][iLink];
         }
      }

      int iAvgValue = 0;
      if ( iAvgCount > 0 )
         iAvgValue = iAvgValueSum / iAvgCount;
      sprintf(szBuff, "RadioLink-%d Ack Time History: Min: %d ms, Max: %d ms, Avg: %d ms",
         iLink+1, iMinValue, iMaxValue, iAvgValue);
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
      yPos += height_text*1.3;

      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      osd_set_colors();
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
      g_pRenderEngine->drawText(xPos + fWidthGraph, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
      sprintf(szBuff, "%d", iMaxValue);
      g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos + fWidthGraph, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iMaxValue/2);
      g_pRenderEngine->drawText(xPos, yPos+hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos+fWidthGraph, yPos+hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

      float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
      xPos += dx;
      fWidthGraph -= dx;
      float xBar = xPos;
      float fWidthBar = fWidthGraph / (SYSTEM_RT_INFO_INTERVALS/4);

      g_pRenderEngine->setStroke(250,250,250,0.5);
      for( float xx=xPos; xx<xPos+fWidthGraph; xx += 0.02 )
      {
         g_pRenderEngine->drawLine(xx, yPos, xx + 0.008, yPos);
         g_pRenderEngine->drawLine(xx, yPos + hGraph*0.5, xx + 0.008, yPos+hGraph*0.5);
      }

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/4; i++ )
      {
         if ( i == (pRTInfoVehicle->iAckTimeIndex[iLink] + 1) )
            g_pRenderEngine->drawLine(xBar + 0.5*fWidthBar, yPos, xBar + 0.5*fWidthBar, yPos + hGraph);
         else
         {
            float hBar = hGraph * (float)pRTInfoVehicle->uAckTimes[i][iLink] / (float)iMaxValue;
            g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
            g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBar, yPos + hGraph - hBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar);
         }
         xBar += fWidthBar;
      }
      osd_set_colors();
      g_pRenderEngine->setColors(get_Color_Dev());
      yPos += hGraph;
      yPos += height_text_small;
   }
   return yPos;
}

void osd_render_debug_stats()
{
   Preferences* pP = get_Preferences();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   controller_runtime_info* pCRTInfo = &g_SMControllerRTInfo;
   controller_debug_video_runtime_info* pCDebugRTInfo = &g_SMControllerDebugVideoRTInfo;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
       pCRTInfo = &s_ControllerRTInfoFreeze;
       pCDebugRTInfo = &s_ControllerDebugRTInfoFreeze;
   }
   controller_runtime_info_vehicle* pCRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);

   // End interval value is not included (up to)
   int iInterval1Start = 0;
   int iInterval1End = pCRTInfo->iCurrentIndex;
   int iInterval2Start = pCRTInfo->iCurrentIndex+1;
   int iInterval2End = SYSTEM_RT_INFO_INTERVALS;
   int iCountIntervals = SYSTEM_RT_INFO_INTERVALS;
   int iNormalizedIndex = pCRTInfo->iCurrentIndex;
   if ( 1 == s_iDebugStatsControllerInfoZoom )
   {
      int iSlice = SYSTEM_RT_INFO_INTERVALS/2;
      iCountIntervals = iSlice;
      // 0 ........ 200 ........ 400

      iInterval1Start = iSlice * (int)(pCRTInfo->iCurrentIndex / iSlice);
      iInterval1End = pCRTInfo->iCurrentIndex;
      iInterval2Start = pCRTInfo->iCurrentIndex + 1;
      iInterval2End = iSlice * (1+(int)(pCRTInfo->iCurrentIndex / iSlice));
      
      iInterval2Start -= iSlice;
      iInterval2End -= iSlice;
      if ( pCRTInfo->iCurrentIndex < iSlice )
      {
         iInterval2Start = iInterval2Start + SYSTEM_RT_INFO_INTERVALS;
         iInterval2End = iInterval2End + SYSTEM_RT_INFO_INTERVALS;
      }

      iNormalizedIndex = pCRTInfo->iCurrentIndex % iSlice;
   }
   if ( 2 == s_iDebugStatsControllerInfoZoom )
   {
      int iSlice = SYSTEM_RT_INFO_INTERVALS/4;
      iCountIntervals = iSlice;
      // 0 ...... 100 ........ 200 ........ 300 ........ 400

      iInterval1Start = iSlice * (int)(pCRTInfo->iCurrentIndex / iSlice);
      iInterval1End = pCRTInfo->iCurrentIndex;
      iInterval2Start = pCRTInfo->iCurrentIndex + 1;
      iInterval2End = iSlice * (1+(int)(pCRTInfo->iCurrentIndex / iSlice));
      
      iInterval2Start -= iSlice;
      iInterval2End -= iSlice;
      if ( pCRTInfo->iCurrentIndex < iSlice )
      {
         iInterval2Start = iInterval2Start + SYSTEM_RT_INFO_INTERVALS;
         iInterval2End = iInterval2End + SYSTEM_RT_INFO_INTERVALS;
      }
      iNormalizedIndex = pCRTInfo->iCurrentIndex % iSlice;
   }
   if ( 3 == s_iDebugStatsControllerInfoZoom )
   {
      int iSlice = SYSTEM_RT_INFO_INTERVALS/8;
      iCountIntervals = iSlice;
      // 0 ...50... 100 ....150.... 200 ...250..... 300 ...350..... 400

      iInterval1Start = iSlice * (int)(pCRTInfo->iCurrentIndex / iSlice);
      iInterval1End = pCRTInfo->iCurrentIndex;
      iInterval2Start = pCRTInfo->iCurrentIndex + 1;
      iInterval2End = iSlice * (1+(int)(pCRTInfo->iCurrentIndex / iSlice));
      
      iInterval2Start -= iSlice;
      iInterval2End -= iSlice;
      if ( pCRTInfo->iCurrentIndex < iSlice )
      {
         iInterval2Start = iInterval2Start + SYSTEM_RT_INFO_INTERVALS;
         iInterval2End = iInterval2End + SYSTEM_RT_INFO_INTERVALS;
      }
      iNormalizedIndex = pCRTInfo->iCurrentIndex % iSlice;
   }

   float fAverageSliceUpdateTime = 0.0;
   float fMaxSliceUpdateTime = 0.0;
   int iIndex = 0;
   for( int i=iInterval1Start; i<iInterval1End; i++ )
   {
      float fUpdateTime = (float)(pCRTInfo->uSliceDurationMs[i]);
      fAverageSliceUpdateTime += fUpdateTime;
      if ( fUpdateTime > fMaxSliceUpdateTime )
         fMaxSliceUpdateTime = fUpdateTime;
      iIndex++;
   }
   for( int i=iInterval2Start; i<iInterval2End; i++ )
   {
      float fUpdateTime = (float)(pCRTInfo->uSliceDurationMs[i]);
      fAverageSliceUpdateTime += fUpdateTime;
      if ( fUpdateTime > fMaxSliceUpdateTime )
         fMaxSliceUpdateTime = fUpdateTime;
      iIndex++;
   }
   fAverageSliceUpdateTime = fAverageSliceUpdateTime/(float)iIndex;

   s_idFontStats = g_idFontStats;
   s_idFontStatsSmall = g_idFontStatsSmall;
   s_OSDStatsLineSpacing = 1.0;

   char szBuff[128];
   char szTitle[128];
   int iCountGraphs = 0;

   double cWhite[] = {255,255,255,1.0};
   double cGreen[] = {0,200,0,1.0};
   double cBlue[] = {50,50,255, 1.0};

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 3.0;
   float hGraphSmall = height_text * 1.6;
   float hGraphXSmall = height_text * 1.2;
   float fHPixel = g_pRenderEngine->getPixelHeight();
   float fWPixel = g_pRenderEngine->getPixelWidth();

   float xPos = 0.0;
   float width = 1.0;

   // Height and position gets computed each time for next render
   static float s_fStaticYPosDebugStats = 0.7;
   static float s_fStaticHeightDebugStats = 0.2;
   static float s_fStaticHeightDebugStatsPart2 = 0.0;
   static float s_fStaticHeightGraph = height_text * 3.0;
   static float s_fStaticHeightGraphSmall = height_text * 1.6;
   static float s_fStaticHeightGraphXSmall = height_text * 1.2;

   hGraph = s_fStaticHeightGraph;
   hGraphSmall = s_fStaticHeightGraphSmall;
   hGraphXSmall = s_fStaticHeightGraphXSmall;

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, s_fStaticYPosDebugStats, width, s_fStaticHeightDebugStats, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   float yPos = s_fStaticYPosDebugStats;
   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;

   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float height = s_fStaticHeightDebugStats;
   height -= s_fOSDStatsMargin*0.7*2.0;

   float widthMax = width;
   float rightMargin = xPos + width;

   float fGraphXStart = xPos;
   float fWidthGraph = widthMax;

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   float dxWithMs = g_pRenderEngine->textWidth(g_idFontStats, "000 ms");
   float xPosSlice = fGraphXStart + dx;
   float fWidthBar = (fWidthGraph-dx) / iCountIntervals;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();

   szBuff[0] = 0;
   if ( pP->iDebugStatsQAButton > 0 )
      sprintf(szBuff, "/[QA%d]", pP->iDebugStatsQAButton );
   snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "Debug Stats [Cancel]%s to exit stats, [Up]/[Down] to zoom, any other [QA] to freeze", szBuff);
   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szTitle);

   char szResolution[64];
   sprintf(szResolution, " ( %d ms/bar )", (int)pCRTInfo->uUpdateIntervalMs);
   
   //sprintf(szBuff, "%03d %03u ms", g_SMControllerRTInfo.iCurrentIndex, g_SMControllerRTInfo.uCurrentSliceStartTime%1000);
   sprintf(szBuff, "Avg: %.1f ms, Max: %.1f ms %s", fAverageSliceUpdateTime, fMaxSliceUpdateTime, szResolution);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);
   float y = yPos;
   y += 1.5*height_text*s_OSDStatsLineSpacing;

   float yTop = y;

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(250,250,250, OSD_STRIKE_WIDTH);

   for( float yLine=yTop; yLine<=yPos+height-s_fStaticHeightDebugStatsPart2-0.05; yLine += 0.05 )
   {
      for( float dxLine=0.0; dxLine<1.0; dxLine += 0.1 )
         g_pRenderEngine->drawLine(fGraphXStart+dx + fWidthGraph*dxLine, yLine, fGraphXStart+dx + fWidthGraph*dxLine, yLine + 0.03);
      g_pRenderEngine->setStroke(180,180,180, OSD_STRIKE_WIDTH);
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   for( float dxLine=0.0; dxLine<1.0; dxLine += 0.1 )
   {
      sprintf(szBuff, "%d ms", (int)(((float)((int)iCountIntervals*(int)pCRTInfo->uUpdateIntervalMs))*dxLine));
      g_pRenderEngine->drawText(fGraphXStart+dx + fWidthGraph*dxLine + 0.2*height_text_small, y, s_idFontStatsSmall, szBuff);
   }
   y += height_text*1.3;

   float fHStatsPart2 = s_fStaticHeightDebugStatsPart2;
   s_fStaticHeightDebugStatsPart2 = 0.0;

   u8 uValues1[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues2[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues3[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues4[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues5[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues6[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues7[SYSTEM_RT_INFO_INTERVALS];
   u8 uValues8[SYSTEM_RT_INFO_INTERVALS];
   u32 uValues32[SYSTEM_RT_INFO_INTERVALS];

   int iValues1[SYSTEM_RT_INFO_INTERVALS];
   int iValues2[SYSTEM_RT_INFO_INTERVALS];
   int iValues3[SYSTEM_RT_INFO_INTERVALS];
   int iValues4[SYSTEM_RT_INFO_INTERVALS];

   //--------------------------------------------
   /*
   int iVehicleIndex = iIndexVehicleStart;
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pVRTInfo->uSentVideoDataPackets[iVehicleIndex];
      uTmp2[i] = pVRTInfo->uSentVideoECPackets[iVehicleIndex];
      iVehicleIndex++;
      if ( iVehicleIndex >= SYSTEM_RT_INFO_INTERVALS )
         iVehicleIndex = 0;
   }

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Vehicle Tx Video Packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, uTmp2, NULL, iCountIntervals, 1);
   y += height_text_small;
   /**/

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS )
   {
      iIndex = 0;
      for( int i=iInterval1Start; i<iInterval1End; i++ )
      {
         uValues1[iIndex] = 0;
         uValues2[iIndex] = 0;
         uValues3[iIndex] = 0;
         uValues4[iIndex] = 0;

         for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
         {
            uValues1[iIndex] += pCRTInfo->uRxVideoPackets[i][k];
            uValues1[iIndex] += pCRTInfo->uRxVideoECPackets[i][k];
            uValues2[iIndex] += pCRTInfo->uRxDataPackets[i][k];
            uValues3[iIndex] += pCRTInfo->uRxHighPriorityPackets[i][k];
            if ( pCRTInfo->uRxLastDeltaTime[i][k] > uValues4[iIndex] )
               uValues4[iIndex] = pCRTInfo->uRxLastDeltaTime[i][k];

         }
         if ( uValues2[iIndex] > 0 )
         if ( uValues2[iIndex] < uValues1[iIndex]/5 )
            uValues2[iIndex] = uValues1[iIndex]/5;

         if ( uValues3[iIndex] > 0 )
         if ( uValues3[iIndex] < uValues1[iIndex]/5 )
            uValues3[iIndex] = uValues1[iIndex]/5;

         uValues5[iIndex] = pCRTInfo->uTxPackets[i];
         uValues6[iIndex] = pCRTInfo->uTxHighPriorityPackets[i];
         uValues7[iIndex] = pCRTInfo->uTxFirstDeltaTime[i];
         uValues8[iIndex] = pCRTInfo->uTxLastDeltaTime[i];
         iIndex++;
      }
      iIndex++;
      for( int i=iInterval2Start; i<iInterval2End; i++ )
      {
         uValues1[iIndex] = 0;
         uValues2[iIndex] = 0;
         uValues3[iIndex] = 0;
         uValues4[iIndex] = 0;

         for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
         {
            uValues1[iIndex] += pCRTInfo->uRxVideoPackets[i][k];
            uValues1[iIndex] += pCRTInfo->uRxVideoECPackets[i][k];
            uValues2[iIndex] += pCRTInfo->uRxDataPackets[i][k];
            uValues3[iIndex] += pCRTInfo->uRxHighPriorityPackets[i][k];
            if ( pCRTInfo->uRxLastDeltaTime[i][k] > uValues4[iIndex] )
               uValues4[iIndex] = pCRTInfo->uRxLastDeltaTime[i][k];
         }

         if ( uValues2[iIndex] > 0 )
         if ( uValues2[iIndex] < uValues1[iIndex]/5 )
            uValues2[iIndex] = uValues1[iIndex]/5;

         if ( uValues3[iIndex] > 0 )
         if ( uValues3[iIndex] < uValues1[iIndex]/5 )
            uValues3[iIndex] = uValues1[iIndex]/5;

         uValues5[iIndex] = pCRTInfo->uTxPackets[i];
         uValues6[iIndex] = pCRTInfo->uTxHighPriorityPackets[i];
         uValues7[iIndex] = pCRTInfo->uTxFirstDeltaTime[i];
         uValues8[iIndex] = pCRTInfo->uTxLastDeltaTime[i];
         iIndex++;
      }

      strcpy(szTitle, "Rx/Tx Video/Data/High Priority Packets");
      if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_ADV )
         strcat(szTitle, " (Clock Sync: Adv)");
      else if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_BASIC )
         strcat(szTitle, " (Clock Sync: Basic)");
      else
         strcat(szTitle, " (Clock Sync: None)");
      strcat(szTitle, szResolution);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_rxtx_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uValues1, uValues2, uValues3, uValues4, uValues5, uValues6, uValues7, uValues8, iCountIntervals, iNormalizedIndex, false);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS )
   {
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
      {
         iIndex = 0;
         for( int i=iInterval1Start; i<iInterval1End; i++ )
         {
            uValues1[iIndex++] = pCRTInfo->uRxMissingPackets[i][iInt];
         }
         iIndex++;
         for( int i=iInterval2Start; i<iInterval2End; i++ )
         {
            uValues1[iIndex++] = pCRTInfo->uRxMissingPackets[i][iInt];
         }
         sprintf(szBuff, "Rx Missing Packets (interface %d)", iInt+1);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text*1.3;
         y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphXSmall, fWidthGraph, uValues1, iCountIntervals, iNormalizedIndex);
         y += height_text_small;
      }
      iCountGraphs++;
   }

/*
   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uTxPackets[i+iStartIntervals];
         uTmp2[i] = 0;
         uTmp3[i] = pCRTInfo->uTxHighPriorityPackets[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Tx Regular/High Priority Packets");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, uTmp2, uTmp3, iCountIntervals, 1);
      y += height_text_small;
      iCountGraphs++;
   }
*/

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS )
   {
      iIndex = 0;
      for( int i=iInterval1Start; i<iInterval1End; i++ )
      {
         uValues1[iIndex++] = pCRTInfo->uRxMaxAirgapSlots[i];
      }
      iIndex++;
      for( int i=iInterval2Start; i<iInterval2End; i++ )
      {
         uValues1[iIndex++] = pCRTInfo->uRxMaxAirgapSlots[i];
      }

      strcpy(szTitle, "Rx Max air gaps");
      strcat(szTitle, szResolution);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;
      //y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uValues1, NULL, NULL, iCountIntervals, iNormalizedIndex, false);
      y += height_text_small;
      iCountGraphs++;

      /*
      iIndex = 0;
      for( int i=iInterval1Start; i<iInterval1End; i++ )
      {
         uValues1[iIndex++] = pCRTInfo->uRxMaxAirgapSlots2[i];
      }
      iIndex++;
      for( int i=iInterval2Start; i<iInterval2End; i++ )
      {
         uValues1[iIndex++] = pCRTInfo->uRxMaxAirgapSlots2[i];
      }

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Max air gaps (2)");
      y += height_text*1.3;
      //y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uValues1, NULL, NULL, iCountIntervals, iNormalizedIndex, false);
      y += height_text_small;
      iCountGraphs++;
      */
   }

/*
   //-----------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM )
   {
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
   for( int iAnt=0; iAnt<pCRTInfo->radioInterfacesDbm[0][iInt].iCountAntennas; iAnt++ )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         iTmp1[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmLast[iAnt];
         iTmp2[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmMin[iAnt];
         iTmp3[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmMax[iAnt];
         iTmp4[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmAvg[iAnt];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Dbm");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraph, fWidthGraph, iTmp1, iTmp2, iTmp3, iTmp4, iCountIntervals);
      y += height_text_small;
   }
   }
   iCountGraphs++;
   }
*/

   //-----------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR )
   {
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
      {
         iIndex = 0;
         for( int i=iInterval1Start; i<iInterval1End; i++ )
         {
            iValues1[iIndex] = pCRTInfo->radioInterfacesSignalInfoVideo[i][iInt].iSNRMin;
            iValues2[iIndex] = pCRTInfo->radioInterfacesSignalInfoVideo[i][iInt].iSNRMax;
            iValues4[iIndex] = getRadioMinimSNRForDataRate(pCRTInfo->iRecvVideoDataRate[i][iInt]);
            iValues3[iIndex] = getRadioMinimSNRForDataRate(pCRTInfo->iRecvVideoDataRate[i][iInt])+3;
            if ( 0 == iValues4[iIndex] )
            {
               iValues3[iIndex] = 1000;
               iValues4[iIndex] = 1000;
            }
            iIndex++;
         }
         iIndex++;
         for( int i=iInterval2Start; i<iInterval2End; i++ )
         {
            iValues1[iIndex] = pCRTInfo->radioInterfacesSignalInfoVideo[i][iInt].iSNRMin;
            iValues2[iIndex] = pCRTInfo->radioInterfacesSignalInfoVideo[i][iInt].iSNRMax;
            iValues4[iIndex] = getRadioMinimSNRForDataRate(pCRTInfo->iRecvVideoDataRate[i][iInt]);
            iValues3[iIndex] = getRadioMinimSNRForDataRate(pCRTInfo->iRecvVideoDataRate[i][iInt])+3;
            if ( 0 == iValues4[iIndex] )
            {
               iValues3[iIndex] = 1000;
               iValues4[iIndex] = 1000;
            }
            iIndex++;
         }


         sprintf(szBuff, "SNR (Interface %d)", iInt+1);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text*1.3;
         y += _osd_render_debug_stats_min_max_graph_lines(fGraphXStart, y, hGraph, fWidthGraph, iValues1, iValues2, iValues3, iValues4, iCountIntervals, iNormalizedIndex, true);
         y += height_text_small;
      }
      iCountGraphs++;
   }

   //-----------------------------------------------
   /*
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
   for( int iAnt=0; iAnt<pCRTInfo->radioInterfacesDbm[0][iInt].iCountAntennas; iAnt++ )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         iTmp2[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmChangeSpeedMin[iAnt];
         iTmp3[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmChangeSpeedMax[iAnt];
      }
      sprintf(szBuff, "Dbm Change Speed (interface %d, antenna: %d)", iInt+1, iAnt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Dbm");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraph, fWidthGraph, NULL, iTmp2, iTmp3, NULL, iCountIntervals);
      y += height_text_small;
   }
   }
   */

/*
   //--------------------------------------------------------------------------

   if ( iCountGraphs >=3 )
   {
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->setStroke(255,255,255, 1.0);
   xPosSlice = fGraphXStart + dx;
   for( int i=0; i<iCountIntervals; i++ )
   {
      xPosSlice += fWidthBar;
      g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5, y, xPosSlice + fWidthBar*0.5, y + height_text_small);
      if ( 0 != s_iDebugStatsControllerInfoZoom )
      {
         g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5 + fWidthPixel, y, xPosSlice + fWidthBar*0.5 + fWidthPixel, y + height_text_small);
         g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5 + fWidthPixel*2.0, y, xPosSlice + fWidthBar*0.5 + fWidthPixel*2.0, y + height_text_small);
      }
   }
   y += height_text_small;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP )
   {
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uRxMissingPacketsMaxGap[i+iStartIntervals][iInt];
      }
      sprintf(szBuff, "Rx Missing Packets Max Gap (interface %d)", iInt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
   }
   iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uRxProcessedPackets[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Consumed Packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraph, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 1);
   y += height_text_small;
   iCountGraphs++;
   }
*/

// ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED )
   {
      iIndex = 0;
      for( int i=iInterval1Start; i<iInterval1End; i++ )
      {
         if ( pCRTInfo->uOutputedVideoBlocksMaxECUsed[i] )
            uValues1[iIndex] = 1;
         else if( pCRTInfo->uOutputedVideoBlocksTwoECUsed[i] )
            uValues1[iIndex] = 2;
         else if( pCRTInfo->uOutputedVideoBlocksSingleECUsed[i] )
            uValues1[iIndex] = 3;
         iIndex++;
      }
      iIndex++;
      for( int i=iInterval2Start; i<iInterval2End; i++ )
      {
         if ( pCRTInfo->uOutputedVideoBlocksMaxECUsed[i] )
            uValues1[iIndex] = 1;
         else if( pCRTInfo->uOutputedVideoBlocksTwoECUsed[i] )
            uValues1[iIndex] = 2;
         else if( pCRTInfo->uOutputedVideoBlocksSingleECUsed[i] )
            uValues1[iIndex] = 3;
         iIndex++;
      }

      sprintf(szTitle, "Outputed video EC max packets used ( %d / %d )", pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iBlockDataPackets, pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iBlockECs);
      strcat(szTitle, szResolution);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uValues1, iCountIntervals, iNormalizedIndex);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES )
   {
      iIndex = 0;
      for( int i=iInterval1Start; i<iInterval1End; i++ )
      {
         uValues32[iIndex] = pCDebugRTInfo->uOutputFramesInfo[i];
         uValues1[iIndex] = pCRTInfo->uOutputedVideoBlocksSkippedBlocks[i];
         iIndex++;
      }
      iIndex++;
      for( int i=iInterval2Start; i<iInterval2End; i++ )
      {
         uValues32[iIndex] = pCDebugRTInfo->uOutputFramesInfo[i];
         uValues1[iIndex] = pCRTInfo->uOutputedVideoBlocksSkippedBlocks[i];
         iIndex++;
      }

      int iCurrentNAL = -1;
      int iIndexLastKF = -1;
      int iKeyframeMS = -1;

      iIndex = pCRTInfo->iCurrentIndex + 1;
      if ( iIndex >= SYSTEM_RT_INFO_INTERVALS )
         iIndex -= SYSTEM_RT_INFO_INTERVALS;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS-2; i++ )
      {
         u32 uNALFlags = (pCDebugRTInfo->uOutputFramesInfo[iIndex] & 0x0000FF00);
         int iNAL = iCurrentNAL;
         if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_NAL_I )
            iNAL = 1;
         if ( uNALFlags & VIDEO_STATUS_FLAGS2_IS_NAL_P )
            iNAL = 5;

         if ( iNAL != -1 )
         if ( iNAL != iCurrentNAL )
         {
            if ( (iNAL == 1) && (iCurrentNAL == 5) )
            {
               if ( iNAL == 1 )
               {
                  if ( iIndexLastKF != -1 )
                  {
                     iKeyframeMS = (i - iIndexLastKF) * pCRTInfo->uUpdateIntervalMs;
                     break;
                  }
                  iIndexLastKF = i;
               }
            }
            iCurrentNAL = iNAL;
         }
         iIndex++;
      }

      sprintf(szTitle, "Output Video Frames %s, detected KF interval: %d ms", szResolution, iKeyframeMS);

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;

      double cP[] = {140, 180, 250, 1};
      double cI[] = {90, 250, 40, 1};
      double cE[] = {80, 250, 80, 1};

      y += _osd_render_debug_stats_output_video(fGraphXStart, y, hGraphSmall, fWidthGraph, uValues32, uValues1, iCountIntervals, iNormalizedIndex, cP, cI, cE);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   /*
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPackets[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed clean video packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 1);
   y += height_text_small;
   */

   //--------------------------------------------
   /*
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoBlocksSingleECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with single EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;

   //--------------------------------------------
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoBlocksTwoECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with two EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;

   //--------------------------------------------
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoBlocksMultipleECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with more than two EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;
   */
/*
   // ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME )
   {   
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);
      for( int iLink=0; iLink<g_SM_RadioStats.countLocalRadioLinks; iLink++ )
      {
         xPosSlice = fGraphXStart + dx;
         for( int i=0; i<iCountIntervals; i++ )
         {
            iTmp1[i] = pRTInfoVehicle->uMinAckTime[i+iStartIntervals][iLink];  
            iTmp2[i] = pRTInfoVehicle->uMaxAckTime[i+iStartIntervals][iLink];
            if ( iTmp2[i] > 0 )
            {
               g_pRenderEngine->setColors(get_Color_OSDText(), 0.3);
               g_pRenderEngine->drawRect(xPosSlice, yTop, fWidthBar, 0.9 - yTop);
               osd_set_colors();
            }
            xPosSlice += fWidthBar;
         }
         sprintf(szBuff, "RadioLink-%d Min/Max Ack Time (ms)", iLink+1);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text*1.3;
         y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraphSmall*1.3, fWidthGraph, NULL, iTmp1, iTmp2, NULL, iCountIntervals);
         y += height_text_small;
         iCountGraphs++;
      }
   }

   // ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY )
   {
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);
      y = _osd_render_ack_time_hist(pRTInfoVehicle, xPos, fGraphXStart, y, hGraphSmall, fWidthGraph*0.5 );
      iCountGraphs++;
   }
*/

/*
   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoBlocksSkippedBlocks[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Unrecoverable Video Blocks skipped");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;
   iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfoVehicle->uCountReqRetransmissions[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Requested Retransmissions");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;

      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfoVehicle->uCountAckRetransmissions[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Ack Retransmissions");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
      iCountGraphs++;
   }
*/
   //------------------------------------------------

/*
   // ----------------------------------------------
   float fStrokeSize = 2.0;
   g_pRenderEngine->setStrokeSize(fStrokeSize);
   float yMid = 0.5*(y+yTop);
   xPosSlice = fGraphXStart + dx;

   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      u32 uFlags = pCRTInfo->uFlagsAdaptiveVideo[i+iStartIntervals];
      if ( 0 == uFlags )
      {
         xPosSlice += fWidthBar;
         continue;
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE )
      {
         g_pRenderEngine->setStroke(80, 80, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER )
      {
         g_pRenderEngine->setStroke(250, 80, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER )
      {
         g_pRenderEngine->setStroke(80, 250, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_USER )
      {
         g_pRenderEngine->setStroke(80, 80, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_LOWER )
      {
         g_pRenderEngine->setStroke(250, 80, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_HIGHER )
      {
         g_pRenderEngine->setStroke(80, 250, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_RECV_ACK )
      {
         g_pRenderEngine->setStroke(50, 50, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice + fWidthPixel, yMid, xPosSlice + fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice + fWidthPixel*2, yMid, xPosSlice + fWidthPixel*2, y);
      }

      xPosSlice += fWidthBar;
   }
   iCountGraphs++;
   }
*/   
   float xLine = fGraphXStart + dx + iNormalizedIndex * fWidthBar;
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->setStroke(255,255,100, OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xLine, yPos, xLine, yPos+height - fHStatsPart2);
   g_pRenderEngine->drawLine(xLine+g_pRenderEngine->getPixelWidth(), yPos, xLine + g_pRenderEngine->getPixelWidth(), yPos+height - fHStatsPart2);
   g_pRenderEngine->drawLine(xLine+fWidthBar-g_pRenderEngine->getPixelWidth(), yPos, xLine +fWidthBar-g_pRenderEngine->getPixelWidth(), yPos+height - fHStatsPart2);
   osd_set_colors();

   float yStartPart2 = y;

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY )
   {
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);

      int iMinTime = 1000000;
      int iMaxTime = 0;
      int iAvgTime = 0;
      int iCounts = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/2; i++ )
      {
         if ( (i == pRTInfoVehicle->iAckTimeIndex[0]) || (i == pRTInfoVehicle->iAckTimeIndex[0] + 1) || (i == pRTInfoVehicle->iAckTimeIndex[0]-1) )
            continue;
         iCounts++;
         int iTime = pRTInfoVehicle->uAckTimes[i][0];
         iAvgTime += iTime;
         if ( iTime < iMinTime ) iMinTime = iTime;
         if ( iTime > iMaxTime ) iMaxTime = iTime;
      }

      if ( iCounts > 0 )
         iAvgTime /= iCounts;


      strcpy(szTitle, "Ack Roundtrip Times History:");
      g_pRenderEngine->setColors(get_Color_Dev());

      float fWText = g_pRenderEngine->textWidth(s_idFontStats, szTitle) + 0.02;
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);

      sprintf(szBuff, "Avg / Min / Max RT time: %d / %d / %d ms", iAvgTime, iMinTime, iMaxTime);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, szBuff);
      fWText += g_pRenderEngine->textWidth(s_idFontStats, szBuff) + 0.02;

      g_pRenderEngine->setColors(cWhite);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[white] ping ack");
      fWText += g_pRenderEngine->textWidth(s_idFontStats, "[white] ping ack") + 0.02;
      g_pRenderEngine->setColors(cGreen);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[green] adaptive ack");
      fWText += g_pRenderEngine->textWidth(s_idFontStats, "[green] adaptive ack") + 0.02;
      g_pRenderEngine->setColors(cBlue);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[blue] retr ack");
      fWText += g_pRenderEngine->textWidth(s_idFontStats, "[blue] retr ack") + 0.02;

      y += height_text*1.3;

      g_pRenderEngine->setColors(get_Color_Dev());
      sprintf(szBuff, "%d", iMinTime);
      g_pRenderEngine->drawText(xPos, y + hGraph - height_text_small*0.7, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iMaxTime);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", (iMaxTime+iMinTime)/2);
      g_pRenderEngine->drawText(xPos, y + hGraph*0.5 - height_text_small*0.7, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iMinTime + (iMaxTime-iMinTime)/4);
      g_pRenderEngine->drawText(xPos, y + hGraph*0.75 - height_text_small*0.7, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dx, y - fHPixel, fGraphXStart + fWidthGraph - dx, y - fHPixel);
      g_pRenderEngine->drawLine(fGraphXStart + dx, y + hGraph *0.5, fGraphXStart + fWidthGraph - dx, y + hGraph *0.5);
      g_pRenderEngine->drawLine(fGraphXStart + dx, y + hGraph *0.75, fGraphXStart + fWidthGraph - dx, y + hGraph *0.75);

      float xPosBar = fGraphXStart + dx;
      float fWidthBar2 = (fWidthGraph-dx) / ((float)SYSTEM_RT_INFO_INTERVALS/2.0);
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/2; i++ )
      {
         if ( (iMaxTime == 0) || (i == pRTInfoVehicle->iAckTimeIndex[0]) || (i == pRTInfoVehicle->iAckTimeIndex[0] + 1) || (i == pRTInfoVehicle->iAckTimeIndex[0]-1) )
         {
            if ( i == pRTInfoVehicle->iAckTimeIndex[0] )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraph);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraph);
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iTime = pRTInfoVehicle->uAckTimes[i][0];

         float fHeight = hGraph * (float)iTime / (float)iMaxTime;
         if ( fHeight < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }
         
         if ( pRTInfoVehicle->uAckTypes[i][0] == 0x01 )
            g_pRenderEngine->setColors(cWhite);
         else if ( pRTInfoVehicle->uAckTypes[i][0] == 0x02 )
            g_pRenderEngine->setColors(cGreen);
         else if ( pRTInfoVehicle->uAckTypes[i][0] == 0x04 )
            g_pRenderEngine->setColors(cBlue);
         else
            osd_set_colors();

         g_pRenderEngine->drawRect(xPosBar, y + hGraph - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         xPosBar += fWidthBar2;
      }

      osd_set_colors();
      y += hGraph;
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_PROC_TIMES )
   {
      int iMinTotalTime = 1000000;
      int iMaxTotalTime = 0;
      int iAvgTotalTime = 0;
      int iCounts = 0;
      int iIntervalsCount = SYSTEM_RT_INFO_INTERVALS_FRAMES/3;
      for( int i=0; i<iIntervalsCount; i++ )
      {
         if ( (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
            continue;
         iCounts++;
         int iTotalTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 12) & 0x0F);
         iAvgTotalTime += iTotalTime;

         if ( iTotalTime < iMinTotalTime ) iMinTotalTime = iTotalTime;
         if ( iTotalTime > iMaxTotalTime ) iMaxTotalTime = iTotalTime;
      }

      if ( iCounts > 0 )
         iAvgTotalTime /= iCounts;

      sprintf(szTitle, "Video Frames Processing Times: total avg, min/max  %d, %d/%d  ms", iAvgTotalTime, iMinTotalTime, iMaxTotalTime);

      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;

      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text_small*0.7, g_idFontStatsSmall, "0 ms");
      sprintf(szBuff, "%d ms", iMaxTotalTime);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d ms", iAvgTotalTime);
      g_pRenderEngine->drawText(xPos, y + 0.5 * hGraphSmall - height_text_small*0.3, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dx, y - fHPixel, fGraphXStart + fWidthGraph - dx, y - fHPixel);
      g_pRenderEngine->drawLine(fGraphXStart + dx, y + hGraphSmall * 0.5, fGraphXStart + fWidthGraph - dx, y + hGraphSmall * 0.5);

      float xPosBar = fGraphXStart + dx;
      float fWidthBar2 = (fWidthGraph-dx) / (float)iIntervalsCount;

      for( int i=0; i<iIntervalsCount; i++ )
      {
         if ( (iMaxTotalTime == 0) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iReadTime = (int)(pCDebugRTInfo->uVideoFramesProcessingTimes[i] & 0x0F);
         int iSendTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 4) & 0x0F);
         int iTotalTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 12) & 0x0F);

         float fHeightRead = hGraphSmall * (float) iReadTime / (float)iMaxTotalTime;
         float fHeightSend = hGraphSmall * (float) iSendTime / (float)iMaxTotalTime;
         float fHeightTotal = hGraphSmall * (float) iTotalTime / (float)iMaxTotalTime;

         if ( fHeightTotal < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }

         g_pRenderEngine->setColors(cBlue);
         g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeightRead, fWidthBar2*0.33 - 2.0 * fWPixel, fHeightRead);
         g_pRenderEngine->setColors(cGreen);
         g_pRenderEngine->drawRect(xPosBar + fWidthBar*0.33 + fWPixel, y + hGraphSmall - fHeightSend, fWidthBar2*0.33 - 2.0 * fWPixel, fHeightSend);
         g_pRenderEngine->setColors(cWhite);
         g_pRenderEngine->drawRect(xPosBar + fWidthBar*0.66 + 2.0*fWPixel, y + hGraphSmall - fHeightTotal, fWidthBar2*0.33 - 2.0 * fWPixel, fHeightTotal);

         xPosBar += fWidthBar2;
      }
      
      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SIZES )
   {
      int iMinSize = 1000000;
      int iMaxSize = 0;
      int iAvgSize = 0;
      int iCounts = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
            continue;
         iCounts++;
         int iSize = (int)(pCDebugRTInfo->uOutputedFramesSizes[i] & 0xFFFFFF);
         iAvgSize += iSize;

         if ( iSize < iMinSize ) iMinSize = iSize;
         if ( iSize > iMaxSize ) iMaxSize = iSize;
      }

      if ( iCounts > 0 )
         iAvgSize /= iCounts;

      sprintf(szTitle, "Video Frames Sizes: avg, min/max  %d, %d/%d  kbytes", iAvgSize/1000, iMinSize/1000, iMaxSize/1000);

      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      float fWText = g_pRenderEngine->textWidth(s_idFontStats, szTitle) + 0.02;
      g_pRenderEngine->setColors(cBlue);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[blue] = keyframes");
      g_pRenderEngine->setColors(get_Color_Dev());
      y += height_text*1.3;

      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text_small*0.7, g_idFontStatsSmall, "0");
      sprintf(szBuff, "%d", iMaxSize/1000);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iAvgSize/1000);
      g_pRenderEngine->drawText(xPos, y + 0.5 * hGraphSmall - height_text_small*0.3, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dx, y - fHPixel, fGraphXStart + fWidthGraph - dx, y - fHPixel);

      float xPosBar = fGraphXStart + dx;
      float fWidthBar2 = (fWidthGraph-dx) / SYSTEM_RT_INFO_INTERVALS_FRAMES;

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (iMaxSize == 0) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iSize = (int)(pCDebugRTInfo->uOutputedFramesSizes[i] & 0xFFFFFF);

         float fHeight = hGraphSmall * (float) iSize / (float)iMaxSize;

         if ( fHeight < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }

         if ( pCDebugRTInfo->uReceivedFrameNALFlags[i] & (VIDEO_STATUS_FLAGS2_IS_NAL_I >> 8) )
         {
            g_pRenderEngine->setColors(cBlue);
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
            g_pRenderEngine->setColors(get_Color_Dev());
         }
         else
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         xPosBar += fWidthBar2;
      }

      float fhAverage = hGraphSmall * (float) iAvgSize / (float)iMaxSize;
      g_pRenderEngine->setColors(get_Color_OSDText());
      g_pRenderEngine->setStroke(255,255,255,1.0);
      g_pRenderEngine->drawLine(fGraphXStart + dx, y + hGraphSmall - fhAverage, fGraphXStart + fWidthGraph - dx, y + hGraphSmall - fhAverage);
      g_pRenderEngine->setStroke(0,0,0,1.0);
      g_pRenderEngine->drawLine(fGraphXStart + dx, y + hGraphSmall - fhAverage + fHPixel, fGraphXStart + fWidthGraph - dx, y + hGraphSmall - fhAverage + fHPixel);
      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_CAPTURE_JITTER )
   {
      int iMinTime = 1000000;
      int iMaxTime = 0;
      int iAvgTime = 0;
      int iCounts = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
            continue;
         iCounts++;
         int iTime = (int)(pCDebugRTInfo->uCaptureFramesDistanceTimes[i] & 0xFF);
         iAvgTime += iTime;

         if ( iTime < iMinTime ) iMinTime = iTime;
         if ( iTime > iMaxTime ) iMaxTime = iTime;
      }

      if ( iCounts > 0 )
         iAvgTime /= iCounts;

      sprintf(szTitle, "Video Frames Distance/Jitter Times (@Capture): avg, min/max  %d, %d/%d  milisec", iAvgTime, iMinTime, iMaxTime);
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      //float fWText = g_pRenderEngine->textWidth(s_idFontStats, szTitle) + 0.02;
      //g_pRenderEngine->setColors(cBlue);
      //g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[blue] = keyframes");
      g_pRenderEngine->setColors(get_Color_Dev());
      y += height_text*1.3;

      sprintf(szBuff, "%d", iMinTime);
      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text_small*0.7, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iMaxTime);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", (iMaxTime + iMinTime)/2);
      g_pRenderEngine->drawText(xPos, y + 0.5 * hGraphSmall - height_text_small*0.3, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dx, y - fHPixel, fGraphXStart + fWidthGraph - dx, y - fHPixel);

      float xPosBar = fGraphXStart + dx;
      float fWidthBar2 = (fWidthGraph-dx) / SYSTEM_RT_INFO_INTERVALS_FRAMES;

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (iMaxTime == 0) || (iMaxTime == iMinTime) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iTime = (int)(pCDebugRTInfo->uCaptureFramesDistanceTimes[i] & 0xFF);

         float fHeight = hGraphSmall * (float) (iTime-iMinTime) / (float)(iMaxTime-iMinTime);

         if ( fHeight < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }
         if ( iTime > iAvgTime )
            g_pRenderEngine->setColors(cBlue);
         else
            g_pRenderEngine->setColors(cWhite);
         g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         xPosBar += fWidthBar2;
      }
     
      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SEND_DURATION )
   {
      int iMinTime = 1000000;
      int iMaxTime = 0;
      int iAvgTime = 0;
      int iCounts = 0;
      int iTime = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
            continue;
         iCounts++;

         iTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 8) & 0xFF);
         iTime += (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 4) & 0x0F);
         iAvgTime += iTime;

         if ( iTime < iMinTime ) iMinTime = iTime;
         if ( iTime > iMaxTime ) iMaxTime = iTime;
      }

      if ( iCounts > 0 )
         iAvgTime /= iCounts;

      sprintf(szTitle, "Video Frames Send Duration (@Air Side): avg, min, max  %d, %d, %d  milisec", iAvgTime, iMinTime, iMaxTime);
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      float fWText = g_pRenderEngine->textWidth(s_idFontStats, szTitle) + 0.02;
      g_pRenderEngine->setColors(cBlue);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[blue] = keyframes");
      fWText += g_pRenderEngine->textWidth(s_idFontStats, "[blue] = keyframes") + 0.02;
      g_pRenderEngine->setColors(cGreen);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[green] = other data");
      fWText += g_pRenderEngine->textWidth(s_idFontStats, "[green] = other data") + 0.02;
      g_pRenderEngine->setColors(get_Color_Dev());
      szBuff[0] = 0;
      int iMSPerFrame = 0;
      if ( 0 != pActiveModel->video_params.iVideoFPS )
         iMSPerFrame = 1000/pActiveModel->video_params.iVideoFPS;
      sprintf(szBuff, "FPS: %d, %d ms/frame", pActiveModel->video_params.iVideoFPS, iMSPerFrame);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, szBuff);
      y += height_text*1.3;

      sprintf(szBuff, "%d ms", 0);
      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text*0.7, g_idFontStats, szBuff);
      sprintf(szBuff, "%d ms", (iMSPerFrame*3)/2);
      g_pRenderEngine->drawText(xPos, y - height_text*0.3, g_idFontStats, szBuff);

      sprintf(szBuff, "%d ms",iMSPerFrame);
      g_pRenderEngine->drawText(xPos, y + 0.33 * hGraphSmall - height_text*0.3, g_idFontStats, szBuff);

      sprintf(szBuff, "%d ms",iMSPerFrame/2);
      g_pRenderEngine->drawText(xPos, y + 0.66* hGraphSmall - height_text*0.3, g_idFontStats, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y - fHPixel, fGraphXStart + fWidthGraph - dxWithMs, y - fHPixel);
      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y + hGraphSmall, fGraphXStart + fWidthGraph - dxWithMs, y + hGraphSmall);
      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y + 0.33 * hGraphSmall, fGraphXStart + fWidthGraph - dxWithMs, y + 0.33 * hGraphSmall);
      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y + 0.66 * hGraphSmall, fGraphXStart + fWidthGraph - dxWithMs, y + 0.66 * hGraphSmall);

      float xPosBar = fGraphXStart + dxWithMs;
      float fWidthBar2 = (fWidthGraph-dxWithMs) / SYSTEM_RT_INFO_INTERVALS_FRAMES;
      float fHeight = 0.0;
      float fMaxBarHeight = 0.0;
      double cWhite[] = {255,255,255, 1.0};
      double cYellow[] = {255,255,0, 1.0};
      double cRed[] = {255,50,50, 1.0};

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (iMaxTime == 0) || (iMaxTime == iMinTime) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }

         fMaxBarHeight = 0.0;
         // Sent video tx time
         iTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 8) & 0xFF);
         fHeight = hGraphSmall * (float) (iTime) / (float)((iMSPerFrame*3)/2);
         if ( fHeight > 1.0 )
            fHeight = 1.0;
         if ( fHeight > fMaxBarHeight )
            fMaxBarHeight = fHeight;
         if ( fHeight > fHPixel )
         {
            if ( iTime >= iMSPerFrame )
               g_pRenderEngine->setColors(cRed);
            else if ( iTime >= (iMSPerFrame*8)/10 )
               g_pRenderEngine->setColors(cYellow);
            else if ( pCDebugRTInfo->uReceivedFrameNALFlags[i] & (VIDEO_STATUS_FLAGS2_IS_NAL_I >> 8) )
               g_pRenderEngine->setColors(cBlue);
            else
               g_pRenderEngine->setColors(get_Color_Dev());
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
            g_pRenderEngine->setColors(get_Color_Dev());
         }

         // Expected video tx time
         iTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 24) & 0xFF);
         fHeight = hGraphSmall * (float) (iTime) / (float)((iMSPerFrame*3)/2);
         if ( fHeight > 1.0 )
            fHeight = 1.0;
         if ( fHeight > fMaxBarHeight )
            fMaxBarHeight = fHeight;
         if ( fHeight > fHPixel )
         {
            g_pRenderEngine->setColors(cGreen);
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight*0.5);
            g_pRenderEngine->setColors(get_Color_Dev());
         }

         // Sent others tx time
         iTime = (int)((pCDebugRTInfo->uVideoFramesProcessingTimes[i] >> 4) & 0x0F);
         float fHeight = hGraphSmall * (float) (iTime) / (float)((iMSPerFrame*3)/2);
         if ( fHeight > 1.0 )
            fHeight = 1.0;

         if ( (iTime == 0) || (fHeight < 2.0*fHPixel) )
         {
            xPosBar += fWidthBar2;
            continue;
         }

         g_pRenderEngine->setColors(cGreen);
         g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fMaxBarHeight - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         g_pRenderEngine->setColors(get_Color_Dev());

         xPosBar += fWidthBar2;
      }

      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_RECV_DURATION )
   {
      int iMinTime = 1000000;
      int iMaxTime = 0;
      int iAvgTime = 0;
      int iMinThroughputKb = 1000000000;
      int iMaxThroughputKb = 0;
      int iAvgThroughputKb = 0;
      int iCounts = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
            continue;
         iCounts++;

         int iTime = (int)(pCDebugRTInfo->uReceivedFrameDurationTensMs[i]);
         iAvgTime += iTime;

         if ( iTime < iMinTime ) iMinTime = iTime;
         if ( iTime > iMaxTime ) iMaxTime = iTime;

         int iThrKb = (int)(pCDebugRTInfo->uReceivedFrameThroughputBPS[i]/1000);
         iAvgThroughputKb += iThrKb;

         if ( iThrKb < iMinThroughputKb ) iMinThroughputKb = iThrKb;
         if ( iThrKb > iMaxThroughputKb ) iMaxThroughputKb = iThrKb;
      }

      if ( iCounts > 0 )
      {
         iAvgTime /= iCounts;
         iAvgThroughputKb /= iCounts;
      }
      sprintf(szTitle, "Video Frames Receive Duration (@GS Side): avg, min/max  %.1f, %.1f / %.1f  milisec", (float)iAvgTime/10.0, (float)iMinTime/10.0, (float)iMaxTime/10.0);
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      float fWText = g_pRenderEngine->textWidth(s_idFontStats, szTitle) + 0.02;
      g_pRenderEngine->setColors(cBlue);
      g_pRenderEngine->drawText(xPos + fWText, y, s_idFontStats, "[blue] = keyframes");
      g_pRenderEngine->setColors(get_Color_Dev());
      y += height_text*1.3;

      sprintf(szBuff, "%.1f ms", (float)iMinTime/10.0);
      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text_small*0.7, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%.1f ms", (float)iMaxTime/10.0);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%.1f ms", (float)(iMaxTime + iMinTime)/2.0/10.0);
      g_pRenderEngine->drawText(xPos, y + 0.5 * hGraphSmall - height_text_small*0.3, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y - fHPixel, fGraphXStart + fWidthGraph - dxWithMs, y - fHPixel);

      float xPosBar = fGraphXStart + dxWithMs;
      float fWidthBar2 = (fWidthGraph-dxWithMs) / SYSTEM_RT_INFO_INTERVALS_FRAMES;

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (iMaxTime == 0) || (iMaxTime == iMinTime) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iTime = (int)(pCDebugRTInfo->uReceivedFrameDurationTensMs[i]);

         float fHeight = hGraphSmall * (float) (iTime-iMinTime) / (float)(iMaxTime-iMinTime);

         if ( fHeight < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }

         if ( pCDebugRTInfo->uReceivedFrameNALFlags[i] & (VIDEO_STATUS_FLAGS2_IS_NAL_I >> 8) )
         {
            g_pRenderEngine->setColors(cBlue);
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
            g_pRenderEngine->setColors(get_Color_Dev());
         }
         else
            g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         xPosBar += fWidthBar2;
      }
     
      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;

      sprintf(szTitle, "Video Frames Receive Throughput (@GS Side): avg, min/max  %.1f, %.1f / %.1f  Mb/s", (float)iAvgThroughputKb/1000.0, (float)iMinThroughputKb/1000.0, (float)iMaxThroughputKb/1000.0);
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTitle);
      y += height_text*1.3;

      sprintf(szBuff, "%.1f Mb/s", (float)iMinThroughputKb/1000.0);
      g_pRenderEngine->drawText(xPos, y + hGraphSmall - height_text_small*0.7, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%.1f Mb/s", (float)iMaxThroughputKb/1000.0);
      g_pRenderEngine->drawText(xPos, y - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%.1f Mb/s", (float)(iMaxThroughputKb + iMinThroughputKb)/2.0/1000.0);
      g_pRenderEngine->drawText(xPos, y + 0.5 * hGraphSmall - height_text_small*0.3, g_idFontStatsSmall, szBuff);

      g_pRenderEngine->drawLine(fGraphXStart + dxWithMs, y - fHPixel, fGraphXStart + fWidthGraph - dxWithMs, y - fHPixel);

      xPosBar = fGraphXStart + dxWithMs;
      
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
      {
         if ( (iMaxThroughputKb == 0) || (iMaxThroughputKb == iMinThroughputKb) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex + 1) || (i == pCDebugRTInfo->iCurrentFrameBufferIndex - 1) )
         {
            if ( i == pCDebugRTInfo->iCurrentFrameBufferIndex )
            {
               g_pRenderEngine->setColors(get_Color_OSDText());
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2, y, xPosBar + 0.5*fWidthBar2, y + hGraphSmall);
               g_pRenderEngine->drawLine(xPosBar + 0.5*fWidthBar2 + fWPixel, y, xPosBar + 0.5*fWidthBar2 + fWPixel, y + hGraphSmall);
               g_pRenderEngine->setColors(get_Color_Dev());
            }
            xPosBar += fWidthBar2;
            continue;
         }
         int iThrKb = (int)(pCDebugRTInfo->uReceivedFrameThroughputBPS[i]/1000);
         float fHeight = hGraphSmall * (float) (iThrKb - iMinThroughputKb) / (float)(iMaxThroughputKb-iMinThroughputKb);

         if ( fHeight < fHPixel )
         {
            xPosBar += fWidthBar2;
            continue;
         }
         g_pRenderEngine->drawRect(xPosBar, y + hGraphSmall - fHeight, fWidthBar2 - 2.0 * fWPixel, fHeight);
         xPosBar += fWidthBar2;
      }
     
      osd_set_colors();
      y += hGraphSmall;
      y += height_text_small;
      iCountGraphs++;
   }

   s_fStaticHeightDebugStatsPart2 = (y - yStartPart2);
 
   s_fStaticHeightDebugStats = y - s_fStaticYPosDebugStats + s_fOSDStatsMargin*0.7;
   s_fStaticYPosDebugStats = 0.94 - s_fStaticHeightDebugStats;

   s_fStaticHeightGraph = height_text * 3.0 * 1.2;
   s_fStaticHeightGraphSmall = height_text * 1.6 * 1.2;
   s_fStaticHeightGraphXSmall = height_text * 1.2 * 1.2;
   if ( iCountGraphs <= 3 )
   {
      s_fStaticHeightGraph = height_text * 3.0 * 2.5;
      s_fStaticHeightGraphSmall = height_text * 1.6 * 2.5;
      s_fStaticHeightGraphXSmall = height_text * 1.2 * 2.5;
   }
   else if ( iCountGraphs <= 5 )
   {
      s_fStaticHeightGraph = height_text * 3.0 * 1.5;
      s_fStaticHeightGraphSmall = height_text * 1.6 * 1.5;
      s_fStaticHeightGraphXSmall = height_text * 1.2 * 1.5;
   }
}

void osd_reder_debug_ping_stats()
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == g_pSMDbgPingStats )
      g_pSMDbgPingStats = shared_mem_ctrl_ping_stats_info_open_for_read();

   if ( NULL != g_pSMDbgPingStats )
      memcpy((u8*)&g_SMDbgPingStats, (u8*)g_pSMDbgPingStats, sizeof(shared_mem_ctrl_ping_stats));

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

   float fWidth = 0.14;
   float fHeight = 0.6;
   float xPos = 0.02;
   float yPos = 0.14;

   char szBuff[32];
   char szBuff2[32];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, fWidth, fHeight, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   fWidth -= 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float fRightMargin = xPos + fWidth;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Debug Ping RT Stats");
   
   float y = yPos + height_text*1.5*s_OSDStatsLineSpacing;
   
   if ( NULL == g_pSMDbgPingStats )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No info"); 
      return;
   }

   int iCountIntervals = 20;
   int iCounts[20];
   static int s_iLastPingArrayIndex = 0;
   static int s_iPingCountsTimeout[20];
   static int s_iPingCountsInit = 0;

   int iMaxRT = 0;
   int iMinRT = 10000;
   int iAvgRT = 0;
   int iSum = 0;
   int iSumCount = 0;

   if ( 0 == s_iPingCountsInit )
   {
      s_iPingCountsInit = 1;
      memset((u8*)&(s_iPingCountsTimeout[0]), 0, sizeof(s_iPingCountsTimeout));
   }

   memset((u8*)&(iCounts[0]), 0, sizeof(iCounts));

   for( int i=0; i<MAX_DBG_PING_DATAPOINTS; i++ )
   {
      int iVal = (int)g_SMDbgPingStats.uRTTime[0][i]; 
      if ( 0 == iVal )
         continue;

      iSum += iVal;
      iSumCount++;

      if ( iVal > iMaxRT )
         iMaxRT = iVal;
      if ( iVal < iMinRT )
         iMinRT = iVal;

      if ( iVal >= iCountIntervals )
         iCounts[0]++;
      else
         iCounts[iVal]++;
   }

   if ( iSumCount > 0 )
      iAvgRT = iSum/iSumCount;

   int iMaxCounter = 0;
   for( int i=0; i<iCountIntervals; i++ )
   {
      if ( iCounts[i] > iMaxCounter )
         iMaxCounter = iCounts[i];

      if ( s_iPingCountsTimeout[i] > 0 )
         s_iPingCountsTimeout[i]--;
   }

   while ( s_iLastPingArrayIndex != g_SMDbgPingStats.iCurrentDataPointIndex[0] )
   {
      int iVal = (int)g_SMDbgPingStats.uRTTime[0][s_iLastPingArrayIndex]; 
      if ( 0 != iVal )
      {
         int iTimeIndex = 0;
         if ( iVal >= iCountIntervals )
            iTimeIndex = 0;
         else
            iTimeIndex = iVal;

         s_iPingCountsTimeout[iTimeIndex] = 10;
      }
      s_iLastPingArrayIndex = (s_iLastPingArrayIndex+1) % MAX_DBG_PING_DATAPOINTS;
   }

   strcpy(szBuff, "N/A");
   if ( NULL != pActiveModel )
   {
      if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_ADV )
         strcpy(szBuff, "Adv");
      else if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_BASIC )
         strcpy(szBuff, "Basic");
      else if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_NONE )
         strcpy(szBuff, "None");
      else
         strcpy(szBuff, "Error");
   }
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Clock Sync Type:", szBuff);
   y += height_text_small;

   u32 uPingIntervalMs = 500;
   if ( NULL != pActiveModel )
      uPingIntervalMs = compute_ping_interval_ms(pActiveModel->uModelFlags, pActiveModel->rxtx_sync_type, pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags);
   else
   {
      uPingIntervalMs = 1000/DEFAULT_PING_FREQUENCY;
      if ( (NULL != get_ControllerSettings()) && (get_ControllerSettings()->nPingClockSyncFrequency != 0) )
         uPingIntervalMs = 1000/get_ControllerSettings()->nPingClockSyncFrequency;
   }
   strcpy(szBuff, "N/A");
   if ( 0 != uPingIntervalMs )
      sprintf(szBuff, "%d/sec", 1000/uPingIntervalMs);
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Pings/Sec:", szBuff);
   y += height_text_small;

   sprintf(szBuff, "%d ms", iMinRT);
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Min RT:", szBuff);
   y += height_text_small;

   sprintf(szBuff, "%d ms", iMaxRT);
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Max RT:", szBuff);
   y += height_text_small;

   sprintf(szBuff, "%d ms", iAvgRT);
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Avg RT:", szBuff);
   y += height_text_small;

   sprintf(szBuff, "%d", iCounts[0]);
   _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, "Spikes:", szBuff);
   y += height_text_small;

   y += height_text_small;

   float fHeightGraphs = yPos + fHeight - y;
   float fHeightBar = fHeightGraphs/(float)(iCountIntervals+1);
   float fWidthPrefix = 1.2*g_pRenderEngine->textWidth(s_idFontStatsSmall, "55 ms");
   float fMaxWidthBar = (fRightMargin-xPos)*0.94 - fWidthPrefix - g_pRenderEngine->textWidth(s_idFontStatsSmall, "55");
   float hPixel = g_pRenderEngine->getPixelHeight();


   for( int i=1; i<=iCountIntervals; i++ )
   {
      int iVal = 0;
      int iTimer = 0;
      if ( i == iCountIntervals )
      {
         iVal = iCounts[0];
         iTimer = s_iPingCountsTimeout[0];
         strcpy(szBuff, "Spikes");
      }
      else
      {
         iVal = iCounts[i];
         iTimer = s_iPingCountsTimeout[i];
         if ( i >= 10 )
            sprintf(szBuff, "%d ms", i);
         else
            sprintf(szBuff, " %d ms", i);
      }
      sprintf(szBuff2, "%d", iVal);

      if ( iVal == 0 )
      {
         osd_set_colors();
         g_pRenderEngine->drawLine(xPos + fWidthPrefix, y + 2.0*hPixel, xPos + fWidthPrefix, y + fHeightBar - 2.0*hPixel);
      }
      else if ( iMaxCounter > 0 )
      {
         float fWidthBar = fMaxWidthBar * (float)iVal / (float)iMaxCounter;
         if ( 0 == iTimer )
            osd_set_colors();
         else
         {
            int iColor = 255 - iTimer * iTimer * 2.5;
            g_pRenderEngine->setFill(iColor,iColor,255,1.0);
         }
         g_pRenderEngine->drawRect(xPos + fWidthPrefix, y + hPixel, fWidthBar, fHeightBar - 4.0*hPixel);
      }
 
      _osd_stats_draw_line(xPos, fRightMargin, y, s_idFontStatsSmall, szBuff, szBuff2);
      osd_set_colors();
      y += fHeightBar;
   }
   osd_set_colors();
}
