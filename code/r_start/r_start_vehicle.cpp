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
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>
#include <dirent.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/encr.h"
#include "../base/models.h"
#include "../base/hardware.h"
#include "../base/hardware_audio.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_files.h"
#include "../base/hardware_procs.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../base/tx_powers.h"
#include "../common/string_utils.h"
#include "../r_vehicle/launchers_vehicle.h"
#include "../r_vehicle/video_sources.h"
#include "../r_vehicle/shared_vars.h"
#include "../r_vehicle/timers.h"
#include "../r_vehicle/hw_config_check.h"
#include "r_start_vehicle.h"

extern bool s_bQuit;
extern Model modelVehicle;

sem_t* g_pSemaphoreRestart = NULL;
shared_mem_process_stats* s_pProcessStatsRouter = NULL;
shared_mem_process_stats* s_pProcessStatsTelemetry = NULL;
shared_mem_process_stats* s_pProcessStatsCommands = NULL;
shared_mem_process_stats* s_pProcessStatsRC = NULL;

u32 s_failCountProcessThreshold = 5;
u32 s_failCountProcessRouter = 0;
u32 s_failCountProcessTelemetry = 0;
u32 s_failCountProcessCommands = 0;
u32 s_failCountProcessRC = 0;

int s_iRadioSilenceFailsafeTimeoutCounts = 0;

char szFileUpdate[MAX_FILE_PATH_SIZE];
char szFileReinitRadio[MAX_FILE_PATH_SIZE];
bool s_bUpdateDetected = false;

void _set_default_sik_params_for_vehicle(Model* pModel)
{
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Can't set default SiK radio params for NULL model.");
      return;
   }
   log_line("Setting default SiK radio params for all SiK interfaces for current model...");
   int iCountSiKInterfaces = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( hardware_radio_index_is_sik_radio(i) )
      {
         iCountSiKInterfaces++;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
         {
            log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", i+1);
            continue;
         }
         int iLinkIndex = pModel->radioInterfacesParams.interface_link_id[i];
         if ( (iLinkIndex >= 0) && (iLinkIndex < pModel->radioLinksParams.links_count) )
         {
            u32 uFreq = pModel->radioLinksParams.link_frequency_khz[iLinkIndex];
            hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
               uFreq, pModel->radioInterfacesParams.interface_raw_power[i],
               pModel->radioLinksParams.downlink_datarate_data_bps[iLinkIndex],
               (u32)((pModel->radioLinksParams.link_radio_flags[iLinkIndex] & RADIO_FLAGS_SIK_ECC)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLinkIndex] & RADIO_FLAGS_SIK_LBT)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLinkIndex] & RADIO_FLAGS_SIK_MCSTR)?1:0),
               NULL);
         }
      }
   }
   log_line("Setting default SiK radio params for all SiK interfaces (%d SiK interfaces) for current model. Done.", iCountSiKInterfaces);
}

bool _check_radio_config_relay(Model* pModel)
{
   if ( NULL == pModel )
      return false;
   bool bMustSave = false;

   if ( pModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAYED_NODE )
   {
      pModel->relay_params.uCurrentRelayMode = 0;
      bMustSave = true;
   }

   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   {
      if ( ! (pModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) )
      {
         log_line("Start sequence: Relaying was active. Switch to relaying the main vehicle on start up.");
         pModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
         bMustSave = true;
      }
   }

   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   {
      bool bInvalidConfig = false;
      int iInvalidLinkConfig = -1;
      for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
      {
         if ( pModel->radioLinksParams.link_frequency_khz[i] == pModel->relay_params.uRelayFrequencyKhz )
         if ( i != pModel->relay_params.isRelayEnabledOnRadioLinkId )
         {
            iInvalidLinkConfig = i;
            bInvalidConfig = true;
            break;
         }
      }
      if ( bInvalidConfig )
      {
         log_softerror_and_alarm("Start sequence: Invalid relaying configuration: relay link frequency %s is the same as frequency for regular radio link %d. Reset and disabling relaying.", str_format_frequency(pModel->relay_params.uRelayFrequencyKhz), iInvalidLinkConfig+1 );
         pModel->resetRelayParamsToDefaults(&(pModel->relay_params));
         bMustSave = true;
      }
      if ( (1 == hardware_get_radio_interfaces_count()) || (1 == pModel->radioLinksParams.links_count) )
      {
         log_softerror_and_alarm("Start sequence: Invalid relaying configuration: Only one radio interface or radio link present. Reset and disabling relaying." );
         pModel->resetRelayParamsToDefaults(&(pModel->relay_params));
         bMustSave = true;
      }
   }

   return bMustSave;
}

bool _check_radio_config(Model* pModel)
{
   if ( NULL == pModel )
      return false;

   bool bMustSave = false;

   log_line("===================================================");
   log_line("Checking vehicle radio hardware configuration...");

   log_line("[HW Radio Check] Full radio configuration before doing any changes:");
   log_full_current_radio_configuration(pModel);
   
   if ( check_update_hardware_nics_vehicle(pModel) || recheck_disabled_radio_interfaces(pModel) )
   {
      pModel->radioRuntimeCapabilities.uFlagsRuntimeCapab &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      pModel->radioLinksParams.uGlobalRadioLinksFlags &= ~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;

      log_line("[HW Radio Check] Hardware radio interfaces configuration check complete and configuration was changed. This is the new hardware radio interfaces and radio links configuration:");
      log_full_current_radio_configuration(pModel);
      _set_default_sik_params_for_vehicle(pModel);
      bMustSave = true;
   }
   else
      log_line("[HW Radio Check] No radio hardware configuration change detected. No change.");

   for( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
   {
      int iCardModel = modelVehicle.radioInterfacesParams.interface_card_model[i];
      int iMaxPowerRaw = tx_powers_get_max_usable_power_raw_for_card(modelVehicle.hwCapabilities.uBoardType, iCardModel);

      if ( modelVehicle.radioInterfacesParams.interface_raw_power[i] > iMaxPowerRaw )
      {
         modelVehicle.radioInterfacesParams.interface_raw_power[i] = iMaxPowerRaw;
         bMustSave = true;
      }
   }

   if ( pModel->check_update_radio_links() )
   {
      _set_default_sik_params_for_vehicle(pModel);
      bMustSave = true;
   }

   // Check & update radio card models and high capacity flags

   log_line("Start sequence: Checking radio interfaces cards models. %d hardware radio cards, %d model radio cards...", hardware_get_radio_interfaces_count(), pModel->radioInterfacesParams.interfaces_count);
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
      log_line("Start sequence: Vehicle model radio interface %d: MAC: %s, model: [%s]", i+1, pModel->radioInterfacesParams.interface_szMAC[i], str_get_radio_card_model_string(pModel->radioInterfacesParams.interface_card_model[i]));
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         log_softerror_and_alarm("Start sequence: Failed to get hardware radio card info for card %d.", i+1);
         continue;
      }
      log_line("Start sequence: Hardware radio interface %d: %s, model: [%s]", i+1, pRadioInfo->szMAC, str_get_radio_card_model_string(pRadioInfo->iCardModel));
   }
   
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
       radio_hw_info_t* pRadioInfo = hardware_get_radio_info_from_mac(pModel->radioInterfacesParams.interface_szMAC[i]);
       if ( NULL == pRadioInfo )
       {
          log_softerror_and_alarm("Start sequence: Failed to get radio card info for card %d with MAC: [%s].", i+1, pModel->radioInterfacesParams.interface_szMAC[i]);
          continue;
       }

       if ( pRadioInfo->isHighCapacityInterface )
       {
          pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
       }
       else
       {
          pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~(RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY);
          pModel->radioInterfacesParams.interface_capabilities_flags[i] &= ~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
       }
       // If user defined, do not change it
       if ( pModel->radioInterfacesParams.interface_card_model[i] < 0 )
       {
          log_line("Start sequence: Radio interface %d model was set by user to [%s]. Do not change it (to [%s]).", i+1, str_get_radio_card_model_string(-pModel->radioInterfacesParams.interface_card_model[i]), str_get_radio_card_model_string(pRadioInfo->iCardModel) );
          continue;
       }
       char szModel[128];
       strcpy(szModel, str_get_radio_card_model_string(pRadioInfo->iCardModel));
       log_line("Start sequence: Hardware radio interface %d type: [%s]. Current model radio interface %d type: [%s].", i+1, szModel, i+1, str_get_radio_card_model_string(pModel->radioInterfacesParams.interface_card_model[i]));
       if ( modelVehicle.radioInterfacesParams.interface_card_model[i] != pRadioInfo->iCardModel )
       {
          log_line("Start sequence: Updating model radio type.");
          pModel->radioInterfacesParams.interface_card_model[i] = pRadioInfo->iCardModel;
          bMustSave = true;
       }
   }

   // Check for radio links on same frequency
   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   for( int k=i+1; k<pModel->radioLinksParams.links_count; k++ )
   {
      if ( pModel->radioLinksParams.link_frequency_khz[i] != pModel->radioLinksParams.link_frequency_khz[k] )
         continue;

      log_softerror_and_alarm("Vehicle radio link %d and %d has same frequency: %s. Changing one of them.",
         i+1, k+1, str_format_frequency(pModel->radioLinksParams.link_frequency_khz[i]));

      if ( getBand(pModel->radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_58 )
      {
         if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY58 )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY58;
         else if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY58_2 )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY58_2;
         if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY58_3 )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY58_3;
      }
      else
      {
         if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY;
         else if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY_2 )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY_2;
         if ( pModel->radioLinksParams.link_frequency_khz[k] != DEFAULT_FREQUENCY_3 )
            pModel->radioLinksParams.link_frequency_khz[k] = DEFAULT_FREQUENCY_3;
      }

      bMustSave = true;
   }

   bMustSave |= _check_radio_config_relay(pModel);

   bMustSave |= pModel->validateRadioSettings();
   log_line("Checking vehicle radio hardware configuration complete. Was updated and must save? %s", bMustSave?"yes":"no");
   log_line("========================================================================");

   return bMustSave;
}

void try_open_process_stats()
{
   if ( NULL == s_pProcessStatsRouter )
   {
      s_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_TX);
      if ( NULL == s_pProcessStatsRouter )
         log_softerror_and_alarm("Failed to open shared mem to router process watchdog for reading: %s", SHARED_MEM_WATCHDOG_ROUTER_TX);
      else
         log_line("Opened shared mem to router process watchdog for reading.");
   }
   if ( NULL == s_pProcessStatsTelemetry )
   {
      s_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      if ( NULL == s_pProcessStatsTelemetry )
         log_softerror_and_alarm("Failed to open shared mem to telemetry Tx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      else
         log_line("Opened shared mem to telemetry Tx process watchdog for reading.");
   }
   if ( NULL == s_pProcessStatsCommands )
   {
      s_pProcessStatsCommands = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_COMMANDS_RX);
      if ( NULL == s_pProcessStatsCommands )
         log_softerror_and_alarm("Failed to open shared mem to commands Rx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_COMMANDS_RX);
      else
         log_line("Opened shared mem to commands Rx process watchdog for reading.");
   }

   if ( modelVehicle.rc_params.rc_enabled )
   if ( NULL == s_pProcessStatsRC )
   {
      s_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_RX);
      if ( NULL == s_pProcessStatsRC )
         log_softerror_and_alarm("Failed to open shared mem to RC Rx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_RC_RX);
      else
         log_line("Opened shared mem to RC Rx process watchdog for reading.");
   }
}

  
void _launch_vehicle_processes(bool bWait)
{
   hardware_sleep_ms(100);
   vehicle_launch_tx_telemetry(&modelVehicle);
   
   hardware_sleep_ms(100);
   vehicle_launch_rx_commands(&modelVehicle);

   if ( modelVehicle.rc_params.rc_enabled )
   {
      hardware_sleep_ms(100);
      vehicle_launch_rx_rc(&modelVehicle);
   }

   hardware_sleep_ms(100);

   vehicle_launch_tx_router(&modelVehicle);

   log_line("Launch sequence: done launching router. Wait at end? %s", bWait?"yes":"no");

   // Wait for all the processes to start (takes time for I2C detection)
   if ( bWait )
   {
      for( int i=0; i<10; i++ )
         hardware_sleep_ms(500);
   }
}

void _restart_procs(bool bStopRouterToo)
{
   log_softerror_and_alarm("----------------------------------------------------");
   log_softerror_and_alarm("Stop and restart processes (stop router too? %s)...", bStopRouterToo?"yes":"no");
   log_softerror_and_alarm("----------------------------------------------------");
   
   vehicle_stop_rx_commands();
   vehicle_stop_rx_rc();
   vehicle_stop_tx_telemetry();
   if ( bStopRouterToo )
      vehicle_stop_tx_router();

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, s_pProcessStatsRouter);
   s_pProcessStatsRouter = NULL;
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_TX, s_pProcessStatsTelemetry);
   s_pProcessStatsTelemetry = NULL;
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_COMMANDS_RX, s_pProcessStatsCommands);
   s_pProcessStatsCommands = NULL;
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_RX, s_pProcessStatsRC);
   s_pProcessStatsRC = NULL;

   bool bHasProcesses = true;
   int iCounter = 50;
   char szOutput[4096];

   while ( bHasProcesses && (iCounter > 0) )
   {
      hardware_sleep_ms(200);
      iCounter--;
      bHasProcesses = false;
      if ( hw_process_exists("ruby_rt_vehicle") )
         bHasProcesses = true;
      if ( hw_process_exists("ruby_tx_telemetry") )
         bHasProcesses = true;

      szOutput[0] = 0;
      hw_execute_bash_command("ps -aef | grep ruby | grep rx_com", szOutput);
      if ( NULL != strstr(szOutput, "ruby_start -rx_com") )
         bHasProcesses = true;

      szOutput[0] = 0;
      hw_execute_bash_command("ps -aef | grep ruby | grep rc", szOutput);
      if ( NULL != strstr(szOutput, "ruby_start -rc") )
         bHasProcesses = true;

      if ( video_sources_is_caputure_process_running() )
         bHasProcesses = true;
   }


   hw_execute_bash_command_raw("ps -aef | grep ruby", szOutput);
   log_line("Processes ps ruby output: [%s]", szOutput);

   hw_execute_bash_command_raw("ps -aef | grep majestic", szOutput);
   log_line("Processes ps maj output: [%s]", szOutput);

   log_softerror_and_alarm("----------------------------------------------");
   if ( bHasProcesses )
      log_softerror_and_alarm("Requested to stop all processes but some are still running.");
   else
      log_softerror_and_alarm("Stopped all processes successfully.");
   log_softerror_and_alarm("----------------------------------------------");

   if ( hw_process_exists("sysupgrade") )
   {
      log_softerror_and_alarm("Sysupgrade is in progress. Do not restart Ruby processes.");
      s_bQuit = true;
      return;
   }

   sem_unlink(SEMAPHORE_STOP_VEHICLE_ROUTER);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_COMMANDS);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_TELEM_TX);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_RC_RX);

   if ( NULL != g_pSemaphoreRestart )
       sem_close(g_pSemaphoreRestart);
   g_pSemaphoreRestart = NULL;
   sem_unlink(SEMAPHORE_RESTART_VEHICLE_PROCS);

   g_pSemaphoreRestart = sem_open(SEMAPHORE_RESTART_VEHICLE_PROCS, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == g_pSemaphoreRestart) || (SEM_FAILED == g_pSemaphoreRestart) )
   {
      log_error_and_alarm("Failed to open semaphore: %s", SEMAPHORE_RESTART_VEHICLE_PROCS);
      g_pSemaphoreRestart = NULL;
   } 

   modelVehicle.reloadIfChanged(true);

   hardware_sleep_ms(100);
   log_softerror_and_alarm("----------------------");
   log_softerror_and_alarm("Launching processes...");
   _launch_vehicle_processes(true);

   log_softerror_and_alarm("--------------------------------------------");
   log_softerror_and_alarm("Restarting processes: Done. Wait a little...");
   log_softerror_and_alarm("--------------------------------------------");

   for( int i=0; i<5; i++ )
      hardware_sleep_ms(300);

   s_failCountProcessRouter = 0;
   s_failCountProcessTelemetry = 0;
   s_failCountProcessCommands = 0;
   s_failCountProcessRC = 0;
   g_TimeNow = get_current_timestamp_ms();
   g_TimeLastCheckRadioSilenceFailsafe = g_TimeNow;

   log_softerror_and_alarm("-------------------------------");
   log_softerror_and_alarm("Restarting processes: Finished.");
   log_softerror_and_alarm("-------------------------------");
}

// returns true if values changed

bool read_config_file()
{
   #ifdef HW_PLATFORM_RASPBERRY
   int f = config_file_get_value("arm_freq");
   int g = config_file_get_value("gpu_freq");
   int v = config_file_get_value("over_voltage");

   log_line("Read Pi config file: overvoltage: %d, arm freq: %d, gpu freq: %d", v,f,g);
   if ( (f != modelVehicle.processesPriorities.iFreqARM) || (g != modelVehicle.processesPriorities.iFreqGPU) || (v != modelVehicle.processesPriorities.iOverVoltage) )
   {
      modelVehicle.processesPriorities.iFreqARM = f;
      modelVehicle.processesPriorities.iFreqGPU = g;
      modelVehicle.processesPriorities.iOverVoltage = v;
      log_line("Read Pi config file values changed. Will update model file.");
      return true;
   }
   return false;
   #endif
   return true;
}

bool _check_restrict_files()
{
   if ( access(szFileUpdate, R_OK) != -1 )
   {
      s_bUpdateDetected = true;
      return false;
   }
   if ( access(szFileReinitRadio, R_OK) != -1 )
      return false;

   return true;
}

void handle_sigint_veh(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   s_bQuit = true;
} 

int r_start_vehicle(int argc, char *argv[])
{
   log_init("RubyVehicle");
   log_arguments(argc, argv);

   bool noWatchDog = false;
   if ( argc > 1 )
   if ( 0 == strcmp(argv[1], "-nowd") )
      noWatchDog = true;

   signal(SIGINT, handle_sigint_veh);
   signal(SIGTERM, handle_sigint_veh);
   signal(SIGQUIT, handle_sigint_veh);

   char szBuff[2048];
   char szFile[128];
   check_licences();

   hardware_detectBoardAndSystemType();
   hardware_enumerate_radio_interfaces();
   int retry = 0;
   while ( 0 == hardware_get_radio_interfaces_count() && retry < 30 )
   {
      hardware_sleep_ms(200);
      hardware_enumerate_radio_interfaces();
      retry++;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("There are no radio interfaces (NICS/wlans) on this device.");
      return -1;
   }
   
   ruby_clear_all_ipc_channels();

   u32 uBoardType = 0;
   char szBoardId[256];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_BOARD_TYPE);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%u %s", &uBoardType, szBoardId);
      fclose(fd);
   }
   log_line("Start sequence: Board type: %u -> %s", uBoardType, str_get_hardware_board_name(uBoardType));


   sprintf(szBuff, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ALARM_ON);
   hw_execute_bash_command_silent(szBuff, NULL);
 
   bool bMustSave = false;

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! modelVehicle.loadFromFile(szFile, true) )
   {
      modelVehicle.resetToDefaults(true);
      modelVehicle.is_spectator = false;
      bMustSave = true;
   }

   log_line("Start sequence: Loaded model. Developer flags: live log: %s, enable radio silence failsafe: %s, log only errors: %s, radio config guard interval: %d ms",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?"yes":"no",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?"yes":"no",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)?"yes":"no",
          (int)((modelVehicle.uDeveloperFlags >> DEVELOPER_FLAGS_WIFI_GUARD_DELAY_MASK_SHIFT) & 0xFF) );
   log_line("Start sequence: Model has vehicle developer mode flag: %s", (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");
   log_line("Start sequence: Model processes flags: [%s]", str_format_processes_flags(modelVehicle.processesPriorities.uProcessesFlags));
   log_line("Start sequence: Initial vehicle radio links configuration before any radio checks:");
   modelVehicle.logVehicleRadioInfo();
   g_pCurrentModel = &modelVehicle;

   if ( modelVehicle.uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
   {
      log_line("Log is disabled on vehicle. Disabled logs.");
      log_disable();
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_VEHICLE_REBOOT_CACHE);
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("Start sequence: No cached telemetry info, reset current vehicle stats.");
      modelVehicle.m_Stats.uCurrentOnTime = 0;
      modelVehicle.m_Stats.uCurrentFlightTime = 0;
      modelVehicle.m_Stats.uCurrentFlightDistance = 0;
      modelVehicle.m_Stats.uCurrentFlightTotalCurrent = 0;
      modelVehicle.m_Stats.uCurrentTotalCurrent = 0;
      modelVehicle.m_Stats.uCurrentMaxAltitude = 0;
      modelVehicle.m_Stats.uCurrentMaxDistance = 0;
      modelVehicle.m_Stats.uCurrentMaxCurrent = 0;
      modelVehicle.m_Stats.uCurrentMinVoltage = 100000;
      bMustSave = true;
   }

   if ( read_config_file() )
      bMustSave = true;

   if ( access( "/boot/nodhcp", R_OK ) == -1 )
   {
       if ( ! modelVehicle.enableDHCP )
          bMustSave = true;
       modelVehicle.enableDHCP = true;
   }
   else
   {
       if ( modelVehicle.enableDHCP )
          bMustSave = true;
       modelVehicle.enableDHCP = false;
   }    
   uBoardType = hardware_getBoardType();
   if ( (uBoardType & BOARD_TYPE_MASK) != (modelVehicle.hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
   {
      modelVehicle.hwCapabilities.uBoardType = uBoardType;
      bMustSave = true;
   }

   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   if ( ((modelVehicle.hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_UNKNOWN )
   if ( hardware_radio_has_rtl8733bu_cards() )
   {
      modelVehicle.hwCapabilities.uBoardType = BOARD_TYPE_OPENIPC_SIGMASTAR_338Q | (BOARD_SUBTYPE_OPENIPC_AIO_THINKER << BOARD_SUBTYPE_SHIFT);
      bMustSave = true;
   }
   u32 alarmsOriginal = modelVehicle.alarms;

   // Check the file system for write access
   
   log_line("Start sequence: Checking the file system for write access...");

   modelVehicle.alarms &= ~(ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR);
   if ( check_write_filesystem() < 0 )
      modelVehicle.alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;

   if ( modelVehicle.alarms & ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR )
      log_line("Start sequence: Checking the file system for write access: Failed.");
   else
      log_line("Start sequence: Checking the file system for write access: Succeeded.");

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   hw_execute_bash_command("ulimit -i 1024", NULL);
   hw_execute_bash_command("ulimit -l 1024", NULL);
   #endif

   // Detect serial UARTs

   hardware_serial_init_ports();

   u32 telemetry_flags = modelVehicle.telemetry_params.flags;
   
   modelVehicle.alarms &= (~ALARM_ID_UNSUPORTED_USB_SERIAL);

   if ( hardware_serial_has_unsupported_ports() )
      modelVehicle.alarms |= ALARM_ID_UNSUPORTED_USB_SERIAL;
   
   if ( 0 == hardware_serial_get_ports_count() )
   {
      if ( modelVehicle.telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
         bMustSave = true;
      modelVehicle.telemetry_params.fc_telemetry_type = TELEMETRY_TYPE_NONE;
   }
   
   log_line("Start sequence: Rechecking model serial ports for changes based on current hardware serial ports...");
   if ( modelVehicle.populateVehicleSerialPorts() )
      bMustSave = true;
   log_line("Start sequence: Done rechecking serial ports for changes.");

   if ( alarmsOriginal != modelVehicle.alarms )
      bMustSave = true;
   if ( telemetry_flags != modelVehicle.telemetry_params.flags )
      bMustSave = true;

   // Detect audio cards

   log_line("Start sequence: Detecting audio devices...");
   bool bHadAudioDevice = modelVehicle.audio_params.has_audio_device;

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command_raw( "arecord -l 2>&1 | grep card | grep USB", szBuff );
   if ( 0 < strlen(szBuff) && NULL != strstr(szBuff, "USB") )
   {
      modelVehicle.audio_params.has_audio_device = true;
      modelVehicle.audio_params.uFlags &= ~((u32)0x03);
      modelVehicle.audio_params.uFlags |= (u32)0x02;
   }
   else
   {
      modelVehicle.audio_params.has_audio_device = false;
      modelVehicle.audio_params.uFlags &= ~((u32)0x03);
      modelVehicle.audio_params.enabled = false;
   }
   #endif

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   
   modelVehicle.audio_params.has_audio_device = true;
   if ( hardware_board_has_audio_builtin(modelVehicle.hwCapabilities.uBoardType) )
   {
      modelVehicle.audio_params.uFlags &= ~((u32)0x03);
      modelVehicle.audio_params.uFlags |= (u32)0x01;
   }
   #endif

   log_line("Start sequence: Finished detecting audio device for board %s: %s", str_get_hardware_board_name(modelVehicle.hwCapabilities.uBoardType), modelVehicle.audio_params.has_audio_device?"Audio capture device found.":"No audio capture devices found.");

   if ( bHadAudioDevice != modelVehicle.audio_params.has_audio_device )
      bMustSave = true;

   // Validate camera/video settings

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("Start sequence: Validate majestic config file...");
   if ( 0 != hardware_camera_maj_validate_config() )
      return 0;
   log_line("Start sequence: Done validating majestic config file.");

   if ( 0 != modelVehicle.camera_params[modelVehicle.iCurrentCamera].szCameraBinProfileName[0] )
   {
      modelVehicle.camera_params[modelVehicle.iCurrentCamera].szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;
      log_line("Start sequence: Validate majestic calibration file: [%s]...", modelVehicle.camera_params[modelVehicle.iCurrentCamera].szCameraBinProfileName );
      char szCalibrationFile[MAX_FILE_PATH_SIZE];
      strcpy(szCalibrationFile, "/etc/sensors/c_");
      strcat(szCalibrationFile, modelVehicle.camera_params[modelVehicle.iCurrentCamera].szCameraBinProfileName);
      if ( NULL == strstr(szCalibrationFile, ".bin") )
         strcat(szCalibrationFile, ".bin");

      if ( access(szCalibrationFile, R_OK) == -1 )
      {
         int iCamType = modelVehicle.getActiveCameraType();
         log_line("Start sequence: Calibration file is missing. Set default calibration for camera type: %d (%s).", iCamType, str_get_hardware_camera_type_string(iCamType));
         modelVehicle.camera_params[modelVehicle.iCurrentCamera].iCameraBinProfile = 0;
         modelVehicle.camera_params[modelVehicle.iCurrentCamera].szCameraBinProfileName[0] = 0;
         hardware_camera_set_default_oipc_calibration(iCamType);
         bMustSave = true;
      }
      else
         log_line("Start sequence: Calibration file is ok.");
   }
   #endif

   log_line("Start sequence: Validate camera settings...");
   if ( modelVehicle.find_and_validate_camera_settings() )
      bMustSave = true;

   // Validate encryption settings
   if ( modelVehicle.enc_flags != MODEL_ENC_FLAGS_NONE )
   {
      if ( ! lpp(NULL, 0) )
      {
         modelVehicle.enc_flags = MODEL_ENC_FLAGS_NONE;
         bMustSave = true;
      }
   }

   // Check logger service flag change

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);

   if ( access(szFile, R_OK) != -1 )
   {
      if ( ! (modelVehicle.uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE) )
      {
         modelVehicle.uModelFlags |= MODEL_FLAG_USE_LOGER_SERVICE;
         bMustSave = true;
      }
   }
   else
   {
      if ( modelVehicle.uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE )
      {
         modelVehicle.uModelFlags &= ~MODEL_FLAG_USE_LOGER_SERVICE;
         bMustSave = true;
      }
   }

   bMustSave |= _check_radio_config(&modelVehicle);

   log_line("Start sequence: Vehicle radio links configuration after the radio checks:");
   modelVehicle.logVehicleRadioInfo();

   log_line("Start sequence: Setting all the radio cards params (%s)...", (modelVehicle.relay_params.isRelayEnabledOnRadioLinkId >= 0)?"relaying enabled":"no relay links");

   bMustSave = true;
   log_line("Start sequence: Configured all the radio cards params.");

   log_current_full_radio_configuration(&modelVehicle);

   // Update model settings to disk

   if ( bMustSave )
   {
      log_line("Start sequence: Updating local model file.");
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      modelVehicle.saveToFile(szFile, false);
   }

   for( int i=0; i<modelVehicle.hardwareInterfacesInfo.serial_port_count; i++ )
   {
      if ( modelVehicle.hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
      if ( (modelVehicle.hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) != SERIAL_PORT_USAGE_NONE )
      {
         int iPortId = ( modelVehicle.hardwareInterfacesInfo.serial_port_supported_and_usage[i] >> 8 ) & 0x0F;
         // USB serial
         if ( modelVehicle.hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_EXTRNAL_USB )
         {
            char szPort[32];
            sprintf(szPort, "/dev/ttyUSB%d", iPortId);
            hardware_configure_serial(szPort, (long)modelVehicle.hardwareInterfacesInfo.serial_port_speed[i]);
         }
         // Hardware serial
         else
         {
            char szPort[32];
            sprintf(szPort, "/dev/serial%d", iPortId);
            hardware_configure_serial(szPort, (long)modelVehicle.hardwareInterfacesInfo.serial_port_speed[i]);
         }
      }
   }

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_ruby_process(NULL, "ruby_alive", NULL, NULL);
   #endif
   
   log_line("Launching processes...");
   _launch_vehicle_processes(true);

   log_line("");
   log_line("----------------------------------------------------------");
   log_line("Finished launching processes. Switching to watchdog state.");
   log_line("----------------------------------------------------------");
   log_line("");

   log_line("Switch to watchdog state after a delay from start.");
   
   if ( noWatchDog )
      log_line_watchdog("Watchdog is disabled");
   else
      log_line_watchdog("Watchdog state started.");

   if ( modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   g_TimeStart = get_current_timestamp_ms();
   g_TimeLastCheckRadioSilenceFailsafe = get_current_timestamp_ms();
   for( int i=0; i<10; i++ )
      hardware_sleep_ms(500);

   u32 maxTimeForProcessMs = 500;
   char szTime[64];
   char szTime2[64];
   char szTime3[64];
   int counter = 0;
   bool bMustRestart = false;
   int iRestartCount = 0;
   u32 uTimeLoopLog = g_TimeStart;

   strcpy(szFileUpdate, FOLDER_RUBY_TEMP);
   strcat(szFileUpdate, FILE_TEMP_UPDATE_IN_PROGRESS);

   strcpy(szFileReinitRadio, FOLDER_RUBY_TEMP);
   strcat(szFileReinitRadio, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);

   char szFileArmed[MAX_FILE_PATH_SIZE];
   strcpy(szFileArmed, FOLDER_RUBY_TEMP);
   strcat(szFileArmed, FILE_TEMP_ARMED);

   g_pSemaphoreRestart = sem_open(SEMAPHORE_RESTART_VEHICLE_PROCS, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == g_pSemaphoreRestart) || (SEM_FAILED == g_pSemaphoreRestart) )
   {
      log_error_and_alarm("Failed to open semaphore: %s", SEMAPHORE_RESTART_VEHICLE_PROCS);
      g_pSemaphoreRestart = NULL;
   } 

   int iSkipCount = 0;
   while ( ! s_bQuit )
   {
      hardware_sleep_ms(maxTimeForProcessMs);
      counter++;
      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();

      if ( g_TimeNow > uTimeLoopLog + 10000 )
      {
         uTimeLoopLog = g_TimeNow;
         log_line("Main loop alive. Total restarts count: %d", iRestartCount);
      }

      if ( iSkipCount > 0 )
      {
         iSkipCount--;
         hardware_sleep_ms(500);
         continue;
      }

      if ( s_bUpdateDetected )
      {
         int iCounter = 120;
         while ( (! s_bQuit) && (iCounter > 0) )
         {
            hardware_sleep_ms(500);
            iCounter--;
         }
         break;
      }

      int iSemValue = 0;
      if ( NULL != g_pSemaphoreRestart )
      if ( 0 == sem_getvalue(g_pSemaphoreRestart, &iSemValue) )
      if ( iSemValue > 0 )
      if ( 0 == sem_trywait(g_pSemaphoreRestart) )
      {
         log_line("Semaphore to restart processes is set. Restarting...");
         _restart_procs(true);
         iRestartCount++;
      }

      if ( modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE )
      if ( g_TimeNow > g_TimeLastCheckRadioSilenceFailsafe + 20000 )
      if ( ! s_bUpdateDetected )
      {
         g_TimeLastCheckRadioSilenceFailsafe = g_TimeNow;

         log_line("Vehicle is alive. Total restarts count: %d", iRestartCount);

         modelVehicle.reloadIfChanged(true);

         if ( NULL == s_pProcessStatsRouter || NULL == s_pProcessStatsTelemetry || NULL == s_pProcessStatsCommands || ((NULL == s_pProcessStatsRC) && modelVehicle.rc_params.rc_enabled) )
            try_open_process_stats();
         if ( NULL == s_pProcessStatsRouter )
            continue;
         log_format_time(s_pProcessStatsRouter->lastRadioRxTime, szTime);
         log_line("Router last radio RX time: %s, %u ms ago", szTime, g_TimeLastCheckRadioSilenceFailsafe - s_pProcessStatsRouter->lastRadioRxTime);
         log_format_time(s_pProcessStatsRouter->lastRadioTxTime, szTime);
         log_line("Router last radio TX time: %s, %u ms ago", szTime, g_TimeLastCheckRadioSilenceFailsafe - s_pProcessStatsRouter->lastRadioTxTime);

         bool bCheckRadioFailSafe = false;
         if ( modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE )
         if ( access(szFileArmed, R_OK) != -1 ) 
            bCheckRadioFailSafe = true;

         if ( bCheckRadioFailSafe )
         if ( s_pProcessStatsRouter->lastRadioRxTime > 0 )
         if ( (s_pProcessStatsRouter->lastRadioRxTime+10000 < g_TimeNow) )
         {
            char szComm[128];
            sprintf(szComm, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ALARM_ON);
            hw_execute_bash_command_silent(szComm, NULL);

            log_line("Radio silence failsafe is enabled and radio timeout has triggered. Signal router to reinit radio interfaces.");
            if( access(szFileReinitRadio, R_OK) == -1 )
            {
               if ( 0 == s_iRadioSilenceFailsafeTimeoutCounts )
               {
                  char szComm5[MAX_FILE_PATH_SIZE];
                  sprintf(szComm5, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_REQUEST);
                  hw_execute_bash_command_silent(szComm5, NULL);
                  s_iRadioSilenceFailsafeTimeoutCounts++;
               }
               else
               {
                  hardware_reboot();
               }
            }
            else
               log_line("Will not signal router, a radio reinit is in progress already.");
         }
      }
      if ( noWatchDog )
         continue;

      if ( (NULL == s_pProcessStatsRouter) || (NULL == s_pProcessStatsTelemetry) || (NULL == s_pProcessStatsCommands) || ((NULL == s_pProcessStatsRC) && modelVehicle.rc_params.rc_enabled))
         try_open_process_stats();
      
      bMustRestart = false;

      if ( NULL != s_pProcessStatsRouter )
      {
         if ( (s_pProcessStatsRouter->lastActiveTime + maxTimeForProcessMs*4 < g_TimeNow) && _check_restrict_files() )
            s_failCountProcessRouter++;
         else
            s_failCountProcessRouter = 0;

         if ( s_failCountProcessRouter >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsRouter->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsRouter->lastIPCIncomingTime, szTime2);
            log_format_time(s_pProcessStatsRouter->lastRadioTxTime, szTime3);
            char* szPIDs = hw_process_get_pids_inline("ruby_rt_vehicle");
            if ( strlen(szPIDs) > 2 )
            {
               log_line_watchdog("Router pipeline watchdog check failed: router process (PID [%s]) has blocked! Last active time: %s, last IPC incoming time: %s, last radio TX time: %s", szPIDs, szTime, szTime2, szTime3);
               log_softerror_and_alarm("Router pipeline watchdog check failed: router process (PID [%s]) has blocked! Last active time: %s, last IPC incoming time: %s, last radio TX time: %s", szPIDs, szTime, szTime2, szTime3);
            }
            else
            {
               log_line_watchdog("Router pipeline watchdog check failed: router process (PID [%s]) has crashed! Last active time: %s, last IPC incoming time: %s, last radio TX time: %s", szPIDs, szTime, szTime2, szTime3);
               log_softerror_and_alarm("Router pipeline watchdog check failed: router process (PID [%s]) has crashed! Last active time: %s, last IPC incoming time: %s, last radio TX time: %s", szPIDs, szTime, szTime2, szTime3);
            }
            log_softerror_and_alarm("Router is blocked on substep: %d, %u ms ago", s_pProcessStatsRouter->uLoopSubStep, g_TimeNow - s_pProcessStatsRouter->lastActiveTime);

            modelVehicle.reloadIfChanged(true);
            if ( modelVehicle.hasCamera() )
            if ( modelVehicle.isActiveCameraOpenIPC() )
            {
               szPIDs = hw_process_get_pids_inline("majestic");
               log_line("Majestic PID(s): (%s)", szPIDs);
            }
            bMustRestart = true;
         }
      }

      if ( NULL != s_pProcessStatsTelemetry )
      {
         if ( (s_pProcessStatsTelemetry->lastActiveTime + maxTimeForProcessMs*4 < g_TimeNow) && _check_restrict_files() )
            s_failCountProcessTelemetry++;
         else
            s_failCountProcessTelemetry = 0;
         if ( s_failCountProcessTelemetry >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsTelemetry->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsTelemetry->lastIPCOutgoingTime, szTime2);
            char* szPIDs = hw_process_get_pids_inline("ruby_tx_telemetry");
            if ( strlen(szPIDs) > 2 )
            {
               log_line_watchdog("Telemetry TX pipeline watchdog check failed: telemetry tx process (PID [%s]) has blocked! Last active time: %s, last IPC outgoing time: %s", szPIDs, szTime, szTime2);
               log_softerror_and_alarm("Telemetry TX pipeline watchdog check failed: telemetry tx process (PID [%s]) has blocked! Last active time: %s, last IPC outgoing time: %s", szPIDs, szTime, szTime2);
            }
            else
            {
               log_line_watchdog("Telemetry TX pipeline watchdog check failed: telemetry tx process (PID [%s]) has crashed! Last active time: %s, last IPC outgoing time: %s", szPIDs, szTime, szTime2);
               log_softerror_and_alarm("Telemetry TX pipeline watchdog check failed: telemetry tx process (PID [%s]) has crashed! Last active time: %s, last IPC outgoing time: %s", szPIDs, szTime, szTime2);
            }
            bMustRestart = true;
         }
      }
      
      if ( NULL != s_pProcessStatsCommands )
      {
         if ( (s_pProcessStatsCommands->lastActiveTime +3*maxTimeForProcessMs*4 + 4000 < g_TimeNow) && _check_restrict_files() )
            s_failCountProcessCommands++;
         else
            s_failCountProcessCommands = 0;
         if ( s_failCountProcessCommands >= 2*s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsCommands->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsCommands->lastIPCIncomingTime, szTime2);
            char* szPIDs = hw_process_get_pids_inline("ruby_start");
            if ( strlen(szPIDs) > 2 )
            {
               log_line_watchdog("Commands RX process watchdog check failed: commands rx process (PID [%s]) has blocked! Last active time: %s", szPIDs, szTime);
               log_softerror_and_alarm("Commands RX process watchdog check failed: commands rx process (PID [%s]) has blocked! Last active time: %s", szPIDs, szTime);
            }
            else
            {
               log_line_watchdog("Commands RX process watchdog check failed: commands rx process (PID [%s]) has crashed! Last active time: %s", szPIDs, szTime);
               log_softerror_and_alarm("Commands RX process watchdog check failed: commands rx process (PID [%s]) has crashed! Last active time: %s", szPIDs, szTime);
            }
            bMustRestart = true;
         }
      }

      if ( NULL != s_pProcessStatsRC )
      if ( modelVehicle.rc_params.rc_enabled )
      {
         if ( (s_pProcessStatsRC->lastActiveTime + maxTimeForProcessMs*4 < g_TimeNow) && _check_restrict_files() )
            s_failCountProcessRC++;
         else
            s_failCountProcessRC = 0;

         if ( s_failCountProcessRC >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsRC->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsRC->lastIPCIncomingTime, szTime2);
            char* szPIDs = hw_process_get_pids_inline("ruby_start");
            if ( strlen(szPIDs) > 2 )
            {
               log_line_watchdog("RC RX process watchdog check failed: RC rx process (PID [%s]) has blocked! Last active time: %s, last IPC incoming time: %s", szPIDs, szTime, szTime2);
               log_softerror_and_alarm("RC RX process watchdog check failed: RC rx process (PID [%s]) has blocked! Last active time: %s, last IPC incoming time: %s", szPIDs, szTime, szTime2);
            }
            else
            {
               log_line_watchdog("RC RX process watchdog check failed: RC rx process (PID [%s]) has crashed! Last active time: %s, last IPC incoming time: %s", szPIDs, szTime, szTime2);
               log_softerror_and_alarm("RC RX process watchdog check failed: RC rx process (PID [%s]) has crashed! Last active time: %s, last IPC incoming time: %s", szPIDs, szTime, szTime2);
            }
            bMustRestart = true;
         }
      }

      if ( bMustRestart )
      if ( access(szFileUpdate, R_OK) == -1 )
      {
         _restart_procs(true);
         iSkipCount = 20;
         iRestartCount++;
         bMustRestart = false;
      }
   }

   log_line_watchdog("Received request to quit. Stopping watchdog.");

   if ( NULL != g_pSemaphoreRestart )
       sem_close(g_pSemaphoreRestart);
   g_pSemaphoreRestart = NULL;

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, s_pProcessStatsRouter);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_TX, s_pProcessStatsTelemetry);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_COMMANDS_RX, s_pProcessStatsCommands);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_RX, s_pProcessStatsRC);
     
   return 0;
}
