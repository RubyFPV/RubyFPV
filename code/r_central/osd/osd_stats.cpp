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
#include "../../base/config.h"
#include "../../base/shared_mem.h"
#include "../../base/video_capture_res.h"
#include "../../base/ctrl_settings.h"
#include "../../base/ctrl_interfaces.h"
#include "../../base/camera_utils.h"
#include "../../common/string_utils.h"
#include "../../radio/radiolink.h"

#include <ctype.h>
#include "../shared_vars.h"
#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "osd_common.h"
#include "osd.h"
#include "osd_stats_dev.h"
#include "osd_stats_video_bitrate.h"
#include "osd_stats_radio.h"
#include "osd_widgets.h"
#include "../local_stats.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../pairing.h"
#include "../timers.h"
#include "../ui_alarms.h"

float s_fOSDStatsGraphLinesAlpha = 0.9;
float s_fOSDStatsGraphBottomLinesAlpha = 0.6;
float s_OSDStatsLineSpacing = 1.0;
float s_fOSDStatsMargin = 0.016;
bool s_bDebugStatsShowAll = false;

static int s_iPeakTotalPacketsInBuffer = 0;
static int s_iPeakCountRender = 0;

static u32 s_uStatsRenderRCCount = 0;

u32 s_idFontStats = 0;
u32 s_idFontStatsSmall = 0;

#define SNAPSHOT_HISTORY_SIZE 3

static u32 s_uOSDSnapshotTakeTime = 0;
static u32 s_uOSDSnapshotCount = 0;
static u32 s_uOSDSnapshotLastDiscardedBuffers = 0;
static u32 s_uOSDSnapshotLastDiscardedSegments = 0;
static u32 s_uOSDSnapshotLastDiscardedPackets = 0;
static t_packet_header_ruby_telemetry_extended_extra_info_retransmissions s_Snapshot_PHRTE_Retransmissions;

static u32 s_uOSDSnapshotHistoryDiscardedBuffers[SNAPSHOT_HISTORY_SIZE];
static u32 s_uOSDSnapshotHistoryDiscardedSegments[SNAPSHOT_HISTORY_SIZE];
static u32 s_uOSDSnapshotHistoryDiscardedPackets[SNAPSHOT_HISTORY_SIZE];

static shared_mem_radio_stats s_OSDSnapshot_RadioStats;
static shared_mem_video_stream_stats_rx_processors s_OSDSnapshot_VideoDecodeStats;

#define MAX_OSD_COLUMNS 10

static int   s_iCountOSDStatsBoundingBoxes;
static float s_iOSDStatsBoundingBoxesX[50];
static float s_iOSDStatsBoundingBoxesY[50];
static float s_iOSDStatsBoundingBoxesW[50];
static float s_iOSDStatsBoundingBoxesH[50];
static int   s_iOSDStatsBoundingBoxesIds[50];
static int   s_iOSDStatsBoundingBoxesColumns[50];
static float s_fOSDStatsColumnsHeights[MAX_OSD_COLUMNS];
static float s_fOSDStatsColumnsWidths[MAX_OSD_COLUMNS];

static float s_fOSDStatsSpacingH = 0.0;
static float s_fOSDStatsSpacingV = 0.0;
static float s_fOSDStatsMarginHLeft = 0.0;
static float s_fOSDStatsMarginHRight = 0.0;
static float s_fOSDStatsMarginVBottom = 0.0;
static float s_fOSDStatsMarginVTop = 0.0;

static float s_fOSDStatsWindowsMinimBoxHeight = 0.0;
static float s_fOSDVideoDecodeWidthZoom = 1.0;


void _osd_stats_draw_line(float xLeft, float xRight, float y, u32 uFontId, const char* szTextLeft, const char* szTextRight)
{
   g_pRenderEngine->drawText(xLeft, y, uFontId, szTextLeft);
   if ( (NULL != szTextRight) && (0 != szTextRight[0]) )
      g_pRenderEngine->drawTextLeft(xRight, y, uFontId, szTextRight);
}

void osd_stats_init()
{
   s_uStatsRenderRCCount = 0;
}

// Return height
float osd_stats_render_video_stream_graph(float xPos, float yPos, float fWidth, float fHeightGraph, bool bIsMinimal, bool bIsCompact, bool bIsSnapshot)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = NULL;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uActiveVehicleId )
      {
         pVDS = &(g_SM_VideoDecodeStats.video_streams[i]);
         break;
      }
   }

   if ( (NULL == pActiveModel) || (0 == uActiveVehicleId) || (NULL == pVDS) )
      return 0.0;
   
   int iGraphIntervals = 1;
   int iRTValuesPerGraphInterval = 1;
   if ( (g_pControllerSettings->nGraphVideoRefreshInterval > 0) && (g_SMControllerRTInfo.uUpdateIntervalMs > 0) )
   {
      iGraphIntervals = (SYSTEM_RT_INFO_INTERVALS * g_SMControllerRTInfo.uUpdateIntervalMs) / g_pControllerSettings->nGraphVideoRefreshInterval;
      iRTValuesPerGraphInterval = g_pControllerSettings->nGraphVideoRefreshInterval / g_SMControllerRTInfo.uUpdateIntervalMs;
   }
   if ( 0 == iGraphIntervals )
      iGraphIntervals = 1;
   int iRTStartIndex = g_SMControllerRTInfo.iCurrentIndex - (g_SMControllerRTInfo.iCurrentIndex % iRTValuesPerGraphInterval) - 1;
   if ( iRTStartIndex < 0 )
      iRTStartIndex = SYSTEM_RT_INFO_INTERVALS-1;
   int iRTTotalUsedIntervals = iGraphIntervals * iRTValuesPerGraphInterval;

   int maxGraphValue = 4;
   int iRTIndex = iRTStartIndex;
   for(int i=0; i<iGraphIntervals; i++ )
   {
      float fSumPackets = 0.0;
      for(int k=0; k<iRTValuesPerGraphInterval; k++)
      {
         fSumPackets += g_SMControllerRTInfo.uOutputedVideoPackets[iRTIndex];
         iRTIndex--;
         if ( iRTIndex < 0 )
            iRTIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }
      if ( fSumPackets > maxGraphValue )
         maxGraphValue = fSumPackets;
   }

   char szBuff[128];

   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();
   float y = yPos;
   y += height_text_small*0.2;
   
   if ( ! bIsMinimal )
   {
      if ( bIsCompact )
         sprintf(szBuff,"%.1f sec, %d ms/bar", (float)iGraphIntervals * (float)g_pControllerSettings->nGraphVideoRefreshInterval / 1000.0, g_pControllerSettings->nGraphVideoRefreshInterval);
      else
         sprintf(szBuff,"%d ms/bar %d x %d ms resolution (%d-%d bars)", g_pControllerSettings->nGraphVideoRefreshInterval, SYSTEM_RT_INFO_INTERVALS, g_SMControllerRTInfo.uUpdateIntervalMs, iGraphIntervals, iRTValuesPerGraphInterval);
      
      if ( bIsSnapshot )
         g_pRenderEngine->drawTextLeft(xPos+fWidth, y, s_idFontStats, szBuff);
      else
         g_pRenderEngine->drawTextLeft(xPos+fWidth, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);

      y += height_text_small*1.0;
   }

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+fHeightGraph-height_text_small*0.5, s_idFontStatsSmall, "0");

   float dxGraph = 0.01;
   float fWidthGraph = fWidth - dxGraph;

   double pc[4];
   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + fWidth, y);         
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphBottomLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphBottomLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+fHeightGraph, xPos + dxGraph + fWidthGraph, y+fHeightGraph);
   float midLine = fHeightGraph/2.0;
   for( float i=0; i<=fWidthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   double colorECUsed[4] = {50,250,0, 1.0};
   double colorECMaxUsed[4] = {220,250,20, 1.0};
   double colorReqRetransmissions[4] = {100,100,255, 1.0};
   double colorRetransmitted[4] = {40,20,255,1.0};
   double colorDropped[4] = {255,50,50,1.0};

   g_pRenderEngine->setStrokeSize(0);

   float yBottomGraph = y + fHeightGraph;// - 1.0/g_pRenderEngine->getScreenHeight();

   float widthBar = fWidthGraph / iGraphIntervals;
   float fWidthBarRect = widthBar-2.0*wPixel;
   if ( fWidthBarRect < 2.0 * wPixel )
      fWidthBarRect = widthBar;

   float xBarSt = xPos + fWidth - g_pRenderEngine->getPixelWidth();
   float xBarMid = xBarSt + widthBar*0.5;
   float xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();

   controller_runtime_info_vehicle* pRTInfoActiveVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, uActiveVehicleId);

   iRTIndex = iRTStartIndex;
   for( int i=0; i<iGraphIntervals; i++ )
   {
      float fSumPackets = 0.0;
      float fSumECUsed = 0.0;
      float fSumRetransmitted = 0.0;
      float fSumDropped = 0.0;
      float hBar = 0.0;
      bool  bECUsedMax = false;
      int   iCountReqRetransmissions = 0;
      int   iCountDiscardedRetr = 0;

      for(int k=0; k<iRTValuesPerGraphInterval; k++)
      {
         if ( g_SMControllerRTInfo.uOutputedVideoBlocksMaxECUsed[iRTIndex] )
             bECUsedMax = true;
         fSumPackets += g_SMControllerRTInfo.uOutputedVideoPackets[iRTIndex];
         fSumECUsed += g_SMControllerRTInfo.uOutputedVideoBlocksSingleECUsed[iRTIndex];
         fSumECUsed += g_SMControllerRTInfo.uOutputedVideoBlocksTwoECUsed[iRTIndex];
         fSumECUsed += g_SMControllerRTInfo.uOutputedVideoBlocksMultipleECUsed[iRTIndex];
         fSumRetransmitted += g_SMControllerRTInfo.uOutputedVideoPacketsRetransmitted[iRTIndex];
         fSumDropped += g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[iRTIndex];
         iCountDiscardedRetr += g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[iRTIndex];
         if ( NULL != pRTInfoActiveVehicle )
            iCountReqRetransmissions += pRTInfoActiveVehicle->uCountReqRetransmissions[iRTIndex];

         iRTIndex--;
         if ( iRTIndex < 0 )
            iRTIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }

      xBarSt -= widthBar;
      xBarEnd -= widthBar;

      float percentTotal = (float)(fSumPackets)/(float)(maxGraphValue);
      if ( percentTotal > 1.0 )
         percentTotal = 1.0;
      float percentDropped = (float)(fSumDropped)/(float)(maxGraphValue);
      if ( percentDropped > 1.0 )
         percentDropped = 1.0;
      if ( (percentTotal > 0.001) && (fSumPackets > 0.001) )
      {
         hBar = (fHeightGraph-widthBar)*percentTotal;

         // Clean packets
         if ( (fSumRetransmitted < 1.0) && (fSumECUsed < 1.0) )
         {
            g_pRenderEngine->setColors(osdGetColorVideoFrameNormal());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar);
         }
         // EC packets only
         else if ( fSumRetransmitted < 1.0 )
         {
            float fPercentEC = fSumECUsed / fSumPackets;
            if ( fPercentEC < .25 )
               fPercentEC = .25;
            if ( hBar < 0.5 * fHeightGraph )
            if ( fPercentEC < 0.4 )
               fPercentEC = 0.4;
            if ( bECUsedMax )
               g_pRenderEngine->setColors(osdGetColorVideoFrameECMax());
            else
               g_pRenderEngine->setColors(osdGetColorVideoFrameNormal());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar * (1.0 - fPercentEC));
            g_pRenderEngine->setColors(osdGetColorVideoFrameEC());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar * fPercentEC, fWidthBarRect, hBar * fPercentEC);
         }
         // Retr packets only
         else if ( fSumECUsed < 1.0 )
         {
            float fPercentRetr = fSumRetransmitted / fSumPackets;
            if ( fPercentRetr < .25 )
               fPercentRetr = .25;
            if ( hBar < 0.5 * fHeightGraph )
            if ( fPercentRetr < 0.4 )
               fPercentRetr = 0.4;
            g_pRenderEngine->setColors(osdGetColorVideoFrameNormal());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar * (1.0 - fPercentRetr));
            g_pRenderEngine->setColors(osdGetColorVideoFrameRetr());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar * fPercentRetr, fWidthBarRect, hBar * fPercentRetr);
         }
         // Both EC and retr
         else
         {
            float fPercentEC = fSumECUsed / fSumPackets;
            if ( fPercentEC < .25 )
               fPercentEC = .25;
            float fPercentRetr = fSumRetransmitted / fSumPackets;
            if ( fPercentRetr < .25 )
               fPercentRetr = .25;
            if ( fPercentEC + fPercentRetr > 1.0 )
               fPercentEC = 1.0 - fPercentRetr;

            if ( bECUsedMax )
               g_pRenderEngine->setColors(osdGetColorVideoFrameECMax());
            else
               g_pRenderEngine->setColors(osdGetColorVideoFrameNormal());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar * (1.0 - fPercentEC - fPercentRetr));

            g_pRenderEngine->setColors(osdGetColorVideoFrameEC());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar * (fPercentEC + fPercentRetr), fWidthBarRect, hBar * fPercentEC);

            g_pRenderEngine->setColors(osdGetColorVideoFrameRetr());
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar * fPercentRetr, fWidthBarRect, hBar * fPercentRetr);
         }
      }

      if ( percentDropped > 0.0001 )
      {
         if ( percentDropped < 0.3 )
            percentDropped = 0.3;
         hBar = (fHeightGraph-widthBar)*percentDropped;
         g_pRenderEngine->setColors(osdGetColorVideoFrameMissing());
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar);
      }

      if ( iCountReqRetransmissions > 0 )
      {
         g_pRenderEngine->setColors(osdGetColorVideoFrameRetr());
         if ( iCountDiscardedRetr > 0 )
            g_pRenderEngine->setColors(osdGetColorVideoFrameRetrDiscard());
         g_pRenderEngine->fillCircle(xBarSt + 0.5 * widthBar, yBottomGraph - fHeightGraph + widthBar, widthBar*0.8);
      }

      if ( iCountDiscardedRetr > 0 )
      {
         float dy = 4.0*hPixel;
         g_pRenderEngine->setColors(osdGetColorVideoFrameRetrDiscard());
         g_pRenderEngine->drawLine(xBarSt - 2.0*wPixel, yBottomGraph - fHeightGraph - dy, xBarSt - 2.0*wPixel, yBottomGraph + dy);
         g_pRenderEngine->drawLine(xBarSt - wPixel, yBottomGraph - fHeightGraph - dy, xBarSt - wPixel, yBottomGraph + dy);
         g_pRenderEngine->drawLine(xBarSt, yBottomGraph - fHeightGraph - dy, xBarSt, yBottomGraph + dy);
      }
   }
   osd_set_colors();
   y += fHeightGraph;
   return (y-yPos);
}

void osd_stats_video_decode_snapshot_update(int iDeveloperMode, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_stream_stats* pVDS, shared_mem_video_stream_stats_rx_processors* pSM_VideoStats)
{
   if ( g_bHasVideoDecodeStatsSnapshot && ( s_uOSDSnapshotTakeTime > 1 ) && (g_pControllerSettings->iVideoDecodeStatsSnapshotClosesOnTimeout == 1) )
   if ( g_TimeNow > s_uOSDSnapshotTakeTime + 10000 )
   {
      g_bHasVideoDecodeStatsSnapshot = false;
      s_uOSDSnapshotTakeTime = 1;
   }

   // Initialize for the first time
   if ( 0 == s_uOSDSnapshotTakeTime )
   {
      s_uOSDSnapshotTakeTime = 1;
      s_uOSDSnapshotCount = 0;

      // To fix or remove
      //s_uOSDSnapshotLastDiscardedBuffers = pVDS->total_DiscardedBuffers;
      //s_uOSDSnapshotLastDiscardedSegments = pVDS->total_DiscardedSegments;
      //s_uOSDSnapshotLastDiscardedPackets = pVDS->total_DiscardedLostPackets;

      for( int i=0; i<SNAPSHOT_HISTORY_SIZE; i++ )
      {
         s_uOSDSnapshotHistoryDiscardedBuffers[i] = 0;
         s_uOSDSnapshotHistoryDiscardedSegments[i] = 0;
         s_uOSDSnapshotHistoryDiscardedPackets[i] = 0;
      }
   }

   bool bMustTakeSnapshot = false;
   // To fix or remove
   /*
   if ( g_TimeNow > g_RouterIsReadyTimestamp + 5000 )
   if ( s_uOSDSnapshotLastDiscardedBuffers != pVDS->total_DiscardedBuffers ||
        s_uOSDSnapshotLastDiscardedSegments != pVDS->total_DiscardedSegments ||
        s_uOSDSnapshotLastDiscardedPackets != pVDS->total_DiscardedLostPackets )
      bMustTakeSnapshot = true;
   
   bool bHasActivity = false;
   bool bHasSilenceAtStart = true;
   for ( int i=0; i<12; i++ )
   {
      if ( i >= MAX_HISTORY_VIDEO_INTERVALS )
         break;
      if ( i < 5 )
      if ( (pCRS->history[i].uCountRequestedRetransmissions > 0) ||
           (pCRS->history[i].uCountRequestedPacketsForRetransmission > 0) ||
           (pVDSH->missingTotalPacketsAtPeriod[i] > 0) )
         bHasSilenceAtStart = false;

      if ( i >= 5 )
      if ( (pCRS->history[i].uCountRequestedRetransmissions > 0) ||
           (pCRS->history[i].uCountRequestedPacketsForRetransmission > 0) ||
           (pVDSH->missingTotalPacketsAtPeriod[i] > 0) )
         bHasActivity = true;

      if ( i > 5 )
      if ( bHasActivity && bHasSilenceAtStart )
      {
         bMustTakeSnapshot = true;
         break;
      }
   }

   if ( bMustTakeSnapshot )
   {
      bool bTakeSnapshot = false;
      if ( !g_bHasVideoDecodeStatsSnapshot )
         bTakeSnapshot = true;
      else if ( ( g_pControllerSettings->iVideoDecodeStatsSnapshotClosesOnTimeout == 2 ) && (g_TimeNow > s_uOSDSnapshotTakeTime+500) )
         bTakeSnapshot = true;
      if ( bTakeSnapshot )
      {
         s_uOSDSnapshotTakeTime = g_TimeNow;
         s_uOSDSnapshotCount++;
         g_bHasVideoDecodeStatsSnapshot = true;

         for( int i=SNAPSHOT_HISTORY_SIZE-1; i>0; i-- )
         {
            s_uOSDSnapshotHistoryDiscardedBuffers[i] = s_uOSDSnapshotHistoryDiscardedBuffers[i-1];
            s_uOSDSnapshotHistoryDiscardedSegments[i] = s_uOSDSnapshotHistoryDiscardedSegments[i-1];
            s_uOSDSnapshotHistoryDiscardedPackets[i] = s_uOSDSnapshotHistoryDiscardedPackets[i-1];
         }

         // To fix or remove
         //s_uOSDSnapshotHistoryDiscardedBuffers[0] = pVDS->total_DiscardedBuffers - s_uOSDSnapshotLastDiscardedBuffers;
         //s_uOSDSnapshotHistoryDiscardedSegments[0] = pVDS->total_DiscardedSegments - s_uOSDSnapshotLastDiscardedSegments;
         //s_uOSDSnapshotHistoryDiscardedPackets[0] = pVDS->total_DiscardedLostPackets - s_uOSDSnapshotLastDiscardedPackets;

         //s_uOSDSnapshotLastDiscardedBuffers = pVDS->total_DiscardedBuffers;
         //s_uOSDSnapshotLastDiscardedSegments = pVDS->total_DiscardedSegments;
         //s_uOSDSnapshotLastDiscardedPackets = pVDS->total_DiscardedLostPackets;

         memcpy( &s_OSDSnapshot_RadioStats, pSM_RadioStats, sizeof(shared_mem_radio_stats));
         memcpy( &s_OSDSnapshot_VideoDecodeStats, pSM_VideoStats, sizeof(shared_mem_video_stream_stats_rx_processors));
         memcpy( &s_OSDSnapshot_VideoDecodeHist, pSM_VideoHistoryStats, sizeof(shared_mem_video_stream_stats_history_rx_processors));
         memcpy( &s_Snapshot_PHRTE_Retransmissions, &(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfoRetransmissions), sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }
   */
}

float osd_render_stats_video_decode_get_height(int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_stream_stats_rx_processors* pSM_VideoStats, float scale)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   if ( NULL == pActiveModel )
      return 0.0;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = 3.0*height_text;
   float hGraphHistory = 2.5*height_text;
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   bool bIsMinimal = false;
   bool bIsCompact = false;
   bool bIsNormal = false;
   bool bIsExtended = false;

   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS )
      bIsMinimal = true;
   else if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS )
      bIsCompact = true;
   else if ( pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS )
      bIsExtended = true;
   else
      bIsNormal = true;
   
   // Stream info
   height += height_text*s_OSDStatsLineSpacing;

   // Stream info 2
   if ( bIsCompact || bIsNormal || bIsExtended )
      height += height_text_small*s_OSDStatsLineSpacing;

   // Stream info 3 (H264 profile/level)
   if ( bIsExtended )
   {
      height += height_text_small*s_OSDStatsLineSpacing;
      if ( iDeveloperMode )
         height += height_text_small*s_OSDStatsLineSpacing;
   }

   // Retr, adaptive
   if ( bIsNormal || bIsExtended )
   {
      height += 2.0*height_text*s_OSDStatsLineSpacing;
      // Retr count
      if ( iDeveloperMode )
         height += height_text*s_OSDStatsLineSpacing;
      // Adaptive dev
      if ( bIsExtended && iDeveloperMode )
         height += 7 * height_text_small*s_OSDStatsLineSpacing;
   }

   // Audio
   if ( bIsExtended )
   if ( pActiveModel->isAudioCapableAndEnabled() )
      height += height_text*s_OSDStatsLineSpacing;

   // Bar for video blocks pressent in video rx buffers
   if ( iDeveloperMode && bIsExtended )
      height += height_text*s_OSDStatsLineSpacing;

   // Graph
   if ( ! bIsMinimal )
      height += 1.0*height_text_small;
   height += 0.5*height_text_small;
   height += hGraphHistory;

   return height;
}

float osd_render_stats_video_decode_get_width(int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_stream_stats_rx_processors* pSM_VideoStats, float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
   {
      if ( (!bIsSnapshot) && (!(osd_get_current_data_source_vehicle_model()->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS)) )
         return g_fOSDStatsForcePanelWidth*s_fOSDVideoDecodeWidthZoom;
      else
         return g_fOSDStatsForcePanelWidth;
   }
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA A");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( NULL != osd_get_current_data_source_vehicle_model() && ((osd_get_current_data_source_vehicle_model()->osd_params.osd_flags[osd_get_current_layout_index()]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS) )
      width *= 1.55;
   return width;
}

float osd_render_stats_video_decode(float xPos, float yPos, int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_stream_stats_rx_processors* pSM_VideoStats, float scale)
{
   if ( NULL == osd_get_current_data_source_vehicle_model() || NULL == pSM_RadioStats || NULL == pSM_VideoStats )
      return 0.0;

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();

   if ( NULL == pActiveModel )
      return 0.0;

   shared_mem_video_stream_stats* pVDS = NULL;
   
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( pSM_VideoStats->video_streams[i].uVehicleId == uActiveVehicleId )
      {
         pVDS = &(pSM_VideoStats->video_streams[i]);
         break;
      }
   }

   if ( (NULL == pVDS) || (NULL == pActiveModel) )
      return 0.0;

   int iIndexRouterRuntimeInfo = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexRouterRuntimeInfo = i;
   }
   if ( -1 == iIndexRouterRuntimeInfo )
       return 0.0;

   t_structure_vehicle_info* pVehicleRTInfo = get_vehicle_runtime_info_for_vehicle_id(uActiveVehicleId);
   if ( NULL == pVehicleRTInfo )
      return 0.0;

   bool bIsMinimal = false;
   bool bIsCompact = false;
   bool bIsNormal = false;
   bool bIsExtended = false;

   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS )
      bIsMinimal = true;
   else if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS )
      bIsCompact = true;
   else if ( pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS )
      bIsExtended = true;
   else
      bIsNormal = true;

   if ( ! bIsSnapshot )
      osd_stats_video_decode_snapshot_update(iDeveloperMode, pSM_RadioStats, pVDS, pSM_VideoStats);

   float fReceivedMaxVideoMbps = pSM_RadioStats->radio_streams[0][STREAM_ID_VIDEO_1].rxBytesPerSec*8/1000.0/1000.0;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( pSM_RadioStats->radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId == pActiveModel->uVehicleId )
      {
         fReceivedMaxVideoMbps = pSM_RadioStats->radio_streams[i][STREAM_ID_VIDEO_1].rxBytesPerSec*8/1000.0/1000.0;
         break;
      }
   }

   float fReceivedVideoMbps = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_video_bitrate_bps/1000.0/1000.0;

   s_iPeakCountRender++;
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = 3.0*height_text;
   float hGraphHistory = 2.5*height_text;

   float width = osd_render_stats_video_decode_get_width(iDeveloperMode, bIsSnapshot, pSM_RadioStats, pSM_VideoStats, scale);
   float height = osd_render_stats_video_decode_get_height(iDeveloperMode, bIsSnapshot, pSM_RadioStats, pSM_VideoStats, scale);
   float wPixel = g_pRenderEngine->getPixelWidth();

   char szBuff[64];
   char szBuff2[64];
   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;
   
   float y = yPos;

   char szCurrentProfile[32];
   szCurrentProfile[0] = 0;
   strcpy(szCurrentProfile, str_get_video_profile_name(pVDS->PHVS.uCurrentVideoLinkProfile));
   if ( pVDS->PHVS.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE )
      strcat(szCurrentProfile, "-");
   if ( pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
      strcat(szCurrentProfile, "-1Way");

   // -------------------------
   // Begin - Title

   if ( bIsSnapshot )
   {
      sprintf(szBuff, "Snapshot %d", s_uOSDSnapshotCount);
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
   }
   else
   {
      //if ( ! (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS) )
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Video Stream");
   
      u32 uMaxVideoRadioBitrate = pActiveModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();

      sprintf(szBuff, "%s %.1f Mbs", szCurrentProfile, fReceivedMaxVideoMbps);

      if ( fReceivedVideoMbps*1000*1000 >= uMaxVideoRadioBitrate )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      
      if ( g_bHasVideoDataOverloadAlarm && (g_TimeLastVideoDataOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoDataOverloadAlarm + 5000) )
         g_pRenderEngine->setColors(get_Color_IconError());

      if ( ! (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS) )
      {
         osd_show_value_left(xPos+width,yPos, szBuff, s_idFontStats);
         //y += height_text*1.3*s_OSDStatsLineSpacing;
      }
      else
         osd_show_value_left(xPos+width,yPos, szBuff, s_idFontStatsSmall);
   }

   y += height_text*s_OSDStatsLineSpacing + height_text*0.4;

   // End - Title
   // ---------------------------------------------------------

   osd_set_colors();


   // -----------------------------------------------------
   // Stream Info

   u32 uVehicleIdVideo = 0;
   if ( (osd_get_current_data_source_vehicle_index() >= 0) && (osd_get_current_data_source_vehicle_index() < MAX_CONCURENT_VEHICLES) )
      uVehicleIdVideo = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uVehicleId;
   if ( link_has_received_videostream(uVehicleIdVideo) )
   {
      char szVideoType[64];
      strcpy(szVideoType, "N/A");
      if (((pVDS->PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
         strcpy(szVideoType, "H265");
      else if (((pVDS->PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H264 )
         strcpy(szVideoType, "H264");

      if ( ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) > 2) ||
           ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) < -2) )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s FPS: *%d (%d-%d)", szVideoType, getOptionVideoResolutionName(pVDS->iCurrentVideoWidth, pVDS->iCurrentVideoHeight), pVDS->iCurrentVideoTxSourceFPS, pVDS->iDetectedFPS, pVDS->iDetectedSlices);
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s FPS: %d (%d-%d)", szVideoType, getOptionVideoResolutionName(pVDS->iCurrentVideoWidth, pVDS->iCurrentVideoHeight), pVDS->iCurrentVideoTxSourceFPS, pVDS->iDetectedFPS, pVDS->iDetectedSlices);

      g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
      float wtmp = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);

      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "KF: %d (%d) ms ", pVDS->PHVS.uCurrentVideoKeyframeIntervalMs, pVDS->iDetectedKeyframeMs);
      if ( pActiveModel->isVideoLinkFixedOneWay() )
         strcat(szBuff, "1Way");
      else if ( pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
         strcat(szBuff, "Auto");
      else
         strcat(szBuff, "Fixed");

      static int sl_iLastKeyframeValue = 0;
      static u32 sl_uLastKeyframeValueChange = 0;
      if ( pVDS->PHVS.uCurrentVideoKeyframeIntervalMs != sl_iLastKeyframeValue )
      {
         sl_iLastKeyframeValue = pVDS->PHVS.uCurrentVideoKeyframeIntervalMs;
         sl_uLastKeyframeValueChange = g_TimeNow;
      }

      if ( (g_TimeNow < sl_uLastKeyframeValueChange+1500) && ((g_TimeNow/g_uOSDElementChangeBlinkInterval)%2) )
         osd_set_colors_text(get_Color_OSDElementChanged());
      else
         osd_set_colors();

      g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStatsSmall, szBuff);
      osd_set_colors();
   }
   else
   {
      sprintf(szBuff, "Stream: N/A");
      g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
   }
   y += height_text_small*s_OSDStatsLineSpacing;

   u32 videoBitrate = pActiveModel->video_link_profiles[pVDS->PHVS.uCurrentVideoLinkProfile].uTargetVideoBitrateBPS;
   
   // --------------------------------------------------
   // Stream info 2

   if ( bIsCompact || bIsNormal || bIsExtended )
   {
      static u32 s_uTimeLastECSchemeChangedTime = 0;
      static int s_iLastECSchemeReceivedData = 0;
      static int s_iLastECSchemeReceivedEC = 0;

      u32 uECSpreadHigh = (pVDS->uCurrentVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
      u32 uECSpreadLow = (pVDS->uCurrentVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
      u32 uECSpread = uECSpreadLow | (uECSpreadHigh<<1);
      
      u16 uCurrentECScheme = g_SM_RouterVehiclesRuntimeInfo.uCurrentAdaptiveECScheme[iIndexRouterRuntimeInfo];
      if ( (0 == uCurrentECScheme) || (0xFFFF == uCurrentECScheme) )
        uCurrentECScheme = (pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iBlockDataPackets << 8) | (pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iBlockECs & 0xFF);


      if ( (s_iLastECSchemeReceivedData != ((uCurrentECScheme>>8) & 0xFF)) || (s_iLastECSchemeReceivedEC != (uCurrentECScheme & 0xFF)) )
      {
         s_uTimeLastECSchemeChangedTime = g_TimeNow;
         s_iLastECSchemeReceivedData = (uCurrentECScheme>>8) & 0xFF;
         s_iLastECSchemeReceivedEC = uCurrentECScheme & 0xFF;
      }

      /*
      if ( (s_iLastECSchemeReceivedData != pVDS->PHVS.uCurrentBlockDataPackets) || (s_iLastECSchemeReceivedEC != pVDS->PHVS.uCurrentBlockECPackets) )
      {
         s_uTimeLastECSchemeChangedTime = g_TimeNow;
         s_iLastECSchemeReceivedData = pVDS->PHVS.uCurrentBlockDataPackets;
         s_iLastECSchemeReceivedEC = pVDS->PHVS.uCurrentBlockECPackets;
      }
      */

      bool bECChanged = false;
      if ( g_bOSDElementChangeNotification )
      if ( (pActiveModel->osd_params.osd_flags3[pActiveModel->osd_params.iCurrentOSDScreen] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS) || g_pControllerSettings->iDeveloperMode )
      if ( g_TimeNow < s_uTimeLastECSchemeChangedTime + g_uOSDElementChangeTimeout )
         bECChanged = true;

      char szTmpScheme[64];
      szTmpScheme[0] = 0;

      if ( (((uCurrentECScheme >> 8) & 0xFF) != pVDS->PHVS.uCurrentBlockDataPackets) ||
           ((uCurrentECScheme & 0xFF) != pVDS->PHVS.uCurrentBlockECPackets) )
         sprintf(szTmpScheme, " (%d/%d)", pVDS->PHVS.uCurrentBlockDataPackets, pVDS->PHVS.uCurrentBlockECPackets);
      szBuff2[0] = 0;
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "EC: %s%s %d/%d%s / %u / %d", szCurrentProfile,
         (pActiveModel->video_link_profiles[pVDS->PHVS.uCurrentVideoLinkProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME)?" (A) ":"",
         ((uCurrentECScheme >> 8) & 0xFF), (uCurrentECScheme & 0xFF), szTmpScheme,
         uECSpread, pVDS->PHVS.uCurrentBlockPacketSize);

      if ( bECChanged && g_bOSDElementChangeNotification )
      if ( (pActiveModel->osd_params.osd_flags3[pActiveModel->osd_params.iCurrentOSDScreen] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS) || g_pControllerSettings->iDeveloperMode )
         osd_set_colors_text(get_Color_OSDElementChanged());

      if ( (! bECChanged) || (g_TimeNow >= s_uTimeLastECSchemeChangedTime + g_uOSDElementChangeTimeout/2) ||
          (bECChanged && ((g_TimeNow/g_uOSDElementChangeBlinkInterval)%2)))
         g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
      if ( bECChanged )
         osd_set_colors();
      y += height_text_small*s_OSDStatsLineSpacing;
   }
  
   // Stream info 2
   // --------------------------------------------------


   // --------------------------------------------------
   // Stream info 3 (H264 profile/level)

   if ( bIsExtended )
   {
      if (((pVDS->PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
         strcpy(szBuff, "H265 Profile & Level:");
      else if (((pVDS->PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H264 )
         strcpy(szBuff, "H264 Profile & Level:");
      else
         strcpy(szBuff, "UKN Profile & Level:");

      /*
      char szTmp[16];
      szTmp[0] = 0;
      for( int i=7; i>=0; i--)
      {
         if ( pVDS->uDetectedH264ProfileConstrains & (1<<i) )
            strcat(szTmp, "1");
         else
            strcat(szTmp, "0");
         if ( i == 4 )
            strcat(szTmp, "-");
      }
      */
      snprintf(szBuff2, sizeof(szBuff2)/sizeof(szBuff2[0]), "(0x%02X 0x%02X) %s %d.%d", pVDS->uDetectedH264Profile, pVDS->uDetectedH264ProfileConstrains, str_get_decode_h264_profile_name(pVDS->uDetectedH264Profile, pVDS->uDetectedH264ProfileConstrains, pVDS->uDetectedH264Level), pVDS->uDetectedH264Level/10, pVDS->uDetectedH264Level%10);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, szBuff, szBuff2);
      y += height_text_small*s_OSDStatsLineSpacing;

      if ( iDeveloperMode )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%u (%u)", g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks, g_uTotalLocalAlarmDevRetransmissions);
         g_pRenderEngine->setColors(get_Color_Dev());
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Dropped video blocks (total alarms):"), szBuff);
         osd_set_colors();
         y += height_text_small*s_OSDStatsLineSpacing;
      }
   }
  
   // Stream info 3
   // --------------------------------------------------

   // --------------------------------------
   // Stream info Retr, adaptive info

   if ( bIsNormal || bIsExtended )
   {
      strcpy(szBuff, "Retransmissions: ");
      float wtmp = g_pRenderEngine->textWidth(s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);

      bool bRetransmissionsIsOn = false;
      if ( g_SM_RouterVehiclesRuntimeInfo.bIsDoingRetransmissions[iIndexRouterRuntimeInfo] )
         bRetransmissionsIsOn = true;
      if ( g_bAdaptiveVideoIsPaused )
         bRetransmissionsIsOn = false;
      if ( bRetransmissionsIsOn )
      {
         char szBuff3[64];
         strcpy(szBuff3, L("On"));
         g_pRenderEngine->setColors(get_Color_IconSucces());
         
         g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, szBuff3);
         wtmp += g_pRenderEngine->textWidth(s_idFontStats, szBuff3);
         osd_set_colors();

         sprintf(szBuff3, " Max %d ms", 5*(((pVDS->uCurrentVideoProfileEncodingFlags) & 0xFF00) >> 8));
         g_pRenderEngine->drawText(xPos + wtmp, y + 0.4*(height_text-height_text_small), s_idFontStatsSmall, szBuff3);
      }
      else
      {
         char szBuff3[64];
         strcpy(szBuff3, L("Off"));
         if ( g_bAdaptiveVideoIsPaused )
            strcpy(szBuff3, L("Off (A)"));
         g_pRenderEngine->setColors(get_Color_IconWarning());
         g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, szBuff3);
         osd_set_colors();
      }
      y += height_text*s_OSDStatsLineSpacing; // line 1

      if ( iDeveloperMode )
      {
         strcpy(szBuff, "N/A");
         controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, uActiveVehicleId);
         if ( NULL != pRTInfoVehicle )
         {
            int iCountRetr = 0;
            int iCountPackets = 0;
            float fMultiply = 1.0;
            int iSlices = 1000/g_SMControllerRTInfo.uUpdateIntervalMs;
            if ( iSlices > SYSTEM_RT_INFO_INTERVALS )
            {
               iSlices = 500/g_SMControllerRTInfo.uUpdateIntervalMs;
               fMultiply = 2.0;
            }
            if ( iSlices > SYSTEM_RT_INFO_INTERVALS )
            {
               iSlices = SYSTEM_RT_INFO_INTERVALS;
               fMultiply = 1000.0/(float)(g_SMControllerRTInfo.uUpdateIntervalMs * SYSTEM_RT_INFO_INTERVALS);
            }
            int iIndex = g_SMControllerRTInfo.iCurrentIndex;
            for( int i=0; i<iSlices; i++ )
            {
               iCountRetr += pRTInfoVehicle->uCountReqRetransmissions[iIndex];
               iCountPackets += pRTInfoVehicle->uCountReqRetrPackets[iIndex];
               iIndex--;
               if ( iIndex < 0 )
                  iIndex = SYSTEM_RT_INFO_INTERVALS-1;
            }
            sprintf(szBuff, "%d %d/sec", (int)(fMultiply * (float)iCountRetr), (int)(fMultiply * (float)iCountPackets));
         }
         g_pRenderEngine->setColors(get_Color_Dev());
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, L("Retr req / packets:"), szBuff);
         osd_set_colors();     
         y += height_text*s_OSDStatsLineSpacing;
      }

      strcpy(szBuff, "Adaptive Video: ");
      wtmp = g_pRenderEngine->textWidth(s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);

      bool bIsOff = false;
      if ( pActiveModel->isVideoLinkFixedOneWay() ||
           ( !(pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) ) )
         bIsOff = true;
      if ( pActiveModel->isAllVideoLinksFixedRate() )
         bIsOff = true;

      if ( bIsOff )
      {
         g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( pActiveModel->isAllVideoLinksFixedRate() )
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Off (Fixed radio rates)"));
         else
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Off"));
         osd_set_colors();
      }
      else if ( pVehicleRTInfo->bIsAdaptiveVideoPaused || g_bAdaptiveVideoIsPaused )
      {
         g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( g_bAdaptiveVideoIsPaused )
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Paused All"));
         else if ( pVehicleRTInfo->bIsAdaptiveVideoPaused )
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Paused"));
         osd_set_colors();

      }
      else if ( g_SM_RouterVehiclesRuntimeInfo.bIsAdaptiveVideoActive[iIndexRouterRuntimeInfo] )
      {
         char szBuff3[64];
         sprintf(szBuff3, "%s (%d)", L("On"), pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength);
         g_pRenderEngine->setColors(get_Color_IconSucces());
         g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, szBuff3);
         wtmp += g_pRenderEngine->textWidth(s_idFontStats, szBuff3);
         osd_set_colors();

         sprintf(szBuff3, " Lvl now: %d", pVDS->iAdaptiveVideoLevelNow );
         g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, szBuff3);
      }
      else
      {
         g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( ! g_SM_RouterVehiclesRuntimeInfo.bIsAdaptiveVideoActive[iIndexRouterRuntimeInfo] )
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Not active"));
         else
            g_pRenderEngine->drawText(xPos + wtmp, y, s_idFontStats, L("Off N/A"));
         osd_set_colors();
      }

      y += height_text*s_OSDStatsLineSpacing; // line 2

      // Adaptive dev info
      if ( iDeveloperMode && bIsExtended )
      {
         g_pRenderEngine->setColors(get_Color_Dev());

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitVideoLost, pVDS->adaptiveHitsHigh.iCountHitVideoLost);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Hits low/high on video lost frame:", szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitRSSI, pVDS->adaptiveHitsHigh.iCountHitRSSI);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Hits low/high on RSSI:", szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitSNR, pVDS->adaptiveHitsHigh.iCountHitSNR);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Hits low/high on SNR:"), szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitRxLost, pVDS->adaptiveHitsHigh.iCountHitRxLost);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Hits low/high on Rx lost:"), szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitRetr, pVDS->adaptiveHitsHigh.iCountHitRetr);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Hits low/high on retr:"), szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitECUsed, pVDS->adaptiveHitsHigh.iCountHitECUsed);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Hits low/high on EC used:"), szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         sprintf(szBuff, "%d / %d", pVDS->adaptiveHitsLow.iCountHitECMax, pVDS->adaptiveHitsHigh.iCountHitECMax);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Hits low/high on EC max:"), szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;

         osd_set_colors();
      }
   }
   // Stream info Retr, adaptive info
   // --------------------------------------

   // Audio
   if ( bIsExtended )
   if ( pActiveModel->isAudioCapableAndEnabled() )
   {
      strcpy(szBuff, "N/A");
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( pSM_RadioStats->radio_streams[i][STREAM_ID_AUDIO].uVehicleId == pActiveModel->uVehicleId )
         {
            sprintf(szBuff, "%u kbps", (pSM_RadioStats->radio_streams[i][STREAM_ID_AUDIO].rxBytesPerSec*8)/1000);
            break;
         }
      }
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Audio:"), szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }

   // Bar for video blocks pressent in video rx buffers
   if ( iDeveloperMode && bIsExtended )
   {
      static u32 s_uTimeLastMaxVideoBlocksInBuffersUpdate = 0;
      static int s_iMaxVideoBlocksInBuffers = 0;
      if ( (g_TimeNow > s_uTimeLastMaxVideoBlocksInBuffersUpdate + 10000) && (s_iMaxVideoBlocksInBuffers > 0) )
         s_iMaxVideoBlocksInBuffers--;
      strcpy(szBuff, "N/A");
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, uActiveVehicleId);
      if ( NULL != pRTInfoVehicle )
      {
         if ( pRTInfoVehicle->iCountBlocksInVideoRxBuffers > s_iMaxVideoBlocksInBuffers )
         {
            s_iMaxVideoBlocksInBuffers = pRTInfoVehicle->iCountBlocksInVideoRxBuffers;
            s_uTimeLastMaxVideoBlocksInBuffersUpdate = g_TimeNow;
         }
         sprintf(szBuff, "%d (max %d)", pRTInfoVehicle->iCountBlocksInVideoRxBuffers, s_iMaxVideoBlocksInBuffers);
      }
      g_pRenderEngine->setColors(get_Color_Dev());
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, L("Video Buffers Use:"), szBuff);
      osd_set_colors();     
      y += height_text*s_OSDStatsLineSpacing;
   }

   y += osd_stats_render_video_stream_graph(xPos, y, widthMax, hGraphHistory, bIsMinimal, bIsCompact, bIsSnapshot);
   y += height_text_small*0.3;
   osd_set_colors();

   // End history buffer
   //----------------------------------------------------------------

/*
   // Max EC packets used

   if ( pActiveModel->bDeveloperMode || s_bDebugStatsShowAll )
   if ( bIsExtended )
   {
      g_pRenderEngine->setColors(get_Color_Dev());
      float maxValue = 1.001;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pVDSH->outputHistoryMaxECPacketsUsedPerPeriod[i] > maxValue )
            maxValue = pVDSH->outputHistoryMaxECPacketsUsedPerPeriod[i];
      }

      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStatsSmall, "Max EC packets used");
      y += height_text_small*1.0;

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph*0.6-height_text_small*0.5, s_idFontStatsSmall, "0");

      pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.6, xPos + dxGraph + width, y+hGraph*0.6);
      midLine = hGraph*0.6/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      float hPixel = g_pRenderEngine->getPixelHeight();
      yBottomGraph = y + hGraph*0.6 - hPixel;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         float fPercent = (float)pVDSH->outputHistoryMaxECPacketsUsedPerPeriod[i]/maxValue;
         if ( pVDSH->outputHistoryMaxECPacketsUsedPerPeriod[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*0.6*fPercent + hPixel, widthBar-wPixel, hGraph*0.6*fPercent);
         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;
      }
      y += hGraph*0.6;
      y += height_text_small*0.2;
   }

   // Max EC packets used
   //---------------------------------------------

   // ----------------------------------------------------
   // History packets max gap graph

   if ( iDeveloperMode )
   if ( bIsExtended )
   {
      g_pRenderEngine->setColors(get_Color_Dev());

      g_pRenderEngine->drawTextLeft(xPos+widthMax, y, s_idFontStatsSmall, "Packets Max Gap");
      y += height_text_small*1.0;
      int maxGraphValue = 4;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pVDSH->outputHistoryBlocksMaxPacketsGapPerPeriod[i] > maxGraphValue )
            maxGraphValue = pVDSH->outputHistoryBlocksMaxPacketsGapPerPeriod[i];
      }

      sprintf(szBuff, "%d", maxGraphValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText( xPos, y+hGraph*0.6-height_text_small*0.5, s_idFontStatsSmall, "0");

      double* pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.6, xPos + dxGraph + width, y+hGraph*0.6);
      float midLine = hGraph*0.6/2.0;
   
      for( float i=0.0; i<=width-2.0*wPixel; i+= 5.0*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);
      float hBarGap = 0.0;
      yBottomGraph = y + hGraph*0.6 - 1.0/g_pRenderEngine->getScreenHeight();

      g_pRenderEngine->setFill(200,50,50, s_fOSDStatsGraphLinesAlpha);

      for( int i=0; i<totalHistoryValues; i++ )
      {
         hBarGap = 0;
         u8 val = pVDSH->outputHistoryBlocksMaxPacketsGapPerPeriod[i]; 
         if ( val > 0 )
         {
            hBarGap = hGraph*0.6 * (0.1 + 0.9 * val/(float)maxGraphValue);
            if ( hBarGap > hGraph*0.6 )
               hBarGap = hGraph*0.6;
            g_pRenderEngine->drawRect(xPos+widthMax-(i+1)*widthBar, yBottomGraph-hBarGap, widthBar-wPixel, hBarGap);
         }
      }

      y += hGraph*0.6;
      y += height_text_small*0.2;
   }

   // History packets max gap graph
   // ----------------------------------------------------

   // -----------------------------------------
   // Pending good blocks to output graph

   if ( pActiveModel->bDeveloperMode || s_bDebugStatsShowAll )
   if ( bIsExtended )
   {
      g_pRenderEngine->setColors(get_Color_Dev());
      float maxValue = 1.0;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pVDSH->outputHistoryMaxGoodBlocksPendingPerPeriod[i] > maxValue )
            maxValue = pVDSH->outputHistoryMaxGoodBlocksPendingPerPeriod[i];
      }

      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStatsSmall, "Max good blocks pending output");
      y += height_text_small;

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph*0.8-height_text_small*0.5, s_idFontStatsSmall, "0");

      pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.8, xPos + dxGraph + width, y+hGraph*0.8);
      midLine = hGraph*0.8/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      float hPixel = g_pRenderEngine->getPixelHeight();
      yBottomGraph = y + hGraph*0.8 - hPixel;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         float fPercent = (float)pVDSH->outputHistoryMaxGoodBlocksPendingPerPeriod[i]/maxValue;
         if ( pVDSH->outputHistoryMaxGoodBlocksPendingPerPeriod[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*0.8*fPercent+hPixel, widthBar-wPixel, hGraph*0.8*fPercent);

         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;
      }
      y += hGraph*0.8;
      y += height_text_small*0.2;
   }

   // Pending good blocks to output graph
   // -----------------------------------------

   osd_set_colors();

   if ( bIsNormal || bIsExtended )
   {
      int maxPacketsPerSec = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++)
      {
         if ( pSM_RadioStats->radio_streams[i][0].uVehicleId == pActiveModel->uVehicleId )
         {
            maxPacketsPerSec = pSM_RadioStats->radio_streams[0][STREAM_ID_VIDEO_1].rxPacketsPerSec;
            break;
         }
      }
      
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Received Packets/sec:");
      sprintf(szBuff, "%d", maxPacketsPerSec);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }
   if ( bIsExtended )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Received Blocks/sec:");

      u32 uCountBlocksOut = 0;
      u32 uTimeMs = 0;
      for( int i=0; i<totalHistoryValues/2; i++ )
      {
         uCountBlocksOut += pVDSH->outputHistoryBlocksOkPerPeriod[i];
         uCountBlocksOut += pVDSH->outputHistoryBlocksReconstructedPerPeriod[i];
         uTimeMs += pVDSH->outputHistoryIntervalMs;
      }
      if ( uTimeMs != 0 )
         sprintf(szBuff, "%d", uCountBlocksOut * 1000 / uTimeMs );
      else
         sprintf(szBuff, "0");

      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }

   */

/*
   if ( bIsExtended )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Lost packets max gap:");
      sprintf(szBuff, "%d", maxHistoryPacketsGap);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Lost packets max gap time:");
      if ( videoBitrate > 0 )
         sprintf(szBuff, "%d ms", (maxHistoryPacketsGap*pVDS->PHVS.uCurrentBlockPacketSize*1000*8)/videoBitrate);
      else
         sprintf(szBuff, "N/A ms");
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }

   //-------------------------------------------------
   // History requested retransmissions vs missing packets

   if ( bIsNormal || bIsExtended )
   if ( iDeveloperMode )
   {
      float hGraphRetransmissions = hGraph;
     
      float maxValue = 1.0;
      float maxRecv = 0.0;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pCRS->history[i].uCountReRequestedPacketsForRetransmission > maxValue )
            maxValue = pCRS->history[i].uCountReRequestedPacketsForRetransmission;
         if ( pCRS->history[i].uCountRequestedPacketsForRetransmission > maxValue )
            maxValue = pCRS->history[i].uCountRequestedPacketsForRetransmission;

         if ( pCRS->history[i].uCountReRequestedPacketsForRetransmission + pCRS->history[i].uCountRequestedPacketsForRetransmission > maxValue )
            maxValue = pCRS->history[i].uCountReRequestedPacketsForRetransmission + pCRS->history[i].uCountRequestedPacketsForRetransmission;

         if ( pVDSH->missingTotalPacketsAtPeriod[i] > maxValue )
            maxValue = pVDSH->missingTotalPacketsAtPeriod[i];
      }
      
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pCRS->history[i].uCountReceivedRetransmissionPackets > maxRecv )
            maxRecv = pCRS->history[i].uCountReceivedRetransmissionPackets;
      }

      if ( maxValue > 20 )
         maxValue = 20;
      if ( maxRecv > 20 )
         maxRecv = 20;

      osd_set_colors();

      float ftmp = 0;

      y += height_text_small*0.3;

      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStatsSmall, " packets");
      ftmp += g_pRenderEngine->textWidth(s_idFontStatsSmall, " packets");
    
      g_pRenderEngine->setFill(40,250,50, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(40,250,50, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, s_idFontStatsSmall, " Recv");
      ftmp += g_pRenderEngine->textWidth(s_idFontStatsSmall, " Recv");
      osd_set_colors();

      g_pRenderEngine->setFill(10,50,200, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(10,50,200, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, s_idFontStatsSmall, " Re-Req /");
      ftmp += g_pRenderEngine->textWidth(s_idFontStatsSmall, " Re-Req /");
      osd_set_colors();

      g_pRenderEngine->setFill(150,180,250, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(150,180,250, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, s_idFontStatsSmall, "/ Req /");
      ftmp += g_pRenderEngine->textWidth(s_idFontStatsSmall, "/ Req /");
      osd_set_colors();

      g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, s_idFontStatsSmall, "Missing ");
      ftmp += g_pRenderEngine->textWidth(s_idFontStatsSmall, "Missing ");
      osd_set_colors();

      y += height_text_small*1.2;
   
      osd_set_colors();

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", (int)(maxValue/2.0));
      g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y+hGraphRetransmissions*0.5-height_text_small*0.5, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y+hGraphRetransmissions-height_text_small*0.5, s_idFontStatsSmall, "0");

      pc = get_Color_OSDText();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphRetransmissions, xPos + dxGraph + width, y+hGraphRetransmissions);
      midLine = hGraphRetransmissions/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      yBottomGraph = y + hGraphRetransmissions - 1.0/g_pRenderEngine->getScreenHeight();

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar;
            
      for( int i=0; i<totalHistoryValues; i++ )
      {
         g_pRenderEngine->setStrokeSize(1.0);
          
         float fPercentTotalReq = (float)pCRS->history[i].uCountRequestedPacketsForRetransmission/(maxValue+maxRecv);
         if ( fPercentTotalReq > 1.0 )
            fPercentTotalReq = 1.0;
         float fPercentTotalReReq = (float)pCRS->history[i].uCountReRequestedPacketsForRetransmission/(maxValue+maxRecv);
         if ( fPercentTotalReReq > 1.0 )
            fPercentTotalReReq = 1.0;

         float fPercentTotalRecv = (float)pCRS->history[i].uCountReceivedRetransmissionPackets/(maxValue+maxRecv);
         if ( fPercentTotalRecv > 1.0 )
            fPercentTotalRecv = 1.0;

         float fPercentTotalMissing = (float)pVDSH->missingTotalPacketsAtPeriod[i]/maxValue;
         if ( fPercentTotalMissing > 1.0 )
            fPercentTotalMissing = 1.0;

         float hBarReq = hGraphRetransmissions*fPercentTotalReq;
         float hBarReReq = hGraphRetransmissions*fPercentTotalReReq;
         float hBarRecv = hGraphRetransmissions*fPercentTotalRecv;

         float yBottom = yBottomGraph;
         if ( pCRS->history[i].uCountReceivedRetransmissionPackets != 0 )
         {
            g_pRenderEngine->setStroke(40,250,50, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setFill(40,250,50, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBarSt, yBottom - hBarRecv, widthBar-wPixel, hBarRecv);
            yBottom -= hBarRecv;
         } 

         if ( pCRS->history[i].uCountRequestedPacketsForRetransmission != 0 )
         {
            g_pRenderEngine->setStroke(250,250,250, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setFill(150,180,250, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBarSt, yBottom - hBarReq, widthBar-wPixel, hBarReq);
            yBottom -= hBarReq;
         }
         if ( pCRS->history[i].uCountReRequestedPacketsForRetransmission != 0 )
         {
            g_pRenderEngine->setStroke(200,200,200, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setFill(10,50,200, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBarSt, yBottom - hBarReReq, widthBar-wPixel, hBarReReq);
            yBottom -= hBarReReq;
         }         
         if ( (pVDSH->missingTotalPacketsAtPeriod[i] != 0) ||
               ((i<totalHistoryValues-1) && (pVDSH->missingTotalPacketsAtPeriod[i+1] != 0)) )
         {
            g_pRenderEngine->setStrokeSize(2.0);
            g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawLine(xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing, xBarSt + widthBar, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing);
            if ( i < totalHistoryValues-1 )
            {
               float fPercentTotalMissingNext = (float)pVDSH->missingTotalPacketsAtPeriod[i+1]/maxValue;
               if ( fPercentTotalMissingNext > 1.0 )
                  fPercentTotalMissingNext = 1.0;
            
               g_pRenderEngine->drawLine(xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing, xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissingNext);    
            }
         }

         xBarSt -= widthBar;
         xBarEnd -= widthBar;
      }

      y += hGraphRetransmissions;
      y += height_text_small * 0.5;
   }
   // History requested retransmissions vs missing packets
   //-------------------------------------------------


   // Dev requested retransmissions

   if ( iDeveloperMode )
   {
      g_pRenderEngine->setColors(get_Color_Dev());

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Retransmissions (Req/Ack/Recv):");
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "   Total:");
      strcpy(szBuff, "N/A");
      int percent = 0;
      if ( pCRS->totalRequestedRetransmissions > 0 )
         percent = pCRS->totalReceivedRetransmissions*100/pCRS->totalRequestedRetransmissions;
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryExtraInfoRetransmissions )
         sprintf(szBuff, "%u / %u / %u %d%%", pCRS->totalRequestedRetransmissions, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfoRetransmissions.totalReceivedRetransmissionsRequestsUnique, pCRS->totalReceivedRetransmissions, percent);
      else
         sprintf(szBuff, "%u / N/A / %u %d%%", pCRS->totalRequestedRetransmissions, pCRS->totalReceivedRetransmissions, percent);

      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "   Last 500ms:");
      strcpy(szBuff, "N/A");
      percent = 0;
      if ( pCRS->totalRequestedRetransmissionsLast500Ms > 0 )
         percent = pCRS->totalReceivedRetransmissionsLast500Ms*100/pCRS->totalRequestedRetransmissionsLast500Ms;
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryExtraInfoRetransmissions )
         sprintf(szBuff, "%u / %u / %u %d%%", pCRS->totalRequestedRetransmissionsLast500Ms, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfoRetransmissions.totalReceivedRetransmissionsRequestsUniqueLast5Sec, pCRS->totalReceivedRetransmissionsLast500Ms, percent);
      else
         sprintf(szBuff, "%u / N/A / %u %d%%", pCRS->totalRequestedRetransmissionsLast500Ms, pCRS->totalReceivedRetransmissionsLast500Ms, percent);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "   Segments:");
      strcpy(szBuff, "N/A");
      percent = 0;
      if ( 0 != pCRS->totalRequestedVideoPackets )
         percent = pCRS->totalReceivedVideoPackets*100/pCRS->totalRequestedVideoPackets;

      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryExtraInfoRetransmissions )
         sprintf(szBuff, "%u / %u / %u %d%%", pCRS->totalRequestedVideoPackets, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfoRetransmissions.totalReceivedRetransmissionsRequestsSegmentsUnique, pCRS->totalReceivedVideoPackets, percent );
      else
         sprintf(szBuff, "%u / N/A / %u %d%%", pCRS->totalRequestedVideoPackets, pCRS->totalReceivedVideoPackets, percent );
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Retr Time (min/max/avg):");
      
      u8 uMin = 0xFF;
      u8 uMax = 0;
      u32 uAverage = 0;
      u32 uCountValues = 0;
      for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS;i++ )
      {
         if ( pCRS->history[i].uAverageRetransmissionRoundtripTime == 0 )
            continue;
         if ( pCRS->history[i].uCountReceivedRetransmissionPackets == 0 )
            continue;
         uCountValues++;
         uAverage += pCRS->history[i].uAverageRetransmissionRoundtripTime;

         if ( pCRS->history[i].uMinRetransmissionRoundtripTime < uMin )
            uMin = pCRS->history[i].uMinRetransmissionRoundtripTime;
         if ( pCRS->history[i].uMaxRetransmissionRoundtripTime > uMax )
            uMax = pCRS->history[i].uMaxRetransmissionRoundtripTime;
      }
      if ( uCountValues > 0 )
         uAverage = uAverage/uCountValues;
      if ( uMin == 0xFF )
         sprintf(szBuff, "0/0/0 ms");
      else
         sprintf(szBuff, "%d/%d/%d ms", uMin, uMax, uAverage);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Retr Time P (min/max/avg):");
      
      sprintf(szBuff, "%d/%d/%d", pCRS->uMinPacketRetransmissionTime, pCRS->uMaxPacketRetransmissionTime, pCRS->uAvgPacketRetransmissionTime);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      osd_set_colors();
   }
*/
   osd_set_colors();

//////////////////////////////////////////////////

/*
   // --------------------------------------------------------------
   // History received/ack/completed/dropped retransmissions

   if ( bIsSnapshot )
   {
      osd_set_colors();

      y += height_text_small*0.2;
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStatsSmall, "Req/Ack/Done/Dropped Retransmissions");
      y += height_text_small*1.1;

      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + width, y+hGraph);
     
      yBottomGraph = y + hGraph;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pCRS->history[i].uCountRequestedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.13, hGraph*0.12);
         }

         if ( pCRS->history[i].uCountAcknowledgedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(50,100,250, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.37, hGraph*0.12);
         }

         if ( pCRS->history[i].uCountCompletedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(100, 250, 100, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.63, hGraph*0.12);
         }
        
         if ( pCRS->history[i].uCountDroppedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(250,50,50, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.87, hGraph*0.12);
         }
        
         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;

      }
      y += hGraph;
      y += height_text_small*0.3;
      
      osd_set_colors();
   }

   // History received/ack/completed/dropped retransmissions
   // --------------------------------------------------------------
*/

   osd_set_colors();

   if ( bIsSnapshot )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Press [Cancel] to dismiss");
      y += height_text*s_OSDStatsLineSpacing;
   }

   return height;
}


float osd_render_stats_telemetry_get_height(float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   height += 11*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_telemetry_get_width(float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}


float osd_render_stats_telemetry(float xPos, float yPos, float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;

   float width = osd_render_stats_telemetry_get_width(scale);
   float height = osd_render_stats_telemetry_get_height(scale);

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2.0*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Telemetry Stats");
   if ( NULL != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel )
      sprintf(szBuff, "Update rate: %d Hz", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->telemetry_params.iUpdateRateHz);
   else
      sprintf(szBuff, "N/A");
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.5*s_OSDStatsLineSpacing;

   u32 uMaxLostTime = TIMEOUT_TELEMETRY_LOST;   
   if ( NULL != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->telemetry_params.iUpdateRateHz > 10 )
      uMaxLostTime = TIMEOUT_TELEMETRY_LOST/2;
   
   static u32 s_uTimeOSDRubyTelemetryLostShowRedUntill = 0;
   static u32 s_uTimeOSDRubyTelemetryLostRedValue = 0;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRubyTelemetryLost )
   {
      s_uTimeOSDRubyTelemetryLostShowRedUntill = g_TimeNow+2000;
      s_uTimeOSDRubyTelemetryLostRedValue = g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetry;
   }

   static u32 s_uTimeOSDFCTelemetryLostShowRedUntill = 0;
   static u32 s_uTimeOSDFCTelemetryLostRedValue = 0;
   if ( g_TimeNow > g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetry + uMaxLostTime )
   {
      s_uTimeOSDFCTelemetryLostShowRedUntill = g_TimeNow+2000;
      s_uTimeOSDFCTelemetryLostRedValue = g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetry;
   }

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Telemetry set timeout:");
   sprintf(szBuff, "%d ms", uMaxLostTime);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   /*
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Last Ruby telemetry:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetry);
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDRubyTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDRubyTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   if ( g_TimeNow < s_uTimeOSDRubyTelemetryLostShowRedUntill )
      osd_set_colors();
      
   y += height_text*s_OSDStatsLineSpacing;
   */

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last Ruby telem (full):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetryExtended);
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDRubyTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDRubyTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last Ruby telem (short):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfoShort )
   {
      if ( g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetryShort >= 1000 )
         sprintf(szBuff, "%u sec ago", (g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetryShort)/1000);
      else
         sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvRubyTelemetryShort);
   }
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDRubyTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDRubyTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   /*
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Last FC telem:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetry);
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDFCTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDFCTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text, s_idFontStats, szBuff);   
   if ( g_TimeNow < s_uTimeOSDRubyTelemetryLostShowRedUntill )
      osd_set_colors();
   
   y += height_text*s_OSDStatsLineSpacing;
   */

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last FC telem (full):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetryFull);
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDFCTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDFCTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   y += height_text*s_OSDStatsLineSpacing;


   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last FC telem (short):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetryShort )
   {
      if ( g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetryShort >= 1000 )
         sprintf(szBuff, "%u sec ago", (g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetryShort)/1000);
      else
         sprintf(szBuff, "%u ms ago", g_TimeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvFCTelemetryShort);
   }
   else
      strcpy(szBuff, "Never");
   if ( g_TimeNow < s_uTimeOSDFCTelemetryLostShowRedUntill )
   {
      sprintf(szBuff, "%u ms ago", s_uTimeOSDFCTelemetryLostRedValue);
      g_pRenderEngine->setColors(get_Color_IconError());
   }
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Ruby Freq (full/short):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      sprintf(szBuff, "%d/%d Hz", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iFrequencyRubyTelemetryFull, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iFrequencyRubyTelemetryShort);
   else
      strcpy(szBuff, "N/A");
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "FC Freq (full/short):");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d/%d Hz", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iFrequencyFCTelemetryFull, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iFrequencyFCTelemetryShort);
   else
      strcpy(szBuff, "N/A");
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Data from FC:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d kbps", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_kbps);
   else
      strcpy(szBuff, "N/A");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_kbps == 0 )
      g_pRenderEngine->setColors(get_Color_IconError());
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   osd_set_colors();
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Messages from FC:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d msg/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[6]);
   else
      strcpy(szBuff, "N/A");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[6] == 0 )
      g_pRenderEngine->setColors(get_Color_IconError());
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   osd_set_colors();
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Heartbeats from FC:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d msg/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_hudmsgpersec & 0x0F);
   else
      strcpy(szBuff, "N/A");
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_hudmsgpersec & 0x0F) == 0 )
      g_pRenderEngine->setColors(get_Color_IconError());
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   osd_set_colors();
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "SysMsgs from FC:");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d msg/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_hudmsgpersec >> 4);
   else
      strcpy(szBuff, "N/A");
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.fc_hudmsgpersec >> 4) == 0 )
      g_pRenderEngine->setColors(get_Color_IconError());
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);   
   osd_set_colors();
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}


float osd_render_stats_audio_decode_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.7*height_text*s_OSDStatsLineSpacing;
   float hGraph = 3.0*height_text;

   height += height_text;
   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL != pModel) && pModel->isAudioCapableAndEnabled() )
   {
      height += hGraph;
      height += height_text*s_OSDStatsLineSpacing;
   }
   return height;
}

float osd_render_stats_audio_decode_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_audio_decode(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = 3.0*height_text;
   float wPixel = g_pRenderEngine->getPixelWidth();

   float width = osd_render_stats_audio_decode_get_width();
   float height = osd_render_stats_audio_decode_get_height();

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;
   float dxGraph = 0.01;
   float fWidthGraph = widthMax - dxGraph;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Audio Decode Stats");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   osd_set_colors();

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   if ( (NULL == pActiveModel) || (! pActiveModel->isAudioCapableAndEnabled()) )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Audio is not enabled.");
      return height;
   }

   char szBuff[128];
   strcpy(szBuff, "N/A");
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_AUDIO].uVehicleId == pActiveModel->uVehicleId )
      {
         sprintf(szBuff, "%u kbps", (g_SM_RadioStats.radio_streams[i][STREAM_ID_AUDIO].rxBytesPerSec*8)/1000);
         break;
      }
   }
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, L("Audio:"), szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   int iGraphIntervals = 1;
   int iRTValuesPerGraphInterval = 1;
   if ( (g_pControllerSettings->nGraphVideoRefreshInterval > 0) && (g_SMControllerRTInfo.uUpdateIntervalMs > 0) )
   {
      iGraphIntervals = (SYSTEM_RT_INFO_INTERVALS * g_SMControllerRTInfo.uUpdateIntervalMs) / g_pControllerSettings->nGraphVideoRefreshInterval;
      iRTValuesPerGraphInterval = g_pControllerSettings->nGraphVideoRefreshInterval / g_SMControllerRTInfo.uUpdateIntervalMs;
   }
   int iRTStartIndex = g_SMControllerRTInfo.iCurrentIndex - (g_SMControllerRTInfo.iCurrentIndex % iRTValuesPerGraphInterval) - 1;
   if ( iRTStartIndex < 0 )
      iRTStartIndex = SYSTEM_RT_INFO_INTERVALS-1;
   int iRTTotalUsedIntervals = iGraphIntervals * iRTValuesPerGraphInterval;

   int maxGraphValue = 4;
   int iRTIndex = iRTStartIndex;
   for(int i=0; i<iGraphIntervals; i++ )
   {
      float fSumPackets = 0.0;
      for(int k=0; k<iRTValuesPerGraphInterval; k++)
      {
         fSumPackets += g_SMControllerRTInfo.uOutputedAudioPackets[iRTIndex];
         fSumPackets += g_SMControllerRTInfo.uOutputedAudioPacketsCorrected[iRTIndex];
         iRTIndex--;
         if ( iRTIndex < 0 )
            iRTIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }
      if ( fSumPackets > maxGraphValue )
         maxGraphValue = fSumPackets;
   }

   sprintf(szBuff,"%.1f sec, %d ms/bar", (float)iGraphIntervals * (float)g_pControllerSettings->nGraphVideoRefreshInterval / 1000.0, g_pControllerSettings->nGraphVideoRefreshInterval);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   y += height_text_small*1.0;


   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.5, s_idFontStatsSmall, "0");

   double pc[4];
   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthMax, y);         
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphBottomLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphBottomLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + fWidthGraph, y+hGraph);
   float midLine = hGraph/2.0;
   for( float i=0; i<=fWidthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   double colorECUsed[4] = {0,150,0, 1.0};
   double colorECMaxUsed[4] = {220,250,20, 1.0};
   double colorReqRetransmissions[4] = {100,100,255, 1.0};
   double colorRetransmitted[4] = {40,20,255,1.0};
   double colorDropped[4] = {255,50,50,1.0};

   g_pRenderEngine->setStrokeSize(0);

   float yBottomGraph = y + hGraph;// - 1.0/g_pRenderEngine->getScreenHeight();

   float widthBar = fWidthGraph / iGraphIntervals;
   float fWidthBarRect = widthBar-wPixel;
   if ( fWidthBarRect < 2.0 * wPixel )
      fWidthBarRect = widthBar;

   float xBarSt = xPos + widthMax - g_pRenderEngine->getPixelWidth();
   float xBarMid = xBarSt + widthBar*0.5;
   float xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();

   controller_runtime_info_vehicle* pRTInfoActiveVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, uActiveVehicleId);

   iRTIndex = iRTStartIndex;
   for( int i=0; i<iGraphIntervals; i++ )
   {
      float fSumPackets = 0.0;
      float fSumECUsed = 0.0;
      float fSumDropped = 0.0;
      float hBar = 0.0;
      
      for(int k=0; k<iRTValuesPerGraphInterval; k++)
      {
         fSumPackets += g_SMControllerRTInfo.uOutputedAudioPackets[iRTIndex];
         fSumECUsed += g_SMControllerRTInfo.uOutputedAudioPacketsCorrected[iRTIndex];
         fSumDropped += g_SMControllerRTInfo.uOutputedAudioPacketsSkipped[iRTIndex];
         iRTIndex--;
         if ( iRTIndex < 0 )
            iRTIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }

      xBarSt -= widthBar;
      xBarEnd -= widthBar;

      float percentTotal = (float)(fSumPackets)/(float)(maxGraphValue);
      if ( percentTotal > 1.0 )
         percentTotal = 1.0;
      float percentDropped = (float)(fSumDropped)/(float)(maxGraphValue);
      if ( percentDropped > 1.0 )
         percentDropped = 1.0;
      if ( percentTotal > 0.001 )
      {
         hBar = (hGraph-widthBar)*percentTotal;

         // Clean packets
         if ( fSumECUsed < 1.0 )
         {
            g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar);
         }
         // EC packets
         else
         {
            float fPercentEC = fSumECUsed / fSumPackets;
            if ( fPercentEC < .25 )
               fPercentEC = .25;
            if ( hBar < 0.5 * hGraph )
            if ( fPercentEC < 0.4 )
               fPercentEC = 0.4;
            g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar * (1.0 - fPercentEC));
            //g_pRenderEngine->setFill(colorRetransmitted[0], colorRetransmitted[1], colorRetransmitted[2], colorRetransmitted[3]);
            g_pRenderEngine->setFill(colorECUsed[0], colorECUsed[1], colorECUsed[2], colorECUsed[3]);
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar * fPercentEC, fWidthBarRect, hBar * fPercentEC);
         }
      }
      if ( percentDropped > 0.0001 )
      {
         if ( percentDropped < 0.3 )
            percentDropped = 0.3;
         hBar = (hGraph-widthBar)*percentDropped;
         g_pRenderEngine->setFill(colorDropped[0], colorDropped[1], colorDropped[2], colorDropped[3]);
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBar, fWidthBarRect, hBar);
      }
   }
   osd_set_colors();
   y += hGraph;
   return height;
}


float osd_render_stats_rc_get_height(float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + 0.7*height_text*s_OSDStatsLineSpacing;

   height += 0.05*scale;
   height += 4*height_text*s_OSDStatsLineSpacing;
   height += 5*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_rc_get_width(float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_rc(float xPos, float yPos, float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height_text_s = g_pRenderEngine->textHeight(s_idFontStats)*scale;

   float width = osd_render_stats_rc_get_width(scale);
   float height = osd_render_stats_rc_get_height(scale);

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2.0*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;
   float wPixel = g_pRenderEngine->getPixelWidth();

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "RC Stats");
   sprintf(szBuff, "%.1f sec", RC_INFO_HISTORY_SIZE*50/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.5*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Active Link:");
   strcpy(szBuff, "Ruby RC");
   if ( ! osd_get_current_data_source_vehicle_model()->rc_params.rc_enabled )
      strcpy(szBuff, "External");
   if ( NULL != osd_get_current_data_source_vehicle_model() && ((osd_get_current_data_source_vehicle_model()->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED) == 0 ) )
      strcpy(szBuff, "External");

   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing + height_text*0.2;

   if ( NULL == osd_get_current_data_source_vehicle_model() || (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo) )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No info from vehicle"); 
      return height + 0.012;
   }
   if ( NULL == g_pSM_DownstreamInfoRC )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Not connected"); 
      return height + 0.012;
   }

   // Graph
   float dxGraph = height_text_s*0.6;
   widthMax -= dxGraph;
   float hGraph = 0.03*scale;
   float widthBar = (float)widthMax / (float)RC_INFO_HISTORY_SIZE;
   int maxGap = 0;

   if ( ! osd_get_current_data_source_vehicle_model()->rc_params.rc_enabled )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "RC link is disabled");
      y += height_text*s_OSDStatsLineSpacing;
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No RC link graph to show");
      y += hGraph + height_text*0.7 - height_text*s_OSDStatsLineSpacing;
   }
   else
   {
   g_pRenderEngine->drawText(xPos, y-height_text*0.1, s_idFontStatsSmall, "4");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text*0.6, s_idFontStatsSmall, "0");

   double pc[4];
   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   if ( g_SM_DownstreamInfoRC.is_failsafe )
   {
      g_pRenderEngine->setFill(190,10,0,0.6);
      g_pRenderEngine->setStroke(190,10,0,0.6);
   }

   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph+widthMax, y);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph+widthMax, y+hGraph);
   for( float i=0; i<widthMax-4.0*wPixel; i+=4.0*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph/2.0, xPos + dxGraph + i + 2.0*wPixel, y+hGraph/2.0);

   g_pRenderEngine->drawLine(xPos+dxGraph + g_SM_DownstreamInfoRC.last_history_slice*widthBar, y+hGraph-g_pRenderEngine->getPixelHeight(), xPos+dxGraph + g_SM_DownstreamInfoRC.last_history_slice*widthBar, y);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_SM_DownstreamInfoRC.last_history_slice*widthBar+wPixel, y+hGraph-g_pRenderEngine->getPixelHeight(), xPos+dxGraph + g_SM_DownstreamInfoRC.last_history_slice*widthBar+wPixel, y);

   float yBottomGraph = y+hGraph;
   for( int i=0; i<RC_INFO_HISTORY_SIZE; i++ )
   {
      if ( i == g_SM_DownstreamInfoRC.last_history_slice )
         continue;
      u8 val = g_SM_DownstreamInfoRC.history[i];
      u8 cRecv = val & 0x0F;
      u8 cGap = (val >> 4) & 0x0F;
      if ( cGap > maxGap )
         maxGap = cGap;
      if ( cGap > 4 )
         cGap = 4;
      if ( cRecv > 4 )
         cRecv = 4;
      float hBar = hGraph*cRecv/(float)4.0;
      float hGap = hGraph*cGap/(float)4.0;
      if ( cRecv > 0 )
      {
         g_pRenderEngine->setFill(190,190,190,0.7);
         g_pRenderEngine->setStroke(190,190,190,0.7);
         g_pRenderEngine->setStrokeSize(1.0);
         g_pRenderEngine->drawRect(xPos+dxGraph+i*widthBar, yBottomGraph-hBar, widthBar, hBar);
      }
      if ( cGap > 0 )
      {
         g_pRenderEngine->setFill(190,10,0,0.7);
         g_pRenderEngine->setStroke(190,10,0,0.7);
         g_pRenderEngine->setStrokeSize(1.0);
         g_pRenderEngine->drawRect(xPos+dxGraph+i*widthBar, y, widthBar, hGap);
      }
   }

   if ( g_SM_DownstreamInfoRC.is_failsafe )
   {
      g_pRenderEngine->setFill(190,10,0,0.6);
      g_pRenderEngine->setStroke(190,10,0,0.6);
      g_pRenderEngine->setStrokeSize(0);
      g_pRenderEngine->drawText(xPos+height_text*1.2, y+hGraph-height_text*1.5, s_idFontStatsSmall, "! FAILSAFE !");
   }

   osd_set_colors();   

   y += hGraph + height_text*0.5;
   }

   osd_set_colors();

   bool bShowMAVLink = false;
   if ( NULL != osd_get_current_data_source_vehicle_model() && ( ! osd_get_current_data_source_vehicle_model()->rc_params.rc_enabled ) )
      bShowMAVLink = true;
   else if ( NULL != osd_get_current_data_source_vehicle_model() && ((osd_get_current_data_source_vehicle_model()->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED) == 0 ) )
      bShowMAVLink = true;
   if ( g_SM_DownstreamInfoRC.is_failsafe )   
   if ( NULL != osd_get_current_data_source_vehicle_model() && osd_get_current_data_source_vehicle_model()->rc_params.failsafeFlags == RC_FAILSAFE_NOOUTPUT )
      bShowMAVLink = true;

   sprintf(szBuff, "CH1: %04d", g_SM_DownstreamInfoRC.rc_channels[0]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH1: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[0]);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH5: %04d", g_SM_DownstreamInfoRC.rc_channels[4]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH5: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[4]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH2: %04d", g_SM_DownstreamInfoRC.rc_channels[1]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH2: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[1]);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH6: %04d", g_SM_DownstreamInfoRC.rc_channels[5]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH6: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[5]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH3: %04d", g_SM_DownstreamInfoRC.rc_channels[2]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH3: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[2]);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH7: %04d", g_SM_DownstreamInfoRC.rc_channels[6]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH7: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[6]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH4: %04d", g_SM_DownstreamInfoRC.rc_channels[3]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH4: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[3]);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH8: %04d", g_SM_DownstreamInfoRC.rc_channels[7]);
   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "ECH8: %04d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[7]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   y += height_text*0.3;

   if ( ! g_pCurrentModel->rc_params.rc_enabled )
      return height;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Recv frames:");
   sprintf(szBuff, "%u", g_SM_DownstreamInfoRC.recv_packets);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Lost frames:");
   sprintf(szBuff, "%u", g_SM_DownstreamInfoRC.lost_packets);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Max gap:");
   sprintf(szBuff, "%d ms", maxGap * 1000 / g_pCurrentModel->rc_params.rc_frames_per_second );
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Failsafed count:");
   sprintf(szBuff, "%d", g_SM_DownstreamInfoRC.failsafe_count);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}

float osd_render_stats_efficiency_get_height(float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + 0.7*height_text*s_OSDStatsLineSpacing;

   height += 2*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_efficiency_get_width(float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_efficiency(float xPos, float yPos, float scale)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;

   float width = osd_render_stats_efficiency_get_width(scale);
   float height = osd_render_stats_efficiency_get_height(scale);

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Efficiency Stats");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   osd_set_colors();

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "mA/km:");
   float eff = 0.0;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->m_Stats.uCurrentFlightDistance > 500 )
      eff = (float)g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightDistance/100.0/1000.0);
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "mA/h:");
   eff = 0;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->m_Stats.uCurrentFlightTime > 0 )
      eff = ( (float)g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightTime))*3600.0;
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}

float osd_render_stats_video_stream_h264_frames_info_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 1.1*height_text*s_OSDStatsLineSpacing;

   float hGraph = height_text*4.0;

   // stats
   height += 5.0 * height_text*s_OSDStatsLineSpacing;

   height += 0.5*height_text;
   height += hGraph;
   return height;
}

float osd_render_stats_video_stream_h264_frames_info_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAA AAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_video_stream_h264_frames_info(float xPos, float yPos)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   
   shared_mem_video_stream_stats* pVDS = NULL;
   int iIndexRouterRuntimeInfo = -1;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uActiveVehicleId )
         pVDS = &(g_SM_VideoDecodeStats.video_streams[i]);
   }
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexRouterRuntimeInfo = i;
   }
   if ( (NULL == pVDS) || (NULL == pActiveModel) || (-1 == iIndexRouterRuntimeInfo) )
      return 0.0; 

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   
   float hGraph = height_text*4.0;
   float wPixel = g_pRenderEngine->getPixelWidth();

   float width = osd_render_stats_video_stream_h264_frames_info_get_width();
   float height = osd_render_stats_video_stream_h264_frames_info_get_height();

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

   sprintf(szBuff, "Video Frames %u FPS / ", pVDS->iCurrentVideoTxSourceFPS);
   if ( pActiveModel->isVideoLinkFixedOneWay() || ( ! (pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)) )
      strcat(szBuff, "Fixed KF");
   else
      strcat(szBuff, "Auto KF");
   
   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.2*s_OSDStatsLineSpacing;

   int iSlices = camera_get_active_camera_h264_slices(pActiveModel);

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "888 kb");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / (float)MAX_FRAMES_SAMPLES;
   
   static u32 s_uLastTimeKeyFrameValueChangedInOSD = 0;
   static u32 s_uLastKeyFrameValueInOSD = 0;
   if ( s_uLastKeyFrameValueInOSD != pVDS->PHVS.uCurrentVideoKeyframeIntervalMs )
   {
      s_uLastKeyFrameValueInOSD = pVDS->PHVS.uCurrentVideoKeyframeIntervalMs;
      s_uLastTimeKeyFrameValueChangedInOSD = g_TimeNow;
   }
   bool bShowChanging = false;
   if ( g_TimeNow < s_uLastTimeKeyFrameValueChangedInOSD + 1000 )
      bShowChanging = true;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Recv KF:");
   sprintf(szBuff, "%d ms", (int)pVDS->PHVS.uCurrentVideoKeyframeIntervalMs);
   if ( bShowChanging )
      g_pRenderEngine->setColors(get_Color_OSDChangedValue());
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   if ( bShowChanging )
   {
      osd_set_colors();
      g_pRenderEngine->setColors(get_Color_Dev());
   }
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "%u kBytes (%d pkts)", (g_SM_VideoFramesStatsOutput.uAverageIFrameSizeBytes)/1000, 1 + g_SM_VideoFramesStatsOutput.uAverageIFrameSizeBytes / g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].video_data_length);
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Avg Iframe size:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "%u kBytes (%d pkts)", (g_SM_VideoFramesStatsOutput.uAveragePFrameSizeBytes)/1000, 1 +  g_SM_VideoFramesStatsOutput.uAveragePFrameSizeBytes / g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].video_data_length);
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Avg Pframe size:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // End minimal view
   //--------------------------------------------------
   //if ( g_pControllerSettings->iShowVideoStreamInfoCompactType == 2 )
   //   return height;

   sprintf(szBuff, "%d/%u", iSlices, g_SM_VideoFramesStatsOutput.uDetectedSlices);
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Slices (set/detected):", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "%d",g_SM_VideoFramesStatsOutput.uDetectedFPS);
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Detected FPS:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   y += height_text*0.5;
   u32 uMaxValue = 0;
   u32 uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_SM_VideoFramesStatsOutput.uFramesSizes[i] < uMinValue )
         uMinValue = g_SM_VideoFramesStatsOutput.uFramesSizes[i];
      if ( g_SM_VideoFramesStatsOutput.uFramesSizes[i] > uMaxValue )
         uMaxValue = g_SM_VideoFramesStatsOutput.uFramesSizes[i];
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;

   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   sprintf(szBuff, "%d kB", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+8.0*wPixel, y-height_text_small*0.1, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d kB", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+8.0*wPixel, y+hGraph*0.5-height_text_small*0.6, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d kB", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+8.0*wPixel, y+hGraph-height_text_small*0.9, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos + widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 7*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 4.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   float xBarStart = xPos;
   double pc1[4];
   memcpy(pc1, get_Color_OSDText(), 4*sizeof(double));

   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       if ( i == (int)g_SM_VideoFramesStatsOutput.uLastFrameIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          xBarStart += widthBar;
          continue;
       }

       if ( g_SM_VideoFramesStatsOutput.uFramesSizes[i] == 0 )
       {
          xBarStart += widthBar;
          continue;
       }

       int iFrameType = (g_SM_VideoFramesStatsOutput.uFramesTypesAndDuration[i] & 0xC0) >> 6;
       u32 uValue = g_SM_VideoFramesStatsOutput.uFramesSizes[i];
       float hBar = (hGraph * (float)uValue) / (float)uMaxValue;
       if ( iFrameType == 1 )
       {
          g_pRenderEngine->setFill(50,70,250,0.9);
          g_pRenderEngine->setStroke(50,70,250,0.9);
       }
       else if ( iFrameType != 0 )
       {
          g_pRenderEngine->setFill(250,250,50,0.9);
          g_pRenderEngine->setStroke(250,250,50,0.9);
       }
       else
       {
          g_pRenderEngine->setFill(pc1);
          g_pRenderEngine->setStroke(pc1[0], pc1[1], pc1[2], pc1[3]);
       }
       g_pRenderEngine->drawRect(xBarStart, yBottomGraph - hBar, widthBar, hBar);
       xBarStart += widthBar;
   }

   osd_set_colors();
   y += hGraph + height_text*0.4;
   return height;
}


float osd_render_stats_flight_end(float scale)
{
   u32 fontId = g_idFontOSD;
   float height_text = g_pRenderEngine->textHeight(fontId)*scale;
   Preferences* pP = get_Preferences();

   //float width = 0.38 * scale;
   float width = g_pRenderEngine->textWidth(fontId, "AAAAAAAA AAAAAAAA AAAAAA AAAAAA AAA AAAAAAAA");

   float lineHeight = height_text*s_OSDStatsLineSpacing*1.4;
   float height = 4.0 *s_fOSDStatsMargin*scale*1.1 + lineHeight;
   height += 14.6*lineHeight + 0.8 * height_text;
   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      height += lineHeight;

   float iconHeight = 2.2*lineHeight;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();
   float xPos = (1.0-width)/2.0;
   float yPos = (1.0-height)/2.0;
   char szBuff[512];

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);

   float alfa = 0.9;
   if ( pP->iInvertColorsOSD )
      alfa = 0.8;
   double pc[4];
   pc[0] = get_Color_OSDBackground()[0];
   pc[1] = get_Color_OSDBackground()[1];
   pc[2] = get_Color_OSDBackground()[2];
   pc[3] = 0.9;
   g_pRenderEngine->setColors(pc, alfa);

   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN*scale);
   osd_set_colors();

   xPos += 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += 2*s_fOSDStatsMargin*scale*0.7;
   width -= 4*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();

   float rightMargin = xPos + width;

   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, yPos, fontId, "Post Flight Statistics");
   else
      g_pRenderEngine->drawText(xPos, yPos, fontId, "Post Run Statistics");

   if ( iconWidth > 0.001 )
   {
      u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawIcon(xPos+width - iconWidth - 0.9*POPUP_ROUND_MARGIN*scale/g_pRenderEngine->getAspectRatio() , yPos-height_text*0.1, iconWidth, iconHeight, idIcon);
   }
   float y = yPos + lineHeight;

   osd_set_colors();

   if ( NULL == g_pCurrentModel || (!g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry) )
   {
      g_pRenderEngine->setGlobalAlfa(fAlpha);
      return height;
   }

   strcpy(szBuff, g_pCurrentModel->getLongName());
   szBuff[0] = toupper(szBuff[0]);
   g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
   y += lineHeight*1.9;

   g_pRenderEngine->drawLine(xPos, y-lineHeight*0.6, xPos+width, y-lineHeight*0.6);
   
   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      sprintf(szBuff, "Flight No. %d", (int)g_pCurrentModel->m_Stats.uTotalFlights);
   else
      sprintf(szBuff, "Run No. %d", (int)g_pCurrentModel->m_Stats.uTotalFlights);

   g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
   y += lineHeight;

   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, fontId, "Flight time:");
   else
      g_pRenderEngine->drawText(xPos, y, fontId, "Run time:");
   if ( g_pCurrentModel->m_Stats.uCurrentFlightTime >= 3600 )
      sprintf(szBuff, "%dh:%02dm:%02ds", g_pCurrentModel->m_Stats.uCurrentFlightTime/3600, (g_pCurrentModel->m_Stats.uCurrentFlightTime/60)%60, (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60);
   else
      sprintf(szBuff, "%02dm:%02ds", (g_pCurrentModel->m_Stats.uCurrentFlightTime/60)%60, (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60);
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, fontId, "Total flight distance:");
   else
      g_pRenderEngine->drawText(xPos, y, fontId, "Total traveled distance:");

   if ( pP->iUnits == prefUnitsImperial )
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
         sprintf(szBuff, "%.1f mi", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentFlightDistance/100)/1000.0);
      else
         sprintf(szBuff, "0.0 mi");
   }
   else
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
         sprintf(szBuff, "%.1f km", (g_pCurrentModel->m_Stats.uCurrentFlightDistance/100)/1000.0);
      else
         sprintf(szBuff, "0.0 km");
   }
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, fontId, "Total current:");
   sprintf(szBuff, "%d mA", (int)(g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10));
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, fontId, "Max Distance:");
   if ( g_pCurrentModel->m_Stats.uCurrentMaxDistance < 1500 )
   {
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%d ft", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxDistance));
      else
         sprintf(szBuff, "%d m", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxDistance));
   }
   else
   {
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%.1f mi", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentMaxDistance/1000.0));
      else
         sprintf(szBuff, "%.1f km", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentMaxDistance/1000.0));
   }
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
   {
      g_pRenderEngine->drawText(xPos, y, fontId, "Max Altitude:");
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%d ft", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxAltitude));
      else
         sprintf(szBuff, "%d m", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxAltitude));
      g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
      y += lineHeight;
   }

   g_pRenderEngine->drawText(xPos, y, fontId, "Max Current:");
   sprintf(szBuff, "%.1f A", (float)g_pCurrentModel->m_Stats.uCurrentMaxCurrent/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, fontId, "Min Voltage:");
   sprintf(szBuff, "%.1f V", (float)g_pCurrentModel->m_Stats.uCurrentMinVoltage/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   if ( pP->iUnits == prefUnitsImperial )
      g_pRenderEngine->drawText(xPos, y, fontId, "Efficiency mA/mi:");
   else
      g_pRenderEngine->drawText(xPos, y, fontId, "Efficiency mA/km:");
   float eff = 0.0;
   if ( g_pCurrentModel->m_Stats.uCurrentFlightDistance > 500 )
   {
      float fv = (float)_osd_convertKm((float)g_pCurrentModel->m_Stats.uCurrentFlightDistance/100.0/1000.0);
      if ( fv > 0.0000001 )
         eff = g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/fv;
      sprintf(szBuff, "%d", (int)eff);
   }
   else
      sprintf(szBuff, "Too Short");
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, fontId, "Efficiency mA/h:");
   eff = 0;
   if ( g_pCurrentModel->m_Stats.uCurrentFlightTime > 0 )
      eff = ((float)g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightTime))*3600.0;
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   y += lineHeight*0.5;
   g_pRenderEngine->drawLine(xPos, y-3.0*g_pRenderEngine->getPixelHeight(), xPos+width, y-3.0*g_pRenderEngine->getPixelHeight());

   g_pRenderEngine->drawText(xPos, y, fontId, "Odometer:");
   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "%.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   else
      sprintf(szBuff, "%.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, fontId, "All time flight time:");
   else
      g_pRenderEngine->drawText(xPos, y, fontId, "All time drive time:");

   if ( g_pCurrentModel->m_Stats.uTotalFlightTime >= 3600 )
      sprintf(szBuff, "%dh:%02dm:%02ds", g_pCurrentModel->m_Stats.uTotalFlightTime/3600, (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60, (g_pCurrentModel->m_Stats.uTotalFlightTime)%60);
   else
      sprintf(szBuff, "%02dm:%02ds", (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60, (g_pCurrentModel->m_Stats.uTotalFlightTime)%60);

   g_pRenderEngine->drawTextLeft(rightMargin, y, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->setGlobalAlfa(fAlpha);

   ///////////////////////////////////
   y += height_text * 1.8;
   float width_text = g_pRenderEngine->textWidth(s_idFontStats, "Ok");    
   float selectionMargin = 0.01;
   float fWidthSelection = width_text + 2.0*selectionMargin;
   g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
   g_pRenderEngine->drawRoundRect(xPos-1.0*selectionMargin/g_pRenderEngine->getAspectRatio(), y-0.8*selectionMargin, fWidthSelection+2.0*selectionMargin/g_pRenderEngine->getAspectRatio(), height_text+2*selectionMargin, selectionMargin);
   g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
   g_pRenderEngine->drawText(xPos, y+g_pRenderEngine->getPixelHeight()*1.2, g_idFontMenu, "Ok");

   osd_set_colors();
   g_pRenderEngine->drawMessageLines(xPos + fWidthSelection + width_text, y - height_text*0.2, "Press [Ok/Menu] or [Back/Cancel] to close this stats.", MENU_TEXTLINE_SPACING, rightMargin-(xPos + fWidthSelection + width_text), s_idFontStats);
   return height;
}

float osd_render_stats_flights(float scale)
{
/*
   Preferences* pP = get_Preferences();
   float text_scale = osd_getFontHeight()*scale;
   float height_text = TextHeight(*render_getFontOSD(), text_scale);

   text_scale *= 0.6;
   height_text *= 0.6;

   float width = toScreenX(0.64) * scale;
   float height = height_text*4.0;

   float xPos = toScreenX(0.2) * scale;
   float yPos = toScreenY(0.18) * scale;
   char szBuff[512];

   float fAlpha = render_get_globalAlfa(); 
   render_set_globalAlfa(0.85);
   float alfa = 0.7;
   if ( pP->iInvertColorsOSD )
      alfa = 0.74;

   render_set_colors(get_Color_OSDBackground(), alfa);

   Roundrect(xPos, yPos, width, height, 1.5*toScreenX(POPUP_ROUND_MARGIN)*scale, 1.5*toScreenX(POPUP_ROUND_MARGIN)*scale);
   osd_set_colors();

   xPos += toScreenX(s_fOSDStatsMargin)*scale;
   yPos = yPos + height - toScreenX(s_fOSDStatsMargin)*scale*0.5;
   width -= 2*toScreenX(s_fOSDStatsMargin)*scale;

   float widthMax = width;
   float rightMargin = xPos + width;

   if ( NULL == g_pCurrentModel )
   {
      render_set_globalAlfa(fAlpha);
      return height;
   }

   sprintf(szBuff, "%s Statistics", g_pCurrentModel->getShortName());
   draw_message(szBuff, xPos, yPos, text_scale*0.98, render_getFontOSD());
   
   yPos -= height_text*1.4;

   osd_set_colors();

   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "Total flights: %d,  Total flight time: %dh:%02dm:%02ds,  Total flight distance: %.1f mi",
                       g_pCurrentModel->stats_TotalFlights, g_pCurrentModel->stats_TotalFlightTime/10/3600,
                       (g_pCurrentModel->stats_TotalFlightTime/10/60)%60, (g_pCurrentModel->stats_TotalFlightTime/10)%60,
                       _osd_convertKm(g_pCurrentModel->stats_TotalFlightDistance/10.0/1000.0));
   else
      sprintf(szBuff, "Total flights: %d,  Total flight time: %dh:%02dm:%02ds,  Total flight distance: %.1f km",
              g_pCurrentModel->stats_TotalFlights, g_pCurrentModel->stats_TotalFlightTime/10/3600,
              (g_pCurrentModel->stats_TotalFlightTime/10/60)%60, (g_pCurrentModel->stats_TotalFlightTime/10)%60,
              g_pCurrentModel->stats_TotalFlightDistance/10.0/1000.0);
   draw_message(szBuff, xPos, yPos, text_scale*0.9, render_getFontOSD());

   render_set_globalAlfa(fAlpha);

   return height;
*/
return 0.0;
}

float osd_render_stats_dev_get_height()
{
   Preferences* p = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float hGraph = 0.04;
   float hGraphSmall = 0.028;

   float height = 1.6 *s_fOSDStatsMargin*1.3 + 2 * 0.7*height_text*s_OSDStatsLineSpacing;

   if ( p->iDebugShowDevRadioStats )
      height += 12 * height_text * s_OSDStatsLineSpacing;

   if ( p->iDebugShowDevVideoStats )
   {
      height += 1.0*(hGraph + height_text) + 4.0*(hGraphSmall + height_text);
   }
   return height;
}

float osd_render_stats_dev_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}


float osd_render_stats_dev(float xPos, float yPos, float scale)
{
   Preferences* p = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;

   float width = osd_render_stats_dev_get_width();
   float height = osd_render_stats_dev_get_height();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawText( xPos, yPos, s_idFontStats, "Developer Mode");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   u8 uLinkMinAck = 255;
   u8 uLinkMaxAck = 0;

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   
   int iIndexVehicleRuntimeInfo = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
      {
         iIndexVehicleRuntimeInfo = i;
         break;
      }
   }
   controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, uActiveVehicleId);
   int iCountInterfaces = hardware_get_radio_interfaces_count();
   if ( (-1 != iIndexVehicleRuntimeInfo) && g_bIsRouterReady && (NULL != pActiveModel) && (NULL != pRTInfoVehicle) )
   {
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS; i++ )
      for( int k=0; k<iCountInterfaces; k++ )
      {
          if ( 0 != pRTInfoVehicle->uMinAckTime[i][k] )
          if ( pRTInfoVehicle->uMinAckTime[i][k] < uLinkMinAck )
             uLinkMinAck = pRTInfoVehicle->uMinAckTime[i][k];
          if ( pRTInfoVehicle->uMaxAckTime[i][k] < uLinkMaxAck )
             uLinkMaxAck = pRTInfoVehicle->uMaxAckTime[i][k];
      }
   }

   if ( p->iDebugShowDevRadioStats )
   {
   if ( uLinkMaxAck < 2000 )
      sprintf(szBuff, "%d ms", uLinkMaxAck);
   else
      strcpy(szBuff, "N/A");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Link RT (max):", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   if ( 0 != uLinkMaxAck )
      sprintf(szBuff, "%d ms", uLinkMaxAck);
   else
      strcpy(szBuff, "N/A");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Link RT (minim):", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // TO FIX
   /*
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats && MAX_U32 != g_SM_ControllerRetransmissionsStats.retransmissionTimeAverage && MAX_U32 != g_SM_ControllerRetransmissionsStats.retransmissionTimeMinim )
      sprintf(szBuff, "%d/%d ms", g_SM_ControllerRetransmissionsStats.retransmissionTimeMinim, g_SM_ControllerRetransmissionsStats.retransmissionTimeAverage);
   if ( ! (g_pCurrentModel->video_link_profiles[(g_SM_VideoDecodeStats.video_link_profile & 0x0F)].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS ) )
      strcpy(szBuff, "Disabled");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Video RT Min/Avg:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats && MAX_U32 != g_SM_ControllerRetransmissionsStats.retransmissionTimeLast )
      sprintf(szBuff, "%d ms", g_SM_ControllerRetransmissionsStats.retransmissionTimeLast);
   if ( ! (g_pCurrentModel->video_link_profiles[(g_SM_VideoDecodeStats.video_link_profile & 0x0F)].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS ) )
      strcpy(szBuff, "Disabled");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Video RT Last:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Retransmissions:");
   y += height_text*s_OSDStatsLineSpacing;
   float dx = s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1], ((g_SM_ControllerRetransmissionsStats.totalCountReceivedVideoPackets[1] + g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1])>0)?g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]*100/(g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]+g_SM_ControllerRetransmissionsStats.totalCountReceivedVideoPackets[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Requested:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_SM_ControllerRetransmissionsStats.totalCountReRequestedPackets[1], (g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]>0)?(g_SM_ControllerRetransmissionsStats.totalCountReRequestedPackets[1]*100/g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Retried:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_SM_ControllerRetransmissionsStats.totalCountReceivedRetransmissions[1], (g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]>0)?(g_SM_ControllerRetransmissionsStats.totalCountReceivedRetransmissions[1]*100/g_SM_ControllerRetransmissionsStats.totalCountRequestedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Received:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDuplicated[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDuplicated[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Recv Duplicated:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDiscarded[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDiscarded[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0 );
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Discarded:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountRecvOriginalAfterRetransmission[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountRecvOriginalAfterRetransmission[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0 );
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Original recv after retr.:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "N/A");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d", (int)(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[6]));
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "FC msg/sec", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   */
   }
   

   osd_set_colors();

   if ( p->iDebugShowDevVideoStats )
   {
   // ------------------------------------------------
   // Retransmissions graphs

   // TO FIX
    /*
   y += (height_text-height_text_small);

   static int s_iOSDStatsDevMaxGraphValue = 0;

   int maxGraphValue = g_pSM_RetransmissionsStats->maxValue;
   if ( maxGraphValue >= s_iOSDStatsDevMaxGraphValue )
   {
      s_iOSDStatsDevMaxGraphValue = maxGraphValue;
   }
   else
   {
      if ( s_iOSDStatsDevMaxGraphValue > 50 )
         s_iOSDStatsDevMaxGraphValue-=5;
      else if ( s_iOSDStatsDevMaxGraphValue > 0 )
         s_iOSDStatsDevMaxGraphValue--;
   }

   int totalHistoryValues = MAX_HISTORY_INTERVALS_RETRANSMISSIONS_STATS;
   float dxGraph = 0.01*scale;
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / totalHistoryValues;

   g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, "Regular vs Retransmissions");

   y += height_text_small*0.9;

   float yTopGrid = y;

   hGraph = hGraphSmall;

   double* pc = get_Color_OSDText();
   float midLine = hGraph/2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();
   float yTmp;
   float yTmpPrev;


   // Actually received and retransmissions counts

   int iMax = s_iOSDStatsDevMaxGraphValue;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( iMax < g_pSM_RetransmissionsStats->expectedVideoPackets[i] )
         iMax = g_pSM_RetransmissionsStats->expectedVideoPackets[i];
      if ( iMax < g_pSM_RetransmissionsStats->receivedVideoPackets[i] )
         iMax = g_pSM_RetransmissionsStats->receivedVideoPackets[i];
   }
   if ( iMax <= 0 )
      iMax = 1;
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-0.3*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   //sprintf(szBuff, "%d", g_pSM_RetransmissionsStats->expectedVideoPackets[totalHistoryValues-1]);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y+0.5*hGraph-0.6*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   //for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   //   g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   float xBarSt = xPos + dxGraph;
   float xBarCenter = xPos + dxGraph + widthBar*0.5;
   float xBarCenterPrev = xPos + dxGraph - widthBar*0.5;

   int index = g_pSM_RetransmissionsStats->currentSlice;

   for( int i=0; i<totalHistoryValues; i++ )
   {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index] / (float)iMax;
      g_pRenderEngine->setStroke(50,120,200,0.7);
      g_pRenderEngine->setFill(50,120,200,0.7);
      //g_pRenderEngine->drawLine(xBarSt, yTmp, xBarSt + widthBar, yTmp);
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);

      if ( i != 0 && index != totalHistoryValues-1 )
      {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->expectedVideoPackets[index] / (float)iMax;
      yTmpPrev = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->expectedVideoPackets[index+1] / (float)iMax;
      g_pRenderEngine->setStroke(230,230,0,0.75);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());

      yTmp = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedVideoPackets[index] / (float)iMax;
      yTmpPrev = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedVideoPackets[index+1] / (float)iMax;
      g_pRenderEngine->setStroke(20,250,70,0.8);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }

   y += hGraph;

   // Retransmissions graphs

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphRegular;

   y += (height_text-height_text_small);
   sprintf(szBuff,"%d ms/bar", g_pSM_RetransmissionsStats->refreshInterval);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   float w = g_pRenderEngine->textWidth(height_text*0.9, s_idFontStats, szBuff);
   w += 0.02*scale;
   sprintf(szBuff,"%.1f seconds", (((float)totalHistoryValues) * g_pSM_RetransmissionsStats->refreshInterval)/1000.0);
   g_pRenderEngine->drawTextLeft(xPos+widthMax-w, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   w += g_pRenderEngine->textWidth( height_text*0.9, s_idFontStats, szBuff);

   y += height_text_small*0.9;

   sprintf(szBuff, "%d", s_iOSDStatsDevMaxGraphValue);
   g_pRenderEngine->drawText(xPos, y-0.3*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   xBarCenter = xPos + dxGraph + widthBar*0.5;
   xBarCenterPrev = xPos + dxGraph - widthBar*0.5;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;

   for( int i=0; i<totalHistoryValues; i++ )
   {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      g_pRenderEngine->setStroke(50,120,200,0.7);
      g_pRenderEngine->setFill(50,120,200,0.7);
      //g_pRenderEngine->drawLine(xBarSt, yTmp, xBarSt + widthBar, yTmp);
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);

      if ( i != 0 && index != totalHistoryValues-1 )
      {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      yTmpPrev = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index+1] / (float)s_iOSDStatsDevMaxGraphValue;
      g_pRenderEngine->setStroke(20,250,70,0.8);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());

      yTmp = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      yTmpPrev = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index+1] / (float)s_iOSDStatsDevMaxGraphValue;
      if ( g_pSM_RetransmissionsStats->ignoredRetransmissions[i] > 0 )
      {
         g_pRenderEngine->setStroke(200,20,0,0.65);
         g_pRenderEngine->setFill(200,20,0,0.65);
         g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      //g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      //g_pRenderEngine->drawLine(xBarCenter, yTmp+g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev+g_pRenderEngine->getPixelHeight());
      }

      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }

   y += hGraph;

   // Percent retried

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphSmall;

   float fPercentMax = 0;
   int sumReceived = 0;
   int sumIgnored = 0;
   int sumRequested = 0;
   int sumReRequested = 0;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[i] > 0 )
      if ( (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[i]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[i] > fPercentMax )
         fPercentMax = (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[i]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[i];

      sumReceived += g_pSM_RetransmissionsStats->receivedRetransmissions[i];
      sumIgnored += g_pSM_RetransmissionsStats->ignoredRetransmissions[i];
      sumRequested += g_pSM_RetransmissionsStats->requestedRetransmissions[i];
      sumReRequested += g_pSM_RetransmissionsStats->reRequestedRetransmissions[i];
   }
   int average = 0;
   if ( sumRequested > 0 )
      average = sumReRequested*100/sumRequested;

   if ( fPercentMax < 1.0 )
      fPercentMax = 1.0;
   y += (height_text-height_text_small);
   sprintf(szBuff,"Percentage Retried (avg: %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   sprintf(szBuff, "%d%%", (int)(fPercentMax*100.0) );
   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;

   
   for( int i=0; i<totalHistoryValues; i++ )
   {
      g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[i] > 0 && index != totalHistoryValues-1 )
      {
      float percent = (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[index]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[index];

      g_pRenderEngine->setStroke(250,250,50, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,50, s_fOSDStatsGraphLinesAlpha);

      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight()) * percent / fPercentMax;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;


   // Percent received

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphSmall;
   y += (height_text-height_text_small);

   average = 0;
   if ( sumRequested > 0 )
      average = sumReceived*100/sumRequested;

   sprintf(szBuff,"Percentage Received (avg %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, "100%");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;
   
   for( int i=0; i<totalHistoryValues; i++ )
   {
      g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

      if ( index != totalHistoryValues-1 )
      {
      float percent = 0.0;
      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[index] > 0 )
         percent = (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index] / (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index];
      if ( percent < 0.0 ) percent = 0.0;
      if ( percent > 1.0 )
      {
         percent = 1.0;
         g_pRenderEngine->setStroke(250,250,50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(250,250,50, s_fOSDStatsGraphLinesAlpha);
      }
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* percent;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }

      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;

   // Percent discarded

   g_pRenderEngine->setColors(get_Color_Dev());

   y += (height_text-height_text_small);

   average = 0;
   if ( sumReceived > 0 )
      average = sumIgnored*100/sumReceived;

   sprintf(szBuff,"Percentage Discarded (avg %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, "100%");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;
   
   g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( index != totalHistoryValues-1 )
      {
      float percent = 0.0;
      if ( g_pSM_RetransmissionsStats->receivedRetransmissions[index] > 0 )
         percent = (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index] / (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index];
      if ( percent < 0.0 ) percent = 0.0;
      if ( percent > 1.0 ) percent = 1.0;
      float fh = (hGraph-2*g_pRenderEngine->getPixelHeight())* percent;
      int tmp = fh/g_pRenderEngine->getPixelHeight();
      fh = g_pRenderEngine->getPixelHeight() * (float)tmp;
      yTmp = yBtm - fh;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;

   float yBottomGrid = y;

   xBarSt = xPos + dxGraph + widthGraph;


   osd_set_colors();
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+widthBar*0.5, yTopGrid, xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar, yBottomGrid);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+widthBar*0.5 + wPixel, yTopGrid, xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+wPixel, yBottomGrid);
  
   g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha*0.7);
   g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha*0.7);

   for( int i=0; i<totalHistoryValues; i++ )
   {
      xBarSt -= widthBar;
      if ( i%3 )
         continue;

      g_pRenderEngine->drawLine(xBarSt, yTopGrid, xBarSt, yBottomGrid);
   }
*/
   }
   return height;
}


void _osd_render_vehicle_dev_stats()
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();

   int iIndexVehicleRuntimeInfo = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }
   if ( (NULL == pActiveModel) || (-1 == iIndexVehicleRuntimeInfo) || (0 == uActiveVehicleId) )
      return;


   float height_text = g_pRenderEngine->textHeight(s_idFontStats);

   float width = 0.36;
   float hGraph = height_text*2.0;
   float height = height_text*s_OSDStatsLineSpacing*6.1 + hGraph;
   float xPos = 0.01;
   float yPos = 1.0 - height - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.5;
   width -= 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Vehicle Dev Stats");
   sprintf(szBuff, "VID: %u", uActiveVehicleId);
   
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Router main loop FPS now:");
   sprintf(szBuff, "%u FPS/sec", g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueNow);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Router FPS avg,min,max local:");
   sprintf(szBuff, "%u, %u, %u FPS/sec",
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueAverageLocal,
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueMinimLocal,
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueMaximLocal);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Router FPS avg,min,max:");
   sprintf(szBuff, "%u, %u, %u FPS/sec",
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueAverage,
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueMinim,
      g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRouterCounters.uValueMaxim);
   g_pRenderEngine->drawTextLeft( rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   // Tx time history graph
   // To fix 11.5
   //sprintf(szBuff, "Radio Tx Time (avg: %d ms)", g_VehiclesRuntimeInfo[iIndexVehicleRuntimeInfo].vehicleDebugRadioTxTimers.uComputedTotalTxTimeMilisecPerSecondAverage);
   //g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
   //y += height_text;

   /*
   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, "100%");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;
   
   g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( index != totalHistoryValues-1 )
      {
      float percent = 0.0;
      if ( g_pSM_RetransmissionsStats->receivedRetransmissions[index] > 0 )
         percent = (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index] / (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index];
      if ( percent < 0.0 ) percent = 0.0;
      if ( percent > 1.0 ) percent = 1.0;
      float fh = (hGraph-2*g_pRenderEngine->getPixelHeight())* percent;
      int tmp = fh/g_pRenderEngine->getPixelHeight();
      fh = g_pRenderEngine->getPixelHeight() * (float)tmp;
      yTmp = yBtm - fh;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;
   */
}

void _osd_render_stats_panels_horizontal()
{
   if ( NULL == g_pCurrentModel )
      return;
   
   Preferences* p = get_Preferences();
   float fStatsSize = 1.0;

   float yMax = 1.0-osd_getMarginY()-osd_getBarHeight()-0.01 - osd_getSecondBarHeight();

   if ( g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLean )
      yMax -= 0.05; 

   float xSpacing = 0.01*fStatsSize;

   float yStats = yMax;
   float xStats = 0.99-osd_getMarginX();

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      float fWidth = osd_getVerticalBarWidth();
      xStats -= fWidth;
      yMax = 1.0-osd_getMarginY()-0.01;
      yStats = yMax;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      osd_render_stats_dev(xStats, yStats-osd_render_stats_dev_get_height(), fStatsSize);
      xStats -= osd_render_stats_dev_get_width() + xSpacing;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP) )
   {
      osd_render_stats_graphs_vehicle_tx_gap(xStats, yStats-osd_render_stats_graphs_vehicle_tx_gap_get_height());
      xStats -= osd_render_stats_graphs_vehicle_tx_gap_get_width() + xSpacing;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY) )
   {
      osd_render_stats_video_bitrate_history(xStats - osd_render_stats_video_bitrate_history_get_width(), yStats-osd_render_stats_video_bitrate_history_get_height());
      xStats -= osd_render_stats_video_bitrate_history_get_width() + xSpacing;
   }
   
   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_VIDEO) )
   {
      float hStat = osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize);
      osd_render_stats_video_decode(xStats, yStats-hStat, g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize);
      xStats -= osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize) + xSpacing;
      if ( p->iDebugShowVideoSnapshotOnDiscard )
      if ( s_uOSDSnapshotTakeTime > 1 && g_TimeNow < s_uOSDSnapshotTakeTime + 15000 )
      {
         hStat = osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, fStatsSize);
         osd_render_stats_video_decode(xStats, yStats-hStat, g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, fStatsSize);
         xStats -= osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode | g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, fStatsSize) + xSpacing;
      }
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_LINKS) )
   if ( g_bIsRouterReady )
   {
      osd_render_stats_local_radio_links( xStats, yStats-osd_render_stats_local_radio_links_get_height(&g_SM_RadioStats, fStatsSize), "Radio Links", &g_SM_RadioStats, fStatsSize);
      xStats -= osd_render_stats_local_radio_links_get_width(&g_SM_RadioStats, fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES) )
   if ( g_bIsRouterReady )
   {
      osd_render_stats_radio_interfaces( xStats, yStats-osd_render_stats_radio_interfaces_get_height(&g_SM_RadioStats), "Radio Interfaces", &g_SM_RadioStats);
      xStats -= osd_render_stats_radio_interfaces_get_width(&g_SM_RadioStats) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      osd_render_stats_efficiency(xStats, yStats - osd_render_stats_efficiency_get_height(fStatsSize), fStatsSize);
      xStats -= osd_render_stats_efficiency_get_width(fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_TELEMETRY_STATS) )
   {
      osd_render_stats_telemetry(xStats, yStats-osd_render_stats_telemetry_get_height(fStatsSize), fStatsSize);
      xStats -= osd_render_stats_telemetry_get_width(fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_AUDIO_DECODE_STATS) )
   {
      osd_render_stats_audio_decode(xStats, yStats-osd_render_stats_audio_decode_get_height());
      xStats -= osd_render_stats_audio_decode_get_width() + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RC) )
   {
      osd_render_stats_rc(xStats, yStats-osd_render_stats_rc_get_height(fStatsSize), fStatsSize);
      xStats -= osd_render_stats_rc_get_width(fStatsSize) + xSpacing;
   }

}

void _osd_render_stats_panels_vertical()
{
   Preferences* p = get_Preferences();
   float fStatsSize = 1.0;
  
   float hBar = osd_getBarHeight();
   float hBar2 = osd_getSecondBarHeight();
   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      hBar = 0.0;
      hBar2 = 0.0;
   }

   float yMin = osd_getMarginY() + hBar + hBar2 + 0.04; 
   float yMax = 1.0-osd_getMarginY() - hBar - hBar2 - 0.03;
   
   if ( g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLean )
      yMax -= 0.05; 

   float yStats = yMin;
   float xStats = 0.99-osd_getMarginX();
   float fMaxColumnWidth = 0.0;
   float fSpacingH = 0.01;
   float fSpacingV = 0.01;

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      xStats -= osd_getVerticalBarWidth();

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      if ( yStats + osd_render_stats_dev_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_dev(xStats-osd_render_stats_dev_get_width(), yStats, fStatsSize);
      yStats += osd_render_stats_dev_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_dev_get_width() )
         fMaxColumnWidth = osd_render_stats_dev_get_width();
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP) )
   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   {
      if ( yStats + osd_render_stats_graphs_vehicle_tx_gap_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_graphs_vehicle_tx_gap(xStats-osd_render_stats_graphs_vehicle_tx_gap_get_width(), yStats);
      yStats += osd_render_stats_graphs_vehicle_tx_gap_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_graphs_vehicle_tx_gap_get_width() )
         fMaxColumnWidth = osd_render_stats_graphs_vehicle_tx_gap_get_width();
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY) )
   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   {
      if ( yStats + osd_render_stats_video_bitrate_history_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_video_bitrate_history(xStats-osd_render_stats_video_bitrate_history_get_width(), yStats);
      yStats += osd_render_stats_video_bitrate_history_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_bitrate_history_get_width() )
         fMaxColumnWidth = osd_render_stats_video_bitrate_history_get_width();
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      if ( yStats + osd_render_stats_efficiency_get_height(fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_efficiency(xStats-osd_render_stats_efficiency_get_width(1.0), yStats, fStatsSize);
      yStats += osd_render_stats_efficiency_get_height(fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_efficiency_get_width(fStatsSize) )
         fMaxColumnWidth = osd_render_stats_efficiency_get_width(fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_LINKS) )
   if ( g_bIsRouterReady )
   {
      if ( yStats + osd_render_stats_local_radio_links_get_height(&g_SM_RadioStats, fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_local_radio_links( xStats-osd_render_stats_local_radio_links_get_width(&g_SM_RadioStats, fStatsSize), yStats, "Radio Links", &g_SM_RadioStats, fStatsSize);
      yStats += osd_render_stats_local_radio_links_get_height(&g_SM_RadioStats, fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_local_radio_links_get_width(&g_SM_RadioStats, fStatsSize) )
         fMaxColumnWidth = osd_render_stats_local_radio_links_get_width(&g_SM_RadioStats, fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES) )
   if ( g_bIsRouterReady )
   {
      if ( yStats + osd_render_stats_radio_interfaces_get_height(&g_SM_RadioStats) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_radio_interfaces( xStats-osd_render_stats_radio_interfaces_get_width(&g_SM_RadioStats), yStats, "Radio Interfaces", &g_SM_RadioStats);
      yStats += osd_render_stats_radio_interfaces_get_height(&g_SM_RadioStats);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_radio_interfaces_get_width(&g_SM_RadioStats) )
         fMaxColumnWidth = osd_render_stats_radio_interfaces_get_width(&g_SM_RadioStats);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_TELEMETRY_STATS) )
   {
      if ( yStats + osd_render_stats_telemetry_get_height(fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_telemetry(xStats-osd_render_stats_telemetry_get_width(fStatsSize), yStats, fStatsSize);
      yStats += osd_render_stats_telemetry_get_height(fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_telemetry_get_width(fStatsSize) )
         fMaxColumnWidth = osd_render_stats_telemetry_get_width(fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_AUDIO_DECODE_STATS) )
   {
      if ( yStats + osd_render_stats_audio_decode_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_audio_decode(xStats-osd_render_stats_audio_decode_get_width(), yStats);
      yStats += osd_render_stats_audio_decode_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_audio_decode_get_width() )
         fMaxColumnWidth = osd_render_stats_audio_decode_get_width();
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RC) )
   {
      if ( yStats + osd_render_stats_rc_get_height(fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_rc(xStats-osd_render_stats_rc_get_width(fStatsSize), yStats, fStatsSize);
      yStats += osd_render_stats_rc_get_height(fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_rc_get_width(fStatsSize) )
         fMaxColumnWidth = osd_render_stats_rc_get_width(fStatsSize);
   }

   
   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_VIDEO) )
   {
      if ( yStats + osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_video_decode(xStats - osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode, false,  &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize), yStats, g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize);
      yStats += osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize) )
         fMaxColumnWidth = osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, fStatsSize);
   }

   if ( p->iDebugShowFullRXStats )
      osd_render_stats_full_rx_port();
}

void _osd_stats_swap_pannels(int iIndex1, int iIndex2)
{
   int tmp = s_iOSDStatsBoundingBoxesIds[iIndex1];
   s_iOSDStatsBoundingBoxesIds[iIndex1] = s_iOSDStatsBoundingBoxesIds[iIndex2];
   s_iOSDStatsBoundingBoxesIds[iIndex2] = tmp;

   float f = s_iOSDStatsBoundingBoxesX[iIndex1];
   s_iOSDStatsBoundingBoxesX[iIndex1] = s_iOSDStatsBoundingBoxesX[iIndex2];
   s_iOSDStatsBoundingBoxesX[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesY[iIndex1];
   s_iOSDStatsBoundingBoxesY[iIndex1] = s_iOSDStatsBoundingBoxesY[iIndex2];
   s_iOSDStatsBoundingBoxesY[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesW[iIndex1];
   s_iOSDStatsBoundingBoxesW[iIndex1] = s_iOSDStatsBoundingBoxesW[iIndex2];
   s_iOSDStatsBoundingBoxesW[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesH[iIndex1];
   s_iOSDStatsBoundingBoxesH[iIndex1] = s_iOSDStatsBoundingBoxesH[iIndex2];
   s_iOSDStatsBoundingBoxesH[iIndex2] = f;
}

void _osd_stats_autoarange_left( int iCountArranged, int iCurrentColumn )
{
   if ( iCountArranged >= s_iCountOSDStatsBoundingBoxes )
      return;

   if ( 0 == iCountArranged && 0 == iCurrentColumn )
      for( int i=0; i<MAX_OSD_COLUMNS; i++ )
         s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   float fColumnCurrentHeight = 0.0;
   float fCurrentColumnX = s_fOSDStatsMarginHLeft;
   for( int i=0; i<iCurrentColumn; i++ )
      fCurrentColumnX += (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
   
   s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX;// + s_iOSDStatsBoundingBoxesW[iCountArranged];
   s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop;
   fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;  

   if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iCurrentColumn] )
      s_fOSDStatsColumnsWidths[iCurrentColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

   iCountArranged++;
   
   // Try to fit more panels to current column
 
   for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( fColumnCurrentHeight + s_iOSDStatsBoundingBoxesH[i] <= 1.0 - s_fOSDStatsMarginVBottom - s_fOSDStatsMarginVTop )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX;// + s_iOSDStatsBoundingBoxesW[iCountArranged];
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop + fColumnCurrentHeight;
         fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         iCountArranged++;
      }
   }

   // Still have panels to add? Move to next column
   if ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      _osd_stats_autoarange_left(iCountArranged, iCurrentColumn+1);
   }
}


void _osd_stats_autoarange_right( int iCountArranged, int iCurrentColumn )
{
   if ( iCountArranged >= s_iCountOSDStatsBoundingBoxes )
      return;

   if ( 0 == iCountArranged && 0 == iCurrentColumn )
      for( int i=0; i<MAX_OSD_COLUMNS; i++ )
         s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   float fColumnCurrentHeight = 0.0;
   float fCurrentColumnX = 1.0-s_fOSDStatsMarginHRight;
   for( int i=0; i<iCurrentColumn; i++ )
      fCurrentColumnX -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
   
   s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX - s_iOSDStatsBoundingBoxesW[iCountArranged];
   s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop;
   
   if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iCurrentColumn] )
      s_fOSDStatsColumnsWidths[iCurrentColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

   fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;  
   iCountArranged++;
   
   // Try to fit more panels to current column
 
   for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( fColumnCurrentHeight + s_iOSDStatsBoundingBoxesH[i] <= 1.0 - (s_fOSDStatsMarginVBottom+s_fOSDStatsMarginVTop) )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX - s_iOSDStatsBoundingBoxesW[iCountArranged];
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop + fColumnCurrentHeight;
         fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         iCountArranged++;
      }
   }

   // Still have panels to add? Move to next column

   if ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      _osd_stats_autoarange_right(iCountArranged, iCurrentColumn+1);
   }
}


void _osd_stats_autoarange_top()
{
   for( int i=0; i<MAX_OSD_COLUMNS; i++ )
      s_fOSDStatsColumnsHeights[i] = s_fOSDStatsMarginVTop;

   // Add first the tall ones that take full height

   int iCountArranged = 0;
   int iColumn = 0;
   float fRowCurrentWidth = 0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesH[i] >= 1.0 - s_fOSDStatsMarginVBottom - s_fOSDStatsMarginVTop - s_fOSDStatsWindowsMinimBoxHeight - s_fOSDStatsSpacingV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop;
         s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
         s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

         fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;   
         iCountArranged++;
         iColumn++;
      }
   }

   // Add to remaning empty columns

   if ( fRowCurrentWidth < 1.0 - 2.0*s_fOSDStatsMarginHRight )
   {
      for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
      {
         if ( fRowCurrentWidth + s_iOSDStatsBoundingBoxesW[iCountArranged] <= 1.0 - 2.0*s_fOSDStatsMarginHRight )
         {
            // Move it to sorted position
            _osd_stats_swap_pannels(iCountArranged, i);
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
            s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop;
            s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
            fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
           
            iCountArranged++;
            iColumn++;
         }
      }
   }
   
   // Add rempaining panels to where they can fit
   
   while ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      bool bFittedInColumn = false;
      for( int iColumn=0; iColumn<9; iColumn++ )
      {
         if ( s_fOSDStatsColumnsHeights[iColumn] <= s_fOSDStatsMarginVTop + 0.01 )
            continue;
         if ( s_fOSDStatsColumnsHeights[iColumn] + s_iOSDStatsBoundingBoxesH[iCountArranged]  <= 1.0 - 0.7*s_fOSDStatsMarginVBottom )
         {
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged];
            for( int i=0; i<iColumn; i++ )
                s_iOSDStatsBoundingBoxesX[iCountArranged] -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
            
            if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iColumn] )
            {
               float dx = s_iOSDStatsBoundingBoxesW[iCountArranged] - s_fOSDStatsColumnsWidths[iColumn];
               s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
               // Move all already arranged on next columns
               for( int i=0; i<iCountArranged; i++ )
                  if ( s_iOSDStatsBoundingBoxesColumns[i] >= iColumn )
                     s_iOSDStatsBoundingBoxesX[i] -= dx;
            }

            s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsColumnsHeights[iColumn];
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            
            bFittedInColumn = true;
            break;
         }
      }

      if ( ! bFittedInColumn )
      {
         //s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         //s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginVTop;
         //fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH; 

         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsColumnsHeights[9];
         s_fOSDStatsColumnsHeights[9] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
      }
      iCountArranged++;
   }
}

void _osd_stats_autoarange_bottom()
{
   for( int i=0; i<MAX_OSD_COLUMNS; i++ )
      s_fOSDStatsColumnsHeights[i] = s_fOSDStatsMarginVTop;

   // Add first the tall ones that take full height

   int iCountArranged = 0;
   int iColumn = 0;
   float fRowCurrentWidth = 0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesH[i] >= 1.0 - s_fOSDStatsMarginVBottom - s_fOSDStatsMarginVTop - s_fOSDStatsWindowsMinimBoxHeight - s_fOSDStatsSpacingV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginVTop - s_iOSDStatsBoundingBoxesH[iCountArranged];
         s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
         s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
         
         fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
         iCountArranged++;
         iColumn++;
      }
   }

   // Add to remaning empty columns

   if ( fRowCurrentWidth < 1.0 - 2.0*s_fOSDStatsMarginHRight )
   {
      for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
      {
         if ( fRowCurrentWidth + s_iOSDStatsBoundingBoxesW[iCountArranged] <= 1.0 - 2.0*s_fOSDStatsMarginHRight )
         {
            // Move it to sorted position
            _osd_stats_swap_pannels(iCountArranged, i);
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
            s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginVTop - s_iOSDStatsBoundingBoxesH[iCountArranged];
            s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
            fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
            iCountArranged++;
            iColumn++;
         }
      }
   }
   
   // Add rempaining panels to where they can fit
   
   while ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      bool bFittedInColumn = false;
      for( int iColumn=0; iColumn<9; iColumn++ )
      {
         if ( s_fOSDStatsColumnsHeights[iColumn] <= s_fOSDStatsMarginVTop + 0.01 )
            continue;
         if ( s_fOSDStatsColumnsHeights[iColumn] + s_iOSDStatsBoundingBoxesH[iCountArranged] <= 1.0 - 0.7*s_fOSDStatsMarginVBottom )
         {
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged];
            for( int i=0; i<iColumn; i++ )
                s_iOSDStatsBoundingBoxesX[iCountArranged] -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
            
            if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iColumn] )
            {
               float dx = s_iOSDStatsBoundingBoxesW[iCountArranged] - s_fOSDStatsColumnsWidths[iColumn];
               s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
               // Move all already arranged on next columns
               for( int i=0; i<iCountArranged; i++ )
                  if ( s_iOSDStatsBoundingBoxesColumns[i] >= iColumn )
                     s_iOSDStatsBoundingBoxesX[i] -= dx;
            }

            s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsColumnsHeights[iColumn] - s_iOSDStatsBoundingBoxesH[iCountArranged];
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            bFittedInColumn = true;
            break;
         }
      }

      if ( ! bFittedInColumn )
      {
         //s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         //s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginVTop - s_iOSDStatsBoundingBoxesH[iCountArranged];
         //fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH; 
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginHRight - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsColumnsHeights[9] - s_iOSDStatsBoundingBoxesH[iCountArranged];
         s_fOSDStatsColumnsHeights[9] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
      }
      iCountArranged++;
   }
}

void osd_render_stats_panels()
{
   if ( NULL == g_pCurrentModel )
      return;
   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pModel )
      return;

   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->disableAlphaBlending();

   int iOSDLayoutIndex = pModel->osd_params.iCurrentOSDScreen;

   Preferences* p = get_Preferences();

   s_idFontStats = g_idFontStats;
   s_idFontStatsSmall = g_idFontStatsSmall;

   s_OSDStatsLineSpacing = 1.0;

   //if ( osd_getScaleOSDStats() >= 1.0 )
   //   g_fOSDStatsForcePanelWidth = 0.2*osd_getScaleOSDStats();
   //else
   //   g_fOSDStatsForcePanelWidth = 0.2*osd_getScaleOSDStats();

   g_fOSDStatsForcePanelWidth = g_pRenderEngine->textWidth(s_idFontStats, "AAAAA AAAAAAAA AAAAAA AAAAAA");
   g_fOSDStatsForcePanelWidth += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   for( int i=0; i<MAX_OSD_COLUMNS; i++ )
      s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   s_iCountOSDStatsBoundingBoxes = 0;

   g_fOSDStatsBgTransparency = 1.0;
   switch ( ((pModel->osd_params.osd_preferences[iOSDLayoutIndex])>>20) & 0x0F )
   {
      case 0: g_fOSDStatsBgTransparency = 0.25; break;
      case 1: g_fOSDStatsBgTransparency = 0.5; break;
      case 2: g_fOSDStatsBgTransparency = 0.75; break;
      case 3: g_fOSDStatsBgTransparency = 0.98; break;
   }

   s_fOSDStatsSpacingH = 0.014/g_pRenderEngine->getAspectRatio();
   s_fOSDStatsSpacingV = 0.014;

   s_fOSDStatsMarginHLeft = osd_getMarginX() + 0.01/g_pRenderEngine->getAspectRatio();
   s_fOSDStatsMarginHRight = s_fOSDStatsMarginHLeft;
   s_fOSDStatsMarginVBottom = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.01;
   s_fOSDStatsMarginVTop = s_fOSDStatsMarginVBottom;


   if ( (pModel->osd_params.osd_flags[iOSDLayoutIndex] & OSD_FLAG_SHOW_CPU_INFO ) ||
        p->iShowControllerCPUInfo )
      s_fOSDStatsMarginVTop += osd_getFontHeight();
   
   if ( pModel->osd_params.osd_flags3[iOSDLayoutIndex] & OSD_FLAG3_LAYOUT_STATS_AUTO_WIDGETS_MARGINS )
   {
      // Add some room at margins for widgets, if widgets are present on the margins of the screen
      for( int i=0; i<osd_widgets_get_count(); i++ )
      {
         type_osd_widget* pWidget = osd_widgets_get(i);
         if ( NULL == pWidget )
            continue;
         for( int iModel=0; iModel<MAX_MODELS; iModel++ )
         {
            type_osd_widget_display_info* pWDI = &(pWidget->display_info[iModel][iOSDLayoutIndex]);
            if ( (pWDI->uVehicleId == 0) || (pWDI->uVehicleId == MAX_U32) )
               break;
            if ( ! pWDI->bShow )
               continue;

            // Right side widget?
            if ( pWDI->fXPos > 0.75 )
            if ( pWDI->fXPos + pWDI->fWidth > 0.9 )
               s_fOSDStatsMarginHRight += pWDI->fWidth + (1.0 - pWDI->fXPos - pWDI->fWidth);
            // Left side widget?

            if ( pWDI->fXPos < 0.1 )
               s_fOSDStatsMarginHLeft += pWDI->fWidth + pWDI->fXPos;
         }
      }
   }

   if ( pModel->osd_params.osd_flags2[iOSDLayoutIndex] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      s_fOSDStatsMarginHLeft = osd_getVerticalBarWidth() + osd_getMarginX() + 0.02;
      s_fOSDStatsMarginHRight = s_fOSDStatsMarginHLeft;
      s_fOSDStatsMarginVBottom = osd_getMarginY();
      s_fOSDStatsMarginVTop = s_fOSDStatsMarginVBottom;
   }

   if ( s_fOSDStatsMarginHLeft < 0.01 )
      s_fOSDStatsMarginHLeft = 0.01;
   if ( s_fOSDStatsMarginHRight < 0.01 )
      s_fOSDStatsMarginHRight = 0.01;
   if ( s_fOSDStatsMarginVBottom < 0.01 )
      s_fOSDStatsMarginVBottom = 0.01;
   if ( s_fOSDStatsMarginVTop < 0.01 )
      s_fOSDStatsMarginVTop = 0.01;
   
   // Max id used: 19

   if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_RADIO_RX_HISTORY_CONTROLLER )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 17;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_rx_type_history_get_width(false);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_rx_type_history_get_height(false);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 18;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_rx_type_history_get_width(true);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_rx_type_history_get_height(true);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO )
   if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 14;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_adaptive_video_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_adaptive_video_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_LINKS) )
   if ( g_bIsRouterReady )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 7;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_local_radio_links_get_width(&g_SM_RadioStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_local_radio_links_get_height(&g_SM_RadioStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES) )
   if ( g_bIsRouterReady )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 8;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_interfaces_get_width(&g_SM_RadioStats);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_interfaces_get_height(&g_SM_RadioStats);
      s_iCountOSDStatsBoundingBoxes++;
   }

   
   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_VIDEO) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 6;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }
   
   if ( p->iDebugShowVideoSnapshotOnDiscard )
   if ( g_bHasVideoDecodeStatsSnapshot )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 11;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_width(g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_height(g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 9;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_efficiency_get_width(1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_efficiency_get_height(1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pControllerSettings->iDeveloperMode && (pModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 12;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stream_h264_frames_info_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stream_h264_frames_info_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_TELEMETRY_STATS) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 15;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_telemetry_get_width(1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_telemetry_get_height(1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_AUDIO_DECODE_STATS) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 16;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_audio_decode_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_audio_decode_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_STATS_RC) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 10;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_rc_get_width(1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_rc_get_height(1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 1;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_dev_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_dev_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( g_pControllerSettings->iDeveloperMode || s_bDebugStatsShowAll )
   if ( pModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 4;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_graphs_vehicle_tx_gap_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_graphs_vehicle_tx_gap_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 5;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_bitrate_history_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_bitrate_history_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   s_fOSDStatsWindowsMinimBoxHeight = 2.0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      s_iOSDStatsBoundingBoxesColumns[i] = -1;

      if ( s_iOSDStatsBoundingBoxesH[i] < s_fOSDStatsWindowsMinimBoxHeight  )
         s_fOSDStatsWindowsMinimBoxHeight = s_iOSDStatsBoundingBoxesH[i];
   }


   // Auto arange

   if ( pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT )
      _osd_stats_autoarange_left(0, 0);
   else if ( pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT )
      _osd_stats_autoarange_right(0, 0);
   else if ( pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM )
      _osd_stats_autoarange_bottom();
   else
      _osd_stats_autoarange_top();

   // Draw

   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesIds[i] == 14 )
         osd_render_stats_adaptive_video(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 1 )
         osd_render_stats_dev(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 4 )
         osd_render_stats_graphs_vehicle_tx_gap(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 5 )
         osd_render_stats_video_bitrate_history(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 6 )
         osd_render_stats_video_decode(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], g_pControllerSettings->iDeveloperMode, false, &g_SM_RadioStats, &g_SM_VideoDecodeStats, 1.0);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 11 )
         osd_render_stats_video_decode(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], g_pControllerSettings->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, 1.0);
     
      if ( s_iOSDStatsBoundingBoxesIds[i] == 12 )
      {
        osd_render_stats_video_stream_h264_frames_info(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

        // Developer stats
        /*
        if ( g_pControllerSettings->iDeveloperMode )
        {
           float heightAdaptive = osd_render_stats_dev_adaptive_video_get_height();
           float xPosAdaptive = 0.32;
           float yPosAdaptive = 0.94 - heightAdaptive;
           float widthAdaptive = 0.36;
           osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
           g_pRenderEngine->drawRoundRect(xPosAdaptive - s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), yPosAdaptive - s_fOSDStatsMargin, widthAdaptive + 2.0 * s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), heightAdaptive + 2.0 * s_fOSDStatsMargin, 1.5*POPUP_ROUND_MARGIN);
           g_pRenderEngine->drawRoundRect(xPosAdaptive - s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), yPosAdaptive - s_fOSDStatsMargin, widthAdaptive + 2.0 * s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), heightAdaptive + 2.0 * s_fOSDStatsMargin, 1.5*POPUP_ROUND_MARGIN);
           g_pRenderEngine->drawRoundRect(xPosAdaptive - s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), yPosAdaptive - s_fOSDStatsMargin, widthAdaptive + 2.0 * s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), heightAdaptive + 2.0 * s_fOSDStatsMargin, 1.5*POPUP_ROUND_MARGIN);
           g_pRenderEngine->drawRoundRect(xPosAdaptive - s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), yPosAdaptive - s_fOSDStatsMargin, widthAdaptive + 2.0 * s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio(), heightAdaptive + 2.0 * s_fOSDStatsMargin, 1.5*POPUP_ROUND_MARGIN);
           osd_set_colors();

           osd_render_stats_dev_adaptive_video_info(xPosAdaptive, yPosAdaptive, widthAdaptive);
        }
        */
      }
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 7 )
         osd_render_stats_local_radio_links( s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], "Radio Links", &g_SM_RadioStats, 1.0);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 8 )
         osd_render_stats_radio_interfaces( s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], "Radio Interfaces", &g_SM_RadioStats);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 9 )
         osd_render_stats_efficiency(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 15 )
         osd_render_stats_telemetry(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 16 )
         osd_render_stats_audio_decode(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 10 )
         osd_render_stats_rc(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 17 )
         osd_render_stats_radio_rx_type_history(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], false);
      if ( s_iOSDStatsBoundingBoxesIds[i] == 18 )
         osd_render_stats_radio_rx_type_history(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], true);
      
      //char szBuff[32];
      //sprintf(szBuff, "%d", i);
      //g_pRenderEngine->drawText(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], s_idFontStats, szBuff);
   }


   // Draw the ones that should be on top first

   if ( s_bDebugStatsShowAll || (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS) )
      osd_render_video_frames_stats();

   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
   }

   if ( p->iDebugShowFullRXStats )
      osd_render_stats_full_rx_port();

   if ( g_pControllerSettings->iDeveloperMode )
   if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_VEHICLE_DEV_STATS )
      _osd_render_vehicle_dev_stats();

   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
}
