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
#include "../../base/ctrl_preferences.h"
#include "osd_common.h"
#include "osd_lean.h"
#include "osd_warnings.h"
#include "osd.h"
#include <math.h>

#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../local_stats.h"
#include "../link_watch.h"
#include "../timers.h"

extern bool s_bDebugOSDShowAll;

void render_osd_layout_lean()
{
   if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry) || (NULL == g_pCurrentModel) )
      return;
   
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   Preferences* pP = get_Preferences();

   char szBuff[64];
   char szTmp[64];

   float height_text_big = osd_getFontHeightBig();
   float height_text_text = osd_getFontHeightSmall();
   float height_text_line2 = osd_getFontHeight();

   float yLine2 = 1.0-osd_getMarginY()-height_text_line2 - osd_getSpacingV();
   float yText = yLine2 - height_text_text - 2.0*osd_getSpacingV();
   float yBig = yText - height_text_big - osd_getSpacingV();

   float xCellWidth = (1.0-2.0*osd_getMarginX())/6.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BGBARS )
   {
      //float fAlpha = g_pRenderEngine->setGlobalAlfa(0.5);
      osd_set_colors_background_fill();
      g_pRenderEngine->drawRect(osd_getMarginX(), yBig-osd_getSpacingV(), 1.0-2*osd_getMarginX(), 1.0 - osd_getMarginY() - (yBig-osd_getSpacingV()));
      //g_pRenderEngine->setGlobalAlfa(fAlpha);
   }
   osd_set_colors();

   //--------------------
   // Bottom line

   float xCell = osd_getMarginX();

   strcpy(szBuff, "THROTTLE % 0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "THROTTLE %% %d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle);

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_THROTTLE) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   strcpy(szBuff, "AMPS (A) 0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "AMPS (A) %.1f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.current/1000.0);
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;
   
   double pC[4];
   memcpy(pC, get_Color_OSDText(), 4*sizeof(double));

   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bWarningBatteryVoltage )
   {
      memcpy(pC, get_Color_OSDWarning(), 4*sizeof(double));
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
   }
   g_pRenderEngine->setColors(pC);

   strcpy(szBuff, "BATT (V) 0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "BATT (V) %.2f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage/1000.0);
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   osd_set_colors();
   xCell += xCellWidth;

   strcpy(szBuff, "BAT USED (mAh) 0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "BAT USED (mAh) %u", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.mah);
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   szBuff[0] = 0;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
      strcat(szBuff, " 0:00");
   
   
   int sec = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   int min = (g_pCurrentModel->m_Stats.uCurrentFlightTime)/60;

   szBuff[0] = 0;   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
   {
      sprintf(szTmp, " %d:%02d", min, sec);
      strcat(szBuff, szTmp);
   }
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   {
      u8 mode = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED);
      szBuff[0] = 0;   
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
         strcat(szBuff, model_getShortFlightMode(mode));
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
      {
         sprintf(szTmp, " %d:%02d", min, sec);
         strcat(szBuff, szTmp);
      }
   }
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) ||
          (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   int nQualityMain = 0;
   if ( g_bIsRouterReady )
   {
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL != pNICInfo && (g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId == 0) )
         if ( g_SM_RadioStats.radio_interfaces[i].rxQuality > nQualityMain )
            nQualityMain = g_SM_RadioStats.radio_interfaces[i].rxQuality;
      }
   }
   sprintf(szBuff, "LINK QUALITY %d%%", nQualityMain);

   bool bHasRubyRC = false;
   
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->rc_params.rc_enabled && (!g_pCurrentModel->is_spectator) )
      bHasRubyRC = true;
 
   if ( NULL != g_pCurrentModel && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
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
         sprintf(szBuff, "RC RSSI: %d%%", val);
      else
         strcpy(szBuff, "RC RSSI: ---");
   }
   if ( bHasRubyRC && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRCFailsafeState )
      strcpy(szBuff, "!RC FS!");


   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_RADIO_LINKS) )
   {
      float w = g_pRenderEngine->textWidth(g_idFontOSD, szBuff);
      if ( w > xCellWidth )
         xCell -= (w-xCellWidth);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);
   }

   //--------------
   // Top line

   xCell = osd_getMarginX();

   bool bShowBothSpeeds = false;
   bool bShowAnySpeed = false;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED))
      bShowAnySpeed = true;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED)))
      bShowBothSpeeds = true;

   strcpy(szBuff,"0");
   if ( bShowBothSpeeds )
      strcpy(szBuff,"0/0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
   {
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0), _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f), _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));

         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
            else
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
         }
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0), 0.0);
            else
               sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f), 0.0);
         }
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed && 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
            sprintf(szBuff, "0/0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
            strcpy(szBuff,"0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
            strcpy(szBuff,"0");
      }
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || bShowAnySpeed )
   {
      osd_show_value_centered(xCell+xCellWidth/2,yBig, szBuff, g_idFontOSDBig);
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMetric )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "G/A SPEED (km/h)", g_idFontOSDSmall);
         else if ( pP->iUnits == prefUnitsMeters )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "G/A SPEED (m/s)", g_idFontOSDSmall);
         else if ( pP->iUnits == prefUnitsImperial )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "G/A SPEED (mi/h)", g_idFontOSDSmall);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yText, "G/A SPEED (ft/s)", g_idFontOSDSmall);
      }
      else
      {
         if ( pP->iUnits == prefUnitsMetric )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "SPEED (km/h)", g_idFontOSDSmall);
         else if ( pP->iUnits == prefUnitsMeters )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "SPEED (m/s)", g_idFontOSDSmall);
         else if ( pP->iUnits == prefUnitsImperial )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "SPEED (mi/h)", g_idFontOSDSmall);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yText, "SPEED (ft/s)", g_idFontOSDSmall);
      }
   }

   xCell += xCellWidth;

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) )
   {
      strcpy(szBuff, "0");
      bool bKm = false;
      sprintf(szBuff, "%u", (unsigned int)_osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0));
      if ( _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0) >= 1000 )
      {
         sprintf(szBuff, "%.1f", _osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0/1000.0));
         bKm = true;
      }
      osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);

      if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
      {
      if ( bKm )
         osd_show_value_centered(xCell+xCellWidth/2, yText, "DISTANCE (mi)", g_idFontOSDSmall);
      else
         osd_show_value_centered(xCell+xCellWidth/2, yText, "DISTANCE (ft)", g_idFontOSDSmall);
      }
      else
      {
      if ( bKm )
         osd_show_value_centered(xCell+xCellWidth/2, yText, "DISTANCE (km)", g_idFontOSDSmall);
      else
         osd_show_value_centered(xCell+xCellWidth/2, yText, "DISTANCE (m)", g_idFontOSDSmall);
      }
   }

   xCell += xCellWidth;

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      {
         float alt = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs/100.0f-1000.0;
         if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative && 0 != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude)
            alt = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude/100.0f-1000.0;
         alt = _osd_convertHeightMeters(alt);
         if ( fabs(alt) < 10.0 )
         {
            if ( fabs(alt) < 0.1 )
               alt = 0.0;
            sprintf(szBuff, "%.1f", alt);
         }
         else
            sprintf(szBuff, "%d", (int)alt);
         if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || (alt < -500.0) )
            sprintf(szBuff, "0");

         osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff,  g_idFontOSDBig);
         if ( (pP->iUnitsHeight == prefUnitsImperial) || (pP->iUnitsHeight == prefUnitsFeets) )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "ALTITUDE (ft)", g_idFontOSDSmall);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yText, "ALTITUDE (m)", g_idFontOSDSmall);
      }
   }

   xCell += xCellWidth;

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      {
         if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0));

         osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
         if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
            osd_show_value_centered(xCell+xCellWidth/2, yText, "VERT SPEED (ft/s)", g_idFontOSDSmall);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yText, "VERT SPEED (m/s)", g_idFontOSDSmall);
      }
   }

   xCell += xCellWidth;

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HOME) )
   {
      strcpy(szBuff, "0");
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
        sprintf(szBuff, "%d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.heading);

      osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
      osd_show_value_centered(xCell+xCellWidth/2, yText, "HEADING", g_idFontOSDSmall);
   }

   xCell += xCellWidth;

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_TOTAL_DISTANCE) )
   {
      strcpy(szBuff, "0");
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%d", (int)_osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.total_distance/100));

      osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
      if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
         osd_show_value_centered(xCell+xCellWidth/2, yText, "TOTAL DIST (ft)", g_idFontOSDSmall);
      else
         osd_show_value_centered(xCell+xCellWidth/2, yText, "TOTAL DIST (m)", g_idFontOSDSmall);
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HOME ) )
      osd_show_home(0.5-height_text_text, yBig, true, 1.0);   
}

void render_osd_layout_lean_extended()
{
   if ( ( ! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry) || (NULL == g_pCurrentModel) )
      return;
   
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   Preferences* pP = get_Preferences();

   char szBuff[64];
   char szBuff2[64];
   char szTmp[64];

   u32 fontLine2 = g_idFontOSDSmall;
   u32 fontLine1 = g_idFontOSD;

   float height_text_small = osd_getFontHeightSmall();
   float height_text = osd_getFontHeight();

   float yLine2 = 1.0-osd_getMarginY()-height_text_small - osd_getSpacingV();
   float yLine1 = yLine2 - height_text;

   bool bShowBothSpeeds = false;
   bool bShowAnySpeed = false;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED))
      bShowAnySpeed = true;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED)))
      bShowBothSpeeds = true;

   float xCellWidth = (1.0-2.0*osd_getMarginX())/12.0;
   if ( bShowBothSpeeds )
      xCellWidth = (1.0-2.0*osd_getMarginX()-height_text)/12.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_BGBARS )
   {
      //float fAlpha = g_pRenderEngine->setGlobalAlfa(0.5);
      osd_set_colors_background_fill();
      g_pRenderEngine->drawRect(osd_getMarginX(), yLine1-osd_getSpacingV(), 1.0-2*osd_getMarginX(), 1.0 - osd_getMarginY() - (yLine1-osd_getSpacingV()));
      //g_pRenderEngine->setGlobalAlfa(fAlpha);
   }
   osd_set_colors();

   //--------------------
   // Bottom line

   float xCell = osd_getMarginX();

   strcpy(szBuff,"0");
   if ( bShowBothSpeeds )
      strcpy(szBuff,"0/0");
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
   {
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0), _osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f), _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));

         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
            else
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
         }
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0), 0.0);
            else
               sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f), 0.0);
         }
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed && 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
            sprintf(szBuff, "0/0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_GROUND_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.hspeed )
            strcpy(szBuff,"0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_SHOW_AIR_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.aspeed )
            strcpy(szBuff,"0");
      }
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || bShowAnySpeed )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMetric )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "G/A SPEED (km/h)", fontLine2);
         else if ( pP->iUnits == prefUnitsMeters )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "G/A SPEED (m/s)", fontLine2);
         else if ( pP->iUnits == prefUnitsImperial )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "G/A SPEED (mi/h)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "G/A SPEED (ft/s)", fontLine2);
      }
      else
      {
         if ( pP->iUnits == prefUnitsMetric )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "SPEED (km/h)", fontLine2);
         else if ( pP->iUnits == prefUnitsMeters )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "SPEED (m/s)", fontLine2);
         else if ( pP->iUnits == prefUnitsImperial )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "SPEED (mi/h)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "SPEED (ft/s)", fontLine2);
      }
      xCell += xCellWidth;
      if ( bShowBothSpeeds )
         xCell += height_text;
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_DISTANCE) )
   {
      strcpy(szBuff, "0");
      bool bKm = false;
      sprintf(szBuff, "%u", (unsigned int)_osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0));
      if ( _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0) >= 1000 )
      {
         sprintf(szBuff, "%.1f", _osd_convertKm(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.distance/100.0/1000.0));
         bKm = true;
      }
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);

      if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
      {
         if ( bKm )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "DIST (mi)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "DIST (ft)", fontLine2);
      }
      else
      {
         if ( bKm )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "DIST (km)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "DIST (m)", fontLine2);
      }
      xCell += xCellWidth;
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      {
         float alt = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude_abs/100.0f-1000.0;
         if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative && 0 != g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude)
            alt = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.altitude/100.0f-1000.0;
         alt = _osd_convertHeightMeters(alt);
         if ( fabs(alt) < 10.0 )
         {
            if ( fabs(alt) < 0.1 )
               alt = 0.0;
            sprintf(szBuff, "%.1f", alt);
         }
         else
            sprintf(szBuff, "%d", (int)alt);
         if ( (! g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent) || (alt < -500.0) )
            sprintf(szBuff, "0");

         osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff,  fontLine1);
         if ( (pP->iUnitsHeight == prefUnitsImperial) || (pP->iUnitsHeight == prefUnitsFeets) )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "ALT (ft)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "ALT (m)", fontLine2);
      }
      xCell += xCellWidth;
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_GENERIC ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE || 
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      {
         if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bFCTelemetrySourcePresent )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.vspeed/100.0f-1000.0));

         osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
         if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "V-SP (ft/s)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "V-SP (m/s)", fontLine2);
      }
      xCell += xCellWidth;
   }

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_HOME ) )
   {
      osd_show_home(xCell+xCellWidth/2-height_text, yLine1, true, 1.0);
      xCell += xCellWidth;
   }


   strcpy(szBuff, "% 0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%% %d", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.throttle);

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_THROTTLE) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "THROTTLE", fontLine2);
      xCell += xCellWidth;
   }

   double pC[4];
   memcpy(pC, get_Color_OSDText(), 4*sizeof(double));
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bWarningBatteryVoltage )
   {
      memcpy(pC, get_Color_OSDWarning(), 4*sizeof(double));
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
   }
   g_pRenderEngine->setColors(pC);

   strcpy(szBuff, "0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%.2f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.voltage/1000.0);

   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "BATT (V)", fontLine2);
      osd_set_colors();
      xCell += xCellWidth;
   }

   strcpy(szBuff, "0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%.1f", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.current/1000.0);
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "AMPS (A)", fontLine2);
      xCell += xCellWidth;
   }

   strcpy(szBuff, "0");
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      sprintf(szBuff, "%u", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.mah);
   if ( (NULL != pActiveModel) && (pActiveModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "USED (mAh)", fontLine2);
      xCell += xCellWidth;
   }

   int nQualityMain = 0;
   if ( g_bIsRouterReady )
   {
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL != pNICInfo && (g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId == 0) )
         if ( g_SM_RadioStats.radio_interfaces[i].rxQuality > nQualityMain )
            nQualityMain = g_SM_RadioStats.radio_interfaces[i].rxQuality;
      }
   }

   sprintf(szBuff, "%d%%", nQualityMain);
   sprintf(szBuff2, "LINK");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_RADIO_LINKS) )
   {
      bool bHasRubyRC = false;
   
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->rc_params.rc_enabled && (!g_pCurrentModel->is_spectator) )
         bHasRubyRC = true;
 
      if ( NULL != g_pCurrentModel && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotRubyTelemetryInfo )
      {
         if ( bHasRubyRC )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_rc_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
         else if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_mavlink_rc_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
         else if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uplink_mavlink_rx_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
      }
      if ( bHasRubyRC && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bRCFailsafeState )
      {
         strcpy(szBuff, "!RC FS!");
         strcpy(szBuff2, "RC RSSI");
      }
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff2, fontLine2);
      xCell += xCellWidth;
   }

   szBuff[0] = 0;
   szBuff2[0] = 0;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
      strcat(szBuff2, "0:00");
   
   int sec = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   int min = (g_pCurrentModel->m_Stats.uCurrentFlightTime)/60;

   szBuff[0] = 0;
   szBuff2[0] = 0;   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
   {
      sprintf(szTmp, "%d:%02d", min, sec);
      strcat(szBuff2, szTmp);
   }
   if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   {
      u8 mode = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED);
      szBuff[0] = 0; 
      szBuff2[0] = 0;  
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) )
         strcat(szBuff, model_getShortFlightMode(mode));
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
      {
         sprintf(szTmp, "%d:%02d", min, sec);
         strcat(szBuff2, szTmp);
      }
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_FLIGHT_MODE) ||
          (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()] & OSD_FLAG_SHOW_TIME) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff2, fontLine2);
      xCell += xCellWidth;
   }
}
