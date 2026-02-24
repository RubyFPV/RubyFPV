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

#include "osd_plugins.h"
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/ctrl_preferences.h"
#include "../../base/plugins_settings.h"
#include "../../base/hardware_procs.h"
#include "../shared_vars.h"
#include "../timers.h"
#include "osd_common.h"
#include "../../radio/radiolink.h"
#include "../../radio/radiopackets2.h"
#include "../../renderer/render_engine.h"
#include "../pairing.h"
#include "../ruby_central.h"
#include "../local_stats.h"
#include "../link_watch.h"
#include "../process_router_messages.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "../colors.h"
#include "osd.h"

plugin_osd_t* g_pPluginsOSD[MAX_OSD_PLUGINS];
int g_iPluginsOSDCount = 0;
bool g_bOSDPluginsNeedTelemetryStreams = false;

void _osd_plugins_populate_public_telemetry_info()
{
   int iVehicleIndex = osd_get_current_data_source_vehicle_index();
   if ( ! g_VehiclesRuntimeInfo[iVehicleIndex].bGotRubyTelemetryInfo )
      return;
   g_VehicleTelemetryInfo.pi_temperature = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.temperatureC;
   g_VehicleTelemetryInfo.cpu_load = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.cpu_load;
   g_VehicleTelemetryInfo.cpu_mhz = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.cpu_mhz;
   g_VehicleTelemetryInfo.throttled = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.throttled;
   g_VehicleTelemetryInfo.rssi_dbm = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.uplink_rssi_dbm[0];
   g_VehicleTelemetryInfo.rssi_quality = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.uplink_link_quality[0];
   strncpy(&g_VehicleTelemetryInfo.vehicle_name[0], (char*)&(g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.vehicle_name[0]), MAX_VEHICLE_NAME_LENGTH);
   g_VehicleTelemetryInfo.vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   g_VehicleTelemetryInfo.vehicle_type = g_VehiclesRuntimeInfo[iVehicleIndex].headerRubyTelemetryExtended.vehicle_type;

   if ( ! g_VehiclesRuntimeInfo[iVehicleIndex].bGotFCTelemetry )
      return;

   g_VehicleTelemetryInfo.uFCFlags = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.uFCFlags;
   g_VehicleTelemetryInfo.flight_mode = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.flight_mode;
   g_VehicleTelemetryInfo.arm_time = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.arm_time;
   g_VehicleTelemetryInfo.throttle = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.throttle;
   g_VehicleTelemetryInfo.voltage = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.voltage;
   g_VehicleTelemetryInfo.current = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.current;
   g_VehicleTelemetryInfo.voltage2 = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.voltage2;
   g_VehicleTelemetryInfo.current2 = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.current2;
   g_VehicleTelemetryInfo.mah = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.mah;
   g_VehicleTelemetryInfo.altitude = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.altitude;
   g_VehicleTelemetryInfo.altitude_abs = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.altitude_abs;
   g_VehicleTelemetryInfo.distance = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.distance;
   g_VehicleTelemetryInfo.total_distance = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.total_distance;

   g_VehicleTelemetryInfo.vspeed = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.vspeed;
   g_VehicleTelemetryInfo.aspeed = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.aspeed;
   g_VehicleTelemetryInfo.hspeed = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.hspeed;
   //g_VehicleTelemetryInfo.roll = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.roll;
   //g_VehicleTelemetryInfo.pitch = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.pitch;
   g_VehicleTelemetryInfo.roll = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.roll/100.0-180.0;
   g_VehicleTelemetryInfo.pitch = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.pitch/100.0-180.0;

   if ( (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()]) & OSD_FLAG_REVERT_PITCH )
      g_VehicleTelemetryInfo.pitch = -g_VehicleTelemetryInfo.pitch;
   if ( (g_pCurrentModel->osd_params.osd_flags[osd_get_current_layout_index()]) & OSD_FLAG_REVERT_ROLL )
      g_VehicleTelemetryInfo.roll = -g_VehicleTelemetryInfo.roll;

   g_VehicleTelemetryInfo.satelites = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.satelites;
   g_VehicleTelemetryInfo.gps_fix_type = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.gps_fix_type;
   g_VehicleTelemetryInfo.hdop = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.hdop;
   g_VehicleTelemetryInfo.heading = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.heading;
   g_VehicleTelemetryInfo.latitude = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.latitude;
   g_VehicleTelemetryInfo.longitude = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.longitude;

   g_VehicleTelemetryInfo.rc_rssi = g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.rc_rssi;
   memcpy(&(g_VehicleTelemetryInfo.extra_info[0]), &(g_VehiclesRuntimeInfo[iVehicleIndex].headerFCTelemetry.extra_info[0]), sizeof(u8)*12);

   g_VehicleTelemetryInfo.isHomeSet = g_VehiclesRuntimeInfo[iVehicleIndex].bHomeSet?1:0;
   g_VehicleTelemetryInfo.home_latitude = g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLat * 10000000.0f;
   g_VehicleTelemetryInfo.home_longitude = g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLon * 10000000.0f;

   g_VehicleTelemetryInfo.fCameraFOVHorizontal = 90;
   g_VehicleTelemetryInfo.fCameraFOVVertical = 90;

   if ( NULL != g_pCurrentModel )
   {
   g_VehicleTelemetryInfo.fMaxCurrent = (float)g_pCurrentModel->m_Stats.uCurrentMaxCurrent;
   g_VehicleTelemetryInfo.fMinVoltage = (float)g_pCurrentModel->m_Stats.uCurrentMinVoltage;
   g_VehicleTelemetryInfo.fMaxAltitude = (float)g_pCurrentModel->m_Stats.uCurrentMaxAltitude;
   g_VehicleTelemetryInfo.fMaxDistance = (float)g_pCurrentModel->m_Stats.uCurrentMaxDistance;

   g_VehicleTelemetryInfo.fTotalCurrent = (float)g_pCurrentModel->m_Stats.uCurrentTotalCurrent;
   g_VehicleTelemetryInfo.u32VehicleOnTime = g_pCurrentModel->m_Stats.uCurrentOnTime;
   g_VehicleTelemetryInfo.u32VehicleArmTime = g_pCurrentModel->m_Stats.uCurrentFlightTime;
   }

   g_VehicleTelemetryInfo.pExtraInfo = NULL;
}

SinglePluginSettings* osd_get_settings_for_plugin_for_model(const char* szPluginUID, Model* pModel)
{
   if ( NULL == szPluginUID || NULL == pModel )
      return NULL;

   SinglePluginSettings* pPlugin = pluginsGetByUID(szPluginUID);
   if ( NULL == pPlugin )
   {
      log_line("Found new OSD plugin, GUID: %s. Adding it to plugin settings file...", szPluginUID);

      for ( int i=0; i<g_iPluginsOSDCount; i++ )
         if ( NULL != g_pPluginsOSD[i] )
         if ( 0 == strcmp(g_pPluginsOSD[i]->szUID, szPluginUID ) )
         {
            pPlugin = pluginsSettingsAddNew(osd_plugins_get_name(i), szPluginUID);
            save_PluginsSettings();
            log_line("Added plugin settings for GUID: %s", szPluginUID);
            break;
         }
   }

   if ( NULL == pPlugin )
   {
      log_softerror_and_alarm("Failed to add settings for new OSD plugin, GUID: %s", szPluginUID);
      return NULL;
   }

   if ( doesPluginHasModelSettings(pPlugin, pModel) )
      return pPlugin;

   // Populate default plugin settings for this model

   log_line("Populating default OSD plugin settings for current model, for plugin GUID %s", szPluginUID);

   int iModelSettingsIndex = getPluginModelSettingsIndex(pPlugin, pModel);

   int iOSDPluginIndex = -1;
   for ( int i=0; i<g_iPluginsOSDCount; i++ )
      if ( NULL != g_pPluginsOSD[i] )
      if ( 0 == strcmp(g_pPluginsOSD[i]->szUID, szPluginUID ) )
      {
         iOSDPluginIndex = i;
         break;
      }

   if ( -1 == iOSDPluginIndex )
   {
      log_softerror_and_alarm("Can't find OSD plugin info for GUID: %s", szPluginUID);
      return NULL;
   }

   if ( NULL != g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetDefaultWidth &&
        NULL != g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetDefaultHeight )
   {
      pPlugin->fWidth[iModelSettingsIndex][0] = (*(g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetDefaultWidth))();
      pPlugin->fHeight[iModelSettingsIndex][0] = (*(g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetDefaultHeight))();
      pPlugin->fXPos[iModelSettingsIndex][0] = 0.5 * (1.0 - pPlugin->fWidth[iModelSettingsIndex][0]);
      pPlugin->fYPos[iModelSettingsIndex][0] = 0.5 * (1.0 - pPlugin->fHeight[iModelSettingsIndex][0]);
   }
   else
   {
      pPlugin->fWidth[iModelSettingsIndex][0] = 0.1/g_pRenderEngine->getAspectRatio();
      pPlugin->fHeight[iModelSettingsIndex][0] = 0.1;
      pPlugin->fXPos[iModelSettingsIndex][0] = 0.5 * (1.0 - pPlugin->fWidth[iModelSettingsIndex][0]);
      pPlugin->fYPos[iModelSettingsIndex][0] = 0.5 * (1.0 - pPlugin->fHeight[iModelSettingsIndex][0]);
   }

   // Compute a default position not ocupied by other plugins (on a 4x4 grid)

   float xGridWidth = 0.24;
   float yGridHeight = 0.21;
   for( int iCell=0; iCell<16; iCell++ )
   {
      bool bCellFree = true;
      float x = xGridWidth*(iCell%4) + 0.04;
      float y = yGridHeight*((int)(iCell/4)) + 0.14;
      float w = xGridWidth;
      float h = yGridHeight;

      // Allow small overlap

      x += xGridWidth*0.1;
      y += yGridHeight*0.1;
      w -= xGridWidth*0.2;
      h -= yGridHeight*0.2;
      for ( int k=0; k<g_iPluginsOSDCount; k++ )
      {
         if ( NULL == g_pPluginsOSD[k] || k == iOSDPluginIndex || NULL == g_pCurrentModel )
            continue;

         SinglePluginSettings* pPluginSettings2 = pluginsGetByUID(g_pPluginsOSD[k]->szUID);

         if ( NULL == pPluginSettings2 )
            continue;

         int osdLayoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

         if ( g_pRenderEngine->rectIntersect( x, y, w,h, pPluginSettings2->fXPos[iModelSettingsIndex][osdLayoutIndex],  pPluginSettings2->fYPos[iModelSettingsIndex][osdLayoutIndex], pPluginSettings2->fWidth[iModelSettingsIndex][osdLayoutIndex], pPluginSettings2->fHeight[iModelSettingsIndex][osdLayoutIndex] ) )
         {
            bCellFree = false;
            break;
         }
      }

      // Do not move default AHI plugin
      if ( 0 == strcmp(g_pPluginsOSD[iOSDPluginIndex]->szUID, "89930-2454352Q-ASDGF9J-RAHI") )
         break;

      if ( bCellFree )
      {
         pPlugin->fXPos[iModelSettingsIndex][0] = x;
         pPlugin->fYPos[iModelSettingsIndex][0] = y;
         break;
      }
   }


   for( int i=1; i<MODEL_MAX_OSD_SCREENS; i++ )
   {
      pPlugin->fWidth[iModelSettingsIndex][i] = pPlugin->fWidth[iModelSettingsIndex][0];
      pPlugin->fHeight[iModelSettingsIndex][i] = pPlugin->fHeight[iModelSettingsIndex][0];
      pPlugin->fXPos[iModelSettingsIndex][i] = pPlugin->fXPos[iModelSettingsIndex][0];
      pPlugin->fYPos[iModelSettingsIndex][i] = pPlugin->fYPos[iModelSettingsIndex][0];
   }


   int nSettingsCount = 0;

   if ( NULL != g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetPluginSettingsCount )
      nSettingsCount = (*(g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetPluginSettingsCount))();
   if ( nSettingsCount < 0 || nSettingsCount >= MAX_PLUGIN_SETTINGS )
      nSettingsCount = MAX_PLUGIN_SETTINGS - 1;

   pPlugin->nSettingsCount = nSettingsCount;

   for( int i=0; i<nSettingsCount; i++ )
   {
            int defValue = 0;
            if ( NULL != g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetPluginSettingDefaultValue )
               defValue = (*(g_pPluginsOSD[iOSDPluginIndex]->pFunctionGetPluginSettingDefaultValue))(i);
            for( int k=0; k<MODEL_MAX_OSD_SCREENS; k++ )
               pPlugin->nSettings[iModelSettingsIndex][k][i] = defValue;
   }

   save_PluginsSettings();

   return pPlugin;
}

void _osd_load_plugin(const char* szFile)
{
   if ( g_iPluginsOSDCount >= MAX_OSD_PLUGINS-1 )
      return;

   log_line("Loading OSD plugin [%s] ...", szFile);
   g_pPluginsOSD[g_iPluginsOSDCount] = (plugin_osd_t*)malloc(sizeof(plugin_osd_t));
   if ( NULL == g_pPluginsOSD[g_iPluginsOSDCount] )
      return;
   strcpy(g_pPluginsOSD[g_iPluginsOSDCount]->szPluginFile, szFile);
   g_pPluginsOSD[g_iPluginsOSDCount]->bBoundingBox = false;
   g_pPluginsOSD[g_iPluginsOSDCount]->bHighlight = false;
   g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary = dlopen(szFile, RTLD_LAZY | RTLD_GLOBAL);

   if ( g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary == NULL)
   {
      log_softerror_and_alarm("Failed to load OSD plugin: [%s]", szFile);
      return;
   }

   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionInit = (void (*)(void*)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "init");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionRender = (void (*)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "render");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetName = (char* (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getName");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetVersion = (int (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getVersion");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetUID = (char* (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getUID");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetDefaultWidth = (float (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getDefaultWidth");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetDefaultHeight = (float (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getDefaultHeight");

   if ( NULL == g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionInit ||
        NULL == g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionRender ||
        NULL == g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetName ||
        NULL == g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetUID )
   {
      log_softerror_and_alarm("Failed to load OSD plugin: [%s]: does not export the required interfaces.", szFile);
      dlclose(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary);
      return;
   }

   // Get optional functions

   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingsCount = (int (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingsCount");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingName = (char* (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingName");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingType = (int (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingType");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingMinValue = (int (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingMinValue");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingMaxValue = (int (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingMaxValue");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingDefaultValue = (int (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingDefaultValue");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingOptionsCount = (int (*)(int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingOptionsCount");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetPluginSettingOptionName = (char* (*)(int, int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "getPluginSettingOptionName");

   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionOnNewVehicle = (void (*)(u32)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "onNewVehicle");

   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionRequestTelemetryStreams = (int (*)(void)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "requestTelemetryStreams");
   g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionOnTelemetryStreamData = (void (*)(u8*, int, int)) dlsym(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary, "onTelemetryStreamData");

   char* szPluginName = (*(g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetName))();
   char* szPluginUID = (*(g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionGetUID))();

   strcpy(g_pPluginsOSD[g_iPluginsOSDCount]->szUID, szPluginUID);

   if ( NULL == g_pRenderEngineOSDPlugins )
      g_pRenderEngineOSDPlugins = new RenderEngineUI();

   (*(g_pPluginsOSD[g_iPluginsOSDCount]->pFunctionInit))((void*)g_pRenderEngineOSDPlugins);

   // Gets and/or populates default settings for this plugin and this model

   g_iPluginsOSDCount++;
   SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(szPluginUID, g_pCurrentModel);

   if ( NULL == pPluginSettings )
   {
      g_iPluginsOSDCount--;
      log_softerror_and_alarm("Failed to create settings for OSD plugin GUID %s: can't create the settings for the plugin.", szPluginUID);
      dlclose(g_pPluginsOSD[g_iPluginsOSDCount]->pLibrary);
      return;
   }

   log_line("Loaded OSD plugin: %s, UID: %s, file: [%s]", szPluginName, szPluginUID, szFile);
}

void osd_plugins_load()
{
   for( int i=0; i<g_iPluginsOSDCount; i++ )
      if ( NULL != g_pPluginsOSD[i]->pLibrary )
         dlclose(g_pPluginsOSD[i]->pLibrary);

   g_iPluginsOSDCount = 0;
   g_bOSDPluginsNeedTelemetryStreams = false;

   load_PluginsSettings();

   log_line("Loading OSD plugins...");

   DIR *d;
   struct dirent *dir;
   char szFile[1024];
   d = opendir(FOLDER_OSD_PLUGINS);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         ruby_signal_alive();
         if ( strlen(dir->d_name) < 4 )
            continue;
         bool match = false;
         if ( NULL != strstr(dir->d_name, ".so."))
            match = true;
         if ( ! match )
            continue;
         sprintf(szFile, "%s%s", FOLDER_OSD_PLUGINS, dir->d_name);
         _osd_load_plugin(szFile);
      }
      closedir(d);
   }

   log_line("Loaded %d OSD plugins.", g_iPluginsOSDCount);
}

void osd_plugins_render()
{
   if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
      return;

   Model* pModel = osd_get_current_data_source_vehicle_model();
   if ( NULL == pModel )
      return;
   if ( pModel->is_spectator && (!(pModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   Preferences* p = get_Preferences();
   _osd_plugins_populate_public_telemetry_info();

   bool bAnyHighlight = false;
   bool bOldOSDPluginsNeedTelemetry = g_bOSDPluginsNeedTelemetryStreams;
   g_bOSDPluginsNeedTelemetryStreams = false;

   for( int i=0; i<g_iPluginsOSDCount; i++ )
   {
      char* szUID = osd_plugins_get_uid(i);
      SinglePluginSettings* pPlugin = osd_get_settings_for_plugin_for_model(szUID, pModel);
      if ( NULL == pPlugin )
         continue;
      if ( 0 == pPlugin->nEnabled )
         continue;

      if ( NULL == g_pPluginsOSD[i] )
         continue;
      if ( g_pPluginsOSD[i]->bHighlight )
      {
         bAnyHighlight = true;
         break;
      }

      if ( NULL != g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams )
      {
         int iRes = (*(g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams))();
         if ( iRes > 0 )
            g_bOSDPluginsNeedTelemetryStreams = true;
      }
   }

   if ( bOldOSDPluginsNeedTelemetry != g_bOSDPluginsNeedTelemetryStreams )
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY, (u32)(g_bOSDPluginsNeedTelemetryStreams?1:0));

   osd_set_colors();

   if ( bAnyHighlight )
   {
      float fA = g_pRenderEngine->setGlobalAlfa(0.7);
      double pC[4] = {255,100,100,0.7};
      g_pRenderEngine->setStroke(pC);

      g_pRenderEngine->drawLine(0.01, 0.5, 0.99, 0.5 );
      g_pRenderEngine->drawLine(0.5, 0.01, 0.5, 0.99 );

      g_pRenderEngine->drawLine(0.01, 0.33, 0.99, 0.33 );
      g_pRenderEngine->drawLine(0.0, 0.66, 1.0, 0.66);
      g_pRenderEngine->drawLine(0.33, 0.0, 0.33, 1.0 );
      g_pRenderEngine->drawLine(0.66, 0.0, 0.66, 1.0 );
      g_pRenderEngine->setGlobalAlfa(fA);
   }


   for( int i=0; i<g_iPluginsOSDCount; i++ )
   {
      char* szUID = osd_plugins_get_uid(i);
      SinglePluginSettings* pPlugin = osd_get_settings_for_plugin_for_model(szUID, pModel);
      if ( NULL == pPlugin )
         continue;
      if ( 0 == pPlugin->nEnabled )
         continue;

      if ( NULL == g_pPluginsOSD[i] )
         continue;
      if ( NULL == g_pPluginsOSD[i]->pFunctionRender )
         continue;

      if ( !(pModel->osd_params.instruments_flags[osd_get_current_layout_index()] & (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN<<i)) )
         continue;

      int iModelSettingsIndex = getPluginModelSettingsIndex(pPlugin, pModel);
      int osdLayoutIndex = pModel->osd_params.iCurrentOSDScreen;


      vehicle_and_telemetry_info_t telemetry_info;
      vehicle_and_telemetry_info2_t telemetry_info2;

      memcpy(&telemetry_info, &g_VehicleTelemetryInfo, sizeof(vehicle_and_telemetry_info_t));
      telemetry_info.pExtraInfo = &telemetry_info2;
      telemetry_info2.uTimeNow = g_TimeNow;
      telemetry_info2.uTimeNowVehicle = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfo.uTimeNow;
      telemetry_info2.uVehicleId = 0;
      telemetry_info2.uRelayedVehicleId = pModel->relay_params.uRelayedVehicleId;
      telemetry_info2.uIsRelaing = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_IS_RELAYING;
      telemetry_info2.uIsSpectatorMode = pModel->is_spectator;

      telemetry_info2.uWindHeading = 0xFFFF;
      telemetry_info2.fWindSpeed = 0.0f;
      if ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      {
         telemetry_info2.uWindHeading = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[7] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[8];
         if ( telemetry_info2.uWindHeading == 0 )
            telemetry_info2.uWindHeading = 0xFFFF;
         else
            telemetry_info2.uWindHeading--;

         u16 uSpeed = ((u16)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[9] << 8) | g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetry.extra_info[10];
         if ( 0 != uSpeed )
            telemetry_info2.fWindSpeed = ((float)uSpeed-1)/100.0;
      }
      telemetry_info2.uThrottleInput = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfo.uThrottleInput;
      telemetry_info2.uThrottleOutput = g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerRubyTelemetryExtraInfo.uThrottleOutput;
      telemetry_info2.uVehicleId = pModel->uVehicleId;
      telemetry_info2.uIsSpectatorMode = (pModel->is_spectator?1:0);

      plugin_settings_info_t2 plugin_settings;
      plugin_settings_info_t2_extra plugin_settings_extra_info;

      plugin_settings.uFlags = 0;
      plugin_settings.pExtraInfo = &plugin_settings_extra_info;
      plugin_settings.fLineThicknessPx = 2.0;
      plugin_settings.fOutlineThicknessPx = 0.5;
      plugin_settings.fBackgroundAlpha = 0.5;
      switch( p->iAHIStrokeSize )
      {
         case -2: plugin_settings.fLineThicknessPx = 1.0; break;
         case -1: plugin_settings.fLineThicknessPx = 1.8; break;
         case 0: plugin_settings.fLineThicknessPx = 2.4; break;
         case 1: plugin_settings.fLineThicknessPx = 3.1; break;
         case 2: plugin_settings.fLineThicknessPx = 3.6; break;
      }
      plugin_settings.fOutlineThicknessPx = plugin_settings.fLineThicknessPx + 2.0;

      u32 tr = ((pModel->osd_params.osd_preferences[osd_get_current_layout_index()])>>8) & 0xFF;
      switch ( tr )
      {
         case 0: plugin_settings.fBackgroundAlpha = 0.05; break;
         case 1: plugin_settings.fBackgroundAlpha = 0.26; break;
         case 2: plugin_settings.fBackgroundAlpha = 0.5; break;
         case 3: plugin_settings.fBackgroundAlpha = 0.9; break;
      }
      plugin_settings.fBackgroundAlpha += 0.1;
      if ( plugin_settings.fBackgroundAlpha > 1.0 )
         plugin_settings.fBackgroundAlpha = 1.0;

      int nSettingsCount = osd_plugin_get_settings_count(i);
      if ( nSettingsCount > MAX_PLUGIN_SETTINGS )
         nSettingsCount = MAX_PLUGIN_SETTINGS;

      for( int k=0; k<nSettingsCount; k++ )
          plugin_settings.nSettingsValues[k] = pPlugin->nSettings[iModelSettingsIndex][osdLayoutIndex][k];

      plugin_settings_extra_info.iMeasureUnitsType = p->iUnits;

      float xPos = osd_getMarginX() + (1.0-2.0*osd_getMarginX())*pPlugin->fXPos[iModelSettingsIndex][osdLayoutIndex];
      float yPos = osd_getMarginY() + (1.0-2.0*osd_getMarginY())*pPlugin->fYPos[iModelSettingsIndex][osdLayoutIndex];

      (*(g_pPluginsOSD[i]->pFunctionRender))(&telemetry_info, &plugin_settings, xPos, yPos, pPlugin->fWidth[iModelSettingsIndex][osdLayoutIndex], pPlugin->fHeight[iModelSettingsIndex][osdLayoutIndex]);

      if ( g_pPluginsOSD[i]->bBoundingBox )
      {
         osd_set_colors();
         if ( g_pPluginsOSD[i]->bHighlight )
         {
            double pC[4] = {255,50,50,1.0};
            g_pRenderEngine->setStroke(pC, 3.0);
         }
         g_pRenderEngine->setFill(0,0,0,0);
         g_pRenderEngine->drawRect(xPos, yPos, pPlugin->fWidth[iModelSettingsIndex][osdLayoutIndex], pPlugin->fHeight[iModelSettingsIndex][osdLayoutIndex]);
      }
   }
}

int osd_plugins_get_count()
{
   return g_iPluginsOSDCount;
}

plugin_osd_t* osd_plugins_get(int index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;
   return g_pPluginsOSD[index];
}

char* osd_plugins_get_name(int index)
{
   static char s_szOSDPluginName[256];
   s_szOSDPluginName[0] = 0;
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;

   if ( NULL != g_pPluginsOSD[index]->pFunctionGetName )
   {
      char* szPluginName = (*(g_pPluginsOSD[index]->pFunctionGetName))();
      if ( NULL != g_pPluginsOSD[index]->pFunctionGetVersion )
         sprintf(s_szOSDPluginName, "%s v.%d", szPluginName,(*(g_pPluginsOSD[index]->pFunctionGetVersion))());
      else
         strcpy(s_szOSDPluginName, szPluginName);
   }
   return s_szOSDPluginName;
}

char* osd_plugins_get_short_name(int index)
{
   static char s_szOSDPluginName[256];
   s_szOSDPluginName[0] = 0;
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;

   if ( NULL != g_pPluginsOSD[index]->pFunctionGetName )
   {
      char* szPluginName = (*(g_pPluginsOSD[index]->pFunctionGetName))();
      strcpy(s_szOSDPluginName, szPluginName);
   }
   return s_szOSDPluginName;
}

char* osd_plugins_get_uid(int index)
{
   static char s_szOSDPluginUID[256];
   s_szOSDPluginUID[0] = 0;
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;

   if ( NULL != g_pPluginsOSD[index]->pFunctionGetUID )
   {
      char* szPluginUID = (*(g_pPluginsOSD[index]->pFunctionGetUID))();
      strcpy(s_szOSDPluginUID, szPluginUID);
   }
   return s_szOSDPluginUID;
}

void osd_plugins_delete(int index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return;

   if ( NULL != g_pPluginsOSD[index]->pLibrary )
      dlclose(g_pPluginsOSD[index]->pLibrary);

   char szComm[1024];
   sprintf(szComm, "rm -rf %s", g_pPluginsOSD[index]->szPluginFile);
   hw_execute_bash_command(szComm, NULL);

   for( int k=index; k<g_iPluginsOSDCount-1; k++ )
      g_pPluginsOSD[k] = g_pPluginsOSD[k+1];
   g_iPluginsOSDCount--;
}

int osd_plugin_get_settings_count(int index )
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return 0;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingsCount )
   {
      int count = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingsCount))();
      return count;
   }
   return 0;
}

char* osd_plugin_get_setting_name(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingName )
   {
      char* szName = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingName))(sub_index);
      return szName;
   }
   return NULL;
}

int osd_plugin_get_setting_type(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return PLUGIN_SETTING_TYPE_BOOL;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingType )
   {
      int type = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingType))(sub_index);
      return type;
   }
   return PLUGIN_SETTING_TYPE_BOOL;
}

int osd_plugin_get_setting_minvalue(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return 0;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingMinValue )
   {
      int val = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingMinValue))(sub_index);
      return val;
   }
   return 0;
}

int osd_plugin_get_setting_maxvalue(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return 0;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingMaxValue )
   {
      int val = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingMaxValue))(sub_index);
      return val;
   }
   return 0;
}

int osd_plugin_get_setting_defaultvalue(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return 0;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingDefaultValue )
   {
      int val = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingDefaultValue))(sub_index);
      return val;
   }
   return 0;
}

int osd_plugin_get_setting_options_count(int index, int sub_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return 0;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingOptionsCount )
   {
      int val = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingOptionsCount))(sub_index);
      return val;
   }
   return 0;
}

char* osd_plugin_get_setting_option_name(int index, int sub_index, int option_index)
{
   if ( index < 0 || index >= g_iPluginsOSDCount )
      return NULL;
   if ( NULL != g_pPluginsOSD[index]->pFunctionGetPluginSettingOptionName )
   {
      char* szVal = (*(g_pPluginsOSD[index]->pFunctionGetPluginSettingOptionName))(sub_index, option_index);
      return szVal;
   }
   return NULL;
}
