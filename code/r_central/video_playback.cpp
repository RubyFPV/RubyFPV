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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_files.h"
#include "osd/osd.h"
#include "video_playback.h"
#include "timers.h"
#include "shared_vars.h"
#include "pairing.h"
#include "colors.h"
#include "fonts.h"
#include "process_router_messages.h"
#include "ruby_central.h"

static u32 s_uTimestampVideoPlaybackLastLoopMs = 0;
static u32 s_uTimeLastVideoPlayerProcessCheck = 0;

static FILE* s_pFilePlaybackSRT = NULL;
static u32 s_uTimeSRTCurrentFrameStart = 0;
static u32 s_uTimeSRTCurrentFrameEnd = 0;
static char s_szSRTLine1[128];
static char s_szSRTLine2[128];

static FILE* s_pFilePlaybackOSD = NULL;
int s_iMSPOSDFCType, s_iMSPOSDFontType, s_iMSPOSDRows, s_iMSPOSDCols;
u16 s_uMSPOSDDisplayBuffer[MAX_MSP_CHARS_BUFFER];
static u32 s_uTimeOSDCurrentFrameStart = 0;
static u32 s_uCountOSDFramesRead = 0;

void video_playback_play_file(const char* szVideoInfoFile)
{
   char szComm[512];
   char szFile[MAX_FILE_PATH_SIZE];
   int iFPS, iDurrationSec, iWidth, iHeight, iType;

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER);
   hw_execute_bash_command(szComm, NULL);

   if ( (NULL == szVideoInfoFile) || (0 == szVideoInfoFile[0]) )
      return;
   FILE* fd = fopen(szVideoInfoFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to open video info file [%s]", szVideoInfoFile);
      return;
   }
   if ( 1 != fscanf(fd, "%s", szFile) )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to read from video info file [%s]", szVideoInfoFile);
      fclose(fd);
      return;
   }
   if ( 2 != fscanf(fd, "%d %d", &iFPS, &iDurrationSec) )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to read from video info file [%s]", szVideoInfoFile);
      fclose(fd);
      return;
   }
   if ( 3 != fscanf(fd, "%d %d %d", &iWidth, &iHeight, &iType) )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to read from video info file [%s]", szVideoInfoFile);
      fclose(fd);
      return;
   }

   if ( 4 != fscanf(fd, "%d %d %d %d", &s_iMSPOSDFCType, &s_iMSPOSDFontType, &s_iMSPOSDCols, &s_iMSPOSDRows) )
   {
      s_iMSPOSDFCType = 0;
      s_iMSPOSDFontType = 0;
      s_iMSPOSDRows = 0;
      s_iMSPOSDCols = 0;
      log_softerror_and_alarm("VideoPlayback: Failed to read MSPOSD info from video info file [%s]", szVideoInfoFile);
   }
   if ( s_iMSPOSDRows < 0 )
      s_iMSPOSDRows = 0;
   if ( s_iMSPOSDRows > 24 )
      s_iMSPOSDRows = 24;
   if ( s_iMSPOSDCols < 0 )
      s_iMSPOSDCols = 0;
   if ( s_iMSPOSDCols > 64 )
      s_iMSPOSDCols = 64;

   log_line("VideoPlayback: Read info file: wxh: %dx%d, type: %d, fc: %d, osd font: %d, cols/rows: %d/%d",
       iWidth, iHeight, iType, s_iMSPOSDFCType, s_iMSPOSDFontType, s_iMSPOSDCols, s_iMSPOSDRows);
   fclose(fd);

   if ( pairing_isStarted() )
   {
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY, 1);
      hardware_sleep_ms(200);
   }  
   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -file %s%s -fps %d", VIDEO_PLAYER_OFFLINE, FOLDER_MEDIA, szFile, iFPS);
   #endif

   #ifdef HW_PLATFORM_RADXA
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -file %s%s -fps %d", VIDEO_PLAYER_OFFLINE, FOLDER_MEDIA, szFile, iFPS);
   #endif

   if ( g_pControllerSettings->iCoresAdjustment )
   {
      char szTmp[32];
      sprintf(szTmp, " -af %d", (0x01 << CORE_AFFINITY_VIDEO_OUTPUT));
      strcat(szComm, szTmp);
   }

   if ( g_pControllerSettings->iPrioritiesAdjustment && (g_pControllerSettings->iThreadPriorityVideo > 1) && (g_pControllerSettings->iThreadPriorityVideo < 100) )
   {
      char szTmp[32];
      sprintf(szTmp, " -rawp %d", g_pControllerSettings->iThreadPriorityVideo );
      strcat(szComm, szTmp);
   }

   strcat(szComm, "&");
   hw_execute_bash_command_nonblock(szComm, NULL);
   hardware_sleep_ms(100);

   g_bIsVideoPlaying = true;
   g_uVideoPlayingTimeMs = 0;
   g_uVideoPlayingLengthSec = iDurrationSec;
   s_uTimestampVideoPlaybackLastLoopMs = g_TimeNow;
   s_uTimeLastVideoPlayerProcessCheck = 0;

   char szFullPath[MAX_FILE_PATH_SIZE];
   strcpy(szFullPath, FOLDER_MEDIA);
   strcat(szFullPath, szFile);

   hardware_file_replace_extension(szFullPath, "srt");
   s_pFilePlaybackSRT = fopen(szFullPath, "r");
   if ( NULL == s_pFilePlaybackSRT )
      log_softerror_and_alarm("VideoPlayback: Failed to open srt file: [%s]", szFullPath);
   else
      log_line("VideoPlayback: Opened playback srt file: [%s]", szFullPath);
   s_uTimeSRTCurrentFrameStart = 0;
   s_uTimeSRTCurrentFrameEnd = 0;
   s_szSRTLine1[0] = 0;
   s_szSRTLine2[0] = 0;

   hardware_file_replace_extension(szFullPath, "osd");
   s_pFilePlaybackOSD = fopen(szFullPath, "r");
   if ( NULL == s_pFilePlaybackOSD )
      log_softerror_and_alarm("VideoPlayback: Failed to open OSD file: [%s]", szFullPath);
   else
      log_line("VideoPlayback: Opened playback OSD file: [%s]", szFullPath);
   s_uTimeOSDCurrentFrameStart = 0;
   s_uCountOSDFramesRead = 0;
   log_line("VideoPlayback: Playing now file [%s], duration: %d sec", szFile, iDurrationSec);
}

void video_playback_stop()
{
   if ( ! g_bIsVideoPlaying )
      return;

   if ( NULL != s_pFilePlaybackSRT )
      fclose(s_pFilePlaybackSRT);
   s_pFilePlaybackSRT = NULL;

   if ( NULL != s_pFilePlaybackOSD )
      fclose(s_pFilePlaybackOSD);
   s_pFilePlaybackOSD = NULL;

   log_line("VideoPlayback: Stopping video playback...");
   hw_stop_process(VIDEO_PLAYER_OFFLINE);
 
   g_bIsVideoPlaying = false;
   render_all(get_current_timestamp_ms(), true);

   log_line("Stopped video playback.");

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER);
   hw_execute_bash_command(szComm, NULL);
      
   if ( pairing_isStarted() )
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY, 0);

   render_all(get_current_timestamp_ms(), true);
}

void _video_playback_read_next_srt_frame()
{
   if ( (NULL == s_pFilePlaybackSRT) || (g_uVideoPlayingTimeMs < s_uTimeSRTCurrentFrameEnd) )
      return;
   
   int iFrame = 0;
   char szStartTime[128];
   char szEndTime[128];
   char szBuff[128];

   if ( 1 != fscanf(s_pFilePlaybackSRT, "%d", &iFrame) )
      return;

   if ( 3 != fscanf(s_pFilePlaybackSRT, "%s %s %s", szStartTime, szBuff, szEndTime) )
      return;


   int iLines = 0;
   while ( true )
   {
      if ( NULL == fgets(szBuff, 127, s_pFilePlaybackSRT) )
         return;
      if ( (10 == szBuff[0]) || (13 == szBuff[0]) )
      {
         if ( 0 == iLines )
            continue;
         break;
      }
      int iLen = strlen(szBuff);
      if ( iLen > 0 )
      if ( (10 == szBuff[iLen-1]) || (13 == szBuff[iLen-1]) )
         szBuff[iLen-1] = 0;
      iLen = strlen(szBuff);
      if ( iLen > 0 )
      if ( (10 == szBuff[iLen-1]) || (13 == szBuff[iLen-1]) )
         szBuff[iLen-1] = 0;

      if ( 0 == iLines )
         strcpy(s_szSRTLine1, szBuff);
      else
         strcpy(s_szSRTLine2, szBuff);
      iLines++;
   }
   int iLen = strlen(szStartTime);
   for(int i=0; i<iLen; i++ )
   {
      if ( (szStartTime[i] == ':') || (szStartTime[i] == ',') )
         szStartTime[i] = ' ';
   }

   iLen = strlen(szEndTime);
   for(int i=0; i<iLen; i++ )
   {
      if ( (szEndTime[i] == ':') || (szEndTime[i] == ',') )
         szEndTime[i] = ' ';
   }

   int iHour, iMin, iSec, iMilisec;
   if ( 4 != sscanf(szEndTime, "%d %d %d %d", &iHour, &iMin, &iSec, &iMilisec) )
      return;

   s_uTimeSRTCurrentFrameEnd = iMilisec + iSec * 1000 + iMin * 1000 * 60  + iHour * 1000 * 60 * 60;
}


void _video_playback_read_next_osd_frame()
{
   if ( (NULL == s_pFilePlaybackOSD) || (g_uVideoPlayingTimeMs <= s_uTimeOSDCurrentFrameStart) )
      return;
   
   u8 uBuffer[128];
   u16 uBuffer16[MAX_MSP_CHARS_BUFFER];
   if ( 0 == s_uCountOSDFramesRead )
   {
      memset(uBuffer, 0, 40);
      int nRead = fread(uBuffer, 1, 40, s_pFilePlaybackOSD);
      if ( nRead != 40 )
      {
         log_softerror_and_alarm("VideoPlayback: Failed to read OSD file header.");
         return;
      }
      log_line("VideoPlayback: Read OSD file header. FC: %s", uBuffer);
   }

   int nRead = fread(uBuffer, 1, 4, s_pFilePlaybackOSD);
   if ( nRead != 4 )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to read OSD file frame timestamp.");
      return;
   }
   memcpy((u8*)&s_uTimeOSDCurrentFrameStart, uBuffer, sizeof(u32));
   
   nRead = fread(uBuffer16, 1, DEFAULT_MSPOSD_RECORDING_ROWS * DEFAULT_MSPOSD_RECORDING_COLS * sizeof(u16), s_pFilePlaybackOSD);
   if ( nRead != DEFAULT_MSPOSD_RECORDING_ROWS * DEFAULT_MSPOSD_RECORDING_COLS * sizeof(u16) )
   {
      log_softerror_and_alarm("VideoPlayback: Failed to read OSD file frame osd data.");
      return;
   }

   int iPos = 0;
   for( int y=0; y<DEFAULT_MSPOSD_RECORDING_ROWS; y++ )
   for( int x=0; x<DEFAULT_MSPOSD_RECORDING_COLS; x++ )
      s_uMSPOSDDisplayBuffer[x + y * s_iMSPOSDCols] = uBuffer16[iPos++];

   s_uCountOSDFramesRead++;
}

void video_playback_periodic_loop()
{
   if ( ! g_bIsVideoPlaying )
      return;

   if ( access(CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER, R_OK) == -1 )
      g_uVideoPlayingTimeMs += g_TimeNow - s_uTimestampVideoPlaybackLastLoopMs;
   s_uTimestampVideoPlaybackLastLoopMs = g_TimeNow;
   
   _video_playback_read_next_srt_frame();
   _video_playback_read_next_osd_frame();

   if ( g_TimeNow > s_uTimeLastVideoPlayerProcessCheck + 3000 )
   {
      s_uTimeLastVideoPlayerProcessCheck = g_TimeNow;
      if ( g_uVideoPlayingTimeMs > 2000 )
      if ( ! hw_process_exists(VIDEO_PLAYER_OFFLINE) )
      {
         log_line("VideoPlayback: Video player process (%s) does not exist, exit playback.", VIDEO_PLAYER_OFFLINE);
         video_playback_stop();
      }
   }
   if ( g_uVideoPlayingTimeMs > g_uVideoPlayingLengthSec*1000 + 1000 )
   {
      log_line("VideoPlayback: Video playback duration reached. Stopping video player.");
      video_playback_stop();
   }
}

void video_playback_render()
{
   if ( ! g_bIsVideoPlaying )
      return;

   char szBuff[1024];

   g_pRenderEngine->startFrame();

   double cColor[] = {0,0,0,0.7};
   g_pRenderEngine->setColors(cColor, 0.9);
   g_pRenderEngine->drawRoundRect(0.02, 0.03, 0.36, 0.1, 0.02);
   g_pRenderEngine->setColors(get_Color_MenuText());

   g_pRenderEngine->setFill(255,255,255,1);
   g_pRenderEngine->setStroke(0,0,0,1);
   g_pRenderEngine->setStrokeSize(1);
   float y = 0.046;

   if ( g_uVideoPlayingTimeMs/1000 > g_uVideoPlayingLengthSec+1 )
   {
      sprintf(szBuff, "Finished.");
      g_pRenderEngine->drawText(0.04, y, g_idFontMenuLarge, szBuff);
   }
   else
   {
      char szVerb[32];
      strcpy(szVerb, "Playing");
      if ( access(CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER, R_OK) != -1 )
         strcpy(szVerb, "Paused");

      sprintf(szBuff, "%s %02d", szVerb, ((g_uVideoPlayingTimeMs/1000)/60));
      float fWidth = g_pRenderEngine->textWidth(g_idFontMenuLarge, szBuff);
      g_pRenderEngine->drawText(0.04, y, g_idFontMenuLarge, szBuff);

      fWidth += 0.15*g_pRenderEngine->textWidth(g_idFontMenuLarge, "A");

      if ( (g_uVideoPlayingTimeMs/500)%2 )
         g_pRenderEngine->drawText(0.04 + fWidth, y, g_idFontMenuLarge, ":");
      fWidth += 0.5*g_pRenderEngine->textWidth(g_idFontMenuLarge, "A");
      sprintf(szBuff, "%02d / %d:%02d", (g_uVideoPlayingTimeMs/1000)%60, g_uVideoPlayingLengthSec/60, g_uVideoPlayingLengthSec%60);
      g_pRenderEngine->drawText(0.04 + fWidth, y, g_idFontMenuLarge, szBuff);
   }
   sprintf(szBuff, "Press [Menu] for pause/resume or [Back] to stop");
   g_pRenderEngine->drawText(0.04, 0.084, g_idFontMenu, szBuff);  

   float fWidthText = 0.0;
   float hText = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   y = 0.92;
   if ( s_szSRTLine1[0] != 0 )
   {
      fWidthText = g_pRenderEngine->textWidth(g_idFontMenuLarge, s_szSRTLine1);

      g_pRenderEngine->setColors(cColor, 0.4);
      g_pRenderEngine->drawRect(0.5 - 0.5*fWidthText - 0.02, y, fWidthText + 0.04, hText);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawText(0.5 - 0.5*fWidthText, y, g_idFontMenuLarge, s_szSRTLine1);
      y += hText;
   }
   if ( s_szSRTLine2[0] != 0 )
   {
      fWidthText = g_pRenderEngine->textWidth(g_idFontMenuLarge, s_szSRTLine2);

      g_pRenderEngine->setColors(cColor, 0.4);
      g_pRenderEngine->drawRect(0.5 - 0.5*fWidthText - 0.02, y, fWidthText + 0.04, hText);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawText(0.5 - 0.5*fWidthText, y, g_idFontMenuLarge, s_szSRTLine2);
      y += hText;
   }

   if ( (s_pFilePlaybackOSD != NULL) && (s_uCountOSDFramesRead != 0) )
   if ( (s_iMSPOSDCols != 0) && (s_iMSPOSDRows != 0) )
      osd_render_msposd_buffer(s_iMSPOSDFCType, s_iMSPOSDFontType, s_iMSPOSDCols, s_iMSPOSDRows, s_uMSPOSDDisplayBuffer);

   g_pRenderEngine->endFrame();
}