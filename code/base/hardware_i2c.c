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

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "config_hw.h"


#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RASPBERRY)
#include <wiringPiI2C.h>
#endif
#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RADXA)
#include "wiringPiI2C_radxa.h"
#endif

#include "base.h"
#include "hardware_i2c.h"
#include "hardware_procs.h"
#include "hardware.h"

#define HARDWARE_I2C_FILE_STAMP_ID "V.6.7"

hw_i2c_bus_info_t s_HardwareI2CBusInfo[MAX_I2C_BUS_COUNT];
int s_iHardwareI2CBusCount = 0;
int s_iHardwareI2CBussesEnumerated = 0;
int s_iKnownDevicesFound = 0;
int s_iKnownConfigurableDevicesFound = 0;

t_i2c_device_settings s_listI2CDevicesSettings[MAX_I2C_DEVICES];
int s_iCountI2CDevicesSettings = 0;
int s_iI2CDeviceSettingsLoaded = 0;

void hardware_i2c_reset_enumerated_flag()
{
   s_iHardwareI2CBussesEnumerated = 0;
}

void hardware_i2c_log_devices()
{
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   log_line("[Hardware] -----------------------------------------");
   log_line("[Hardware] I2C Buses: %d", s_iHardwareI2CBusCount);

   char szBuff[256];

   for( int i=0; i<s_iHardwareI2CBusCount; i++ )
   {
      szBuff[0] = 0;
      int iCount = 0;
      for( int k=0; k<128; k++ )
      {
         if ( 1 == s_HardwareI2CBusInfo[i].devices[k] )
         {
            iCount++;
            char szTmp[32];
            if ( 0 == szBuff[0] )
               sprintf(szBuff, "%d", k);
            else
            {
               sprintf(szTmp, ", %d", k);
               strcat(szBuff, szTmp);
            }
         }
      }
      if ( 0 == iCount )
         log_line("[Hardware] No I2C devices on bus %d (%d %s)", i, s_HardwareI2CBusInfo[i].nBusNumber, s_HardwareI2CBusInfo[i].szName);
      else
         log_line("[Hardware] %d I2C devices on bus %d (%d %s): %s", iCount, i, s_HardwareI2CBusInfo[i].nBusNumber, s_HardwareI2CBusInfo[i].szName, szBuff);
   }

   log_line("[Hardware] -----------------------------------------");
   log_line("[Hardware] Settings stored for %d I2C devices:", s_iCountI2CDevicesSettings);
   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      if ( s_listI2CDevicesSettings[i].nDeviceType > 0 )
      if ( s_listI2CDevicesSettings[i].nI2CAddress > 0 )
      {
         sprintf(szBuff, "%d-%d", s_listI2CDevicesSettings[i].nI2CAddress, s_listI2CDevicesSettings[i].nDeviceType);
         log_line("[Hardware] Dev I2CAddress/Type: %s, Custom params: %d %d %d", szBuff, s_listI2CDevicesSettings[i].uParams[0], s_listI2CDevicesSettings[i].uParams[1], s_listI2CDevicesSettings[i].uParams[2]);
      }
   }
   log_line("[Hardware] -----------------------------------------");
}

void hardware_i2c_enumerate_busses(int iConsoleLog)
{
   if ( 0 != s_iHardwareI2CBussesEnumerated )
   {
      log_line("[Hardware] I2C devices already enumerated. %d busses, %d known devices found.", s_iHardwareI2CBusCount, s_iKnownDevicesFound);
      return;
   }
   s_iHardwareI2CBussesEnumerated = 1;
   s_iHardwareI2CBusCount = 0;
   s_iKnownDevicesFound = 0;
   s_iKnownConfigurableDevicesFound = 0;

   log_line("[Hardware]: Enumerating I2C busses...");

#ifdef HW_CAPABILITY_I2C
   char szBuff[256];
   int iEnumerateCount = 4;
   #if defined (HW_PLATFORM_RADXA)
   iEnumerateCount = 7;
   #endif
   for( int i=0; i<iEnumerateCount; i++ )
   {
      snprintf(szBuff, 255, "/dev/i2c-%d", i);
      if( access( szBuff, R_OK ) != -1 )
      if ( s_iHardwareI2CBusCount < MAX_I2C_BUS_COUNT )
      {
         log_line("[Hardware]: Found I2C bus: %s", szBuff);
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].nBusNumber = i;
         snprintf(szBuff, 255, "i2c-%d", i);
         strcpy(s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].szName, szBuff);
         for( int k=0; k<128; k++ )
            s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].devices[k] = 0;
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].picoExtenderVersion = 0;
         s_iHardwareI2CBusCount++;
      }
   }
   #if defined (HW_PLATFORM_RASPBERRY)
   for( int i=10; i<14; i++ )
   {
      snprintf(szBuff, 255, "/dev/i2c-%d", i);
      if( access( szBuff, R_OK ) != -1 )
      if ( s_iHardwareI2CBusCount < MAX_I2C_BUS_COUNT )
      {
         log_line("[Hardware]: Found I2C bus: %s", szBuff);
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].nBusNumber = i;
         snprintf(szBuff, 255, "i2c-%d", i);
         strcpy(s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].szName, szBuff);
         for( int k=0; k<128; k++ )
            s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].devices[k] = 0;
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].picoExtenderVersion = 0;
         s_iHardwareI2CBusCount++;
      }
   }
   for( int i=20; i<24; i++ )
   {
      snprintf(szBuff, 255, "/dev/i2c-%d", i);
      if( access( szBuff, R_OK ) != -1 )
      if ( s_iHardwareI2CBusCount < MAX_I2C_BUS_COUNT )
      {
         log_line("[Hardware]: Found I2C bus: %s", szBuff);
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].nBusNumber = i;
         snprintf(szBuff, 255, "i2c-%d", i);
         strcpy(s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].szName, szBuff);
         for( int k=0; k<128; k++ )
            s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].devices[k] = 0;
         s_HardwareI2CBusInfo[s_iHardwareI2CBusCount].picoExtenderVersion = 0;
         s_iHardwareI2CBusCount++;
      }
   }
   #endif
#else
   log_line("[Hardware] I2C is disabled in code.");
#endif
   log_line("[Hardware]: Found %d I2C busses.", s_iHardwareI2CBusCount);

   if ( 1 == iConsoleLog )
      printf("Ruby: Found %d I2C busses. Enumerating I2C devices...\n", s_iHardwareI2CBusCount);

   int countDevicesTotal = 0;

#ifdef HW_CAPABILITY_I2C
   u32 uBoardType = hardware_getBoardType();
   char szOutput[2048];
   for( int i=0; i<s_iHardwareI2CBusCount; i++ )
   {
      for( int k=0; k<128; k++ )
         s_HardwareI2CBusInfo[i].devices[k] = 0;
      int iCountDevicesFoundOnThisBus = 0;

      for( int k=0; k<8; k++ )
      {
         int addrStart = k*16;
         int addrEnd = k*16+15;
         if ( k == 0 )
            addrStart = 8;
         if ( k == 7 )
            addrEnd = (7*16)+7;

         if ( ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B) && (0 == s_HardwareI2CBusInfo[i].nBusNumber) )
         {
            if ( k == 0 )
               addrStart = addrEnd = I2C_DEVICE_ADDRESS_CAMERA_HDMI;
            if ( k == 1 )
               addrStart = addrEnd = I2C_DEVICE_ADDRESS_CAMERA_CSI;
            if ( k == 2 )
               addrStart = addrEnd = I2C_DEVICE_ADDRESS_CAMERA_VEYE;
            if ( k == 3 )
               addrStart = addrEnd = I2C_DEVICE_ADDRESS_PICO_EXTENDER;
            if ( k > 3 )
               break;
            log_line("[Hardware]: Pi4: Searching for device on bus %d at address: 0x%02X", s_HardwareI2CBusInfo[i].nBusNumber, addrStart);
         }

         snprintf( szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "i2cdetect -y %d 0x%02X 0x%02X | tr '\n' ' ' | sed -e 's/[^0-9a-fA-F]/ /g' -e 's/^ *//g' -e 's/ *$//g' | tr -s ' ' | sed $'s/ /\\\n/g'", s_HardwareI2CBusInfo[i].nBusNumber, addrStart, addrEnd);
         hw_execute_bash_command_raw_silent(szBuff, szOutput);

         if ( 0 == szOutput[0] )
            continue;
         const char* szTokens = " \n";
         char* szContext = szOutput;
         char* szWord = NULL;
         int count = 0;
         int toSkip = 16 + k + 1;
         while ( (szWord = strtok_r(szContext, szTokens, &szContext)) )
         {
            count++;
            if ( count <= toSkip )
              continue;
            long l = strtol(szWord, NULL, 16);
            if ( l >= 16*(k+1) )
              break;
            if ( (l >= addrStart) && (l <= addrEnd) && (l >= 0) && (l < 128) )
            {
               s_HardwareI2CBusInfo[i].devices[l] = 1;
               char szDeviceName2[256];
               hardware_i2c_get_device_name(l, szDeviceName2);
               log_line("[Hardware]: Found I2C device on bus I2C-%d at address 0x%02X, device type: %s", s_HardwareI2CBusInfo[i].nBusNumber, (int)l, szDeviceName2);
               if ( 1 == iConsoleLog )
               if ( hardware_is_known_i2c_device((u8)l) )
               {
                  printf("Ruby: Found I2C device on bus I2C-%d at address 0x%02X, device type: %s\n", s_HardwareI2CBusInfo[i].nBusNumber, (int)l, szDeviceName2);
                  fflush(stdout);
               }
               if ( hardware_is_known_i2c_device((u8)l) )
                  s_iKnownDevicesFound++;

               if ( (l == I2C_DEVICE_ADDRESS_PICO_EXTENDER) ||
                    (l == I2C_DEVICE_ADDRESS_INA219_1) ||
                    (l == I2C_DEVICE_ADDRESS_INA219_2) ||
                    (l == I2C_DEVICE_ADDRESS_SSD1306_1) ||
                    (l == I2C_DEVICE_ADDRESS_SSD1306_2) ||
                    ((l >= I2C_DEVICE_MIN_ADDRESS_RANGE) && (l <= I2C_DEVICE_MAX_ADDRESS_RANGE)) )
                  s_iKnownConfigurableDevicesFound++;

               if ( l == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
               {
                  log_line("[Hardware]: Found Pico Extender I2C device. Getting version info...");
                  int nFile = wiringPiI2CSetup(I2C_DEVICE_ADDRESS_PICO_EXTENDER);
                  int nVersion = 0;
                  if ( nFile > 0 )
                  {
                     nVersion = wiringPiI2CReadReg8(nFile, I2C_DEVICE_COMMAND_ID_GET_VERSION);
                     close(nFile);
                  }
                  log_line("[Hardware]: Got Pico Extender version: %d.%d", nVersion>>4, nVersion & 0x0F);
                  s_HardwareI2CBusInfo[i].picoExtenderVersion = (u8)nVersion;
               }
               countDevicesTotal++;
               iCountDevicesFoundOnThisBus++;
            }
         }
      }
      if ( iCountDevicesFoundOnThisBus > 50 )
      {
         log_softerror_and_alarm("[Hardware]: Found too many I2C devices on a single I2C Bus. Probabbly it's broken. Invalidate all detected devices on this bus.");
         for( int k=0; k<128; k++ )
            s_HardwareI2CBusInfo[i].devices[k] = 0;
         countDevicesTotal -= iCountDevicesFoundOnThisBus;
      }
   }
#endif
   log_line("[Hardware]: Found a total of %d I2C devices on all busses.", countDevicesTotal);
   if ( 1 == iConsoleLog )
      printf("Ruby: Found a total of %d I2C devices.\n", countDevicesTotal);
}

int hardware_i2c_get_found_count_known_devices()
{
   return s_iKnownDevicesFound;
}

int hardware_i2c_get_found_count_configurable_devices()
{
   return s_iKnownConfigurableDevicesFound;
}

int hardware_i2c_get_busses_count()
{
   return s_iHardwareI2CBusCount;
}

hw_i2c_bus_info_t* hardware_i2c_get_bus_info(int busIndex)
{
   if ( busIndex < 0 || busIndex >= s_iHardwareI2CBusCount )
      return NULL;
   return &(s_HardwareI2CBusInfo[busIndex]);
}

int hardware_i2c_has_device_id(u8 deviceAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( (deviceAddress >= 128) || (deviceAddress < 8) )
      return 0;
   if ( 0 == s_iHardwareI2CBusCount )
      return 0;

   for( int i=0; i<s_iHardwareI2CBusCount; i++ )
      if ( 1 == s_HardwareI2CBusInfo[i].devices[deviceAddress] )
         return 1;
   #endif
   return 0;
}

int hardware_i2c_get_device_bus_number(u8 deviceAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( deviceAddress >= 128 )
      return -1;
   if ( 0 == s_iHardwareI2CBusCount )
      return -1;

   for( int i=0; i<s_iHardwareI2CBusCount; i++ )
      if ( 1 == s_HardwareI2CBusInfo[i].devices[deviceAddress] )
         return s_HardwareI2CBusInfo[i].nBusNumber;
   #endif
   return -1;
}

int hardware_is_known_i2c_device(u8 deviceAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   #if defined (HW_PLATFORM_RASPBERRY)
   if ( (deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_HDMI) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_CSI) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_VEYE) )
     return 1;
   #endif

   if ( (deviceAddress == I2C_DEVICE_ADDRESS_INA219_1) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_INA219_2) )
     return 1;

   if ( (deviceAddress == I2C_DEVICE_ADDRESS_PICO_RC_IN) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_PICO_EXTENDER) )
     return 1;

   if ( (deviceAddress == I2C_DEVICE_ADDRESS_SSD1306_1) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_SSD1306_2) )
      return 1;

   if ( deviceAddress >= I2C_DEVICE_MIN_ADDRESS_RANGE &&
        deviceAddress <= I2C_DEVICE_MAX_ADDRESS_RANGE )
      return 1;
   #endif
   return 0;
}

void hardware_i2c_get_device_name(u8 deviceAddress, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   strcpy(szOutput, I2C_DEVICE_NAME_UNKNOWN);

   #if defined (HW_CAPABILITY_I2C)
   if ( deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_HDMI )
      strcpy(szOutput, I2C_DEVICE_NAME_CAMERA_HDMI);
   #if defined (HW_PLATFORM_RASPBERRY)
   if ( deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_CSI )
      strcpy(szOutput, I2C_DEVICE_NAME_CAMERA_CSI);
   #endif
   if ( deviceAddress == I2C_DEVICE_ADDRESS_CAMERA_VEYE )
      strcpy(szOutput, I2C_DEVICE_NAME_CAMERA_VEYE);

   if ( (deviceAddress == I2C_DEVICE_ADDRESS_INA219_1) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_INA219_2) )
      strcpy(szOutput, I2C_DEVICE_NAME_INA219);

   if ( deviceAddress == I2C_DEVICE_ADDRESS_PICO_RC_IN )
      strcpy(szOutput, I2C_DEVICE_NAME_RC_IN);
   if ( deviceAddress == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
      strcpy(szOutput, I2C_DEVICE_NAME_PICO_EXTENDER);

   if ( (deviceAddress == I2C_DEVICE_ADDRESS_SSD1306_1) ||
        (deviceAddress == I2C_DEVICE_ADDRESS_SSD1306_2) )
      strcpy(szOutput, I2C_DEVICE_NAME_OLED_SCREEN);

   if ( (deviceAddress >= I2C_DEVICE_MIN_ADDRESS_RANGE) &&
        (deviceAddress <= I2C_DEVICE_MAX_ADDRESS_RANGE) )
      strcpy(szOutput, I2C_DEVICE_NAME_RUBY_ADDON);
   #endif
}


int hardware_i2c_save_device_settings()
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_HARDWARE_I2C_DEVICES);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Hardware]: Failed to save I2C devices settings to file: %s",szFile);
      return 0;
   }

   fprintf(fd, "%s\n", HARDWARE_I2C_FILE_STAMP_ID);
   
   fprintf(fd, "I2C_Device_Settings: %d\n", s_iCountI2CDevicesSettings );
   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      char szName[MAX_I2C_DEVICE_NAME];
      strcpy(szName, s_listI2CDevicesSettings[i].szDeviceName );
      int len = strlen(szName);
      for( int j=0; j<len; j++ )
         if ( szName[j] == ' ' )
            szName[j] = '~';
      // last value is count of extra values to read
      fprintf(fd, "%d %d %d %s %d %d %d %u\n", s_listI2CDevicesSettings[i].nI2CAddress, s_listI2CDevicesSettings[i].nVersion, s_listI2CDevicesSettings[i].nDeviceType, szName, s_listI2CDevicesSettings[i].bConfigurable, s_listI2CDevicesSettings[i].bEnabled, 0, s_listI2CDevicesSettings[i].uCapabilitiesFlags );
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         fprintf(fd, "%u ", s_listI2CDevicesSettings[i].uParams[k]);
      fprintf(fd, "\n");
   }

   if ( NULL != fd )
      fclose(fd);

   log_line("[Hardware]: Saved I2C devices settings to file: %s", szFile);
   log_line("[Hardware]: Saved I2C devices settings for %d I2C devices.", s_iCountI2CDevicesSettings);
   #endif
   return 1;
}

int hardware_i2c_load_device_settings()
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_HARDWARE_I2C_DEVICES);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (missing file). Recreated the file.", szFile);
      hardware_i2c_save_device_settings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, HARDWARE_I2C_FILE_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid file stamp)", szFile);
      fclose(fd);
      hardware_i2c_save_device_settings();
      return 0;
   }

   s_iCountI2CDevicesSettings = 0;
   if ( 1 != fscanf(fd, "%*s %d", &s_iCountI2CDevicesSettings) )
   {
      s_iCountI2CDevicesSettings = 0;
      failed = 16;
      log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid i2c count)", szFile);
      hardware_i2c_save_device_settings();
   }

   if ( s_iCountI2CDevicesSettings >= MAX_I2C_DEVICES )
      s_iCountI2CDevicesSettings = MAX_I2C_DEVICES-1;

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      if ( failed != 0 )
         break;

      int i2cAddress = 0;
      int deviceType = 0;
      int nVersion = 0;
      if ( 4 != fscanf(fd, "%d %d %d %s", &i2cAddress, &nVersion, &deviceType, s_listI2CDevicesSettings[i].szDeviceName) )
      {
         log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid i2c data)", szFile);
         failed = 17;
       	 break;
      }
      int len = strlen(s_listI2CDevicesSettings[i].szDeviceName);
      for( int j=0; j<len; j++ )
         if ( s_listI2CDevicesSettings[i].szDeviceName[j] == '~' )
            s_listI2CDevicesSettings[i].szDeviceName[j] = ' ';
      int tmp1;
      int tmp2;
      int extraFields = 0;
      u32 extraValue = 0;
      if ( 4 != fscanf(fd, "%d %d %d %u", &tmp1, &tmp2, &extraFields, &extraValue) )
      {
         log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid i2c data2)", szFile);
         failed = 18;
         break;
      }
      s_listI2CDevicesSettings[i].uCapabilitiesFlags = extraValue;

      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         if ( 1 != fscanf(fd, "%u", &(s_listI2CDevicesSettings[i].uParams[k]) ) )
         {
            log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid i2c data3)", szFile);
            failed = 18;
            break;
         }
      for( int k=0; k<extraFields; k++ )
         if ( 1 != fscanf(fd, "%u", &extraValue ) )
         {
            log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid i2c data4)", szFile);
            failed = 19;
            break;
         }
      if ( i2cAddress > 0 && i2cAddress < 128 )
      {
         s_listI2CDevicesSettings[i].nI2CAddress = i2cAddress;
         s_listI2CDevicesSettings[i].nVersion = nVersion;
         s_listI2CDevicesSettings[i].nDeviceType = deviceType;
         //log_line("I2C dev 0x%02X version: %d", i2cAddress, nVersion );

         s_listI2CDevicesSettings[i].bConfigurable = tmp1;
         s_listI2CDevicesSettings[i].bEnabled = tmp2;
      }
   }

   fclose(fd);

   if ( failed )
   {
      s_iCountI2CDevicesSettings = 0;
      log_softerror_and_alarm("[Hardware]: Failed to load I2C devices settings from file: %s (invalid config file, error code: %d)", szFile, failed);
      return 0;
   }

   s_iI2CDeviceSettingsLoaded = 1;
   log_line("[Hardware]: Loaded I2C devices settings from file: %s", szFile);
   log_line("[Hardware]: Loaded I2C devices settings for %d I2C devices", s_iCountI2CDevicesSettings);
   #endif
   return 1;
}

int hardware_i2c_HasPicoExtender() // to deprecate
{
   #if defined (HW_CAPABILITY_I2C)
   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER )
         return 1;
   #endif
   return 0;
}

int hardware_i2c_get_pico_extender_version()
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   for( int i=0; i<s_iHardwareI2CBusCount; i++ )
      if ( 1 == s_HardwareI2CBusInfo[i].devices[I2C_DEVICE_ADDRESS_PICO_EXTENDER] )
         return (int)(s_HardwareI2CBusInfo[i].picoExtenderVersion);
   #endif
   return 0;
}

t_i2c_device_settings* hardware_i2c_get_device_settings(u8 i2cAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      if ( s_listI2CDevicesSettings[i].nI2CAddress == i2cAddress )
      {
         log_line("[Hardware]: Got stored device settings for I2C device 0x%02X", (u8)i2cAddress);
         return &(s_listI2CDevicesSettings[i]);
      }
   }
   #endif
   log_line("No device settings stored for I2C device 0x%02X !", (u8)i2cAddress);
   return NULL;
}

t_i2c_device_settings* hardware_i2c_add_device_settings(u8 i2cAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nI2CAddress == i2cAddress )
         return &(s_listI2CDevicesSettings[i]);

   if ( s_iCountI2CDevicesSettings >= MAX_I2C_DEVICES )
      return NULL;

   log_line("[Hardware]: Adding settings for I2C device address: 0x%02X; current I2C devices count: %d", i2cAddress, s_listI2CDevicesSettings );

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( i2cAddress == I2C_DEVICE_ADDRESS_CAMERA_CSI )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_CAMERA_CSI);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_CAMERA_CSI;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }
   #endif

   if ( i2cAddress == I2C_DEVICE_ADDRESS_CAMERA_HDMI )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_CAMERA_HDMI);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_CAMERA_HDMI;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }

   if ( i2cAddress == I2C_DEVICE_ADDRESS_CAMERA_VEYE )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_CAMERA_VEYE);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_CAMERA_VEYE;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }

   if ( (i2cAddress == I2C_DEVICE_ADDRESS_INA219_1) ||
        (i2cAddress == I2C_DEVICE_ADDRESS_INA219_2) )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_INA219);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_INA219;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 1;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }

   if ( (i2cAddress == I2C_DEVICE_ADDRESS_SSD1306_1) || (i2cAddress == I2C_DEVICE_ADDRESS_SSD1306_2) )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_OLED_SCREEN);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_OLED_SCREEN;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }

   if ( 1 )
   {
      strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_UNKNOWN);
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nI2CAddress = i2cAddress;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_UNKNOWN;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 1;
      s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 0;

      if ( (i2cAddress >= I2C_DEVICE_MIN_ADDRESS_RANGE) && (i2cAddress <= I2C_DEVICE_MAX_ADDRESS_RANGE) )
      {
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_RUBY_ADDON;
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      }
      for( int k=0; k<MAX_I2C_DEVICE_SETTINGS; k++ )
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[k] = 0;

      if ( i2cAddress == I2C_DEVICE_ADDRESS_PICO_RC_IN )
      {
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_PICO_RC_IN;
         strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_RC_IN);
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[0] = 0; // SBUS
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[1] = 1; // Inverted
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 1;
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      }

      if ( i2cAddress == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
      {
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].nDeviceType = I2C_DEVICE_TYPE_PICO_EXTENDER;
         strcpy(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].szDeviceName, I2C_DEVICE_NAME_PICO_EXTENDER);
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[0] = 0; // SBUS
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[1] = 1; // Inverted
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[2] = 1; // Rotary Encoder does: Menu navigation
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uParams[3] = 1; // Rotary Encoder speed: slow
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].uCapabilitiesFlags = 0;
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bConfigurable = 1;
         s_listI2CDevicesSettings[s_iCountI2CDevicesSettings].bEnabled = 1;
      }

      s_iCountI2CDevicesSettings++;
      return &(s_listI2CDevicesSettings[s_iCountI2CDevicesSettings-1]);
   }
   #endif
   return NULL;
}


void hardware_i2c_update_device_info(u8 i2cAddress)
{
   #if defined (HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   int index = -1;
   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nI2CAddress == i2cAddress )
      {
         index = i;
         break;
      }

   if ( index == -1 )
   {
      hardware_i2c_add_device_settings(i2cAddress);
      return;
   }

   log_line("[Hardware]: Updating settings for I2C device address: 0x%02X; current I2C devices count: %d", i2cAddress, s_iCountI2CDevicesSettings );

   if ( i2cAddress == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
   {
      s_listI2CDevicesSettings[index].nDeviceType = I2C_DEVICE_TYPE_PICO_EXTENDER;
      strcpy(s_listI2CDevicesSettings[index].szDeviceName, I2C_DEVICE_NAME_PICO_EXTENDER);
      s_listI2CDevicesSettings[index].uParams[0] = 0; // SBUS
      s_listI2CDevicesSettings[index].uParams[1] = 1; // Inverted
      s_listI2CDevicesSettings[index].uParams[2] = 1; // Rotary Encoder does: Menu navigation
      s_listI2CDevicesSettings[index].uParams[3] = 1; // Rotary Encoder speed: slow
      s_listI2CDevicesSettings[index].bConfigurable = 1;
      s_listI2CDevicesSettings[index].bEnabled = 1;
   }
   #endif
}

// returns 1 if settings where updated, 0 if no change
int _hardware_i2c_check_and_update_extender_device_settings(u8 i2cAddress)
{
#ifdef HW_CAPABILITY_I2C
   t_i2c_device_settings* pDeviceInfo = hardware_i2c_get_device_settings(i2cAddress);
   int iUpdatedI2CSettings = 0;
   if ( NULL == pDeviceInfo )
   {
      log_softerror_and_alarm("[Hardware]: Can't create/get device settings for I2C device id: %02X.", i2cAddress);
      return 0;
   }

   if ( pDeviceInfo->nDeviceType != I2C_DEVICE_TYPE_RUBY_ADDON )
   {
      log_line("[Hardware]: Set I2C device id 0x%02X as an external Ruby addon/extender device", i2cAddress);
      pDeviceInfo->nDeviceType = I2C_DEVICE_TYPE_RUBY_ADDON;
      iUpdatedI2CSettings = 1;
   }

   char szBuff[128];
   u8 bufferOut[32];
   u8 bufferIn[64];
   int fd = wiringPiI2CSetup(i2cAddress);

   if ( fd < 0 )
   {
      log_softerror_and_alarm("[Hardware]: Can't open file handle to I2C device id 0x%02X.", i2cAddress);
      return iUpdatedI2CSettings;
   }

   bufferOut[0] = I2C_COMMAND_START_FLAG;
   bufferOut[1] = I2C_COMMAND_ID_GET_FLAGS;
   bufferOut[2] = base_compute_crc8(bufferOut,2);
   wiringPiI2CWrite(fd, bufferOut[0]);
   wiringPiI2CWrite(fd, bufferOut[1]);
   wiringPiI2CWrite(fd, bufferOut[2]);

   int res1 = wiringPiI2CRead(fd);
   int res2 = wiringPiI2CRead(fd);
   int res3 = wiringPiI2CRead(fd);
   log_line("[Hardware]: Got I2C external device 0x%02X flags: %d, %d", i2cAddress, res1, res2);
   if ( res1 >= 0 && res2 >= 0 && res3 >= 0 )
   {
      bufferIn[0] = res1;
      bufferIn[1] = res2;
      bufferIn[2] = base_compute_crc8(bufferIn,2);
      if ( bufferIn[2] != res3 )
         log_softerror_and_alarm("[Hardware]: Received incorrect CRC on I2C command flags response.");

      u32 uFlags = ((u32)res1) | (((u32)res2)<<8);
     
      if ( pDeviceInfo->uCapabilitiesFlags != uFlags )
      {
         log_line("[Hardware]: Got I2C device 0x%02X flags: %u. Different from old ones. Updating local info.", i2cAddress, uFlags);
         pDeviceInfo->uCapabilitiesFlags = uFlags;
         iUpdatedI2CSettings = 1;
      }
      else
         log_line("[Hardware]: Got I2C device 0x%02X flags: %u. Unchanged.", i2cAddress, uFlags);
   }

   int iTryGetName = 10;

   while ( iTryGetName > 0 )
   {
      iTryGetName--;

      bufferOut[0] = I2C_COMMAND_START_FLAG;
      bufferOut[1] = I2C_COMMAND_ID_GET_NAME;
      bufferOut[2] = base_compute_crc8(bufferOut,2);
      wiringPiI2CWrite(fd, bufferOut[0]);
      wiringPiI2CWrite(fd, bufferOut[1]);
      wiringPiI2CWrite(fd, bufferOut[2]);

      szBuff[0] = 0;
      int res = 0;
      for( int i=0; i<=I2C_PROTOCOL_STRING_LENGTH; i++ )
      {
         res = wiringPiI2CRead(fd);
         if ( res < 0 )
         {
            szBuff[i] = 0;
            break;
         }
         szBuff[i] = res;
         bufferIn[i] = res;
      }
      if ( res < 0 )
      {
         log_softerror_and_alarm("[Hardware]: Failed to get I2C full response to get name command.");         
         continue;
      }
      if ( bufferIn[I2C_PROTOCOL_STRING_LENGTH] != base_compute_crc8(bufferIn,I2C_PROTOCOL_STRING_LENGTH) )
      {
         log_softerror_and_alarm("[Hardware]: Received incorrect CRC on I2C command get name response.");
         continue;
      }

      szBuff[I2C_PROTOCOL_STRING_LENGTH-1] = 0;
      if ( 0 == szBuff[0] )
      {
         log_line("[Hardware]: Could not get I2C device 0x%02X name. No response from device.", i2cAddress);
         continue;
      }
      else if ( 0 == strcmp(szBuff, pDeviceInfo->szDeviceName) )
         log_line("[Hardware]: Got I2C device 0x%02X name: [%s]. Unchanged.", i2cAddress, szBuff);
      else
      {
         log_line("[Hardware]: Got I2C device 0x%02X name: [%s]. Different from old one. Updating local info.", i2cAddress, szBuff);
         strncpy(pDeviceInfo->szDeviceName, szBuff, MAX_I2C_DEVICE_NAME-1);
         pDeviceInfo->szDeviceName[MAX_I2C_DEVICE_NAME-1] = 0;
         iUpdatedI2CSettings = 1;
      }
      iTryGetName = 0;
   }
   close(fd);

   log_line("[Hardware]: Checked info for I2C device 0x%02X. Any changes? %s", i2cAddress, (iUpdatedI2CSettings?"Yes":"No"));
   return iUpdatedI2CSettings;
#else
   log_line("[Hardware] I2C devices disabled in code (2).");
   return 0;
#endif
}

void hardware_i2c_check_and_update_device_settings()
{
   #if defined(HW_CAPABILITY_I2C)
   char szBuff[128];
   int iUpdatedI2CSettings = 0;

   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( u8 u=0; u<128; u++ )
   {
      if ( ! hardware_i2c_has_device_id(u) )
         continue;

      hardware_i2c_get_device_name(u, szBuff);
      if ( hardware_is_known_i2c_device(u) )
         log_line("[Hardware]: Found known I2C device at address: %02X, type: [%s]", u, szBuff);
      else
         log_line("[Hardware]: Found unknown I2C device at address: %02X, type: [%s]", u, szBuff);

      t_i2c_device_settings* pDeviceInfo = hardware_i2c_get_device_settings(u);
      if ( NULL == pDeviceInfo )
      {
         hardware_i2c_add_device_settings(u);
         iUpdatedI2CSettings = 1;
      }
      else if ( pDeviceInfo->nDeviceType == I2C_DEVICE_TYPE_UNKNOWN )
      {
         hardware_i2c_update_device_info(u);
         iUpdatedI2CSettings = 1;
      }

      pDeviceInfo = hardware_i2c_get_device_settings(u);
      if ( NULL == pDeviceInfo )
      {
         log_softerror_and_alarm("[Hardware]: Can't create/get device settings for I2C device id: %02X.", u);
         continue;
      }
      if ( u == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
      {
         pDeviceInfo->nVersion = hardware_i2c_get_pico_extender_version();
         iUpdatedI2CSettings = 1;
      }

      if ( u >= I2C_DEVICE_MIN_ADDRESS_RANGE && u <= I2C_DEVICE_MAX_ADDRESS_RANGE )
      {
         log_line("[Hardware]: I2C device id 0x%02X is an external configurable extender. Quering info from it...", u);
         if ( _hardware_i2c_check_and_update_extender_device_settings(u) )
            iUpdatedI2CSettings = 1;
      }
   }

   log_line("[Hardware]: Done cheching for I2C devices settings.");
   if ( iUpdatedI2CSettings )
   {
      log_line("[Hardware]: I2C devices settings where updated. Saving new config.");
      hardware_i2c_save_device_settings();
   }
   #endif
}


int hardware_i2c_has_current_sensor()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_INA219 )
         return 1;
   #endif
   return 0;
}

int hardware_i2c_has_external_extenders()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
         return 1;
   #endif
   return 0;
}

int hardware_i2c_has_external_extenders_rotary_encoders()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
      {
         if ( s_listI2CDevicesSettings[i].uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY )
            return 1;
         if ( s_listI2CDevicesSettings[i].uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY2 )
            return 1;
      }
   #endif
   return 0;
}

int hardware_i2c_has_external_extenders_buttons()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
      {
         if ( s_listI2CDevicesSettings[i].uCapabilitiesFlags & I2C_CAPABILITY_FLAG_BUTTONS )
            return 1;
      }
   #endif
   return 0;
}

int hardware_i2c_has_external_extenders_rcin()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
      {
         if ( s_listI2CDevicesSettings[i].uCapabilitiesFlags & I2C_CAPABILITY_FLAG_RC_INPUT )
            return s_listI2CDevicesSettings[i].nI2CAddress;
      }
   }
   #endif
   return 0; 
}

int hardware_i2c_has_oled_screen()
{
   #if defined(HW_CAPABILITY_I2C)
   if ( 0 == s_iHardwareI2CBussesEnumerated )
      hardware_i2c_enumerate_busses(0);

   if ( ! s_iI2CDeviceSettingsLoaded )
      hardware_i2c_load_device_settings();

   for( int i=0; i<s_iCountI2CDevicesSettings; i++ )
   {
      if ( s_listI2CDevicesSettings[i].nDeviceType == I2C_DEVICE_TYPE_OLED_SCREEN )
         return s_listI2CDevicesSettings[i].nI2CAddress;
   }
   #endif
   return 0; 
}
