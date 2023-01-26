/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "../base/config.h"
#include "osd_common.h"
#include "osd_lean.h"
#include "osd_warnings.h"
#include "osd.h"
#include <math.h>

#include "colors.h"
#include "../renderer/render_engine.h"
#include "shared_vars.h"
#include "local_stats.h"
#include "link_watch.h"
#include "timers.h"

extern bool s_bDebugOSDShowAll;

void render_osd_layout_lean()
{
   if ( NULL == g_pdpfct || NULL == g_pCurrentModel )
      return;
   
   Preferences* pP = get_Preferences();

   char szBuff[64];
   char szBuff2[64];
   char szTmp[64];

   float height_text_big = osd_getFontHeightBig();
   float height_text_text = osd_getFontHeightSmall();
   float height_text_line2 = osd_getFontHeight();

   float yLine2 = 1.0-osd_getMarginY()-height_text_line2 - osd_getSpacingV();
   float yText = yLine2 - height_text_text - 2.0*osd_getSpacingV();
   float yBig = yText - height_text_big - osd_getSpacingV();

   float xCellWidth = (1.0-2.0*osd_getMarginX())/6.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_BGBARS )
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
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "THROTTLE %% %d", g_pdpfct->throttle);

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_THROTTLE) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   strcpy(szBuff, "AMPS (A) 0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "AMPS (A) %.1f", g_pdpfct->current/1000.0);
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;
   
   double* pC = get_Color_OSDText();
   if ( g_bOSDWarningBatteryVoltage )
   {
      pC = get_Color_OSDWarning();
      float fA = pC[3];
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
      g_pRenderEngine->setColors(pC);
      pC[3] = fA;
   }
   else
      g_pRenderEngine->setColors(pC);

   strcpy(szBuff, "BATT (V) 0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "BATT (V) %.2f", g_pdpfct->voltage/1000.0);
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   osd_set_colors();
   xCell += xCellWidth;

   strcpy(szBuff, "BAT USED (mAh) 0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "BAT USED (mAh) %u", g_pdpfct->mah);
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   szBuff[0] = 0;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
      strcat(szBuff, " 0:00");
   
   
   int sec = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   int min = (g_pCurrentModel->m_Stats.uCurrentFlightTime)/60;

   szBuff[0] = 0;   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
   {
      sprintf(szTmp, " %d:%02d", min, sec);
      strcat(szBuff, szTmp);
   }
   if ( NULL != g_pdpfct )
   {
      u8 mode = g_pdpfct->flight_mode & (~FLIGHT_MODE_ARMED);
      szBuff[0] = 0;   
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
         strcat(szBuff, model_getShortFlightMode(mode));
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
      {
         sprintf(szTmp, " %d:%02d", min, sec);
         strcat(szBuff, szTmp);
      }
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) ||
          (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);

   xCell += xCellWidth;

   int nQualityMain = 0;
   if ( NULL != g_pSM_RadioStats )
   {
      for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL != pNICInfo && (g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == 0) )
         if ( g_pSM_RadioStats->radio_interfaces[i].rxQuality > nQualityMain )
            nQualityMain = g_pSM_RadioStats->radio_interfaces[i].rxQuality;
      }
   }
   sprintf(szBuff, "LINK QUALITY %d%%", nQualityMain);

   bool bHasRubyRC = false;
   
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->rc_params.rc_enabled && (!g_pCurrentModel->is_spectator) )
      bHasRubyRC = true;
 
   if ( NULL != g_pCurrentModel && NULL != g_pPHRTE )
   {
      int val = 0;
      if ( bHasRubyRC )
         val = g_pPHRTE->uplink_rc_rssi;
      else if ( g_pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         val = g_pPHRTE->uplink_mavlink_rc_rssi;

      //if ( ! bHasRubyRC )
      //if ( val == 0 || val == 255 )
      //if ( g_pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RX_RSSI )
      //   val = g_pPHRTE->uplink_mavlink_rx_rssi;

      if ( val != 255 )
         sprintf(szBuff, "RC RSSI: %d%%", val);
      else
         strcpy(szBuff, "RC RSSI: ---");
   }
   if ( bHasRubyRC && g_bRCFailsafe )
      strcpy(szBuff, "!RC FS!");


   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_RADIO_LINKS) )
   {
      float w = g_pRenderEngine->textWidth(height_text_text, g_idFontOSD, szBuff);
      if ( w > xCellWidth )
         xCell -= (w-xCellWidth);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff, g_idFontOSD);
   }

   //--------------
   // Top line

   xCell = osd_getMarginX();

   bool bShowBothSpeeds = false;
   bool bShowAnySpeed = false;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED))
      bShowAnySpeed = true;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED)))
      bShowBothSpeeds = true;

   strcpy(szBuff,"0");
   if ( bShowBothSpeeds )
      strcpy(szBuff,"0/0");
   if ( link_has_fc_telemetry() )
   {
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0), _osd_convertMeters(g_pdpfct->aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f), _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));

         if ( 0 == g_pdpfct->hspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertMeters(g_pdpfct->aspeed/100.0f-1000.0));
            else
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));
         }
         if ( 0 == g_pdpfct->aspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0), 0.0);
            else
               sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f), 0.0);
         }
         if ( 0 == g_pdpfct->aspeed && 0 == g_pdpfct->hspeed )
            sprintf(szBuff, "0/0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_pdpfct->hspeed )
            strcpy(szBuff,"0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_pdpfct->aspeed )
            strcpy(szBuff,"0");
      }
   }

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

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_DISTANCE) )
   {
   strcpy(szBuff, "0");
   bool bKm = false;
   sprintf(szBuff, "%u", (unsigned int)_osd_convertMeters(g_pdpfct->distance/100.0));
   if ( _osd_convertMeters(g_pdpfct->distance/100.0) >= 1000 )
   {
      sprintf(szBuff, "%.1f", _osd_convertKm(g_pdpfct->distance/100.0/1000.0));
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

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_ALTITUDE) )
   {
   strcpy(szBuff, "0");
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
      float alt = g_pdpfct->altitude_abs/100.0f-1000.0;
      if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative && 0 != g_pdpfct->altitude)
         alt = g_pdpfct->altitude/100.0f-1000.0;
      alt = _osd_convertMeters(alt);
      if ( fabs(alt) < 10.0 )
      {
         if ( fabs(alt) < 0.1 )
            alt = 0.0;
         sprintf(szBuff, "%.1f", alt);
      }
      else
         sprintf(szBuff, "%d", (int)alt);
      if ( (! link_has_fc_telemetry()) || (alt < -500.0) )
         sprintf(szBuff, "0");

      osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff,  g_idFontOSDBig);
      if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
         osd_show_value_centered(xCell+xCellWidth/2, yText, "ALTITUDE (ft)", g_idFontOSDSmall);
      else
         osd_show_value_centered(xCell+xCellWidth/2, yText, "ALTITUDE (m)", g_idFontOSDSmall);
   }
   }

   xCell += xCellWidth;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_ALTITUDE) )
   {
   strcpy(szBuff, "0");
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
          g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
      if ( link_has_fc_telemetry() )
         sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->vspeed/100.0f-1000.0));

      osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
      if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
         osd_show_value_centered(xCell+xCellWidth/2, yText, "VERT SPEED (ft/s)", g_idFontOSDSmall);
      else
         osd_show_value_centered(xCell+xCellWidth/2, yText, "VERT SPEED (m/s)", g_idFontOSDSmall);
   }
   }

   xCell += xCellWidth;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_HOME) )
   {
   strcpy(szBuff, "0");
   if ( link_has_fc_telemetry() )
     sprintf(szBuff, "%d", g_pdpfct->heading);

   osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
   osd_show_value_centered(xCell+xCellWidth/2, yText, "HEADING", g_idFontOSDSmall);
   }

   xCell += xCellWidth;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_TOTAL_DISTANCE) )
   {
   strcpy(szBuff, "0");
   sprintf(szBuff, "%u", (unsigned long)_osd_convertMeters(g_pdpfct->total_distance/100));

   osd_show_value_centered(xCell+xCellWidth/2, yBig, szBuff, g_idFontOSDBig);
   if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
      osd_show_value_centered(xCell+xCellWidth/2, yText, "TOTAL DIST (ft)", g_idFontOSDSmall);
   else
      osd_show_value_centered(xCell+xCellWidth/2, yText, "TOTAL DIST (m)", g_idFontOSDSmall);
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_HOME ) )
      osd_show_home(0.5-height_text_text, yBig, true, 1.0);   
}

void render_osd_layout_lean_extended()
{
   if ( NULL == g_pdpfct || NULL == g_pCurrentModel )
      return;
   
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
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED))
      bShowAnySpeed = true;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED)))
      bShowBothSpeeds = true;

   float xCellWidth = (1.0-2.0*osd_getMarginX())/12.0;
   if ( bShowBothSpeeds )
      float xCellWidth = (1.0-2.0*osd_getMarginX()-height_text)/12.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_BGBARS )
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
   if ( link_has_fc_telemetry() )
   {
      if ( bShowBothSpeeds )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0), _osd_convertKm(g_pdpfct->aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f), _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));

         if ( 0 == g_pdpfct->hspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertMeters(g_pdpfct->aspeed/100.0f-1000.0));
            else
               sprintf(szBuff, "%.1f / %.1f", 0.0, _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));
         }
         if ( 0 == g_pdpfct->aspeed )
         {
            if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
               sprintf(szBuff, "%.1f / %.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0), 0.0);
            else
               sprintf(szBuff, "%.1f / %.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f), 0.0);
         }
         if ( 0 == g_pdpfct->aspeed && 0 == g_pdpfct->hspeed )
            sprintf(szBuff, "0/0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_GROUND_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->hspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_pdpfct->hspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_pdpfct->hspeed )
            strcpy(szBuff,"0");
      }
      else if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_AIR_SPEED )
      {
         if ( pP->iUnits == prefUnitsMeters || pP->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->aspeed/100.0f-1000.0));
         else
            sprintf(szBuff, "%.1f", _osd_convertKm((g_pdpfct->aspeed/100.0f-1000.0)*3.6f));
         if ( 0 == g_pdpfct->aspeed )
            strcpy(szBuff,"0");
      }
   }

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

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_DISTANCE) )
   {
      strcpy(szBuff, "0");
      bool bKm = false;
      sprintf(szBuff, "%u", (unsigned int)_osd_convertMeters(g_pdpfct->distance/100.0));
      if ( _osd_convertMeters(g_pdpfct->distance/100.0) >= 1000 )
      {
         sprintf(szBuff, "%.1f", _osd_convertKm(g_pdpfct->distance/100.0/1000.0));
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

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
           g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
           g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
           g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      {
         float alt = g_pdpfct->altitude_abs/100.0f-1000.0;
         if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative && 0 != g_pdpfct->altitude)
            alt = g_pdpfct->altitude/100.0f-1000.0;
         alt = _osd_convertMeters(alt);
         if ( fabs(alt) < 10.0 )
         {
            if ( fabs(alt) < 0.1 )
               alt = 0.0;
            sprintf(szBuff, "%.1f", alt);
         }
         else
            sprintf(szBuff, "%d", (int)alt);
         if ( (! link_has_fc_telemetry()) || (alt < -500.0) )
            sprintf(szBuff, "0");

         osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff,  fontLine1);
         if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "ALT (ft)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "ALT (m)", fontLine2);
      }
      xCell += xCellWidth;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_ALTITUDE) )
   {
      strcpy(szBuff, "0");
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
           g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
           g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
           g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      {
         if ( link_has_fc_telemetry() )
            sprintf(szBuff, "%.1f", _osd_convertMeters(g_pdpfct->vspeed/100.0f-1000.0));

         osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
         if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "V-SP (ft/s)", fontLine2);
         else
            osd_show_value_centered(xCell+xCellWidth/2, yLine2, "V-SP (m/s)", fontLine2);
      }
      xCell += xCellWidth;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_HOME ) )
   {
      osd_show_home(xCell+xCellWidth/2-height_text, yLine1, true, 1.0);
      xCell += xCellWidth;
   }


   strcpy(szBuff, "% 0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "%% %d", g_pdpfct->throttle);

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_THROTTLE) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "THROTTLE", fontLine2);
      xCell += xCellWidth;
   }

   double* pC = get_Color_OSDText();
   if ( g_bOSDWarningBatteryVoltage )
   {
      pC = get_Color_OSDWarning();
      float fA = pC[3];
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
      g_pRenderEngine->setColors(pC);
      pC[3] = fA;
   }
   else
      g_pRenderEngine->setColors(pC);

   strcpy(szBuff, "0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "%.2f", g_pdpfct->voltage/1000.0);

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "BATT (V)", fontLine2);
      osd_set_colors();
      xCell += xCellWidth;
   }

   strcpy(szBuff, "0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "%.1f", g_pdpfct->current/1000.0);
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "AMPS (A)", fontLine2);
      xCell += xCellWidth;
   }

   strcpy(szBuff, "0");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "%u", g_pdpfct->mah);
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_BATTERY) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, "USED (mAh)", fontLine2);
      xCell += xCellWidth;
   }

   int nQualityMain = 0;
   if ( NULL != g_pSM_RadioStats )
   {
      for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL != pNICInfo && (g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == 0) )
         if ( g_pSM_RadioStats->radio_interfaces[i].rxQuality > nQualityMain )
            nQualityMain = g_pSM_RadioStats->radio_interfaces[i].rxQuality;
      }
   }

   sprintf(szBuff, "%d%%", nQualityMain);
   sprintf(szBuff2, "LINK");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_RADIO_LINKS) )
   {
      bool bHasRubyRC = false;
   
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->rc_params.rc_enabled && (!g_pCurrentModel->is_spectator) )
         bHasRubyRC = true;
 
      if ( NULL != g_pCurrentModel && NULL != g_pPHRTE )
      {
         if ( bHasRubyRC )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_pPHRTE->uplink_rc_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
         else if ( g_pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_pPHRTE->uplink_mavlink_rc_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
         else if ( g_pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         {
            sprintf(szBuff, "RC RSSI: %d%%", g_pPHRTE->uplink_mavlink_rx_rssi);
            strcpy(szBuff2, "RC RSSI");
         }
      }
      if ( bHasRubyRC && g_bRCFailsafe )
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
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
      strcat(szBuff2, "0:00");
   
   int sec = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   int min = (g_pCurrentModel->m_Stats.uCurrentFlightTime)/60;

   szBuff[0] = 0;
   szBuff2[0] = 0;   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
      strcat(szBuff, "NONE");
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
   {
      sprintf(szTmp, "%d:%02d", min, sec);
      strcat(szBuff2, szTmp);
   }
   if ( NULL != g_pdpfct )
   {
      u8 mode = g_pdpfct->flight_mode & (~FLIGHT_MODE_ARMED);
      szBuff[0] = 0; 
      szBuff2[0] = 0;  
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) )
         strcat(szBuff, model_getShortFlightMode(mode));
      if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
      {
         sprintf(szTmp, "%d:%02d", min, sec);
         strcat(szBuff2, szTmp);
      }
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) ||
          (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_TIME) )
   {
      osd_show_value_centered(xCell+xCellWidth/2, yLine1, szBuff, fontLine1);
      osd_show_value_centered(xCell+xCellWidth/2, yLine2, szBuff2, fontLine2);
      xCell += xCellWidth;
   }
}
