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

#include "osd_common.h"
#include "osd.h"
#include "osd_ahi.h"
#include "osd_warnings.h"
#include "osd_stats.h"
#include "osd_debug_stats.h"
#include "osd_gauges.h"
#include "osd_lean.h"
#include "osd_plugins.h"
#include "osd_links.h"
#include "osd_widgets.h"
#include "../../common/string_utils.h"
#include "../../../mavlink/common/mavlink.h"
#include <math.h>


#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../colors.h"
#include "../../base/config.h"
#include "../../base/video_capture_res.h"
#include "../../base/models.h"
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_settings.h"
#include "../../base/hardware.h"
#include "../../base/hardware_procs.h"
#include "../../base/utils.h"
#include "../../base/tx_powers.h"

#include "../link_watch.h"
#include "../pairing.h"
#include "../local_stats.h"
#include "../launchers_controller.h"
#include "../../radio/radiopackets2.h"
#include "../../radio/radiolink.h"
#include "../timers.h"
#include "../warnings.h"


bool s_bDebugOSDShowAll = false;
bool s_bOSDDisableRendering = false;
u32 s_RenderCount = 0;

bool s_bShowOSDFlightEndStats = false;

bool s_ShowOSDFlightsStats = false;
u32 s_TimeOSDFlightsStatsOn = 0;
u32 s_uTimeStartFlashOSDElements = 0;


bool osd_is_debug()
{
   return s_bDebugOSDShowAll;
}

void osd_disable_rendering()
{
   s_bOSDDisableRendering = true;
}
void osd_enable_rendering()
{
   s_bOSDDisableRendering = false;
}


void osd_add_stats_flight_end()
{
   s_bShowOSDFlightEndStats = true;
}

void osd_remove_stats_flight_end()
{
   s_bShowOSDFlightEndStats = false;
}

bool osd_is_stats_flight_end_on()
{
   return s_bShowOSDFlightEndStats;
}

void osd_add_stats_total_flights()
{
   if ( s_ShowOSDFlightsStats )
      return;

   s_TimeOSDFlightsStatsOn = g_TimeNow;
   s_ShowOSDFlightsStats = true;
}


void osd_show_voltage(float x, float y, float voltage, bool bRightAlign)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL == pActiveModel) || (pActiveModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      return;

   osd_set_colors();

   bool bMultiLine = false;
   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      bMultiLine = true;
      bRightAlign = false;
   }
   float x0 = x;
   char szBuff[32];
   snprintf(szBuff, 31, "%.1f", voltage);
   
   if ( NULL != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel )
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->osd_params.battery_cell_count > 0 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->osd_params.battery_cell_count;

   float w = 0;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bWarningBatteryVoltage )
   {
      double pC[4];
      memcpy(pC, get_Color_OSDWarning(), 4*sizeof(double));
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
      g_pRenderEngine->setColors(pC);
      if ( bRightAlign )
         w = osd_show_value_sufix_left(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
      else
         w = osd_show_value_sufix(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
   }
   else
   {
      if ( bRightAlign )
         w = osd_show_value_sufix_left(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
      else
         w = osd_show_value_sufix(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
   }
   osd_set_colors();

   if ( NULL != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel )
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage > 1000 )
   {
      if ( g_pCurrentModel->osd_params.battery_cell_count > 0 )
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel->osd_params.battery_cell_count;
      else
      {
         float voltage = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage/1000.0;
         int cellCount = floor(voltage/4.3)+1;
         if ( cellCount > g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount )
         {
            g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount = cellCount;
            log_line("Updated battery cell count to %d (for received voltage of: %.2f)", cellCount, voltage );
         }
      }
   }

   if ( NULL != g_pCurrentModel )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BATTERY_CELLS ) )
   if ( 0 != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount )
   {
      x -= w;
      x -= 0.016*osd_getScaleOSD();
      if ( bMultiLine )
      {
         x = x0;
         y += osd_getFontHeightBig();
      }
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%.2f", voltage/(float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount);

      double pC[4];
      memcpy(pC, get_Color_OSDText(), 4*sizeof(double));
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bWarningBatteryVoltage )
         memcpy(pC, get_Color_OSDWarning(), 4*sizeof(double));
      
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bWarningBatteryVoltage )
      {
         if ( ( g_TimeNow / 500 ) % 2 )
            pC[3] = 0.0;
         g_pRenderEngine->setColors(pC);
      }
      if ( bRightAlign )
         w += osd_show_value_sufix_left(x,y,szBuff, "V", g_idFontOSDSmall, g_idFontOSDSmall);
      else
         w += osd_show_value_sufix(x,y,szBuff, "V", g_idFontOSDSmall, g_idFontOSDSmall);

      float height_text_s = g_pRenderEngine->textHeight(g_idFontOSDSmall);
      if ( 1 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount )
         strcpy(szBuff, "(1 cell)");
      else
         snprintf(szBuff, 31, "(%d cells)", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].iComputedBatteryCellCount);
      if ( bRightAlign )
         osd_show_value_left(x,y+height_text_s*0.8,szBuff,g_idFontOSDSmall);
      else
         osd_show_value(x,y+height_text_s*0.8,szBuff,g_idFontOSDSmall);
   }

   osd_set_colors();
}

void osd_show_amps(float x, float y, float amps, bool bRightAlign)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL == pActiveModel) || (pActiveModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      return;

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      bRightAlign = false;
   char szBuff[32];
   snprintf(szBuff, 31, "%.1f", amps);
   if ( bRightAlign )
      osd_show_value_sufix_left(x,y, szBuff, "A", g_idFontOSDBig, g_idFontOSD);
   else
      osd_show_value_sufix(x,y, szBuff, "A", g_idFontOSDBig, g_idFontOSD);
}

float osd_show_mah(float x, float y, u32 mah, bool bRightAlign)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL == pActiveModel) || (pActiveModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      return 0.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      bRightAlign = false;

   char szBuff[32];
   sprintf(szBuff, "%u", mah);

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      if ( bRightAlign )
         return osd_show_value_sufix_left(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSDSmall);
      else
         return osd_show_value_sufix(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSDSmall);
   }
   else
   {
      if ( bRightAlign )
         return osd_show_value_sufix_left(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSD);
      else
         return osd_show_value_sufix(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSD);
   }
}

float _osd_show_gps(float x, float y, bool bMultiLine)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL == pActiveModel) || (pActiveModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      return 0.0;

   int iOSVehicleDataSourceIndex = osd_get_current_data_source_vehicle_index();

   char szBuff[32];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();

   float x0 = x;
   float w = 0;
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) || (!g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].bGotFCTelemetry) )
   {
      x += osd_show_value(x,y, "No GPS", g_idFontOSD);
      x += osd_getSpacingH();
      return x-x0;
   }

   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.gps_fix_type >= GPS_FIX_TYPE_3D_FIX || (( g_TimeNow / 300 ) % 3) != 0 )
      g_pRenderEngine->drawIcon(x,y, 0.8*height_text_big/g_pRenderEngine->getAspectRatio(), 0.8*height_text_big, g_idIconGPS);
   x += 1.0*height_text_big/g_pRenderEngine->getAspectRatio();

   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.gps_fix_type >= GPS_FIX_TYPE_3D_FIX )
      g_pRenderEngine->drawText(x-0.3*height_text_big, y-0.7*osd_getSpacingV(), g_idFontOSDSmall, "3D");

   sprintf(szBuff, "%d", g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.satelites);
   w = osd_show_value(x,y, szBuff, g_idFontOSDBig);
   x += w + 0.5*osd_getSpacingH();

   if ( pairing_isStarted() )
   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].pModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
   if ( vehicle_runtime_has_received_fc_telemetry(g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].uVehicleId) )
   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].pModel->is_spectator || g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].bPairedConfirmed )
   {
      if ( bMultiLine )
      {
         osd_show_value_centered((x0+x)*0.5, y+height_text_big-height_text*0.1, "HDOP", g_idFontOSDSmall);  
         sprintf(szBuff, "%.1f", g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.hdop/100.0);
         osd_show_value_centered((x0+x)*0.5, y+height_text_big-height_text*0.1+osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
      }
      else
      {
         w = osd_show_value(x, y-height_text*0.1, "HDOP", g_idFontOSDSmall);  
         sprintf(szBuff, "%.1f", g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.hdop/100.0);
         osd_show_value(x, y + osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
         x += w;
         x += osd_getSpacingH();
      }
   }
   u16 hdop2 = (((u16)g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.extra_info[3])<<8) + ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[4]);
   
   bool bShowMore = false;
   //if ( (g_pCurrentModel->iGPSCount > 1) || ((hdop2 != 0xFFFF) && (hdop2 != 0)) || ((satelites2 != 0xFF)&&(satelites2 != 0)) )
   //   bShowMore = true;
   if ( g_pCurrentModel->iGPSCount > 1 )
      bShowMore = true;

   if ( ! bShowMore )
      return x-x0;

   // Second GPU
   
   float x1 = x;

   if ( ((g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.extra_info[2] >= GPS_FIX_TYPE_3D_FIX) && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[2] != 0xFF)) || (( g_TimeNow / 300 ) % 3) != 0 )
      g_pRenderEngine->drawIcon(x,y, 0.8*height_text_big/g_pRenderEngine->getAspectRatio(), 0.8*height_text_big, g_idIconGPS);
   x += 1.0*height_text_big/g_pRenderEngine->getAspectRatio();

   if ( (g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.extra_info[2] >= GPS_FIX_TYPE_3D_FIX) && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[2] != 0xFF) )
      g_pRenderEngine->drawText(x-0.3*height_text_big, y-0.7*osd_getSpacingV(), g_idFontOSDSmall, "3D");

   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.extra_info[1] == 0xFF )
      strcpy(szBuff, "N/A");
   else
      sprintf(szBuff, "%d", g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].headerFCTelemetry.extra_info[1]);
   w = osd_show_value(x,y, szBuff, g_idFontOSDBig);
   x += w + 0.5*osd_getSpacingH();

   if ( pairing_isStarted() )
   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].pModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
   if ( vehicle_runtime_has_received_fc_telemetry(g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].uVehicleId) )
   if ( g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].pModel->is_spectator || g_VehiclesRuntimeInfo[iOSVehicleDataSourceIndex].bPairedConfirmed )
   {
      if ( bMultiLine )
      {
         osd_show_value_centered((x1+x)*0.5, y+height_text_big-height_text*0.1, "HDOP", g_idFontOSDSmall);
         if ( hdop2 == 0xFFFF )
            strcpy(szBuff, "---");
         else
            sprintf(szBuff, "%.1f", hdop2/100.0);
         osd_show_value_centered((x1+x)*0.5, y+height_text_big-height_text*0.1+osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
      }
      else
      {
         w = osd_show_value(x, y-height_text*0.1, "HDOP", g_idFontOSDSmall);  
         if ( hdop2 == 0xFFFF )
            strcpy(szBuff, "---");
         else
            sprintf(szBuff, "%.1f", hdop2/100.0);
         osd_show_value(x, y + osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
         x += w;
         x += osd_getSpacingH();
      }
   }
   return x- x0;
}

float osd_show_flight_mode(float x, float y)
{
   char szBuff[32];
   strcpy(szBuff, "NONE");
   u8 mode = 0;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      mode = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED);

   strcpy(szBuff, model_getShortFlightMode(mode));

   osd_set_colors_background_fill();

   float w = 2.3*osd_getBarHeight()/g_pRenderEngine->getAspectRatio();
   if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
      w = w * 1.2;

   g_pRenderEngine->drawRect(x, y, w, osd_getBarHeight() );

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iDeveloperMode && ((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].pModel != NULL) && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvAnyRubyTelemetry > g_TimeNow-70)) )
      g_pRenderEngine->setColors(get_Color_Dev());
   else
      osd_set_colors();
   
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED) )
   {
      float yPos = y + 0.5*(osd_getBarHeight()-2.0*osd_getSpacingV()-osd_getFontHeight());
      osd_show_value_centered(x+0.5*w,yPos, szBuff, g_idFontOSD);
      if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
         osd_show_value_centered(x+0.5*w, yPos+0.9*osd_getFontHeight(), "SPECTATOR", g_idFontOSDSmall);
   }
   else
   {
      osd_show_value_centered(x+0.5*w,y+0.5*osd_getSpacingV(), szBuff, g_idFontOSD);
      osd_show_value_centered(x+0.5*w,y+0.7*osd_getSpacingV()+0.9*osd_getFontHeight(), "DISARMED", g_idFontOSDSmall);
      if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
         osd_show_value_centered(x+0.5*w, y+0.7*osd_getSpacingV()+1.8*osd_getFontHeight(), "SPECTATOR", g_idFontOSDSmall);
   }

   osd_set_colors();
   return w;
}

float osd_show_video_link_mbs(float xPos, float yPos, bool bLeft)
{
   char szBuff[32];
   char szSuffix[32];
   float w = 0;
   szSuffix[0] = 0;

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pActiveModel )
      return 0.0;

   bool bMultiLine = false;
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      bMultiLine = true;

   u32 uVideoBitrate = 0;
   u32 totalMaxVideo_bps = 0;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      uVideoBitrate = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_video_bitrate_bps;
   if ( g_bIsRouterReady )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId != pActiveModel->uVehicleId )
            continue;
         totalMaxVideo_bps = g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].rxBytesPerSec * 8;
         break;
      }
   }

   u32 uMaxVideoRadioBitrate = pActiveModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();

   strcpy(szSuffix, "Mbps");

   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      sprintf(szBuff,"%.1f", totalMaxVideo_bps/1000.0/1000.0);
   else
      //sprintf(szBuff, "%.1f (%.1f)", totalMaxVideo_bps/1000.0/1000.0, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.downlink_tx_video_bitrate_bps/1000.0/1000.0);
      //sprintf(szBuff, "%.1f", uVideoBitrate/1000.0/1000.0);
      sprintf(szBuff,"%.1f", totalMaxVideo_bps/1000.0/1000.0);

   if ( uVideoBitrate >= uMaxVideoRadioBitrate )
      g_pRenderEngine->setColors(get_Color_IconWarning());
   if ( g_bHasVideoDataOverloadAlarm && (g_TimeLastVideoDataOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoDataOverloadAlarm + 5000) )
      g_pRenderEngine->setColors(get_Color_IconError());

   if ( bLeft )
   {
      if ( bMultiLine )
      {
         float height_text = osd_getFontHeight();
         osd_show_value_left(xPos,yPos, szBuff, g_idFontOSD);
         yPos += height_text;
         osd_show_value_left(xPos,yPos, szSuffix, g_idFontOSDSmall);
      }
      else
         w = osd_show_value_sufix_left(xPos,yPos, szBuff, szSuffix, g_idFontOSDSmall, g_idFontOSDSmall);
   }
   else
      w = osd_show_value_sufix(xPos,yPos, szBuff, szSuffix, g_idFontOSDSmall, g_idFontOSDSmall);

   osd_set_colors();
   return w;
}

float osd_show_armtime(float xPos, float yPos, bool bRender)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;

   Model* pModel = osd_get_current_data_source_vehicle_model();
   int iRuntimeIndex = osd_get_current_data_source_vehicle_index();
   if ( (NULL == pModel) || (iRuntimeIndex < 0) )
      return 0.0;

   int sec = (pModel->m_Stats.uCurrentFlightTime)%60;
   int min = (pModel->m_Stats.uCurrentFlightTime)/60;

   int sec_on = (pModel->m_Stats.uCurrentOnTime)%60;
   int min_on = (pModel->m_Stats.uCurrentOnTime)/60;

   float w = 0;
   char szBuff[32];

   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();

   if ( bRender )
      g_pRenderEngine->drawIcon(xPos, yPos+0.07*height_text_big, 0.86*height_text_big/g_pRenderEngine->getAspectRatio(), 0.86*height_text_big, g_idIconClock);
   w += 1.1*height_text_big/g_pRenderEngine->getAspectRatio();
   float w0 = w;

   if ( (pModel->m_Stats.uCurrentFlightTime > 0) && (g_VehiclesRuntimeInfo[iRuntimeIndex].bIsArmed) )
   {
      yPos += 0.5*(osd_getBarHeight()-2.0*osd_getSpacingV()-height_text);	
      sprintf(szBuff, "%d", min);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSD);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSD, szBuff);

      w += height_text*0.11;
      if ( (pModel->m_Stats.uCurrentFlightTime % 2) == 0 )
      {
         bool bDrawBGBox = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         if ( bRender )
            osd_show_value(xPos+w,yPos, ":", g_idFontOSD);
         
         g_pRenderEngine->drawBackgroundBoundingBoxes(bDrawBGBox);
      }
      w += height_text*0.17;
      sprintf(szBuff, "%02d", sec);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSD);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSD, szBuff);
      return w;
   }
   else
   {
      sprintf(szBuff, "%d", min_on);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSDSmall);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff);
      w += height_text*0.11;
      if ( (pModel->m_Stats.uCurrentOnTime % 2) == 0 )
      {
         bool bDrawBGBox = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         if ( bRender )
            osd_show_value(xPos+w,yPos, ":", g_idFontOSDSmall);

         g_pRenderEngine->drawBackgroundBoundingBoxes(bDrawBGBox);
      }
      w += height_text*0.17;
      sprintf(szBuff, "%02d", sec_on);

      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSDSmall);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff);

      yPos += osd_getFontHeightSmall();
      if ( bRender )
         osd_show_value(xPos+w0,yPos, "ON TIME", g_idFontOSDSmall);
      return w;
   }

   return w;
}

// Returns the width

float osd_show_home(float xPos, float yPos, bool showHeading, float fScale)
{
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) )
      return 0.0;

   if ( (!s_bDebugOSDShowAll) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE) )
      return 0.0;

   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      return 0.0;

   float height_text = osd_getFontHeight() * fScale;
   float height_text_big = osd_getFontHeightBig() * fScale;

   bool showHome = false;
   if ( NULL != g_pCurrentModel && (0 < g_pCurrentModel->iGPSCount) )
      showHome = true;
   if ( s_bDebugOSDShowAll )
      showHome = true;

   if ( ! showHome )
      return 0.0;

   float dy = 0.07*height_text_big;
   float fWidth = 1.1*height_text_big/g_pRenderEngine->getAspectRatio();
   
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || ((g_TimeNow/500)%2) )
      g_pRenderEngine->drawIcon(xPos, yPos+dy, 0.86*height_text_big/g_pRenderEngine->getAspectRatio(), 0.86*height_text_big, g_idIconHome);

   if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet) && (!s_bDebugOSDShowAll) )
      return fWidth;
   if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) && (!s_bDebugOSDShowAll) )
      return fWidth;

   xPos += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   fWidth = 2.0*height_text/g_pRenderEngine->getAspectRatio();

   float heading_home = osd_course_to(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLat, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLon, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon);
   if ( g_pCurrentModel->osd_params.invert_home_arrow )
      heading_home = osd_course_to(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLat, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLon);
   heading_home = heading_home - 180.0;

   float rel_heading = heading_home - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading; //direction arrow needs to point relative to camera/osd/craft
   if ( g_pCurrentModel->osd_params.invert_home_arrow )
      rel_heading = 180.0 - rel_heading;
   rel_heading = rel_heading + g_pCurrentModel->osd_params.home_arrow_rotate + 180;
   if (rel_heading < 0) rel_heading += 360.0;
   if (rel_heading >= 360) rel_heading -= 360.0;

   //static float rel_heading = 20;
   //rel_heading += 2.0;
   //if ( rel_heading > 360 )
   //   rel_heading = 0.0;

   fScale = (osd_getBarHeight()-2.0*osd_getSpacingV())/0.18;
   float x[5] = {0.0f,  -0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f, 0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f};
   float y[5] = {0.08f*fScale, 0.15f*fScale, -0.15f*fScale, 0.15f*fScale, 0.08f*fScale};
   for( int i=0; i<5; i++ )
      x[i] = x[i]/g_pRenderEngine->getAspectRatio();
   
   osd_rotatePoints(x, y, rel_heading, 5, xPos,yPos + 0.48*(osd_getBarHeight()-2.0*osd_getSpacingV()), 0.34);

   g_pRenderEngine->setColors(get_Color_OSDText());

   float fStroke = g_pRenderEngine->getStrokeSize();
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->fillPolygon(x,y,5);

   double pc2[4];
   pc2[0] = 0; pc2[1] = 0; pc2[2] = 0;
   pc2[3] = 0.3;
   g_pRenderEngine->setColors(pc2);
   g_pRenderEngine->setStroke(pc2, 1.5);
   g_pRenderEngine->drawPolyLine(x,y,5);
   g_pRenderEngine->setStrokeSize(fStroke);
   osd_set_colors();

   return fWidth + 0.01;
}

// Returns new xPos
float osd_show_txpowers(float xPos)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pActiveModel )
      return xPos;

   float height_text = osd_getFontHeight();

   for( int iVehicleRadioLink=pActiveModel->radioLinksParams.links_count-1; iVehicleRadioLink >= 0; iVehicleRadioLink-- )
   {
      char szTxPower[32];
      bool bRed = true;
      bool bYellow = false;
      bool bPITMode = false;

      t_structure_vehicle_info* pVRTInfo = get_vehicle_runtime_info_for_vehicle_id(pActiveModel->uVehicleId);
      if ( (NULL != pVRTInfo) && pVRTInfo->bGotRubyTelemetryInfo && (pVRTInfo->headerRubyTelemetryExtended.uExtraRubyFlags & FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE) )
         bPITMode = true;
      if ( (NULL != pVRTInfo) && pVRTInfo->bGotRubyTelemetryInfo && (pVRTInfo->headerRubyTelemetryExtended.uExtraRubyFlags & FLAG_RUBY_TELEMETRY_EXTRA_FLAGS_IS_IN_TX_PIT_MODE_HOT) )
         bPITMode = true;

      strcpy(szTxPower, "0 mW");
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         strcpy(szTxPower, "- mW");
      else if ( pVRTInfo->headerRubyTelemetryExtended.iTxPowers[iVehicleRadioLink] == 0 )
      {
         strcpy(szTxPower, "N/A mW");
         bYellow = true;
      }
      else
      {
         /*
         int iPowerMw = -1;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iVehicleRadioLink )
               continue;
            if ( ! hardware_radio_type_is_ieee(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
               continue;

            int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
            iPowerMw = tx_powers_convert_raw_to_mw(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i]);
            break;
         }
         */
         char szPowerPref[16];
         szPowerPref[0] = 0;
         int iPowerMw = pVRTInfo->headerRubyTelemetryExtended.iTxPowers[iVehicleRadioLink];
         if ( iPowerMw < 0 )
         {
            iPowerMw = -iPowerMw;
            strcpy(szPowerPref, "*");
         }
         if ( iPowerMw < 1000 )
            sprintf(szTxPower, "%s%d mW", szPowerPref, iPowerMw);
         else
            sprintf(szTxPower, "%s%.1f W", szPowerPref, ((float)iPowerMw)/1000.0);

         if ( iPowerMw >= 0 )
            bRed = false;
         if ( iPowerMw < 10 )
            bYellow = true;

         if ( !(g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS) )
            bRed = true;

         if ( !(g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
            bRed = true;
      }
      if ( bRed || bPITMode )
         g_pRenderEngine->setColors(get_Color_IconError());
      else if ( bYellow )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      else
         g_pRenderEngine->setColors(get_Color_OSDText());

      float fyPower = osd_getMarginY() + 0.5*osd_getSpacingV();
      float fPowerWidth = g_pRenderEngine->textWidth(g_idFontOSD, szTxPower);
      float fwPowerIcon = osd_getFontHeight()*0.6;
      osd_show_value_left(xPos, fyPower, szTxPower, g_idFontOSD);
      if ( bPITMode )
         osd_show_value(xPos-fPowerWidth*0.7, fyPower + height_text, "PIT", g_idFontOSD);
      else
         g_pRenderEngine->drawIcon(xPos - 0.5*fPowerWidth - 0.5 * fwPowerIcon, fyPower + height_text, fwPowerIcon, fwPowerIcon * g_pRenderEngine->getAspectRatio(), g_idIconRadio);
      g_pRenderEngine->setColors(get_Color_OSDText());
      xPos -= fPowerWidth;
      xPos -= osd_getSpacingH();
   }
   return xPos;
}

float osd_show_throttle(float xPos, float yPos, bool bLeft)
{
   char szBuff[32];
   float height_text = osd_getFontHeight();
   float dy = 0.5*(osd_getBarHeight() - height_text - 2.0*osd_getSpacingV());

   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%d%%", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle);
   else
      strcpy(szBuff, "0%");

   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryExtraInfo )
   {
      u16 uThrottleInput = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfo.uThrottleInput;
      if ( uThrottleInput > 2500 )
         uThrottleInput = 2500;
      char szTmp[32];
      sprintf(szTmp, " %d", (int)uThrottleInput);
      strcat(szBuff, szTmp);
   }

   float w = 0.0;
   if ( bLeft )
   {
      w = osd_show_value_left(xPos, yPos+dy,szBuff,g_idFontOSD);
      g_pRenderEngine->drawIcon(xPos - w - 0.4*osd_getSpacingH() - 1.2*height_text/g_pRenderEngine->getAspectRatio(), yPos+dy-height_text*0.1, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconThrotle);
      w += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   }
   else
   {
      g_pRenderEngine->drawIcon(xPos, yPos+dy-height_text*0.1, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconThrotle);
      w = osd_show_value(xPos+0.4*osd_getSpacingH() + 1.2*height_text/g_pRenderEngine->getAspectRatio(), yPos+dy,szBuff,g_idFontOSD);
      w += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   }
   return w;
}


float _osd_show_rc_rssi(float xPos, float yPos, float fScale)
{
   char szBuff[32];
   bool bHasRubyRC = false;
   
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   int iRuntimeIndex = osd_get_current_data_source_vehicle_index();
   if ( (NULL == pActiveModel) || (iRuntimeIndex < 0) )
      return 0.0;

   if ( pActiveModel->rc_params.rc_enabled && (!pActiveModel->is_spectator) )
      bHasRubyRC = true;
 
   osd_set_colors();

   float x = xPos;
   float y = yPos;

   strcpy(szBuff, "RC RSSI: N/A");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
   {
      int val = 0;
      if ( bHasRubyRC )
         val = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_rc_rssi;
      else if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         val = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_mavlink_rc_rssi;
      
      //if ( ! bHasRubyRC )
      //if ( val == 0 || val == 255 )
      //if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RX_RSSI )
      //   val = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_mavlink_rx_rssi;

      if ( val != 255 )
      {
         sprintf(szBuff, "RC RSSI: %d%%", val);

         bool bShowYellow = false;
         bool bShowRed = false;

         if ( (val > 0) && (val <= 50) )
            bShowRed = true;
         else if ( (val > 0) && (val <= 75) )
            bShowYellow = true;

         if ( (g_TimeNow/500) % 2 )
         {
            if ( bShowRed )
            {
               double color[4] = {255,100,100,1.0};
               g_pRenderEngine->setColors(color);
               g_pRenderEngine->setStroke(0,0,0,0.5);
               g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
            }
            if ( bShowYellow )
            {
               double color[4] = {255,255,50,1.0};
               g_pRenderEngine->setColors(color);
               g_pRenderEngine->setStroke(0,0,0,0.5);
               g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
            }
         }
      }
      else
         strcpy(szBuff, "RC RSSI: ---");
   }
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRCFailsafeState )
      strcpy(szBuff, "!RC FS!");

   float w = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff);
   if ( (!g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRCFailsafeState) || (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRCFailsafeState && (( g_TimeNow / 300 ) % 3 )) )
      osd_show_value(x-w,y, szBuff, g_idFontOSDSmall);
   osd_set_colors();
   return w;
}

float osd_show_controller_voltage(float xPos, float yPos, bool bSmall)
{
   float height_text = osd_getFontHeight();
   float x0 = xPos;
   ControllerSettings* pCS = get_ControllerSettings();
   if ( hardware_i2c_has_current_sensor() )
   if ( pCS->iShowVoltage )
   {
      if ( NULL == g_pSMVoltage )
         g_pSMVoltage = shared_mem_i2c_current_open_for_read();
      if ( NULL == g_pSMVoltage )
         return 0.0;
      float fVoltage = 0.0;
      float fCurrent = 0.0;
      if ( g_SMVoltage.lastSetTime != MAX_U32 )
      if ( g_SMVoltage.voltage != MAX_U32 )
         fVoltage = g_SMVoltage.voltage*0.001f;
      if ( g_SMVoltage.lastSetTime != MAX_U32 )
      if ( g_SMVoltage.current != MAX_U32 )
         fCurrent = g_SMVoltage.current*0.1f;

      char szBuff[32];
      if ( g_SMVoltage.uParam == 0 )
         sprintf(szBuff, "%.1f V", fVoltage);
      if ( g_SMVoltage.uParam == 1 )
         sprintf(szBuff, "%d mA", (int)fCurrent);
      if ( g_SMVoltage.uParam == 2 )
         sprintf(szBuff, "%.1f V, %d mA", fVoltage, (int)fCurrent);
      if ( bSmall )
         xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      else
         xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSD);
      xPos -= height_text*0.2;
   }
   return x0 - xPos;
}

float osd_show_cpus(float xPos, float yPos, float fScale )
{
   char szBuff[32];
   float height_text = osd_getFontHeightSmall();
   float x0 = xPos;
   float dxIcon = 1.5*height_text/g_pRenderEngine->getAspectRatio();

   bool bMultiLine = false;
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT) )
      bMultiLine = true;

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   osd_set_colors();

   // Vehicle side
   //----------------------

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_CPU_INFO ) )
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      {
         bool bF = false;
         if ( (NULL != pActiveModel) && (pActiveModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_TEMPS_F) )
            bF = true;
         if ( bF )
            sprintf(szBuff, "%d F",  (int)osd_convertTemperature((float)(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.temperatureC), true));
         else
            sprintf(szBuff, "%d C",  (int)osd_convertTemperature((float)(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.temperatureC), false));
         if ( (NULL != pActiveModel) && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.temperatureC >= ((pActiveModel->hwCapabilities.uHWFlags & 0xFF00)>>8)-5) )
         if ( (g_TimeNow/500)%2 )
            g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( (NULL != pActiveModel) && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.temperatureC >= ((pActiveModel->hwCapabilities.uHWFlags & 0xFF00)>>8)) )
         if ( (g_TimeNow/500)%2 )
            g_pRenderEngine->setColors(get_Color_IconError());
      }
      else
         strcpy(szBuff, "N/A");
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos += height_text*0.16;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconTemperature);
      xPos -= dxIcon;
      xPos -= height_text*0.2;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }

      osd_set_colors();

      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d Mhz",  (int)((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.cpu_mhz));// * 0.953)); // 1024 scalling
      else
         sprintf(szBuff, "- Mhz");
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.3;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d %%", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.cpu_load);
      else
         strcpy(szBuff, "- %%");

      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.2;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconCPU);
      xPos -= dxIcon;
      xPos -= height_text*0.24;

      xPos -= osd_show_value_left(xPos, yPos, "V:", g_idFontOSDSmall);
      xPos -= height_text*0.4;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
   }

   // Controller side
   // --------------------------

   Preferences* p = get_Preferences();
   if ( s_bDebugOSDShowAll || p->iShowControllerCPUInfo )
   {
      xPos -= height_text*0.5;

      if ( bMultiLine )
      {
         yPos += height_text*0.6;
         xPos = x0;
      }

      bool bF = false;
      if ( (NULL != pActiveModel) && (pActiveModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_TEMPS_F) )
         bF = true;
      if ( bF )
         sprintf(szBuff, "%d F",  (int)osd_convertTemperature(g_iControllerCPUTemp, true));
      else
         sprintf(szBuff, "%d C",  (int)osd_convertTemperature(g_iControllerCPUTemp, false));

      if ( g_iControllerCPUTemp >= 70 )
      if ( (g_TimeNow/500)%2 )
         g_pRenderEngine->setColors(get_Color_IconWarning());

      if ( g_iControllerCPUTemp >= 75 )
      if ( (g_TimeNow/500)%2 )
         g_pRenderEngine->setColors(get_Color_IconError());

      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos += height_text*0.16;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconTemperature);
      xPos -= dxIcon;
      xPos -= height_text*0.2;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      osd_set_colors();

      sprintf(szBuff, "%d Mhz",  (int)((float)g_iControllerCPUSpeedMhz)); // * 0.953) ); // 1024 scaling
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.3;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }

      sprintf(szBuff, "%d %%", g_iControllerCPULoad);
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.2;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconCPU);
      xPos -= dxIcon;
      xPos -= height_text*0.24;
      xPos -= osd_show_value_left(xPos, yPos, "C:", g_idFontOSDSmall);
      xPos -= height_text*0.24;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      xPos -= osd_show_controller_voltage(xPos, yPos, true);

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
   }
   return x0 - xPos;
}

float osd_show_gps_pos(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      return 0.0;
   char szBuff[32];
   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      sprintf(szBuff, "Lon: %.6f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLon);
   else
      sprintf(szBuff, "Lat: %.6f,", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLat);
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()]) & OSD_FLAG_SCRAMBLE_GPS) )
   {
      int index = 0;
      while ( index < (int)strlen(szBuff) )
      {
         if ( szBuff[index] == '.' )
            break;
         szBuff[index] = 'X';
         index++;
      }
   }

   osd_set_colors();

   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      yPos += osd_getFontHeightSmall();
   else
      xPos += w + osd_getSpacingH()*0.7;

   sprintf(szBuff, "Lon: %.6f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLon);
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()]) & OSD_FLAG_SCRAMBLE_GPS) )
   {
      int index = 0;
      while ( index < (int)strlen(szBuff) )
      {
         if ( szBuff[index] == '.' )
            break;
         szBuff[index] = 'X';
         index++;
      }
   }

   w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);
   xPos += w + osd_getSpacingH()*0.7;
   return 0.0;
}

float osd_show_total_distance(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      return 0.0;
   Preferences *pP = get_Preferences();
   char szBuff[128];
   char szPrefix[32];
   strcpy(szPrefix, "Total:");
   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      strcpy(szPrefix, "T:");
   if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
   {
      if ( _osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100.0/1000.0) > 1.0 )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %.1f mi", szPrefix, (float)_osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100.0/1000.0));
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %d ft", szPrefix, (int)_osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100));
   }
   else
   {
      if ( _osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100.0/1000.0) > 1.0 )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %.1f km", szPrefix, (float)_osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100.0/1000.0));
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %d m", szPrefix, (int)_osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100));
   }
   osd_set_colors();
   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);
   return w;
}

float osd_show_home_pos(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      return 0.0;
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) )
      return 0.0;
   char szBuff[32];
   sprintf(szBuff, "HLat: %.6f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat);
   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSD);
   xPos += w + osd_getSpacingH()*0.5;

   sprintf(szBuff, "HLon: %.6f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon);
   w = osd_show_value(xPos,yPos, szBuff, g_idFontOSD);
   xPos += w + osd_getSpacingH()*0.7;
   return 0.0;
}


void osd_show_recording(bool bShowWhenStopped, float xPos, float yPos)
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();
   float w = 0.03*osd_getScaleOSD();

   //static long s_lMemDiskOSDFree = 0;
   //static long s_lMemDiskOSDTotal = 0;
   //static u32  s_lMemDiskLastTime = 0;

   if ( (! g_bIsVideoRecording) && (!s_bDebugOSDShowAll) )
   {
      if ( !(g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT) )
      if ( g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE )
      if ( bShowWhenStopped )
      {
         //s_lMemDiskOSDFree = 0;
         //s_lMemDiskOSDTotal = 0;
         //s_lMemDiskLastTime = 0;

         if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
            return;
         if ( (!s_bDebugOSDShowAll) && (!(g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE )) )
            return;

         osd_set_colors();
         float hIcon = 1.4*height_text;
         float wIcon = 1.4*height_text/g_pRenderEngine->getAspectRatio();
         float dy = 0.4*(osd_getBarHeight()- hIcon);
         g_pRenderEngine->drawIcon(xPos-wIcon, yPos+dy, wIcon, hIcon, g_idIconCamera);
         osd_show_video_profile_mode(xPos-height_text*0.2-wIcon, yPos+dy + 0.3*(hIcon-height_text), g_idFontOSD, true);
      }
      return;
   }
   osd_set_colors();

   float hIcon = 1.4*height_text;
   float wIcon = 1.4*height_text/g_pRenderEngine->getAspectRatio();
   float dy = 0.4*(osd_getBarHeight()- hIcon);
   float fWidthMode = 0.0;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE ) )
   {
      g_pRenderEngine->drawIcon(xPos-wIcon, yPos+dy, wIcon, hIcon, g_idIconCamera);
      fWidthMode = osd_show_video_profile_mode(xPos-height_text*0.2-wIcon, yPos+dy + 0.3*(hIcon-height_text), g_idFontOSD, true);
   }

   xPos -= wIcon + fWidthMode + height_text*0.4;

   double pColorRed[4] = {140,0,0,1};
   double pColorRedLow[4] = {80,20,20,0.7};
   double pColorYellow[4] = {150,150,80,0.7};

   Preferences *p = get_Preferences();

   char szTime[64];
   u32 tMili = g_TimeNow - g_uVideoRecordingStartTime;

   //if ( p->iVideoDestination == prefVideoDestination_Disk )
   {
      if ( (tMili/900)%2 )
         sprintf(szTime, "%02d:%02d", (tMili/1000)/60, (tMili/1000)%60 );
      else
         sprintf(szTime, "%02d %02d", (tMili/1000)/60, (tMili/1000)%60 );
      //s_lMemDiskOSDFree = 0;
      //s_lMemDiskOSDTotal = 0;
      //s_lMemDiskLastTime = 0;
   }
   /*
   else
   {
      if  ( 0 == s_lMemDiskOSDTotal )
      {
         s_lMemDiskLastTime = g_TimeNow-3000;
         s_lMemDiskOSDTotal = 1;
      }
      if ( g_TimeNow > s_lMemDiskLastTime + 4000 )
      {
         s_lMemDiskLastTime = g_TimeNow;
         char szComm[1024];
         char szBuff[2048];
         char szTemp[64];
         sprintf(szComm, "df %s | sed -n 2p", FOLDER_TEMP_VIDEO_MEM);
         hw_execute_bash_command_raw(szComm, szBuff);
         //log_line("Output: [%s]", szBuff);
         long lu;
         sscanf(szBuff, "%s %ld %ld %ld", szTemp, &s_lMemDiskOSDTotal, &lu, &s_lMemDiskOSDFree);
      }
      sprintf(szTime, "%d/%d", (int)(s_lMemDiskOSDTotal/1000-s_lMemDiskOSDFree/1000), (int)(s_lMemDiskOSDTotal/1000));
   }
   */
   if ( p->iShowBigRecordButton )
   {
      yPos += osd_getBarHeight()+osd_getSecondBarHeight()*1.2;
      xPos = 0.5-height_text*3.0;

      if ( (g_TimeNow / 1000 ) % 2 )
         pColorRed[3] = 0.0;
      else
         pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      bool bBB = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
      w = 1.1*osd_show_value(xPos,yPos+dy, "REC", g_idFontOSD);
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBB);
      float m = w*0.18+0.5*w*0.1;

      pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(2.0);
      g_pRenderEngine->drawRoundRect(xPos-m, yPos+dy, w+2*m,height_text+m, 0.01*osd_getScaleOSD());

      if ( (g_TimeNow / 1000 ) % 2 )
         pColorRed[3] = 0.0;
      else
         pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      bBB = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
      w = osd_show_value(xPos,yPos+dy, "REC", g_idFontOSD);

      osd_set_colors();
      osd_show_value(xPos-height_text*0.1, yPos + dy + height_text*1.2, szTime, g_idFontOSD);
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBB);
      return;
   }

   pColorRed[3] = 1.0;

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(get_Color_OSDText());
   else
      g_pRenderEngine->setColors(pColorYellow);

   w = 1.1 * g_pRenderEngine->textWidth(g_idFontOSDSmall, "REC");
   float m = w*0.18+0.5*w*0.1;
   xPos -= w + 2*m;

   bool bBB = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
   osd_show_value(xPos,yPos+dy, "REC", g_idFontOSDSmall);
   g_pRenderEngine->drawBackgroundBoundingBoxes(bBB);

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(pColorRed);
   else
      g_pRenderEngine->setColors(pColorRedLow);
   g_pRenderEngine->setStrokeSize(2.0);
   g_pRenderEngine->drawRoundRect(xPos-m, yPos+dy, w+2*m, height_text_small+m, 0.01*osd_getScaleOSD());

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(get_Color_OSDText());
   else
      g_pRenderEngine->setColors(pColorYellow);

   bBB = g_pRenderEngine->drawBackgroundBoundingBoxes(false);
   osd_show_value(xPos,yPos+dy, "REC", g_idFontOSDSmall);
   osd_set_colors();
   osd_show_value(xPos-m*0.3,yPos+dy+height_text_small*1.1, szTime, g_idFontOSDSmall);
   g_pRenderEngine->drawBackgroundBoundingBoxes(bBB);
}

void render_bars()
{
   if ( ! (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BGBARS) )
      return;

   float fGlobalAlpha = g_pRenderEngine->setGlobalAlfa(1.0);
   osd_set_colors_background_fill();

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      float fWidth = osd_getVerticalBarWidth();
      g_pRenderEngine->drawRect(osd_getMarginX(), osd_getMarginY(), fWidth, 1.0-2*osd_getMarginY());
      g_pRenderEngine->drawRect(1.0-osd_getMarginX()-fWidth, osd_getMarginY(), fWidth, 1.0-2*osd_getMarginY());
      g_pRenderEngine->setGlobalAlfa(fGlobalAlpha);
      osd_set_colors();
      return;
   }
   float hBar = osd_getBarHeight();
   hBar += osd_getSecondBarHeight();

   g_pRenderEngine->drawRect(osd_getMarginX(), osd_getMarginY(), 1.0-2*osd_getMarginX(),hBar);
   g_pRenderEngine->drawRect(osd_getMarginX(), 1.0-hBar-osd_getMarginY(), 1.0-2*osd_getMarginX(),hBar);	
   g_pRenderEngine->setGlobalAlfa(fGlobalAlpha);
   /*
   {
      float widthRight = 0.0;
      if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.battery_show_per_cell )
         widthRight = 0.096*osd_getScaleOSD();
      else
         widthRight = 0.064*osd_getScaleOSD();
      g_pRenderEngine->drawRect(1.0-osd_getMarginX()-widthRight, 1.0-osd_getBarHeight()-osd_getSecondBarHeight()-osd_getMarginY() + g_pRenderEngine->getPixelHeight(), widthRight, osd_getSecondBarHeight());
   }
   */
   osd_set_colors();
}

void osd_show_signal_bars()
{
   if ( NULL == g_pCurrentModel )
      return;

   float height_text = osd_getFontHeight();
   float height_text_s = osd_getFontHeightSmall();
   char szBuff[64];

   int position = (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SIGNAL_BARS_MASK)>>14;

   float xPos = osd_getMarginX() + 2.0*g_pRenderEngine->getPixelWidth();
   float yPos = osd_getMarginY() + 2.0*osd_getSpacingV() + osd_getBarHeight() + osd_getSecondBarHeight();
   float fBarAspect = 9.0;
   float fBarHeight = height_text_s*0.8;
   float fBarWidth = fBarHeight * fBarAspect;

   if ( position == 1 )
      yPos = 1.0 - osd_getMarginY() - 2.0*osd_getSpacingV() - osd_getBarHeight() - osd_getSecondBarHeight() - fBarHeight;

   if ( position == 2 || position == 3 )
   {
      fBarAspect = 12.0;
      fBarHeight = height_text_s*0.6;
      fBarWidth = fBarHeight * fBarAspect;

      yPos = 1.0 - osd_getMarginY() - 2.0*osd_getSpacingV() - osd_getBarHeight() - osd_getSecondBarHeight() - fBarHeight;
   }
   if ( position == 3 )
      xPos = 1.0 - osd_getMarginX() - 2.0*g_pRenderEngine->getPixelWidth() - fBarHeight;
   

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      xPos = osd_getMarginX() + osd_getSpacingH() + osd_getVerticalBarWidth();
      yPos = osd_getMarginY() + 2.0*osd_getSpacingV();
      if ( position == 1 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
      }
      if ( position == 2 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
      }
      if ( position == 3 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
         xPos = 1.0 - osd_getMarginX() - osd_getSpacingH() - fBarHeight - osd_getVerticalBarWidth();
      }
   }

   osd_set_colors();
   if ( !g_bIsRouterReady )
   {
      strcpy(szBuff, "No signal");
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->drawText(xPos, yPos, g_idFontOSD, szBuff);
      osd_set_colors();
      return;
   }

   float dbmMin = -90;
   float dbmMax = -50;
   float xTextLeft = xPos + g_SM_RadioStats.countLocalRadioInterfaces * fBarHeight*1.2 + fBarHeight*0.3;
   float xTextRight = xPos - g_SM_RadioStats.countLocalRadioInterfaces * fBarHeight*1.2 - fBarHeight*0.1;

   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int cardIndex = i;
      if ( position == 3 || position == 1 )
         cardIndex = hardware_get_radio_interfaces_count()-i-1;
 
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(cardIndex);
      float fPercent = (g_fOSDDbm[cardIndex]-dbmMin)/(dbmMax - dbmMin);
      if ( fPercent < 0.0f ) fPercent = 0.0f;
      if ( fPercent > 1.0f ) fPercent = 1.0f;

      int iRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
      if ( -1 == iRadioLinkId )
         fPercent = 0.0;
      if ( g_fOSDDbm[cardIndex] < -110 )
         fPercent = 0.0;
      fPercent = fPercent * (float)g_SM_RadioStats.radio_interfaces[cardIndex].rxQuality / 100.0;
      osd_set_colors_background_fill();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(get_Color_OSDText());
      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawRoundRect(xPos, yPos, fBarWidth, fBarHeight, 0.0);
      else
         g_pRenderEngine->drawRoundRect(xPos, yPos-fBarWidth, fBarHeight, fBarWidth, 0.0);

      osd_set_colors();

      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawRoundRect(xPos+3.0*g_pRenderEngine->getPixelWidth(), yPos+3.0*g_pRenderEngine->getPixelHeight(), fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelWidth()), fBarHeight-6.0*g_pRenderEngine->getPixelHeight(), 0.0);
      else
         g_pRenderEngine->drawRoundRect(xPos+3.0*g_pRenderEngine->getPixelWidth(), yPos-3.0*g_pRenderEngine->getPixelHeight()-fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelHeight()), fBarHeight-6.0*g_pRenderEngine->getPixelWidth(), fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelHeight()), 0.0);

      char szCardName[128];
      char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szN && 0 != szN[0] )
         sprintf(szCardName, "%s (%s)", pNICInfo->szUSBPort, szN);
      else
         strcpy(szCardName, pNICInfo->szUSBPort);

      if ( -1 == iRadioLinkId )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: N/A", szCardName);
      else if ( pNICInfo->openedForWrite && pNICInfo->openedForRead )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: RX/TX, %d %%", szCardName, (int)(fPercent*100.0));
      else if ( pNICInfo->openedForWrite )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: TX, %d %%", szCardName, (int)(fPercent*100.0));
      else if ( pNICInfo->openedForRead )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: RX, %d %%", szCardName, (int)(fPercent*100.0));
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: N/A, %d %%", szCardName, (int)(fPercent*100.0));

      osd_set_colors();
      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawText(xPos + fBarWidth + height_text*0.25, yPos, g_idFontOSDSmall, szBuff); 
      else if ( position == 2 )
         g_pRenderEngine->drawText(xTextLeft, yPos-height_text_s*1.2 - (float)(hardware_get_radio_interfaces_count()-i-1)*height_text*1.05, g_idFontOSDSmall, szBuff); 
      else
         g_pRenderEngine->drawTextLeft(xTextRight, yPos-height_text_s*1.2 - (float)i*height_text*1.05, g_idFontOSDSmall, szBuff); 

      if ( 0 == position )
         yPos += height_text*1.05;
      if ( 1 == position )
         yPos -= height_text*1.05;
      if ( 2 == position )
         xPos += fBarHeight*1.2;
      if ( 3 == position )
         xPos -= fBarHeight*1.2;
   }

   osd_set_colors();
}

void osd_show_grid()
{
   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pModel )
      return;

   float fA = g_pRenderEngine->setGlobalAlfa(0.6);
   double pc[4];
   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], 0.3);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], 0.3);
 

   g_pRenderEngine->setStrokeSize(2.02);
   if ( g_pRenderEngine->getScreenHeight() > 720 )
      g_pRenderEngine->setStrokeSize(3.01);

   if ( s_bDebugOSDShowAll || (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) )
   {
      g_pRenderEngine->drawLine(0.5 - 0.02/g_pRenderEngine->getAspectRatio(), 0.5, 0.5 + 0.02/g_pRenderEngine->getAspectRatio(), 0.5 ); 
      g_pRenderEngine->drawLine(0.5, 0.5 - 0.02, 0.5, 0.5 + 0.02 ); 
   }

   if ( s_bDebugOSDShowAll || (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_DIAGONAL) )
   {
      g_pRenderEngine->drawLine(0.001, 0.001, 0.999, 0.999 ); 
      g_pRenderEngine->drawLine(0.999, 0.001, 0.001, 0.999);  
   }

   if ( s_bDebugOSDShowAll || (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
   {
      float fSizeY = 0.04;
      float fSizeX = fSizeY/g_pRenderEngine->getAspectRatio();
      float fMargin = 0.25;
      g_pRenderEngine->drawLine(fMargin, fMargin, fMargin + fSizeX, fMargin ); 
      g_pRenderEngine->drawLine(fMargin, fMargin, fMargin, fMargin + fSizeY ); 
        
      g_pRenderEngine->drawLine(fMargin, 1.0-fMargin, fMargin+fSizeX, 1.0-fMargin );
      g_pRenderEngine->drawLine(fMargin, 1.0-fMargin, fMargin, 1.0-fMargin - fSizeY ); 

      g_pRenderEngine->drawLine(1.0-fMargin, fMargin, 1.0-fMargin - fSizeX, fMargin ); 
      g_pRenderEngine->drawLine(1.0-fMargin, fMargin, 1.0-fMargin, fMargin + fSizeY ); 

      g_pRenderEngine->drawLine(1.0-fMargin, 1.0-fMargin, 1.0-fMargin - fSizeX, 1.0-fMargin ); 
      g_pRenderEngine->drawLine(1.0-fMargin, 1.0-fMargin, 1.0-fMargin, 1.0-fMargin - fSizeY );
   }
   else if ( s_bDebugOSDShowAll || (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_SQUARES) )
   {
      g_pRenderEngine->drawLine(0.001, 0.25, 0.999, 0.25 ); 
      g_pRenderEngine->drawLine(0.001, 0.75, 0.999, 0.75); 
      g_pRenderEngine->drawLine(0.25, 0.001, 0.25, 0.999 ); 
      g_pRenderEngine->drawLine(0.75, 0.001, 0.75, 0.999 ); 
   }

   g_pRenderEngine->setGlobalAlfa(fA);
   osd_set_colors();
}

float render_osd_voltagesamps(float x, float y)
{
   float x0 = x;

   bool bMultiLine = false;
   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      bMultiLine = true;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY ) )
   {
      y -= osd_getSecondBarHeight()+0.3*osd_getSpacingV();
      osd_show_voltage(x,y, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage/1000.0, true);
      if ( bMultiLine )
         y += 2.0*osd_getFontHeightBig() + osd_getSpacingV();
      else
         y += osd_getSecondBarHeight();
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY ) )
   {
      osd_show_amps(x,y, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.current/1000.0, true);
      if ( bMultiLine )
      {
         x = x0;
         y += osd_getSecondBarHeight();
         osd_show_mah(x,y, (float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.mah, true);
      }
      else
      {
         x -= 0.05*osd_getScaleOSD();
         x -= osd_show_mah(x,y, (float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.mah, true);
      }
   }
   return x0 - x;
}

void _render_osd_left_right()
{
   Preferences* p = get_Preferences();
   char szBuff[64];
   char szBuff2[64];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();
   float height_text_small = osd_getFontHeightSmall();
   float vSpacing = osd_getSpacingV()*2.0;

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);

   if ( NULL == pActiveModel )
      return;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BGBARS) )
      render_bars();

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HID_IN_OSD) )
      osd_show_HID();

   if ( s_bDebugOSDShowAll ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_DIAGONAL) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_SQUARES) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
      osd_show_grid();

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_SIGNAL_BARS) )
      osd_show_signal_bars();

   // -------------------------------------------------
   // Left side

   float x0 = osd_getMarginX() + osd_getSpacingH()*0.5;
   float y0 = osd_getMarginY() + osd_getSpacingV()*1.5;

   float x = x0;
   float y = y0;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_GPS_INFO ) )
   {
      _osd_show_gps(x,y, false);
      y += height_text_big + vSpacing;
   }
 
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GPS_POS ) )
   {
      osd_show_gps_pos(x,y, 0.9);
      y += 2.0*height_text_small + vSpacing*0.4;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_TOTAL_DISTANCE ) )
   {
      osd_show_total_distance(x,y, 1.0);
      y += height_text_small + vSpacing*0.4;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE ) )
   {
      osd_show_flight_mode(x,y);
      y += height_text_big + 2.0*vSpacing;
   }
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME ) )
   {
      osd_show_armtime(x,y, true);
      y += height_text + vSpacing;
   }


   bool bShowAnySpeed = false;
   bool bShowBothSpeeds = false;
   if ( s_bDebugOSDShowAll || ((pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED)) )
      bShowBothSpeeds = true;
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED) )
      bShowAnySpeed = true;

   if ( bShowAnySpeed || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) )
      y += 1.0*vSpacing;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) )
   {
      strcpy(szBuff,"0");
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
           strcpy(szBuff,"0");
      }
      if ( bShowBothSpeeds )
         snprintf(szBuff2, sizeof(szBuff2)/sizeof(szBuff2[0]), "G: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         osd_show_value_sufix(x, y, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         osd_show_value_sufix(x, y, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         osd_show_value_sufix(x, y, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(x, y, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      y += height_text;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED) )
   {
      strcpy(szBuff,"0");
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
           strcpy(szBuff,"0");
      }
      if ( bShowBothSpeeds )
         snprintf(szBuff2, sizeof(szBuff2)/sizeof(szBuff2[0]), "A: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         osd_show_value_sufix(x, y, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         osd_show_value_sufix(x, y, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         osd_show_value_sufix(x, y, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(x, y, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      y += height_text;
   }

   bool bShowDistance = false;
   if ( s_bDebugOSDShowAll || ((pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) && (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE)) )
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || ((g_TimeNow/500)%2) )
   {
      bShowDistance = true;
      float fDistMeters = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0;
      if ( fDistMeters/1000.0 > 500.0 )
      {
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude/10000000.0f;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude/10000000.0f;
      }
      sprintf(szBuff, "D: %u", (unsigned int) _osd_convertMeters(fDistMeters));
      if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      {
        if ( _osd_convertMeters(fDistMeters) >= 10000.0 )
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
        else
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
      }
      else
      {
         if ( _osd_convertMeters(fDistMeters) >= 10000 )
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
         else
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
      }
      y += height_text_big + 1.0*vSpacing;
   }

   bool bShowAlt = false;
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE ) )
   if ( (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
   {
     bShowAlt = true;
     if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
     {
        if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0));
     }
     else
        sprintf(szBuff,"0.0");
     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x+0.5*osd_getSpacingH(),y, removeTrailingZero(szBuff), "ft/s",g_idFontOSD, g_idFontOSDSmall);
     else
        osd_show_value_sufix(x+0.5*osd_getSpacingH(),y, removeTrailingZero(szBuff), "m/s",g_idFontOSD, g_idFontOSDSmall);

     y += height_text;

     float alt = _osd_convertHeightMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs/100.0f-1000.0);
     if ( pActiveModel->osd_params.altitude_relative )
        alt = _osd_convertHeightMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude/100.0f-1000.0);
     if ( fabs(alt) < 10.0 )
     {
        if ( fabs(alt) < 0.1 )
          alt = 0.0;
        sprintf(szBuff, "H: %.1f", alt);
     }
     else
        sprintf(szBuff, "H: %d", (int)alt);
     if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || (alt < -500.0) )
        sprintf(szBuff, "H: ---");
     if ( (p->iUnitsHeight == prefUnitsImperial) || (p->iUnitsHeight == prefUnitsFeets) )
        osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
     else
        osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);

      y += height_text_big + 1.0*vSpacing;
   }

   if ( bShowDistance || bShowAlt )
     y += 1.0*vSpacing;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY ) )
   {
      y += 2.0*vSpacing;
      render_osd_voltagesamps(x,y);
      y += 3.0*height_text_big + vSpacing*0.2;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE ) )
   {
      y += 1.0*vSpacing;
      g_pRenderEngine->drawIcon(x, y+height_text_big*0.1, height_text_big*0.8/g_pRenderEngine->getAspectRatio(), height_text_big*0.8, g_idIconHeading);
      x += 1.1*height_text_big/g_pRenderEngine->getAspectRatio();

      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
         sprintf(szBuff, "%03d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading);
      else
         sprintf(szBuff, "---");

      x += osd_show_value(x,y+(height_text_big-height_text)*0.5, szBuff, g_idFontOSD);
      x += osd_show_value(x+height_text*0.05,y, "o", g_idFontOSDSmall);
      x = x0;
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HOME ) )
   {
      osd_show_home(x, y, true, 1.0);   
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_THROTTLE ) )
   {
      osd_show_throttle(x,y,false);
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_PITCH ) )
   if ( (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )  
         sprintf(szBuff, "Pitch: %d", (int)(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch/100.0-180.0));
      else
         sprintf(szBuff, "Pitch: -");
      osd_show_value(x,y, szBuff, g_idFontOSD);
      y += height_text + vSpacing;
   }

   //---------------------------------------------
   // Right side

   x0 = 1.0 - osd_getMarginX() - osd_getSpacingH()*0.5;
   y0 = osd_getMarginY() + osd_getSpacingV()*1.5;

   x = x0;
   y = y0;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_RC_RSSI ) )
   {
      _osd_show_rc_rssi(x,y, 1.0);
      y += height_text + 1.0*vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_RADIO_LINKS) || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
   {
      for( int iVehicleRadioLink=g_pCurrentModel->radioLinksParams.links_count-1; iVehicleRadioLink >= 0; iVehicleRadioLink-- )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( (g_pCurrentModel->relay_params.uRelayedVehicleId == 0) || (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId != iVehicleRadioLink) )
            continue;
         y += osd_show_relay_radio_link_new(1.0 - osd_getMarginX() - osd_getVerticalBarWidth() ,y, iVehicleRadioLink, false);
         y += 2.0*vSpacing;
      }

      for( int iVehicleRadioLink=g_pCurrentModel->radioLinksParams.links_count-1; iVehicleRadioLink >= 0; iVehicleRadioLink-- )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iVehicleRadioLink) )
            continue;
         for( int iLocalRadioLink=0; iLocalRadioLink < g_SM_RadioStats.countLocalRadioLinks; iLocalRadioLink++ )
         {
            if ( g_SM_RadioStats.radio_links[iLocalRadioLink].matchingVehicleRadioLinkId == iVehicleRadioLink )
            {
               y += osd_show_local_radio_link_new(1.0 - osd_getMarginX() - osd_getVerticalBarWidth() ,y, iLocalRadioLink, false);
               y += 2.0*vSpacing;
            }
         }
      }
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE) )
   {
      osd_set_colors();
      float dy = -0.2*height_text;
      g_pRenderEngine->drawIcon(x-1.0*height_text, y+dy, 1.4*height_text/g_pRenderEngine->getAspectRatio(), 1.4*height_text, g_idIconCamera);
      osd_show_video_profile_mode(x-1.1*height_text, y, g_idFontOSD, true);
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MBPS ) )
   {
      osd_show_video_link_mbs(x,y, true);
      y += height_text_big + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED ) )
   {
      u32 uVehicleIdVideo = 0;
      if ( (osd_get_current_data_source_vehicle_index() >= 0) && (osd_get_current_data_source_vehicle_index() < MAX_CONCURENT_VEHICLES) )
         uVehicleIdVideo = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uVehicleId;
      if ( link_has_received_videostream(uVehicleIdVideo) && (NULL != pVDS) )
         strcpy(szBuff, getOptionVideoResolutionName(pVDS->iCurrentVideoWidth, pVDS->iCurrentVideoHeight));
      else
         sprintf(szBuff, "[waiting]");
      osd_show_value_left(x,y, szBuff, g_idFontOSD);
      y += height_text;

      if ( link_has_received_videostream(uVehicleIdVideo) && (NULL != pVDS) )
      {
         if ( pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE )
         {
            if ( ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) > 2) ||
                 ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) < -2) )
               sprintf(szBuff, "*%d FPS", pVDS->iCurrentVideoTxSourceFPS);
            else
               sprintf(szBuff, "%d FPS", pVDS->iCurrentVideoTxSourceFPS);
         }
      }
      else
         sprintf(szBuff, "[waiting]");
      osd_show_value_left(x,y, szBuff, g_idFontOSDSmall);
      y += height_text_small + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_CPU_INFO ) ||
        p->iShowControllerCPUInfo )
   {
      osd_show_cpus(x,y, 1.0);
      y += 4.0*height_text_big + 2.0*vSpacing;
   }

}

void osd_debug()
{
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry = true;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent = true;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags = FC_TELE_FLAGS_ARMED | FC_TELE_FLAGS_HAS_ATTITUDE;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.flight_mode = FLIGHT_MODE_STAB | FLIGHT_MODE_ARMED;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.gps_fix_type = GPS_FIX_TYPE_3D_FIX;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hdop = 111;
   if ( s_RenderCount < 2 || g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed < 5000 )
   {
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude = 101000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs = 101000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed = 101000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed = 101000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading = 2;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.roll = 180*100;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch = 180*100;
   }

   if ( s_RenderCount % 10 )
   {
      // Current pos: 47.14378586227096, 27.583722513468402
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude = 47.136136 * 10000000 + 20000 * sin(s_RenderCount/50.0);
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude = 27.5777667 * 10000000 + 20000 * cos(s_RenderCount/50.0);
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags |= FC_TELE_FLAGS_POS_CURRENT;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance = 100 * distance_meters_between( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude/10000000.0, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude/10000000.0 );
   }
   else
   {
      // Home:  47.13607918623999, 27.577757896625428
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude = 47.136136 * 10000000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude = 27.5777667 * 10000000;
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags |= FC_TELE_FLAGS_POS_HOME;     
   }

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.satelites++;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.satelites > 24 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.satelites = 0;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle++;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle > 100 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle = 0;

   //g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading = 120;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading+=2;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading > 360 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading -= 360;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage+=50;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage > 16000 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage = 0;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs+=20;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude+=20;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude > 105240 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude = 99580;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed+=10;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed > 104000 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed = 100000;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed+=10;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed > 102540 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed = 98770;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.roll+=20;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.roll > 230*100 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.roll = 130*100;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch+=20;
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch > 230*100 )
      g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch = 130*100;

   u16 uWindHeading = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[7] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[8];
   uWindHeading++;
   if ( uWindHeading > 359 )
      uWindHeading = 1;
   
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[7] = uWindHeading >> 8;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[8] = uWindHeading & 0xFF;

   u16 uWindSpeed = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[9] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[10];
   uWindSpeed+=5;
   if ( uWindSpeed > 1000)
      uWindSpeed = 1;

   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[9] = uWindSpeed >> 8;
   g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[10] = uWindSpeed & 0xFF;
}

void osd_render_elements()
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);
   if ( NULL == pActiveModel )
      return;

   if ( ( 0 < pActiveModel->iGPSCount) && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude > 5) || (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude < -5) )
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_POS_CURRENT )
      {
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLat = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude/10000000.0f;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLastLon = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude/10000000.0f;
      }
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_POS_HOME )
      {
         if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet )
            warnings_add(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uVehicleId, "Home Position Aquired and Stored", g_idIconHome);
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet = true;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude/10000000.0f;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude/10000000.0f;
      }
   }

   if ( s_bDebugOSDShowAll )
      osd_debug();

   char szBuff[64];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();
   float height_text_small = osd_getFontHeightSmall();
   float x,y,y0;

   Preferences* p = get_Preferences();
   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST )
   {
      if ( g_TimeNow >= s_uTimeStartFlashOSDElements )
      {
         if ( g_TimeNow < s_uTimeStartFlashOSDElements + 50 )
            return;
         else if ( g_TimeNow < s_uTimeStartFlashOSDElements + 350 )
         {
            set_Color_OSDText( 250, 250, 100, 1.0);
            set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
         }
         else if ( g_TimeNow < s_uTimeStartFlashOSDElements + 400 )
            return;
         else if ( g_TimeNow < s_uTimeStartFlashOSDElements + 750 )
         {
            set_Color_OSDText( 250, 250, 100, 1.0);
            set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
         }
         else if ( g_TimeNow < s_uTimeStartFlashOSDElements + 800 )
            return;
         else
            s_uTimeStartFlashOSDElements = 0;
      }
   }
   else
      s_uTimeStartFlashOSDElements = 0;
   
   osd_set_colors();

   if ( (g_bToglleAllOSDOff || g_bToglleOSDOff) && (!s_bDebugOSDShowAll) )
   {
      if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
         return;

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();

      if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
         x = 0.5 - 0.02*osd_getScaleOSD();
      if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
         x = 0.5 - 0.02*osd_getScaleOSD();
      osd_show_recording(true,x,y);


      // Bottom part
      //--------------------------
      /*
      x = 1.0 - osd_getSpacingH()-osd_getMarginX();
      y = 1.0 - osd_getMarginY() - osd_getBarHeight()-osd_getSecondBarHeight() + osd_getSpacingV();
      osd_show_voltage(x,y, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage/1000.0, true);
   
      x = 1.0 - osd_getSpacingH()-osd_getMarginX();
      y += osd_getSecondBarHeight();
      osd_show_amps(x,y, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.current/1000.0, true);

      x -= 0.074*osd_getScaleOSD();
      osd_show_mah(x,y, (float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.mah, true);
      */
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   if ( !(pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED) )
      return;

   if ( pActiveModel->osd_params.iCurrentOSDScreen == osdLayoutLean )
   {
      render_osd_layout_lean();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      osd_show_recording(false,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   if ( pActiveModel->osd_params.iCurrentOSDScreen == osdLayoutLeanExtended )
   {
      render_osd_layout_lean_extended();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      osd_show_recording(false,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   osd_set_colors();

   if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
   {
      _render_osd_left_right();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      x = 0.5 - 0.02*osd_getScaleOSD();
      osd_show_recording(true,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BGBARS) )
      render_bars();
   
   osd_set_colors();

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HID_IN_OSD) )
      osd_show_HID();

   if ( s_bDebugOSDShowAll ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_DIAGONAL) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_SQUARES) ||
      (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
      osd_show_grid();
  
   // Top part - left
   // ------------------------------

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GPS_POS ) )
   {
      float y = 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
      float x = 0.3 + 0.07*osd_getScaleOSD();
      osd_show_gps_pos(x,y, 0.9);
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_TOTAL_DISTANCE ) )
   {
      float y = 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
      float x = 0.2*osd_getScaleOSD();
      osd_show_total_distance(x,y, 1.0);
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_SIGNAL_BARS ) )
      osd_show_signal_bars();

   x = osd_getMarginX() + osd_getSpacingH();
   y = osd_getMarginY() + osd_getSpacingV();

   x = osd_getMarginX() + osd_getSpacingH()*0.5;
   if ( (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) && 
        (s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_GPS_INFO )) )
      x += _osd_show_gps(x,y, true);
   else
      x += osd_getSpacingH()*0.5;

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE ) )
   {
      x += osd_show_flight_mode(x,osd_getMarginY());
      x += osd_getSpacingH();
   }
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME ) )
   {
      if ( !(pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME_LOWER) )
        osd_show_armtime(x,y, true);
   }

   // Top part - right
   //----------------------------------------

   x = 1.0 - osd_getMarginX() - 0.5*osd_getSpacingH();

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_RC_RSSI ) )
   {
      x -= _osd_show_rc_rssi(x,y, 1.0) + osd_getSpacingH();
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_RADIO_LINKS) || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
   {
      for( int iVehicleRadioLink=g_pCurrentModel->radioLinksParams.links_count-1; iVehicleRadioLink >= 0; iVehicleRadioLink-- )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( (g_pCurrentModel->relay_params.uRelayedVehicleId == 0) || (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId != iVehicleRadioLink) )
            continue;
         x -= osd_show_relay_radio_link_new(x,osd_getMarginY() + 0.5*osd_getSpacingV(), iVehicleRadioLink, true);
         x -= osd_getSpacingH();
      }

      for( int iVehicleRadioLink=g_pCurrentModel->radioLinksParams.links_count-1; iVehicleRadioLink >= 0; iVehicleRadioLink-- )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( (g_pCurrentModel->relay_params.uRelayedVehicleId !=  0) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iVehicleRadioLink) )
            continue;
         for( int iLocalRadioLink=0; iLocalRadioLink < g_SM_RadioStats.countLocalRadioLinks; iLocalRadioLink++ )
         {
            if ( g_SM_RadioStats.radio_links[iLocalRadioLink].matchingVehicleRadioLinkId == iVehicleRadioLink )
            {
               x -= osd_show_local_radio_link_new(x,osd_getMarginY() + 0.5*osd_getSpacingV(), iLocalRadioLink, true);
               x -= osd_getSpacingH();
            }
         }
      }
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_TX_POWER) )
      x = osd_show_txpowers(x);

   float xStartRecording = x;
   osd_show_recording(true,x,y);


   // Top part full layout right
   //--------------------------------------------------------

   x = 0.2*osd_getScaleOSD()+osd_getMarginX();
   y = osd_getMarginY() + osd_getBarHeight() + 0.5*osd_getSpacingV();

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MBPS ) )
   {
      x = xStartRecording;
      x -= osd_show_video_link_mbs(x,y, true);
      x -= osd_getSpacingH()*0.6;
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED ) )
   {
      u32 uVehicleIdVideo = 0;
      if ( (osd_get_current_data_source_vehicle_index() >= 0) && (osd_get_current_data_source_vehicle_index() < MAX_CONCURENT_VEHICLES) )
         uVehicleIdVideo = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uVehicleId;
      if ( NULL == pVDS )
         strcpy(szBuff, "N/A");
      else if ( link_has_received_videostream(uVehicleIdVideo) )
      {
         if ( ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) > 2) ||
              ((pVDS->iCurrentVideoTxSourceFPS - pActiveModel->video_params.iVideoFPS) < -2) )
            sprintf(szBuff, "%s *%d FPS", getOptionVideoResolutionName(pVDS->iCurrentVideoWidth, pVDS->iCurrentVideoHeight), pVDS->iCurrentVideoTxSourceFPS);
         else
            sprintf(szBuff, "%s %d FPS", getOptionVideoResolutionName(pVDS->iCurrentVideoWidth, pVDS->iCurrentVideoHeight), pVDS->iCurrentVideoTxSourceFPS);
      }
      else
         sprintf(szBuff, "[waiting]");
      x += osd_show_value_left(x,y, szBuff, g_idFontOSDSmall);
   }

   // Top part full layout right line 3
   //--------------------------------------------------------

   y = osd_getMarginY() + osd_getBarHeight() + 0.7*osd_getSpacingV();
   x = 1.0 - osd_getMarginX() - osd_getSpacingH();

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_CPU_INFO ) ||
        p->iShowControllerCPUInfo )
   {
      y = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.7*osd_getSpacingV();
      x = 1.0 - osd_getMarginX();
      x -= osd_show_cpus(x,y, 1.0);
   }

   // Bottom part - left
   //------------------------------------

   if ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      return;

   x = osd_getMarginX() + osd_getSpacingH()*0.6;
   y = 1.0 - osd_getMarginY()-osd_getBarHeight()+osd_getSpacingV();
   y0 = 1.0 - osd_getMarginY()-osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
   
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || ((pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) && (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE) ) )
   if ( (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || ((g_TimeNow/500)%2) )
   {
      float fDistMeters = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0;
      if ( fDistMeters/1000.0 > 500.0 )
      {
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bHomeSet = true;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLat = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.latitude/10000000.0f;
         g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].fHomeLon = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.longitude/10000000.0f;
      }
      sprintf(szBuff, "D: %u", (unsigned int) _osd_convertMeters(fDistMeters));
      if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      {
        if ( _osd_convertMeters(fDistMeters) >= 10000.0 )
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
        else
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
      }
      else
      {
         if ( _osd_convertMeters(fDistMeters) >= 10000 )
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
         else
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
      }
   }

   bool bShowBothSpeeds = false;
   if ( s_bDebugOSDShowAll || ((pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED)) )
      bShowBothSpeeds = true;
   float xSpeed = x;
   if ( bShowBothSpeeds )
      xSpeed -= 0.5 * osd_getSpacingH();
     
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) )
   {
      char szBuff2[64];
      strcpy(szBuff,"0");
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));

        if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
           strcpy(szBuff,"0");
      }

      if ( bShowBothSpeeds )
         snprintf(szBuff2, sizeof(szBuff2)/sizeof(szBuff2[0]), "G: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      xSpeed += 0.5*osd_getSpacingH();
   }
      
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED) )
   {
      char szBuff2[64];
      strcpy(szBuff,"0");
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
           strcpy(szBuff,"0");
      }

      if ( bShowBothSpeeds )
         snprintf(szBuff2, sizeof(szBuff2)/sizeof(szBuff2[0]), "A: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      xSpeed += 0.5*osd_getSpacingH();
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED) || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) )
   {
      x += 0.08*osd_getScaleOSD();
      if ( bShowBothSpeeds )
         x += g_pRenderEngine->textWidth(g_idFontOSD, "AAA");
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE ) )
   if ( (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
   {
     float alt = _osd_convertHeightMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs/100.0f-1000.0);
     if ( pActiveModel->osd_params.altitude_relative )
        alt = _osd_convertHeightMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude/100.0f-1000.0);
     if ( fabs(alt) < 10.0 )
     {
        if ( fabs(alt) < 0.1 )
          alt = 0.0;
        sprintf(szBuff, "H: %.1f", alt);
     }
     else
        sprintf(szBuff, "H: %d", (int)alt);
     if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || (alt < -500.0) )
        sprintf(szBuff, "H: ---");
     if ( (p->iUnitsHeight == prefUnitsImperial) || (p->iUnitsHeight == prefUnitsFeets) )
        osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
     else
        osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);

     if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
     {
        float vSpeed = 0.0;
        if ( pActiveModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED )
           vSpeed = _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0);
        else
           vSpeed = _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0);
        if ( fabs(vSpeed) < 0.1 )
           vSpeed = 0.0;
        sprintf(szBuff, "%.1f", vSpeed);
     }
     else
        sprintf(szBuff,"0.0");

     if ( bShowBothSpeeds )
      x += 1.0 * osd_getSpacingH();

     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x,y0, removeTrailingZero(szBuff), "ft/s",g_idFontOSD, g_idFontOSDSmall);
     else
        osd_show_value_sufix(x,y0, removeTrailingZero(szBuff), "m/s",g_idFontOSD, g_idFontOSDSmall);

     x += 0.06*osd_getScaleOSD();
     if ( alt > 999.99 )
        x += 0.02*osd_getScaleOSD();
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE ) )
   {
      g_pRenderEngine->drawIcon(x, y+height_text_big*0.14, height_text_big*0.8/g_pRenderEngine->getAspectRatio(), height_text_big*0.8, g_idIconHeading);
      x += 1.2*height_text_big/g_pRenderEngine->getAspectRatio();

      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
         sprintf(szBuff, "%03d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading);
      else
         sprintf(szBuff, "---");

      x += osd_show_value(x,y+(height_text_big-height_text)*0.5, szBuff, g_idFontOSD);
      x += osd_show_value(x+height_text*0.05,y, "o", g_idFontOSDSmall);
      x += 1.5*osd_getSpacingH();
   }


   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HOME ) )
   {
      x += osd_show_home(x, y, true, 1.0);
      x += osd_getSpacingH(); 
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_WIND ) )
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   {
      float fIconHeight = 1.0*height_text_big;
      float fIconWidth = fIconHeight/g_pRenderEngine->getAspectRatio();
      g_pRenderEngine->drawIcon(x,y, fIconWidth, fIconHeight, g_idIconWind);
      x += fIconWidth + height_text_small*0.4 / g_pRenderEngine->getAspectRatio();
      u16 uDir = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[7] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[8];
      u16 uSpeed = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[9] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[10];
      if ( 0 == uDir )
      {
         strcpy(szBuff, "H: N/A");
         sprintf(szBuff, "H: %d", (int)uDir-1);
         osd_show_value(x,y-height_text_small*0.2, szBuff, g_idFontOSDSmall);
      }
      else
      {
         float fScale = (osd_getBarHeight()-2.0*osd_getSpacingV())/0.21;
         float xp[5] = {0.0f,  -0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f, 0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f};
         float yp[5] = {0.08f*fScale, 0.15f*fScale, -0.15f*fScale, 0.15f*fScale, 0.08f*fScale};
         for( int i=0; i<5; i++ )
            xp[i] = xp[i]/g_pRenderEngine->getAspectRatio();
         
         float rel_heading = (float)(uDir-1) - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading; //direction arrow needs to point relative to camera/osd/craft
         //rel_heading -= 180;

         osd_rotatePoints(xp, yp, rel_heading, 5, x + 0.02/g_pRenderEngine->getAspectRatio(), y, 0.34);

         g_pRenderEngine->setColors(get_Color_OSDText());

         float fStroke = g_pRenderEngine->getStrokeSize();
         g_pRenderEngine->setStrokeSize(0);
         g_pRenderEngine->fillPolygon(xp,yp,5);

         double pc2[4];
         pc2[0] = 0; pc2[1] = 0; pc2[2] = 0;
         pc2[3] = 0.3;
         g_pRenderEngine->setColors(pc2);
         g_pRenderEngine->setStroke(pc2, 1.5);
         g_pRenderEngine->drawPolyLine(xp,yp,5);
         g_pRenderEngine->setStrokeSize(fStroke);
         osd_set_colors();
      }
      if ( 0 == uSpeed )
         strcpy(szBuff, "N/A");
      else
      {
         sprintf(szBuff, "%.1f m/s", ((float)uSpeed-1)/100.0);
         if ( p->iUnits == prefUnitsMetric )
            sprintf(szBuff, "%.1f km/h", ((float)uSpeed-1)*3.6/100.0);
         else if ( p->iUnits == prefUnitsImperial )
            sprintf(szBuff, "%.1f mi/h", _osd_convertKm(((float)uSpeed-1)*3.6/100.0));
         else if ( p->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f ft/s", _osd_convertMeters(((float)uSpeed-1)/100.0));
      }
      x += osd_show_value(x,y+height_text_small*0.76, szBuff, g_idFontOSD);
      x += osd_getSpacingH();
   }

   // Bottom part - right
   //------------------------------------

   x = 1.0 - osd_getMarginX() - osd_getSpacingH()*0.6;
   y = 1.0 - osd_getMarginY()-osd_getBarHeight()+osd_getSpacingV();

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY ) )
   {
      x -= render_osd_voltagesamps(x,y);
      x -= osd_getSpacingH();
   }

   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME ) )
   {
      if ( (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME_LOWER) )
      {
         x -= osd_getSpacingH()*0.5;
         float fW = osd_show_armtime(x,y-osd_getSpacingV(),false);
         x -= osd_show_armtime(x-fW,y-osd_getSpacingV()*0.7, true);
         x -= osd_getSpacingH();
      }
   }

   float xStart = x;
   float xEnd1 = x;
   float xEnd2 = x;
   float xEnd3 = x;

   float yTemp = y;
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_THROTTLE ) )
   {
      yTemp = 1.0 - osd_getMarginY()-osd_getSpacingV() - height_text*1.2;
      xEnd1 -= osd_show_throttle(xStart,yTemp,true);
      xEnd1 -= osd_getSpacingH();
      yTemp -= height_text*0.9;
   }

   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_PITCH ) )
   if ( (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (pActiveModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
   {
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )  
         sprintf(szBuff, "%d", (int)(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch/100.0-180.0));
      else
         sprintf(szBuff, "-");
      xEnd2 -= osd_show_value_left(xStart,yTemp, szBuff, g_idFontOSD);
      xEnd2 -= height_text*0.15;
      xEnd2 -= osd_show_value_left(xEnd2,yTemp + (height_text-height_text_small)*0.5, "Pitch: ", g_idFontOSDSmall);
      xEnd2 -= osd_getSpacingH();
      yTemp -= height_text*0.9;
   }
   
   if ( pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   if ( s_bDebugOSDShowAll || (pActiveModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_SHOW_FC_TEMPERATURE ) )
   {
      if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry) || 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.temperatureC )
         strcpy(szBuff, "N/A");
      else
      {
         //if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
         if ( (NULL != pActiveModel) && (pActiveModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_TEMPS_F) )
            sprintf(szBuff, "%d F", (int)osd_convertTemperature(((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.temperatureC)-100.0, true));
         else
            sprintf(szBuff, "%d C", (int)osd_convertTemperature(((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.temperatureC)-100.0, false));
      }
      xEnd3 -= osd_show_value_left(xStart,yTemp, szBuff, g_idFontOSD);
      xEnd3 -= height_text*0.15;
      xEnd3 -= osd_show_value_left(xEnd3,yTemp + (height_text-height_text_small)*0.5, "Temp: ", g_idFontOSDSmall);
      xEnd3 -= osd_getSpacingH();
   }

   x = xEnd1;
   if ( xEnd2 < x )
      x = xEnd2;
   if ( xEnd3 < x )
      x = xEnd3;

   g_pRenderEngine->enableFontScaling(false);
}

void osd_render_instruments()
{
   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pModel )
      return;

   if ( pModel->is_spectator && (!(pModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
      return;

   if ( !(pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED) )
      return;

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();
   Preferences* p = get_Preferences();
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();

   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_HAS_ATTITUDE )
      osd_show_ahi(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.roll/100.0-180.0, g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.pitch/100.0-180.0);
   else
      osd_show_ahi(0,0);

   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}

void osd_render_stats()
{
   Model* pModel = osd_get_current_data_source_vehicle_model();

   if ( NULL == pModel )
      return;

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();
   Preferences* p = get_Preferences();
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();


   bool showStats = true;
   if ( g_bToglleAllOSDOff || g_bToglleStatsOff )
      showStats = false;
   if ( s_bDebugOSDShowAll )
      showStats = true;
   
   if ( !(pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED) )
      return;

   if ( showStats )
      osd_render_stats_panels();

   if ( osd_is_stats_flight_end_on() )
      osd_render_stats_flight_end(0.8);

   if ( s_ShowOSDFlightsStats )
   {
      osd_render_stats_flights(1.0);
      if ( g_TimeNow > s_TimeOSDFlightsStatsOn + 10000 )
         s_ShowOSDFlightsStats = false;
   }

   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}

void osd_render_warnings()
{
   if ( NULL == g_pCurrentModel )
      return;
   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pModel )
      return;
   if ( pModel->is_spectator && (!(pModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   osd_warnings_render();
}

void osd_render_msposd_buffer(int iFCType, int iOSDFontType, int iCols, int iRows, u16* pCharBuffer)
{
   if ( (iCols <= 0) || (iRows <= 0) || (NULL == pCharBuffer) )
      return;

   Preferences* pP = get_Preferences();

   u32 uImgId = g_idImgMSPOSDBetaflight;
   if ( iFCType == MSP_FLAGS_FC_TYPE_INAV )
      uImgId = g_idImgMSPOSDINAV;
   if ( iFCType == MSP_FLAGS_FC_TYPE_ARDUPILOT )
      uImgId = g_idImgMSPOSDArdupilot;
   if ( iFCType == MSP_FLAGS_FC_TYPE_PITLAB )
      uImgId = g_idImgMSPOSDPitLab;
   if ( iOSDFontType != 0 )
   {
      if ( iOSDFontType == 1 )
         uImgId = g_idImgMSPOSDBetaflight;
      if ( iOSDFontType == 2 )
         uImgId = g_idImgMSPOSDINAV;
      if ( iOSDFontType == 3 )
         uImgId = g_idImgMSPOSDArdupilot;
      if ( iOSDFontType == 4 )
         uImgId = g_idImgMSPOSDPitLab;
   }
   
   int iImgCharWidth = 36;
   int iImgCharHeight = 54;
   if ( g_pRenderEngine->getScreenHeight() <= 800 )
   {
      iImgCharWidth = 24;
      iImgCharHeight = 36;    
   }
   float fScreenCharWidth = (1.0 - 2.0*osd_getMarginX()) / (float)iCols;
   float fScreenCharHeight = (1.0 - 2.0*osd_getMarginY()) / (float)iRows;

   if ( (pP->iMSPOSDSize > 30) && (pP->iMSPOSDSize < 150) )
   {
      fScreenCharWidth = ((float)pP->iMSPOSDSize) * fScreenCharWidth / 100.0;
      fScreenCharHeight = ((float)pP->iMSPOSDSize) * fScreenCharHeight / 100.0;
   }

   float fStartPosX = osd_getMarginX();
   float fStartPosY = osd_getMarginY();

   if ( (pP->iMSPOSDDeltaX > -40) && (pP->iMSPOSDDeltaX < 40) && (pP->iMSPOSDDeltaY > -40) && (pP->iMSPOSDDeltaY < 40) )
   {
      fStartPosX += pP->iMSPOSDDeltaX * fScreenCharWidth;
      fStartPosY += pP->iMSPOSDDeltaY * fScreenCharHeight;
   }

   for( int y=0; y<iRows; y++ )
   for( int x=0; x<iCols; x++ )
   {
      u16 uChar = pCharBuffer[x + y*iCols];
      if ( 0 == (uChar & 0xFF) )
         continue;
      u8 uPage = uChar >> 8;

      int iImgSrcX = ((int)(uPage & 0x03)) * iImgCharWidth;
      int iImgSrcY = ((int)(uChar & 0xFF)) * iImgCharHeight;

      g_pRenderEngine->bltSprite(fStartPosX + x * fScreenCharWidth, fStartPosY + y * fScreenCharHeight,
         iImgSrcX, iImgSrcY, iImgCharWidth, iImgCharHeight, uImgId);
   }
}

void _osd_render_msp(Model* pModel)
{
   if ( NULL == pModel )
      return;

   t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(pModel->uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   if ( (pRuntimeInfo->mspState.headerTelemetryMSP.uMSPOSDRows == 0) ||
        (pRuntimeInfo->mspState.headerTelemetryMSP.uMSPOSDCols == 0) ||
        ((pRuntimeInfo->mspState.headerTelemetryMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK) == 0) )
      return;

   osd_render_msposd_buffer(pRuntimeInfo->mspState.headerTelemetryMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK,
      pModel->osd_params.uFlags & OSD_BIT_FLAGS_MASK_MSPOSD_FONT,
      pRuntimeInfo->mspState.headerTelemetryMSP.uMSPOSDCols,
      pRuntimeInfo->mspState.headerTelemetryMSP.uMSPOSDRows,
      pRuntimeInfo->mspState.uScreenChars);
}


void osd_show_monitor()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
       return;

   osd_getSetScreenScale(p->iOSDScreenSize);
   
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();
   g_pRenderEngine->setStroke(0,0,0,0.7);
   g_pRenderEngine->setStrokeSize(2.0);
   float fDotHeight = 4.0*g_pRenderEngine->getPixelHeight();
   float fDotWidth = 4.0*g_pRenderEngine->getPixelWidth();

   int pos = s_RenderCount%10;
   g_pRenderEngine->drawRect(osd_getMarginX() + pos*fDotWidth*2.0, 1.0 - osd_getMarginY()-fDotHeight*5.0, fDotWidth, fDotHeight);

   int dy = (g_ProcessStatsRouter.uLoopCounter)%4;
   pos = (g_ProcessStatsRouter.uLoopCounter/4)%10;
   g_pRenderEngine->drawRect(osd_getMarginX() + pos*fDotWidth*2.0, 1.0 - osd_getMarginY()-fDotHeight*3.0+dy*0.5*fDotHeight, fDotWidth, fDotHeight);

   osd_set_colors();

   char szBuff[32];
   sprintf(szBuff, "%d", g_nTotalControllerCPUSpikes );
   g_pRenderEngine->drawText(osd_getMarginX(), 1.0 - osd_getMarginY() - fDotHeight*5.0 - g_pRenderEngine->textHeight(g_idFontOSDSmall), g_idFontOSDSmall, szBuff);
}

void osd_render_all()
{
   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->disableAlphaBlending();

   if ( s_bOSDDisableRendering )
   {
      Model* pModel = osd_get_current_data_source_vehicle_model();
      if ( (NULL == pModel) || (0 == g_uActiveControllerModelVID) )
      {
         g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
         return;
      }
      osd_render_instruments();
      osd_widgets_render(pModel->uVehicleId, osd_get_current_layout_index());
      osd_plugins_render();
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }

   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( (NULL == pModel) || (0 == g_uActiveControllerModelVID) )
   {
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }
   if ( ! (pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED) )
   if ( ! (pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY) )
   {
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }
   if ( ! pairing_isStarted() )
   {
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }
   Preferences* p = get_Preferences();
   
   s_RenderCount++;

   if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED )
   if ( (NULL != p) && (p->iShowProcessesMonitor) )
      osd_show_monitor();


   if ( pModel->is_spectator && (!(pModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
   {
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }
   if ( !pairing_isStarted() )
   {
      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      return;
   }
   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();
   
   osd_setMarginX(0.0);
   osd_setMarginY(0.0);

   u32 uTransparency = ((pModel->osd_params.osd_preferences[osd_get_current_layout_index()]) & OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK) >> OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT;
   osd_set_transparency((int)uTransparency);

   u32 scale = pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & 0xFF;
   osd_setScaleOSD((int)scale);

   scale = (pModel->osd_params.osd_preferences[osd_get_current_layout_index()]>>16) & 0x0F;
   osd_setScaleOSDStats((int)scale);

   if ( NULL != p )
      osd_getSetScreenScale(p->iOSDScreenSize);
   else
      log_softerror_and_alarm("Failed to get pointer to preferences structure.");

   if ( g_bIsRouterReady )
      shared_vars_osd_update();

   if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY )
   {
      g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      double color2[4];
      color2[0] = color2[1] = color2[2] = 0;
      color2[0] = 0;
      color2[3] = 1.0;
      switch ( uTransparency )
      {
         case 0: color2[3] = 0.05; break;
         case 1: color2[3] = 0.26; break;
         case 2: color2[3] = 0.5; break;
         case 3: color2[3] = 0.8; break;
         case 4: color2[3] = 1.0; break;
      }
      g_pRenderEngine->setFontBackgroundBoundingBoxFillColor(color2);
      g_pRenderEngine->setBackgroundBoundingBoxPadding(0.0);
   }

   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();

   if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED )
   {
      if ( pModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      if ( pModel->osd_params.osd_flags3[osd_get_current_layout_index()] & OSD_FLAG3_RENDER_MSP_OSD )
      if ( pModel->osd_params.osd_layout_preset[osd_get_current_layout_index()] != OSD_PRESET_NONE )
      if ( 0 == g_pControllerSettings->iEnableDebugStats )
         _osd_render_msp(pModel);
      osd_render_elements();
   }
   // Set again default OSD colors as OSD elements might have just flashed (yellow)

   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();

   if ( 0 == g_pControllerSettings->iEnableDebugStats )
   {
      if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED )
         osd_render_instruments();

      osd_widgets_render(pModel->uVehicleId, osd_get_current_layout_index());
      osd_plugins_render();
   }
   g_pRenderEngine->drawBackgroundBoundingBoxes(false);

   if ( 0 == g_pControllerSettings->iEnableDebugStats )
   {
      if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED )
         osd_render_stats();

      osd_render_warnings();
   }

   if ( g_pControllerSettings->iEnableDebugStats )
      osd_render_debug_stats();

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iDbgPingGraphs )
      osd_reder_debug_ping_stats();

   if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_ENABLED )
   if ( (NULL != p) && (p->iShowProcessesMonitor) )
      osd_show_monitor();

   if ( ! (g_bToglleAllOSDOff || g_bToglleOSDOff) )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   {
      osd_set_colors();
      if ( pModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
         osd_render_relay( 0.5, 1.0 - osd_getMarginY(), true);
      else
         osd_render_relay( 0.5, 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() - osd_getSpacingV(), true);
   }

   g_pRenderEngine->drawBackgroundBoundingBoxes(false);
   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
}

void osd_start_flash_osd_elements()
{
   if ( 0 == s_uTimeStartFlashOSDElements )
   {
      log_line("Signaled to start flashing OSD elements.");
      s_uTimeStartFlashOSDElements = g_TimeNow;
   }
}