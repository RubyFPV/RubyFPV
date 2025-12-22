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
#include "osd_stats_video_bitrate.h"
#include "osd_common.h"
#include "../colors.h"
#include "../shared_vars.h"
#include "../timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

float osd_render_stats_video_bitrate_history_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 3.5;
   float hGraph2 = height_text * 2.0;

   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   height += hGraph + height_text + height_text_small*s_OSDStatsLineSpacing;

   height += height_text*s_OSDStatsLineSpacing + height_text;
   //if ( ! ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
   //   height += height_text*s_OSDStatsLineSpacing;
   //else
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
      height += hGraph2;
   
   height += 4.0*height_text_small*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_video_bitrate_history_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

void osd_render_stats_video_bitrate_history(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 3.5;
   float hGraph2 = height_text * 2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();

   float width = osd_render_stats_video_bitrate_history_get_width();
   float height = osd_render_stats_video_bitrate_history_get_height();

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   osd_set_colors();
   //g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Video Bitrate");
   
   char szBuff[64];
   sprintf(szBuff, "(Mbps, %d ms/slice)", (int)g_SM_DevVideoBitrateHistory.uGraphSliceInterval);
   osd_show_value_left(rightMargin,yPos, szBuff, s_idFontStatsSmall);

   float y = yPos + height_text*2.0*s_OSDStatsLineSpacing;

   if ( (NULL == pVDS) || (NULL == pActiveModel) )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Invalid Model");
      return;
   }

   if ( ! g_bGotStatsVideoBitrate )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Data");
      return;
   }

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "888");
   float widthGraph = widthMax - dxGraph*2;
   float widthBar = widthGraph / g_SM_DevVideoBitrateHistory.uTotalDataPoints;
   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, y-height_text_small*0.6, width, yBottomGraph - y + height_text_small*1.2, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
      
   u32 uMaxGraphValue = 0; // In mbps
   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uTotalDataPoints; i++ )
   {
      if ( g_SM_DevVideoBitrateHistory.history[i].uMinVideoDataRateMbps > uMaxGraphValue )
         uMaxGraphValue = g_SM_DevVideoBitrateHistory.history[i].uMinVideoDataRateMbps;
      if ( g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateCurrentProfileKb/1000 > uMaxGraphValue )
         uMaxGraphValue = g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateCurrentProfileKb/1000;
      if ( g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateKb/1000 > uMaxGraphValue )
         uMaxGraphValue = g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateKb/1000;
      if ( g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateAvgKb/1000 > uMaxGraphValue )
         uMaxGraphValue = g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateAvgKb/1000;
      if ( g_SM_DevVideoBitrateHistory.history[i].uTotalVideoBitrateAvgKb/1000 > uMaxGraphValue )
         uMaxGraphValue = g_SM_DevVideoBitrateHistory.history[i].uTotalVideoBitrateAvgKb/1000;
   }
   if ( uMaxGraphValue < 6 )
      uMaxGraphValue = 6;

   sprintf(szBuff, "%d", (int)uMaxGraphValue);
   g_pRenderEngine->drawText(xPos + dxGraph*0.2, y-height_text_small*0.6, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph+widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.6, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "%d", (int)uMaxGraphValue/2);
   g_pRenderEngine->drawText(xPos + dxGraph*0.2, y+0.5*hGraph-height_text_small*0.6, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+0.5*hGraph-height_text_small*0.6, s_idFontStatsSmall, szBuff);

   strcpy(szBuff, "0");
   g_pRenderEngine->drawText(xPos + dxGraph*0.2, y+hGraph-height_text_small*0.6, s_idFontStatsSmall,szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+hGraph-height_text_small*0.6, s_idFontStatsSmall,szBuff);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   //g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   //g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+0.5*hGraph, xPos + dxGraph + i + 2.0*wPixel, y+0.5*hGraph);
   }

   float hDataratePrev = 0.0;
   float hDatarateLowPrev = 0.0;
   float hVideoBitratePrev = 0.0;
   float hVideoBitrateAvgPrev = 0.0;
   float hTotalBitrateAvgPrev = 0.0;

   g_pRenderEngine->setStrokeSize(2.0);
   float xBarMiddle = xPos + dxGraph + widthBar*0.5 - g_pRenderEngine->getPixelWidth();
   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uTotalDataPoints; i++ )
   {
      float hDatarate = g_SM_DevVideoBitrateHistory.history[i].uMinVideoDataRateMbps*hGraph/uMaxGraphValue;
      float hDatarateLow = hDatarate * pActiveModel->radioLinksParams.uMaxLinkLoadPercent[0]/100.0;
      if ( (pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad > 0) && (pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad <= 90) )
         hDatarateLow = hDatarate * pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad / 100.0;

      float hVideoBitrate = g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateKb*hGraph/uMaxGraphValue/1000.0;
      float hVideoBitrateAvg = g_SM_DevVideoBitrateHistory.history[i].uVideoBitrateAvgKb*hGraph/uMaxGraphValue/1000.0;
      float hTotalBitrateAvg = g_SM_DevVideoBitrateHistory.history[i].uTotalVideoBitrateAvgKb*hGraph/uMaxGraphValue/1000.0;
      if ( i != 0 )
      {
         g_pRenderEngine->setStroke(200,200,255, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle-widthBar, y+hGraph-hVideoBitratePrev+hPixel, xBarMiddle, y+hGraph-hVideoBitrate+hPixel);

         g_pRenderEngine->setStroke(250,230,50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle-widthBar, y+hGraph-hVideoBitrateAvgPrev, xBarMiddle, y+hGraph-hVideoBitrateAvg);

         g_pRenderEngine->setStroke(255,250,220, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle-widthBar, y+hGraph-hTotalBitrateAvgPrev, xBarMiddle, y+hGraph-hTotalBitrateAvg);

         g_pRenderEngine->setStroke(250,50,100, s_fOSDStatsGraphLinesAlpha);

         g_pRenderEngine->drawLine(xBarMiddle-widthBar, y+hGraph-hDataratePrev, xBarMiddle, y+hGraph-hDataratePrev);
         if ( i != g_SM_DevVideoBitrateHistory.uCurrentDataPoint )
         if ( fabs(hDatarate-hDataratePrev) >= g_pRenderEngine->getPixelHeight() )           
            g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hDatarate, xBarMiddle, y+hGraph-hDataratePrev);
         
         g_pRenderEngine->setStroke(250,100,150, s_fOSDStatsGraphLinesAlpha);

         g_pRenderEngine->drawLine(xBarMiddle-widthBar, y+hGraph-hDatarateLow, xBarMiddle, y+hGraph-hDatarateLow);
         if ( i != g_SM_DevVideoBitrateHistory.uCurrentDataPoint )
         if ( fabs(hDatarateLow-hDatarateLowPrev) >= g_pRenderEngine->getPixelHeight() )           
            g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hDatarateLow, xBarMiddle, y+hGraph-hDatarateLowPrev);
      
         if ( g_SM_DevVideoBitrateHistory.history[i-1].uVideoProfileSwitches != g_SM_DevVideoBitrateHistory.history[i].uVideoProfileSwitches )
         if ( (g_SM_DevVideoBitrateHistory.history[i-1].uVideoProfileSwitches>>4) != (g_SM_DevVideoBitrateHistory.history[i].uVideoProfileSwitches>>4) )
         {
            g_pRenderEngine->setStroke(50,250,50, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawLine(xBarMiddle - widthBar*0.5, y, xBarMiddle - widthBar*0.5, y + hGraph);
         }

         if ( i == g_SM_DevVideoBitrateHistory.uCurrentDataPoint )
         {
            g_pRenderEngine->setStroke(100,100,250, 1.0);
            g_pRenderEngine->drawLine(xBarMiddle+widthBar*0.5, y+hGraph, xBarMiddle+widthBar*0.5, y);
         }
      }

      hDataratePrev = hDatarate;
      hDatarateLowPrev = hDatarateLow;
      hVideoBitratePrev = hVideoBitrate;
      hVideoBitrateAvgPrev = hVideoBitrateAvg;
      hTotalBitrateAvgPrev = hTotalBitrateAvg;
      xBarMiddle += widthBar;
   }
   y += hGraph + height_text*0.4;
   g_pRenderEngine->setStrokeSize(1.0);
   
   strcpy(szBuff, "Bitrate: ");
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
   float fTmpWidth = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

   strcpy(szBuff, "Avg Bitrate: ");
   g_pRenderEngine->drawText(xPos + fTmpWidth, y, s_idFontStatsSmall, szBuff);
   fTmpWidth += g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

   strcpy(szBuff, "Total Bitrate: ");
   g_pRenderEngine->drawText(xPos + fTmpWidth, y, s_idFontStatsSmall, szBuff);
   fTmpWidth += g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

   y += height_text_small*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Set Capture Video Bitrate:");
   y += height_text*s_OSDStatsLineSpacing;

   char szTmp[64];
   szTmp[0] = 0;
   // To fix or remove
   /*
   str_format_bitrate(pVDS->uLastSetVideoBitrate & VIDEO_BITRATE_FIELD_MASK, szTmp);
   if ( pVDS->uLastSetVideoBitrate & VIDEO_BITRATE_FLAG_ADJUSTED )
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Auto %s", szTmp);
   else
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Initial: %s", szTmp);
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   */
   
   strcpy(szBuff, "Current target video bitrate/profile: 0 bps / NA");
   
   char szProfile[64];
   strcpy(szProfile, str_get_video_profile_name(g_SM_DevVideoBitrateHistory.history[0].uVideoProfileSwitches>>4));
   if ( 0 != (g_SM_DevVideoBitrateHistory.history[0].uVideoProfileSwitches & 0x0F) )
   {
      sprintf(szTmp, "-%d", (g_SM_DevVideoBitrateHistory.history[0].uVideoProfileSwitches & 0x0F) );
      strcat(szProfile, szTmp);
   }
   str_format_bitrate(g_SM_DevVideoBitrateHistory.uCurrentTargetVideoBitrate, szTmp);

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Current target bitrate/profile: %s / %s", szTmp, szProfile);
   
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;

   sprintf(szBuff, "Overflowing now at: %d", g_SM_DevVideoBitrateHistory.uQuantizationOverflowValue);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;
   
   strcpy(szBuff, "Auto Quantization: On");
   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION) )
      strcpy(szBuff, "Auto Quantization: Off");
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   //---------------------------------------------------------------------------
   // Second graph

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
   {   
      u32 uMaxQuant = 0;
      u32 uMinQuant = 1000;
      for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uTotalDataPoints; i++ )
      {
         if ( g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization == 0xFF )
            continue;
         if ( g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization > uMaxQuant )
            uMaxQuant = g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization;
         if ( g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization < uMinQuant )
            uMinQuant = g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization;
      }

      if ( uMaxQuant == uMinQuant || uMaxQuant == uMinQuant+1 )
      {
         //if ( ! ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
         //{
         //   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Auto Adjustments is Off.");
         //   return;
         //}
         uMaxQuant = uMinQuant + 2;
      }

      u32 uMaxDelta = uMaxQuant - uMinQuant;

      y += height_text*0.6;
      yBottomGraph = y + hGraph2;
      yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

      sprintf(szBuff, "%d", (int)uMaxQuant);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.6, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos + dxGraph+widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.6, s_idFontStatsSmall, szBuff);

      sprintf(szBuff, "%d", (int)(uMaxQuant+uMinQuant)/2);
      g_pRenderEngine->drawText(xPos, y+0.5*hGraph2-height_text_small*0.6, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+0.5*hGraph2-height_text_small*0.6, s_idFontStatsSmall, szBuff);

      sprintf(szBuff, "%d", (int)uMinQuant);
      g_pRenderEngine->drawText(xPos, y+hGraph2-height_text_small*0.6, s_idFontStatsSmall,szBuff);
      g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+hGraph2-height_text_small*0.6, s_idFontStatsSmall,szBuff);

      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      //g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      //g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      
      for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      {
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+0.5*hGraph2, xPos + dxGraph + i + 2.0*wPixel, y+0.5*hGraph2);
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2);
      }

      float hVideoQuantizationPrev = 0.0;
      xBarMiddle = xPos + dxGraph + widthBar*0.5 - g_pRenderEngine->getPixelWidth();

      g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9);
            
      g_pRenderEngine->setStrokeSize(2.0);

      for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uTotalDataPoints; i++ )
      {
         float hVideoQuantization = (g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization-uMinQuant)*hGraph2/uMaxDelta;
    
         if ( (g_SM_DevVideoBitrateHistory.history[i].uVideoQuantization == 0xFF) ||
              (i > 0 && g_SM_DevVideoBitrateHistory.history[i-1].uVideoQuantization == 0xFF ) )
         {
    
            g_pRenderEngine->setFill(100,100,200, s_fOSDStatsGraphLinesAlpha*0.6);
            g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9*0.6);   
            g_pRenderEngine->setStrokeSize(0.0);
         
            g_pRenderEngine->drawRect(xBarMiddle-widthBar*0.5, y+4.0*g_pRenderEngine->getPixelHeight(), widthBar, hGraph2-8.0*g_pRenderEngine->getPixelHeight());
    
            g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9);   
            g_pRenderEngine->setStrokeSize(2.0);
         }
         else if ( i != 0 )
            g_pRenderEngine->drawLine(xBarMiddle, y+hGraph2-hVideoQuantization, xBarMiddle+widthBar, y+hGraph2-hVideoQuantizationPrev);
    
         hVideoQuantizationPrev = hVideoQuantization;
         xBarMiddle += widthBar;
      }
   }
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();
}

void osd_render_video_frames_stats()
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStats);

   float width = 1.0 - 2.0 * osd_getMarginX();
   float height = height_text * 1.8;

   float xPos = osd_getMarginX();
   float yPos = 1.0 - height - osd_getMarginY();
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
      yPos -= osd_getBarHeight();
   if ( pActiveModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      yPos -= osd_getSecondBarHeight();

   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   //g_pRenderEngine->enableAlphaBlending();

   char szBuff[128];
   if ( ((g_SMControllerDebugVideoRTInfo.iComputedRxVideoFPS - pActiveModel->video_params.iVideoFPS) > 2) ||
        ((g_SMControllerDebugVideoRTInfo.iComputedRxVideoFPS - pActiveModel->video_params.iVideoFPS) < -2) )
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Video Frames, FPS: *%d", g_SMControllerDebugVideoRTInfo.iComputedRxVideoFPS);
   else
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Video Frames, FPS: %d", g_SMControllerDebugVideoRTInfo.iComputedRxVideoFPS);
   float fTextWidth = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency*0.7);
   g_pRenderEngine->drawRect(xPos, yPos, width, height);
   g_pRenderEngine->drawRect(xPos, yPos - height_text_small - 0.1*s_fOSDStatsMargin , fTextWidth + 1.0 * s_fOSDStatsMargin, height_text_small + 0.1*s_fOSDStatsMargin);
   osd_set_colors();

   g_pRenderEngine->drawText(xPos + 0.5*s_fOSDStatsMargin, yPos - height_text_small + 0.3 * s_fOSDStatsMargin, s_idFontStatsSmall, szBuff);

   float fYMargin = 0.6*s_fOSDStatsMargin;
   xPos += 0.8*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += fYMargin;
   width -= 1.6*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   height -= 2.0 * fYMargin;

   float widthMax = width;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();

   double cSecond[] = {255,255,255,1.0};

   float fWidthBar = widthMax/(float)SYSTEM_RT_INFO_INTERVALS_FRAMES;
   float xBar = xPos;
   u32 uTimeLastFrame = MAX_U32;

   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS_FRAMES; i++ )
   {
       if ( (uTimeLastFrame == MAX_U32) && g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i] != 0 )
          uTimeLastFrame = g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i];

       if ( g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i] != 0 )
       if ( g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i]/1000 !=uTimeLastFrame/1000 )
       {
          uTimeLastFrame = g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i];
          g_pRenderEngine->setColors(cSecond);
          g_pRenderEngine->drawLine(xBar - 3.0*wPixel, yPos - fYMargin + 2.0*hPixel, xBar - 3.0*wPixel, yPos + height + fYMargin - 2.0 * hPixel);
          g_pRenderEngine->drawLine(xBar - 2.0*wPixel, yPos - fYMargin + 2.0*hPixel, xBar - 2.0*wPixel, yPos + height + fYMargin - 2.0 * hPixel);
       }

       if ( i == g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex )
       {
          xBar += fWidthBar;
          continue;
       }

       if ( g_SMControllerDebugVideoRTInfo.uReceivedFrameStartTime[i] == 0 )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameMissing());
          //g_pRenderEngine->drawLine(xBar + 0.5*fWidthBar, yPos + 0.5*height, xBar + 0.5*fWidthBar, yPos + 0.5*height + 2.0*hPixel);
          //g_pRenderEngine->drawLine(xBar + 0.5*fWidthBar + wPixel, yPos + 0.5*height, xBar + 0.5*fWidthBar + wPixel, yPos + 0.5*height + 2.0*hPixel);
          g_pRenderEngine->drawRect(xBar + 1.0 * wPixel, yPos + 0.5 * height - 4.0*hPixel, fWidthBar - 4.0*wPixel, 8.0*hPixel);
          xBar += fWidthBar;
          continue;
       }

       int iCountNAL = 0;
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_I )
          iCountNAL++;
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_P )
          iCountNAL++;
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_O )
          iCountNAL++;

       if ( iCountNAL == 0 )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameMissing());
          g_pRenderEngine->drawRect(xBar, yPos, fWidthBar - 3.0*wPixel, height);
          xBar += fWidthBar;
          continue;
       }

       // Frame has retransmissions?
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramePackets[i] & 0x0000FF00 )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameRetr());
          g_pRenderEngine->drawRect(xBar, yPos, fWidthBar - 3.0*wPixel, height);
          xBar += fWidthBar;
          continue;
       }

       float ySemiBar = yPos;
       float fHeightSemiBar = height/(float)iCountNAL;
       if ( iCountNAL > 1 )
          fHeightSemiBar = (height - 3.0*hPixel*(iCountNAL-1))/(float)iCountNAL;

       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_I )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameI());
          if ( g_SMControllerDebugVideoRTInfo.uOutputFramePackets[i] & (((u32)0x01)<<24) )
             g_pRenderEngine->setColors(osdGetColorVideoFrameECMax());
          else if ( g_SMControllerDebugVideoRTInfo.uOutputFramePackets[i] & 0x00FF0000 )
             g_pRenderEngine->setColors(osdGetColorVideoFrameEC());
          g_pRenderEngine->drawRect(xBar, ySemiBar, fWidthBar - 3.0*wPixel, fHeightSemiBar);
          ySemiBar += fHeightSemiBar + 3.0 * hPixel;
       }
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_P )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameNormal());
          if ( g_SMControllerDebugVideoRTInfo.uOutputFramePackets[i] & (((u32)0x01)<<24) )
             g_pRenderEngine->setColors(osdGetColorVideoFrameECMax());
          else if ( g_SMControllerDebugVideoRTInfo.uOutputFramePackets[i] & 0x00FF0000 )
             g_pRenderEngine->setColors(osdGetColorVideoFrameEC());
          g_pRenderEngine->drawRect(xBar, ySemiBar, fWidthBar - 3.0*wPixel, fHeightSemiBar);
          ySemiBar += fHeightSemiBar + 3.0 * hPixel;
       }
       if ( g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[i] & VIDEO_STATUS_FLAGS2_IS_NAL_O )
       {
          g_pRenderEngine->setColors(osdGetColorVideoFrameO());
          g_pRenderEngine->drawRect(xBar, ySemiBar, fWidthBar - 3.0*wPixel, fHeightSemiBar);
          ySemiBar += fHeightSemiBar + 3.0 * hPixel;
       }
       xBar += fWidthBar;
   }

   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
   osd_set_colors();
}
