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
#include "../base/models.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_procs.h"
#include "../base/vehicle_settings.h"
#include "../radio/radioflags.h"
#include "../base/ctrl_settings.h"
#include "first_boot.h"
#include "r_start_vehicle.h"
#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
#include "../base/ctrl_preferences.h"
#endif

Model s_ModelFirstBoot;

void do_first_boot_pre_initialization(bool bIgnoreDrivers)
{
   log_line("---------------------------------------");
   log_line("Do first time boot preinitialization...");

   char szOutput[9096];
   hw_execute_bash_command("lsmod", szOutput);
   log_line("Output of lsmod: [%s]", szOutput);
   log_line("------------------------------");

   #if defined (HW_PLATFORM_RASPBERRY)
   printf("\nRuby: Doing first time ever initialization on Raspberry. Please wait...\n");
   fflush(stdout);

   hw_execute_bash_command("sync", NULL);
   if ( ! bIgnoreDrivers )
      hardware_install_drivers(1);
   
   printf("\nRuby: Done doing first time ever initialization on Raspberry.\n");
   fflush(stdout);
   #endif

   #if defined (HW_PLATFORM_RADXA)

   printf("\nRuby: Doing first time ever initialization on Radxa. Please wait...\n");
   fflush(stdout);

   hw_execute_bash_command("systemctl disable glances", NULL);

   if ( ! bIgnoreDrivers )
      hardware_install_drivers(1);
   if ( ! hardware_is_running_on_runcam_vrx() )
      hw_execute_bash_command_raw("echo 'performance' | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", NULL);

   hw_execute_bash_command_silent("mkdir -p /tmp/ruby/", NULL);
   log_init_local_only("RubyStartFirst");
   //log_enable_stdout();
   log_add_file("/tmp/ruby/log_first_radxa.log");
   hw_execute_bash_command("echo \"\nRuby: Doing first time ever initialization on Radxa...\n\" > /tmp/ruby/log_first_radxa.log", NULL);

   char szComm[256];
   sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   hardware_set_default_radxa_cpu_freq();
   
   hw_execute_bash_command("sync", NULL);
   
   if ( hardware_is_running_on_runcam_vrx() )
   {
      // Disable joystick left action as a QA for RunCam VRx
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iActionQuickButton2 = quickActionNone;
      save_Preferences();
   }

   printf("\nRuby: Done doing first time ever initialization on Radxa.\n");
   fflush(stdout);
   hw_execute_bash_command("echo \"\nRuby: Done doing first time ever initialization on Radxa.\n\" > /tmp/ruby/log_first_radxa.log", NULL);

   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("rm -rf /etc/init.d/S*majestic", NULL);
   if ( ! bIgnoreDrivers )
      hardware_install_drivers(1);
   hardware_camera_check_set_oipc_sensor();
   hardware_camera_set_default_oipc_calibration(hardware_getCameraType());
   hardware_set_default_sigmastar_cpu_freq();
   #endif

   log_line("Done first time boot preinitialization.");
   log_line("---------------------------------------");
}


void do_first_boot_initialization_raspberry(bool bIsVehicle, u32 uBoardType)
{
   log_line("Doing first time boot setup for Raspberry platform...");
   if ( bIsVehicle )
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hostname", NULL);
   }
   else
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hostname", NULL);
   }

   hardware_sleep_ms(200);
   hardware_mount_boot();

   if ( ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) || ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) )
   {
      log_line("Raspberry Pi Zero detected on the first boot ever of the system. Updating settings for Pi Zero.");
      //sprintf(szBuff, "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
      //execute_bash_command(szBuff);
   }
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO2 )
   {
      log_line("Raspberry Pi Zero 2 detected on the first boot ever of the system. Updating settings for Pi Zero 2.");
      //sprintf(szBuff, "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
      //execute_bash_command(szBuff);
   }
}


void do_first_boot_initialization_radxa(bool bIsVehicle, u32 uBoardType)
{
   log_line("Doing first time boot setup for Radxa platform...");

   hw_execute_bash_command("sudo sysctl -w net.ipv6.conf.all.disable_ipv6=0", NULL);
   hw_execute_bash_command("sudo sysctl -w net.ipv6.conf.default.disable_ipv6=0", NULL);
   hw_execute_bash_command("sudo sysctl -p", NULL);
   hw_execute_bash_command("PATH=\"/usr/sbin:/usr/local/sbin:$PATH\"", NULL);

   hw_execute_bash_command("rm -rf /usr/bin/pulseaudio", NULL);

   hw_execute_bash_command("sudo systemctl stop systemd-timesyncd", NULL);
   hw_execute_bash_command("sudo systemctl disable systemd-timesyncd", NULL);

   hw_execute_bash_command("/etc/init.d/avahi-daemon stop", NULL);
   hw_execute_bash_command("rm -rf /etc/init.d/avahi-daemon", NULL);
   hw_execute_bash_command("rm -rf /usr/sbin/avahi-daemon", NULL);
}


void do_first_boot_initialization_openipc(bool bIsVehicle, u32 uBoardType)
{
   log_line("Doing first time boot setup for OpenIPC platform...");
   if ( (access("/etc/majestic.yaml", R_OK) != -1) && (hardware_file_get_file_size("/etc/majestic.yaml") > 200) )
      hw_execute_bash_command("cp -rf /etc/majestic.yaml /etc/majestic.yaml.org", NULL);

   hw_execute_bash_command("ln -s /lib/firmware/ath9k_htc/htc_9271.fw.3 /lib/firmware/ath9k_htc/htc_9271-1.4.0.fw", NULL);
   hw_execute_bash_command("sed -i 's/console:/#console:/' /etc/inittab", NULL);
}

void do_first_boot_initialization(bool bIsVehicle, u32 uBoardType)
{
   log_line("-------------------------------------------------------");
   log_line("First Boot detected. Doing first boot initialization..." );

   char szComm[256];

   #if defined (HW_PLATFORM_RADXA) || defined (HW_PLATFORM_RASPBERRY)
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %sbin_org", FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sruby_* %sbin_org/", FOLDER_BINARIES, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   #endif

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s*", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   #ifdef HW_PLATFORM_RASPBERRY
   do_first_boot_initialization_raspberry(bIsVehicle, uBoardType);
   #endif
   #ifdef HW_PLATFORM_RADXA
   do_first_boot_initialization_radxa(bIsVehicle, uBoardType);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   do_first_boot_initialization_openipc(bIsVehicle, uBoardType);
   #endif

   char szBuff[256];
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   sprintf(szComm, "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
   hw_execute_bash_command(szComm, szBuff);

   first_boot_create_default_model(bIsVehicle, uBoardType);

   if ( bIsVehicle )
   {
      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      hardware_camera_maj_apply_all_settings(&s_ModelFirstBoot, &(s_ModelFirstBoot.camera_params[s_ModelFirstBoot.iCurrentCamera].profiles[s_ModelFirstBoot.camera_params[s_ModelFirstBoot.iCurrentCamera].iCurrentProfile]),
          s_ModelFirstBoot.video_params.iCurrentVideoProfile, &(s_ModelFirstBoot.video_params));
      #endif
   }
   else
   {
      load_ControllerSettings();

      #ifdef HW_PLATFORM_RASPBERRY
      ControllerSettings* pcs = get_ControllerSettings();
      if ( NULL != pcs )
      {
         pcs->iFreqARM = DEFAULT_ARM_FREQ;
         if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO2 )
            pcs->iFreqARM = 1000;
         else if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3B )
            pcs->iFreqARM = 1200;
         else if ( ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3BPLUS) || (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B || ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3APLUS) )
            pcs->iFreqARM = 1400;
         else if ( ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZERO) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZEROW) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_NONE) 
               && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2B) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2BV11) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2BV12) )
            pcs->iFreqARM = 1200;

         hardware_mount_boot();
         hardware_sleep_ms(50);
         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         config_file_set_value("config.txt", "arm_freq", pcs->iFreqARM);
         config_file_set_value("config.txt", "arm_freq_min", pcs->iFreqARM);

         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }
      #endif

      #if defined (HW_PLATFORM_RADXA)
      ControllerSettings* pcs = get_ControllerSettings();
      if ( NULL != pcs )
         pcs->iFreqARM = hardware_get_cpu_speed();
      #endif
      save_ControllerSettings();
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_FIRST_BOOT);
   hw_execute_bash_command_silent(szComm, NULL);
   hardware_sleep_ms(50);
   //if ( ! s_isVehicle )
   //   execute_bash_command("raspi-config --expand-rootfs > /dev/null 2>&1", NULL);   


   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( ! pRadioHWInfo->isConfigurable )
         continue;

      if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812au(i, 10);
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812eu(i, 10);
      if ( hardware_radio_driver_is_atheros_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_atheros(i, 10);
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s*", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);
   log_line("First boot initialization completed.");
   log_line("---------------------------------------------------------");
}


Model* first_boot_create_default_model(bool bIsVehicle, u32 uBoardType)
{
   log_line("Creating a default model.");
   s_ModelFirstBoot.resetToDefaults(true);
   s_ModelFirstBoot.find_and_validate_camera_settings();

   char szFile[MAX_FILE_PATH_SIZE];
   
   if ( bIsVehicle )
   {
      s_ModelFirstBoot.hwCapabilities.uBoardType = uBoardType;

      #ifdef HW_PLATFORM_RASPBERRY

      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

      s_ModelFirstBoot.processesPriorities.iFreqARM = DEFAULT_ARM_FREQ;
      if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO2 )
         s_ModelFirstBoot.processesPriorities.iFreqARM = 1000;
      else if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3B )
         s_ModelFirstBoot.processesPriorities.iFreqARM = 1200;
      else if ( ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3BPLUS) || ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B) || ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3APLUS) )
         s_ModelFirstBoot.processesPriorities.iFreqARM = 1400;
      else if ( ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZERO) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZEROW) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_NONE) 
               && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2B) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2BV11) && ((uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PI2BV12) )
         s_ModelFirstBoot.processesPriorities.iFreqARM = 1200;

      config_file_set_value("config.txt", "arm_freq", s_ModelFirstBoot.processesPriorities.iFreqARM);
      config_file_set_value("config.txt", "arm_freq_min", s_ModelFirstBoot.processesPriorities.iFreqARM);
      // Enable hdmi, we might need it for debug
      config_file_force_value("config.txt", "hdmi_force_hotplug", 1);
      config_file_force_value("config.txt", "ignore_lcd", 0);
      //config_file_force_value("config.txt", "hdmi_safe", 1);
        
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      #endif

      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      if ( hardware_board_is_sigmastar(s_ModelFirstBoot.hwCapabilities.uBoardType) )
         s_ModelFirstBoot.processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
      else
         s_ModelFirstBoot.processesPriorities.iFreqARM = hardware_get_cpu_speed();
      #endif

      bool bHasAtheros = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( pNICInfo->iRadioType == RADIO_TYPE_ATHEROS )
            bHasAtheros = true;
         if ( pNICInfo->iRadioType == RADIO_TYPE_RALINK )
            bHasAtheros = true;
      }

      s_ModelFirstBoot.setVideoProfilesDefaultVideoBitrates();
      
      if ( bHasAtheros )
      {
         for( int i=0; i<s_ModelFirstBoot.radioLinksParams.links_count; i++ )
         {
            s_ModelFirstBoot.radioLinksParams.downlink_datarate_video_bps[i] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
            s_ModelFirstBoot.radioLinksParams.downlink_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
         }
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uTargetVideoBitrateBPS = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uTargetVideoBitrateBPS = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_LONG_RANGE].uTargetVideoBitrateBPS = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_USER].uTargetVideoBitrateBPS = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_CUST].uTargetVideoBitrateBPS = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_PIP].uTargetVideoBitrateBPS = 5000000;
      }

      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, "reset_info.txt");
      if ( access( szFile, R_OK ) != -1 )
      {
         log_line("Found info for restoring vehicle frequencies after a reset to defaults. Using it.");
         FILE* fd = fopen(szFile, "r");
         if ( NULL != fd )
         {
            char szBuff[128];
            fscanf(fd, "%u %u %u %u %u %s",
               &s_ModelFirstBoot.uVehicleId, &s_ModelFirstBoot.uControllerId,
               &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[0],
               &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[1],
               &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[2],
               szBuff);
            fclose(fd);
         
            if ( szBuff[0] == '*' && szBuff[1] == 0 )
               szBuff[0] = 0;
            szBuff[MAX_VEHICLE_NAME_LENGTH] = 0;
            strcpy(s_ModelFirstBoot.vehicle_name, szBuff);
            log_line("Restored vehicle frequencies.");
         }
         else
            log_softerror_and_alarm("Failed to restore vehicle frequencies.");
         char szComm[256];
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szFile);
         hw_execute_bash_command(szComm, NULL);
      }

      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      _set_default_sik_params_for_vehicle(&s_ModelFirstBoot);
      s_ModelFirstBoot.saveToFile(szFile, false);
   }
   else
   {
      s_ModelFirstBoot.resetRadioLinksParams();

      s_ModelFirstBoot.radioInterfacesParams.interfaces_count = 1;
      s_ModelFirstBoot.radioInterfacesParams.interface_radiotype_and_driver[0] = 0xFF0000;
      s_ModelFirstBoot.radioInterfacesParams.interface_supported_bands[0] = RADIO_HW_SUPPORTED_BAND_24;
      strcpy(s_ModelFirstBoot.radioInterfacesParams.interface_szMAC[0], "YYYYYY");
      strcpy(s_ModelFirstBoot.radioInterfacesParams.interface_szPort[0], "Y");
      s_ModelFirstBoot.radioInterfacesParams.interface_current_frequency_khz[0] = DEFAULT_FREQUENCY;
      s_ModelFirstBoot.radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      s_ModelFirstBoot.radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;

      s_ModelFirstBoot.radioLinksParams.link_radio_flags[0] = DEFAULT_RADIO_FRAMES_FLAGS;
      s_ModelFirstBoot.radioLinksParams.downlink_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_VIDEO;
      s_ModelFirstBoot.radioLinksParams.downlink_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_DATA;

      s_ModelFirstBoot.populateRadioInterfacesInfoFromHardware();

      s_ModelFirstBoot.b_mustSyncFromVehicle = true;
      s_ModelFirstBoot.is_spectator = false;

      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      s_ModelFirstBoot.saveToFile(szFile, false);
   }

   return &s_ModelFirstBoot;
}