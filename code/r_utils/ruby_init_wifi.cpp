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
#include "../base/config_file_names.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define INTWIFI_INTERFACE_NAME "intwifi"
#define WPA_CONF_PATH "/tmp/ruby/wpa_supplicant_intwifi.conf"
#define WIFI_RECONNECT_DELAY_SEC 3

// Reads wifi_connect.cfg: line1=enabled(0/1), line2=SSID, line3=password
// Returns 1 if enabled and valid, 0 otherwise
int _load_wifi_config(int* pEnabled, char* szSSID, int iSSIDMaxLen, char* szPassword, int iPassMaxLen)
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_WIFI_CONNECT);

   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_line("WiFi config file not found: %s", szFile);
      return 0;
   }

   char szLine[256];

   // Line 1: enabled
   if ( NULL == fgets(szLine, sizeof(szLine), fd) )
   {
      fclose(fd);
      return 0;
   }
   removeTrailingNewLines(szLine);
   *pEnabled = atoi(szLine);

   // Line 2: SSID
   if ( NULL == fgets(szSSID, iSSIDMaxLen, fd) )
   {
      fclose(fd);
      return 0;
   }
   removeTrailingNewLines(szSSID);

   // Line 3: Password
   if ( NULL == fgets(szPassword, iPassMaxLen, fd) )
   {
      fclose(fd);
      return 0;
   }
   removeTrailingNewLines(szPassword);

   fclose(fd);

   if ( 0 == szSSID[0] )
   {
      log_line("WiFi SSID is empty.");
      return 0;
   }

   return 1;
}

// Finds the built-in (non-USB) WiFi interface name
// Returns 1 if found, 0 otherwise. Writes interface name to szOutName.
int _find_builtin_wifi_interface(char* szOutName, int iMaxLen)
{
   szOutName[0] = 0;

   // Check if intwifi already exists (already renamed)
   char szCheck[256];
   hw_execute_bash_command_raw("ip link show intwifi 2>/dev/null | head -1", szCheck);
   if ( 0 != szCheck[0] && NULL != strstr(szCheck, "intwifi") )
   {
      strncpy(szOutName, INTWIFI_INTERFACE_NAME, iMaxLen);
      szOutName[iMaxLen-1] = 0;
      log_line("Built-in WiFi interface already renamed to %s", INTWIFI_INTERFACE_NAME);
      return 1;
   }

   // Enumerate wlan interfaces and find the one that is NOT a USB device
   char szOutput[4096];
   hw_execute_bash_command_raw("ls /sys/class/net/ | grep wlan", szOutput);

   char* pLine = strtok(szOutput, "\n");
   while ( pLine != NULL )
   {
      char szBusCheck[512];
      char szBuff[512];

      // Check if this interface is a USB device by looking for busnum in sysfs
      snprintf(szBusCheck, sizeof(szBusCheck), "cat /sys/class/net/%s/device/uevent 2>/dev/null | grep -i usb", pLine);
      szBuff[0] = 0;
      hw_execute_bash_command_raw(szBusCheck, szBuff);

      if ( 0 == szBuff[0] )
      {
         // No USB reference found - this is likely the built-in WiFi
         log_line("Found built-in WiFi interface: %s (no USB bus reference)", pLine);
         strncpy(szOutName, pLine, iMaxLen);
         szOutName[iMaxLen-1] = 0;
         return 1;
      }

      pLine = strtok(NULL, "\n");
   }

   log_line("No built-in WiFi interface found.");
   return 0;
}

// Renames interface to intwifi if not already done
int _rename_interface_to_intwifi(const char* szCurrentName)
{
   if ( 0 == strcmp(szCurrentName, INTWIFI_INTERFACE_NAME) )
      return 1; // Already renamed

   char szComm[256];

   log_line("Renaming %s to %s...", szCurrentName, INTWIFI_INTERFACE_NAME);

   snprintf(szComm, sizeof(szComm), "ip link set %s down 2>/dev/null", szCurrentName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(100);

   snprintf(szComm, sizeof(szComm), "ip link set %s name %s 2>/dev/null", szCurrentName, INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(100);

   // Verify rename
   char szCheck[256];
   hw_execute_bash_command_raw("ip link show intwifi 2>/dev/null | head -1", szCheck);
   if ( NULL == strstr(szCheck, "intwifi") )
   {
      log_softerror_and_alarm("Failed to rename %s to %s", szCurrentName, INTWIFI_INTERFACE_NAME);
      return 0;
   }

   log_line("Successfully renamed %s to %s", szCurrentName, INTWIFI_INTERFACE_NAME);
   return 1;
}

// Writes wpa_supplicant config file
void _write_wpa_config(const char* szSSID, const char* szPassword)
{
   hw_execute_bash_command("mkdir -p /tmp/ruby", NULL);

   FILE* fd = fopen(WPA_CONF_PATH, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to create wpa_supplicant config at %s", WPA_CONF_PATH);
      return;
   }

   fprintf(fd, "ctrl_interface=/var/run/wpa_supplicant\n");
   fprintf(fd, "update_config=1\n\n");
   fprintf(fd, "network={\n");
   fprintf(fd, "    ssid=\"%s\"\n", szSSID);
   if ( 0 != szPassword[0] )
   {
      fprintf(fd, "    psk=\"%s\"\n", szPassword);
      fprintf(fd, "    key_mgmt=WPA-PSK\n");
   }
   else
   {
      fprintf(fd, "    key_mgmt=NONE\n");
   }
   fprintf(fd, "}\n");

   fclose(fd);
   log_line("Wrote wpa_supplicant config to %s for SSID: %s", WPA_CONF_PATH, szSSID);
}

// Stops any running wpa_supplicant on intwifi
void _stop_wpa_supplicant()
{
   hw_execute_bash_command("killall -q wpa_supplicant 2>/dev/null", NULL);
   hardware_sleep_ms(200);
}

// Connects the intwifi interface to the configured WiFi network
int _connect_wifi(const char* szSSID, const char* szPassword)
{
   char szComm[512];

   _stop_wpa_supplicant();

   _write_wpa_config(szSSID, szPassword);

   // Bring interface up
   snprintf(szComm, sizeof(szComm), "ip link set %s up", INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(500);

   // Start wpa_supplicant
   snprintf(szComm, sizeof(szComm), "wpa_supplicant -B -i %s -c %s 2>/dev/null", INTWIFI_INTERFACE_NAME, WPA_CONF_PATH);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(2000);

   // Run DHCP
   snprintf(szComm, sizeof(szComm), "dhclient %s 2>/dev/null &", INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(3000);

   // Check if we got an IP
   char szIP[256];
   szIP[0] = 0;
   snprintf(szComm, sizeof(szComm), "ip addr show %s | grep 'inet ' | awk '{print $2}' | cut -d/ -f1", INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command_raw(szComm, szIP);
   removeTrailingNewLines(szIP);

   if ( 0 != szIP[0] )
   {
      log_line("Internal WiFi connected to %s. IP: %s", szSSID, szIP);
      return 1;
   }
   else
   {
      log_softerror_and_alarm("Internal WiFi failed to get IP on %s", szSSID);
      return 0;
   }
}

// Disconnects and brings down intwifi
void _disconnect_wifi()
{
   char szComm[256];

   snprintf(szComm, sizeof(szComm), "dhclient -r %s 2>/dev/null", INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command(szComm, NULL);

   _stop_wpa_supplicant();

   snprintf(szComm, sizeof(szComm), "ip link set %s down 2>/dev/null", INTWIFI_INTERFACE_NAME);
   hw_execute_bash_command(szComm, NULL);

   log_line("Internal WiFi disconnected and interface down.");
}

// Sets the wifi disabled flag (used by auto-disable on vehicle connect)
void _set_wifi_disabled_flag()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_WIFI_DISABLED);
   FILE* fd = fopen(szFile, "w");
   if ( fd )
   {
      fprintf(fd, "1");
      fclose(fd);
   }
}

void _clear_wifi_disabled_flag()
{
   char szComm[MAX_FILE_PATH_SIZE + 16];
   snprintf(szComm, sizeof(szComm), "rm -f %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_WIFI_DISABLED);
   hw_execute_bash_command(szComm, NULL);
}

int _is_wifi_disabled_by_vehicle()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_WIFI_DISABLED);
   return (access(szFile, R_OK) != -1) ? 1 : 0;
}


int main(int argc, char *argv[])
{
   log_init("RubyWiFiInit");
   log_arguments(argc, argv);

   bool bReconnect = false;
   bool bDisconnect = false;
   bool bRenameOnly = false;

   for ( int i = 1; i < argc; i++ )
   {
      if ( 0 == strcmp(argv[i], "-reconnect") )
         bReconnect = true;
      if ( 0 == strcmp(argv[i], "-disconnect") )
         bDisconnect = true;
      if ( 0 == strcmp(argv[i], "-rename") )
         bRenameOnly = true;
   }

   hardware_detectBoardAndSystemType();

   #if !defined(HW_PLATFORM_RADXA)
   log_line("Internal WiFi support is only available on Radxa platform. Exiting.");
   return 0;
   #endif

   // Find and rename built-in WiFi interface
   char szInterfaceName[64];
   if ( ! _find_builtin_wifi_interface(szInterfaceName, sizeof(szInterfaceName)) )
   {
      log_line("No built-in WiFi interface found. Exiting.");
      return 0;
   }

   if ( ! _rename_interface_to_intwifi(szInterfaceName) )
   {
      log_softerror_and_alarm("Failed to rename built-in WiFi interface. Exiting.");
      return -1;
   }

   if ( bRenameOnly )
   {
      log_line("Rename-only mode. Done.");
      return 0;
   }

   // Handle disconnect request
   if ( bDisconnect )
   {
      _disconnect_wifi();
      _set_wifi_disabled_flag();
      return 0;
   }

   // Handle reconnect request (from vehicle disconnect)
   if ( bReconnect )
   {
      // Debounce: wait before reconnecting
      log_line("Reconnect requested. Waiting %d seconds...", WIFI_RECONNECT_DELAY_SEC);
      sleep(WIFI_RECONNECT_DELAY_SEC);

      // Check if disabled again during wait (quick vehicle reconnect)
      if ( _is_wifi_disabled_by_vehicle() )
      {
         log_line("WiFi was re-disabled during reconnect wait. Aborting reconnect.");
         return 0;
      }
   }

   // Load WiFi config
   int iEnabled = 0;
   char szSSID[128];
   char szPassword[128];
   szSSID[0] = 0;
   szPassword[0] = 0;

   if ( ! _load_wifi_config(&iEnabled, szSSID, sizeof(szSSID), szPassword, sizeof(szPassword)) )
   {
      log_line("No valid WiFi config found. Exiting.");
      return 0;
   }

   if ( ! iEnabled )
   {
      log_line("Internal WiFi is disabled in config. Exiting.");
      return 0;
   }

   _clear_wifi_disabled_flag();

   log_line("Connecting internal WiFi to SSID: %s", szSSID);
   int iResult = _connect_wifi(szSSID, szPassword);

   log_line("Ruby WiFi Init process completed. Connected: %s", iResult ? "yes" : "no");
   return iResult ? 0 : -1;
}
