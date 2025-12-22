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

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <locale.h>
#include <time.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/resource.h>
#include <pthread.h>
#include <math.h>

#include "ruby_central.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/shared_mem.h"
#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/hardware_audio.h"
#include "../base/hardware_files.h"
#include "../base/hdmi.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../utils/utils_controller.h"
#include "../base/plugins_settings.h"
#include "../base/vehicle_rt_info.h"
//#include "../base/radio_utils.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../base/core_plugins_settings.h"
#include "../base/utils.h"
#if defined (HW_PLATFORM_RASPBERRY)
#include "../renderer/render_engine_raw.h"
#endif
#if defined (HW_PLATFORM_RADXA)
#include "../renderer/drm_core.h"
#include "../renderer/render_engine_cairo.h"
#include <SDL2/SDL.h>
#endif

#include "../common/string_utils.h"
#include "../common/strings_loc.h"
#include "../common/relay_utils.h"
#include "../common/favorites.h"

#include "colors.h"
#include "osd.h"
#include "osd_common.h"
#include "osd_plugins.h"
#include "osd_widgets.h"
#include "osd_debug_stats.h"
#include "menu.h"
#include "fonts.h"
#include "popup.h"
#include "shared_vars.h"
#include "pairing.h"
#include "link_watch.h"
#include "warnings.h"
#include "keyboard.h"
#include "media.h"
#include "render_commands.h"
#include "handle_commands.h"
#include "menu_confirmation.h"
#include "menu_confirmation_hdmi.h"
#include "ui_alarms.h"
#include "notifications.h"
#include "local_stats.h"
#include "rx_scope.h"
#include "forward_watch.h"
#include "timers.h"
#include "launchers_controller.h"
#include "events.h"
#include "menu_info_booster.h"
#include "menu_confirmation_import.h"
#include "menu_confirmation_sdcard_update.h"
#include "process_router_messages.h"
#include "quickactions.h"
#include "oled/oled_render.h"
#include "video_playback.h"

u32 s_idBgImage[5];
u32 s_idBgImageMenu[5];
int s_iBgImageCount = 1;
int s_iBgImageIndexPrev = 0;
int s_iBgImageIndex = 0;
u32 s_uTimeLastChangeBgImage = 0;

bool g_bPlayIntro = true;
bool g_bPlayIntroWillEnd = false;
sem_t* s_pSemaphoreVideoIntroWillFinish = NULL;
sem_t* s_pSemaphoreVideoIntro = NULL;

bool g_bMarkedHDMIReinit = false;
bool g_bIsReinit = false;
bool g_bIsHDMIConfirmation = false;
bool s_bShowMira = false;

static int s_iRubyFPS = 0;
static int s_iFPSCount = 0;
static u32 s_uFPSLastTimeCheck = 0;

static int s_iCountRequestsPauseWatchdog = 0;

static u32 s_uMicroTimeMenuRender = 0;
static u32 s_uMicroTimePopupRender = 0;
static u32 s_uMicroTimeOSDRender = 0;

static u32 s_uTimeLastRender = 0;
static u32 s_uTimeLastRenderDuration = 0;

static u32 s_TimeCentralInitializationComplete = 0;
static u32 s_TimeLastMenuInput = 0;

static bool s_bFreezeOSD = false;
static u32 s_uTimeFreezeOSD = 0;

static u32 s_uTimeToSwitchLogLevel = 0;

Popup popupNoModel("No vehicle defined or linked to!", 0.2, 0.45, 5);
Popup popupStartup("System starting. Please wait.", 0.05, 0.16, 0);

static char s_szFileHDMIChanged[128];

MenuConfirmationHDMI* s_pMenuConfirmHDMI = NULL;
MenuConfirmationImport* s_pMenuConfirmationImport = NULL;

Popup* ruby_get_startup_popup()
{
   popupStartup.useSmallLines(false);
   return &popupStartup;
}

int ruby_get_start_sequence_step()
{
   return s_StartSequence;
}

char* _ruby_central_get_star_seq_string(int iSeqId)
{
   static char s_szRubyCentralStartSeqText[128];
   s_szRubyCentralStartSeqText[0] = 0;
   sprintf(s_szRubyCentralStartSeqText, "N/A (%d)", iSeqId);

   if ( iSeqId == START_SEQ_NONE )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_NONE");
   else if ( iSeqId == START_SEQ_PRE_LOAD_CONFIG )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_LOAD_CONFIG");
   else if ( iSeqId == START_SEQ_LOAD_CONFIG )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_LOAD_CONFIG");
   else if ( iSeqId == START_SEQ_PRE_SEARCH_DEVICES )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_SEARCH_DEVICES");
   else if ( iSeqId == START_SEQ_SEARCH_DEVICES )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SEARCH_DEVICES");
   else if ( iSeqId == START_SEQ_PRE_SEARCH_INTERFACES )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_SEARCH_INTERFACES");
   else if ( iSeqId == START_SEQ_SEARCH_INTERFACES )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SEARCH_INTERFACES");
   else if ( iSeqId == START_SEQ_PRE_NICS )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_NICS");
   else if ( iSeqId == START_SEQ_NICS )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_NICS");
   else if ( iSeqId == START_SEQ_PRE_SYNC_DATA )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_SYNC_DATA");
   else if ( iSeqId == START_SEQ_SYNC_DATA )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SYNC_DATA");
   else if ( iSeqId == START_SEQ_SYNC_DATA_FAILED )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SYNC_DATA_FAILED");

   else if ( iSeqId == START_SEQ_PRE_LOAD_DATA )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PRE_LOAD_DATA");
   else if ( iSeqId == START_SEQ_LOAD_DATA )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_LOAD_DATA");
   else if ( iSeqId == START_SEQ_LOAD_PLUGINS )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_LOAD_PLUGINS");
   else if ( iSeqId == START_SEQ_START_PROCESSES )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_START_PROCESSES");
   else if ( iSeqId == START_SEQ_SCAN_MEDIA_PRE )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SCAN_MEDIA_PRE");
   else if ( iSeqId == START_SEQ_SCAN_MEDIA )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_SCAN_MEDIA");
   else if ( iSeqId == START_SEQ_PROCESS_VIDEO )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_PROCESS_VIDEO");
   else if ( iSeqId == START_SEQ_COMPLETED )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_COMPLETED");
   else if ( iSeqId == START_SEQ_FAILED )
     strcpy(s_szRubyCentralStartSeqText, "START_SEQ_FAILED");
   
   return s_szRubyCentralStartSeqText;
}


void load_resources()
{
   loadAllFonts(true);

   s_iBgImageCount = 3;
   int iDelta = 1;
   iDelta = 5;

   for( int i=0; i<s_iBgImageCount; i++ )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      sprintf(szFile, "res/ruby_bg%d.png", i+iDelta);
      if ( access(szFile, R_OK) != -1 )
         s_idBgImage[i] = g_pRenderEngine->loadImage(szFile);

      sprintf(szFile, "res/ruby_bg%d_blr.png", i+iDelta);
      
      if ( access(szFile, R_OK) != -1 )
         s_idBgImageMenu[i] = g_pRenderEngine->loadImage(szFile);
      else
         s_idBgImageMenu[i] = s_idBgImage[0];
   }

   srand(get_current_timestamp_ms());
   s_iBgImageIndexPrev = 0;
   s_iBgImageIndex = rand()%s_iBgImageCount;
   if ( (s_iBgImageIndex < 0) || (s_iBgImageIndex > s_iBgImageCount-1) )
      s_iBgImageIndex = 0;

   osd_load_resources();
}

void _draw_background_picture()
{
   if ( g_TimeNow > s_uTimeLastChangeBgImage + 40000 )
   {
      s_uTimeLastChangeBgImage = g_TimeNow;
      if ( (! g_bUpdateInProgress) && (! g_bSearching) )
      {
         log_line("Not searching or updating. Update background picture.");
         s_iBgImageIndexPrev = s_iBgImageIndex;
         int iIndex = s_iBgImageIndex;
         if ( s_iBgImageCount == 2 )
         {
            iIndex = (iIndex+1)%2;
            s_iBgImageIndex =iIndex;
         }
         else if ( s_iBgImageCount > 2 )
         {
            while ( iIndex == s_iBgImageIndex )
            {
               iIndex = rand()%s_iBgImageCount;
            }
            s_iBgImageIndex = iIndex;
         }
         log_line("New image index: %d (id: %u), prev image index: %d (id: %u)", s_iBgImageIndex, s_idBgImage[s_iBgImageIndex], s_iBgImageIndexPrev, s_idBgImage[s_iBgImageIndexPrev]);
      }
   }

   int iImageId = s_idBgImage[s_iBgImageIndex];
   int iImageIdPrev = s_idBgImage[s_iBgImageIndexPrev];
   
   if ( isMenuOn() )
   {
      iImageId = s_idBgImageMenu[s_iBgImageIndex];
      iImageIdPrev = s_idBgImageMenu[s_iBgImageIndexPrev];
   }

   if ( (! g_bUpdateInProgress) && (! g_bSearching) && (g_TimeNow >= s_uTimeLastChangeBgImage) && (g_TimeNow < s_uTimeLastChangeBgImage + 3000) )
   {
      int iAlpha = ((s_uTimeLastChangeBgImage+3000) - g_TimeNow)/12;
      if ( iAlpha < 0 )
         iAlpha = 0;
      if ( iAlpha > 255 )
         iAlpha = 255;
      g_pRenderEngine->drawImage(0, 0, 1,1, iImageIdPrev);
      g_pRenderEngine->drawImageAlpha(0, 0, 1,1, iImageId, 255-iAlpha);
   }
   else
      g_pRenderEngine->drawImage(0, 0, 1,1, iImageId);

   double cc[4] = { 80,30,40,1.0 };

   g_pRenderEngine->setColors(cc);
   float width_text = g_pRenderEngine->textRawWidth(g_idFontMenuLarge, SYSTEM_NAME);
   char szBuff[256];
   getSystemVersionString(szBuff, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   g_pRenderEngine->drawText(0.91, 0.94, g_idFontMenuLarge, SYSTEM_NAME);
   g_pRenderEngine->drawText(0.915+width_text, 0.94, g_idFontMenuLarge, szBuff);

   bool bNoModel = false;

   if ( (NULL == g_pCurrentModel) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount()) ) ||
        (g_bFirstModelPairingDone && (0 == g_uActiveControllerModelVID) ) )
      bNoModel = true;

   g_pRenderEngine->setGlobalAlfa(1.0);
   //double c[4] = {0,0,0,1};
   //g_pRenderEngine->setColors(c);
   //sprintf(szBuff, "Welcome to %s", SYSTEM_NAME);
   //g_pRenderEngine->drawText(0.42, 0.2, g_idFontMenuLarge, szBuff);
   //g_pRenderEngine->drawText(0.42, 0.24, g_idFontMenuLarge, "Digital FPV System");

   float fXTextStart = 0.3;
   
   double c[4] = {40,40,40,1};
   g_pRenderEngine->setColors(c);
   sprintf(szBuff, "Welcome to %s Digital FPV System", SYSTEM_NAME);
   g_pRenderEngine->drawText(fXTextStart, 0.1, g_idFontMenuLarge, szBuff);

   double c2[4] = {0,0,0,1};
   g_pRenderEngine->setColors(c2);

   if ( s_StartSequence == START_SEQ_COMPLETED )
   if ( bNoModel )
   {
      if ( 0 == getControllerModelsCount() )
      {
         g_pRenderEngine->drawText(fXTextStart, 0.3, g_idFontMenuLarge, "Info: No vehicle defined!");
         g_pRenderEngine->drawText(fXTextStart, 0.34, g_idFontMenuLarge, "You have no vehicles linked to this controller.");
         g_pRenderEngine->drawText(fXTextStart, 0.37, g_idFontMenuLarge, "Press [Menu] key and then select 'Search' to search for a vehicle to connect to.");
      }
      else if ( ! g_bSearching )
      {
         g_pRenderEngine->drawText(fXTextStart, 0.3, g_idFontMenuLarge, "Info: No vehicle selected!");
         g_pRenderEngine->drawText(fXTextStart, 0.34, g_idFontMenuLarge, "You have no vehicle selected as active.");
         g_pRenderEngine->drawText(fXTextStart, 0.37, g_idFontMenuLarge, "Press [Menu] key and then select 'My Vehicles' to select the vehicle to connect to.");
      }
   }
}

// returns true if it rendered a background
bool _render_video_background()
{
   if ( g_bPlayIntro )
      return true;

   u32 uVehicleIdFullVideo = 0;
   u32 uVehicleSoftwareVersion = 0;
   bool bVehicleHasCamera = true;
   bool bDisplayingRelayedVideo = false;
   bool bCantDisplayVideo = false;

   if ( NULL != g_pCurrentModel )
   {
      uVehicleIdFullVideo = g_pCurrentModel->uVehicleId;
      uVehicleSoftwareVersion = g_pCurrentModel->sw_version;
      Model* pModel = relay_controller_get_relayed_vehicle_model(g_pCurrentModel);
      if ( NULL != pModel )
      if ( relay_controller_must_display_remote_video(g_pCurrentModel) )
      {
         uVehicleIdFullVideo = pModel->uVehicleId;
         uVehicleSoftwareVersion = pModel->sw_version;
         bDisplayingRelayedVideo = true;
      }
   }

   if ( 0 != uVehicleIdFullVideo )
   {
      bVehicleHasCamera = true;
      t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(uVehicleIdFullVideo);
      Model* pModel = findModelWithId(uVehicleIdFullVideo, 60);

      if ( pModel->is_spectator )      
      if ( (NULL != pRuntimeInfo) && pRuntimeInfo->bGotRubyTelemetryInfo )
      if ( pRuntimeInfo->headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA )
         pModel->iCameraCount = 1;

      if ( (NULL != pRuntimeInfo) && pRuntimeInfo->bGotRubyTelemetryInfo )
      if ( ! (pRuntimeInfo->headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA) )
         bVehicleHasCamera = false;

      if ( (NULL != pModel) && (pModel->iCameraCount <= 0) )
         bVehicleHasCamera = false;

      #if defined (HW_PLATFORM_RASPBERRY)
      if ( (NULL != pModel) && (pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
         bCantDisplayVideo = true;
      #endif

      if ( pModel->b_mustSyncFromVehicle )
         bVehicleHasCamera = true;
   }

   if ( 0 != uVehicleIdFullVideo )
   if ( (! bCantDisplayVideo) && (! g_bUpdateInProgress) )
   if ( bVehicleHasCamera && link_has_received_videostream(uVehicleIdFullVideo) )
   if ( (uVehicleSoftwareVersion >> 16) == SYSTEM_SW_BUILD_NUMBER )
      return false;

   if ( 0 != uVehicleIdFullVideo )
   if ( (! bCantDisplayVideo) && (! g_bUpdateInProgress) )
   if ( bVehicleHasCamera && link_has_received_videostream(uVehicleIdFullVideo) )
   if ( (uVehicleSoftwareVersion >> 16) >= 11503 )
   if ( (uVehicleSoftwareVersion >> 16) <= SYSTEM_SW_BUILD_NUMBER )
      return false;

   g_pRenderEngine->setGlobalAlfa(1.0);

   double c1[4] = {0,0,0,1};
   if ( (uVehicleSoftwareVersion >>16) < 11503 )
      c1[0] = 70;
   if ( (uVehicleSoftwareVersion >>16) > SYSTEM_SW_BUILD_NUMBER )
      c1[0] = 70;
   g_pRenderEngine->setColors(c1);
   g_pRenderEngine->drawRect(0, 0, 1,1 );

   double c[4] = {200,200,200,1};
   g_pRenderEngine->setColors(c);

   char szText[256];

   strcpy(szText, "Waiting for video feed");
   if ( bDisplayingRelayedVideo )
      strcpy(szText, "Waiting for video feed from relayed vehicle");

   if ( g_bFirstModelPairingDone )
   if ( ! bVehicleHasCamera )
   {
      strcpy(szText, "This vehicle has no cameras or video streams");
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      {
         Model* pModel = findModelWithId(uVehicleIdFullVideo, 61);
         if ( NULL != pModel )
            sprintf(szText, "%s has no cameras or video streams", pModel->getLongName());
      }
   }

   if ( bCantDisplayVideo )
      strcpy(szText, "Raspberry can't display H265 video");
   if ( bVehicleHasCamera )
   {
      static u32 sl_uLastTimeLogWaitVideo = 0;
      if ( g_TimeNow > sl_uLastTimeLogWaitVideo + 4000 )
      {
         sl_uLastTimeLogWaitVideo = g_TimeNow;
         log_line("Waiting for video feed from VID %u", uVehicleIdFullVideo);
      }
   }

   if ( ((uVehicleSoftwareVersion >> 16) < 11503) )
      strcpy(szText, L("Vehicle OTA software update required"));
   if ( (uVehicleSoftwareVersion >> 16) > SYSTEM_SW_BUILD_NUMBER )
      strcpy(szText, L("Controller software update required"));

   if ( g_bUpdateInProgress )
      strcpy(szText, "Update is in progress");

   float width_text = g_pRenderEngine->textRawWidth(g_idFontOSDBig, szText);
   g_pRenderEngine->drawText((1.0-width_text)*0.5, 0.45, g_idFontOSDBig, szText);
   g_pRenderEngine->drawText((1.0-width_text)*0.5, 0.45, g_idFontOSDBig, szText);

   if ( !g_bSearching )
   if ( ((uVehicleSoftwareVersion >>16) < 11503) || ((uVehicleSoftwareVersion>>16) > SYSTEM_SW_BUILD_NUMBER) )
   {
      float fHeight = 1.5*g_pRenderEngine->textHeight(g_idFontOSDBig);
      char szWarning[256];
      strcpy(szWarning, L("Video protocols have changed. You must update your vehicle"));
      if ( (uVehicleSoftwareVersion >>16) > SYSTEM_SW_BUILD_NUMBER )
         strcpy(szWarning, L("Video protocols have changed. You must update your controller"));
      width_text = g_pRenderEngine->textRawWidth(g_idFontOSDBig, szWarning);
      g_pRenderEngine->drawText((1.0-width_text)*0.5, 0.45+fHeight, g_idFontOSDBig, szWarning);
      g_pRenderEngine->drawText((1.0-width_text)*0.5, 0.45+fHeight, g_idFontOSDBig, szWarning);
   }
   return true;
}

void render_background_and_paddings(bool bForceBackground)
{
   if ( g_bPlayIntro && (! g_bPlayIntroWillEnd) )
   {
      if ( is_semaphore_signaled_clear(s_pSemaphoreVideoIntroWillFinish, SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH) )
      {
         log_line("Video intro playback will finish detected.");
         g_bPlayIntroWillEnd = true;
         if ( NULL != s_pSemaphoreVideoIntroWillFinish )
            sem_close(s_pSemaphoreVideoIntroWillFinish);
         s_pSemaphoreVideoIntroWillFinish = NULL;
         sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
      }
      if ( ! g_bPlayIntroWillEnd )
         return;
   }

   bool bShowBgPicture = false;
   bool bShowBgVideo = true;
   u32 uPairingStartBufferTime = 1000;

   if ( bForceBackground || g_bSearching || (! g_bIsRouterReady) || (! g_bFirstModelPairingDone) )
   {
      bShowBgPicture = true;
      bShowBgVideo = false;
   }

   if ( (! pairing_isStarted()) || (! link_has_received_main_vehicle_ruby_telemetry()) || (g_TimeNow < pairing_getStartTime() + uPairingStartBufferTime) )
   {
      bShowBgPicture = true;
      bShowBgVideo = false;
   }
   if ( NULL != g_pPopupLooking )
   {
      bShowBgPicture = true;
      bShowBgVideo = false;
   }

   if ( pairing_isStarted() && (g_TimeNow >= pairing_getStartTime() + uPairingStartBufferTime) )
   if ( (NULL != osd_get_current_data_source_vehicle_model()) && link_has_received_videostream(osd_get_current_data_source_vehicle_model()->uVehicleId) )
   {
      bShowBgPicture = false;
      bShowBgVideo = true;
   }

   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   #if defined (HW_PLATFORM_RASPBERRY)
   if ( (NULL != pActiveModel) && (pActiveModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
   {
      bShowBgPicture = false;
      bShowBgVideo = true;    
   }
   #endif

   if ( g_bUpdateInProgress )
      bShowBgVideo = true;
   if ( g_bPlayIntro && g_bPlayIntroWillEnd )
   {
      bShowBgPicture = true;
      bShowBgVideo = false;
   }

   if ( bShowBgPicture )
   {
      _draw_background_picture();
      return;
   }

   if ( NULL == g_pCurrentModel )
      return;

   u32 uVehicleSoftwareVersion = g_pCurrentModel->sw_version;
   Model* pModel = relay_controller_get_relayed_vehicle_model(g_pCurrentModel);
   if ( NULL != pModel )
   if ( relay_controller_must_display_remote_video(pModel) )
      uVehicleSoftwareVersion = pModel->sw_version;
   if ( (uVehicleSoftwareVersion >> 16) < 11503 )
      bShowBgVideo = true;
   if ( (uVehicleSoftwareVersion >> 16) > SYSTEM_SW_BUILD_NUMBER )
      bShowBgVideo = true;

   if ( bShowBgVideo )
   if ( _render_video_background() )
      return;
 
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);

   float fScreenAspect = (float)(g_pRenderEngine->getScreenWidth())/(float)(g_pRenderEngine->getScreenHeight());
   if ( (!g_bSearching) || g_bSearchFoundVehicle )
   if ( (NULL != g_pCurrentModel) && (!bForceBackground) )
   {
      double c[4] = {0,0,0,1};
      g_pRenderEngine->setGlobalAlfa(1.0);
      g_pRenderEngine->setColors(c);

      float fVideoAspect = (float)(g_pCurrentModel->video_params.iVideoWidth) / (float)(g_pCurrentModel->video_params.iVideoHeight);
      if ( fVideoAspect > fScreenAspect+0.01 )
      {
         int h = 1 + 0.5*(g_pRenderEngine->getScreenHeight() - g_pRenderEngine->getScreenWidth()/fVideoAspect);
         if ( h > 1 )
         {
            float fHBand = (float)h/(float)g_pRenderEngine->getScreenHeight();
            fHBand += g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->drawRect(0, 0, 1.0, fHBand);
            g_pRenderEngine->drawRect(0,1.0-fHBand, 1.0, fHBand);
         }
      }
      else if ( fVideoAspect < fScreenAspect-0.01 )
      {
         int w = 1 + 0.5*(g_pRenderEngine->getScreenWidth() - g_pRenderEngine->getScreenHeight()*fVideoAspect);
         if ( w > 1 )
         {
            float fWBand = (float)w/(float)g_pRenderEngine->getScreenWidth();
            fWBand += g_pRenderEngine->getPixelWidth();
            g_pRenderEngine->drawRect(0, 0, fWBand, 1.0);
            g_pRenderEngine->drawRect(1.0-fWBand, 0, fWBand, 1.0);
         }
      }
   }

   static float s_fTargetAdaptiveBandHeightPercent = 0.0;
   static float s_fCurrentAdaptiveBandHeightPercent = 0.0;
   static u32   s_uLastTimeAdaptiveOnLowestLevel = 0;
   bool bIsOnLow = false;
   if ( (NULL != pActiveModel) && (NULL != pVDS) )
   {
      if ( pVDS->bIsOnLowestAdaptiveLevel && (pActiveModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS) )
      {
         s_fTargetAdaptiveBandHeightPercent = 1.0;
         s_uLastTimeAdaptiveOnLowestLevel = g_TimeNow;
         bIsOnLow = true;
      }
      else if ( g_TimeNow > s_uLastTimeAdaptiveOnLowestLevel + 3000 )
      {
         s_fTargetAdaptiveBandHeightPercent = 0.0;
      }
   }
   else
      s_fTargetAdaptiveBandHeightPercent = 0.0;
   
   if ( s_fCurrentAdaptiveBandHeightPercent > 0.01 )
   if ( bIsOnLow || (fabs(s_fTargetAdaptiveBandHeightPercent - s_fCurrentAdaptiveBandHeightPercent) > 0.02) || (s_fTargetAdaptiveBandHeightPercent > 0.02) )
   {
      double c[4] = {0,0,0,1};
      g_pRenderEngine->setGlobalAlfa(1.0);
      g_pRenderEngine->setColors(c);
      float fHBand = 0.16;//(float)h/(float)g_pRenderEngine->getScreenHeight();
      fHBand *= s_fCurrentAdaptiveBandHeightPercent;
      g_pRenderEngine->drawRect(0, 0, 1.0, fHBand);
      g_pRenderEngine->drawRect(0,1.0-fHBand, 1.0, fHBand);
   }

   if ( fabs(s_fTargetAdaptiveBandHeightPercent - s_fCurrentAdaptiveBandHeightPercent) > 0.02 )
   {
      if ( s_fTargetAdaptiveBandHeightPercent > 0.9 )
         s_fCurrentAdaptiveBandHeightPercent = s_fCurrentAdaptiveBandHeightPercent + (s_fTargetAdaptiveBandHeightPercent - s_fCurrentAdaptiveBandHeightPercent)*0.2;
      else
         s_fCurrentAdaptiveBandHeightPercent = s_fCurrentAdaptiveBandHeightPercent + (s_fTargetAdaptiveBandHeightPercent - s_fCurrentAdaptiveBandHeightPercent)*0.1;
   }
}

void render_all_with_menus(u32 timeNow, bool bRenderMenus, bool bForceBackground, bool bDoInputLoop)
{
   Preferences* p = get_Preferences();

   if ( g_pControllerSettings->iFreezeOSD && s_bFreezeOSD )
      return;

   if ( g_bIsVideoPlaying )
   {
      video_playback_render();
      return;
   }

   u32 uTimeStart = get_current_timestamp_ms();

   g_pRenderEngine->startFrame();
   
   render_background_and_paddings(bForceBackground);
   
   if ( (!g_bSearching) || g_bSearchFoundVehicle )
   if ( ! bForceBackground )
   if ( (s_StartSequence == START_SEQ_COMPLETED) || (s_StartSequence == START_SEQ_FAILED) )
   {
      if ( pairing_isStarted() )
      if ( g_bIsRouterReady )
      if ( g_TimeNow >= g_RouterIsReadyTimestamp + 250 )
      if ( NULL == g_pPopupLooking )
      {
         u32 t = get_current_timestamp_micros();
         osd_render_all();
         t = get_current_timestamp_micros() - t;
         if ( t < 400000 )
            s_uMicroTimeOSDRender = (s_uMicroTimeOSDRender*5 + t)/6;
      }
      if ( g_bIsRouterReady )
         alarms_render();
   }

   bool bDevMode = false;
   if ( 0 != g_pControllerSettings->iDeveloperMode )
      bDevMode = true;
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
      bDevMode = true;

   if ( bDevMode )
   //if ( ! bForceBackground )
   if ( (! g_bToglleAllOSDOff) && (!g_bToglleStatsOff) )
   if ( ! p->iDebugShowFullRXStats )
   if ( 0 == g_pControllerSettings->iEnableDebugStats )
   //if ( g_bIsRouterReady )
   {
      float yPos = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.01*osd_getScaleOSD();
      float xPos = osd_getMarginX() + 0.02*osd_getScaleOSD();
      if ( NULL != g_pCurrentModel && osd_get_current_layout_index() >= 0 && osd_get_current_layout_index() < MODEL_MAX_OSD_SCREENS )
      if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      {
         xPos = osd_getMarginX() + osd_getVerticalBarWidth() + 0.01*osd_getScaleOSD();
         yPos = osd_getMarginY() + 0.01*osd_getScaleOSD();
      }
   
      osd_set_colors_text(get_Color_Dev());
      if ( (g_TimeNow/500) % 2 )
      {
         char szDbgText[32];
         strcpy(szDbgText, "[");
         if ( 0 != g_pControllerSettings->iDeveloperMode )
            strcat(szDbgText, "D");
         if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
            strcat(szDbgText, "+D");
         if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
         if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_STREAM_TIMINGS )
            strcat(szDbgText, "T");

         strcat(szDbgText, "]");

         float fHeight = g_pRenderEngine->textHeight(g_idFontOSDBig);
         float fWidthText = g_pRenderEngine->textWidth(g_idFontOSDBig, szDbgText);
         float fWidth = 0.8*fHeight/g_pRenderEngine->getAspectRatio() + fWidthText;
         g_pRenderEngine->setFill(0,0,0,0.5);
         g_pRenderEngine->setStroke(0,0,0,0);
         bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
         g_pRenderEngine->disableAlphaBlending();
         g_pRenderEngine->drawRect(xPos, yPos, fWidth, fHeight*1.6);
         osd_set_colors_text(get_Color_Dev());
         osd_show_value( xPos+(fWidth-fWidthText)*0.5, yPos+fHeight*0.2, szDbgText, g_idFontOSDBig );
         g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
      }
      osd_set_colors();
   }

   u32 t = get_current_timestamp_micros();
   u32 uTimePopups = 0;
   popups_render_bottom();
   popups_render();
   t = get_current_timestamp_micros() - t;
   if ( t < 400000 )
      uTimePopups += t;

   if ( bRenderMenus )
   {
      t = get_current_timestamp_micros();
      menu_render();
      t = get_current_timestamp_micros() - t;
      if ( t < 400000 )
         s_uMicroTimeMenuRender = (s_uMicroTimeMenuRender*5 + t)/6;
   }

   t = get_current_timestamp_micros();
   popups_render_topmost();
   t = get_current_timestamp_micros() - t;
   if ( t < 400000 )
   {
      uTimePopups += t;
      s_uMicroTimePopupRender = (s_uMicroTimePopupRender*5 + uTimePopups)/6;
   }

   if ( bDevMode )
   if ( p->iShowCPULoad )
   {
      char szBuff[64];
      float yPos = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.01*osd_getScaleOSD();
      float xPos = osd_getMarginX() + 0.02*osd_getScaleOSD();
      if ( NULL != g_pCurrentModel && osd_get_current_layout_index() >= 0 && osd_get_current_layout_index() < MODEL_MAX_OSD_SCREENS )
      if ( g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      {
         xPos = osd_getMarginX() + osd_getVerticalBarWidth() + 0.01*osd_getScaleOSD();
         yPos = osd_getMarginY() + 0.01*osd_getScaleOSD();
      }
   
      osd_set_colors();
      g_pRenderEngine->setFill(0,0,0,0.5);
      g_pRenderEngine->setStroke(0,0,0,0);
      bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
      g_pRenderEngine->disableAlphaBlending();
      g_pRenderEngine->drawRect(xPos, yPos-0.003, 0.74, 0.05);

      osd_set_colors_text(get_Color_Dev());      

      xPos += 0.02*osd_getScaleOSD();
      yPos += 0.003;
      sprintf(szBuff, "UI FPS: %d", s_iRubyFPS);
      xPos += osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

      xPos += 0.02*osd_getScaleOSD();
      sprintf(szBuff, "Menu: %.1f ms/frame", (float)s_uMicroTimeMenuRender/1000.0);
      xPos += osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

      xPos += 0.02*osd_getScaleOSD();
      sprintf(szBuff, "Popup: %.1f ms/fr", (float)s_uMicroTimePopupRender/1000.0);
      xPos += osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

      xPos += 0.02*osd_getScaleOSD();
      sprintf(szBuff, "OSD: %.1f ms/fr", s_uMicroTimeOSDRender/1000.0);
      xPos += osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

      xPos += 0.02*osd_getScaleOSD();
      sprintf(szBuff, "OSD: %d ms/sec", (int)(s_uMicroTimeOSDRender*s_iRubyFPS/1000.0));
      osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

      g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
   }
  
   if ( handle_commands_is_command_in_progress() )
      render_commands();   

   if ( s_bShowMira )
   {
      static u32 s_idImageCalibrateHDMI2 = g_pRenderEngine->loadImage("res/calibrate_hdmi.png");
      if ( 0 == s_idImageCalibrateHDMI2 )
         log_softerror_and_alarm("Failed to load tv mira image for HDMI calibration.");
      else
         log_line("Loaded image for TV mira for HDMI calibration.");

      if ( 0 != s_idImageCalibrateHDMI2 )
         g_pRenderEngine->drawImage(0,0, 1,1, s_idImageCalibrateHDMI2);

      osd_set_colors_text(get_Color_Dev());
      g_pRenderEngine->drawText(0.05, 0.05, g_idFontMenuLarge, "Press [Back]/[Cancel] to close it.");
   }

   if ( NULL != p && p->iOSDFlipVertical )
      g_pRenderEngine->rotate180();

   g_pRenderEngine->endFrame();

   g_TimeNow = get_current_timestamp_ms();
   s_uTimeLastRenderDuration = g_TimeNow - uTimeStart;

   s_iFPSCount++;
   if ( g_TimeNow >= s_uFPSLastTimeCheck + 1000 )
   {
      s_iRubyFPS = (s_iFPSCount * (g_TimeNow - s_uFPSLastTimeCheck))/1000;
      s_iFPSCount = 0;
      s_uFPSLastTimeCheck = g_TimeNow;
   }
}

void render_all(u32 timeNow, bool bForceBackground, bool bDoInputLoop)
{
   render_all_with_menus(timeNow, true, bForceBackground, bDoInputLoop);
}

static bool s_bThreadCPUStateRunning = false;

void* _thread_check_controller_cpu_state(void *argument)
{
   log_line("Started thread to check CPU state...");
   s_bThreadCPUStateRunning = true;
   if ( g_pControllerSettings->iPrioritiesAdjustment )
      hw_set_current_thread_raw_priority("central check cpu state", g_pControllerSettings->iThreadPriorityOthers);
   hw_log_current_thread_attributes("central check cpu");

   u32 uTimeLastCheck = 0;
   while ( ! g_bQuit )
   {
      u32 uTime = get_current_timestamp_ms();
      if ( uTime < uTimeLastCheck + 10000 )
      {
         for( int i=0; i<10; i++ )
         {
            hardware_sleep_ms(900);
            if ( g_bQuit )
               break;
         }
         continue;
      }
      if ( g_bQuit )
         break;
      uTimeLastCheck = uTime;
      g_uControllerCPUFlags = hardware_get_flags();
      g_iControllerCPUSpeedMhz = hardware_get_cpu_speed();
      g_iControllerCPUTemp = hardware_get_cpu_temp();
   }
   log_line("Finished thread to check CPU state.");
   s_bThreadCPUStateRunning = false;
   return NULL;
}

void compute_cpu_state()
{
   static u32 s_TimeLastCPUComputeLoad = 0;

   if ( g_TimeNow > s_TimeLastCPUComputeLoad + 1000 )
   {
      s_TimeLastCPUComputeLoad = g_TimeNow;

      FILE* fd = NULL;
      static unsigned long long valgcpu[4] = {0,0,0,0};
      unsigned long long tmp[4];
      unsigned long long total;

      fd = fopen("/proc/stat", "r");
      if ( NULL != fd )
      {
         fscanf(fd, "%*s %llu %llu %llu %llu", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
         fclose(fd);
         fd = NULL;
      }
      else
      {
          tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
      }
      
      if ( tmp[0] < valgcpu[0] || tmp[1] < valgcpu[1] || tmp[2] < valgcpu[2] || tmp[3] < valgcpu[3] )
            g_iControllerCPULoad = 0;
      else
      {
         total = (tmp[0] - valgcpu[0]) + (tmp[1] - valgcpu[1]) + (tmp[2] - valgcpu[2]);
         if ( (total + (tmp[3] - valgcpu[3])) != 0 )
            g_iControllerCPULoad = (total * 100) / (total + (tmp[3] - valgcpu[3]));
      }
         
      valgcpu[0] = tmp[0]; valgcpu[1] = tmp[1]; valgcpu[2] = tmp[2]; valgcpu[3] = tmp[3]; 
   }

   static bool s_bThreadCheckCPUCreated = false;
   static u32 s_TimeLastCPUComputeState = 0;
   if ( ! s_bThreadCheckCPUCreated )
   if ( g_TimeNow > s_TimeLastCPUComputeState + 10000 )
   {
      s_TimeLastCPUComputeState = g_TimeNow;
      s_bThreadCheckCPUCreated = true;
      pthread_t pth;
      pthread_attr_t attr;
      hw_init_worker_thread_attrs(&attr, (g_pControllerSettings->iCoresAdjustment?CORE_AFFINITY_OTHERS:-1), -1, SCHED_OTHER, 0, "central check cpu state");
      pthread_create(&pth, &attr, &_thread_check_controller_cpu_state, NULL);
      pthread_attr_destroy(&attr);
   }
}

int ruby_start_recording()
{
   if ( (! g_bIsRouterReady) || (NULL == g_pCurrentModel) || (! pairing_isStarted()) )
       return -1;

   if ( g_bIsVideoProcessing )
   {
      Popup* p = new Popup("Please wait for the current video file processing to complete.", 0.1,0.7, 0.54, 5);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return -1;
   }

   if ( g_bIsVideoRecording )
       return -1;

   #ifdef HW_PLATFORM_RASPBERRY
   system("sudo mount -o remount,rw /");
   #endif

   hw_execute_bash_command("mkdir -p tmp", NULL );
   hw_execute_bash_command("chmod 777 tmp", NULL );

   char szComm[512];

   if ( access( FOLDER_MEDIA, R_OK ) == -1 )
   {
      sprintf(szComm, "mkdir -p %s",FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL );
      sprintf(szComm, "chmod 777 %s",FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL );

      sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL);

      if ( access( FOLDER_MEDIA, R_OK ) == -1 )
      {
         Popup* p = new Popup("There is an issue writing to the SD card.", 0.1,0.7, 0.54, 5);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         return -1;
      }
   }

   sprintf(szComm, "df -m %s | tail -n 1", FOLDER_MEDIA);
   char szBuff[2048];
   if ( 1 == hw_execute_bash_command_raw(szComm, szBuff) )
   {
      char szTemp[1024];
      long lb, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
      if ( lf < 200 )
      {
         sprintf(szTemp, "You don't have enough free space on the SD card to start recording (%d Mb free). Move your media files to USB memory stick.", (int)lf);
         warnings_add(0, szTemp, g_idIconCamera, get_Color_IconError(), 6);
         return -1;
      }
      if ( lf < 1000 )
      {
         sprintf(szTemp, "You are running low on storage space (%d Mb free). Move your media files to USB memory stick.", (int)lf);
         warnings_add(0, szTemp, g_idIconCamera, get_Color_IconWarning(), 6);
         log_line("Warning: Free storage space is only %d Mb. Video recording might stop", (int)lf);
      }
   }

   u8 uCmd = 1;
   send_control_message_to_router_and_data(PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING, &uCmd, 1);
   log_line("Sent message to router to start recording.");
   return 0;
}

int ruby_stop_recording()
{
   if ( (! g_bIsRouterReady) || (NULL == g_pCurrentModel) || (! pairing_isStarted()) )
       return -1;

   if ( ! g_bIsVideoRecording )
       return -1;

   u8 uCmd = 0;
   send_control_message_to_router_and_data(PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING, &uCmd, 1);
   log_line("Sent message to router to stop recording.");
   return 0;
}

bool ruby_is_recording()
{
   return g_bIsVideoRecording;
}

int exectuteActionsInputDebugStats()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return 0;
   if ( g_bIsReinit || g_bSearching )
      return 0;
   if ( (!pairing_isStarted()) || (! g_bIsRouterReady) )
      return 0;

   int iRet = 0;

   if ( p->iDebugStatsQAButton == 1 )
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1 )
   {
      g_pControllerSettings->iEnableDebugStats = 1 - g_pControllerSettings->iEnableDebugStats;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      iRet = 1;
   }

   if ( p->iDebugStatsQAButton == 2 )
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2 )
   {
      g_pControllerSettings->iEnableDebugStats = 1 - g_pControllerSettings->iEnableDebugStats;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      iRet = 1;
   }

   if ( p->iDebugStatsQAButton == 3 )
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3 )
   {
      g_pControllerSettings->iEnableDebugStats = 1 - g_pControllerSettings->iEnableDebugStats;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      iRet = 1;
   }

   if ( (keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_BACK) )
   if ( ! isMenuOn() )
   {
      g_pControllerSettings->iEnableDebugStats = 0;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      iRet = 1;
   }

   if ( g_pControllerSettings->iEnableDebugStats )
   if ( ! isMenuOn() )
   if ( ((p->iDebugStatsQAButton != 1) && (keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1)) ||
        ((p->iDebugStatsQAButton != 2) && (keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2)) ||
        ((p->iDebugStatsQAButton != 3) && (keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3)) )
   {
      osd_debug_stats_toggle_freeze();
      iRet = 1;
   }

   if ( g_pControllerSettings->iEnableDebugStats )
   if ( ! isMenuOn() )
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_PLUS )
   {
      osd_debug_stats_toggle_zoom(true);
      iRet = 1;
   }

   if ( g_pControllerSettings->iEnableDebugStats )
   if ( ! isMenuOn() )
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MINUS )
   {
      osd_debug_stats_toggle_zoom(false);
      iRet = 1;
   }

   return iRet;
}

void executeQuickActions()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return;
   if ( g_bIsReinit || g_bSearching )
      return;
   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup("You must be connected to a vehicle to execute Quick Actions.", 0.1,0.8, 0.54, 5);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   log_force_full_log();

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1  )
      log_line("Pressed button QA1");
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2  )
      log_line("Pressed button QA2");
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3  )
      log_line("Pressed button QA3");

   log_line("Current assigned QA actions: button1: %d, button2: %d, button3: %d",
    p->iActionQuickButton1,p->iActionQuickButton2,p->iActionQuickButton3);
   
   log_regular_mode();
   
   if ( (!pairing_isStarted()) || (! g_bIsRouterReady) )
   {
      warnings_add(0, "Please connect to a vehicle first, to execute Quick Actions.");
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionCycleOSD == p->iActionQuickButton1) || 
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionCycleOSD == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionCycleOSD == p->iActionQuickButton3) )
   {
      executeQuickActionCycleOSD();
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionOSDSize == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionOSDSize == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionOSDSize == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("change OSD size") )
         return;
      p->iScaleOSD++;
      if ( p->iScaleOSD > 3 )
         p->iScaleOSD = -1;
      save_Preferences();
      osd_apply_preferences();
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionTakePicture == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionTakePicture == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionTakePicture == p->iActionQuickButton3) )
   {
      executeQuickActionTakePicture();
      return;
   }
         
   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && (quickActionPITMode == p->iActionQuickButton1)) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && (quickActionPITMode == p->iActionQuickButton2)) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && (quickActionPITMode == p->iActionQuickButton3)) )
   {
      executeQuickActionSwitchPITMode();
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionToggleOSD == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionToggleOSD == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionToggleOSD == p->iActionQuickButton3) )
   {

      if ( ! quickActionCheckVehicle("toggle the OSD") )
         return;
      g_bToglleOSDOff = ! g_bToglleOSDOff;
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionToggleStats == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionToggleStats == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionToggleStats == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("toggle the statistics") )
         return;
      g_bToglleStatsOff = ! g_bToglleStatsOff;
      return;
   }
         
   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionToggleAllOff == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionToggleAllOff == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionToggleAllOff == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("toggle all info on/off") )
         return;
      g_bToglleAllOSDOff = ! g_bToglleAllOSDOff;
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionOSDFreeze == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionOSDFreeze == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionOSDFreeze == p->iActionQuickButton3) )
   {
      g_bFreezeOSD = ! g_bFreezeOSD;
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionRelaySwitch == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionRelaySwitch == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionRelaySwitch == p->iActionQuickButton3) )
   {
      executeQuickActionRelaySwitch();
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionVideoRecord == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionVideoRecord == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionVideoRecord == p->iActionQuickButton3) )
   {
      executeQuickActionRecord();
      return;
   }

   #ifdef FEATURE_ENABLE_RC
   if ( NULL != g_pCurrentModel )
   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionRCEnable == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionRCEnable == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionRCEnable == p->iActionQuickButton3) )
   {
      if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
      {
         warnings_add(0, "Can't enable RC while in spectator mode.");
         return;
      }
      if ( ! quickActionCheckVehicle("enable/disable the RC link output") )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

      if ( params.flags & RC_FLAGS_OUTPUT_ENABLED )
         params.flags &= (~RC_FLAGS_OUTPUT_ENABLED);
      else
         params.flags |= RC_FLAGS_OUTPUT_ENABLED;
      handle_commands_abandon_command();
      handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t));
      return;
   }
   #endif

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionCameraProfileSwitch == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionCameraProfileSwitch == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionCameraProfileSwitch == p->iActionQuickButton3) )
   {
      if ( g_pCurrentModel->is_spectator )
      {
         warnings_add(0, "Can't switch camera profile for spectator vehicles.");
         return;
      }
      if ( handle_commands_is_command_in_progress() )
      {
         return;
      }

      int iProfileOrg = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
      int iProfile = iProfileOrg;
      camera_profile_parameters_t* pProfile1 = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);
      iProfile++;
      if ( iProfile >= MODEL_CAMERA_PROFILES-1 )
         iProfile = 0;

      camera_profile_parameters_t* pProfile2 = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);
      
      //char szBuff[64];
      //sprintf(szBuff, "Switching to camera profile %s", model_getCameraProfileName(iProfile));
      //warnings_add(g_pCurrentModel->uVehicleId, szBuff);

      g_pCurrentModel->log_camera_profiles_differences(pProfile1, pProfile2, iProfileOrg, iProfile);

      handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PROFILE, iProfile, NULL, 0);
      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionRotaryFunction == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionRotaryFunction == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionRotaryFunction == p->iActionQuickButton3) )
   {
      g_pControllerSettings->nRotaryEncoderFunction++;
      if ( g_pControllerSettings->nRotaryEncoderFunction > 2 )
         g_pControllerSettings->nRotaryEncoderFunction = 1;
      save_ControllerSettings();
      if ( 0 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: None");
      if ( 1 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: Menu Navigation");
      if ( 2 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: Camera Adjustment");

      return;
   }

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionSwitchFavorite == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionSwitchFavorite == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionSwitchFavorite == p->iActionQuickButton3) )
   {
      executeQuickActionSwitchFavoriteVehicle();
      return;
   }
}


void r_check_processes_filesystem()
{
   char szFilesMissing[1024];
   szFilesMissing[0] = 0;
   bool failed = false;
   if( access( "ruby_rt_station", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rt_station"); }
   if( access( "ruby_rx_telemetry", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rx_telemetry"); }
   if( access( "ruby_video_proc", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_video_proc"); }
   if( access( VIDEO_PLAYER_SM, R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " "); strcat(szFilesMissing, VIDEO_PLAYER_SM); }
   if( access( VIDEO_PLAYER_OFFLINE, R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " "); strcat(szFilesMissing, VIDEO_PLAYER_OFFLINE); }
     
   log_line("Checked proccesses. Result: %s", failed?"failed":"all ok");

   if ( failed )
   {
      log_error_and_alarm("Some Ruby binaries are missing: [%s].", szFilesMissing);

      Popup* p = new Popup("Some processes are missing from the SD card!", 0.2, 0.65, 0.6, 0);
      p->setCentered();
      p->setIconId(g_idIconError,get_Color_IconError());
      popups_add(p);
   }
}

void ruby_load_models()
{
   log_line("Loading models...");
   loadAllModels();
   g_pCurrentModel = getCurrentModel();
   log_line("Loaded models complete.");
   if ( NULL != g_pCurrentModel )
      log_line("Current model must synch settings from vehicle? %s", g_pCurrentModel->b_mustSyncFromVehicle?"yes":"no");
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_ACTIVE_CONTROLLER_MODEL);
   if ( access(szFile, R_OK) == -1 )
   {
      if ( g_bFirstModelPairingDone )
      if ( (0 != getControllerModelsCount()) || ( 0 != getControllerModelsSpectatorCount()) )
      if ( NULL != g_pCurrentModel )
          g_uActiveControllerModelVID = g_pCurrentModel->uVehicleId;

      if ( ! controllerHasModelWithId(g_uActiveControllerModelVID) )
         g_uActiveControllerModelVID = 0;

      // Recreate active model file
      ruby_set_active_model_id(g_uActiveControllerModelVID);
   }
   FILE* fd = fopen(szFile, "rb");
   if ( NULL != fd )
   {
      fscanf(fd, "%u", &g_uActiveControllerModelVID);
      fclose(fd);
      log_line("Controller current active model id is: %u", g_uActiveControllerModelVID);
   }
   else
      log_softerror_and_alarm("Can't access active model id file (%s)", szFile);

   if ( ! controllerHasModelWithId(g_uActiveControllerModelVID) )
   {
      log_line("Controller does not have a model for current active controller model id %u. Reset active model id to 0.", g_uActiveControllerModelVID);
      ruby_set_active_model_id(0);
   }
   if ( g_bFirstModelPairingDone )
   {
      if ( (0 == getControllerModelsCount()) && ( 0 == getControllerModelsSpectatorCount()) )
      {
         log_line("No controller or spectator models and first pairing was done. No active model to use.");
         ruby_set_active_model_id(0);
         return;
      }
   }

   log_line("Current model is in %s mode", g_pCurrentModel->is_spectator?"spectator":"controller");
   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[2]));
   
   log_line("Current model radio links: %d, 1st radio link frequency: %s, 2nd radio link: %s, 3rd radio link: %s",
      g_pCurrentModel->radioLinksParams.links_count, 
      szFreq1, szFreq2, szFreq3);
}


bool ruby_central_has_sdcard_update(bool bDoUpdateToo)
{
   // Disable MMC update from SD card
   return false;

   if ( ! hardware_is_running_on_runcam_vrx() )
      return false;
   char szOutput[2048];
   hw_execute_bash_command_raw("lsblk -l | grep mmcblk1p3", szOutput);
   removeTrailingNewLines(szOutput);
   log_line("[SDCard] Output of lsblk: [%s]", szOutput);
   if ( (strlen(szOutput) < 10) || (NULL == strstr(szOutput, "mmcblk1p3")) )
      return false;

   // Already mounted? It's native SD card boot
   if ( NULL != strstr(szOutput, "/") )
   {
      log_line("[SDCard] SD card already mounted as main Ruby instalation.");
      return false;
   }

   // Try mount and look for Ruby files
   bool bHasRuby = false;
   hw_execute_bash_command("mkdir -p /mnt/tmp-ruby", NULL);
   hw_execute_bash_command("mount /dev/mmcblk1p3 /mnt/tmp-ruby", NULL);
   if ( access("/mnt/tmp-ruby/home/radxa/ruby/ruby_start", R_OK) == -1 )
   {
      log_line("[SDCard] ruby_start not found on SD card mount point.");
      bHasRuby = false;
   }
   else
   {
      log_line("[SDCard] ruby_start found on SD card mount point.");
      bHasRuby = true;
   }

   if ( bHasRuby )
   {
      szOutput[0] = 0;
      hw_execute_bash_command_raw("/mnt/tmp-ruby/home/radxa/ruby/ruby_start -ver 2>&1", szOutput);
      log_line("[SDCard] Ruby version on the SD card: [%s]", szOutput);
      for( int i=0; i<(int)strlen(szOutput); i++ )
      {
         if ( szOutput[i] == '.' )
            szOutput[i] = ' ';
      }
      int iMajor = 0;
      int iMinor = 0;
      log_line("[SDCard] Ruby version on the SD card formated: [%s]", szOutput);
      if ( 2 != sscanf(szOutput, "%d %d", &iMajor, &iMinor) )
         bHasRuby = false;
      if ( bHasRuby )
      {
         log_line("[SDCard] Ruby version on the SD card parsed: %d.%d", iMajor, iMinor);
         log_line("[SDCard] Ruby version running now: %d.%d", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
         
         if ( (iMajor < SYSTEM_SW_VERSION_MAJOR) ||
              ((iMajor == SYSTEM_SW_VERSION_MAJOR) && (iMinor <= SYSTEM_SW_VERSION_MINOR)) )
         {
            log_line("[SDCard] SD card ruby binaries are older than current instalation.");
            bHasRuby = false;
         }
      }
   }

   if ( bHasRuby )
      log_line("[SDCard] Newer Ruby binaries found on SD card.");

   if ( ! bDoUpdateToo )
   {
      log_line("[SDCard] No update asked for. Unmount.");
      hw_execute_bash_command("umount /mnt/tmp-ruby", NULL);
      hw_execute_bash_command("rm -rf /mnt/tmp-ruby", NULL);
      return bHasRuby;
   }

   log_line("[SDCard] Updating Ruby binaries from SD card...");
   hw_execute_bash_command("cp -rf /mnt/tmp-ruby/home/radxa/ruby/ruby_* /home/radxa/ruby/", NULL);
   hw_execute_bash_command("cp -rf /mnt/tmp-ruby/home/radxa/ruby/res/* /home/radxa/ruby/res/", NULL);
   hw_execute_bash_command("cp -rf /mnt/tmp-ruby/home/radxa/ruby/updates/* /home/radxa/ruby/updates/", NULL);
   sync();
   hardware_sleep_ms(500);
   log_line("[SDCard] Done updating Ruby binaries from SD card.");
   hw_execute_bash_command("umount /mnt/tmp-ruby", NULL);
   hw_execute_bash_command("rm -rf /mnt/tmp-ruby", NULL);
   return bHasRuby;
}


void* _thread_video_recording_processing(void *argument)
{
   log_line("[VideoRecording-Th] Thread to process record started.");
   hw_log_current_thread_attributes("video processing recording");

   hardware_sleep_ms(900);

   char szComm[1024];
   char szPIDs[1024];
   while ( true )
   {
      bool bProcRunning = false;
      hw_process_get_pids("ruby_video_proc", szPIDs);
      removeTrailingNewLines(szPIDs);
      if ( strlen(szPIDs) > 2 )
         bProcRunning = true;

      if ( bProcRunning )
      {
         hardware_sleep_ms(100);
         continue;
      }
      break;
   }
      
   log_line("[VideoRecording-Th] Video processing process finished.");

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
   if ( access(szFile, R_OK) == -1 )
      warnings_add(0, L("Completed processing video recording file"), g_idIconCamera, get_Color_IconNormal());
   else
   {
      warnings_add(0, L("Processing video recording file failed"), g_idIconCamera, get_Color_IconError());

      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      FILE* fd = fopen(szFile, "r");

      while ( (NULL != fd) && ((read = getline(&line, &len, fd)) != -1))
      {
        if ( read > 0 )
           warnings_add(0, L(line), g_idIconCamera, get_Color_IconError());
      }
      if ( NULL != fd )
         fclose(fd);
   }

   sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
   hw_execute_bash_command(szComm, NULL );

   sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
   hw_execute_bash_command(szComm, NULL );

   sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
   hw_execute_bash_command(szComm, NULL );
  
   g_bIsVideoProcessing = false;
   g_bIsVideoRecording = false;
   g_uVideoRecordingStartTime = 0;
   log_line("[VideoRecording-Th] Exit recording thread.");
   return NULL;
}

void ruby_central_show_mira(bool bShow)
{
   s_bShowMira = bShow;
}

bool ruby_central_is_showing_mira()
{
   return s_bShowMira;
}

void start_loop()
{
   hardware_sleep_ms(START_SEQ_DELAY);
   log_line("Executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));

   if ( s_StartSequence == START_SEQ_PRE_LOAD_CONFIG )
   {
      log_line("Loading configuration...");
      popupStartup.setTitle(SYSTEM_NAME);
      if ( g_bIsReinit )
         popupStartup.addLine("Restarting...");
      
      popupStartup.addLine(L("Loading configuration..."));
      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_LOAD_CONFIG;
      return;
   }
   if ( s_StartSequence == START_SEQ_LOAD_CONFIG )
   {
      osd_apply_preferences();

      load_PluginsSettings();
      load_CorePluginsSettings();
      load_CorePlugins(1);
      if ( access("/sys/class/net/usb0", R_OK ) == -1 )
      {
         char szBuff[256];
         sprintf(szBuff, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_USB_TETHERING_DEVICE);
         hw_execute_bash_command(szBuff, NULL);
      }

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_PRE_SEARCH_DEVICES;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SEARCH_DEVICES )
   {
      popupStartup.addLine(L("Searching for external devices..."));
      s_StartSequence = START_SEQ_SEARCH_DEVICES;
      return;
   }

   if ( s_StartSequence == START_SEQ_SEARCH_DEVICES )
   {
      hardware_i2c_check_and_update_device_settings();
      ruby_signal_alive();
      controller_stop_i2c();
      controller_start_i2c();
      ruby_signal_alive();
      hardware_sleep_ms(200);

      s_StartSequence = START_SEQ_PRE_SEARCH_INTERFACES;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SEARCH_INTERFACES )
   {
      log_line("Enumerating controller input interfaces...");
      popupStartup.addLine(L("Enumerating controller input interfaces..."));
      s_StartSequence = START_SEQ_SEARCH_INTERFACES;
      return;
   }

   if ( s_StartSequence == START_SEQ_SEARCH_INTERFACES )
   {
      controllerInterfacesEnumJoysticks();
      s_StartSequence = START_SEQ_PRE_NICS;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_NICS )
   {
      log_line("Getting radio hardware info...");
      popupStartup.addLine(L("Getting radio hardware info..."));
      hardware_enumerate_radio_interfaces();

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_NICS;
      return;
   }


   if ( s_StartSequence == START_SEQ_NICS )
   {
     int iCountSupported = 0;
     int iCountUnsupported = 0;
     if ( 0 == hardware_get_radio_interfaces_count() )
     {
         Popup* p = new Popup("No radio interfaces detected on your controller.",0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
     }
     else
     {
        bool allDisabled = true;
        for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
        {
           radio_hw_info_t* pNIC = hardware_get_radio_info(i);
           if ( pNIC->isSupported )
              iCountSupported++;
           else
              iCountUnsupported++;
           u32 flags = controllerGetCardFlags(pNIC->szMAC);
           if ( ! ( flags & RADIO_HW_CAPABILITY_FLAG_DISABLED ) )
              allDisabled = false;
        }
        if ( allDisabled )
        {
           Popup* p = new Popup("All radio interfaces are disabled on the controller.", 0.3,0.4, 0.5, 6);
           p->setIconId(g_idIconError, get_Color_IconError());
           p->addLine("Go to [Menu]->[Controller] -> [Radio Configuration] and enable at least one radio interface.");
           popups_add_topmost(p);
           log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
           s_StartSequence = START_SEQ_PRE_SYNC_DATA;
           return;
        }

        if ( iCountUnsupported == hardware_get_radio_interfaces_count() )
        {
            Popup* p = new Popup("No radio interface on your controller is fully supported.", 0.3,0.4, 0.5, 6);
            p->setIconId(g_idIconError, get_Color_IconError());
            popups_add_topmost(p);
        }
        else if ( iCountUnsupported > 0 )
        {
            Popup* p = new Popup("Some radio interfaces on your controller are not fully supported.", 0.3,0.4, 0.5, 6);
            p->setIconId(g_idIconWarning, get_Color_IconWarning());
            popups_add_topmost(p);
        }

        int iNewCardIndex = controllerAddedNewRadioInterfaces();
        if ( iNewCardIndex >= 0 )
        {
           save_ControllerInterfacesSettings();
           if ( g_bFirstModelPairingDone )
           {
              MenuInfoBooster* pMenu = new MenuInfoBooster();
              if ( NULL != pMenu )
              {
                 pMenu->m_iRadioCardIndex = iNewCardIndex;
                 add_menu_to_stack(pMenu);
              }
           }
        }

        // Check & update radio cards models
        bool bChanged = false;
        ControllerInterfacesSettings* pCIS = get_ControllerInterfacesSettings();
        for( int i=0; i<pCIS->radioInterfacesCount; i++ )
        {
            // If user defined, do not change it
            if ( pCIS->listRadioInterfaces[i].cardModel < 0 )
               continue;
            radio_hw_info_t* pNICInfo = hardware_get_radio_info_from_mac(pCIS->listRadioInterfaces[i].szMAC);
            if ( NULL != pNICInfo )
            if ( pCIS->listRadioInterfaces[i].cardModel != pNICInfo->iCardModel )
            {
               pCIS->listRadioInterfaces[i].cardModel = pNICInfo->iCardModel;
               bChanged = true;
            }
        }
        if ( bChanged )
          save_ControllerInterfacesSettings();
     }
     //char szBuff[256];
     //sprintf(szBuff, "(%d of %d supported radio interfaces)", iCountSupported, iCountSupported + iCountUnsupported);
     //popupStartup.addLine(szBuff);
     log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
     s_StartSequence = START_SEQ_PRE_SYNC_DATA;
     return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SYNC_DATA )
   {
      Preferences* pP = get_Preferences();
      if ( 1 == pP->iAutoExportSettings )
      {
         popupStartup.addLine("Synchronizing data...");
         s_StartSequence = START_SEQ_SYNC_DATA;
         return;
      }

      s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_SYNC_DATA )
   {
      log_line("Auto export settings is enabled. Checking boot count...");

      if ( g_iBootCount < 3 )
      {
         log_line("First or second boot (%d), skipping auto export", g_iBootCount);
         popupStartup.addLine("Synchronizing data skipped.");
         log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
         s_StartSequence = START_SEQ_PRE_LOAD_DATA;
         return;
      }

      log_line("Doing auto export...");
      int nResult = controller_utils_export_all_to_usb();
      if ( nResult != 0 )
      {
            if ( nResult == -2 )
            {
               log_line("Failed to synchonize data to USB memory stick, no memory stick.");
               popupStartup.addLine("Synchronizing data skipped. No USB memory stick.");
            }
            else if ( nResult == -10 || nResult == -11 )
            {
               log_softerror_and_alarm("Failed to synchonize data to USB memory stick, failed to write data to USB stick.");
               popupStartup.addLine("Synchronizing data failed to write to USB memory stick.");
            }
            else
            {
               log_softerror_and_alarm("Failed to synchonize data to USB memory stick.");
               popupStartup.addLine("Synchronizing data failed.");
            }
            s_StartSequence = START_SEQ_SYNC_DATA_FAILED;
            log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
            return;
      }
      log_line("Auto export done.");

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      popupStartup.addLine("Synchronizing data complete.");
      s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_SYNC_DATA_FAILED )
   {
      static int s_iSyncFailedLoop = 0;
      s_iSyncFailedLoop++;
      if ( s_iSyncFailedLoop > 5 )
      {
         log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
         s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      }
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_LOAD_DATA )
   {
      log_line("Start sequence: PRE_LOAD_DATA");
      popupStartup.addLine(L("Loading models..."));
      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_LOAD_DATA )
   {
      log_line("Start sequence: LOAD_DATA");

      load_favorites();
      // Check file system for write access

      log_line("Checking the file system for write access...");
      bool bWriteFailed = false;

      if ( check_write_filesystem() < 0 )
      {
         alarms_add_from_local(ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR, 0, 0);
         bWriteFailed = true;
      }
               
      if ( bWriteFailed )
         log_line("Checking the file system for write access: Failed.");
      else
         log_line("Checking the file system for write access: Succeeded.");
         
      ruby_load_models();
      
      //char szBuff[256];
      //sprintf(szBuff, "(Loaded %d models)", getControllerModelsCount());
      //popupStartup.addLine(szBuff);

      if ( NULL == g_pCurrentModel )
         log_line("Loaded models: current model is NULL, controller active model VID: %u, controller models count: %d",
            g_uActiveControllerModelVID, getControllerModelsCount());
      else
         log_line("Loaded models: current model VID: %u, current model CID: %u, controller active model VID: %u, controller models count: %d",
            g_pCurrentModel->uVehicleId, g_pCurrentModel->uControllerId,
            g_uActiveControllerModelVID, getControllerModelsCount());
      if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
           (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
      {
         popupStartup.addLine(L("(No active model)"));
         if ( 0 == getControllerModelsCount() )
         {
            popupNoModel.setTitle("Info No Vehicles");
            popupNoModel.addLine("You have no vehicles linked to this controller.");
            popupNoModel.addLine("Press [Menu] key and then select 'Search' to search for a vehicle to connect to.");
            popupNoModel.setIconId(g_idIconInfo,get_Color_IconWarning());
         }
         else
         {
            popupNoModel.setTitle("Info No Active Vehicle");
            popupNoModel.addLine("You have no vehicle selected as active.");
            popupNoModel.addLine("Press [Menu] key and then select 'My Vehicles' to select the vehicle to connect to.");
            popupNoModel.setIconId(g_idIconInfo,get_Color_IconWarning());
         }

         popups_add(&popupNoModel);
         //launch_configure_nics(false, NULL);
         log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
         s_StartSequence = START_SEQ_SCAN_MEDIA_PRE;
         return;
      }
      if ( ! load_temp_local_stats() )
         local_stats_reset_all();

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_SCAN_MEDIA_PRE;
      return;
   }


   if ( s_StartSequence == START_SEQ_SCAN_MEDIA_PRE )
   {
      log_line("Start sequence: START_SEQ_SCAN_MEDIA_PRE");
      popupStartup.addLine(L("Scanning media storage..."));
      log_line("Start sequence: Scanning media storage...");
      char szFile[128];
      char szFile2[128];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE);
      strcpy(szFile2, FOLDER_RUBY_TEMP);
      strcat(szFile2, FILE_TEMP_VIDEO_FILE_INFO);
      if ( access(szFile, R_OK) != -1 )
      if ( access(szFile2, R_OK) != -1 )
      {
         long fSize = 0;
         FILE *pF = fopen(szFile, "rb");
         if ( NULL != pF )
         {
            fseek(pF, 0, SEEK_END);
            fSize = ftell(pF);
            fseek(pF, 0, SEEK_SET);
            fclose(pF);
         }

         log_line("Processing unfinished video file %s, length: %d bytes", szFile, fSize);

         char szBuff[64];
         sprintf(szBuff, "nice -n 5 ./ruby_video_proc &");
         hw_execute_bash_command(szBuff, NULL);
         log_line("Start sequence: Processing unfinished video file...");
         popupStartup.addLine("Processing unfinished video file...");
         hardware_sleep_ms(200);
         warnings_add(0, L("Processing video file..."), g_idIconCamera, get_Color_IconNormal());
         g_bIsVideoProcessing = true;
         log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
         s_StartSequence = START_SEQ_PROCESS_VIDEO;
         return;
      }
      if ( (access(szFile, R_OK) != -1) || (access(szFile2, R_OK) != -1 ) )
      {
         char szComm[1024];
         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
         hw_execute_bash_command(szComm, NULL );

         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
         hw_execute_bash_command(szComm, NULL );

         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
         hw_execute_bash_command(szComm, NULL );
      }

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_SCAN_MEDIA;
      return;
   }
   if ( s_StartSequence == START_SEQ_PROCESS_VIDEO )
   {
      pthread_t th;
      pthread_attr_t attr;
      hw_init_worker_thread_attrs(&attr, -1, 256000, SCHED_OTHER, 0, "video_recording");

      if ( 0 != pthread_create(&th, &attr, &_thread_video_recording_processing, NULL) )
      {
         log_softerror_and_alarm("Failed to start thread to process temp recording.");
         popupStartup.addLine("Failed to process temporary video file.");

         char szComm[1024];
         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
         hw_execute_bash_command(szComm, NULL );

         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
         hw_execute_bash_command(szComm, NULL );

         sprintf(szComm, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
         hw_execute_bash_command(szComm, NULL );

         g_bIsVideoProcessing = false;
      }
      else
      {
         log_line("Start sequence: Processing temporary video file will be done in background.");
         popupStartup.addLine("Moved processing of temporary video file to background.");
      }
      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      s_StartSequence = START_SEQ_SCAN_MEDIA;
   }

   if ( s_StartSequence == START_SEQ_SCAN_MEDIA )
   {
      log_line("Start sequence: START_SEQ_SCAN_MEDIA");
      media_init_and_scan();
      Preferences* p = get_Preferences();
      popup_log_set_show_flag(p->iShowLogWindow);

      if ( is_first_boot() )
         popups_add_topmost(new Popup("One time automatic setup after instalation done: Your CPU settings have been adjusted to match your Raspberry Pi. Please reboot the controller at your convenience for the new changes to take effect.", 0.2,0.1, 0.74, 11));

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      log_line("Go to loading plugins step...");
      popupStartup.addLine(L("Loading plugins..."));
      s_StartSequence = START_SEQ_LOAD_PLUGINS;
      return;
   }

   if ( s_StartSequence == START_SEQ_LOAD_PLUGINS )
   {
      if ( access("failed_plugins", R_OK) != -1 )
      {
         log_line("Loading plugins failed last time. Skip it.");
         popupStartup.addLine(L("Skipped plugins (failed)"));
      }
      else
      {
         log_line("Loading plugins...");
         hw_execute_bash_command("touch failed_plugins", NULL);
         osd_plugins_load();
         osd_widgets_load();
         hw_execute_bash_command("rm -rf failed_plugins", NULL);
         log_line("Finished loading plugins.");
      }

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      popupStartup.addLine(L("Configuring processes..."));
      s_StartSequence = START_SEQ_START_PROCESSES;
   }
   
   if ( s_StartSequence == START_SEQ_START_PROCESSES )
   {
      log_line("Start sequence: START_SEQ_START_PROCESSES");
      
      if ( g_bPlayIntro )
      {
         if ( is_semaphore_signaled_clear(s_pSemaphoreVideoIntro, SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED) )
         {
            log_line("Video intro playback end detected.");
            g_bPlayIntro = false;
            g_bPlayIntroWillEnd = true;
            if ( NULL != s_pSemaphoreVideoIntro )
               sem_close(s_pSemaphoreVideoIntro);
            s_pSemaphoreVideoIntro = NULL;
            sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);
         }
         if ( g_bPlayIntro )
         {
            log_line("Video intro is still playing...");
            return;
         }
      }

      link_watch_init();
      r_check_processes_filesystem();

      log_line("Current local model: %X, VID: %u, first pairing done: %d, controller models: %d, spectator models: %d",
         g_pCurrentModel, g_pCurrentModel->uVehicleId, (int)g_bFirstModelPairingDone, getControllerModelsCount(), getControllerModelsSpectatorCount());
      log_line("Current active vehicle id on controller: %u", g_uActiveControllerModelVID);

      if ( (NULL == g_pCurrentModel) ||
           (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) ||
           (g_bFirstModelPairingDone && (0 == g_uActiveControllerModelVID) ) )
      {
         ruby_resume_watchdog("start up sequence completed with no model");
         s_StartSequence = START_SEQ_COMPLETED;
         log_line("Start sequence: COMPLETED.");
         log_line("System Configured. Start sequence done.");
         popupStartup.addLine(L("System configured."));
         popupStartup.addLine(L("No active model."));
         popupStartup.addLine(L("Start sequence done."));
         popupStartup.setTimeout(4);
         popupStartup.resetTimer();
         s_TimeCentralInitializationComplete = g_TimeNow;
         return;
      }
      onMainVehicleChanged(true);

      if ( 0 < hardware_get_radio_interfaces_count() )
      {
         pairing_start_normal();
         popupStartup.addLine(L("Started looking for vehicles."));
      }
      g_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_RX);
      if ( NULL == g_pProcessStatsTelemetry )
         log_line("Failed to open shared mem to telemetry rx process watchdog stats for reading: %s on start. Will try later.", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
      else
         log_line("Opened shared mem to telemetry rx process watchdog stats for reading.");

      g_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_RX);
      if ( NULL == g_pProcessStatsRouter )
         log_line("Failed to open shared mem to video rx process watchdog stats for reading: %s on start. Will try later.", SHARED_MEM_WATCHDOG_ROUTER_RX);
      else
         log_line("Opened shared mem to video rx process watchdog stats for reading.");

      if ( (NULL != g_pCurrentModel) && g_pCurrentModel->rc_params.rc_enabled )
      {
         g_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_TX);
         if ( NULL == g_pProcessStatsRC )
            log_line("Failed to open shared mem to RC tx process watchdog stats for reading: %s on start. Will try later.", SHARED_MEM_WATCHDOG_RC_TX);
         else
            log_line("Opened shared mem to RC tx process watchdog stats for reading.");
      }

      s_StartSequence = START_SEQ_COMPLETED;
      log_line("Start sequence: COMPLETED.");
      log_line("System Configured. Start sequence done.");
      popupStartup.addLine(L("System configured."));
      if ( g_bIsReinit )
         popupStartup.addLine("Restarting checks finished. Will restart now...");
      else
         popupStartup.addLine(L("Start sequence done."));
      if ( 0 < hardware_get_radio_interfaces_count() )
         popupStartup.addLine(L("Waiting for vehicle..."));
      popupStartup.setTimeout(4);
      popupStartup.resetTimer();

      s_TimeCentralInitializationComplete = g_TimeNow;
      if ( g_bIsHDMIConfirmation )
      {
         s_pMenuConfirmHDMI = new MenuConfirmationHDMI("HDMI Output Configuration Updated","Does the HDMI output looks ok? Select [Yes] to keep the canges or select [No] to revert to the old HDMI configuration.", 0);
         s_pMenuConfirmHDMI->m_yPos = 0.3;
         add_menu_to_stack(s_pMenuConfirmHDMI);
      }

      bool bHasSDCardUpdate = ruby_central_has_sdcard_update(false);

      int iMajor = 0;
      int iMinor = 0;
      get_Ruby_BaseVersion(&iMajor, &iMinor);
      if ( (iMajor < 10) || ((iMajor == 10) && (iMinor < 1)) )
      if ( ! bHasSDCardUpdate )
      {
         MenuConfirmation* pMC = new MenuConfirmation(L("Firmware Instalation"), L("Your controller needs to be fully flashed with the latest version of Ruby."), 0, true);
         pMC->addTopLine(L("Instead of a regular OTA update, a full firmware instalation is required as there where changes in latest Ruby that require a complete update of the system."));
         pMC->setIconId(g_idIconWarning);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
      }

      if ( g_iBootCount == 2 )
      {
         log_line("First boot detected of the UI after install. Checking for import settings from USB...");
         if ( controller_utils_usb_import_has_any_controller_id_file() )
         {
            log_line("USB has exported settings. Add confirmation to import them.");
            menu_discard_all();
            s_pMenuConfirmationImport = new MenuConfirmationImport("Automatic Import", "There are controller settings saved and present on the USB stick. Do you want to import them?", 55);
            s_pMenuConfirmationImport->m_yPos = 0.3;
            add_menu_to_stack(s_pMenuConfirmationImport);
         }
      }
      ruby_resume_watchdog("start up sequence completed");

      if ( ! g_bIsReinit )
      if ( ! g_bIsHDMIConfirmation )
      if ( bHasSDCardUpdate )
      {
         MenuConfirmationSDCardUpdate* pMC = new MenuConfirmationSDCardUpdate();
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
      }

      #if defined (SYSTEM_IS_PRERELEASE)
      Popup* pPre = new Popup("This is a pre-release. Don't forget to update to the final release.",0.25,0.4, 0.5, 10);
      pPre->setFont(g_idFontOSDBig);
      pPre->showTimeoutProgress();
      popups_add_topmost(pPre);
      #endif

      log_line("Finished executing start up sequence step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      return;
   }
}


void clear_shared_mems()
{
   memset(&g_SM_VideoFramesStatsOutput, 0, sizeof(shared_mem_video_frames_stats));
   //memset(&g_SM_VideoInfoStatsRadioIn, 0, sizeof(shared_mem_video_frames_stats));
   //memset(&g_VideoInfoStatsFromVehicleCameraOut, 0, sizeof(shared_mem_video_frames_stats));
   //memset(&g_VideoInfoStatsFromVehicleRadioOut, 0, sizeof(shared_mem_video_frames_stats));
   memset(&g_SM_HistoryRxStats, 0, sizeof(shared_mem_radio_stats_rx_hist));
   memset(&g_SM_HistoryRxStatsVehicle, 0, sizeof(shared_mem_radio_stats_rx_hist));
   memset(&g_SM_VideoDecodeStats, 0, sizeof(shared_mem_video_stream_stats_rx_processors));
   
   memset(&g_SMControllerRTInfo, 0, sizeof(controller_runtime_info));
   memset(&g_SM_DownstreamInfoRC, 0, sizeof(t_packet_header_rc_info_downstream));
   memset(&g_SM_RouterVehiclesRuntimeInfo, 0, sizeof(shared_mem_router_vehicles_runtime_info));
   memset(&g_SM_RadioStats, 0, sizeof(shared_mem_radio_stats));
   memset(&g_SM_RadioRxQueueInfo, 0, sizeof(shared_mem_radio_rx_queue_info));
   memset(&g_SM_DevVideoBitrateHistory, 0, sizeof(shared_mem_dev_video_bitrate_history));
   memset(&g_SM_RCIn, 0, sizeof(t_shared_mem_i2c_controller_rc_in));
   memset(&g_SMVoltage, 0, sizeof(t_shared_mem_i2c_current));
}

void synchronize_shared_mems()
{
   if ( ! pairing_isStarted() )
      return;

   static u32 s_uTimeLastSyncSharedMems = 0;

   if ( g_TimeNow < s_uTimeLastSyncSharedMems + 70 )
      return;

   s_uTimeLastSyncSharedMems = g_TimeNow;

   if ( (NULL != g_pCurrentModel) && (!g_bSearching) )
   {
      if ( g_pCurrentModel->rc_params.rc_enabled )
      if ( NULL == g_pProcessStatsRC )
      {
         g_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_TX);
         if ( NULL == g_pProcessStatsRC )
            log_softerror_and_alarm("Failed to open shared mem to RC tx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_RC_TX);
         else
            log_line("Opened shared mem to RC tx process watchdog stats for reading.");
      }

      if ( NULL == g_pProcessStatsTelemetry )
      {
         g_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_RX);
         if ( NULL == g_pProcessStatsTelemetry )
            log_softerror_and_alarm("Failed to open shared mem to telemetry rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
         else
            log_line("Opened shared mem to telemetry rx process watchdog stats for reading.");
      }

      if ( NULL == g_pProcessStatsRouter )
      {
         g_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_RX);
         if ( NULL == g_pProcessStatsRouter )
            log_softerror_and_alarm("Failed to open shared mem to video rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_ROUTER_RX);
         else
            log_line("Opened shared mem to video rx process watchdog stats for reading.");
      }

      if ( NULL == g_pSM_HistoryRxStats )
      {
         g_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_read();
         if ( NULL == g_pSM_HistoryRxStats )
            log_softerror_and_alarm("Failed to open history radio rx stats shared memory for read.");
      }
   }

   if ( NULL == g_pSMControllerRTInfo )
   {
      g_pSMControllerRTInfo = controller_rt_info_open_for_read();
      if ( NULL == g_pSMControllerRTInfo )
         log_softerror_and_alarm("Failed to open shared mem to controller runtime info for reading: %s", SHARED_MEM_CONTROLLER_RUNTIME_INFO);
      else
         log_line("Opened shared mem to controller runtime info for reading.");
   }

   if ( (NULL != g_pSMControllerRTInfo) && (!g_bFreezeOSD) )
   {
      memcpy((u8*)&g_SMControllerRTInfo, g_pSMControllerRTInfo, sizeof(controller_runtime_info));
      if ( (g_SMControllerRTInfo.iCurrentIndex != g_SMControllerRTInfo.iCurrentIndex2) ||
           (g_SMControllerRTInfo.iCurrentIndex2 != g_SMControllerRTInfo.iCurrentIndex3) )
      {
         if ( g_SMControllerRTInfo.iCurrentIndex2 == g_SMControllerRTInfo.iCurrentIndex3 )
            g_SMControllerRTInfo.iCurrentIndex = g_SMControllerRTInfo.iCurrentIndex2;
      }
   }

   if ( NULL == g_pSMControllerDebugVideoRTInfo )
   {
      g_pSMControllerDebugVideoRTInfo = controller_debug_video_rt_info_open_for_read();
      if ( NULL == g_pSMControllerDebugVideoRTInfo )
         log_softerror_and_alarm("Failed to open shared mem to controller debug video runtime info for reading: %s", SHARED_MEM_CONTROLLER_DEBUG_VIDEO_RUNTIME_INFO);
      else
         log_line("Opened shared mem to controller debug video runtime info for reading.");
   }

   if ( (NULL != g_pSMControllerDebugVideoRTInfo) && (NULL != g_pCurrentModel) && (!g_bFreezeOSD) )
   if ( g_pControllerSettings->iEnableDebugStats || (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS) )
      memcpy((u8*)&g_SMControllerDebugVideoRTInfo, g_pSMControllerDebugVideoRTInfo, sizeof(controller_debug_video_runtime_info));

   if ( NULL != g_pProcessStatsRouter )
      memcpy((u8*)&g_ProcessStatsRouter, g_pProcessStatsRouter, sizeof(shared_mem_process_stats));
   if ( NULL != g_pProcessStatsTelemetry )
      memcpy((u8*)&g_ProcessStatsTelemetry, g_pProcessStatsTelemetry, sizeof(shared_mem_process_stats));
   if ( NULL != g_pProcessStatsRC )
      memcpy((u8*)&g_ProcessStatsRC, g_pProcessStatsRC, sizeof(shared_mem_process_stats));

   if ( g_bFreezeOSD )
      return;

   if ( NULL != g_pSM_DownstreamInfoRC )
      memcpy((u8*)&g_SM_DownstreamInfoRC, g_pSM_DownstreamInfoRC, sizeof(t_packet_header_rc_info_downstream));

   if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
      memcpy((u8*)&g_SM_RouterVehiclesRuntimeInfo, g_pSM_RouterVehiclesRuntimeInfo, sizeof(shared_mem_router_vehicles_runtime_info));
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)&g_SM_RadioStats, g_pSM_RadioStats, sizeof(shared_mem_radio_stats));
   
   if ( NULL != g_pSM_HistoryRxStats )
      memcpy((u8*)&g_SM_HistoryRxStats, g_pSM_HistoryRxStats, sizeof(shared_mem_radio_stats_rx_hist));
   
   if ( g_pControllerSettings->iDeveloperMode )
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   {
      if ( NULL != g_pSM_VideoFramesStatsOutput )
      if ( g_TimeNow >= g_SM_VideoFramesStatsOutput.uLastTimeStatsUpdate + 200 )
         memcpy((u8*)&g_SM_VideoFramesStatsOutput, g_pSM_VideoFramesStatsOutput, sizeof(shared_mem_video_frames_stats));
      //if ( NULL != g_pSM_VideoInfoStatsRadioIn )
      //if ( g_TimeNow >= g_SM_VideoInfoStatsRadioIn.uLastTimeStatsUpdate + 200 )
      //   memcpy((u8*)&g_SM_VideoInfoStatsRadioIn, g_pSM_VideoInfoStatsRadioIn, sizeof(shared_mem_video_frames_stats));
   }

   if ( NULL != g_pSM_VideoDecodeStats )
      memcpy((u8*)&g_SM_VideoDecodeStats, g_pSM_VideoDecodeStats, sizeof(shared_mem_video_stream_stats_rx_processors));
   if ( NULL != g_pSM_RadioRxQueueInfo )
      memcpy((u8*)&g_SM_RadioRxQueueInfo, g_pSM_RadioRxQueueInfo, sizeof(shared_mem_radio_rx_queue_info));
   if ( NULL != g_pSM_RCIn )
      memcpy((u8*)&g_SM_RCIn, g_pSM_RCIn, sizeof(t_shared_mem_i2c_controller_rc_in));
   if ( NULL != g_pSMVoltage )
      memcpy((u8*)&g_SMVoltage, g_pSMVoltage, sizeof(t_shared_mem_i2c_current));

}

void ruby_processing_loop(bool bNoKeys)
{
   if ( s_uTimeLastRenderDuration < 10 )
      hardware_sleep_ms(10 - s_uTimeLastRenderDuration);
   g_TimeNow = get_current_timestamp_ms();

   u32 uTimeStart = g_TimeNow;

   try_read_messages_from_router(5);
   keyboard_consume_input_events();
   u32 uSumEvent = keyboard_get_triggered_input_events();

   if ( uSumEvent & 0xFF0000 )
      warnings_add_input_device_unknown_key((int)((uSumEvent >> 16) & 0xFF));
   handle_commands_loop();

   pairing_loop();

   synchronize_shared_mems();

    if ( g_pControllerSettings->iFreezeOSD )
    if ( g_pControllerSettings->iDeveloperMode )
    if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_BACK )
    if ( ! isMenuOn() )
    if ( g_TimeNow > s_uTimeFreezeOSD + 200 )
    {
       s_uTimeFreezeOSD = g_TimeNow;
       s_bFreezeOSD = ! s_bFreezeOSD;
    }

    s_TimeLastMenuInput = g_TimeNow;
    menu_loop(bNoKeys);
    //if ( keyboard_has_long_press_flag() )
    //   menu_loop_parse_input_events();
    if ( ! bNoKeys )
    if ( ! exectuteActionsInputDebugStats() )
    if ( keyboard_get_triggered_input_events() & (INPUT_EVENT_PRESS_QA1 | INPUT_EVENT_PRESS_QA2 | INPUT_EVENT_PRESS_QA3) )
       executeQuickActions();

   if ( g_iMustSendCurrentActiveOSDLayoutCounter > 0 )
   if ( g_TimeNow >= g_TimeLastSentCurrentActiveOSDLayout+200 )
   if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)  )
   {
      g_iMustSendCurrentActiveOSDLayoutCounter--;
      g_TimeLastSentCurrentActiveOSDLayout = g_TimeNow;
      handle_commands_decrement_command_counter();
      handle_commands_send_single_oneway_command(0, COMMAND_ID_SET_OSD_CURRENT_LAYOUT, (u32)g_pCurrentModel->osd_params.iCurrentOSDScreen, NULL, 0);
   }

   if ( g_bIsRouterReady )
   {
      local_stats_update_loop();
      forward_streams_loop();
      link_watch_loop();
      warnings_periodic_loop();
   }

   g_TimeNow = get_current_timestamp_ms();
   if ( (g_TimeNow - uTimeStart) > 200 )
   if ( (s_StartSequence == START_SEQ_COMPLETED) || (s_StartSequence == START_SEQ_FAILED) )
      log_softerror_and_alarm("Main processing loop took too long (%u ms)", g_TimeNow - uTimeStart);
}

void main_loop_r_central()
{
   ruby_processing_loop(false);

   if ( g_bIsVideoPlaying )
      video_playback_periodic_loop();

   s_uTimeLastRenderDuration = 0;

   if ( (s_StartSequence != START_SEQ_COMPLETED) && (s_StartSequence != START_SEQ_FAILED) )
   {
      if ( (g_uLoopCounter % 3) == 0 )
      {
         start_loop();
         log_line("Startup sequence now after executing a step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      }
      else
      {
         hardware_sleep_ms(10);
         log_line("Startup sequence now after pausing a step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      }
      render_all(g_TimeNow, false, false);
      if ( NULL != g_pProcessStatsCentral )
         g_pProcessStatsCentral->lastActiveTime = g_TimeNow;
      log_line("Startup sequence now after rendering a step: %d (%s)", s_StartSequence, _ruby_central_get_star_seq_string(s_StartSequence));
      return;
   }

   if ( s_StartSequence != START_SEQ_COMPLETED )
      return;

   if ( g_bMarkedHDMIReinit )
   {
      g_bMarkedHDMIReinit = false;
      menu_discard_all();
      ruby_reinit_hdmi_display();
   }

   if ( pairing_isStarted() )
      onEventCheckForPairPendingUIActionsToTake();

   compute_cpu_state();

   int dt = 1000/15;
   if ( 0 != g_pControllerSettings->iRenderFPS )
      dt = 1000/g_pControllerSettings->iRenderFPS;
   if ( g_TimeNow >= s_uTimeLastRender+dt )
   {
      ruby_signal_alive();
      s_uTimeLastRender = g_TimeNow;
      render_all(g_TimeNow, false, false);
      if ( NULL != g_pProcessStatsCentral )
         g_pProcessStatsCentral->lastActiveTime = g_TimeNow;

      if ( g_bIsReinit )
      if ( s_iFPSCount > 5 )
         g_bQuit = true;
   }

   if ( g_bIsHDMIConfirmation )
   if ( NULL != s_pMenuConfirmHDMI )
   if ( g_TimeNow > s_TimeCentralInitializationComplete + 10000 )
   if ( menu_is_menu_on_top(s_pMenuConfirmHDMI) )
   if ( access(s_szFileHDMIChanged, R_OK) != -1 )         
   {
      log_line("Reverting HDMI resolution change...");
      ruby_pause_watchdog("reverting HDMI resolution change");
      save_temp_local_stats();
      hardware_sleep_ms(50);

      FILE* fd = fopen(s_szFileHDMIChanged, "r");
      if ( NULL != fd )
      {
         char szBuff[256];
         int group, mode;
         int tmp1, tmp2, tmp3;
         fscanf(fd, "%d %d", &tmp1, &tmp2 );
         fscanf(fd, "%d %d %d", &tmp1, &tmp2, &tmp3 );
         fscanf(fd, "%d %d", &group, &mode );
         fclose(fd);
         log_line("Reverting HDMI resolution back to: group: %d, mode: %d", group, mode);

         sprintf(szBuff, "rm -rf %s%s", FOLDER_CONFIG, FILE_TEMP_HDMI_CHANGED);
         hw_execute_bash_command(szBuff, NULL);

         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         sprintf(szBuff, "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", group);
         hw_execute_bash_command(szBuff, NULL);
         sprintf(szBuff, "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", mode);
         hw_execute_bash_command(szBuff, NULL);
         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }
      onEventReboot();
      hardware_reboot();
   }
}

void ruby_signal_alive()
{
   if ( NULL != g_pProcessStatsCentral )
      g_pProcessStatsCentral->lastActiveTime = g_TimeNow;
   else
      log_softerror_and_alarm("Shared mem for self process stats is NULL. Can't signal to others that process is active.");
}

void ruby_pause_watchdog(const char* szReason)
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();
   if ( (NULL == szReason) || (0 == szReason[0]) )
      log_line("[Watchdog] Pause requested, current counter: %d, reason: N/A", s_iCountRequestsPauseWatchdog);
   else
      log_line("[Watchdog] Pause requested, current counter: %d, reason: (%s)", s_iCountRequestsPauseWatchdog, szReason);
   
   s_iCountRequestsPauseWatchdog++;
   if ( 1 == s_iCountRequestsPauseWatchdog )
   {
      log_line("[Watchdog] Pause watchdog first time, signal watchdog controller");
      sem_t* ps = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE, O_CREAT, S_IWUSR | S_IRUSR, 0);
      if ( (NULL != ps) && (SEM_FAILED != ps) )
      {
         sem_post(ps);
         sem_close(ps); 
      }
      else
         log_softerror_and_alarm("[Watchdog] Failed to open and signal semaphore %s", SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE);
   }
   else
      log_line("[Watchdog] Pause watchdog duplicate [%d]", s_iCountRequestsPauseWatchdog);
}

void ruby_resume_watchdog(const char* szReason)
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();
   if ( (NULL == szReason) || (0 == szReason[0]) )
      log_line("[Watchdog] Resume requested, current counter: %d, reason: N/A", s_iCountRequestsPauseWatchdog);
   else
      log_line("[Watchdog] Resume requested, current counter: %d, reason: (%s)", s_iCountRequestsPauseWatchdog, szReason);
   
   s_iCountRequestsPauseWatchdog--;
   if ( 0 == s_iCountRequestsPauseWatchdog )
   {
      hardware_sleep_ms(20);
      log_line("[Watchdog] Resumed watchdog, signal watchdog controller");
      sem_t* ps = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_RESUME, O_CREAT, S_IWUSR | S_IRUSR, 0);
      if ( (NULL != ps) && (SEM_FAILED != ps) )
      {
         sem_post(ps);
         sem_close(ps); 
      }
      else
         log_softerror_and_alarm("[Watchdog] Failed to open and signal semaphore %s", SEMAPHORE_WATCHDOG_CONTROLLER_RESUME);
   }
   else
   {
      if ( s_iCountRequestsPauseWatchdog < 0 )
         s_iCountRequestsPauseWatchdog = 0;
      log_line("[Watchdog] Resumed watchdog still duplicate [%d]", s_iCountRequestsPauseWatchdog);
   }
}


void ruby_resume_watchdog_force(const char* szReason)
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();
   if ( (NULL == szReason) || (0 == szReason[0]) )
      log_line("[Watchdog] Full resume requested, current counter: %d, reason: N/A", s_iCountRequestsPauseWatchdog);
   else
      log_line("[Watchdog] Full resume requested, current counter: %d, reason: (%s)", s_iCountRequestsPauseWatchdog, szReason);
   
   s_iCountRequestsPauseWatchdog = 0;
   hardware_sleep_ms(20);
   log_line("[Watchdog] Full resumed watchdog, signal watchdog controller");
   for( int i=0; i<20; i++ )
   {
      sem_t* ps = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_RESUME, O_CREAT, S_IWUSR | S_IRUSR, 0);
      if ( (NULL != ps) && (SEM_FAILED != ps) )
      {
         sem_post(ps);
         sem_close(ps); 
      }
      else
         log_softerror_and_alarm("[Watchdog] Failed to open and signal semaphore %s", SEMAPHORE_WATCHDOG_CONTROLLER_RESUME);
   }
}

   
void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int main(int argc, char *argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   g_bDebugState = false;
   if ( argc >= 1 )
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebugState = true;

   if ( access(CONFIG_FILENAME_DEBUG, R_OK) != -1 )
      g_bDebugState = true;
   if ( g_bDebugState )
      log_line("Starting in debug mode.");
   
   setlocale(LC_ALL, "en_GB.UTF-8");

   s_idBgImage[0] = 0;
   s_idBgImage[1] = 0;
   s_idBgImage[2] = 0;
   s_idBgImage[3] = 0;
   s_idBgImage[4] = 0;
   s_idBgImageMenu[0] = 0;
   s_idBgImageMenu[1] = 0;
   s_idBgImageMenu[2] = 0;
   s_idBgImageMenu[3] = 0;
   s_idBgImageMenu[4] = 0;

   log_init("Central");

   if ( strcmp(argv[argc-1], "-mount") == 0 )
   {
      log_enable_stdout();
      char szCommand[256];
      sprintf(szCommand, "mkdir -p %s", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, NULL);
      sprintf(szCommand, "chmod -R 777 %s", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, NULL);

      int iMountRes = hardware_try_mount_usb();
      if ( iMountRes <= 0 )
         log_softerror_and_alarm("Failed to mount USB drive. Error code: %d", iMountRes);
      else
         log_line("Mounted USB driver. Result: %d", iMountRes);
      return 0;
   }

   if ( strcmp(argv[argc-1], "-unmount") == 0 )
   {
      log_enable_stdout();
      extern int s_iUSBMounted;
      s_iUSBMounted = 1;
      hardware_unmount_usb();
      return 0;
   }

   hardware_detectBoardAndSystemType();

   initLocalizationData();

   check_licences();

   char szFile[MAX_FILE_PATH_SIZE];

   g_uControllerId = controller_utils_getControllerId();
   log_line("Controller UID: %u", g_uControllerId);
 
   if ( strcmp(argv[argc-1], "-reinit") == 0 )
   {
      log_line("Ruby Central crashed last time, reinitializing graphics engine...");
      g_bIsReinit = true;
   }

   if ( strcmp(argv[argc-1], "-nointro") == 0 )
      g_bPlayIntro = false;
   if ( g_bIsReinit )
      g_bPlayIntro = false;

   strcpy(s_szFileHDMIChanged, FOLDER_CONFIG);
   strcat(s_szFileHDMIChanged, FILE_TEMP_HDMI_CHANGED);

   g_bIsHDMIConfirmation = false;
   if ( access(s_szFileHDMIChanged, R_OK) != -1 )
      g_bIsHDMIConfirmation = true;

   log_line("Ruby UI starting");

   init_hardware();

   if ( ! load_Preferences() )
      save_Preferences();
   Preferences* p = get_Preferences();

   #if defined (HW_PLATFORM_RASPBERRY)
   // Disable non latin charsets (raster fonts)
   if ( (p->iLanguage == 0) || (p->iLanguage == 4) || (p->iLanguage == 5) )
   {
      p->iLanguage = 1;
      save_Preferences();
   }
   #endif
   if ( p->iLanguage == 4 )
   {
      p->iLanguage = 1;
      save_Preferences();
   }

   setActiveLanguage(p->iLanguage);
   log_line("Set active language to: %d", p->iLanguage);

   if ( ! load_ControllerSettings() )
      save_ControllerSettings();
   g_pControllerSettings = get_ControllerSettings();
   g_pControllerSettings->iEnableDebugStats = 0;

   if ( argc >= 1 )
   if ( strcmp(argv[argc-1], "-ds") == 0 )
   {
      g_pControllerSettings->iEnableDebugStats = 1;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      log_line("Starting with debug stats...");
   }

   s_uTimeToSwitchLogLevel = get_current_timestamp_ms() + 10000;
   
   #if defined (HW_PLATFORM_RASPBERRY)
   hdmi_enum_modes();
   #endif

   #if defined (HW_PLATFORM_RADXA)
   ruby_drm_core_wait_for_display_connected();
   hdmi_enum_modes();
   int iHDMIIndex = hdmi_load_current_mode();
   if ( iHDMIIndex < 0 )
      iHDMIIndex = hdmi_get_best_resolution_index_for(DEFAULT_RADXA_DISPLAY_WIDTH, DEFAULT_RADXA_DISPLAY_HEIGHT, DEFAULT_RADXA_DISPLAY_REFRESH);
   log_line("HDMI mode to use: %d (%d x %d @ %d)", iHDMIIndex, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
   ruby_drm_core_init(0, DRM_FORMAT_ARGB8888, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());
   ruby_drm_core_set_plane_properties_and_buffer(ruby_drm_core_get_main_draw_buffer_id());
   ruby_drm_enable_vsync(g_pControllerSettings->iHDMIVSync);
   #endif

   g_pRenderEngine = render_init_engine();
   log_line("Render Engine was initialized.");

   if ( g_bPlayIntro )
   {
      strcpy(szFile, FOLDER_BINARIES);
      strcat(szFile, "res/intro.h264");
      if ( access(szFile, R_OK) == -1 )
         g_bPlayIntro = false;
      else
      {
         sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
         sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);

         s_pSemaphoreVideoIntroWillFinish = sem_open(SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH, O_RDWR);
         if ( (NULL == s_pSemaphoreVideoIntroWillFinish) || (SEM_FAILED == s_pSemaphoreVideoIntroWillFinish) )
         {
            log_error_and_alarm("Failed to create read semaphore: %s, try alternative.", SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
            s_pSemaphoreVideoIntroWillFinish = sem_open(SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH, O_CREAT, S_IWUSR | S_IRUSR, 0); 
            if ( (NULL == s_pSemaphoreVideoIntroWillFinish) || (SEM_FAILED == s_pSemaphoreVideoIntroWillFinish) )
            {
               log_error_and_alarm("Failed to create read semaphore: %s", SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
               s_pSemaphoreVideoIntroWillFinish = NULL;
            }
         }

         s_pSemaphoreVideoIntro = sem_open(SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED, O_RDWR);
         if ( (NULL == s_pSemaphoreVideoIntro) || (SEM_FAILED == s_pSemaphoreVideoIntro) )
         {
            log_error_and_alarm("Failed to create read semaphore: %s, try alternative.", SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);
            s_pSemaphoreVideoIntro = sem_open(SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED, O_CREAT, S_IWUSR | S_IRUSR, 0); 
            if ( (NULL == s_pSemaphoreVideoIntro) || (SEM_FAILED == s_pSemaphoreVideoIntro) )
            {
               log_error_and_alarm("Failed to create read semaphore: %s", SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);
               s_pSemaphoreVideoIntro = NULL;
            }
         }
         if ( (NULL != s_pSemaphoreVideoIntroWillFinish) && (SEM_FAILED != s_pSemaphoreVideoIntroWillFinish) )
         if ( (NULL != s_pSemaphoreVideoIntro) && (SEM_FAILED != s_pSemaphoreVideoIntro) )
         {
            log_line("Opened semaphore for checking video intro will finish: (%s)", SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
            log_line("Opened semaphore for checking video intro play finish: (%s)", SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);
            is_semaphore_signaled_clear(s_pSemaphoreVideoIntroWillFinish, SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
            is_semaphore_signaled_clear(s_pSemaphoreVideoIntro, SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);
            g_bPlayIntroWillEnd = false;
            char szComm[256];
            sprintf(szComm, "./%s -file res/intro.h264 -fps 15 -endexit&", VIDEO_PLAYER_OFFLINE);
            hw_execute_bash_command_nonblock(szComm, NULL);
            hardware_sleep_ms(500);
            hardware_sleep_ms(500);
            hardware_enable_audio_output();
            hardware_set_audio_output_volume(g_pControllerSettings->iAudioOutputVolume);
            hardware_audio_play_file_async("res/intro1.wav");
         }
      }
   }

   load_resources();
   osd_apply_preferences();
   menu_init();
   Menu::setRenderMode(p->iMenuStyle);
   hardware_swap_buttons(p->iSwapUpDownButtons);
   warnings_remove_all();

   hardware_serial_init_ports();

   clear_shared_mems();   
   ruby_clear_all_ipc_channels();

   g_pProcessStatsCentral = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_CENTRAL);
   if ( NULL == g_pProcessStatsCentral )
      log_softerror_and_alarm("Failed to open shared mem for ruby_central process watchdog for writing: %s", SHARED_MEM_WATCHDOG_CENTRAL);
   else
      log_line("Opened shared mem for ruby_centrall process watchdog for writing.");
 
   ruby_pause_watchdog("UX startup");
   hardware_i2c_load_device_settings();

   if ( ! load_ControllerInterfacesSettings() )
      save_ControllerInterfacesSettings();
      
   save_ControllerInterfacesSettings();

   #if defined (HW_PLATFORM_RADXA)
   log_line("Ruby OLED Init...");
   if ( hardware_i2c_has_oled_screen() )
   {
      oled_render_init();
      oled_render_thread_start();
   }
   #endif

   if ( g_pControllerSettings->iCoresAdjustment )
      hw_set_current_thread_affinity("central", CORE_AFFINITY_CENTRAL_UI, CORE_AFFINITY_CENTRAL_UI);

   if ( g_pControllerSettings->iPrioritiesAdjustment )
   {
      hw_set_priority_current_proc(g_pControllerSettings->iThreadPriorityOthers);
      hw_set_priority_current_proc(g_pControllerSettings->iThreadPriorityCentral);
   }

   g_TimeNow = get_current_timestamp_ms();
   s_uTimeLastChangeBgImage = g_TimeNow;

   log_line("Started.");

   popupStartup.setFont(g_idFontMenu);
   if ( g_bPlayIntro )
   {
      popupStartup.setBackgroundAlpha(0.3);
      popupStartup.setXPos(0.03);
      popupStartup.setYPos(0.05);
   }
   popupNoModel.setFont(g_idFontMenu);
   popups_add(&popupStartup);

   alarms_reset();

   if ( g_bIsReinit )
   {
      Popup* p = new Popup(true, "User Interface is reinitializing, please wait...", 0);
      p->setFont(g_idFontOSDBig);
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add(p);
   }

   g_iBootCount = 0;

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%d", &g_iBootCount);
      fclose(fd);
      fd = NULL;
   }

   shared_vars_state_reset_all_vehicles_runtime_info();

   g_bFirstModelPairingDone = false;

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FIRST_PAIRING_DONE); 
   if ( access(szFile, R_OK) != -1 )
      g_bFirstModelPairingDone = true;
 
   log_line("First pairing is done? %s", g_bFirstModelPairingDone?"yes":"no");

   keyboard_init();

   controller_rt_info_init(&g_SMControllerRTInfo);

   s_StartSequence = START_SEQ_PRE_LOAD_CONFIG;
   log_line("Started main loop.");
   g_TimeStart = get_current_timestamp_ms();

   log_line("Start main loop.");

   while (!g_bQuit) 
   {
      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      if ( rx_scope_is_started() )
      {
         try_read_messages_from_router(10);
         rx_scope_loop();
      }
      else
      {
         main_loop_r_central();

         if ( 0 != s_uTimeToSwitchLogLevel )
         if ( g_TimeNow > s_uTimeToSwitchLogLevel )
         {
            s_uTimeToSwitchLogLevel = 0;
            log_line("Check and switch log level to: %d", p->nLogLevel);
            if ( p->nLogLevel != 0 )
               log_only_errors();
         }
      }
   }
   
   ruby_shutdown_ui();
   return 0;
}


void ruby_set_active_model_id(u32 uVehicleId)
{
   g_uActiveControllerModelVID = uVehicleId;
   Model* pModel = NULL;
   if ( uVehicleId != 0 )
   {
     pModel = findModelWithId(uVehicleId, 62);
     if ( NULL == pModel )
        log_line("Ruby: Set active model vehicle id to %u (no model found on controller for this VID)", g_uActiveControllerModelVID );
     else
        log_line("Ruby: Set active model vehicle id to %u, mode: %s", g_uActiveControllerModelVID, (pModel->is_spectator)?"spectator mode":"control mode");
   }
   else
      log_line("Ruby: Set active model vehicle id to 0");

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_ACTIVE_CONTROLLER_MODEL);
   FILE* fd = fopen(szFile, "wb");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", g_uActiveControllerModelVID);
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Ruby: Failed to save active model vehicle id");
}

void ruby_mark_reinit_hdmi_display()
{
   g_bMarkedHDMIReinit = true;
}

void ruby_reinit_hdmi_display()
{
   log_line("Reinit HDMI display...");

   pairing_stop();

   free_all_fonts();
   render_free_engine();
   
   #if defined (HW_PLATFORM_RADXA)
   ruby_drm_core_uninit();
   ruby_drm_core_wait_for_display_connected();

   hdmi_enum_modes();
   int iHDMIIndex = hdmi_load_current_mode();
   if ( iHDMIIndex < 0 )
      iHDMIIndex = hdmi_get_best_resolution_index_for(DEFAULT_RADXA_DISPLAY_WIDTH, DEFAULT_RADXA_DISPLAY_HEIGHT, DEFAULT_RADXA_DISPLAY_REFRESH);
   log_line("HDMI mode to use: %d (%d x %d @ %d)", iHDMIIndex, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
   ruby_drm_core_init(0, DRM_FORMAT_ARGB8888, hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh());
   ruby_drm_core_set_plane_properties_and_buffer(ruby_drm_core_get_main_draw_buffer_id());
   ruby_drm_enable_vsync(g_pControllerSettings->iHDMIVSync);
   #endif

   g_pRenderEngine = render_init_engine();
   log_line("Render Engine was initialized.");
   
   load_resources();
   osd_apply_preferences();
   menu_init();

   log_line("Done reinit HDMI display.");

   pairing_start_normal(); 
}

void ruby_shutdown_ui()
{
   log_line("Started shutdown UI...");
   g_bQuit = true;


   if ( NULL != s_pSemaphoreVideoIntroWillFinish )
      sem_close(s_pSemaphoreVideoIntroWillFinish);
   if ( NULL != s_pSemaphoreVideoIntro )
      sem_close(s_pSemaphoreVideoIntro);
   s_pSemaphoreVideoIntroWillFinish = NULL;
   s_pSemaphoreVideoIntro = NULL;
   sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_WILL_FINISH);
   sem_unlink(SEMAPHORE_VIDEO_FILE_PLAYBACK_FINISHED);

   keyboard_uninit();
   link_watch_uninit();
   if ( ! g_bIsReinit )
      pairing_stop();

   while ( s_bThreadCPUStateRunning )
   {
      hardware_sleep_ms(50);
   }

   #if defined (HW_PLATFORM_RADXA)
   oled_render_shutdown();
   #endif
   
   controller_stop_i2c();

   log_line("Central: Releasing %d OSD plugins...", g_iPluginsOSDCount);
   for( int i=0; i<g_iPluginsOSDCount; i++ )
      if ( NULL != g_pPluginsOSD[i] )
      if ( NULL != g_pPluginsOSD[i]->pLibrary )
         dlclose(g_pPluginsOSD[i]->pLibrary);

   shared_mem_i2c_current_close(g_pSMVoltage);
   shared_mem_i2c_rotary_encoder_buttons_events_close(g_pSMRotaryEncoderButtonsEvents);

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_RX, g_pProcessStatsTelemetry);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_RX, g_pProcessStatsRouter);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_TX, g_pProcessStatsRC);

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, g_pProcessStatsCentral);
   shared_mem_ctrl_ping_stats_info_close(g_pSMDbgPingStats);

   log_line("Shutdown UI, free hardware...");
 
   hardware_release();

   render_free_engine();

   #if defined (HW_PLATFORM_RADXA)
   ruby_drm_core_uninit();
   #endif

   log_line("Finished shutdown UI.");
}