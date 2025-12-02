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
#include "../../base/ctrl_settings.h"
#include "../../base/ctrl_interfaces.h"
#include "../../common/string_utils.h"
#include "../../radio/radiolink.h"

#include <ctype.h>
#include <math.h>
#include "../shared_vars.h"
#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "osd_common.h"
#include "osd.h"
#include "osd_stats.h"
#include "osd_stats_dev.h"
#include "osd_stats_radio.h"
#include "../local_stats.h"
#include "../launchers_controller.h"
#include "../pairing.h"
#include "../timers.h"

extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphBottomLinesAlpha;
extern float s_fOSDStatsGraphLinesAlpha;
extern bool s_bDebugStatsShowAll;

u32 s_uTimeLastTXLoadTooBig = 0;
u16 s_uLastValueTXLoadTooBig = 0;

void osd_stats_set_last_tx_overload_time_ms(int iMS)
{
   s_uLastValueTXLoadTooBig = (u16)iMS;
}

void osd_render_stats_full_rx_port()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float lineHeight = height_text*s_OSDStatsLineSpacing*1.4;
   u32 fontId = s_idFontStatsSmall;

   float padding = 0.01 * g_pRenderEngine->getAspectRatio();
   float widthCol = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA");
   
   float width = 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_bIsRouterReady )
      width += widthCol * g_SM_RadioStats.countLocalRadioInterfaces;
   else
      width += 0.3;

   width += widthCol + 1.2*padding;

   float height = 0.12 + 0.34*osd_getScaleOSDStats();

   if ( width > 0.9 ) width = 0.9;
   if ( height > 0.9 ) height = 0.9;

   float xPos = 0.04;
   float yPos = 0.16;
   char szBuff[128];
   char szBuff2[64];

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);

   float alfa = 0.96;
   
   double pc[4];
   pc[0] = get_Color_OSDBackground()[0];
   pc[1] = get_Color_OSDBackground()[1];
   pc[2] = get_Color_OSDBackground()[2];
   pc[3] = 0.96;
   g_pRenderEngine->setColors(pc, alfa);

   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += 2*s_fOSDStatsMargin*0.7;
   width -= 4*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   float y = yPos;

   osd_set_colors();

   sprintf(szBuff, "Full Radio Interfaces RX Stats (update rate: %d ms)", g_SM_RadioStats.refreshIntervalMs);
   g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
   y += lineHeight;

   float y0 = y;

   if ( g_bIsRouterReady )
   {
      g_pRenderEngine->drawText(xPos, y, fontId, "Can't access radio RX stats.");
      g_pRenderEngine->setGlobalAlfa(fAlpha);
      return;
   }
 
   g_pRenderEngine->setStrokeSize(1.0);
   lineHeight = height_text*s_OSDStatsLineSpacing*1.3;

   for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      y = y0;
      if ( NULL == pNICInfo )
      {    
         g_pRenderEngine->drawText(xPos, y, fontId, "Can't access radio interface");
         xPos += 0.1;
         continue;
      }
      int iRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;

      char* szName = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szName && 0 != szName[0] )
         sprintf(szBuff, "%s%s (%s): %s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"), szName, pNICInfo->szDriver);
      else
         sprintf(szBuff, "%s%s: %s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"), pNICInfo->szDriver);

      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff2, "%.1f", (float)(g_SM_RadioStats.radio_interfaces[i].lastRecvDataRate));

      sprintf(szBuff, "Mode: N/A %s Mbs", removeTrailingZero(szBuff2));
      if ( g_SM_RadioStats.radio_interfaces[i].openedForWrite && g_SM_RadioStats.radio_interfaces[i].openedForRead )
         sprintf(szBuff, "Mode: TX/RX %s Mbs", removeTrailingZero(szBuff2)); 
      else if ( g_SM_RadioStats.radio_interfaces[i].openedForWrite )
         sprintf(szBuff, "Mode: TX %s Mbs", removeTrailingZero(szBuff2)); 
      else if (g_SM_RadioStats.radio_interfaces[i].openedForRead )
         sprintf(szBuff, "Mode: RX %s Mbs", removeTrailingZero(szBuff2));

      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      y += lineHeight + 0.01;

      g_pRenderEngine->drawText(xPos, y, fontId, "Rx Quality:");
      // To fix may2025? use proper dbm values
      sprintf(szBuff, "%d%%  %d dbm", g_SM_RadioStats.radio_interfaces[i].rxQuality, g_SM_RadioStats.radio_interfaces[i].signalInfo.signalInfoVideo.iDbmLast);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "Rx Relative Quality:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      if ( iRadioLinkId >= 0 )
      {
         sprintf(szBuff, "TX Time/Sec (link %d):", iRadioLinkId+1);
         g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
         // TO DO : to fix: show tx time per radio link
         //sprintf(szBuff, "%d ms", g_SM_RadioStats.radio_links[iRadioLinkId].downlink_tx_time_per_sec);
         
         // To fix 11.5
         //if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         //   sprintf(szBuff, "%d ms/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.txTimePerSec);
         //else
            strcpy(szBuff, "N/A ms/sec");
         g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      }
      else
      {
         g_pRenderEngine->drawText(xPos, y, fontId, "TX Time/Sec:");
         g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, "No Link Assigned");
      }
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "RX Total:");
      sprintf(szBuff, "%d kbps", g_SM_RadioStats.radio_interfaces[i].rxBytesPerSec * 8 / 1000 );
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "RX Packets/Sec:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].rxPacketsPerSec);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawLine(xPos + padding, y - 0.003, xPos + widthCol - 2.0*padding, y - 0.003 );

      g_pRenderEngine->drawText(xPos, y, fontId, "RX Recv Packets:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "RX OK Packets:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].totalRxPackets - g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "RX Lost Packets:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].totalRxPacketsLost);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "RX Broken Packets:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawLine(xPos + padding, y - 0.003, xPos + widthCol - 2.0*padding, y - 0.003 );

      g_pRenderEngine->drawText(xPos, y, fontId, "TX Packets/Sec:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].txPacketsPerSec);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, fontId, "TX Total Packets:");
      sprintf(szBuff, "%d", g_SM_RadioStats.radio_interfaces[i].totalTxPackets);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, fontId, szBuff);
      y += lineHeight;

      xPos += widthCol;
      //if ( i<g_SM_RadioStats.countLocalRadioInterfaces-1 )
         g_pRenderEngine->drawLine(xPos - padding, y0, xPos - padding, y);
   }

   y = y0;
   xPos += widthCol*0.04;
   float xEnd = xPos + widthCol - 0.1*padding;
   lineHeight = height_text*s_OSDStatsLineSpacing*1.2;

   g_pRenderEngine->drawText(xPos, y, fontId, "Streams Statistics:");
   y += 2.0 * lineHeight;

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      if ( g_SM_RadioStats.radio_streams[0][i].timeLastRxPacket == 0 && g_SM_RadioStats.radio_streams[0][i].timeLastTxPacket == 0 )
         continue;

      sprintf(szBuff, "%s Rx Total:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);

      if ( g_SM_RadioStats.radio_streams[0][i].totalRxBytes >= 1000000 )
         sprintf(szBuff, "%u MB", g_SM_RadioStats.radio_streams[0][i].totalRxBytes/1000/1000);
      else if ( g_SM_RadioStats.radio_streams[0][i].totalRxBytes >= 1000 )
         sprintf(szBuff, "%u kB", g_SM_RadioStats.radio_streams[0][i].totalRxBytes/1000);
      else
         sprintf(szBuff, "%u bytes", g_SM_RadioStats.radio_streams[0][i].totalRxBytes);
      g_pRenderEngine->drawTextLeft(xEnd, y, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      sprintf(szBuff, "%u kbps", g_SM_RadioStats.radio_streams[0][i].rxBytesPerSec * 8 / 1000);
      g_pRenderEngine->drawTextLeft(xEnd, y, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx Packets:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      sprintf(szBuff, "%u", g_SM_RadioStats.radio_streams[0][i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, fontId, szBuff);
      y += lineHeight;


      sprintf(szBuff, "%s Rx Lost Packets:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      sprintf(szBuff, "%u", g_SM_RadioStats.radio_streams[0][i].totalLostPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx Lost Percent:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, fontId, szBuff);
      sprintf(szBuff, "%.1f%%", (float)g_SM_RadioStats.radio_streams[0][i].totalLostPackets*100.0/(float)g_SM_RadioStats.radio_streams[0][i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, fontId, szBuff);
      y += lineHeight;

      y += 0.7 * lineHeight;
   }

   g_pRenderEngine->setGlobalAlfa(fAlpha);
}


float osd_render_stats_radio_interface_get_height_single_radio_int(shared_mem_radio_stats* pStats)
{
   if ( NULL == pStats )
      return 0.0;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = osd_getFontHeightSmall();
   float hGraph = height_text * 1.8;

   ControllerSettings* pCS = get_ControllerSettings();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   if ( NULL == pActiveModel )
      return 0.0;

   bool bIsMinimal = false;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS )
      bIsMinimal = true;


   float fHeightInterface = height_text*s_OSDStatsLineSpacing + hGraph;
   if ( ! bIsMinimal )
      fHeightInterface += height_text*s_OSDStatsLineSpacing;

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( ! bIsMinimal )
      fHeightInterface += 3.0 * ( height_text_small*s_OSDStatsLineSpacing );

   return fHeightInterface;
}

float osd_render_stats_radio_interfaces_get_height(shared_mem_radio_stats* pStats)
{
   if ( NULL == pStats )
      return 0.0;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = osd_getFontHeightSmall();
   float hGraph = height_text * 1.8;
   float height = 2.0 *s_fOSDStatsMargin*1.0;

   ControllerSettings* pCS = get_ControllerSettings();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   if ( NULL == pActiveModel )
      return 0.0;

   bool bIsMinimal = false;
   bool bIsCompact = false;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS )
      bIsMinimal = true;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT )
      bIsCompact = true;

   // Title
   if ( (!bIsCompact) && (!bIsMinimal) )
      height += 2.0*height_text*s_OSDStatsLineSpacing + s_fOSDStatsMargin*0.6;
   
   float fHeightInterface = osd_render_stats_radio_interface_get_height_single_radio_int(pStats);
   
   for (int i=0; i<pStats->countLocalRadioInterfaces; i++ )
   {
      height += fHeightInterface;
      if ( pStats->radio_interfaces[i].assignedLocalRadioLinkId < 0 )
         height = height - hGraph + height_text*s_OSDStatsLineSpacing;
   }
   
   height += height_text* 0.7 * (pStats->countLocalRadioInterfaces-1);

   if ( pActiveModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS )
   {
      int iCountConnectedVehicleInterfaces = 0;
      for( int i=0; i<pActiveModel->radioInterfacesParams.interfaces_count; i++ )
      {
         int iVehicleLink = pActiveModel->radioInterfacesParams.interface_link_id[i];
         if ( iVehicleLink < 0 )
            continue;
         for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[k].assignedVehicleRadioLinkId == iVehicleLink )
            {
               iCountConnectedVehicleInterfaces++;
               break;
            }
         }
      }
      height += height_text * s_OSDStatsLineSpacing * 0.7;
      if ( !bIsMinimal )
         height += height_text * s_OSDStatsLineSpacing;
      if ( (!bIsCompact) && (!bIsMinimal) )
         height += 1.0*(height_text*s_OSDStatsLineSpacing + height_text*0.6);
      height += ( height_text*s_OSDStatsLineSpacing*2.0 + hGraph ) * iCountConnectedVehicleInterfaces;
      if ( iCountConnectedVehicleInterfaces > 0 )
         height += height_text*1.0 * (iCountConnectedVehicleInterfaces-1);

      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      if ( ! bIsMinimal )
         height += 3.0 * ( height_text_small*s_OSDStatsLineSpacing ) * iCountConnectedVehicleInterfaces;
   }
   return height;
}

float osd_render_stats_radio_interfaces_get_width(shared_mem_radio_stats* pStats)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAA AAAAA AAAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}


float osd_render_stats_radio_interfaces( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats)
{
   if ( NULL == pStats || NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Pointer to %s stats shared mem is NULL. Can't render %s stats.", szTitle, szTitle);
      return 0.0;
   }

   ControllerSettings* pCS = get_ControllerSettings();
   int iRuntimeInfoToUse = osd_get_current_data_source_vehicle_index();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   if ( NULL == pActiveModel )
      return 0.0;
   if ( ! (pActiveModel->osd_params.osd_flags2[pActiveModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES) )
      return 0.0;
   
   bool bIsMinimal = false;
   bool bIsCompact = false;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS )
      bIsMinimal = true;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT )
      bIsCompact = true;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 1.8;
   
   float width = osd_render_stats_radio_interfaces_get_width(pStats);
   float height = osd_render_stats_radio_interfaces_get_height(pStats);
   
   char szBuff[128];
   char szDR[64];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;

   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   
   if ((!bIsCompact) && (!bIsMinimal) )
   {
      sprintf(szBuff, "%s", szTitle); 
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
   }
   osd_set_colors();

   double cBlueLine[4] = {80,80,250,1};
   double cBarRed[4] = {255,0,0,1};
   double cBarYellow[4] = {255,255,0,1};
   double cBarGreen[4] = {120,200,130,1};
   double cBar[4];
   memcpy(cBar, get_Color_OSDText(), 4*sizeof(double));
   
   float marginH = 0.0;//0.04/g_pRenderEngine->getAspectRatio();
   float maxWidth = width - marginH - 0.001;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();
   float y = yPos;

   if ( (!bIsCompact) && (!bIsMinimal) )
   {
      sprintf(szBuff,"%d ms/bar", pStats->graphRefreshIntervalMs);
      g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);

      float w = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
      sprintf(szBuff,"%.1f sec, ", (((float)MAX_HISTORY_RADIO_STATS_RECV_SLICES) * pStats->graphRefreshIntervalMs)/1000.0);
      g_pRenderEngine->drawTextLeft(rightMargin-w, yPos, s_idFontStatsSmall, szBuff);
      w += g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
      y += height_text*1.0*s_OSDStatsLineSpacing + 2.0*s_fOSDStatsMargin*0.3;
   }


   if ( (!bIsCompact) && (!bIsMinimal) )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Controller (Downlink) Rx Stats:");
      y += height_text*s_OSDStatsLineSpacing + height_text*0.4;
   }

   float hBar = 0.0;
   float hBarLostVideo = 0.0;
   float hBarLostData = 0.0;
   float dxBarWidth = maxWidth/(float)MAX_HISTORY_RADIO_STATS_RECV_SLICES;
   float fStroke = OSD_STRIKE_WIDTH;
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   char szName[100];

   static u8 sl_uLastCounterSlicesUpdatedInInterfacesGraph = 0;
   static int sl_iLastSecondSliceIndexInInterfacesGraph = 0;

   int iFirstUsedRadioInterface = -1;
   for( int i=0; i<pStats->countLocalRadioInterfaces; i++ )
   {
      if ( pStats->radio_interfaces[i].assignedLocalRadioLinkId != -1 )
      if ( ! hardware_radio_index_is_sik_radio(i) )
      {
         iFirstUsedRadioInterface = i;
         break;
      }
   }

   if ( pStats->radio_interfaces[iFirstUsedRadioInterface].hist_rxPacketsCurrentIndex != sl_uLastCounterSlicesUpdatedInInterfacesGraph )
   {
      if ( pStats->radio_interfaces[iFirstUsedRadioInterface].hist_rxPacketsCurrentIndex > sl_uLastCounterSlicesUpdatedInInterfacesGraph )
         sl_iLastSecondSliceIndexInInterfacesGraph += ((int)(pStats->radio_interfaces[iFirstUsedRadioInterface].hist_rxPacketsCurrentIndex) - (int)(sl_uLastCounterSlicesUpdatedInInterfacesGraph));
      else
         sl_iLastSecondSliceIndexInInterfacesGraph += ((int)(pStats->radio_interfaces[iFirstUsedRadioInterface].hist_rxPacketsCurrentIndex) + (MAX_HISTORY_RADIO_STATS_RECV_SLICES - (int)(sl_uLastCounterSlicesUpdatedInInterfacesGraph)));
      if ( sl_iLastSecondSliceIndexInInterfacesGraph >= MAX_HISTORY_RADIO_STATS_RECV_SLICES )
         sl_iLastSecondSliceIndexInInterfacesGraph = 0;
      sl_uLastCounterSlicesUpdatedInInterfacesGraph = pStats->radio_interfaces[iFirstUsedRadioInterface].hist_rxPacketsCurrentIndex;
   }

   int iCountSlicesInSecond = 100;
   if ( 0 != pStats->graphRefreshIntervalMs )
     iCountSlicesInSecond = 1000/pStats->graphRefreshIntervalMs;

   // ----------------------------------------------------------
   // Begin - render controller radio interfaces graphs

   for ( int i=0; i<pStats->countLocalRadioInterfaces; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;
      
      int iLocalRadioLinkId = pStats->radio_interfaces[i].assignedLocalRadioLinkId;
      int iVehicleRadioLinkId = pStats->radio_interfaces[i].assignedVehicleRadioLinkId;
      int iCountInterfacesAssignedToCurrentLocalLink = 0;
      if ( iLocalRadioLinkId >= 0 )
      {
         for( int k=0; k<pStats->countLocalRadioInterfaces; k++ )
            if ( pStats->radio_interfaces[k].assignedLocalRadioLinkId == iLocalRadioLinkId )
               iCountInterfacesAssignedToCurrentLocalLink++;
      }

      float ySt = y;
      bool bIsTxCard = false;
      
      //if ( 1 < pStats->countLocalRadioInterfaces )
      if ( (iLocalRadioLinkId >= 0) && (pStats->radio_links[iLocalRadioLinkId].lastTxInterfaceIndex == i) )
         bIsTxCard = true;

      if ( bIsTxCard && (iCountInterfacesAssignedToCurrentLocalLink > 1) && (1 < pStats->countLocalRadioInterfaces) )
      {
         float fmarginy = 0.002;
         float fmarginx = 0.008/g_pRenderEngine->getAspectRatio();
         double pC[4];
         memcpy(pC, get_Color_OSDBackground(), 4*sizeof(double));
         pC[0] += 20;
         pC[1] += 20;
         pC[2] += 20;
         pC[3] += 0.1;
         g_pRenderEngine->setFill(pC);
         g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], 0.0);

         float fHeightInterface = osd_render_stats_radio_interface_get_height_single_radio_int(pStats);

         g_pRenderEngine->drawRoundRect(xPos-fmarginx, ySt-1.5*fmarginy, rightMargin-xPos+2.0*fmarginx, fHeightInterface + 3.0*fmarginy + height_text*0.2, 0.05);
         osd_set_colors();
      }

      osd_set_colors();

      u8 uMaxGapMs = 0;
      u32 maxBadLost = 0;
      u32 uGapAverage = 0;
      
      if ( iLocalRadioLinkId >= 0 )
      { 
         g_pRenderEngine->setFill(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphBottomLinesAlpha);
         g_pRenderEngine->setStroke(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphBottomLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawLine(xPos+marginH,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + maxWidth, y+hGraph + g_pRenderEngine->getPixelHeight());

         float maxRecv = 0;
         u32 firstLostValue = 0;
         u32 uGapSum = 0;
         int iIndex = pStats->radio_interfaces[i].hist_rxPacketsCurrentIndex;
         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
             if ( pStats->radio_interfaces[i].hist_rxPacketsCount[iIndex] > maxRecv )
                maxRecv = pStats->radio_interfaces[i].hist_rxPacketsCount[iIndex];
             
             if ( (u32)(pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex]) > maxBadLost )
                maxBadLost = pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];

             if ( 0 == firstLostValue )
             if ( pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex] > 0 )
                firstLostValue = pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];

             if ( pStats->radio_interfaces[i].hist_rxGapMiliseconds[iIndex] != 0xFF )
             {
                uGapSum += pStats->radio_interfaces[i].hist_rxGapMiliseconds[iIndex];
                if ( pStats->radio_interfaces[i].hist_rxGapMiliseconds[iIndex] > uMaxGapMs )
                   uMaxGapMs = pStats->radio_interfaces[i].hist_rxGapMiliseconds[iIndex];
             }

             iIndex--;
             if ( iIndex < 0 )
                iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }

         uGapAverage = (uGapSum - uMaxGapMs) / (MAX_HISTORY_RADIO_STATS_RECV_SLICES-1);

         float xBar = xPos + marginH + maxWidth - dxBarWidth;
        
         iIndex = pStats->radio_interfaces[i].hist_rxPacketsCurrentIndex;

         if ( maxRecv >= 0.1 )
         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
            int iCountSliceLostVideo = pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex];
            int iCountSliceLostData = pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];
            hBar = hGraph * (float)(pStats->radio_interfaces[i].hist_rxPacketsCount[iIndex]) / maxRecv;
            hBarLostVideo = hGraph * (float)(iCountSliceLostVideo) / maxRecv;
            hBarLostData = hGraph * (float)(iCountSliceLostData) / maxRecv;
            if ( hBarLostVideo < 0.1 * hGraph )
               hBarLostVideo = 0.1 * hGraph;
            if ( hBarLostData < 0.1 * hGraph )
               hBarLostData = 0.1 * hGraph;

            if ( (hBar + hBarLostVideo + hBarLostData) > hGraph )
            {
               float fScale =  hGraph / (hBar + hBarLostVideo + hBarLostData);
               hBar = hBar * fScale;
               hBarLostVideo = hBarLostVideo * fScale;
               hBarLostData = hBarLostData * fScale;
            }
            
            if ( pStats->radio_interfaces[i].hist_rxPacketsCount[iIndex] > 0 )
            if ( hBar > 0.85 * hGraph )
               hBar = 0.85 * hGraph;

            float yBottomBar = y + hGraph;
            if ( iCountSliceLostVideo != 0 )
            {
               g_pRenderEngine->setStroke(cBarRed[0],cBarRed[1],cBarRed[2],0.9);
               g_pRenderEngine->setFill(cBarRed[0],cBarRed[1],cBarRed[2],0.9);
               g_pRenderEngine->setStrokeSize(fStroke);
               g_pRenderEngine->drawRect(xBar, yBottomBar - hBarLostVideo, dxBarWidth - fWidthPixel, hBarLostVideo);
               yBottomBar -= hBarLostVideo;
            }
            if ( iCountSliceLostData != 0 )
            {
               g_pRenderEngine->setStroke(cBarYellow[0],cBarYellow[1],cBarYellow[2],0.9);
               g_pRenderEngine->setFill(cBarYellow[0],cBarYellow[1],cBarYellow[2],0.9);
               g_pRenderEngine->setStrokeSize(fStroke);
               g_pRenderEngine->drawRect(xBar, yBottomBar - hBarLostData, dxBarWidth - fWidthPixel, hBarLostData);
               yBottomBar -= hBarLostData;
            }
            

            hBar = ((int)(hBar/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

            bool bShowGreen = false;
            if ( (0 == iCountSliceLostVideo) && ( 0 == iCountSliceLostData) )
            {
               for( int iInt=0; iInt<pStats->countLocalRadioInterfaces; iInt++ )
               {
                  if ( iInt != i )
                  //if ( (0 == pStats->radio_interfaces[iInt].hist_rxPacketsLostCountVideo[k]) && (0 == pStats->radio_interfaces[iInt].hist_rxPacketsLostCountData[k]) && (0 == pStats->radio_interfaces[iInt].hist_rxPacketsBadCount[k]) )
                  if ( (pStats->radio_interfaces[iInt].hist_rxPacketsLostCountVideo[iIndex] + pStats->radio_interfaces[iInt].hist_rxPacketsLostCountData[iIndex] + pStats->radio_interfaces[iInt].hist_rxPacketsBadCount[iIndex]) > 0 )
                  {
                     bShowGreen = true;
                     break;
                  }
               }
            }

            if ( bShowGreen )
            {
               g_pRenderEngine->setFill(cBarGreen[0], cBarGreen[1], cBarGreen[2], s_fOSDStatsGraphLinesAlpha);
               g_pRenderEngine->setStroke(cBarGreen[0], cBarGreen[1], cBarGreen[2], s_fOSDStatsGraphLinesAlpha);
            }
            else
            {
               g_pRenderEngine->setFill(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
               g_pRenderEngine->setStroke(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
            }
            g_pRenderEngine->setStrokeSize(fStroke);
            g_pRenderEngine->drawRect(xBar, yBottomBar - hBar, dxBarWidth - fWidthPixel, hBar);
            if ( ((k - sl_iLastSecondSliceIndexInInterfacesGraph) % iCountSlicesInSecond) == 0 )
            {
               g_pRenderEngine->setStroke(cBlueLine, 2.0);
               g_pRenderEngine->drawLine(xBar + dxBarWidth, y, xBar+dxBarWidth, y + hGraph);
               g_pRenderEngine->drawLine(xBar + dxBarWidth + fWidthPixel, y, xBar + dxBarWidth + fWidthPixel, y + hGraph);
            }
            xBar -= dxBarWidth;
            iIndex--;
            if ( iIndex < 0 )
               iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }

         osd_set_colors();

         if ( bIsTxCard && (iCountInterfacesAssignedToCurrentLocalLink > 1) && (1 < pStats->countLocalRadioInterfaces) )
            g_pRenderEngine->drawIcon(xPos, y-height_text*0.2, hGraph*0.7/g_pRenderEngine->getAspectRatio() , hGraph*0.7, g_idIconUplink2);

         y += hGraph;
      }
      else
      {
         g_pRenderEngine->setColors(cBarRed);
         g_pRenderEngine->drawText(xPos, y-0.2*height_text, s_idFontStats, "Interface is disabled");
         y += height_text*s_OSDStatsLineSpacing;
         osd_set_colors();
      }

      y += height_text*0.2;

      if ( hardware_radio_index_is_sik_radio(i) )
         str_format_bitrate(pActiveModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLinkId], szDR);
      else
         str_getDataRateDescriptionNoSufix(pStats->radio_interfaces[i].lastRecvDataRate, szDR);

      if ( bIsMinimal )
      {
         g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szDR);
         float fWT = g_pRenderEngine->textWidth(s_idFontStats, szDR);
         if ( bIsTxCard && (iCountInterfacesAssignedToCurrentLocalLink > 1) && (1 < pStats->countLocalRadioInterfaces) )
         {
            float fIconH = height_text*1.14;
            float fIconW = 0.6 * fIconH / g_pRenderEngine->getAspectRatio();
            g_pRenderEngine->drawIcon(rightMargin - fWT - fIconW - height_text_small*0.2, y-height_text*0.1, fIconW, fIconH, g_idIconArrowUp);
         }

         szName[0] = 0;
         controllerGetCardUserDefinedNameOrShortType(pNICInfo, szName);

         sprintf(szBuff, "Downlink (%s):", szName);
         if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
            sprintf(szBuff, "Downlink-%d (%s):", pStats->radio_interfaces[i].assignedVehicleRadioLinkId, szName);

         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         fWT = g_pRenderEngine->textWidth(s_idFontStats, szBuff);

         szBuff[0] = 0;
         if ( pStats->radio_interfaces[i].openedForWrite && pStats->radio_interfaces[i].openedForRead )
         {
            if ( bIsTxCard )
               strcpy(szBuff, "RX/TX");
            else
               strcpy(szBuff, "RX");
         }   
         else if ( pStats->radio_interfaces[i].openedForWrite )
            strcpy(szBuff, "TX Only");
         else if ( pStats->radio_interfaces[i].openedForRead )
            strcpy(szBuff, "RX Only");
         else
            strcpy(szBuff, "NOT USED");

         if ( controllerIsCardDisabled(pNICInfo->szMAC) )
            strcpy(szBuff, "DISABLED");
         g_pRenderEngine->drawText(xPos + fWT, y, s_idFontStats, szBuff);
         y += height_text*s_OSDStatsLineSpacing;

         y += height_text*0.7;
         continue;
      }

      // Line 1

      //char szBuffD[64];
      //char szBuffU[64];
      //str_format_bitrate(pStats->radio_interfaces[i].rxBytesPerSec * 8, szBuffD);
      //str_format_bitrate(pStats->radio_interfaces[i].txBytesPerSec * 8, szBuffU);
      //snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s / %s", szBuffU, szBuffD);
      str_format_bitrate(pStats->radio_interfaces[i].rxBytesPerSec * 8, szBuff);
      g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
      float fWT = g_pRenderEngine->textWidth(s_idFontStats, szBuff);
      if ( bIsTxCard && (iCountInterfacesAssignedToCurrentLocalLink > 1) && (1 < pStats->countLocalRadioInterfaces) )
      {
         float fIconH = height_text*1.14;
         float fIconW = 0.6 * fIconH / g_pRenderEngine->getAspectRatio();
         g_pRenderEngine->drawIcon(rightMargin - fWT - fIconW - height_text_small*0.2, y-height_text*0.1, fIconW, fIconH, g_idIconArrowUp);
      }

      
      szName[0] = 0;
      controllerGetCardUserDefinedNameOrShortType(pNICInfo, szName);
      strcpy(szBuff,szName);

      sprintf(szName, "%s%s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(Ext)"));
      if ( iLocalRadioLinkId < 0 )
         g_pRenderEngine->setColors(cBarRed);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szName);
      float fW = g_pRenderEngine->textWidth(s_idFontStats, szName);
      fW += height_text_small*0.3;
      if ( 0 != szBuff[0] )
      {
         snprintf(szName, sizeof(szName)/sizeof(szName[0]), "(%s)", szBuff);
         g_pRenderEngine->drawText(xPos + fW, y + 0.5*(height_text-height_text_small), s_idFontStatsSmall, szName);
         fW += height_text_small*0.3 + g_pRenderEngine->textWidth(s_idFontStatsSmall, szName);
      } 
      
      szBuff[0] = 0;
      if ( pStats->radio_interfaces[i].openedForWrite && pStats->radio_interfaces[i].openedForRead )
      {
         if ( bIsTxCard )
            strcpy(szBuff, "RX/TX");
         else
            strcpy(szBuff,"RX");
      }   
      else if ( pStats->radio_interfaces[i].openedForWrite )
         strcpy(szBuff,"TX Only");
      else if ( pStats->radio_interfaces[i].openedForRead )
         strcpy(szBuff,"RX Only");
      else
         strcpy(szBuff,"NOT USED");
      g_pRenderEngine->drawText(xPos + fW, y + 0.5*(height_text-height_text_small), s_idFontStatsSmall, szBuff);
      fW += height_text_small*0.3 + g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
      osd_set_colors();
      y += height_text*s_OSDStatsLineSpacing;

      // Line 2
      szName[0] = 0;
      szBuff[0] = 0;
      char szLinePrefix[128];
      //sprintf(szLinePrefix, "Link %d", iLocalRadioLinkId+1);
      sprintf(szLinePrefix, "%s", str_format_frequency(pStats->radio_interfaces[i].uCurrentFrequencyKhz));

      if ( ! hardware_radio_index_is_sik_radio(i) )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %d dbm SNR: %d", szLinePrefix, (int)g_fOSDDbm[i], (int)g_fOSDSNR[i]);
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s", szLinePrefix);
         
      if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "DISABLED");
      else if ( iLocalRadioLinkId < 0 || iLocalRadioLinkId >= MAX_RADIO_INTERFACES )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "No Radio Link");

      if ( ! controllerIsCardDisabled(pNICInfo->szMAC) )
      if ( pNICInfo->lastFrequencySetFailed || 0 == pStats->radio_interfaces[i].uCurrentFrequencyKhz )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Set Freq Failed");

      strcat(szBuff, " ");
      strcat(szBuff, szDR);
      
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);

      y += height_text*s_OSDStatsLineSpacing;

      // Line 3
      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         g_pRenderEngine->setColors(get_Color_Dev());
         sprintf(szBuff, "Avg / Max Gap: %d / %d ms, %d pkts", (int) uGapAverage, (int)uMaxGapMs, maxBadLost);
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         y += height_text_small * s_OSDStatsLineSpacing;

         int iTotalRecv = 0;
         int iTotalLost = 0;
         int iTotalBad = 0;
         int iSlicesCount = 0;
         int iIndex = pStats->radio_interfaces[i].hist_rxPacketsCurrentIndex;
         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
            iTotalRecv += pStats->radio_interfaces[i].hist_rxPacketsCount[iIndex];
            iTotalLost += pStats->radio_interfaces[i].hist_rxPacketsLostCountVideo[iIndex];
            iTotalLost += pStats->radio_interfaces[i].hist_rxPacketsLostCountData[iIndex];
            iTotalBad += pStats->radio_interfaces[i].hist_rxPacketsBadCount[iIndex];
            iSlicesCount++;
            iIndex--;
            if ( iIndex < 0 )
               iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }

         if ( iTotalRecv + iTotalLost > 0 )
            sprintf(szBuff, "Recv / Bad / Lost: %d / %d / %d, Lost: %.1f%%", iTotalRecv, iTotalBad, iTotalLost, 100.0*(float)iTotalLost/(float)(iTotalRecv+iTotalLost));
         else
            sprintf(szBuff, "Recv / Bad / Lost: %d/ %d / %d", iTotalRecv, iTotalBad, iTotalLost);
         g_pRenderEngine->setColors(get_Color_Dev());
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text_small * s_OSDStatsLineSpacing;

         sprintf(szBuff, "Rx / Tx packets / sec: %d / %d", pStats->radio_interfaces[i].rxPacketsPerSec, pStats->radio_interfaces[i].txPacketsPerSec);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text_small * s_OSDStatsLineSpacing;
         osd_set_colors();
      }
      y += height_text*0.7;
   }

   // End - render controller radio interfaces graphs
   // ----------------------------------------------------------------

   if ( ! (pActiveModel->osd_params.osd_flags2[pActiveModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS ) )
   {
      osd_set_colors();
      return height;
   }
   
   // -----------------------------------------------------------------------------------
   // Vehicle side

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_OSDText());
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos, y, xPos+width, y);
   g_pRenderEngine->drawLine(xPos, y + 4.0*g_pRenderEngine->getPixelHeight(), xPos+width, y + 4.0*g_pRenderEngine->getPixelHeight());
   y += height_text* s_OSDStatsLineSpacing;

   if ( (!bIsCompact) && (!bIsMinimal) )
   {
      y -= height_text*0.2;
      char szTmpName[128];
      if ( pActiveModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
         sprintf(szTmpName, "%s (Uplink) Rx Stats:", pActiveModel->getShortName());
      else
         strcpy(szTmpName, "Vehicle (Uplink) Rx Stats:");
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szTmpName);
      y += height_text*s_OSDStatsLineSpacing + height_text*0.6;
   }

   if ( ! g_VehiclesRuntimeInfo[iRuntimeInfoToUse].bGotStatsVehicleRxCards )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Data.");
      osd_set_colors();
      return height;
   }

   static u8 sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[MAX_RADIO_INTERFACES];
   static int sl_iLastSecondSliceIndexInVehicleInterfacesGraph[MAX_RADIO_INTERFACES];

   for( int i=0; i<pActiveModel->radioInterfacesParams.interfaces_count; i++ )
   {
      int iVehicleLink = pActiveModel->radioInterfacesParams.interface_link_id[i];
      if ( iVehicleLink < 0 )
         continue;
      bool bConnected = false;
      for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[k].assignedVehicleRadioLinkId == iVehicleLink )
         {
            bConnected = true;
            break;
         }
      }
      if ( ! bConnected )
         continue;

      shared_mem_radio_stats_radio_interface* pVehicleInterfaceRadioStats = &(g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[i]);

      if ( pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex != sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[i] )
      {
         if ( pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex > sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[i] )
            sl_iLastSecondSliceIndexInVehicleInterfacesGraph[i] += ((int)(pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex) - (int)(sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[i]));
         else
            sl_iLastSecondSliceIndexInVehicleInterfacesGraph[i] += ((int)(pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex) + (MAX_HISTORY_RADIO_STATS_RECV_SLICES - (int)(sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[i])));
         if ( sl_iLastSecondSliceIndexInVehicleInterfacesGraph[i] >= MAX_HISTORY_RADIO_STATS_RECV_SLICES )
            sl_iLastSecondSliceIndexInVehicleInterfacesGraph[i] = 0;
         sl_uLastCounterSlicesUpdatedInVehicleInterfacesGraph[i] = pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex;
      }

      osd_set_colors();

      g_pRenderEngine->setFill(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphBottomLinesAlpha);
      g_pRenderEngine->setStroke(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphBottomLinesAlpha);
      g_pRenderEngine->setStrokeSize(fStroke);
      g_pRenderEngine->drawLine(xPos+marginH,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + maxWidth, y+hGraph + g_pRenderEngine->getPixelHeight());
      float maxRecv = 0;
      u32 maxBadLost = 0;
      u32 firstLostValue = 0;
      u8 uMaxGapMs = 0;
      u32 uGapSum = 0;
      int iIndex = pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex;

      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
          if ( pVehicleInterfaceRadioStats->hist_rxPacketsCount[iIndex] > maxRecv )
             maxRecv = pVehicleInterfaceRadioStats->hist_rxPacketsCount[iIndex];

          if ( (u32)(pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex]) > maxBadLost )
             maxBadLost = pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex];

          if ( 0 == firstLostValue )
          if ( pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex] > 0 )
             firstLostValue = pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex];

          if ( pVehicleInterfaceRadioStats->hist_rxGapMiliseconds[iIndex] != 0xFF )
          {
             uGapSum += pVehicleInterfaceRadioStats->hist_rxGapMiliseconds[iIndex];
             if ( pVehicleInterfaceRadioStats->hist_rxGapMiliseconds[iIndex] > uMaxGapMs )
                uMaxGapMs = pVehicleInterfaceRadioStats->hist_rxGapMiliseconds[iIndex];
          }
          iIndex--;
          if ( iIndex < 0 )
             iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
      }
      u32 uGapAverage = (uGapSum - uMaxGapMs) / (MAX_HISTORY_RADIO_STATS_RECV_SLICES-1);

      float xBar = xPos + marginH + maxWidth - dxBarWidth;

      g_pRenderEngine->setFill(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStrokeSize(fStroke);
      
      iIndex = pStats->radio_interfaces[i].hist_rxPacketsCurrentIndex;
         
      if ( maxRecv >= 0.1 )
      if ( ! g_VehiclesRuntimeInfo[iRuntimeInfoToUse].bLinkLost )
      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         g_pRenderEngine->setFill(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(cBar[0], cBar[1], cBar[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
         
         hBar = hGraph * (float)(pVehicleInterfaceRadioStats->hist_rxPacketsCount[iIndex]) / maxRecv;
         hBarLostVideo = hGraph * (float)(pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex] + pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex]) / maxRecv;
         hBarLostData = hGraph * (float)(pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex]) / maxRecv;

         /*
         if ( k < MAX_HISTORY_RADIO_STATS_RECV_SLICES-1 )
         {
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[k+1] == 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsCount[k] > 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[k] > 0 )
               hBarLostData = 0.0;
         }
         if ( k < MAX_HISTORY_RADIO_STATS_RECV_SLICES-1 )
         {
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[k+1] == 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsCount[k] > 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[k] > 0 )
               hBarLostData = 0.0;
         }
         if ( k > 0 )
         {
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[k] == 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsCount[k-1] > 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[k-1] > 0 )
               hBarLostVideo = hGraph * (float)(pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[k-1] + pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[k-1]) / maxRecv;
         }
         if ( k > 0 )
         {
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[k] == 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsCount[k-1] > 0 )
            if ( pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[k-1] > 0 )
               hBarLostData = hGraph * (float)(pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[k-1]) / maxRecv;
         }
         */

         if ( (hBar + hBarLostVideo + hBarLostData) > hGraph )
         {
            float fScale = hGraph/(hBar + hBarLostVideo + hBarLostData);
            hBar *= fScale;
            hBarLostVideo *= fScale;
            hBarLostData *= fScale;
         }

         float yBottomBar = y + hGraph;
         if ( hBarLostVideo > 0.001 )
         {
            hBarLostVideo = ((int)(hBarLostVideo/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->setStroke(cBarRed[0],cBarRed[1],cBarRed[2],0.9);
            g_pRenderEngine->setFill(cBarRed[0],cBarRed[1],cBarRed[2],0.9);
            g_pRenderEngine->setStrokeSize(fStroke);
            g_pRenderEngine->drawRect(xBar, yBottomBar - hBarLostVideo, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBarLostVideo);
            yBottomBar -= hBarLostVideo;         
            g_pRenderEngine->setFill(cBar[0],cBar[1],cBar[2], s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setStroke(cBar[0],cBar[1],cBar[2], s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setStrokeSize(fStroke);
         }
         if ( hBarLostData > 0.001 )
         {
            hBarLostData = ((int)(hBarLostData/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->setStroke(cBarYellow[0],cBarYellow[1],cBarYellow[2],0.9);
            g_pRenderEngine->setFill(cBarYellow[0],cBarYellow[1],cBarYellow[2],0.9);
            g_pRenderEngine->setStrokeSize(fStroke);
            g_pRenderEngine->drawRect(xBar, yBottomBar - hBarLostData, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBarLostData);
            yBottomBar -= hBarLostData;         
            g_pRenderEngine->setFill(cBar[0],cBar[1],cBar[2], s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setStroke(cBar[0],cBar[1],cBar[2], s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->setStrokeSize(fStroke);
         }
         
         if ( hBar > 0.001 )
         {
            hBar = ((int)(hBar/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->drawRect(xBar, yBottomBar - hBar, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBar);
            yBottomBar -= hBar;
         }

         if ( ((k - sl_iLastSecondSliceIndexInVehicleInterfacesGraph[i]) % iCountSlicesInSecond) == 0 )
         {
            g_pRenderEngine->setStroke(cBlueLine, 2.0);
            g_pRenderEngine->drawLine(xBar, y, xBar, y + hGraph);
         }
         xBar -= dxBarWidth;
         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
      }

      osd_set_colors();

      y += hGraph;
      y += height_text*0.2;

      int iVehicleRadioLinkId = pActiveModel->radioInterfacesParams.interface_link_id[i];
      if ( pActiveModel->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
         str_format_bitrate(pActiveModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLinkId], szDR);
      else
      {
         if ( g_VehiclesRuntimeInfo[iRuntimeInfoToUse].bGotStatsVehicleRxCards )
            str_getDataRateDescription(g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[i].lastRecvDataRate, 0, szDR);
         else if ( (iVehicleRadioLinkId >= 0) && (iVehicleRadioLinkId < pActiveModel->radioLinksParams.links_count) )
            str_getDataRateDescription(pActiveModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLinkId], 0, szDR);
         else
            strcpy(szDR, "N/A");
      }

      if ( bIsMinimal )
      {
         g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szDR);

         sprintf(szBuff, "Uplink (%s):", str_get_radio_card_model_string_short(pActiveModel->radioInterfacesParams.interface_card_model[i]));
         if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
            sprintf(szBuff, "Uplink-%d (%s):", pActiveModel->radioInterfacesParams.interface_link_id[i], str_get_radio_card_model_string_short(pActiveModel->radioInterfacesParams.interface_card_model[i]));

         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         float fWT = g_pRenderEngine->textWidth(s_idFontStats, szBuff);

         szBuff[0] = 0;
         if ( pActiveModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            strcpy(szBuff, "DISABLED");

         g_pRenderEngine->drawText(xPos + fWT, y, s_idFontStats, szBuff);

         y += height_text*1.0;
         continue;
      }

      char szLinePrefix[128];

      if ( (iVehicleRadioLinkId >= 0) && (iVehicleRadioLinkId < MAX_RADIO_INTERFACES) )
         sprintf(szLinePrefix, "%s", str_format_frequency(pActiveModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]));
      else
         strcpy(szLinePrefix, "Not Used.");   
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s, %s", szLinePrefix, pActiveModel->radioInterfacesParams.interface_szPort[i]);
      
      if ( pActiveModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         sprintf(szBuff, "DISABLED");
      if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= MAX_RADIO_INTERFACES) )
         sprintf(szBuff, "No Radio Link");
      
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      float fW = g_pRenderEngine->textWidth(s_idFontStats, szBuff);
      sprintf(szBuff, "(%s)", str_get_radio_card_model_string_short(pActiveModel->radioInterfacesParams.interface_card_model[i]));
      g_pRenderEngine->drawText(xPos + fW + height_text_small*0.2, y + 0.5*(height_text-height_text_small), s_idFontStatsSmall, szBuff);
      fW += height_text_small*0.2 + g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);  

      y += height_text*s_OSDStatsLineSpacing;

      sprintf(szBuff, "RX Qual: %d%%", g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[i].rxQuality);

      strcat(szBuff, " ");
      strcat(szBuff, szDR);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         g_pRenderEngine->setColors(get_Color_Dev());
         sprintf(szBuff, "Avg / Max Gap: %d / %d ms, %d pkts", (int) uGapAverage, (int)uMaxGapMs, maxBadLost);
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         y += height_text_small * s_OSDStatsLineSpacing;

         int iTotalRecv = 0;
         int iTotalLost = 0;
         int iTotalBad = 0;
         int iSlicesCount = 0;
         iIndex = pVehicleInterfaceRadioStats->hist_rxPacketsCurrentIndex;
         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
            iTotalRecv += pVehicleInterfaceRadioStats->hist_rxPacketsCount[iIndex];
            iTotalLost += pVehicleInterfaceRadioStats->hist_rxPacketsLostCountVideo[iIndex];
            iTotalLost += pVehicleInterfaceRadioStats->hist_rxPacketsLostCountData[iIndex];
            iTotalBad += pVehicleInterfaceRadioStats->hist_rxPacketsBadCount[iIndex];
            iSlicesCount++;
            iIndex--;
            if ( iIndex < 0 )
               iIndex = MAX_HISTORY_RADIO_STATS_RECV_SLICES-1;
         }

         g_pRenderEngine->setColors(get_Color_Dev());

         sprintf(szBuff, "Recv / Bad / Lost: %d / %d / %d", iTotalRecv, iTotalBad, iTotalLost);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text_small * s_OSDStatsLineSpacing;

         sprintf(szBuff, "Rx / Tx packets / sec: %d / %d", pVehicleInterfaceRadioStats->rxPacketsPerSec, pVehicleInterfaceRadioStats->txPacketsPerSec);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text_small * s_OSDStatsLineSpacing;
         osd_set_colors();
      }

      y += height_text*1.0;
   }

   osd_set_colors();
   return height;
}


float osd_render_stats_local_radio_links_get_height(shared_mem_radio_stats* pRadioStats, float scale)
{
   if ( NULL == pRadioStats || NULL == g_pCurrentModel )
      return 0.0;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + height_text*s_OSDStatsLineSpacing;
   float hGraph = height_text*3.0;

   height += 1.3*height_text*s_OSDStatsLineSpacing;

   height += height_text*s_OSDStatsLineSpacing + 0.6*height_text;

   height += 2 * height_text * s_OSDStatsLineSpacing * g_pCurrentModel->radioLinksParams.links_count;

   ControllerSettings* pCS = get_ControllerSettings();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   int iCountVehicles = 1;
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      iCountVehicles++;

   int iCountRadioLinks = pRadioStats->countLocalRadioLinks;

   // Radio links round trip
   if ( 1 == iCountVehicles )
      height += height_text * s_OSDStatsLineSpacing * iCountRadioLinks;
   else
      height += height_text * s_OSDStatsLineSpacing * iCountRadioLinks * (1+iCountVehicles);

   // Retransmissions roundtrip
   if ( NULL != pActiveModel )
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      height += 4.0 * height_text * s_OSDStatsLineSpacing;

   if ( NULL != pActiveModel )
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      // Tx time, EC time, roundtrips
      height += 3 * height_text*s_OSDStatsLineSpacing + 0.3*height_text;

      height += height_text_small*s_OSDStatsLineSpacing; // Ping frequency
      height += height_text_small*s_OSDStatsLineSpacing; // Last response recv from vehicle

      height += hGraph + height_text_small*s_OSDStatsLineSpacing; // Radio rx queue graph
   }

   if ( NULL != pActiveModel )
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      int countStreams = 0;
      for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         if ( (pRadioStats->radio_streams[k][0].uVehicleId != 0) && (pRadioStats->radio_streams[k][i].totalRxPackets > 0) )
            countStreams++;

      height += countStreams * height_text * s_OSDStatsLineSpacing;
      height += 0.2 * height_text;

      height += g_pCurrentModel->radioLinksParams.links_count * height_text_small * s_OSDStatsLineSpacing;
   }
   return height;
}


float osd_render_stats_local_radio_links( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pRadioStats, float scale)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);
   if ( (NULL == pRadioStats) || (NULL == pVDS) || (NULL == pActiveModel) || (NULL == g_pCurrentModel) )
      return 0.0;

   ControllerSettings* pCS = get_ControllerSettings();
   float height_text = g_pRenderEngine->textHeight(s_idFontStats)*scale;
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall)*scale;

   float width = osd_render_stats_local_radio_links_get_width(pRadioStats, scale);
   
   float height = osd_render_stats_local_radio_links_get_height(pRadioStats, scale);
   
   char szBuff[128];
   char szBuff2[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;

   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   sprintf(szBuff, "%s", szTitle);
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      sprintf(szBuff, "%s (ping freq %d)", szTitle, pCS->nPingClockSyncFrequency );
   }
   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
   
   int nQualityLocalRadioLinks[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      nQualityLocalRadioLinks[i] = 0;

   for( int i=0; i<pRadioStats->countLocalRadioInterfaces; i++ )
   {
      int iLocalRadioLinkId = pRadioStats->radio_interfaces[i].assignedLocalRadioLinkId;
      if ( iLocalRadioLinkId < 0 )
         continue;
      if ( pRadioStats->radio_interfaces[i].rxQuality > nQualityLocalRadioLinks[iLocalRadioLinkId] )
         nQualityLocalRadioLinks[iLocalRadioLinkId] = pRadioStats->radio_interfaces[i].rxQuality;
   }
   
   bool bConnectedToVehicleRadioLinks[MAX_RADIO_INTERFACES];
   int iLocalRadioLinkIdForVehicleRadioLinks[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      bConnectedToVehicleRadioLinks[i] = false;
      iLocalRadioLinkIdForVehicleRadioLinks[i] = -1;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int iLocalRadioLinkId = pRadioStats->radio_interfaces[i].assignedLocalRadioLinkId;
      if ( iLocalRadioLinkId < 0 )
         continue;
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
      if ( iVehicleRadioLinkId < 0 )
         continue;
      bConnectedToVehicleRadioLinks[iVehicleRadioLinkId] = true;
      iLocalRadioLinkIdForVehicleRadioLinks[iVehicleRadioLinkId] = iLocalRadioLinkId;
   }

   double pc[4];
   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   bool bWarning = false;
   bool bCritical = false;
   for( int i=0; i<pRadioStats->countLocalRadioLinks; i++ )
   {
      if ( nQualityLocalRadioLinks[i] < OSD_QUALITY_LEVEL_WARNING )
         bWarning = true;
      if ( nQualityLocalRadioLinks[i] < OSD_QUALITY_LEVEL_CRITICAL )
         bCritical = true;
   }
   if ( bWarning )
      memcpy(pc, get_Color_IconWarning(), 4*sizeof(double));
   if ( bCritical )
      memcpy(pc, get_Color_IconError(), 4*sizeof(double));
   g_pRenderEngine->setColors(pc);

   szBuff[0] = 0;

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( ! bConnectedToVehicleRadioLinks[i] )
         continue;

      for( int iLocalLink=0; iLocalLink<pRadioStats->countLocalRadioLinks; iLocalLink++ )
      {
         if ( g_SM_RadioStats.radio_links[iLocalLink].matchingVehicleRadioLinkId != i )
            continue;
      
         char szQ[16];
         sprintf(szQ, "%d: %d%%", i+1, nQualityLocalRadioLinks[iLocalLink]);
         if ( 0 == szBuff[0] )
            strcpy(szBuff, szQ);
         else
         {
            strcat(szBuff, "  ");
            strcat(szBuff, szQ);
         }
         break;
      }
   }
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStats, szBuff);
   osd_set_colors();

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   //---------------------------------------------
   // Begin: Radio links round trip

   int iCountVehicles = 1;
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      iCountVehicles++;

   for( int iVehicleRadioLink=0; iVehicleRadioLink<g_pCurrentModel->radioLinksParams.links_count; iVehicleRadioLink++ )
   {
      if ( ! bConnectedToVehicleRadioLinks[iVehicleRadioLink] )
         continue;
      int iLocalRadioLinkId = iLocalRadioLinkIdForVehicleRadioLinks[iVehicleRadioLink];
      if ( 1 == iCountVehicles )
      {
         int iRTDelay = 0;
         controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, g_pCurrentModel->uVehicleId);
         if ( NULL != pRTInfoVehicle )
            iRTDelay = pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iLocalRadioLinkId]][iLocalRadioLinkId];
         sprintf(szBuff, "Link-%d RT delay:", iVehicleRadioLink+1);
         if ( (iRTDelay == 0) || (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY) )
            strcpy(szBuff2, "N/A");
         else
            sprintf(szBuff2, "%u ms", iRTDelay);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff, szBuff2);
         y += height_text*s_OSDStatsLineSpacing;
         continue;
      }

      sprintf(szBuff, "Link-%d RT delay:", iVehicleRadioLink+1);
      szBuff2[0] = 0;
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff, szBuff2);
      y += height_text*s_OSDStatsLineSpacing;

      for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[k] == 0 )
            continue;

         int iRTDelay = 0;
         controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[k]);
         if ( NULL != pRTInfoVehicle )
            iRTDelay = pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iLocalRadioLinkId]][iLocalRadioLinkId];

         Model* pModel = findModelWithId(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[k], 31);
         if ( NULL != pModel )
            sprintf(szBuff, "%s:",pModel->getLongName());
         else
            sprintf(szBuff, "Vehicle %d:", k+1);
         str_capitalize_first_letter(szBuff);
         if ( iRTDelay == 0 )
            strcpy(szBuff2, "N/A");
         else
            sprintf(szBuff2, "%u ms", iRTDelay);
         _osd_stats_draw_line(xPos + height_text, rightMargin, y, s_idFontStats, szBuff, szBuff2);
         y += height_text*s_OSDStatsLineSpacing;
      }
   }

   // End: Radio links round trip
   //---------------------------------------------

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      // To fix or remove
      /*
      shared_mem_controller_retransmissions_stats* pCRS = NULL;
      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( g_SM_ControllerRetransmissionsStats.video_streams[i].uVehicleId == uActiveVehicleId )
         {
            pCRS = &(g_SM_ControllerRetransmissionsStats.video_streams[i]);
            break;
         }
      }
      */
    
      /*
      int iIndexVehicleRuntimeInfo = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         {
            iIndexVehicleRuntimeInfo = i;
            break;
         }
      }
      */
      g_pRenderEngine->setColors(get_Color_Dev());
      strcpy(szBuff, "None");
      if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_BASIC )
         strcpy(szBuff, "Basic");
      if ( pActiveModel->rxtx_sync_type == RXTX_SYNC_TYPE_ADV )
         strcpy(szBuff, "Adv");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Clock sync type:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      // To fix
      /*
      if ( NULL != pCRS )
      {
         sprintf(szBuff, "%d ms", (int)pCRS->uMinPacketRetransmissionTime);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Retrans min time:", szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;
         sprintf(szBuff, "%d ms", (int)pCRS->uMaxPacketRetransmissionTime);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Retrans max time:", szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;
         sprintf(szBuff, "%d ms", (int)pCRS->uAvgPacketRetransmissionTime);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Retrans avg time:", szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;
      }
      sprintf(szBuff, "%u ms ago", g_TimeNow - pRadioStats->uTimeLastReceivedAResponseFromVehicle);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Last recv response:", szBuff);
      y += height_text_small*s_OSDStatsLineSpacing;
      */
      
      /*
      if ( -1 == iIndexVehicleRuntimeInfo )
         strcpy(szBuff, "N/A");
      else if ( (g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[iIndexVehicleRuntimeInfo] == MAX_U32) || (pActiveModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY) )
         strcpy(szBuff, "N/A");
      else
         sprintf(szBuff, "%d", g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[iIndexVehicleRuntimeInfo]);

      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Commands RT delay:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( -1 == iIndexVehicleRuntimeInfo )
         strcpy(szBuff, "N/A");
      if ( (g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[iIndexVehicleRuntimeInfo] == MAX_U32) || (pActiveModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY) )
         strcpy(szBuff, "N/A");
      else
         sprintf(szBuff, "%d", g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[iIndexVehicleRuntimeInfo]);
      
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Commands RT delay (min):", szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      */
   }
   osd_set_colors();

   for( int iVehicleRadioLink=0; iVehicleRadioLink<g_pCurrentModel->radioLinksParams.links_count; iVehicleRadioLink++ )
   {
      if ( ! bConnectedToVehicleRadioLinks[iVehicleRadioLink] )
         continue;
      int iLocalRadioLinkId = iLocalRadioLinkIdForVehicleRadioLinks[iVehicleRadioLink];

      str_format_bitrate(pRadioStats->radio_links[iLocalRadioLinkId].rxBytesPerSec*8, szBuff);
      sprintf(szBuff2, "Downlink %d (%s):", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();
   }

   for( int iVehicleRadioLink=0; iVehicleRadioLink<g_pCurrentModel->radioLinksParams.links_count; iVehicleRadioLink++ )
   {
      if ( ! bConnectedToVehicleRadioLinks[iVehicleRadioLink] )
         continue;
      int iLocalRadioLinkId = iLocalRadioLinkIdForVehicleRadioLinks[iVehicleRadioLink];

      str_format_bitrate(pRadioStats->radio_links[iLocalRadioLinkId].txBytesPerSec*8, szBuff);
      sprintf(szBuff2, "Uplink %d (%s):", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();
   }

   {
      sprintf(szBuff, "%d ms/sec", pVDS->uCurrentECTimeMsPerSec);
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
         sprintf(szBuff2, "(%s) Video Encoding:", pActiveModel->getShortName());
      else
         strcpy(szBuff2, "Video Encoding Time:");

      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      int iMaxTxTime = DEFAULT_TX_TIME_OVERLOAD;
      if ( ((pActiveModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) || ( (pActiveModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) )
         iMaxTxTime += 200;

      if ( pVDS->uCurrentTxTimeMsPerSec > iMaxTxTime )
      {
         s_uTimeLastTXLoadTooBig = g_TimeNow;
         s_uLastValueTXLoadTooBig = pVDS->uCurrentTxTimeMsPerSec;
      }
      if ( g_bHasVideoTxOverloadAlarm && (g_TimeLastVideoTxOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoTxOverloadAlarm + 5000) )
         s_uTimeLastTXLoadTooBig = g_TimeNow;

      if ( g_TimeNow < s_uTimeLastTXLoadTooBig + 1000 )
      {
         //g_pRenderEngine->setColors(get_Color_IconWarning());
         //if ( g_bHasVideoTxOverloadAlarm && (g_TimeLastVideoTxOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoTxOverloadAlarm + 5000) )
            g_pRenderEngine->setColors(get_Color_IconError());
      }
      else if ( pVDS->uCurrentTxTimeMsPerSec > iMaxTxTime/2 )
         g_pRenderEngine->setColors(get_Color_IconWarning());

      if ( (g_TimeNow < s_uTimeLastTXLoadTooBig + 1000) && (s_uLastValueTXLoadTooBig > 0) )
         sprintf(szBuff, "(max %d) %d ms/sec", (int)s_uLastValueTXLoadTooBig, pVDS->uCurrentTxTimeMsPerSec);
      else
         sprintf(szBuff, "%d ms/sec", pVDS->uCurrentTxTimeMsPerSec);

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
         sprintf(szBuff2, "(%s) TX Load:", pActiveModel->getShortName());
      else
         strcpy(szBuff2, "TX Load:");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();

      szBuff[0] = 0;
      int cst = pActiveModel->rxtx_sync_type;
      if ( cst == RXTX_SYNC_TYPE_NONE )
         strcat(szBuff, "None");
      if ( cst == RXTX_SYNC_TYPE_BASIC )
         strcat(szBuff, "Basic");
      if ( cst == RXTX_SYNC_TYPE_ADV )
         strcat(szBuff, "Advanced");

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
         sprintf(szBuff2, "(%s) RxTx Sync:", pActiveModel->getShortName());
      else
         strcpy(szBuff2, "RxTx Sync:");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         u32 ping_interval_ms = compute_ping_interval_ms(pActiveModel->uModelFlags, pActiveModel->rxtx_sync_type, pVDS->uCurrentVideoProfileEncodingFlags);
         sprintf(szBuff, "%d ms", ping_interval_ms);
         g_pRenderEngine->setColors(get_Color_Dev());
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Clock Sync Freq:", szBuff);
         osd_set_colors();
         y += height_text_small*s_OSDStatsLineSpacing;
      }
      y += height_text*0.3;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   for( int iVehicleRadioLink=0; iVehicleRadioLink<g_pCurrentModel->radioLinksParams.links_count; iVehicleRadioLink++ )
   {
      if ( ! bConnectedToVehicleRadioLinks[iVehicleRadioLink] )
         continue;
      int iLocalRadioLinkId = iLocalRadioLinkIdForVehicleRadioLinks[iVehicleRadioLink];
      g_pRenderEngine->setColors(get_Color_Dev());

      sprintf(szBuff2, "Uplink %d packets:", iVehicleRadioLink+1);
      sprintf(szBuff,"%d pk/sec", pRadioStats->radio_links[iLocalRadioLinkId].txPacketsPerSec);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, szBuff2, szBuff);
      y += height_text_small*s_OSDStatsLineSpacing;
      osd_set_colors();
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      double pColorDev[4];
      memcpy(pColorDev, get_Color_Dev(), 4*sizeof(double));

      g_pRenderEngine->setColors(pColorDev);
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d/sec", (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_video_packets_per_sec+g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_data_packets_per_sec));
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Total Packets:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_data_packets_per_sec);
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Data Packets:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;


      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d/sec", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_compacted_packets_per_sec);
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Data Packets-C:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      int iCountVehicles = 0;
      for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
         if ( pRadioStats->radio_streams[k][0].uVehicleId != 0 )
            iCountVehicles++;

      for( int k=0; k<MAX_CONCURENT_VEHICLES; k++ )
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( (pRadioStats->radio_streams[k][0].uVehicleId != 0) && (pRadioStats->radio_streams[k][i].totalRxPackets > 0) )
         {
            if ( iCountVehicles < 2 )
               sprintf(szBuff, "%s:", str_get_radio_stream_name(i));
            else
            {
               Model* pModelTmp = findModelWithId(pRadioStats->radio_streams[k][0].uVehicleId, 32);
               if ( NULL == pModelTmp )
                  sprintf(szBuff, "Unkown %s:", str_get_radio_stream_name(i));
               else
                  sprintf(szBuff, "%s %s:", pModelTmp->getShortName(), str_get_radio_stream_name(i));
            }
            str_format_bitrate(pRadioStats->radio_streams[k][i].rxBytesPerSec*8, szBuff2);
            _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff, szBuff2);
            y += height_text*s_OSDStatsLineSpacing;
         }
      }
      y += height_text*0.2;
   }

   // Radio Rx queue graph
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      float hGraph = height_text*3.0;
      float marginH = height_text*1.0;
      float maxWidth = width - marginH - 0.001;

      float dxBarWidth = maxWidth/(float)MAX_RADIO_RX_QUEUE_INFO_VALUES;
      float fStroke = OSD_STRIKE_WIDTH;

      double pColorDev[4];
      memcpy(pColorDev, get_Color_Dev(), 4*sizeof(double));
      g_pRenderEngine->setColors(pColorDev);

      g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "Radio Rx Queue size:");
      y += height_text_small*s_OSDStatsLineSpacing + height_text*0.5;
      hGraph -= height_text*0.6;

      g_pRenderEngine->drawLine(xPos + marginH, y - g_pRenderEngine->getPixelHeight(), xPos + marginH + maxWidth, y-g_pRenderEngine->getPixelHeight());
      g_pRenderEngine->drawLine(xPos + marginH, y+hGraph + g_pRenderEngine->getPixelHeight(), xPos + marginH + maxWidth, y+hGraph + g_pRenderEngine->getPixelHeight());

      u8 uMax = 0;
      for( int k=0; k<MAX_RADIO_RX_QUEUE_INFO_VALUES; k++ )
      {
          if ( g_SM_RadioRxQueueInfo.uPendingRxPackets[k] > uMax )
             uMax = g_SM_RadioRxQueueInfo.uPendingRxPackets[k]; 
      }
      sprintf(szBuff, "%d", (int)uMax);
      g_pRenderEngine->drawText(xPos, y-0.5*height_text_small, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph-0.5*height_text_small, s_idFontStatsSmall, "0");
      
      osd_set_colors();
      double pcBarRx[4];
      memcpy(pcBarRx, get_Color_OSDText(), 4*sizeof(double));
      g_pRenderEngine->setFill(pcBarRx[0], pcBarRx[1], pcBarRx[2], s_fOSDStatsGraphBottomLinesAlpha);
      g_pRenderEngine->setStroke(pcBarRx[0], pcBarRx[1], pcBarRx[2], s_fOSDStatsGraphBottomLinesAlpha);
      g_pRenderEngine->setStrokeSize(fStroke);

      float xBar = xPos + marginH + maxWidth;
      int iIndex = g_SM_RadioRxQueueInfo.uCurrentIndex;
      for( int k=0; k<MAX_RADIO_RX_QUEUE_INFO_VALUES; k++ )
      {
         xBar -= dxBarWidth;

         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_RADIO_RX_QUEUE_INFO_VALUES-1;

         if ( g_SM_RadioRxQueueInfo.uPendingRxPackets[iIndex] == 0 )
            continue;

         float hBar = hGraph * (float)(g_SM_RadioRxQueueInfo.uPendingRxPackets[iIndex]) / (float)uMax;
         g_pRenderEngine->drawRect(xBar, y+hGraph - hBar, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBar);
      }

      osd_set_colors();  
   }

   osd_set_colors();

   return height;
}



float osd_render_stats_radio_rx_type_history_get_height(bool bVehicle)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);

   float height = 1.6 *s_fOSDStatsMargin*1.3 + 2 * 0.7*height_text*s_OSDStatsLineSpacing;

   height += 7 * height_text * s_OSDStatsLineSpacing;
   
   return height;
}

float osd_render_stats_radio_rx_type_history_get_width(bool bVehicle)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}


float _osd_render_stats_radio_rx_type_history_interface( float xStart, float yStart, float fWidth, char* szName, shared_mem_radio_stats_interface_rx_hist* pData)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float y = yStart;
   u32 uFontId = s_idFontStats;
   float fWidthChar = 1.2*g_pRenderEngine->textWidth(uFontId, "X");
   char* szSymbol = NULL;
   
   if ( NULL != szName )
      g_pRenderEngine->drawText(xStart, yStart, s_idFontStats, szName);
   
   y += height_text * s_OSDStatsLineSpacing;
   
   if ( NULL == pData )
      return (y-yStart);

   double red[4] = {255,0,0,1.0};
   double blue[4] = {100,100,250,1.0};
      
   int iSlice = ( (pData->iCurrentSlice) % MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES);
         
   float xTmp = xStart;
   
   for( int k=0; k<MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES; k++ )
   {
      if ( (k == ((iSlice+1)%MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES) ) ||
           (k == ((iSlice+2)%MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES) ) )
      {
         if ( k == ((iSlice+2)%MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES) )
         {
            g_pRenderEngine->setColors(red);
            g_pRenderEngine->drawText(xTmp-fWidthChar*0.5, y, uFontId, "I");
            osd_set_colors();
         }
      }
      else   
      {
         if ( pData->uHistPacketsTypes[k] == 0x00 ) // Missing packets or none
         {
            if ( pData->uHistPacketsCount[k] > 0 )
               g_pRenderEngine->setColors(red);
            szSymbol = str_get_packet_history_symbol(pData->uHistPacketsTypes[k], pData->uHistPacketsCount[k]);
            g_pRenderEngine->drawText(xTmp, y + height_text*0.2, uFontId, szSymbol);
            osd_set_colors();
         }
         else if ( pData->uHistPacketsTypes[k] == 0xFF ) // End of a second
         {
            g_pRenderEngine->setColors(blue);
            g_pRenderEngine->drawText(xTmp, y + height_text*0.2, uFontId, "*");
            osd_set_colors();
         }
         else
         {
            szSymbol = str_get_packet_history_symbol(pData->uHistPacketsTypes[k], pData->uHistPacketsCount[k]);
            g_pRenderEngine->drawText(xTmp, y, uFontId, szSymbol);
         }
      }

      xTmp += fWidthChar;
      if ( (xTmp-xStart) > fWidth - fWidthChar )
      {
         xTmp = xStart;
         y += height_text*s_OSDStatsLineSpacing;
      }
   }
   y += height_text*s_OSDStatsLineSpacing;
   return (y-yStart);
}

float osd_render_stats_radio_rx_type_history( float xPos, float yPos, bool bVehicle)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   
   float width = osd_render_stats_radio_rx_type_history_get_width(bVehicle);
   float height = osd_render_stats_radio_rx_type_history_get_height(bVehicle);

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;

   if ( bVehicle )
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Vehicle Rx History");
   else
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Controller Rx History");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   int iCountInterfaces = hardware_get_radio_interfaces_count();
   if ( bVehicle && (NULL != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel) )
      iCountInterfaces = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->radioInterfacesParams.interfaces_count;
   
   if ( NULL == g_pSM_HistoryRxStats )
      return height;

   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( bVehicle && (NULL == pModel) )
   {
       g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Invalid Vehicle");
       osd_set_colors();
       return height;
   }

   for( int iInterface=0; iInterface<iCountInterfaces; iInterface++ )
   {
      char szName[128];
      szName[0] = 0;
      if ( bVehicle )
         sprintf(szName, "Radio Interface %s, %s", pModel->radioInterfacesParams.interface_szPort[iInterface], str_get_radio_card_model_string_short(pModel->radioInterfacesParams.interface_card_model[iInterface]));
      else
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
         if ( NULL == pRadioHWInfo )
            continue;
      
         char szTmp[128];
         szTmp[0] = 0;
         controllerGetCardUserDefinedNameOrType(pRadioHWInfo, szTmp);
         snprintf(szName, sizeof(szName)/sizeof(szName[0]), "Radio Interface %s, %s", pRadioHWInfo->szUSBPort, szTmp);
      }

      if ( bVehicle )
         y += _osd_render_stats_radio_rx_type_history_interface(xPos, y, widthMax, szName, &g_SM_HistoryRxStatsVehicle.interfaces_history[iInterface]);
      else
         y += _osd_render_stats_radio_rx_type_history_interface(xPos, y, widthMax, szName, &g_SM_HistoryRxStats.interfaces_history[iInterface]);
   }

   osd_set_colors();
   return height;
}


float osd_render_stats_local_radio_links_get_width(shared_mem_radio_stats* pStats, float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA A");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}
