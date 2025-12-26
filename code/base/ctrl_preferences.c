/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "ctrl_preferences.h"
#include <ctype.h>

#define PREFERENCES_SETTINGS_STAMP_ID "vIV.3"

#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)

Preferences s_Preferences;

void reset_Preferences()
{
   memset(&s_Preferences, 0, sizeof(s_Preferences));
   s_Preferences.iMenusStacked = 1;
   s_Preferences.iInvertColorsOSD = 0;
   s_Preferences.iInvertColorsMenu = 0;
   s_Preferences.iActionQuickButton1 = quickActionVideoRecord;
   s_Preferences.iActionQuickButton2 = quickActionCycleOSD;
   s_Preferences.iActionQuickButton3 = quickActionTakePicture;
   s_Preferences.iOSDScreenSize = 1;
   s_Preferences.iScaleOSD = 0;
   s_Preferences.iScaleAHI = 0;
   s_Preferences.iScaleMenus = 0;
   s_Preferences.iAddOSDOnScreenshots = 1;
   s_Preferences.iShowLogWindow = 0;
   s_Preferences.iMenuDismissesAlarm = 0;
   s_Preferences.iVideoDestination = prefVideoDestination_Mem;
   s_Preferences.iStartVideoRecOnArm = 0;
   s_Preferences.iStopVideoRecOnDisarm = 0;
   s_Preferences.iShowControllerCPUInfo = 0;
   s_Preferences.iShowBigRecordButton = 0;
   s_Preferences.iSwapUpDownButtons = 0;
   s_Preferences.iSwapUpDownButtonsValues = 0;

   s_Preferences.iAHIToSides = 0;
   s_Preferences.iAHIShowAirSpeed = 0;
   s_Preferences.iAHIStrokeSize = 0;

   s_Preferences.iUnits = prefUnitsMetric;
   s_Preferences.iUnitsHeight = prefUnitsMetric;

   s_Preferences.iColorOSD[0] = 255;
   s_Preferences.iColorOSD[1] = 250;
   s_Preferences.iColorOSD[2] = 220;
   s_Preferences.iColorOSD[3] = 100; // 100%
   s_Preferences.iColorOSDOutline[0] = 10;
   s_Preferences.iColorOSDOutline[1] = 10;
   s_Preferences.iColorOSDOutline[2] = 10;
   s_Preferences.iColorOSDOutline[3] = 90; // 90%
   s_Preferences.iColorAHI[0] = 208;
   s_Preferences.iColorAHI[1] = 238;
   s_Preferences.iColorAHI[2] = 214;
   s_Preferences.iColorAHI[3] = 100; // 100%
   s_Preferences.iOSDOutlineThickness = 0;
   s_Preferences.iRecordingLedAction = 2; // blink

   s_Preferences.iDebugMaxPacketSize = MAX_VIDEO_PACKET_DATA_SIZE;
   s_Preferences.iDebugSBWS = 2;
   s_Preferences.iDebugRestartOnRadioSilence = 0;
   s_Preferences.iOSDFont = 1;
   s_Preferences.iPersistentMessages = 1;
   s_Preferences.nLogLevel = 0;
   s_Preferences.iDebugShowDevVideoStats = 0;
   s_Preferences.iDebugShowDevRadioStats = 0;
   s_Preferences.iDebugShowFullRXStats = 0;
   s_Preferences.iDebugShowVideoSnapshotOnDiscard = 0;
   s_Preferences.iDebugWiFiChangeDelay = DEFAULT_DELAY_WIFI_CHANGE;

   s_Preferences.iAutoExportSettings = 0;
   s_Preferences.iAutoExportSettingsWasModified = 0;

   s_Preferences.iShowProcessesMonitor = 0;
   s_Preferences.iShowCPULoad = 0;
   s_Preferences.uEnabledAlarms = 0xFFFFFFFF;
   s_Preferences.uEnabledAlarms &= ~ (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING);
   s_Preferences.iShowOnlyPresentTxPowerCards = 1;
   s_Preferences.iShowTxBoosters = 0;
   s_Preferences.iMenuStyle = 0;
   s_Preferences.iStopRecordingAfterLinkLostSeconds = 20;

   s_Preferences.iDebugStatsQAButton = 0;
   s_Preferences.uDebugStatsFlags = CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS |
      //CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES |
      //CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM |
      //CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP |
      //CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME |
      //CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY |
      CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR |
      CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED |
      CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS |
      CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES;

   s_Preferences.iLanguage = 1;
   s_Preferences.iOSDFontBold = 0;
   s_Preferences.iMenuFontBold = 0;

   s_Preferences.iMSPOSDSize = 100;
   s_Preferences.iMSPOSDDeltaX = 0;
   s_Preferences.iMSPOSDDeltaY = 0;

   #if defined (HW_PLATFORM_RADXA)
   s_Preferences.iOSDFont = 1;
   s_Preferences.iOSDFontBold = 1;
   #endif

   s_Preferences.iShowCompactMenus = 1;
   s_Preferences.uEnabledQuickMenu = 0xFFFFFFFF;
}

int save_Preferences()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_UI_PREFERENCES);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save preferences to file: %s", szFile);
      return 0;
   }

   fprintf(fd, "%s\n", PREFERENCES_SETTINGS_STAMP_ID);
   
   fprintf(fd, "%d %d %d\n", s_Preferences.iMenusStacked, s_Preferences.iInvertColorsOSD, s_Preferences.iInvertColorsMenu); 
   fprintf(fd, "%d %d %d %d\n", s_Preferences.iOSDScreenSize, s_Preferences.iScaleOSD, s_Preferences.iScaleMenus, s_Preferences.iScaleAHI); 
   fprintf(fd, "%d %d %d\n", s_Preferences.iActionQuickButton1, s_Preferences.iActionQuickButton2, s_Preferences.iActionQuickButton3);
   fprintf(fd, "%d %d\n", s_Preferences.iAddOSDOnScreenshots, s_Preferences.iStatsToggledOff);
   fprintf(fd, "%d\n", s_Preferences.iShowLogWindow);

   fprintf(fd, "%d\n", s_Preferences.iOSDFlipVertical);
   fprintf(fd, "%d\n", s_Preferences.iMenuDismissesAlarm);

   fprintf(fd, "%d %d %d\n", s_Preferences.iVideoDestination, s_Preferences.iStartVideoRecOnArm, s_Preferences.iStopVideoRecOnDisarm);

   fprintf(fd, "%d\n", s_Preferences.iShowControllerCPUInfo);
   fprintf(fd, "%d\n", s_Preferences.iShowBigRecordButton);
   fprintf(fd, "%d %d\n", s_Preferences.iSwapUpDownButtons, s_Preferences.iSwapUpDownButtonsValues);
   fprintf(fd, "%d %d %d\n", s_Preferences.iAHIToSides, s_Preferences.iAHIShowAirSpeed, s_Preferences.iAHIStrokeSize);

   fprintf(fd, "%d\n", s_Preferences.iUnits);

   fprintf(fd, "%d %d\n", s_Preferences.iDebugMaxPacketSize, s_Preferences.iDebugSBWS);

   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorOSD[0], s_Preferences.iColorOSD[1], s_Preferences.iColorOSD[2], s_Preferences.iColorOSD[3]);
   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorAHI[0], s_Preferences.iColorAHI[1], s_Preferences.iColorAHI[2], s_Preferences.iColorAHI[3]);

   fprintf(fd, "%d\n", s_Preferences.iDebugRestartOnRadioSilence);

   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorOSDOutline[0], s_Preferences.iColorOSDOutline[1], s_Preferences.iColorOSDOutline[2], s_Preferences.iColorOSDOutline[3]);
   fprintf(fd, "%d\n", s_Preferences.iOSDOutlineThickness);
   fprintf(fd, "%d\n", s_Preferences.iOSDFont);
   fprintf(fd, "%d %d\n", s_Preferences.iRecordingLedAction, s_Preferences.iPersistentMessages);
   fprintf(fd, "%d\n", s_Preferences.nLogLevel);
   fprintf(fd, "%d %d\n", s_Preferences.iDebugShowDevVideoStats, s_Preferences.iDebugShowDevRadioStats);
   fprintf(fd, "%d\n", s_Preferences.iDebugShowFullRXStats);
   fprintf(fd, "%d\n", s_Preferences.iDebugWiFiChangeDelay);

   // Extra values

   fprintf(fd, "%d\n", s_Preferences.iDebugShowVideoSnapshotOnDiscard);
   fprintf(fd, "%d %d\n", 0, 0);

   fprintf(fd, "%d %d %d\n", s_Preferences.iAutoExportSettings, s_Preferences.iAutoExportSettingsWasModified, s_Preferences.iShowProcessesMonitor);

   fprintf(fd, "%u\n", s_Preferences.uEnabledAlarms);

   fprintf(fd, "%d \n", s_Preferences.iShowCPULoad);
   fprintf(fd, "%d %d %d\n", s_Preferences.iShowOnlyPresentTxPowerCards, s_Preferences.iShowTxBoosters, s_Preferences.iMenuStyle);
   fprintf(fd, "%d %u\n", s_Preferences.iDebugStatsQAButton, s_Preferences.uDebugStatsFlags);
   fprintf(fd, "%d\n", s_Preferences.iStopRecordingAfterLinkLostSeconds);
   fprintf(fd, "%d\n", s_Preferences.iLanguage);

   for( int i=0; i<MAX_PREFERENCES_CHECKBOXES; i++ )
   {
      fprintf(fd, "%d %d ", s_Preferences.iDoNotShowAgainIds[i], s_Preferences.iDoNotShowAgainValues[i]);
      if ( ((i%10) == 0) && (i != 0) )
         fprintf(fd, "\n");
   }
   fprintf(fd, "\n");
   fprintf(fd, "%d\n", s_Preferences.iUnitsHeight);
   fprintf(fd, "%d %d\n", s_Preferences.iOSDFontBold, s_Preferences.iMenuFontBold);
   fprintf(fd, "%d %d %d\n", s_Preferences.iMSPOSDSize, s_Preferences.iMSPOSDDeltaX, s_Preferences.iMSPOSDDeltaY);
   fprintf(fd, "%d\n", s_Preferences.iShowCompactMenus);
   fprintf(fd, "%u\n", s_Preferences.uEnabledQuickMenu);
   fclose(fd);
   log_line("Saved preferences to file: %s", szFile);
   return 1;
}

int load_Preferences()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_UI_PREFERENCES);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      reset_Preferences();
      log_softerror_and_alarm("Failed to load preferences from file: %s (missing file)", szFile);
      return 0;
   }

   int failed = 0;
   int iDummy1 = 0;
   int iDummy2 = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, PREFERENCES_SETTINGS_STAMP_ID) )
      failed = 1;

   if ( failed )
   {
      reset_Preferences();
      log_softerror_and_alarm("Failed to load preferences from file: %s (invalid config file version)", szFile);
      fclose(fd);
      return 0;
   }

   if ( 3 != fscanf(fd, "%d %d %d", &s_Preferences.iMenusStacked, &s_Preferences.iInvertColorsOSD, &s_Preferences.iInvertColorsMenu) )
      failed = 1;

   if ( 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iOSDScreenSize, &s_Preferences.iScaleOSD, &s_Preferences.iScaleMenus, &s_Preferences.iScaleAHI) )
      failed = 1;

   if ( 3 != fscanf(fd, "%d %d %d", &s_Preferences.iActionQuickButton1, &s_Preferences.iActionQuickButton2, &s_Preferences.iActionQuickButton3) )
      failed = 1;

   if ( s_Preferences.iActionQuickButton1 < 0 || s_Preferences.iActionQuickButton1 >= quickActionLast )
      s_Preferences.iActionQuickButton1 = quickActionCycleOSD;
   if ( s_Preferences.iActionQuickButton2 < 0 || s_Preferences.iActionQuickButton2 >= quickActionLast )
      s_Preferences.iActionQuickButton2 = quickActionTakePicture;
   if ( s_Preferences.iActionQuickButton3 < 0 || s_Preferences.iActionQuickButton3 >= quickActionLast )
      s_Preferences.iActionQuickButton3 = quickActionToggleAllOff;

   if ( 2 != fscanf(fd, "%d %d", &s_Preferences.iAddOSDOnScreenshots, &s_Preferences.iStatsToggledOff) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iShowLogWindow) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iOSDFlipVertical) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iMenuDismissesAlarm) )
      failed = 1;

   if ( (!failed) && 3 != fscanf(fd, "%d %d %d", &s_Preferences.iVideoDestination, &s_Preferences.iStartVideoRecOnArm, &s_Preferences.iStopVideoRecOnDisarm) )
      failed = 1;

   if ( (s_Preferences.iVideoDestination != prefVideoDestination_Disk) && (s_Preferences.iVideoDestination != prefVideoDestination_Mem) )
      s_Preferences.iVideoDestination = prefVideoDestination_Mem;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iShowControllerCPUInfo) )
      failed = 1;
   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iShowBigRecordButton) )
      failed = 1;

   if ( (!failed) && 2 != fscanf(fd, "%d %d", &s_Preferences.iSwapUpDownButtons, &s_Preferences.iSwapUpDownButtonsValues) )
      failed = 1;

   if ( (!failed) && 3 != fscanf(fd, "%d %d %d", &s_Preferences.iAHIToSides, &s_Preferences.iAHIShowAirSpeed, &s_Preferences.iAHIStrokeSize) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iUnits) )
      failed = 1;

   if ( (!failed) && 2 != fscanf(fd, "%d %d", &s_Preferences.iDebugMaxPacketSize, &s_Preferences.iDebugSBWS) )
      failed = 1;

   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorOSD[0], &s_Preferences.iColorOSD[1], &s_Preferences.iColorOSD[2], &s_Preferences.iColorOSD[3]) )
      failed = 1;
   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorAHI[0], &s_Preferences.iColorAHI[1], &s_Preferences.iColorAHI[2], &s_Preferences.iColorAHI[3]) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iDebugRestartOnRadioSilence) )
      failed = 1;

   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorOSDOutline[0], &s_Preferences.iColorOSDOutline[1], &s_Preferences.iColorOSDOutline[2], &s_Preferences.iColorOSDOutline[3]) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iOSDOutlineThickness) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iOSDFont) )
   {
      s_Preferences.iOSDFont = 1;
      failed = 1;
   }
   if ( (!failed) && (1 != fscanf(fd, "%d", &s_Preferences.iRecordingLedAction)) )
      { s_Preferences.iRecordingLedAction = 2; failed = 1; }

   if ( (!failed) && (1 != fscanf(fd, "%d", &s_Preferences.iPersistentMessages)) )
      { s_Preferences.iPersistentMessages = 1; failed = 1; }

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.nLogLevel) )
      failed = 1;

   int bOk = 1;
   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iDebugShowDevVideoStats, &s_Preferences.iDebugShowDevRadioStats) )
   {
      s_Preferences.iDebugShowDevVideoStats = 1;
      s_Preferences.iDebugShowDevRadioStats = 1;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugShowFullRXStats) )
   {
      s_Preferences.iDebugShowFullRXStats = 0;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugWiFiChangeDelay) )
   {
      s_Preferences.iDebugShowFullRXStats = 0;
      bOk = 0;
   }

   // Extra fields

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugShowVideoSnapshotOnDiscard) )
   {
      s_Preferences.iDebugShowVideoSnapshotOnDiscard = 0;
      bOk = 0;
   }

   if ( bOk && 2 != fscanf(fd, "%d %d", &iDummy1, &iDummy2) )
   {
      bOk = 0;
   }

   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iAutoExportSettings, &s_Preferences.iAutoExportSettingsWasModified) )
   {
      s_Preferences.iAutoExportSettings = 1;
      s_Preferences.iAutoExportSettingsWasModified = 0;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iShowProcessesMonitor) )
      s_Preferences.iShowProcessesMonitor = 0;

   if ( bOk && 1 != fscanf(fd, "%u", &s_Preferences.uEnabledAlarms) )
      s_Preferences.uEnabledAlarms = 0xFFFFFFFF;

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iShowCPULoad) )
      s_Preferences.iShowCPULoad = 0;

   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iShowOnlyPresentTxPowerCards, &s_Preferences.iShowTxBoosters) )
      s_Preferences.iShowTxBoosters = 0;

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iMenuStyle) )
      s_Preferences.iMenuStyle = 0;

   if ( bOk && 2 != fscanf(fd, "%d %u", &s_Preferences.iDebugStatsQAButton, &s_Preferences.uDebugStatsFlags) )
   {
      s_Preferences.iDebugStatsQAButton = 0;
      s_Preferences.uDebugStatsFlags = CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS |
         CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP |
         //CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME |
         CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY |
         CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iStopRecordingAfterLinkLostSeconds) )
   {
      s_Preferences.iStopRecordingAfterLinkLostSeconds = 20;
      bOk = 0;
   }

   if ( bOk && (1 != fscanf(fd, "%d", &s_Preferences.iLanguage)) )
   {
      s_Preferences.iLanguage = 1;
      bOk = 0;
   }
   if (! bOk )
      s_Preferences.iLanguage = 1;

   if ( bOk )
   {
      for( int i=0; i<MAX_PREFERENCES_CHECKBOXES; i++ )
      {
         fscanf(fd, "%d %d", &s_Preferences.iDoNotShowAgainIds[i], &s_Preferences.iDoNotShowAgainValues[i]);
      }
   }

   if ( bOk && (1 != fscanf(fd, "%d", &s_Preferences.iUnitsHeight)) )
   {
      s_Preferences.iUnitsHeight = prefUnitsMetric;
   }

   if ( bOk && (2 != fscanf(fd, "%d %d", &s_Preferences.iOSDFontBold, &s_Preferences.iMenuFontBold)) )
   {
      s_Preferences.iOSDFontBold = 0;
      s_Preferences.iMenuFontBold = 0;
   }
   
   if ( bOk && (3 != fscanf(fd, "%d %d %d", &s_Preferences.iMSPOSDSize, &s_Preferences.iMSPOSDDeltaX, &s_Preferences.iMSPOSDDeltaY)) )
   {
      s_Preferences.iMSPOSDSize = 100;
      s_Preferences.iMSPOSDDeltaX = 0;
      s_Preferences.iMSPOSDDeltaY = 0;
   }

   if ( bOk && (1 != fscanf(fd, "%d", &s_Preferences.iShowCompactMenus)) )
      s_Preferences.iShowCompactMenus = 1;

   if ( bOk && (1 != fscanf(fd, "%u", &s_Preferences.uEnabledQuickMenu)) )
      s_Preferences.uEnabledQuickMenu = 0xFFFFFFFF;

   // ----------------------------------------------------
   // End reading file;
   // Validate settings

   if ( s_Preferences.iColorOSD[3] == 0 )
   {
      s_Preferences.iColorOSD[0] = 255;
      s_Preferences.iColorOSD[1] = 250;
      s_Preferences.iColorOSD[2] = 220;
      s_Preferences.iColorOSD[3] = 100; // 100%
   }

   if ( s_Preferences.iColorOSDOutline[3] == 0 )
   {
      s_Preferences.iColorOSDOutline[0] = 185;
      s_Preferences.iColorOSDOutline[1] = 185;
      s_Preferences.iColorOSDOutline[2] = 155;
      s_Preferences.iColorOSDOutline[3] = 70; // 70%
   }

   if ( s_Preferences.iColorAHI[3] == 0 )
   {
      s_Preferences.iColorAHI[0] = 208;
      s_Preferences.iColorAHI[1] = 238;
      s_Preferences.iColorAHI[2] = 214;
      s_Preferences.iColorAHI[3] = 100; // 100%
   }

   if ( s_Preferences.iDebugMaxPacketSize < 100 ||
        s_Preferences.iDebugMaxPacketSize > MAX_VIDEO_PACKET_DATA_SIZE )
      s_Preferences.iDebugMaxPacketSize = MAX_VIDEO_PACKET_DATA_SIZE;

   if ( s_Preferences.iDebugSBWS < 2 || s_Preferences.iDebugSBWS > 16 )
      s_Preferences.iDebugSBWS = 2;

   if ( failed )
   {
      log_softerror_and_alarm("Failed to load preferences from file: %s (invalid config file)", szFile);
      fclose(fd);
      return 0;
   }
   if ( ! bOk )
      log_softerror_and_alarm("Some preferences settings where missing from file.");
   fclose(fd);
   log_line("Loaded preferences from file: %s", szFile);
   return 1;
}

Preferences* get_Preferences()
{
   return &s_Preferences;
}

int getPreferencesDoNotShowAgain(int iUniqueId)
{
   if ( iUniqueId <= 0 )
      return 0;

   for( int i=0; i<MAX_PREFERENCES_CHECKBOXES; i++ )
   {
      if ( s_Preferences.iDoNotShowAgainIds[i] == iUniqueId )
         return s_Preferences.iDoNotShowAgainValues[i];
   }
   return 0;
}

void setPreferencesDoNotShowAgain(int iUniqueId, int iDoNotShowAgain)
{
   if ( iUniqueId <= 0 )
      return;

   int iFirstEmptySlot = -1;
   for( int i=0; i<MAX_PREFERENCES_CHECKBOXES; i++ )
   {
      if ( -1 == iFirstEmptySlot )
      if ( 0 == s_Preferences.iDoNotShowAgainIds[i] )
         iFirstEmptySlot = i;

      if ( s_Preferences.iDoNotShowAgainIds[i] == iUniqueId )
      {
         s_Preferences.iDoNotShowAgainValues[i] = iDoNotShowAgain;
         return;
      }
   }
   if ( -1 != iFirstEmptySlot )
   {
      s_Preferences.iDoNotShowAgainIds[iFirstEmptySlot] = iUniqueId;
      s_Preferences.iDoNotShowAgainValues[iFirstEmptySlot] = iDoNotShowAgain;
   }
}

void removePreferencesDoNotShowAgain(int iUniqueId)
{
   if ( iUniqueId <= 0 )
      return;

   for( int i=0; i<MAX_PREFERENCES_CHECKBOXES; i++ )
   {
      if ( s_Preferences.iDoNotShowAgainIds[i] == iUniqueId )
      {
         s_Preferences.iDoNotShowAgainIds[i] = 0;
         s_Preferences.iDoNotShowAgainValues[i] = 0;
         return;
      }
   }
}

#else

int save_Preferences() { return 0; }
int load_Preferences() { return 0; }
void reset_Preferences() {}
Preferences* get_Preferences() { return NULL; }
int getPreferencesDoNotShowAgain(int iUniqueId) { return 0; }
void setPreferencesDoNotShowAgain(int iUniqueId, int iDoNotShowAgain){}
void removePreferencesDoNotShowAgain(int iUniqueId){}
#endif
