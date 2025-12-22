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
#include <semaphore.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_radio_serial.h"
#include "../base/vehicle_settings.h"
#include "../radio/radioflags.h"
#include "../base/ruby_ipc.h"
#include "../base/tx_powers.h"

#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
#include "../base/ctrl_settings.h"
#include "../utils/utils_controller.h"
#include "../base/ctrl_interfaces.h"
#endif

#include "../common/string_utils.h"
#include "../common/strings_loc.h"
#include "r_start_vehicle.h"
#include "r_initradio.h"
#include "../r_vehicle/ruby_rx_commands.h"
#include "../r_vehicle/ruby_rx_rc.h"
#include "first_boot.h"
#include "r_test.h"

static sem_t* s_pSemaphoreStarted = NULL; 

static int s_iBootCount = 0;

static bool g_bDebug = false;
static bool g_bIsFirstBoot = false;
static bool s_isVehicle = false;
static bool s_bIgnoreDrivers = false;

bool s_bQuit = false;
Model modelVehicle;
u32 board_type = BOARD_TYPE_NONE;

void power_leds(int onoff)
{
   #ifdef HW_PLATFORM_RASPBERRY
   DIR *d;
   struct dirent *dir;
   char szBuff[512];
   d = opendir("/sys/class/leds/");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 3 )
            continue;
         if ( dir->d_name[0] != 'l' )
            continue;
         sprintf(szBuff, "/sys/class/leds/%s/brightness", dir->d_name);
         FILE* fd = fopen(szBuff, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%d\n", onoff);
            fclose(fd);
         }
      }
      closedir(d);
   }
   #endif
}

int _get_fast_reboot_counter()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FAST_BOOT_COUNTER);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return 0;
   int iCounter = 0;
   if ( 1 != fscanf(fd, "%d", &iCounter) )
      iCounter = 0;
   fclose(fd);
   return iCounter;
}

void _increase_fast_reboot_counter()
{
   int iCounter = _get_fast_reboot_counter();
   iCounter++;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FAST_BOOT_COUNTER);
   FILE* fd = fopen(szFile, "wt");
   if ( NULL == fd )
   {
      printf("\nERROR: Failed to write fast reboot counter %d\n", iCounter);
      return;
   }
   fprintf(fd, "%d\n", iCounter);
   fclose(fd);

   printf("\nFast reboot counter %d -> %d\n", iCounter-1, _get_fast_reboot_counter());
}

void _reset_fast_reboot_counter()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FAST_BOOT_COUNTER);
   FILE* fd = fopen(szFile, "wt");
   if ( NULL == fd )
      return;
   fprintf(fd, "%d\n", 0);
   fclose(fd);

   printf("\nDid reset fast boot counter.\n");
}


#if defined HW_PLATFORM_OPENIPC_CAMERA
static int s_iLogBootStepsOIPC = 0;
#endif

void _log_oipc_boot_step(const char* szText)
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   FILE* fd = fopen(FILE_BOOT_LOG_STEPS, "a+");
   if ( NULL != fd )
   {
      if ( (NULL == szText) || (0 == szText[0]) )
         fprintf(fd, "%d-%d: [No text]\n", s_iBootCount, s_iLogBootStepsOIPC);
      else
         fprintf(fd, "%d-%d: %s\n", s_iBootCount, s_iLogBootStepsOIPC, szText);
      fflush(fd);
      fclose(fd);
      s_iLogBootStepsOIPC++;
   }
   #endif
}

void _log_oipc_boot_rotate()
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   char szComm[MAX_FILE_PATH_SIZE];
   if ( access(FILE_BOOT_LOG_STEPS, R_OK) != -1 )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mv -f %s %s.bak", FILE_BOOT_LOG_STEPS, FILE_BOOT_LOG_STEPS);
      hw_execute_bash_command_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s", FILE_BOOT_LOG_STEPS);
      hw_execute_bash_command_silent(szComm, NULL);
      _log_oipc_boot_step("Rotated boot log file");
      _log_oipc_boot_step("Boot-Steps:");
   }
   #endif
}


void initLogFiles()
{
   char szComm[256];
   char szSrcFile[MAX_FILE_PATH_SIZE];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s", FOLDER_LOGS, LOG_FILE_LOGGER);
   hw_execute_bash_command_silent(szComm, NULL);

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_SYSTEM);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_system_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_ERRORS);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_errors_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_ERRORS_SOFT);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_errors_soft_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_COMMANDS);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_commands_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_WATCHDOG);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_watchdog_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_VIDEO);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_video_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_CAPTURE_VEYE);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_capture_veye_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
   }


   if ( s_iBootCount >= 40 )
   {
      for( int i=0; i<=5; i++ )
      {
         sprintf(szComm, "rm -rf %s/logs*%d.txt 2>&1", FOLDER_LOGS, s_iBootCount-35-i);
         hw_execute_bash_command_silent(szComm, NULL);
      }
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s", FOLDER_LOGS, LOG_FILE_LIVE_VEHICLE_LOG);
   hw_execute_bash_command_silent(szComm, NULL);
}


void detectSystemType()
{
   if ( hardware_is_vehicle() )
   {
      log_line("Detected system as vehicle/relay.");
      s_isVehicle = true;
   }
   else
   {
      log_line("Detected system as controller.");
      s_isVehicle = false;
   }
   
   log_line("");
   log_line("===========================================================================");
   if ( s_isVehicle )
      log_line("| System detected as vehicle/relay.");
   else
      log_line("| System detected as controller.");
   log_line("===========================================================================");
   log_line("");

   FILE* fd = NULL;
   
   #ifdef HW_PLATFORM_RASPBERRY
   fd = fopen("/boot/ruby_systype.txt", "w");
   #endif
   #ifdef HW_PLATFORM_RADXA
   fd = fopen("/config/ruby_systype.txt", "w");
   #endif
   
   if ( NULL != fd )
   {
      if ( s_isVehicle )
         fprintf(fd, "VEHICLE");
      else
         fprintf(fd, "STATION");
      fclose(fd);
   }
}


void _check_files()
{
   char szFilesMissing[1024];
   szFilesMissing[0] = 0;
   bool failed = false;

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   if( access( "ruby_controller", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_controller"); }
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
   #endif

   if( access( "ruby_rt_vehicle", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rt_vehicle"); }
   if( access( "ruby_tx_telemetry", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_tx_telemetry"); }

   #ifdef HW_PLATFORM_RASPBERRY
   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/ath9k_hw.conf /etc/modprobe.d/ath9k_hw.conf.org", NULL);
      if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
         {failed = true; strcat(szFilesMissing, " Atheros_config");}
   }
   #endif

   if ( failed )
      printf("Ruby: Checked files consistency: failed.\n");
   else
      printf("Ruby: Checked files consistency: ok.\n");
}

void _check_update_drivers_on_update()
{
   if ( s_bIgnoreDrivers )
      return;
   #if defined (HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   char szOutput[4098];
   bool bNeedsInstall = false;
   hw_execute_bash_command("lsmod | grep 88XXau", szOutput);
   if ( strlen(szOutput) < 6 )
   {
      if ( ! hardware_load_driver_rtl8812au() )
         bNeedsInstall = true;
   }
   hw_execute_bash_command("lsmod | grep 8812eu", szOutput);
   if ( strlen(szOutput) < 6 )
   {
      if ( ! hardware_load_driver_rtl8812eu() )
         bNeedsInstall = true;
   }
   if ( bNeedsInstall )
   {
      log_line("Updating drivers. Please wait...");
      printf("Ruby: Updating drivers. Please wait...\n");
      fflush(stdout);
      hardware_install_drivers(1);
   }
   #endif
}

void _log_openipc_info()
{
   hw_execute_bash_command("rm -rf /root/ruby/hw_info.txt", NULL);
   hardware_sleep_ms(50);
   FILE* fd = fopen("/root/ruby/hw_info.txt", "wb");
   if ( NULL == fd )
      return;
   fprintf(fd, "Hardware detected info:\nCamera:\n");
   fclose(fd);
   hw_execute_bash_command("ipcinfo -s >> /root/ruby/hw_info.txt", NULL);
   hw_execute_bash_command("fw_printenv sensor 2>/dev/null >> /root/ruby/hw_info.txt", NULL);
   hw_execute_bash_command("echo 'Env:' >> /root/ruby/hw_info.txt", NULL);
   hw_execute_bash_command("fw_printenv >> /root/ruby/hw_info.txt", NULL);
   hw_execute_bash_command("echo 'OS:' >> /root/ruby/hw_info.txt", NULL);
   hw_execute_bash_command("cat /etc/os-release >> /root/ruby/hw_info.txt", NULL);
}

bool _init_timestamp_and_boot_count()
{
   bool bFirstBoot = false;
   s_iBootCount = 0;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &s_iBootCount) )
      {
         s_iBootCount = 0;
         bFirstBoot = true;
      }
      fclose(fd);
   }
   else
      bFirstBoot = true;

   static long long lStartTimeStamp_ms;
   struct timespec t;
   clock_gettime(RUBY_HW_CLOCK_ID, &t);
   lStartTimeStamp_ms = t.tv_sec*1000LL + t.tv_nsec/1000LL/1000LL;
   
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_TIMESTAMP);

   int count = 0;
   while ( count < 10 )
   {
      fd = fopen(szFile, "w");
      if ( NULL != fd )
      {
         fprintf(fd, "%lld\n", lStartTimeStamp_ms);
         fclose(fd);
         break;
      }
      #ifdef HW_PLATFORM_RASPBERRY
      system("sudo mount -o remount,rw /");
      hardware_sleep_ms(50);
      #endif
      count++;
   }
   s_iBootCount++;
   return bFirstBoot;
}

void _test_log(int argc, char *argv[])
{
   log_enable_stdout();
   log_init_local_only("RubyStart");
   log_arguments(argc, argv);

   int iKeyId = LOGGER_MESSAGE_QUEUE_ID;
   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-testlog") == 0 )
   {
      iKeyId = atoi(argv[argc-1]);
   }

   log_line_forced_to_file("Test: Using initial message queue key id: %d", iKeyId);
   
   /*
   hw_stop_process("ruby_logger");

   if ( ! hw_process_exists("ruby_logger") )
   {
      if ( iKeyId == LOGGER_MESSAGE_QUEUE_ID )
         hw_execute_ruby_process(NULL, "ruby_logger", NULL, NULL);
      else
      {
         char szParams[128];
         sprintf(szParams, "-id %d", iKeyId);
         hw_execute_ruby_process(NULL, "ruby_logger", szParams, NULL);
      }
      hardware_sleep_ms(300);
   }
   */
   key_t logServiceKey = 0;
   int logServiceMessageQueue = -1;

   logServiceKey = generate_msgqueue_key(iKeyId);
   log_line_forced_to_file("Test: Generated logger msgqueue key: 0x%X", logServiceKey);
   
   logServiceMessageQueue = msgget(logServiceKey, S_IWUSR | S_IWGRP | S_IWOTH);
   if ( logServiceMessageQueue == -1 )
      log_line_forced_to_file("Test: Failed to get logger message queue id, error: %d, [%s]", errno,  strerror(errno));
   else
      log_line_forced_to_file("Test: Got logger message queue id: %d", logServiceMessageQueue);

   type_log_message_buffer msg;
   msg.type = 1;
   msg.text[0] = 0;

   strcpy(msg.text, "Test: Manual log entry sent directly to logger service, waiting for it.");

   if ( msgsnd(logServiceMessageQueue, &msg, strlen(msg.text)+1, 0) < 0 )
      log_line_forced_to_file("Test: Failed to send manual log entry (waiting for it).");
   else
      log_line_forced_to_file("Test: Sent successfully manual log entry (waiting for it)");


   strcpy(msg.text, "Test: Manual log entry sent directly to logger service, no wait.");

   if ( msgsnd(logServiceMessageQueue, &msg, strlen(msg.text)+1, IPC_NOWAIT) < 0 )
      log_line_forced_to_file("Test: Failed to send manual log entry (no wait).");
   else
      log_line_forced_to_file("Test: Sent successfully manual log entry (no wait)");


   log_line("Test log line automated");

   //if ( bStartedLog )
   //   hw_stop_process("ruby_logger");
}


void _check_power_levels_of_current_cards(radio_hw_info_t* pRadioInfoArrayPrev, int iCountPrev)
{
   bool bNewCards = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      bool bFound = false;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! pRadioHWInfo->isSupported )
         continue;
      if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
         continue;

      for( int k=0; k<iCountPrev; k++ )
      {
         if ( 0 != pRadioInfoArrayPrev[k].szMAC[0] )
         if ( 0 != pRadioHWInfo->szMAC[0] )
         if ( 0 == strcmp(pRadioHWInfo->szMAC, pRadioInfoArrayPrev[k].szMAC) )
         {
            bFound = true;
            break;
         }
      }

      if ( ! bFound )
      {
         log_line("Radio hardware interface %d: %s (%s, MAC: %s) is new in the system.",
            i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, pRadioHWInfo->szMAC);
         bNewCards = true;
      }
   }

   if ( ! bNewCards )
   {
      log_line("No new supported high capacity radio hardware cards found in the system.");
      return;
   }

   log_line("Checking power levels as there are new hardware radio interfaces on the system...");

   if ( s_isVehicle )
   {
      //bool bUpdated = false;
      for( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
      {
         int iCardModel = modelVehicle.radioInterfacesParams.interface_card_model[i];
         int iMaxPowerRaw = tx_powers_get_max_usable_power_raw_for_card(modelVehicle.hwCapabilities.uBoardType, iCardModel);

         if ( modelVehicle.radioInterfacesParams.interface_raw_power[i] > iMaxPowerRaw )
         {
            modelVehicle.radioInterfacesParams.interface_raw_power[i] = iMaxPowerRaw;
            //bUpdated = true;
         }
      }
   }
   else
   {
      #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
      bool bUpdated = false;
      int iMaximumRawCanBeSet = 10000;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         log_line("Checking radio interface %d: type: %s (%s)",
            i+1, str_get_radio_card_model_string(pCRII->cardModel), (pCRII->cardModel > 0)?"autodetected":"user set");
         int iMaxRawPower = tx_powers_get_max_usable_power_raw_for_card(hardware_getBoardType(), pCRII->cardModel);
         log_line("Max raw power for radio card %d: %d, current raw power: %d", i+1, iMaxRawPower, pCRII->iRawPowerLevel);
      
         if ( iMaxRawPower < iMaximumRawCanBeSet )
            iMaximumRawCanBeSet = iMaxRawPower;
      }
      log_line("Maximum raw power that can be set: %d", iMaximumRawCanBeSet);
      
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         int iMaxRawPower = tx_powers_get_max_usable_power_raw_for_card(hardware_getBoardType(), pCRII->cardModel);
         if ( pCRII->iRawPowerLevel > iMaxRawPower )
         {
            pCRII->iRawPowerLevel = iMaxRawPower;
            bUpdated = true;
         }
      }
      if ( bUpdated )
         save_ControllerSettings();
      #endif
   }
   log_line("Done checking power levels.");
}

void _log_platform(bool bNewLine)
{
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   printf("Built for OpenIPC camera.");
   #elif defined(HW_PLATFORM_LINUX_GENERIC)
   printf("Built for Linux");
   #elif defined(HW_PLATFORM_RASPBERRY)
   printf("Built for Raspberry");
   #elif defined(HW_PLATFORM_RADXA)
   printf("Built for Radxa Zero3");
   #else
   printf("Built for N/A");
   #endif
   if ( bNewLine )
      printf("\n");
}


int _step_process_cmd_line(int argc, char* argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d) ", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      _log_platform(false);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-resetproc") == 0 )
   {
      printf("\nReset processes settings...\n");
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.resetProcessesParams();
         modelVehicle.saveToFile(szFile, false);
         printf("Done\n");
      }
      return  1;
   }

   if ( strcmp(argv[argc-1], "-disableproc") == 0 )
   {
      printf("\nDisable processes settings...\n");
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.disableProcessesParams();
         modelVehicle.saveToFile(szFile, false);
         printf("Done\n");
      }
      return  1;
   }

   if ( (argc > 1) && (strcmp(argv[1], "-dbgproc") == 0) )
   {
      log_init("RubyDbg");
      hw_log_processes(argc, argv);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-noop") == 0 )
   {
      _log_platform(true);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-vehicle") == 0 )
   {
      r_start_vehicle(argc, argv);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-rx_commands") == 0 )
   {
      r_start_commands_rx(argc, argv);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-rc") == 0 )
   {
      r_start_rx_rc(argc, argv);
      return 1;
   }

   if ( (strcmp(argv[argc-1], "-initradio") == 0) || 
        (strcmp(argv[0], "-initradio") == 0) ||
        ((argc>1) && (strcmp(argv[1], "-initradio") == 0)) )
   {
      r_initradio(argc, argv);
      return 1;
   }

   if ( strcmp(argv[argc-1], "-test") == 0 )
   {
      start_test();
      return 1;
   }

   if ( strcmp(argv[argc-1], "-testlog") == 0 )
   {
      _test_log(argc, argv);
      return 1;
   }

   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-testlog") == 0 )
   {
      _test_log(argc, argv);
      return 1;
   }

   g_bDebug = false;
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-debug") == 0 )
      g_bDebug = true;

   s_bIgnoreDrivers = false;
   if ( strcmp(argv[argc-1], "-ignoredrivers") == 0 )
      s_bIgnoreDrivers = true;
   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-ignoredrivers") == 0 )
      s_bIgnoreDrivers = true;

   return 0;
}


int _step_find_console()
{
   char szComm[1024];
   char szFile[256];
   char *tty_name = ttyname(STDIN_FILENO);
   bool foundGoodConsole = false;
  
   sprintf(szComm, "echo 'Ruby console to start in: [%s]' >> /tmp/ruby_boot.log", ((tty_name != NULL)?tty_name:"N/A"));
   hw_execute_bash_command_silent(szComm, NULL);
   printf("\nRuby: Start on console (%s)\n", ((tty_name != NULL)? tty_name:"N/A"));
   fflush(stdout);

   if ( g_bDebug )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/tty1") == 0 )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/pts/0") == 0 )
      foundGoodConsole = true;

   #if defined (HW_PLATFORM_RASPBERRY)
   sprintf(szComm, "echo 'Ruby execute for Raspberry platform' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   #endif

   #if defined (HW_PLATFORM_RADXA)
   sprintf(szComm, "echo 'Ruby execute for Radxa Zero 3 platform' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   #endif

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   sprintf(szComm, "echo 'Ruby execute for OpenIPC platform' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   foundGoodConsole = true;
   char szConsoleName[12];
   strcpy(szConsoleName, "OpenIPCCon");
   tty_name = szConsoleName;
   #endif

   if ( (NULL == tty_name) || (!foundGoodConsole) )
   {
      sprintf(szComm, "echo 'Ruby execute in wrong console. Abort.' >> /tmp/ruby_boot.log");
      hw_execute_bash_command_silent(szComm, NULL);
      printf("\nRuby: Try to execute in wrong console (%s). Exiting.\n", tty_name != NULL ? tty_name:"N/A");
      fflush(stdout);
      return 0;
   }
   
   sprintf(szComm, "echo 'Ruby check semaphore...' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   s_pSemaphoreStarted = sem_open("/RUBY_STARTED_SEMAPHORE", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 0);
   if ( ((SEM_FAILED == s_pSemaphoreStarted) || (NULL == s_pSemaphoreStarted)) && (!g_bDebug) )
   {
      printf("\nRuby (v %d.%d b-%d) is starting...\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      fflush(stdout);
      sleep(8);
      return 0;
   }
   sprintf(szComm, "echo 'Ruby processes are starting...' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);

   printf("\nRuby: Is starting...\n");
   fflush(stdout);

   #ifdef HW_PLATFORM_RASPBERRY
   //execute_bash_command_silent("con2fbmap 1 0", NULL);
   system("sudo mount -o remount,rw /");
   system("sudo mount -o remount,rw /boot");
   system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
   hardware_sleep_ms(50);
   #endif

   #if defined (HW_PLATFORM_RADXA)
   system("sudo mount -o remount,rw /");
   system("cd /home/radxa/ruby");
   hardware_sleep_ms(50);
   #endif

   sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %sruby* 2>/dev/null", FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %sonyx* 2>/dev/null", FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
   hw_execute_bash_command("chown -hR root *", NULL);
   hw_execute_bash_command("chgrp -hR root *", NULL);
   #endif

   sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "mkdir -p %s", FOLDER_CALIBRATION_FILES);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s", FOLDER_CALIBRATION_FILES);
   hw_execute_bash_command(szComm, NULL);

   g_bIsFirstBoot = false;
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FIRST_BOOT);
   if ( access( szFile, R_OK ) != -1 )
      g_bIsFirstBoot = true;

   if ( _init_timestamp_and_boot_count() )
      g_bIsFirstBoot = true;

   initLogFiles();

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);

   if( access( szFile, R_OK ) != -1 )
   if ( ! hw_process_exists("ruby_logger") )
   {
      hw_execute_ruby_process(NULL, "ruby_logger", NULL, NULL);
      hardware_sleep_ms(300);
   }

   log_init("RubyStart");
   log_line("Found good console, starting Ruby...");
   return 1;
}


int _step_check_file_system()
{
   char szComm[256];
   char szFile[MAX_FILE_PATH_SIZE];
   bool readWriteOk = false;
   int readWriteRetryCount = 0;
   FILE* fd = NULL;
   
   #if defined (HW_PLATFORM_RADXA)
   hw_execute_bash_command("date -s 'next year'", NULL);
   hw_execute_bash_command("date -s 'next year'", NULL);
   #endif

   while ( ! readWriteOk )
   {
      printf("Ruby: Trying to access files...\n");
      readWriteRetryCount++;
      power_leds(readWriteRetryCount%2);

      if ( readWriteRetryCount > 50 )
      {
         printf("\nError accessing the file system. Abort.\n\n");
         log_line_forced_to_file("Error accessing the file system. Abort.");

         strcpy(szFile, FOLDER_LOGS);
         strcat(szFile, LOG_FILE_START);
         fd = fopen(szFile, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "Access to files failed after %d tries. Boot count now: %d\n", readWriteRetryCount, s_iBootCount);
            fclose(fd);
            fd = NULL;
         }
         return -1;
      }
      hardware_sleep_ms(100);

      #ifdef HW_PLATFORM_RASPBERRY
      system("sudo mount -o remount,rw /");
      system("sudo mount -o remount,rw /boot");
      system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
      hardware_mount_root();
      hardware_mount_boot();
      hw_execute_bash_command_silent("cd /boot; mount -o remount,rw /boot; cd /home/pi/ruby", NULL);
      hardware_sleep_ms(100);
      #endif

      #if defined(HW_PLATFORM_RADXA)
      system("sudo mount -o remount,rw /");
      system("sudo mount -o remount,rw /config");
      system("cd /config; sudo mount -o remount,rw /config; cd /home/radxa/ruby");
      system("cd /home/radxa/ruby");
      hardware_sleep_ms(100);
      #endif

      // For temporary fifo-s, same tmp root folder on all platforms
      sprintf(szComm, "mkdir -p %s", FOLDER_RUBY_FIFO_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s 2>/dev/null", FOLDER_RUBY_FIFO_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      if ( 0 < strlen(FOLDER_RUBY_FIFO_TEMP) )
      {
         sprintf(szComm, "rm -rf %s* 2>/dev/null", FOLDER_RUBY_FIFO_TEMP);
         hw_execute_bash_command_silent(szComm, NULL);
      }

      sprintf(szComm, "mkdir -p %s", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      if ( 0 < strlen(FOLDER_RUBY_TEMP) )
      {
         sprintf(szComm, "rm -rf %s* 2>/dev/null", FOLDER_RUBY_TEMP);
         hw_execute_bash_command_silent(szComm, NULL);
      }

      sprintf(szComm, "mkdir -p %sruby", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %sruby", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "rm -rf %sruby/* 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "umount %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_LOGS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s*", FOLDER_LOGS);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_CONFIG);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG_MODELS);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_CONFIG_MODELS);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_MEDIA);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_UPDATES);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_UPDATES);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_UPDATES);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s* 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s* 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s* 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
      hw_execute_bash_command_silent(szComm, NULL);


      sprintf(szComm, "mkdir -p %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_DRIVERS);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s", FOLDER_UPDATES, SUBFOLDER_UPDATES_DRIVERS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s%s* 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_DRIVERS);
      hw_execute_bash_command_silent(szComm, NULL);

      #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
      sprintf(szComm, "chmod 777 %sres/* 2>/dev/null", FOLDER_BINARIES);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      #endif

      strcpy(szFile, FOLDER_LOGS);
      strcat(szFile, LOG_FILE_START);
      fd = fopen(szFile, "a+");
      if ( NULL == fd )
      {
         printf("Can't access logs folder (%s)\n", FOLDER_LOGS);
         log_line_forced_to_file("Can't access logs folder (%s)", FOLDER_LOGS);
         continue;
      }

      fprintf(fd, "Check for write access (step 1), succeeded on try number: %d (boot count: %d)\n", readWriteRetryCount, s_iBootCount);

      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_BOOT_COUNT);
      FILE* fd2 = fopen(szFile, "wb");
      if ( NULL == fd2 )
      {
         fprintf(fd, "Failed to open boot count config file for write [%s].\n", szFile);
         fclose(fd);
         fd = NULL;
         printf("Can't access config folder (%s)\n", FOLDER_CONFIG);
         continue;
      }
      fprintf(fd2, "%d\n", s_iBootCount);
      fclose(fd2);
      fd2 = NULL;

      fprintf(fd, "Check for write access (step 2), succeeded on try number: %d (boot count: %d)\n", readWriteRetryCount, s_iBootCount);
      fclose(fd);
      fd = NULL;

      readWriteOk = true;
   }

   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_START);
   fd = fopen(szFile, "a+");
   if ( NULL != fd )
   {
      if ( readWriteOk )
         fprintf(fd, "Access to files ok. Boot count now: %d\n", s_iBootCount);
      else
         fprintf(fd, "Access to files failed. Boot count now: %d\n", s_iBootCount);
      fclose(fd);
      fd = NULL;
   }

   printf("Ruby: Access to files: Ok.\n");
   fflush(stdout);

   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_START);
   fd = fopen(szFile, "a+");
   if ( NULL != fd )
   {
      fprintf(fd, "Starting run number %d; Starting Ruby...\n\n", s_iBootCount);
      fclose(fd);
      fd = NULL;
   }
   log_line("Files are ok, checking processes and init log files...");
   return 0;
}

void _step_check_binaries_and_resources()
{
   char szComm[MAX_FILE_PATH_SIZE];

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_UPDATE_IN_PROGRESS);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_UPDATE_IN_PROGRESS_APPLY);
   hw_execute_bash_command(szComm, NULL);

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   _check_files();
   #endif

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_check_set_oipc_sensor();
   _log_openipc_info();
   #endif

   log_line("Ruby: Start on verison %d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
   printf("Ruby: Start on verison %d.%d (b-%d)\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
   fflush(stdout);

   int iMajor, iMinor;
   get_Ruby_BaseVersion(&iMajor, &iMinor);

   log_line("Ruby: Base version is %d.%d", iMajor, iMinor);
   printf("Ruby: Base version is %d.%d\n", iMajor, iMinor);

   char szInfo[256];
   char szOutput[1024];
   strcpy(szInfo, "Ruby: Platform: ");
   #if defined(HW_PLATFORM_RASPBERRY)
   strcat(szInfo, "Raspberry");
   #endif
   #if defined(HW_PLATFORM_RADXA)
   strcat(szInfo, "Radxa");
   #endif
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   strcat(szInfo, "OpenIPC");
   #endif

   u32 uBoardType = hardware_getBoardType();
   strcat(szInfo, ", Board: ");
   strcat(szInfo, str_get_hardware_board_name(uBoardType));

   #if defined(HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command("cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'", szOutput);
   removeTrailingNewLines(szOutput);
   strcat(szInfo, ", Board id: ");
   strcat(szInfo, szOutput);
   #endif

   hw_execute_bash_command_raw("uname -rn", szOutput);
   removeTrailingNewLines(szOutput);
   strcat(szInfo, ", ");
   strcat(szInfo, szOutput);   

   log_line(szInfo);
   printf(szInfo);
   printf("\n");
   fflush(stdout);

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
   if ( access("/usr/share/fonts/truetype/noto/noto.ttf", R_OK) == -1 )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_RESOURCES);
      strcat(szFile, "noto.ttf");
      if ( access(szFile, R_OK) != -1 )
      {
         hw_execute_bash_command("mkdir -p /usr/share/fonts/truetype/noto/", NULL);
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /usr/share/fonts/truetype/noto/", szFile);
         hw_execute_bash_command(szComm, NULL);
         hw_execute_bash_command("chmod 777 /usr/share/fonts/truetype/noto", NULL);
         hw_execute_bash_command("chmod 777 /usr/share/fonts/truetype/noto/noto.ttf", NULL);
      }
   }
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   log_line("Check OpenIPC presence of sysupgrade stop script...");
   if ( access("/etc/rc.local.stop", R_OK) == -1 )
   {
      hw_execute_bash_command("echo \"/usr/sbin/ruby_stop.sh\" > /etc/rc.local.stop", NULL);
      hw_execute_bash_command("chmod 755 /etc/rc.local.stop", NULL);
   }
   if ( access("/etc/rc.local.stop", X_OK) == -1 )
   {
      hw_execute_bash_command("chmod 755 /etc/rc.local.stop", NULL);
   }
   #endif
}

void _step_load_init_devices()
{
   #ifdef HW_PLATFORM_RADXA
   hw_execute_bash_command("ip link set wlx down 2>&1 1>/dev/null", NULL);
   #endif

   #ifdef HW_CAPABILITY_I2C
   hw_execute_bash_command("modprobe i2c-dev", NULL);
   #endif

   char szOutput[4096];
   hw_execute_bash_command_raw("lsusb", szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("USB Devices:");
   log_line(szOutput);

   hw_execute_bash_command_raw("lsmod", szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("Loaded Modules:");
   log_line(szOutput);      
      
   #ifdef HW_CAPABILITY_I2C
   hw_execute_bash_command_raw("i2cdetect -l", szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("I2C buses:");
   log_line(szOutput);      

   log_line("Ruby: Finding external I2C devices add-ons...");
   printf("Ruby: Finding external I2C devices add-ons...\n");
   fflush(stdout);
   hardware_i2c_reset_enumerated_flag();
   hardware_i2c_enumerate_busses(1);
   // Load existing settings first
   hardware_i2c_load_device_settings();
   hardware_i2c_log_devices();
   // Save existing settings and any new devices
   hardware_i2c_save_device_settings();
   int iKnown = hardware_i2c_get_found_count_known_devices();
   int iConfigurable = hardware_i2c_get_found_count_configurable_devices();
   if ( 0 == iKnown && 0 == iConfigurable )
   {
      log_line("Ruby: Done finding external I2C devices add-ons. None known found." );
      printf("Ruby: Done finding external I2C devices add-ons. None known found.\n" );
   }
   else
   {
      log_line("Ruby: Done finding external I2C devices add-ons. Found %d known devices of which %d are configurable.", iKnown, iConfigurable );
      printf("Ruby: Done finding external I2C devices add-ons. Found %d known devices of which %d are configurable.\n", iKnown, iConfigurable );
   }
   #endif

   fflush(stdout);

   log_line("Ruby: Finding serial ports...");
   printf("Ruby: Finding serial ports...\n");
   fflush(stdout);

   int iCount = hardware_serial_init_ports();
   log_line("Ruby: Initialized %d serial ports.", iCount);
   printf("Ruby: Initialized %d serial ports\n", iCount);
   fflush(stdout);
}

radio_hw_info_t sRadioInfoPrev[MAX_RADIO_INTERFACES];
int iHwRadiosCountPrev = 0;
int iHwRadiosSupportedCountPrev = 0;

void  _step_load_init_radios()
{
   log_line("Loading previous radio configuration...");
   hardware_load_radio_info_into_buffers(&iHwRadiosCountPrev, &iHwRadiosSupportedCountPrev, &sRadioInfoPrev[0]);
   log_line("Loaded previous radio configuration.");
   
   if ( ! s_bIgnoreDrivers )
      hardware_radio_load_radio_modules(1);
     
   hardware_sleep_ms(500);

   char szComm[256];
   char szBuff[256];
   char szOutput[4096];
   int iCount = 0;
   bool bWiFiDetected = false;
   int iMaxWifiCardsToDetect = 4;
   int iWifiIndexToTry = 0;

   while ( iCount < iMaxWifiCardsToDetect*2 )
   {      
      szOutput[0] = 0;
      hw_execute_bash_command_raw("lsmod | grep 88", szOutput);
      removeNewLines(szOutput);
      log_line("Content of lsmod: [%s]", szOutput);

      szOutput[0] = 0;
      hw_execute_bash_command_raw("ip link | grep wl", szOutput);
      removeNewLines(szOutput);
      log_line("Content of ip link: [%s]", szOutput);

      szOutput[0] = 0;
      hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
      removeNewLines(szOutput);
      log_line("Content of class net: [%s]", szOutput);

      sprintf(szBuff, "wlan%d", iWifiIndexToTry);
      if ( NULL != strstr(szOutput, szBuff) )
      {
         log_line("wlan%d detected on the first try: [%s]", iWifiIndexToTry, szOutput);
         bWiFiDetected = true;
      }

      if ( bWiFiDetected )
         break;

      for( int i=0; i<iMaxWifiCardsToDetect; i++ )
      {
         sprintf(szComm, "ip link | grep wlan%d", i);
         hw_execute_bash_command_raw(szComm, szOutput);
         removeNewLines(szOutput);
         if ( NULL != strstr(szOutput, "wlan") )
         {
            sprintf(szComm, "ip link set dev wlan%d down", i );
            hw_execute_bash_command(szComm, NULL);
            hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
            sprintf(szComm, "ip link set dev wlan%d up", i );
            hw_execute_bash_command(szComm, NULL);
            hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
         }
      }
      
      log_line("Trying detect wifi cards (try %d)...", iCount);
      printf("Ruby: Trying to detect 2.4/5.8 Ghz cards...\n");
      fflush(stdout);
      
      hardware_sleep_ms(500);
      for( int i=0; i<5; i++ )
      {
         power_leds(1);
         hardware_sleep_ms(170);
         power_leds(0);
         hardware_sleep_ms(170);
      }

      iCount++;
      iWifiIndexToTry++;
      if ( iWifiIndexToTry >= iMaxWifiCardsToDetect )
      {
         iWifiIndexToTry = 0;
         if ( ! s_bIgnoreDrivers )
            hardware_radio_load_radio_modules(1);
         hardware_sleep_ms(1000);
      }
   }

   for( int i=0; i<=iWifiIndexToTry; i++ )
   {
      sprintf(szComm, "ip link 2>&1 | grep wlan%d", i);
      hw_execute_bash_command_raw(szComm, szOutput);
      removeNewLines(szOutput);
      log_line("Radio interface wlan%d state: [%s]", i, szOutput);
   }
   if ( ! bWiFiDetected )
      log_softerror_and_alarm("Failed to find any wifi cards.");

   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   removeNewLines(szOutput);
   log_line("Network devices found: [%s]", szOutput);

   for( int i=0; i<iMaxWifiCardsToDetect; i++ )
   {
      sprintf(szBuff, "/sys/class/net/wlan%d/device/uevent", i);
      if ( access(szBuff, R_OK) != -1 )
      {
         sprintf(szComm, "cat /sys/class/net/wlan%d/device/uevent", i);
         hw_execute_bash_command_raw(szComm, szOutput);
         removeNewLines(szOutput);
         log_line("Network wlan%d info: [%s]", i, szOutput);
      }
   }

   if ( hardware_has_eth() != NULL )
   {
      log_line("Device has an ETH port.");
      printf("Ruby: Device has an ETH port.\n");
   }
   else
   {
      log_line("Device does not have an ETH port.");
      printf("Ruby: Device does not have an ETH port.\n");    
   }
   fflush(stdout);
}

void _step_enumerate_radios()
{
   printf("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...\n");
   printf("\n");
   log_line("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...");
   fflush(stdout);

   hardware_radio_remove_stored_config();
   hardware_enumerate_radio_interfaces_step(0);

   //int iCountHighCapacityInterfaces = hardware_get_radio_interfaces_count();

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      printf("Ruby: No 2.4/5.8 Ghz radio interfaces found!\n");
      printf("\n");
      log_line("Ruby: No 2.4/5.8 Ghz radio interfaces found!");
      fflush(stdout);
   }
   else
   {
      printf("Ruby: %d radio interfaces found on 2.4/5.8 Ghz bands\n", hardware_get_radio_interfaces_count());
      printf("\n");
      log_line("Ruby: %d radio interfaces found on 2.4/5.8 Ghz bands", hardware_get_radio_interfaces_count());
      fflush(stdout);

      printf("Ruby: %d of %d radio interfaces are supported on 2.4/5.8 Ghz bands\n", hardware_get_supported_radio_interfaces_count(), hardware_get_radio_interfaces_count());
      printf("\n");
      log_line("Ruby: %d of %d radio interfaces are supported on 2.4/5.8 Ghz bands", hardware_get_supported_radio_interfaces_count(), hardware_get_radio_interfaces_count());
      fflush(stdout);   
   }
   printf("Ruby: Finding SiK radio interfaces...\n");
   log_line("Ruby: Finding SiK radio interfaces...");
   fflush(stdout);

   hardware_enumerate_radio_interfaces_step(1);

   if ( ! hardware_radio_has_sik_radios() )
   {
      printf("Ruby: No SiK radio interfaces found.\n");
      log_line("Ruby: No SiK radio interfaces found.");
      fflush(stdout);
   }
   else
   {
      printf("Ruby: %d SiK radio interfaces found.\n", hardware_radio_has_sik_radios());
      log_line("Ruby: %d SiK radio interfaces found.", hardware_radio_has_sik_radios());
      fflush(stdout);    
   }

   printf("Ruby: Finding serial radio interfaces...\n");
   log_line("Ruby: Finding serial radio interfaces...");
   fflush(stdout);

   int iCountAdded = hardware_radio_serial_parse_and_add_from_serial_ports_config();

   if ( 0 == iCountAdded )
   {
      printf("Ruby: No serial radio interfaces found.\n");
      log_line("Ruby: No serial radio interfaces found.");
   }
   else
   {
      printf("\nRuby: %d serial radio interfaces found.\n\n", iCountAdded);
      log_line("Ruby: %d serial radio interfaces found.", iCountAdded);
   }
   printf("Ruby: Done finding radio interfaces.\n");
   log_line("Ruby: Done finding radio interfaces.");
   fflush(stdout);
}

void _step_initialize_check_vehicle()
{
   log_line("Doing initialization checks on vehicle...");

   log_line("Done doing initialization checks on vehicle.");
}

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   s_bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   if ( _step_process_cmd_line(argc, argv) )
      return 0;

   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   char szFile[MAX_FILE_PATH_SIZE];

   _increase_fast_reboot_counter();
   if ( _get_fast_reboot_counter() > 10 )
      _reset_fast_reboot_counter();

   _log_oipc_boot_rotate();
   
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   for( int i=0; i<10; i++ )
   {
      hardware_sleep_ms(500);
      _log_oipc_boot_step("Boot start delay");
   }
   #endif
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "debug");
   if ( access(szFile, R_OK) != -1 )
   {
      _log_oipc_boot_step("Debug wait");
      for( int i=0; i<10; i++ )
         hardware_sleep_ms(800);
      _log_oipc_boot_step("Debug wait done");
   }

   if ( ! _step_find_console() )
   {
      _reset_fast_reboot_counter();
      return 0;
   }

   _log_oipc_boot_step("Console found");

   log_line_forced_to_file("Found good console. Continuing...");
   log_arguments(argc, argv);
   printf("\nRuby: Start (v %d.%d b-%d) r%d\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER, s_iBootCount);
   fflush(stdout);

   if ( g_bDebug )
   {
      printf("\nRuby: Start in debug mode\n");
      fflush(stdout);
      hw_execute_bash_command("touch /tmp/debuglog", NULL);
   }

   if ( s_bIgnoreDrivers )
   {
      printf("\nRuby: Ignore drivers instalation\n");
      fflush(stdout);    
   }

   if ( _step_check_file_system() < 0 )
   {
      _reset_fast_reboot_counter();
      #if defined HW_PLATFORM_OPENIPC_CAMERA
      hw_execute_bash_command("firstboot", NULL);
      #endif
      return 0;
   }

   //#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   //initLocalizationData();
   //#endif
   
   _log_oipc_boot_step("Done check files.");

   init_hardware_only_detection_pins();
   hardware_detectBoardAndSystemType();

   printf("-----------------------------------\n");
   if ( hardware_is_vehicle() )
      printf("Ruby: System detected as vehicle/relay.\n");
   else
      printf("Ruby: System detected as controller.\n");
   fflush(stdout);

   _step_check_binaries_and_resources();
   _log_oipc_boot_step("Done check binaries.");

   char szComm[1204];
   char szOutput[4096];
   szOutput[0] = 0;

   if ( g_bIsFirstBoot )
      do_first_boot_pre_initialization(s_bIgnoreDrivers);

   _step_load_init_devices();

   _log_oipc_boot_step("Done init devices.");

   if ( ! g_bIsFirstBoot )
      _check_update_drivers_on_update();
   _step_load_init_radios();

   _log_oipc_boot_step("Done init radios.");

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_CONFIG_SYSTEM_TYPE);
   hw_execute_bash_command_silent(szComm, NULL);
   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_CONFIG_CAMERA_TYPE);
   hw_execute_bash_command_silent(szComm, NULL);

   if ( access( FILE_FORCE_RESET, R_OK ) != -1 )
   {
      unlink(FILE_FORCE_RESET);
      hw_execute_bash_command("rm -rf config/*", NULL);
      sprintf(szComm, "touch %s%s", FOLDER_CONFIG, FILE_CONFIG_FIRST_BOOT);
      hw_execute_bash_command(szComm, NULL);
      hardware_reboot();
      hardware_sleep_ms(900);
   }

   board_type = (hardware_getBoardType() & BOARD_TYPE_MASK);

   // Detect hardware camera
   hardware_getCameraType();

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
   hw_execute_ruby_process(NULL, "ruby_initdhcp", NULL, NULL);
   #endif

   detectSystemType();

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
   if ( g_bDebug )
   if ( hardware_is_station() )
   {
       load_Preferences();
       Preferences* pP = get_Preferences();
       pP->nLogLevel = 0;
       save_Preferences();
   }
   #endif

   hardware_release();

   _log_oipc_boot_step("Done hardware detect.");

   log_line("Starting Ruby system...");
   fflush(stdout);
   
   _step_enumerate_radios();
   
   _log_oipc_boot_step("Done enumerate radios.");

   // Reenable serial ports that where used for SiK radio and now are just regular serial ports
   
   bool bSerialPortsUpdated = false;
   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info(i);
      if ( NULL == pSerialPort )
         continue;
      if ( pSerialPort->iPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
      if ( ! hardware_serial_is_sik_radio(pSerialPort->szPortDeviceName) )
      {
         log_line("Serial port %d [%s] was a SiK radio. Now it's a regular serial port. Setting it as used for nothing.", i+1, pSerialPort->szPortDeviceName);
         pSerialPort->iPortUsage = SERIAL_PORT_USAGE_NONE;
         bSerialPortsUpdated = true;
      }
   }
   if ( bSerialPortsUpdated )
   {
      hardware_serial_save_configuration();
   }

   _log_oipc_boot_step("Done enumerate serial.");

#ifdef HW_PLATFORM_RASPBERRY
   log_line("Running on Raspberry Pi hardware");
#endif
#ifdef HW_PLATFORM_RADXA
   log_line("Running on Radxa Zero 3 hardware");
#endif
#ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("Running on OpenIPC hardware");
#endif

#ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("Feature local audio recording is: On.");
#else
   log_line("Feature local audio recording is: Off.");
#endif

#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   log_line("Feature radio RxTx thread synchronization is: On.");
#else
   log_line("Feature radio RxTx thread synchronization is: Off.");
#endif

   if ( g_bIsFirstBoot )
      do_first_boot_initialization(s_isVehicle, board_type);

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( access( szFile, R_OK) == -1 )
      first_boot_create_default_model(s_isVehicle, board_type);
   

   if ( s_isVehicle )
   {
      _step_initialize_check_vehicle();

      int c = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         if ( pNIC->isSupported )
            c++;
      }
      if ( 0 == c || 0 == hardware_get_radio_interfaces_count() )
      {
         printf("Ruby: No supported radio interfaces found. Total radio interfaces found: %d\n", hardware_get_radio_interfaces_count());
         fflush(stdout);

         if ( NULL != s_pSemaphoreStarted )
            sem_close(s_pSemaphoreStarted);
         log_line("");
         log_line("------------------------------");
         log_line("Ruby Start Finished.");
         log_line("------------------------------");
         log_line("");
         return 0;
      }
   }
   else
   {
       // Controller
       #if defined (HW_PLATFORM_RADXA)
       hw_execute_bash_command("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", szOutput);
       if ( NULL == strstr(szOutput, "performance") )
          hardware_set_default_radxa_cpu_freq();

       hw_execute_bash_command("systemctl disable systemd-journald.service 2>&1 1>/dev/null", NULL);
       hw_execute_bash_command("systemctl mask systemd-journald.service 2>&1 1>/dev/null", NULL);
       hw_execute_bash_command("systemctl disable rknpu2.service 2>&1 1>/dev/null", NULL);
       hw_execute_bash_command("systemctl mask rknpu2.service 2>&1 1>/dev/null", NULL);
       #endif

       #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
       bool allDisabled = true;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
       {
          radio_hw_info_t* pNIC = hardware_get_radio_info(i);
          u32 flags = controllerGetCardFlags(pNIC->szMAC);
          if ( ! ( flags & RADIO_HW_CAPABILITY_FLAG_DISABLED ) )
             allDisabled = false;
       }
       if ( allDisabled || (hardware_get_radio_interfaces_count() < 2) )
       if ( hardware_get_radio_interfaces_count() > 0 )
       {
          log_line("There is a single radio card or all are disabled. Enable at least the first card.");
          load_ControllerInterfacesSettings();
          radio_hw_info_t* pNIC = hardware_get_radio_info(0);
          u32 uFlags = controllerGetCardFlags(pNIC->szMAC);
          uFlags &= ~(RADIO_HW_CAPABILITY_FLAG_DISABLED);
          uFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
          uFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
          controllerSetCardFlags(pNIC->szMAC, uFlags);
          save_ControllerInterfacesSettings();
          load_ControllerInterfacesSettings();
       }
       #endif
   }

   _log_oipc_boot_step("Check processes versions...");

   hw_execute_ruby_process_wait(NULL, "ruby_start", "-ver", szOutput, 1);
   log_line("ruby_start: [%s]", szOutput);

   hw_execute_ruby_process_wait(NULL, "ruby_rt_vehicle", "-ver", szOutput, 1);
   log_line("ruby_rt_vehicle: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_tx_telemetry", "-ver", szOutput, 1);
   log_line("ruby_tx_telemetry: [%s]", szOutput);

   if ( !s_isVehicle )
   {
      hw_execute_ruby_process_wait(NULL, "ruby_rt_station", "-ver", szOutput, 1);
      log_line("ruby_rt_station: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_rx_telemetry", "-ver", szOutput, 1);
      log_line("ruby_rx_telemetry: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_tx_rc", "-ver", szOutput, 1);
      log_line("ruby_tx_rc: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_central", "-ver", szOutput, 1);
      log_line("ruby_central: [%s]", szOutput);
   }

   _log_oipc_boot_step("Check for update files...");

   #if defined(HW_PLATFORM_RADXA)
   //hw_stop_process("wpa_supplicant");
   #endif

   if ( s_isVehicle )
   {
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(szFile, false);
      }
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VERSION);
   FILE* fd = fopen(szFile, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", (((u32)SYSTEM_SW_VERSION_MAJOR)<<8) | (u32)SYSTEM_SW_VERSION_MINOR | (((u32)SYSTEM_SW_BUILD_NUMBER)<<16));
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to save current version id to file: %s",FILE_CONFIG_CURRENT_VERSION);

   if ( g_bIsFirstBoot )
   {
      printf("\n\n\n");
      printf("Ruby: First install initialization complete. Rebooting now...\n");
      printf("\n\n\n");
      log_line("Ruby: First install initialization complete. Rebooting now...");
      fflush(stdout);

      _reset_fast_reboot_counter();

      #ifdef HW_PLATFORM_RASPBERRY
      hw_execute_bash_command("cp -rf /home/pi/ruby/logs/log_start.txt /home/pi/ruby/logs/log_firstboot_start.txt", NULL);
      hw_execute_bash_command("cp -rf /home/pi/ruby/logs/log_system.txt /home/pi/ruby/logs/log_firstboot.txt", NULL);
      #endif
      #ifdef HW_PLATFORM_RADXA
      hw_execute_bash_command("cp -rf /home/radxa/ruby/logs/log_start.txt /home/radxa/ruby/logs/log_firstboot_start.txt", NULL);
      hw_execute_bash_command("cp -rf /home/radxa/ruby/logs/log_system.txt /home/radxa/ruby/logs/log_firstboot.txt", NULL);
      #endif
      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      hw_execute_bash_command("cp -rf /tmp/logs/log_start.txt /root/ruby/log_firstboot_start.txt", NULL);
      hw_execute_bash_command("cp -rf /tmp/logs/log_system.txt /root/ruby/log_firstboot.txt", NULL);
      #endif
      hw_execute_bash_command("sync", NULL);

      hardware_sleep_ms(500);
      if ( ! g_bDebug )
      {
         hw_execute_bash_command("sudo raspi-config --expand-rootfs > /dev/null 2>&1", NULL);
         hardware_reboot();
      }
      return 0;
   }
 
   _log_oipc_boot_step("Check for hw changes...");
   printf("Ruby: Checking for HW changes...");
   log_line("Checking for HW changes...");
   fflush(stdout);

   if ( s_isVehicle )
   {
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(szFile, false);
      }

      if ( _get_fast_reboot_counter() > 3 )
      {
         log_line("Did 3 fast reboots. Reset current model and reboot...");
         printf("\nDid 3 fast reboots. Reset current model and reboot...\n\n");
         _reset_fast_reboot_counter();
         modelVehicle.resetAllSettingsKeepPairing(true);
         modelVehicle.saveToFile(szFile, false);
         hardware_reboot();
         hardware_sleep_ms(900);
         return  0;
      }
   }

   hardware_i2c_check_and_update_device_settings();
   printf(" Done.\n");
   log_line("Checking for HW changes complete.");
   fflush(stdout);

   _log_oipc_boot_step("Init IPC...");
   ruby_init_ipc_channels();
   _log_oipc_boot_step("Done init IPC.");
   
   if ( s_isVehicle )
   {
      log_line("---------------------------------");
      log_line("|  Vehicle Id: %u", modelVehicle.uVehicleId);
      log_line("---------------------------------");
   }
   printf("Ruby: Starting processes...\n");
   log_line("Starting processes...");
   fflush(stdout);

   printf("Ruby: Init radio interfaces...\n");
   log_line("Init radio interfaces...");
   fflush(stdout);

   char szParams[64];
   strcpy(szParams, "-initradio");
   if ( s_isVehicle )
   {
      for ( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (modelVehicle.radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
         if ( modelVehicle.radioInterfacesParams.interface_link_id[i] >= 0 )
         if ( modelVehicle.radioInterfacesParams.interface_link_id[i] < modelVehicle.radioLinksParams.links_count )
         {
            int dataRateMb = modelVehicle.radioLinksParams.downlink_datarate_video_bps[modelVehicle.radioInterfacesParams.interface_link_id[i]];
            if ( dataRateMb > 0 )
               dataRateMb = dataRateMb / 1000 / 1000;
            if ( dataRateMb > 0 )
            {
               sprintf(szParams, "-initradio %d", dataRateMb);
               break;
            }
         }
      }
   }
   hw_execute_ruby_process_wait(NULL, "ruby_start", szParams, NULL, 1);
   
   log_line("Reloading hardware radio configuration after radio init step completed...");
   hardware_load_radio_info();
   _check_power_levels_of_current_cards(&sRadioInfoPrev[0], iHwRadiosCountPrev);

   printf("Ruby: Starting main process...\n");
   log_line("Starting main process...");
   fflush(stdout);
   
   check_licences();
   
   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s*", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   if ( s_isVehicle )
   {
      printf("Ruby: Starting vehicle...\n");
      log_line("Starting vehicle...");
      fflush(stdout);
      if ( modelVehicle.radioLinksParams.links_count == 1 )
         printf("Ruby: Current frequency: %s\n", str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
      else if ( modelVehicle.radioLinksParams.links_count == 2 )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[1]));
         printf("Ruby: Current frequencies: %s/%s\n", szFreq1, szFreq2);
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[2]));
         printf("Ruby: Current frequencies: %s/%s/%s\n", szFreq1, szFreq2, szFreq3);
      }
      printf("Ruby: Detected radio interfaces: %d\n", hardware_get_radio_interfaces_count());
      printf("Ruby: Supported radio interfaces:\n");
      fflush(stdout);
      int c = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         if ( pNIC->isSupported )
         {
            printf("  %d: %s, driver: %s, USB port: %s\n", c+1, pNIC->szDescription, pNIC->szDriver, pNIC->szUSBPort);
            c++;
         }
      }
      printf("Ruby: Total supported radio interfaces: %d\n", c);
      fflush(stdout);
      hw_execute_ruby_process(NULL, "ruby_start", "-vehicle", NULL);
   }
   else
   {
      #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)

      u32 uControllerId = controller_utils_getControllerId();
      log_line("Controller UID: %u", uControllerId);

      #if defined(HW_PLATFORM_RASPBERRY)
      hw_execute_bash_command_silent("con2fbmap 1 0", NULL);
      //execute_bash_command_silent("printf \"\\033c\"", NULL);
      #endif

      if ( hardware_radio_has_sik_radios() )
      {
         printf("Ruby: Configuring SiK radios...\n");
         log_line("Configuring SiK radios...");
         fflush(stdout);

         load_ControllerSettings();

         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         if ( ! modelVehicle.loadFromFile(szFile, true) )
         {
            modelVehicle.resetToDefaults(true);
            modelVehicle.is_spectator = false;
            modelVehicle.saveToFile(szFile, false);
         }
         log_line("Setting default SiK radio params for all SiK radio interfaces on controller...");

         int iSiKRadioLinkIndex = -1;
         for( int i=0; i<modelVehicle.radioLinksParams.links_count; i++ )
         {
            if ( modelVehicle.radioLinkIsSiKRadio(i) )
            {
               iSiKRadioLinkIndex = i;
               break;
            }
         }

         u32 uFreq = DEFAULT_FREQUENCY_433;
         u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
         u32 uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         u32 uECC = 0;
         u32 uLBT = 0;
         u32 uMCSTR = 0;
         if ( iSiKRadioLinkIndex == -1 )
            log_line("No SiK radio link on current vehicle. Configuring SiK radio interfaces on controller with default parameters.");
         else
         {
            log_line("Current model radio link %d is a SiK radio link. Use it to configure controller.", iSiKRadioLinkIndex+1);
            uFreq = modelVehicle.radioLinksParams.link_frequency_khz[iSiKRadioLinkIndex];
            uDataRate = modelVehicle.radioLinksParams.downlink_datarate_data_bps[iSiKRadioLinkIndex],
            uECC = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_ECC)?1:0;
            uLBT = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_LBT)?1:0;
            uMCSTR = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_MCSTR)?1:0;
         }

         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( hardware_radio_index_is_sik_radio(i) )
            {
               radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
               if ( NULL == pRadioHWInfo )
               {
                  log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", i+1);
                  continue;
               }
               uTxPower = modelVehicle.radioInterfacesParams.interface_raw_power[i];
               hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
                  uFreq, uTxPower, uDataRate,
                  uECC, uLBT, uMCSTR,
                  NULL);
            }
         }
         log_line("Setting default SiK radio params for all SiK interfaces on controller. Done.");

         printf("Ruby: Configured SiK radios.\n");
         log_line("Configured SiK radios.");
         fflush(stdout);
      }

      printf("Ruby: Starting controller...\n");
      log_line("Starting controller...");
      fflush(stdout);

      #ifdef HW_PLATFORM_RADXA
      hw_execute_ruby_process(NULL, "ruby_alive", NULL, NULL);
      #endif
   

      hw_execute_ruby_process(NULL, "ruby_controller", NULL, NULL);

      #endif
   }

   _log_oipc_boot_step("Done started processes.");
   
   printf("Ruby: Started processes. Checking if all ok...\n");
   fflush(stdout);

   for( int i=0; i<10; i++ )
      hardware_sleep_ms(500);
   
   log_line("Checking processes start...");
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   if( access(szFile, R_OK) != -1 )
   {
      if ( hw_process_exists("ruby_logger") )
         log_line("ruby_logger is started");
      else
         log_error_and_alarm("ruby_logger is not running");
   }

   if ( s_isVehicle )
   {
      if ( NULL != s_pSemaphoreStarted )
         sem_close(s_pSemaphoreStarted);
      log_line("");
      log_line("------------------------------");
      log_line("Ruby Start Finished.");
      log_line("------------------------------");
      log_line("");

      char szFileUpdate[128];
      strcpy(szFileUpdate, FOLDER_RUBY_TEMP);
      strcat(szFileUpdate, FILE_TEMP_UPDATE_IN_PROGRESS);

      int iCheckCount = 0;
      while ( true )
      {
         if ( access(szFileUpdate, R_OK) != -1 )
         {
            log_line("Detected update in progress. Stop main look check.");
            _reset_fast_reboot_counter();
            break;
         }

         bool bError = false;

         if ( hw_process_exists("ruby_start") )
            log_line("ruby_start is started");
         else
            { log_error_and_alarm("ruby_start is not running"); bError = true; }


         if ( hw_process_exists("ruby_rt_vehicle") )
            log_line("ruby_rt_vehicle is started");
         else
            { log_error_and_alarm("ruby_rt_vehicle is not running"); bError = true; }
           
         if ( hw_process_exists("ruby_tx_telemetry") )
            log_line("ruby_tx_telemetry is started");
         else
            { log_error_and_alarm("ruby_tx_telemetry is not running"); bError = true; }

         if ( bError )
         {
            printf("Error: Some processes are not running. Check again.\n");
            log_line("Error: Some processes are not running. Check again.");
         }
         else
         {
            u32 uTimeNow = get_current_timestamp_ms();
            char szTime[128];
            sprintf(szTime,"%d %d:%02d:%02d.%03d", s_iBootCount, (int)(uTimeNow/1000/60/60), (int)(uTimeNow/1000/60)%60, (int)((uTimeNow/1000)%60), (int)(uTimeNow%1000));
            printf("%s All processes are running fine.\n", szTime);
         }
         fflush(stdout);
         iCheckCount++;
           
         for( int i=0; i<5; i++ )
            hardware_sleep_ms(500);

         if ( g_bDebug || (iCheckCount > 4) || (!bError) )
         {
            _reset_fast_reboot_counter();
            break;
         }
      }
      _log_oipc_boot_step("Done boot sequence.");
   }
   else // Controller
   {
      int iRetryCounter = 10;
      while ( iRetryCounter > 0 )
      {
         hardware_sleep_ms(2000);
         iRetryCounter--;
         bool bAllOk = false;
         if ( hw_process_exists("ruby_central") )
            log_line("ruby_central is started");
         else
         {
            bAllOk = false;
            log_error_and_alarm("ruby_central is not running");
         }
         if ( hw_process_exists("ruby_controller") )
            log_line("ruby_controller is started");
         else
         {
            bAllOk = false;
            log_error_and_alarm("ruby_controller is not running");
         }
         if ( hw_process_exists("ruby_rt_station") )
            log_line("ruby_rt_station is started");
         else
         {
            bAllOk = false;
            log_error_and_alarm("ruby_rt_station is not running");
         }

         if ( bAllOk )
            break;
      }

      //if ( ! g_bDebug )
      //{
      //   log_line("Clear screen");
      //   system("clear");
      //}
   }

   _reset_fast_reboot_counter();

   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("rm -rf /boot/last_ruby_boot.txt", NULL);
   hw_execute_bash_command("cp -rf logs/log_system.txt /boot/last_ruby_boot.txt", NULL);
   log_line("Copy boot log to /boot partition. Done.");
   #endif

   #ifdef HW_PLATFORM_RADXA
   hw_execute_bash_command("rm -rf /config/last_ruby_boot.txt", NULL);
   hw_execute_bash_command("cp -rf logs/log_system.txt /config/last_ruby_boot.txt", NULL);
   log_line("Copy boot log to /config partition. Done.");
   #endif


   if ( NULL != s_pSemaphoreStarted )
      sem_close(s_pSemaphoreStarted);
   log_line("");
   log_line("------------------------------");
   log_line("Ruby Start Finished.");
   log_line("------------------------------");
   log_line("");
   return 0;
}
