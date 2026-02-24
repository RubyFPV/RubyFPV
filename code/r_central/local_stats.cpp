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

#include "local_stats.h"
#include "../base/base.h"
#include "../base/models.h"
#include "../base/hardware_procs.h"
#include "shared_vars.h"
#include "pairing.h"
#include "timers.h"
#include "link_watch.h"

u32 s_stats_u32LastTimeSecondsCheck = 0;

void local_stats_reset_vehicle_index(int iVehicleIndex)
{
   if ( (iVehicleIndex < 0) || (iVehicleIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   g_VehiclesRuntimeInfo[iVehicleIndex].bIsArmed = false;
   g_VehiclesRuntimeInfo[iVehicleIndex].bHomeSet = false;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLat = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLon = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLastLat = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLastLon = 0.0;

   if ( 0 == g_VehiclesRuntimeInfo[iVehicleIndex].uVehicleId )
      return;
   Model* pModel = findModelWithId(g_VehiclesRuntimeInfo[iVehicleIndex].uVehicleId, 11);
   if ( NULL == pModel )
      return;

   pModel->m_Stats.uCurrentOnTime = 0;
   pModel->m_Stats.uCurrentFlightTime = 0; // seconds
   pModel->m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
   pModel->m_Stats.uCurrentFlightTotalCurrent = 0; // 0.1 miliAmps (1/10000 amps);

   pModel->m_Stats.uCurrentMaxAltitude = 0; // meters
   pModel->m_Stats.uCurrentMaxDistance = 0; // meters
   pModel->m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
   pModel->m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)
}

void local_stats_reset_all()
{
   log_line("Reseted all local stats for all vehicles.");

   s_stats_u32LastTimeSecondsCheck = g_TimeNow;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      local_stats_reset_vehicle_index(i);
}

void local_stats_on_arm(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         local_stats_reset_vehicle_index(i);
         g_VehiclesRuntimeInfo[i].bIsArmed = true;
         break;
      }
   }
}

void local_stats_on_disarm(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         if ( g_VehiclesRuntimeInfo[i].bIsArmed )
         if ( NULL != g_VehiclesRuntimeInfo[i].pModel )
            g_VehiclesRuntimeInfo[i].pModel->m_Stats.uTotalFlights++;
         g_VehiclesRuntimeInfo[i].bIsArmed = false;
         break;
      }
   }
}

void local_stats_update_loop()
{
   if ( (NULL == g_pCurrentModel) || (! pairing_isStarted()) )
      return;

   if ( ! g_bIsRouterReady )
      return;
   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
      return;


   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if (! g_VehiclesRuntimeInfo[i].bGotFCTelemetry )
         continue;
      if ( (0 == g_VehiclesRuntimeInfo[i].uVehicleId) || (MAX_U32 == g_VehiclesRuntimeInfo[i].uVehicleId) )
         continue;
      if ( NULL == g_VehiclesRuntimeInfo[i].pModel )
         continue;

      if ( g_VehiclesRuntimeInfo[i].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      if ( ! g_VehiclesRuntimeInfo[i].bIsArmed )
         g_VehiclesRuntimeInfo[i].bIsArmed = true;

      if ( g_VehiclesRuntimeInfo[i].bIsArmed )
         g_VehiclesRuntimeInfo[i].pModel->updateStatsMaxCurrentVoltage(&(g_VehiclesRuntimeInfo[i].headerFCTelemetry));

      // Every second

      if ( g_TimeNow >= s_stats_u32LastTimeSecondsCheck + 1000 )
         g_VehiclesRuntimeInfo[i].pModel->updateStatsEverySecond(&(g_VehiclesRuntimeInfo[i].headerFCTelemetry));
   }

   if ( g_TimeNow >= s_stats_u32LastTimeSecondsCheck + 1000 )
      s_stats_u32LastTimeSecondsCheck = g_TimeNow;
}

bool load_temp_local_stats()
{
   // Load and delete the file

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_TEMP_CONTROLLER_LOAD_LOCAL_STATS);
   if( access(szFile, R_OK) == -1 )
      return false;

   log_line("Loading temp local stats after a crash or reboot.");

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_TEMP_CONTROLLER_LOCAL_STATS);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load temporary local stats from file: %s (missing file)", szFile);
      char szComm[256];
      sprintf(szComm, "rm -rf %s%s 2>&1", FOLDER_CONFIG, FILE_TEMP_CONTROLLER_LOAD_LOCAL_STATS);
      hw_execute_bash_command(szComm, NULL);
      return false;
   }

   int failed = 0;
   int tmp1, tmp2;
   u32 uVehicleId = 0;
   u32 u1, u2, u3, u4;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (!failed) && (3 != fscanf(fd, "%u %d %d", &uVehicleId, &tmp1, &tmp2)) )
         failed = 1;
      if ( !failed )
      {
         g_VehiclesRuntimeInfo[i].bIsArmed = tmp1;
         g_VehiclesRuntimeInfo[i].bHomeSet = tmp2;
      }

      if ( (!failed) && (4 != fscanf(fd, "%lf %lf %lf %lf", &g_VehiclesRuntimeInfo[i].fHomeLat, &g_VehiclesRuntimeInfo[i].fHomeLon, &g_VehiclesRuntimeInfo[i].fHomeLastLat, &g_VehiclesRuntimeInfo[i].fHomeLastLon)) )
         failed = 1;

      Model* pModel = findModelWithId(uVehicleId, 12);
      if ( NULL == pModel )
      {
         if ( (!failed) && (2 != fscanf(fd, "%u %u", &u1, &u2)) )
            failed = 1;
         if ( (!failed) && (4 != fscanf(fd, "%u %u %u %u", &u1, &u2, &u3, &u4)) )
            failed = 1;
      }
      else
      {
         if ( (!failed) && (2 != fscanf(fd, "%u %u", &(pModel->m_Stats.uCurrentOnTime), &(pModel->m_Stats.uCurrentFlightTime))) )
            failed = 1;
         if ( (!failed) && (4 != fscanf(fd, "%u %u %u %u", &(pModel->m_Stats.uCurrentMaxDistance), &(pModel->m_Stats.uCurrentMaxAltitude), &(pModel->m_Stats.uCurrentMaxCurrent), &(pModel->m_Stats.uCurrentMinVoltage))) )
            failed = 1;
      }

      if ( failed )
      {
         local_stats_reset_all();
         break;
      }
   }

   fclose(fd);

   if ( failed )
      log_line("Incomplete/Invalid temporary local stats file %s", szFile);
   else
      log_line("Loaded temporary local stats from file: %s", szFile);

   char szComm[128];
   sprintf(szComm, "rm -rf %s%s 2>&1", FOLDER_CONFIG, FILE_TEMP_CONTROLLER_LOAD_LOCAL_STATS);
   hw_execute_bash_command(szComm, NULL);

   if ( failed )
      return false;
   return true;
}

void save_temp_local_stats()
{
   if ( NULL == g_pCurrentModel )
      return;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      saveControllerModel(g_VehiclesRuntimeInfo[i].pModel);

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_TEMP_CONTROLLER_LOCAL_STATS);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save temporary local stats to file: %s", szFile);
      return;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      Model* pModel = g_VehiclesRuntimeInfo[i].pModel;

      fprintf(fd, "%u %d %d\n", g_VehiclesRuntimeInfo[i].uVehicleId, (int)g_VehiclesRuntimeInfo[i].bIsArmed, (int)g_VehiclesRuntimeInfo[i].bHomeSet);
      fprintf(fd, "%f %f %f %f\n", g_VehiclesRuntimeInfo[i].fHomeLat, g_VehiclesRuntimeInfo[i].fHomeLon, g_VehiclesRuntimeInfo[i].fHomeLastLat, g_VehiclesRuntimeInfo[i].fHomeLastLon);
      if ( NULL == pModel )
      {
         fprintf(fd, "%u %u\n", 0, 0);
         fprintf(fd, "%u %u %u %u\n", 0, 0, 0, 0);
      }
      else
      {
         fprintf(fd, "%u %u\n", pModel->m_Stats.uCurrentOnTime, pModel->m_Stats.uCurrentFlightTime);
         fprintf(fd, "%u %u %u %u\n", pModel->m_Stats.uCurrentMaxDistance, pModel->m_Stats.uCurrentMaxAltitude, pModel->m_Stats.uCurrentMaxCurrent, pModel->m_Stats.uCurrentMinVoltage);
      }
   }
   fclose(fd);
   log_line("Saved temp local stats for reboot or watchdog.");
}
