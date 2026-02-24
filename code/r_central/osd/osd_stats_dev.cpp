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
#include "../../common/string_utils.h"
#include <math.h>
#include "osd_stats_dev.h"
#include "osd_common.h"
#include "../colors.h"
#include "../shared_vars.h"
#include "../timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

float osd_render_stats_adaptive_video_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float hGraph = height_text * 2.0;

   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_RouterVehiclesRuntimeInfo )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }
   return height;
}

float osd_render_stats_adaptive_video_get_width()
{
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");

   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_adaptive_video(float xPos, float yPos)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();

   int iIndexVehicleRuntimeInfo = -1;

   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }
   if ( (NULL == pVDS) || (NULL == pActiveModel) || (-1 == iIndexVehicleRuntimeInfo) )
      return;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 2.0;

   float width = osd_render_stats_adaptive_video_get_width();
   float height = osd_render_stats_adaptive_video_get_height();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Ctrl Adaptive Video Stats");
}

float osd_render_stats_graphs_vehicle_tx_gap_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float hGraph = height_text * 4.0;
   float hGraph2 = height_text * 3.0;

   float height = 2.0 *s_fOSDStatsMargin*1.1 + height_text*s_OSDStatsLineSpacing;
   height += hGraph + height_text*3.6 + 0.9 * height_text*s_OSDStatsLineSpacing + 2.0*hGraph2 + 2.0*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_graphs_vehicle_tx_gap_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAA");

   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

void  osd_render_stats_graphs_vehicle_tx_gap(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 4.0;
   float hGraph2 = height_text * 3.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_graphs_vehicle_tx_gap_get_width();
   float height = osd_render_stats_graphs_vehicle_tx_gap_get_height();

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;

   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Vehicle Tx Stats");

   osd_set_colors();

   if ( ! g_bGotStatsVehicleTx )
   {
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "No Data.");
      return;
   }

   float rightMargin = xPos + width;
   float marginH = 0.0;
   float widthMax = width - marginH - 0.001;

   char szBuff[64];
   sprintf(szBuff,"%d ms/bar", g_PHVehicleTxHistory.iSliceInterval);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);

   float w = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
   sprintf(szBuff,"%.1f seconds, ", (((float)g_PHVehicleTxHistory.uCountValues) * g_PHVehicleTxHistory.iSliceInterval)/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin-w, yPos, s_idFontStatsSmall, szBuff);
   w += g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   // First graph

   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Tx Min/Max/Avg Gaps (ms)");
   y += height_text_small + height_text*0.2;

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "8878");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / g_PHVehicleTxHistory.uCountValues;
   float xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   if ( 0 == g_PHVehicleTxHistory.uCountValues )
      g_PHVehicleTxHistory.uCountValues = 1;
   if ( g_PHVehicleTxHistory.uCountValues > sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds)/sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[0]))
       g_PHVehicleTxHistory.uCountValues = sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds)/sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[0]);

   u32 uAverageTxGapSum = 0;
   u8 uAverageTxGapCount = 0;
   u32 uAverageVideoPacketsIntervalSum = 0;
   u8 uAverageVideoPacketsIntervalCount = 0;
   u8 uValMaxMax = 0;
   u8 uMaxTxPackets = 0;
   u8 uMaxVideoPacketsGap = 0;
   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF != g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k] )
      {
         uAverageTxGapCount++;
         uAverageTxGapSum += g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k];
      }

      if ( 0xFF != g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k] )
      {
         uAverageVideoPacketsIntervalCount++;
         uAverageVideoPacketsIntervalSum += g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k];
      }

      if ( 0xFF != g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] )
      if ( g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] > uMaxVideoPacketsGap )
         uMaxVideoPacketsGap = g_PHVehicleTxHistory.historyVideoPacketsGapMax[k];

      if ( 0xFF != g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] )
      if ( g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] > uValMaxMax )
         uValMaxMax = g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k];

      if ( 0xFF != g_PHVehicleTxHistory.historyTxPackets[k] )
      if ( g_PHVehicleTxHistory.historyTxPackets[k] > uMaxTxPackets )
         uMaxTxPackets = g_PHVehicleTxHistory.historyTxPackets[k];
   }

   double pc[4];
   memcpy(pc, get_Color_Dev(), 4*sizeof(double));
   g_pRenderEngine->setColors(get_Color_Dev());

   int iMax = 200;
   if ( uValMaxMax <= 100 )
      iMax = 100;
   if ( uValMaxMax <= 50 )
      iMax = 50;

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax*3/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.25, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.75, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.25, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.25);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.5);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.75, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.75);
   }

   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k])/iMax;
         if ( hBar > hGraph )
            hBar = hGraph;
         g_pRenderEngine->drawRect(xBar, y+hGraph - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH*2.0);
   g_pRenderEngine->setStroke(250,70,50, s_fOSDStatsGraphLinesAlpha);

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float hValPrev = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[0])/iMax;
   if ( hValPrev > hGraph )
      hValPrev = hGraph;
   xBar -= widthBar;


   for( int k=1; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k] )
         continue;
      float hBar = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k])/iMax;
      if ( hBar > hGraph )
         hBar = hGraph;

      g_pRenderEngine->drawLine(xBar + 0.5*widthBar, y+hGraph - hBar, xBar + 1.5*widthBar, y+hGraph - hValPrev);
      hValPrev = hBar;
      xBar -= widthBar;
   }

   y += hGraph + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setColors(get_Color_Dev());

   // Second graph (tx packets / sec )

   osd_set_colors();
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Tx Packets / Slice");
   y += height_text_small + height_text*0.2;

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph2 - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   g_pRenderEngine->setColors(get_Color_Dev());

   iMax = 200;
   if ( uMaxTxPackets <= 100 )
      iMax = 100;
   if ( uMaxTxPackets <= 50 )
      iMax = 50;

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph2 + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph2 + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2*0.5);
   }

   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxPackets[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph2*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph2*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyTxPackets[k])/iMax;
         if ( hBar > hGraph2 )
            hBar = hGraph2;
         g_pRenderEngine->drawRect(xBar, y+hGraph2 - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   y += hGraph2 + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();

   // Third graph (video packets in gaps )

   osd_set_colors();
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Video Packets Max Intervals (ms)");
   y += height_text_small + height_text*0.2;

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph2 - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   g_pRenderEngine->setColors(get_Color_Dev());

   iMax = 200;
   if ( uMaxVideoPacketsGap <= 100 )
      iMax = 100;
   if ( uMaxVideoPacketsGap <= 50 )
      iMax = 50;

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph2 + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph2 + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2*0.5);
   }

   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph2*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph2*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapMax[k])/iMax;
         if ( hBar > hGraph2 )
            hBar = hGraph2;
         g_pRenderEngine->drawRect(xBar, y+hGraph2 - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH*2.0);
   g_pRenderEngine->setStroke(250,70,70, s_fOSDStatsGraphLinesAlpha);

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   hValPrev = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapAvg[0])/iMax;
   if ( hValPrev > hGraph2 )
      hValPrev = hGraph2;
   xBar -= widthBar;


   for( int k=1; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k] )
         continue;
      float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k])/iMax;
      if ( hBar > hGraph2 )
         hBar = hGraph2;

      g_pRenderEngine->drawLine(xBar + 0.5*widthBar, y+hGraph2 - hBar, xBar + 1.5*widthBar, y+hGraph2 - hValPrev);
      hValPrev = hBar;
      xBar -= widthBar;
   }

   y += hGraph2 + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();

   g_pRenderEngine->setColors(get_Color_Dev());


   // Final Texts

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Average Tx Gap:");
   if ( 0 == uAverageTxGapCount )
      strcpy(szBuff, "N/A ms");
   else
      sprintf(szBuff, "%d ms", uAverageTxGapSum/uAverageTxGapCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Average  Video Packets Interval:");
   if ( 0 == uAverageVideoPacketsIntervalCount )
      strcpy(szBuff, "N/A ms");
   else
      sprintf(szBuff, "%d ms", uAverageVideoPacketsIntervalSum/uAverageVideoPacketsIntervalCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
}

float osd_render_stats_dev_adaptive_video_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text*1.0;

   float h = 2.0*height_text*s_OSDStatsLineSpacing;
   return h;
}

float osd_render_stats_dev_adaptive_video_info(float xPos, float yPos, float fWidth)
{
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   int iIndexVehicleRuntimeInfo = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }

   if ( -1 == iIndexVehicleRuntimeInfo )
      return 0.0;

   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

   float fRightMargin = xPos + fWidth;

   g_pRenderEngine->setColors(get_Color_Dev());


   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Adaptive/Keyframe Info");

   yPos += height_text*s_OSDStatsLineSpacing;

   return osd_render_stats_dev_adaptive_video_get_height();
}
