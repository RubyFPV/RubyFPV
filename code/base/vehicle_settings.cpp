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
#include "vehicle_settings.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "flags.h"

VehicleSettings s_VehicleSettings;
int s_VehicleSettingsLoaded = 0;

void reset_VehicleSettings()
{
   memset(&s_VehicleSettings, 0, sizeof(s_VehicleSettings));
   s_VehicleSettings.iDevRxLoopTimeout = DEFAULT_MAX_RX_LOOP_TIMEOUT_MILISECONDS_VEHICLE;

   log_line("Reseted vehicle settings.");
}

int save_VehicleSettings()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_VEHICLE_SETTINGS);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save vehicle settings to file: %s",szFile);
      return 0;
   }
   fprintf(fd, "%s\n", VEHICLE_SETTINGS_STAMP_ID);
   fprintf(fd, "%d\n", s_VehicleSettings.iDevRxLoopTimeout);
   fclose(fd);

   log_line("Saved vehicle settings to file: %s", szFile);
   return 1;
}

int load_VehicleSettings()
{
   s_VehicleSettingsLoaded = 1;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_VEHICLE_SETTINGS);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load vehicle settings from file: %s (missing file). Resetted vehicle settings to default.", szFile);
      reset_VehicleSettings();
      save_VehicleSettings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, VEHICLE_SETTINGS_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to load vehicle settings from file: %s (invalid config file version)", szFile);
      reset_VehicleSettings();
      save_VehicleSettings();
      return 0;
   }


   if ( 1 != fscanf(fd, "%d", &s_VehicleSettings.iDevRxLoopTimeout) )
   {
      s_VehicleSettings.iDevRxLoopTimeout = DEFAULT_MAX_RX_LOOP_TIMEOUT_MILISECONDS_VEHICLE;
      failed = 1;
   }

   fclose(fd);

   if ( failed )
   {
      log_line("Incomplete/Invalid settings file %s, error code: %d. Reseted to default.", szFile, failed);
      reset_VehicleSettings();
      save_VehicleSettings();
   }
   else
      log_line("Loaded vehicle settings from file: %s", szFile);
   return 1;
}

VehicleSettings* get_VehicleSettings()
{
   if ( ! s_VehicleSettingsLoaded )
      load_VehicleSettings();
   return &s_VehicleSettings;
}
