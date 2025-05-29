/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_radio.h"
#include "hardware_serial.h"
#include "hardware_radio_sik.h"
#include "hw_procs.h"
#include "../common/string_utils.h"

#define MAX_USB_DEVICES_INFO 24

static int s_iEnumeratedUSBRadioInterfaces = 0;

static usb_radio_interface_info_t s_USB_RadioInterfacesInfo[MAX_USB_DEVICES_INFO];
static int s_iFoundUSBRadioInterfaces = 0;

static radio_hw_info_t sRadioInfo[MAX_RADIO_INTERFACES];
static int s_iHwRadiosCount = 0;
static int s_iHwRadiosSupportedCount = 0;
static int s_HardwareRadiosEnumeratedOnce = 0;

void reset_runtime_radio_rx_info(type_runtime_radio_rx_info* pRuntimeRadioRxInfo)
{
   if ( NULL == pRuntimeRadioRxInfo )
      return;

   memset(pRuntimeRadioRxInfo, 0, sizeof(type_runtime_radio_rx_info));
   pRuntimeRadioRxInfo->nChannel = 0;
   pRuntimeRadioRxInfo->nChannelFlags = 0;
   pRuntimeRadioRxInfo->nFreq = 0;
   pRuntimeRadioRxInfo->nDataRateBPSMCS = 0; // positive: bps, negative: mcs rate, 0: never
   pRuntimeRadioRxInfo->nRadiotapFlags = 0;
   pRuntimeRadioRxInfo->nAntennaCount = 1;
   reset_runtime_radio_rx_info_dbminfo(pRuntimeRadioRxInfo);
}

void reset_runtime_radio_rx_info_dbminfo(type_runtime_radio_rx_info* pRuntimeRadioRxInfo)
{
   if ( NULL == pRuntimeRadioRxInfo )
      return;

   for( int i=0; i<MAX_RADIO_ANTENNAS; i++ )
   {
      pRuntimeRadioRxInfo->nDbmLast[i] = 1000;
      pRuntimeRadioRxInfo->nDbmLastChange[i] = 1000;
      pRuntimeRadioRxInfo->nDbmAvg[i] = 1000;
      pRuntimeRadioRxInfo->nDbmMin[i] = 1000;
      pRuntimeRadioRxInfo->nDbmMax[i] = 1000;
      pRuntimeRadioRxInfo->nDbmNoiseLast[i] = 1000;
      pRuntimeRadioRxInfo->nDbmNoiseAvg[i] = 1000;
      pRuntimeRadioRxInfo->nDbmNoiseMin[i] = 1000;
      pRuntimeRadioRxInfo->nDbmNoiseMax[i] = 1000;
      pRuntimeRadioRxInfo->uLastTimeCapture[i] = 0;
   }
}

radio_hw_info_t* hardware_get_radio_info_array()
{
   return &(sRadioInfo[0]);
}

void hardware_log_radio_info(radio_hw_info_t* pRadioInfoArray, int iCount)
{
   char szBuff[1024];

   log_line("");
   if ( NULL == pRadioInfoArray )
   {
      pRadioInfoArray = &sRadioInfo[0];
      iCount = s_iHwRadiosCount;
   }

   for( int i=0; i<iCount; i++ )
   {
      log_line("* RadioInterface %d: %s MAC:%s phy#%d, supported: %s, USBPort: %s now at %s;", 
         i+1, pRadioInfoArray[i].szName, pRadioInfoArray[i].szMAC, pRadioInfoArray[i].phy_index,
         pRadioInfoArray[i].isSupported?"yes":"no",
         pRadioInfoArray[i].szUSBPort, str_format_frequency(pRadioInfoArray[i].uCurrentFrequencyKhz));

      sprintf( szBuff, "   Type: %s, %s, pid/vid: %s driver: %s, ", 
         str_get_radio_card_model_string(pRadioInfoArray[i].iCardModel), pRadioInfoArray[i].szDescription, pRadioInfoArray[i].szProductId, pRadioInfoArray[i].szDriver);
      
      char szBands[128];
      str_get_supported_bands_string(pRadioInfoArray[i].supportedBands, szBands);
      strcat( szBuff, szBands);
      log_line(szBuff);
      log_line("");
   }
   log_line("=================================================================");
}

void hardware_radio_remove_stored_config()
{
   char szComm[MAX_FILE_PATH_SIZE];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);
}

void hardware_save_radio_info()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   log_line("[HardwareRadio] Saving hardware radio config to file (%s)...", szFile);
   FILE* fp = fopen(szFile, "w");
   if ( NULL == fp )
   {
      log_softerror_and_alarm("Failed to save radio HW info file.");
      return;
   }
   u8 buffer[9056];
   int pos = 0;
   memcpy(buffer+pos, &s_iHwRadiosCount, sizeof(s_iHwRadiosCount));
   pos += sizeof(s_iHwRadiosCount);
   memcpy(buffer+pos, &s_iHwRadiosSupportedCount, sizeof(s_iHwRadiosSupportedCount));
   pos += sizeof(s_iHwRadiosSupportedCount);
   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      memcpy(buffer+pos, &(sRadioInfo[i]), sizeof(radio_hw_info_t));
      pos += sizeof(radio_hw_info_t);
   }
   u32 crc = base_compute_crc32(buffer, pos);
   memcpy( buffer+pos, &crc, sizeof(u32));
   pos += sizeof(u32);

   fwrite( buffer, 1, pos, fp );
   fclose(fp);
   log_line("[HardwareRadio] Saved hardware radio config to file (%s).", szFile);
}

int hardware_load_radio_info()
{
   return hardware_load_radio_info_into_buffers(&s_iHwRadiosCount, &s_iHwRadiosSupportedCount, &sRadioInfo[0]);
}

int hardware_load_radio_info_into_buffers(int* piOutputTotalCount, int* piOutputSupportedCount, radio_hw_info_t* pRadioInfoArray)
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);

   log_line("[HardwareRadio] Loading hardware radio config from file (%s)...", szFile);

   if ( (access(szFile, R_OK) == -1) || (NULL == pRadioInfoArray) )
   {
      log_line("No radio HW info file.");
      if ( NULL != piOutputTotalCount )
         *piOutputTotalCount = 0;
      if ( NULL != piOutputSupportedCount )
         *piOutputSupportedCount = 0;
      return 0;
   }

   int succeeded = 1;
   u8 bufferIn[9056];
   int posIn = 0;

   int iTotalCount = 0;
   int iSupportedCount = 0;
   FILE* fp = fopen(szFile, "r");
   if ( NULL == fp )
      succeeded = 0;

   // Read total interfaces count
   if ( succeeded )
   {
      if ( sizeof(iTotalCount) != fread(&iTotalCount, 1, sizeof(iTotalCount), fp) )
         succeeded = 0;
      if ( (iTotalCount < 0) || (iTotalCount >= MAX_RADIO_INTERFACES) )
         succeeded = 0;
      memcpy(bufferIn+posIn, &iTotalCount, sizeof(iTotalCount));
      posIn += sizeof(iTotalCount);
   }

   // Read supported interfaces count
   if ( succeeded )
   {
      if ( sizeof(iSupportedCount) != fread(&iSupportedCount, 1, sizeof(iSupportedCount), fp) )
         succeeded = 0;
      if ( (iSupportedCount < 0) || (iSupportedCount >= MAX_RADIO_INTERFACES) )
         succeeded = 0;
      memcpy(bufferIn+posIn, &iSupportedCount, sizeof(iSupportedCount));
      posIn += sizeof(iSupportedCount);
   }

   // Read interfaces
   if ( succeeded )
   {
      for( int i=0; i<iTotalCount; i++ )
      {
         if ( sizeof(radio_hw_info_t) != fread(&(pRadioInfoArray[i]), 1, sizeof(radio_hw_info_t), fp) )
            succeeded = 0;
         memcpy(bufferIn+posIn, &(pRadioInfoArray[i]), sizeof(radio_hw_info_t));
         posIn += sizeof(radio_hw_info_t);
      }
   }
   if ( succeeded )
   {
      u32 crc = 0;
      if ( sizeof(u32) != fread(&crc, 1, sizeof(u32), fp ) )
         succeeded = 0;
      u32 crc2 = base_compute_crc32(bufferIn, posIn);
      if ( crc != crc2 )
      {
         succeeded = 0;
         log_softerror_and_alarm("HIC HW Info file is corrupted. Wrong CRC.");
      }
   }

   if ( NULL != fp )
      fclose(fp);

   if ( succeeded )
   {
      for( int i=0; i<iTotalCount; i++ )
      {
         pRadioInfoArray[i].openedForRead = 0;
         pRadioInfoArray[i].openedForWrite = 0;
         memset(&pRadioInfoArray[i].runtimeInterfaceInfoRx, 0, sizeof(type_runtime_radio_interface_info));
         memset(&pRadioInfoArray[i].runtimeInterfaceInfoTx, 0, sizeof(type_runtime_radio_interface_info));
         pRadioInfoArray[i].runtimeInterfaceInfoRx.ppcap = NULL;
         pRadioInfoArray[i].runtimeInterfaceInfoRx.selectable_fd = -1;
         pRadioInfoArray[i].runtimeInterfaceInfoRx.nPort = 0;
         reset_runtime_radio_rx_info(&pRadioInfoArray[i].runtimeInterfaceInfoRx.radioHwRxInfo);
         pRadioInfoArray[i].runtimeInterfaceInfoTx.ppcap = NULL;
         pRadioInfoArray[i].runtimeInterfaceInfoTx.selectable_fd = -1;
         reset_runtime_radio_rx_info(&pRadioInfoArray[i].runtimeInterfaceInfoTx.radioHwRxInfo);

         if ( (pRadioInfoArray[i].supportedBands & RADIO_HW_SUPPORTED_BAND_433) ||
              (pRadioInfoArray[i].supportedBands & RADIO_HW_SUPPORTED_BAND_868) ||
              (pRadioInfoArray[i].supportedBands & RADIO_HW_SUPPORTED_BAND_915) )
         if ( pRadioInfoArray[i].isHighCapacityInterface )
         {
            log_line("Hardware: Removed invalid high capacity flag from radio interface %d (%s, %s)",
               i+1, pRadioInfoArray[i].szName, pRadioInfoArray[i].szDriver);
            pRadioInfoArray[i].isHighCapacityInterface = 0;
         }
      }

      log_line("Loaded radio HW info file.");
      log_line("=================================================================");
      log_line("Total: %d Radios read, %d supported:", iTotalCount, iSupportedCount);
      hardware_log_radio_info(pRadioInfoArray, iTotalCount);

      if ( NULL != piOutputTotalCount )
         *piOutputTotalCount = iTotalCount;
      if ( NULL != piOutputSupportedCount )
         *piOutputSupportedCount = iSupportedCount;

      log_line("[HardwareRadio] Loaded hardware radio config from file (%s).", szFile);
      return 1;
   }

   log_softerror_and_alarm("[HardwareRadio] Failed to load hardware radio config from file (%s).", szFile);
   if ( NULL != piOutputTotalCount )
      *piOutputTotalCount = 0;
   if ( NULL != piOutputSupportedCount )
      *piOutputSupportedCount = 0;
   return 0;
}

int _hardware_detect_card_model(const char* szProductId)
{
   if ( (NULL == szProductId) || (0 == szProductId[0]) )
      return 0;

   if ( NULL != strstr( szProductId, "cf3:9271" ) )
      return CARD_MODEL_TPLINK722N;
   if ( NULL != strstr( szProductId, "40d:3801" ) )
      return CARD_MODEL_ATHEROS_GENERIC;

   if ( NULL != strstr( szProductId, "148f:3070" ) )
      return CARD_MODEL_ALFA_AWUS036NH;

   if ( NULL != strstr( szProductId, "b05:17d2" ) )
      return CARD_MODEL_ASUS_AC56;

   if ( NULL != strstr( szProductId, "846:9052" ) )
      return CARD_MODEL_NETGEAR_A6100;

   if ( NULL != strstr( szProductId, "bda:881a" ) )
      return CARD_MODEL_RTL8812AU_DUAL_ANTENNA;

   #if defined HW_PLATFORM_OPENIPC_CAMERA
   if ( NULL != strstr( szProductId, "bda:8812" ) )
      return CARD_MODEL_RTL8812AU_OIPC_USIGHT;
   #else
   if ( NULL != strstr( szProductId, "bda:8812" ) )
      return CARD_MODEL_ALFA_AWUS036ACH;
   #endif
     
   if ( NULL != strstr( szProductId, "bda:8811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;
   if ( NULL != strstr( szProductId, "bda:0811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;
   if ( NULL != strstr( szProductId, "bda:811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;

   if ( NULL != strstr( szProductId, "bda:010d" ) )
      return CARD_MODEL_RTL8812AU_OIPC_USIGHT2;

   if ( NULL != strstr( szProductId, "2604:12" ) )
      return CARD_MODEL_TENDA_U12;
   if ( NULL != strstr( szProductId, "2604:0012" ) )
      return CARD_MODEL_TENDA_U12;

   if ( NULL != strstr( szProductId, "2357:0101") )
      return CARD_MODEL_RTL8812AU_GENERIC;

   if ( NULL != strstr( szProductId, "2357:120" ) )
      return CARD_MODEL_ARCHER_T2UPLUS;
   if ( NULL != strstr( szProductId, "2357:0120" ) )
      return CARD_MODEL_ARCHER_T2UPLUS;

   if ( NULL != strstr( szProductId, "bda:8813" ) )
      return CARD_MODEL_RTL8814AU;

   if ( NULL != strstr( szProductId, "bda:a81a" ) )
      return CARD_MODEL_BLUE_8812EU;
   if ( NULL != strstr( szProductId, "0bda:a81a" ) )
      return CARD_MODEL_BLUE_8812EU;

   if ( NULL != strstr( szProductId, "bda:f72b" ) )
      return CARD_MODEL_RTL8733BU;
   if ( NULL != strstr( szProductId, "0bda:f72b" ) )
      return CARD_MODEL_RTL8733BU;
   if ( NULL != strstr( szProductId, "bda:b733" ) )
      return CARD_MODEL_RTL8733BU;
   if ( NULL != strstr( szProductId, "0bda:b733" ) )
      return CARD_MODEL_RTL8733BU;
   if ( NULL != strstr( szProductId, "bda:b731" ) )
      return CARD_MODEL_RTL8733BU;
   if ( NULL != strstr( szProductId, "0bda:b731" ) )
      return CARD_MODEL_RTL8733BU;
   return 0;
}

// Assigns the right USB port for each detected radio interface. If possible.

void _hardware_assign_usb_from_physical_ports()
{
   if ( s_iHwRadiosCount <= 0 )
      return;

   log_line("[HardwareRadio] Finding USB ports for %d radio interfaces...", s_iHwRadiosCount);

   // Set some default values first
   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      sRadioInfo[i].szUSBPort[0] = 'Y';
      sRadioInfo[i].szUSBPort[1] = '1' + i;
      sRadioInfo[i].szUSBPort[2] = 0;
   }

   DIR *d;
   struct dirent *dir;
   char szComm[256];
   char szOutput[1024];

   d = opendir("/sys/bus/usb/devices/");
   if ( NULL == d )
   {
      log_line("[HardwareRadio] Can't find USB ports for radio interfaces (can't open /sys/bus/usb/devices).");
      return;
   }

   while ((dir = readdir(d)) != NULL)
   {
      int iLen = strlen(dir->d_name);
      if ( iLen < 3 )
         continue;

      log_line("[HardwareRadio] Quering USB device path: [%s]...", dir->d_name);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep DRIVER", dir->d_name);
      szOutput[0] = 0;
      hw_execute_bash_command_silent(szComm, szOutput);
      if ( 0 == szOutput[0] )
      {
         log_line("[HardwareRadio] No info for USB device: [%s]. Skipping it.", dir->d_name);
         continue;
      }
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, sRadioInfo[i].szName);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 == szOutput[0] )
            continue;

         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep INTERFACE", dir->d_name, sRadioInfo[i].szName);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 == szOutput[0] )
            continue;

         int iPos = 0;
         iLen = strlen(szOutput);
         while ( iPos < iLen )
         {
            if ( szOutput[iPos] == '=' )
            {
               iPos++;
               break;
            }
            iPos++;
         }

         if ( iPos >= iLen )
            continue;

         int bMathcingInterfaceName = 1;
         for( int k=0; k<strlen(sRadioInfo[i].szName); k++ )
            if ( szOutput[iPos+k] != sRadioInfo[i].szName[k] )
               bMathcingInterfaceName = 0;

         if ( ! bMathcingInterfaceName )
            continue;

         // Found the USB port for radio interface i

         log_line("[HardwareRadio] Found USB port tree position for radio interface %d: [%s]", i+1, dir->d_name);

         // Find the product id / vendor id

         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 != szOutput[0] )
         {
            iLen = strlen(szOutput);
            int iPosOut = 0;
            while ( iPosOut < iLen )
            {
               if ( szOutput[iPosOut] == '=' )
               {
                  iPosOut++;
                  break;
               }
               szOutput[iPosOut] = tolower(szOutput[iPosOut]);
               iPosOut++;
            }
            if ( iPosOut < iLen )
            {
               for( int kk=iPosOut; kk<iLen; kk++ )
                  szOutput[kk] = tolower(szOutput[kk]);

               strncpy( sRadioInfo[i].szProductId, &(szOutput[iPosOut]), sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0]) );
               sRadioInfo[i].szProductId[(sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0]))-1] = 0;
               for( int kk=0; kk<strlen(sRadioInfo[i].szProductId); kk++ )
               {
                  if ( sRadioInfo[i].szProductId[kk] == '/' )
                     sRadioInfo[i].szProductId[kk] = ':';
               }
               sRadioInfo[i].iCardModel = _hardware_detect_card_model(sRadioInfo[i].szProductId);
               log_line("[HardwareRadio] Found product/vendor id for radio interface %d: [%s], card model: %d: [%s]", i+1, sRadioInfo[i].szProductId, sRadioInfo[i].iCardModel, str_get_radio_card_model_string(sRadioInfo[i].iCardModel) );
            }
         }

         // Done finding the product id / vendor id

         // Parse the USB port path info

         char szPort[128];
         strncpy(szPort, dir->d_name, 127);
         szPort[127] = 0;

         // Find first : in string

         int iPosEnd = 0;
         iLen = strlen(szPort);
         while ( iPosEnd < iLen )
         {
            if ( szPort[iPosEnd] == ':' )
            {
               szPort[iPosEnd] = 0;
               break;
            }
            iPosEnd++;
         }

         // Found the first : or the end of string.
         // USB ports are the digits before x-y.A.B.C...N:
         // Ignore the x-y part

         log_line("[HardwareRadio] Found USB port part for radio interface %d: [%s]", i+1, szPort);
         
         int iPosMinus = iPosEnd-1;
         while ( iPosMinus > 0 )
         {
            if ( szPort[iPosMinus] == '-' )
               break;
            iPosMinus--;
         }

         if ( szPort[iPosMinus+1] == 0 )
            continue;

         // Is it a short version? X-Y, no : after it;
         if ( szPort[iPosMinus+2] == 0 || ((szPort[iPosMinus+2] == ' ') && (szPort[iPosMinus+3] == 0) ))
         {
            iPosMinus++;
            int nb = atoi(&(szPort[iPosMinus]))-1;
            if ( nb < 0 )
               nb = 0;
            if ( nb > 9 )
               nb = 9;
            sRadioInfo[i].szUSBPort[0] = 'A' + nb;
            sRadioInfo[i].szUSBPort[1] = 0;
            log_line("[HardwareRadio] Assigned short USB port to radio interface %d: [%s]", i+1, sRadioInfo[i].szUSBPort);
            continue;
         }

         // Find first . after -
         while ( iPosMinus < iPosEnd )
         {
            if ( szPort[iPosMinus] == '.' )
            {
               iPosMinus++;
               break;
            }
            iPosMinus++;
         }

         if ( (iPosMinus >= iPosEnd) || (iPosMinus >= iLen) || (iPosMinus < 0) )
            continue;

         log_line("[HardwareRadio] Found USB port part2 for radio interface %d: [%s]", i+1, &(szPort[iPosMinus]));

         if ( (szPort[iPosMinus] < '0') || (szPort[iPosMinus] > '9') )
         {
            log_line("[HardwareRadio] Invalid port string [%s]. Skipping.", &(szPort[iPosMinus]));
            continue;
         }
         char szNb[32];
         szNb[0] = szPort[iPosMinus];
         szNb[1] = 0;
         iPosMinus += 2;

         int nb = atoi(szNb)-1;
         if ( nb < 0 )
            nb = 0;
         if ( nb > 9 )
            nb = 9;
         sRadioInfo[i].szUSBPort[0] = 'A' + nb;
         sRadioInfo[i].szUSBPort[1] = 0;
         if ( iPosMinus < iPosEnd )
            strncpy(&(sRadioInfo[i].szUSBPort[1]), &(szPort[iPosMinus]), MAX_RADIO_PORT_NAME_LENGTH-1);
         sRadioInfo[i].szUSBPort[MAX_RADIO_PORT_NAME_LENGTH-1] = 0;
         log_line("[HardwareRadio] Assigned USB port to radio interface %d: [%s]", i+1, sRadioInfo[i].szUSBPort);
      }
   }
   closedir(d);

   // Assign some USB port to the ones that did not have a match

   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      if ( sRadioInfo[i].szUSBPort[0] != 0 )
         continue;
      sRadioInfo[i].szUSBPort[0] = 'X';
      sRadioInfo[i].szUSBPort[1] = '1' + i;
      sRadioInfo[i].szUSBPort[2] = 0;
   }

   // Sort the radio interfaces on USB port number
   
   for( int i=0; i<s_iHwRadiosCount-1; i++ )
   for( int j=i+1; j<s_iHwRadiosCount; j++ )
      if ( strncmp( sRadioInfo[i].szUSBPort, sRadioInfo[j].szUSBPort, MAX_RADIO_PORT_NAME_LENGTH-1 ) > 0 )
      {
         radio_hw_info_t tmp;
         memcpy(&tmp, &(sRadioInfo[i]), sizeof(radio_hw_info_t));
         memcpy(&(sRadioInfo[i]), &(sRadioInfo[j]), sizeof(radio_hw_info_t));
         memcpy(&(sRadioInfo[j]), &tmp, sizeof(radio_hw_info_t));
      }

   log_line("[HardwareRadio] Done assigning USB ports to radio interfaces:");
   for( int i=0; i<s_iHwRadiosCount; i++ )
      log_line("[HardwareRadio] Radio interface %d USB port: [%s]", i+1, sRadioInfo[i].szUSBPort);
}

int hardware_radio_get_driver_id_card_model(int iCardModel)
{
   if ( (iCardModel == CARD_MODEL_TPLINK722N) || (iCardModel == CARD_MODEL_ATHEROS_GENERIC) )
      return RADIO_HW_DRIVER_ATHEROS;
   else if ( iCardModel == CARD_MODEL_BLUE_8812EU )
      return RADIO_HW_DRIVER_REALTEK_8812EU;
   else if ( iCardModel == CARD_MODEL_RTL8733BU )
      return RADIO_HW_DRIVER_REALTEK_8733BU;
   else if ( iCardModel == CARD_MODEL_RTL8814AU )
      return RADIO_HW_DRIVER_REALTEK_RTL8814AU;
   else if ( (iCardModel > 0) && (iCardModel <50) )
      return RADIO_HW_DRIVER_REALTEK_RTL88XXAU;

   return 0;
}

int hardware_radio_get_driver_id_for_product_id(const char* szProdId)
{
   if ( (NULL == szProdId) || (0 == szProdId[0]) )
      return 0;

   int iCardModel = _hardware_detect_card_model(szProdId);
   return hardware_radio_get_driver_id_card_model(iCardModel);
}

int _hardware_find_usb_radio_interfaces_info()
{
   s_iEnumeratedUSBRadioInterfaces = 1;
   s_iFoundUSBRadioInterfaces = 0;
   int iCountKnownRadioCards = 0;
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Finding USB radio interfaces and devices for OpenIPC platform...");
   #else
   log_line("[HardwareRadio] Finding USB radio interfaces and devices for linux/raspberry platform...");
   #endif

   char szBuff[4096];
   szBuff[0] = 0;
   memset(szBuff, 0, sizeof(szBuff)/sizeof(szBuff[0]));
   hw_execute_bash_command_raw_timeout("lsusb", szBuff, 4000);

   log_line("[HardwareRadio] Output of lsusb: <<<%s>>>", szBuff);

   int iLen = strlen(szBuff);
   if ( iLen >= (int)(sizeof(szBuff)/sizeof(szBuff[0])) )
   {
      iLen = (int)(sizeof(szBuff)/sizeof(szBuff[0])) - 1;
      szBuff[iLen] = 0;
   }
   int iStLinePos = 0;
   while (iStLinePos < iLen)
   {
      // Get one line
      int iEndLinePos = iStLinePos;
      while ( (szBuff[iEndLinePos] != 10) && (szBuff[iEndLinePos] != 13) && (szBuff[iEndLinePos] != 0) && (iEndLinePos < iLen) )
         iEndLinePos++;
      szBuff[iEndLinePos] = 0;

      if ( strlen(&szBuff[iStLinePos]) < 4 )
      {
         // Go to next line
         iEndLinePos++;
         iStLinePos = iEndLinePos;
         continue;       
      }

      // Parse the line
      log_line("[HardwareRadio] Parsing USB line: [%s]", &szBuff[iStLinePos]);
      
      char* pBus = strstr(&szBuff[iStLinePos], "Bus");
      char* pDevice = strstr(&szBuff[iStLinePos], "Device");
      char* pUSBId = strstr(&szBuff[iStLinePos], "ID");

      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb = 0;
      if ( NULL != pBus )
      if ( isdigit(*(pBus + 4)) )
         s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb = atoi(pBus + 4);

      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb = 0;
      if ( NULL != pDevice )
      if ( isdigit(*(pDevice + 7)) )
         s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb = atoi(pDevice+7);

      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[0] = 0;
      if ( NULL != pUSBId )
      {
         char szUSBId[128];
         strncpy(szUSBId, pUSBId+3, sizeof(szUSBId)/sizeof(szUSBId[0])-1);
         for( int i=0; i<strlen(szUSBId); i++ )
         {
            szUSBId[i] = tolower(szUSBId[i]);
            if ( szUSBId[i] == ' ' )
            {
               szUSBId[i] = 0;
               break;
            }
         }
         strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId, szUSBId, sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1 );
         s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1] = 0;
      }

      int iCardModel = _hardware_detect_card_model(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId);
      if ( iCardModel <= 0 )
      {
         log_line("[HardwareRadio] Could not find a known USB product id in the product id string (%s)", s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId);
         log_line("[HardwareRadio] Search it on entire line");
         iCardModel = _hardware_detect_card_model(&szBuff[iStLinePos]);
      }
      int iCardDriver = hardware_radio_get_driver_id_card_model(iCardModel);

      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iPortNb = -1;
      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDriver = iCardDriver;
      strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName, str_get_radio_card_model_string(iCardModel), sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) );
      s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) - 1] = 0;

      log_line("[HardwareRadio] Parsing USB device: bus %d, device %d: prod id: %s, name: %s",
          s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb,
          s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb,
          s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId,
          s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName);

      if ( s_iFoundUSBRadioInterfaces < MAX_USB_DEVICES_INFO-1 )
         s_iFoundUSBRadioInterfaces++;
      else
      {
         log_softerror_and_alarm("[HardwareRadio] Not enough space in USB devices interfaces info to store one more entry. It has %d entries already.", s_iFoundUSBRadioInterfaces);
         iStLinePos = iLen;
         break;
      }

      if ( iCardModel > 0 )
      {
         iCountKnownRadioCards++;
         log_line("[HardwareRadio] USB device %s is a known radio interface, type: %s",
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId, str_get_radio_card_model_string(iCardModel));
      }

      // Go to next line
      iEndLinePos++;
      iStLinePos = iEndLinePos;
   }

   log_line("[HardwareRadio] Done finding USB devices. Found %d USB devices of which %d are known radio cards.", s_iFoundUSBRadioInterfaces, iCountKnownRadioCards);
   return iCountKnownRadioCards;
}

int _hardware_enumerate_wifi_radios()
{
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Enumerating wifi radios for OpenIPC platform...");
   #else
   log_line("[HardwareRadio] Enumerating wifi radios for linux/radxa/raspberry platform...");
   #endif

   char szDriver[128];
   char szComm[256];
   char szBuff[1024];

   _hardware_find_usb_radio_interfaces_info();

   log_line("[HardwareRadio] Finding wireless radio cards...");
   FILE* fp = popen("ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot", "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to enumerate 2.4/5.8 radios.");
      return 0;
   }
   
   int iStartIndex = s_iHwRadiosCount;

   while (fgets(szBuff, 255, fp) != NULL)
   {
      sscanf(szBuff, "%s", sRadioInfo[s_iHwRadiosCount].szName);
      
      if ( 0 == sRadioInfo[s_iHwRadiosCount].szName[0] )
         continue;
      // if ( 0 != strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlx") ) continue;
        
      log_line("[HardwareRadio] Parsing found wireless radio: [%s]", sRadioInfo[s_iHwRadiosCount].szName);
      if ( 0 == strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlan" ) )
      if ( 0 == strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlx" ) )
      {
         log_line("[HardwareRadio] Skipping wireless radio [%s]", sRadioInfo[s_iHwRadiosCount].szName);
         continue;
      }
      
      sRadioInfo[s_iHwRadiosCount].iCardModel = 0;
      sRadioInfo[s_iHwRadiosCount].isSupported = 0;
      sRadioInfo[s_iHwRadiosCount].isSerialRadio = 0;
      sRadioInfo[s_iHwRadiosCount].isConfigurable = 1;
      sRadioInfo[s_iHwRadiosCount].isEnabled = 1;
      sRadioInfo[s_iHwRadiosCount].isTxCapable = 1;
      sRadioInfo[s_iHwRadiosCount].isHighCapacityInterface = 1;
      sRadioInfo[s_iHwRadiosCount].iCurrentDataRateBPS = 0;
      sRadioInfo[s_iHwRadiosCount].uCurrentFrequencyKhz = 0;
      sRadioInfo[s_iHwRadiosCount].lastFrequencySetFailed = 0;
      sRadioInfo[s_iHwRadiosCount].uFailedFrequencyKhz = 0;
      sRadioInfo[s_iHwRadiosCount].phy_index = 0;
      sRadioInfo[s_iHwRadiosCount].uExtraFlags = 0;
      sRadioInfo[s_iHwRadiosCount].supportedBands = 0;
      sRadioInfo[s_iHwRadiosCount].szDescription[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szProductId[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szDriver[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szMAC[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szUSBPort[0] = 'X';
      sRadioInfo[s_iHwRadiosCount].szUSBPort[1] = '0';
      sRadioInfo[s_iHwRadiosCount].iRadioType = RADIO_TYPE_OTHER;
      sRadioInfo[s_iHwRadiosCount].iRadioDriver = 0;
      memset(&(sRadioInfo[s_iHwRadiosCount].uHardwareParamsList[0]), 0, sizeof(u32)*MAX_RADIO_HW_PARAMS);
      sRadioInfo[s_iHwRadiosCount].openedForRead = 0;
      sRadioInfo[s_iHwRadiosCount].openedForWrite = 0;

      memset(&sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoRx, 0, sizeof(type_runtime_radio_interface_info));
      memset(&sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoTx, 0, sizeof(type_runtime_radio_interface_info));
      reset_runtime_radio_rx_info(&sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoRx.radioHwRxInfo);
      sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoRx.ppcap = NULL;
      sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoRx.selectable_fd = -1;
      sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoRx.nPort = 0;
      reset_runtime_radio_rx_info(&sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoTx.radioHwRxInfo);
      sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoTx.ppcap = NULL;
      sRadioInfo[s_iHwRadiosCount].runtimeInterfaceInfoTx.selectable_fd = -1;

      s_iHwRadiosCount++;
      if ( s_iHwRadiosCount >= MAX_RADIO_INTERFACES )
         break;
   }
   pclose(fp);

   log_line("[HardwareRadio] Found a total of %d wifi cards. Get info about them...", s_iHwRadiosCount);
   s_iHwRadiosSupportedCount = 0;

   for( int i=iStartIndex; i<s_iHwRadiosCount; i++ )
   {
      szDriver[0] = 0;

      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      sprintf(szComm, "ls -Al /sys/class/net/%s/device/ | grep driver", sRadioInfo[i].szName);
      hw_execute_bash_command_raw(szComm, szDriver);
      #else
      sprintf(szComm, "cat /sys/class/net/%s/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'", sRadioInfo[i].szName);
      hw_execute_bash_command(szComm, szDriver);
      #endif
      removeTrailingNewLines(szDriver);

      if ( NULL != strstr(szDriver, "such device") )
      {
         log_softerror_and_alarm("[HardwareRadio] This device (%s) is no longer valid. Removing it.", sRadioInfo[i].szName);
         for( int k=i; k<s_iHwRadiosCount-1; k++ )
         {
            memcpy(&(sRadioInfo[k]), &(sRadioInfo[k+1]), sizeof(radio_hw_info_t));
         }
         memset(&(sRadioInfo[s_iHwRadiosCount-1]), 0, sizeof(radio_hw_info_t));
         s_iHwRadiosCount--;
         i--;
         continue;
      }
      log_line("[HardwareRadio] Driver for %s: (%s)", sRadioInfo[i].szName, szDriver);
      log_line("[HardwareRadio] Driver for %s (last part): (%s)", sRadioInfo[i].szName, &szDriver[strlen(szDriver)/2]);
      char* pszDriver = szDriver;
      if ( strlen(szDriver) > 0 )
      {
         for( int k=strlen(szDriver)-1; k>0; k-- )
         {
            if ( szDriver[k] == '/' )
            {
               pszDriver = &szDriver[k];
               break;
            }
         }
      }
      if ( *pszDriver == '/' )
         pszDriver++;
      log_line("[HardwareRadio] Minimised driver string: (%s)", pszDriver);

      sRadioInfo[i].isSupported = 0;
      if ( (NULL != strstr(pszDriver,"rtl88xxau")) ||
           (NULL != strstr(pszDriver,"8812au")) ||
           (NULL != strstr(pszDriver,"rtl8812au")) ||
           (NULL != strstr(pszDriver,"rtl88XXau")) ||
           (NULL != strstr(pszDriver,"8812eu")) ||
           (NULL != strstr(pszDriver,"88x2eu")) ||
           (NULL != strstr(pszDriver,"8733bu")) )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         if ( (NULL != strstr(pszDriver,"rtl88xxau")) ||
              (NULL != strstr(pszDriver,"rtl88XXau")) )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL88XXAU;
         }
         if ( NULL != strstr(pszDriver,"8812au") )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_8812AU;
         }
         if ( NULL != strstr(pszDriver,"rtl8812au") )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL8812AU;
         }
         if ( (NULL != strstr(pszDriver,"8812eu")) || (NULL != strstr(pszDriver,"88x2eu")) )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_8812EU;
         }
         if ( NULL != strstr(pszDriver,"8733bu") )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_8733BU;
         }
      }
      else if ( NULL != strstr(pszDriver,"8814au") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL8814AU;
      }
      // Experimental
      if ( NULL != strstr(pszDriver,"rtl88x2bu") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         sRadioInfo[i].isTxCapable = 0;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL88X2BU;
      }

      if ( NULL != strstr(pszDriver,"rt2800usb") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Ralink");
         sRadioInfo[i].iRadioType = RADIO_TYPE_RALINK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_RALINK;
      }
      if ( NULL != strstr(pszDriver, "mt7601u") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Mediatek");
         sRadioInfo[i].iRadioType = RADIO_TYPE_MEDIATEK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_MEDIATEK;
      }
      if ( NULL != strstr(pszDriver, "ath9k_htc") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Atheros");
         sRadioInfo[i].iRadioType = RADIO_TYPE_ATHEROS;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_ATHEROS;
      }

      int iCopyPos = strlen(szDriver);
      while ( iCopyPos >= 0 )
      {
         if ( (szDriver[iCopyPos] == 10) || (szDriver[iCopyPos] == 13) )
            szDriver[iCopyPos] = 0;
         if ( szDriver[iCopyPos] == '/' )
            break;
         iCopyPos--;
      }
      strcpy(sRadioInfo[i].szDriver, &(szDriver[iCopyPos+1]));

      if ( ! sRadioInfo[i].isSupported )
         log_softerror_and_alarm("Found unsupported radio (%s), driver %s, skipping.", sRadioInfo[i].szName, szDriver);
      else
         log_line("[HardwareRadio] Found supported radio type: %s, driver: %s, driver string short: %s, driver string: [%s]", str_get_radio_type_description(sRadioInfo[i].iRadioType), str_get_radio_driver_description(sRadioInfo[i].iRadioDriver), sRadioInfo[i].szDriver, pszDriver);
      sRadioInfo[i].iCardModel = 0;

      for( int kk=0; kk<(int)(sizeof(sRadioInfo[i].szUSBPort)/sizeof(sRadioInfo[i].szUSBPort[0])); kk++ )
         sRadioInfo[i].szUSBPort[kk] = 0;

      for( int kk=0; kk<(int)(sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0])); kk++ )
         sRadioInfo[i].szProductId[kk] = 0;

      // Find the MAC address
      sprintf(szComm, "iw dev %s info | grep addr", sRadioInfo[i].szName );
      if ( 1 != hw_execute_bash_command_raw(szComm, szBuff) )
      {
         log_softerror_and_alarm("Failed to find MAC address for %s", sRadioInfo[i].szName);
      }
      else if ( 1 != sscanf(szBuff, "%*s %s", szComm) )
      {
         log_softerror_and_alarm("Failed to find MAC address for %s", sRadioInfo[i].szName);
      }
      else
      {
         log_line("Found MAC address %s for %s", szComm, sRadioInfo[i].szName);
         szComm[MAX_MAC_LENGTH-1] = 0;
         int iSt = 0;
         int iEnd = 0;
         while ( iEnd < strlen(szComm) )
         {
            if ( szComm[iEnd] == ':' )
               iEnd++;
            else
            {
               szComm[iSt] = toupper(szComm[iEnd]);
               iSt++;
               iEnd++;
            }
         }
         szComm[iSt] = 0;
         strncpy(sRadioInfo[i].szMAC, szComm, MAX_MAC_LENGTH-1);
         sRadioInfo[i].szMAC[MAX_MAC_LENGTH-1] = 0;
      }

      // Find physical interface number, in form phy#0

      sprintf(szComm, "iw dev | grep -B 1 %s", sRadioInfo[i].szName );
      if ( 1 != hw_execute_bash_command_raw(szComm, szBuff) )
      {
         sRadioInfo[i].phy_index = i;
         log_softerror_and_alarm("Failed to find physical interface index for %s", sRadioInfo[i].szName);
      }
      else if ( 1 != sscanf(szBuff, "%s", szComm) )
      {
         sRadioInfo[i].phy_index = i;
         log_softerror_and_alarm("Failed to find physical interface index for %s", sRadioInfo[i].szName);
      }
      else
      {
         int iPhyStrLen = strlen(szComm);
         log_line("phy string: [%s], length: %d", szComm, iPhyStrLen);
         sRadioInfo[i].phy_index = szComm[iPhyStrLen-1] - '0';
         if ( (iPhyStrLen > 2) && isdigit(szComm[iPhyStrLen-2]) )
         {
            sRadioInfo[i].phy_index = 10 * (szComm[iPhyStrLen-2] - '0') + sRadioInfo[i].phy_index;
         }
      }

      // Check supported bands

      sRadioInfo[i].supportedBands = 0;
      sprintf(szComm, "iw phy%d info | grep 2377", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_23;

      sprintf(szComm, "iw phy%d info | grep 2427", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_24;

      sprintf(szComm, "iw phy%d info | grep 2512", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_25;

      sprintf(szComm, "iw phy%d info | grep 5745", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_58;

      if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
         sRadioInfo[i].supportedBands &= ~RADIO_HW_SUPPORTED_BAND_24;

      char szBands[128];
      str_get_supported_bands_string(sRadioInfo[i].supportedBands, szBands);
      log_line("[HardwareRadio] Interface %s supported bands: [%s]", sRadioInfo[i].szName, szBands);
   }

   _hardware_assign_usb_from_physical_ports();
   return 1;
}

void hardware_reset_radio_enumerated_flag()
{
   s_HardwareRadiosEnumeratedOnce = 0;
}

int hardware_enumerate_radio_interfaces()
{
   return hardware_enumerate_radio_interfaces_step(-1);
}

int hardware_enumerate_radio_interfaces_step(int iStep)
{
   if ( s_HardwareRadiosEnumeratedOnce && ((iStep == -1) || (iStep == 0)) )
      return 1;

   log_line("=================================================================");
   log_line("[HardwareRadio] Enumerating radios (step %d)...", iStep);

   if( (iStep == -1) || (iStep == 0) )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
      if ( access(szFile, R_OK) != -1 )
      {
         log_line("[HardwareRadio] HW Enumerate: has radio HW info file. Using it.");
         if ( hardware_load_radio_info() )
         {
            s_HardwareRadiosEnumeratedOnce = 1;
            hardware_radio_sik_load_configuration();
            return 1;
         }
      }
      else
         log_line("[HardwareRadio] HW Enumerate: no existing radio HW info file. Enumerating HW radios from scratch.");

      s_iHwRadiosCount = 0;
   
      _hardware_enumerate_wifi_radios();
      
      if ( 0 == s_iHwRadiosCount )
         log_error_and_alarm("[HardwareRadio] No 2.4/5.8 radio modules found!");
      else
         log_line("[HardwareRadio] Found %d 2.4/5.8 radio modules.", s_iHwRadiosCount);

      s_HardwareRadiosEnumeratedOnce = 1;
   }

   if( iStep == -1 || iStep == 1 )
      hardware_radio_sik_detect_interfaces();

   log_line("[HardwareRadio] Total: %d radios found", s_iHwRadiosCount);
   log_line("=================================================================");
   
   if( iStep == -1 || iStep == 1 )
   {
      hardware_log_radio_info(&sRadioInfo[0], s_iHwRadiosCount);
      hardware_save_radio_info();
   }
   s_HardwareRadiosEnumeratedOnce = 1;

   if ( 0 == s_iHwRadiosCount )
      return 0;
   return 1;
}

int hardware_radio_get_class_net_adapters_count()
{
   char szOutput[1024];
   szOutput[0] = 0;
   hw_execute_bash_command_raw_silent("ls /sys/class/net/", szOutput);
   removeNewLines(szOutput);
      
   int iCount =0;
   char* szToken = strtok(szOutput, "*");
   while ( NULL != szToken )
   {
      if ( NULL != strstr(szToken, "wlan") )
         iCount++;
      szToken = strtok(NULL, "*");
   }
   return iCount;
}

int hardware_radio_has_rtl8814au_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL8814AU )
            iCount++;
      }
   }

   if ( iCount > 0 )
      return iCount;

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_RTL8814AU )
          iCount++;
   }
   return iCount;
}

int hardware_load_driver_rtl8812au()
{
   char szPlatform[128];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[Hardware] Loading driver RTL8812AU for platform: %s ...", szPlatform);

   #if defined HW_PLATFORM_OPENIPC_CAMERA
   hw_execute_bash_command("modprobe cfg80211", NULL);

   char szFile[MAX_FILE_PATH_SIZE];
   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "/lib/modules/%s/extra/88XXau.ko", szPlatform);
   if ( access(szFile, R_OK) != -1 )
   {
      hw_execute_bash_command_raw("insmod /lib/modules/$(uname -r)/extra/88XXau.ko rtw_tx_pwr_idx_override=1 2>&1", NULL);
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("modprobe 88XXau rtw_tx_pwr_idx_override=1 2>&1", NULL);
   }
   else
   {
      hw_execute_bash_command_raw("insmod /lib/modules/$(uname -r)/extra/8812au.ko rtw_tx_pwr_idx_override=1 2>&1", NULL);
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("modprobe 8812au rtw_tx_pwr_idx_override=1 2>&1", NULL);
   }
   return 1;
   #endif

   char szOutput[256];
   
   hw_execute_bash_command("sudo modprobe cfg80211", NULL);
   hw_execute_bash_command("sudo modprobe 88XXau rtw_tx_pwr_idx_override=1 2>&1", szOutput);
   log_line("Modprobe result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to load driver 8812AU on platform (%s), error: (%s)", szPlatform, szOutput);
      return hardware_install_driver_rtl8812au(0);
   }

   return 1;
}

int hardware_load_driver_rtl8812eu()
{
   char szPlatform[128];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[Hardware] Loading driver RTL8812EU for platform: %s ...", szPlatform);

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("modprobe cfg80211", NULL);
   hw_execute_bash_command_raw("insmod /lib/modules/$(uname -r)/extra/8812eu.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", NULL);
   hardware_sleep_ms(50);
   hw_execute_bash_command_raw("modprobe 8812eu rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", NULL);
   return 1;
   #endif

   char szOutput[256];

   hw_execute_bash_command("sudo modprobe cfg80211", NULL);
   hw_execute_bash_command("sudo modprobe 8812eu rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", szOutput);
   log_line("Modprobe result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to load driver 8812EU on platform (%s), error: (%s)", szPlatform, szOutput);
      return hardware_install_driver_rtl8812eu(0);
   }

   return 1;
}

int hardware_load_driver_rtl8733bu()
{
   char szPlatform[128];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[Hardware] Loading driver RTL8733BU for platform: %s ...", szPlatform);

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("modprobe cfg80211", NULL);
   hw_execute_bash_command_raw("insmod /lib/modules/$(uname -r)/extra/8733bu.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", NULL);
   hardware_sleep_ms(50);
   hw_execute_bash_command_raw("modprobe 8733bu rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", NULL);
   return 1;
   #endif

   hw_execute_bash_command("sudo modprobe cfg80211", NULL);
   return 1;
}

int hardware_load_driver_rtl8814au()
{
   char szPlatform[128];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[Hardware] Loading driver RTL8814AU for platform: %s ...", szPlatform);

   #if defined HW_PLATFORM_OPENIPC_CAMERA
   hw_execute_bash_command("modprobe cfg80211", NULL);
   // Note: Using 88XXau.ko as a placeholder, actual 8814au driver name might differ
   char szFile[MAX_FILE_PATH_SIZE];
   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "/lib/modules/%s/extra/8814au.ko", szPlatform);
   if ( access(szFile, R_OK) != -1 )
   {
      hw_execute_bash_command_raw("insmod /lib/modules/$(uname -r)/extra/8814au.ko rtw_tx_pwr_idx_override=1 2>&1", NULL);
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("modprobe 8814au rtw_tx_pwr_idx_override=1 2>&1", NULL);
   }
   else
   {
      log_softerror_and_alarm("[HardwareRadio] RTL8814AU driver file not found at %s for OpenIPC. Attempting generic modprobe.", szFile);
      hw_execute_bash_command_raw("modprobe 8814au rtw_tx_pwr_idx_override=1 2>&1", NULL);
   }
   // For OpenIPC, we assume driver is pre-installed or handled by firmware.
   // If modprobe/insmod fails, installation is likely a manual process beyond this scope.
   return 1; 
   #endif

   char szOutput[256];
   
   hw_execute_bash_command("sudo modprobe cfg80211", NULL);
   // Attempt to load 8814au, then rtl8814au as fallback names
   hw_execute_bash_command("sudo modprobe 8814au rtw_tx_pwr_idx_override=1 2>&1", szOutput);
   if ( strlen(szOutput) > 10 ) // Heuristic: successful modprobe has minimal output
   {
      log_line("[HardwareRadio] Failed to load driver 8814au, trying rtl8814au. Error: [%s]", szOutput);
      szOutput[0] = 0; // Clear previous output
      hw_execute_bash_command("sudo modprobe rtl8814au rtw_tx_pwr_idx_override=1 2>&1", szOutput);
   }

   log_line("Modprobe result for RTL8814AU: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to load driver RTL8814AU (8814au/rtl8814au) on platform (%s), error: (%s)", szPlatform, szOutput);
      return hardware_install_driver_rtl8814au(0);
   }

   return 1;
}

// Called only once, from ruby_start process
int hardware_radio_load_radio_modules(int iEchoToConsole)
{
   int iCountKnownRadios = _hardware_find_usb_radio_interfaces_info();

   if ( 0 == s_iFoundUSBRadioInterfaces )
   {
      log_softerror_and_alarm("[HardwareRadio] No USB radio devices found!");
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: No USB radio cards found!\n\n");
         fflush(stdout);
      }
      return 0;
   }

   if ( 0 == iCountKnownRadios )
   {
      log_softerror_and_alarm("[HardwareRadio] No known USB radio devices found!");
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: No known USB radio cards found!\n\n");
         fflush(stdout);
      }
      return 0;    
   }

   #if defined(HW_PLATFORM_RASPBERRY)
   log_line("[HardwareRadio] Adding radio modules on Raspberry for detected radio cards...");
   #endif
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   log_line("[HardwareRadio] Adding radio modules on OpenIPC for detected radio cards...");
   #endif
   #if defined(HW_PLATFORM_RADXA)
   log_line("[HardwareRadio] Adding radio modules on Radxa for detected radio cards...");
   #endif

   if ( iEchoToConsole )
   {
      printf("Ruby: Adding radio modules for detected radio cards...\n");
      fflush(stdout);
   }

   char szOutput[1024];
   log_line("[HardwareRadio] Loading radio modules...");

   int iRTL8812AULoaded = 0;
   int iRTL8812EULoaded = 0;
   int iRTL8733BULoaded = 0;
   int iRTL8814AULoaded = 0;
   int iAtherosLoaded = 0;
   int iCountLoaded = 0;

   if ( hardware_radio_has_rtl8812au_cards() )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: Adding radio modules for RTL8812AU radio cards...\n");
         fflush(stdout);
      }
      log_line("[HardwareRadio] Found RTL8812AU cards. Loading module...");
      if ( 1 != hardware_load_driver_rtl8812au() )
      {
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver RTL8812AU");
         if ( iEchoToConsole )
         {
            printf("Ruby: ERROR on loading driver RTL8812AU\n");
            fflush(stdout);
         }
      }
      else
      {
         iRTL8812AULoaded = 1;
         iCountLoaded++;
      }
   }

   if ( hardware_radio_has_rtl8812eu_cards() )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: Adding radio modules for RTL8812EU radio cards...\n");
         fflush(stdout);
      }
      log_line("[HardwareRadio] Found RTL8812EU cards. Loading module...");

      if ( 1 != hardware_load_driver_rtl8812eu() )    
      {
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver RTL8812EU");
         if ( iEchoToConsole )
         {
            printf("Ruby: ERROR on loading driver RTL8812EU\n");
            fflush(stdout);
         }
      }
      else
      {
         iRTL8812EULoaded = 1;
         iCountLoaded++;    
      }
   }

   if ( hardware_radio_has_rtl8733bu_cards() )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: Adding radio modules for RTL8733BU radio cards...\n");
         fflush(stdout);
      }
      log_line("[HardwareRadio] Found RTL8733BU cards. Loading module...");

      if ( 1 != hardware_load_driver_rtl8733bu() )    
      {
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver RTL8733BU");
         if ( iEchoToConsole )
         {
            printf("Ruby: ERROR on loading driver RTL8733BU\n");
            fflush(stdout);
         }
      }
      else
      {
         iRTL8733BULoaded = 1;
         iCountLoaded++;    
      }
   }

   if ( hardware_radio_has_atheros_cards() )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: Adding radio modules for Atheros radio cards...\n");
         fflush(stdout);
      }
      hw_execute_bash_command("modprobe ath9k_hw txpower=10", szOutput);
      hw_execute_bash_command("modprobe ath9k_htc", szOutput);
      iAtherosLoaded = 1;
      iCountLoaded++;
   }

   if ( hardware_radio_has_rtl8814au_cards() ) // This function will be created in a later step
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: Adding radio modules for RTL8814AU radio cards...\n");
         fflush(stdout);
      }
      log_line("[HardwareRadio] Found RTL8814AU cards. Loading module...");
      if ( 1 != hardware_load_driver_rtl8814au() )
      {
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver RTL8814AU");
         if ( iEchoToConsole )
         {
            printf("Ruby: ERROR on loading driver RTL8814AU\n");
            fflush(stdout);
         }
      }
      else
      {
         iRTL8814AULoaded = 1;
         iCountLoaded++;
      }
   }

   log_line("[HardwareRadio] Added %d modules. Added RTL8812AU? %s. Added RTL8812EU? %s. Added RTL8733BU? %s. Added RTL8814AU? %s. Added Atheros? %s",
      iCountLoaded, (iRTL8812AULoaded?"yes":"no"), (iRTL8812EULoaded?"yes":"no"), (iRTL8733BULoaded?"yes":"no"), (iRTL8814AULoaded?"yes":"no"), (iAtherosLoaded?"yes":"no"));
   if ( iEchoToConsole )
   {
      printf( "Ruby: Added %d modules. Added RTL8812AU? %s. Added RTL8812EU? %s. Added RTL8733BU? %s. Added RTL8814AU? %s. Added Atheros? %s\n",
         iCountLoaded, (iRTL8812AULoaded?"yes":"no"), (iRTL8812EULoaded?"yes":"no"), (iRTL8733BULoaded?"yes":"no"), (iRTL8814AULoaded?"yes":"no"), (iAtherosLoaded?"yes":"no"));
      fflush(stdout);
   }
   return iCountLoaded;
}

int _hardware_try_install_rtl8812au(char* szSrcDriver)
{
   if ( (NULL == szSrcDriver) || (0 == szSrcDriver[0]) )
   {
      log_softerror_and_alarm("[HardwareRadio] No file driver specified for RTL8812AU");
      return 0;
   }
   char szOutput[1024];
   char szDriverFullPath[MAX_FILE_PATH_SIZE];
   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);

   strcpy(szDriverFullPath, FOLDER_BINARIES);
   strcat(szDriverFullPath, "drivers/");
   strcat(szDriverFullPath, szSrcDriver);
   log_line("[HardwareRadio] Driver file to use: [%s]", szDriverFullPath);
   
   if ( access(szDriverFullPath, R_OK) == -1 )
   {
      log_softerror_and_alarm("[HardwareRadio] Can't access driver file: [%s]", szDriverFullPath);
      if ( ! hardware_is_running_on_runcam_vrx() )
         return 0;

      strcpy(szDriverFullPath, "/home/88XXau_wfb.ko");
      log_line("[HardwareRadio] Driver file to use: [%s]", szDriverFullPath);
      if ( access(szDriverFullPath, R_OK) == -1 )
      {
         log_softerror_and_alarm("[HardwareRadio] Can't access driver file: [%s]", szDriverFullPath);
         return 0;
      }
   }

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /lib/modules/%s/kernel/drivers/net/wireless/88XXau.ko", szDriverFullPath, szPlatform);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "insmod /lib/modules/%s/kernel/drivers/net/wireless/88XXau.ko rtw_tx_pwr_idx_override=1 2>&1", szPlatform);
   hw_execute_bash_command_raw(szComm, szOutput);
   log_line("[HardwareRadio] Insert mod result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to insert driver (%s) on platform (%s), error: (%s)", szDriverFullPath, szPlatform, szOutput);
      return 0;
   }
   int iRes = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( -1 == iRes )
      log_softerror_and_alarm("Updating drivers database after driver install took too lonk. Abandoning operation.");
   hardware_sleep_ms(50);
   hw_execute_bash_command_raw("modprobe 88XXau rtw_tx_pwr_idx_override=1 2>&1", szOutput);
   log_line("[HardwareRadio] Modprobe result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to install driver (%s) on platform (%s), error: (%s)", szDriverFullPath, szPlatform, szOutput);
      return 0;
   }
   log_line("[HardwareRadio] Installed driver (%s) on platform (%s)", szDriverFullPath, szPlatform);
   iRes = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( -1 == iRes )
      log_softerror_and_alarm("Updating drivers database after driver install took too lonk. Abandoning operation.");
   return 1;
}

int hardware_install_driver_rtl8812au(int iEchoToConsole)
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   return 1;
   #endif

   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[HardwareRadio] Installing RTL8812AU driver for platform: [%s]", szPlatform);
   if ( iEchoToConsole )
   {
      printf("Ruby: Installing RTL8812AU driver for platform: %s ...\n", szPlatform);
      fflush(stdout);
   }

   hw_execute_bash_command("sudo modprobe cfg80211", NULL);

   #if defined HW_PLATFORM_RASPBERRY
   char szDriverFile[128];
   if ( NULL != strstr(szPlatform, "v7l+") )
      strcpy(szDriverFile, "88XXau-pi+.ko");
   else
      strcpy(szDriverFile, "88XXau-pi.ko");

   int iRes = _hardware_try_install_rtl8812au(szDriverFile);
   if ( 1 == iRes )
      return iRes;
  
   if ( NULL != strstr(szPlatform, "v7l+") )
      strcpy(szDriverFile, "88XXau-pi.ko");
   else
      strcpy(szDriverFile, "88XXau-pi+.ko");
   return _hardware_try_install_rtl8812au(szDriverFile);

   #endif

   #if defined HW_PLATFORM_RADXA
   char szDriverFile[128];
   strcpy(szDriverFile, "88XXau-radxa.ko");
   return _hardware_try_install_rtl8812au(szDriverFile);
   #endif

   return 0;
}

int _hardware_try_install_rtl8812eu(char* szSrcDriver)
{
   if ( (NULL == szSrcDriver) || (0 == szSrcDriver[0]) )
   {
      log_softerror_and_alarm("[HardwareRadio] No file driver specified for RTL8812EU");
      return 0;
   }
   char szOutput[1024];
   char szDriverFullPath[MAX_FILE_PATH_SIZE];
   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);

   strcpy(szDriverFullPath, FOLDER_BINARIES);
   strcat(szDriverFullPath, "drivers/");
   strcat(szDriverFullPath, szSrcDriver);
   log_line("[HardwareRadio] Driver file to use: [%s]", szDriverFullPath);
   
   if ( access(szDriverFullPath, R_OK) == -1 )
   {
      log_softerror_and_alarm("[HardwareRadio] Can't access driver file: [%s]", szDriverFullPath);
      if ( ! hardware_is_running_on_runcam_vrx() )
         return 0;

      strcpy(szDriverFullPath, "/home/8812eu_radxa.ko");
      log_line("[HardwareRadio] Driver file to use: [%s]", szDriverFullPath);
      if ( access(szDriverFullPath, R_OK) == -1 )
      {
         log_softerror_and_alarm("[HardwareRadio] Can't access driver file: [%s]", szDriverFullPath);
         return 0;
      }
   }

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /lib/modules/%s/kernel/drivers/net/wireless/8812eu.ko", szDriverFullPath, szPlatform);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "insmod /lib/modules/%s/kernel/drivers/net/wireless/8812eu.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", szPlatform);
   hw_execute_bash_command_raw(szComm, szOutput);
   log_line("[HardwareRadio] Insert mod result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to insert driver (%s) on platform (%s), error: (%s)", szDriverFullPath, szPlatform, szOutput);
      return 0;
   }

   int iRes = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( -1 == iRes )
      log_softerror_and_alarm("Updating drivers database after driver install took too lonk. Abandoning operation.");
   hardware_sleep_ms(50);

   hw_execute_bash_command_raw("modprobe 8812eu rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1", szOutput);
   log_line("[HardwareRadio] Modprobe result: [%s]", szOutput);
   if ( strlen(szOutput) > 10 )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to install driver (%s) on platform (%s), error: (%s)", szDriverFullPath, szPlatform, szOutput);
      return 0;
   }
   log_line("[HardwareRadio] Installed driver (%s) on platform (%s)", szDriverFullPath, szPlatform);
   iRes = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( -1 == iRes )
      log_softerror_and_alarm("Updating drivers database after driver install took too lonk. Abandoning operation.");
   return 1;
}

int hardware_install_driver_rtl8812eu(int iEchoToConsole)
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   return 1;
   #endif

   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[HardwareRadio] Installing RTL8812EU driver for platform: [%s]", szPlatform);
   if ( iEchoToConsole )
   {
      printf("Ruby: Installing RTL8812EU driver for platform: %s ...\n", szPlatform);
      fflush(stdout);
   }

   hw_execute_bash_command("sudo modprobe cfg80211", NULL);

   #if defined HW_PLATFORM_RASPBERRY
   char szDriverFile[128];
   if ( NULL != strstr(szPlatform, "v7l+") )
      strcpy(szDriverFile, "8812eu-pi+.ko");
   else
      strcpy(szDriverFile, "8812eu-pi.ko");
   int iRes = _hardware_try_install_rtl8812eu(szDriverFile);
   if ( 1 == iRes )
      return 1;

   if ( NULL != strstr(szPlatform, "v7l+") )
      strcpy(szDriverFile, "8812eu-pi.ko");
   else
      strcpy(szDriverFile, "8812eu-pi+.ko");
   return _hardware_try_install_rtl8812eu(szDriverFile);
   #endif

   #if defined HW_PLATFORM_RADXA
   char szDriverFile[128];
   strcpy(szDriverFile, "8812eu-radxa.ko");
   return _hardware_try_install_rtl8812eu(szDriverFile);
   #endif

   return 0;
}

int hardware_install_driver_rtl8733bu(int iEchoToConsole)
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   return 1;
   #endif

   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[HardwareRadio] Installing RTL8733BU driver for platform: [%s]", szPlatform);
   if ( iEchoToConsole )
   {
      printf("Ruby: Installing RTL8733BU driver for platform: %s ...\n", szPlatform);
      fflush(stdout);
   }
   return 1;
}

int hardware_install_driver_rtl8814au(int iEchoToConsole)
{
   #if defined HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Driver installation for RTL8814AU on OpenIPC is typically handled by the firmware image.");
   if ( iEchoToConsole )
   {
      printf("Ruby: Driver installation for RTL8814AU on OpenIPC is handled by the firmware.\n");
      fflush(stdout);
   }
   return 1; // Assume success or manual intervention needed
   #endif

   char szPlatform[256];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("[HardwareRadio] Installing RTL8814AU driver for platform: [%s]", szPlatform);
   if ( iEchoToConsole )
   {
      printf("Ruby: Installing RTL8814AU driver for platform: %s ...\n", szPlatform);
      fflush(stdout);
   }

   // Placeholder for actual driver installation logic.
   // For now, as per subtask, log that manual installation is required.
   log_softerror_and_alarm("[HardwareRadio] RTL8814AU driver files are not currently packaged with Ruby. Manual installation is required for this chipset.");
   if ( iEchoToConsole )
   {
      printf("Ruby: ERROR: RTL8814AU driver files not packaged. Manual installation required.\n");
      fflush(stdout);
   }
   
   // Example of what could be here if driver files were available:
   // char szDriverFile[128];
   // if ( NULL != strstr(szPlatform, "v7l+") ) // Example for RPi
   //    strcpy(szDriverFile, "8814au-pi+.ko");
   // else
   //    strcpy(szDriverFile, "8814au-pi.ko");
   // return _hardware_try_install_rtl8814au_generic(szDriverFile, "8814au", iEchoToConsole); // A new helper similar to _hardware_try_install_rtl8812au

   return 0; // Return 0 to indicate failure, prompting manual installation.
}

void hardware_install_drivers(int iEchoToConsole)
{
   char szPlatform[128];
   hw_execute_bash_command("uname -r", szPlatform);
   removeTrailingNewLines(szPlatform);
   log_line("Platform: [%s]", szPlatform);

   log_line("[HardwareRadio] Installing drivers for platform: %s, board: %s ...", szPlatform, str_get_hardware_board_name(hardware_getBoardType()));
   if ( iEchoToConsole )
   {
      printf("Ruby: Installing drivers for platform: %s ...\n", szPlatform);
      fflush(stdout);
   }
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   log_line("[HardwareRadio] Drivers are already installed on OpenIPC.");
   #else
   char szComm[256];
   hw_execute_bash_command("rmmod 88XXau 2>&1 1>/dev/null", NULL);
   hw_execute_bash_command("rmmod 88XXau_wfb 2>&1 1>/dev/null", NULL);
   hw_execute_bash_command("rmmod 8812au 2>&1 1>/dev/null", NULL);
   hw_execute_bash_command("rmmod 8812eu 2>&1 1>/dev/null", NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf /lib/modules/%s/updates/88XXau*", szPlatform);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf /lib/modules/%s/updates/8812au*", szPlatform);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf /lib/modules/%s/updates/8812eu*", szPlatform);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf /lib/modules/%s/kernel/drivers/net/wireless/8812eu*", szPlatform);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf /lib/modules/%s/kernel/drivers/net/wireless/88XXau*", szPlatform);
   hw_execute_bash_command(szComm, NULL);
   if ( iEchoToConsole )
   {
      printf("Ruby: Updating drivers database. It might take a while. Please wait...\n");
      fflush(stdout);
   }
   int iRes = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( (-1 == iRes) && iEchoToConsole )
   {
      log_softerror_and_alarm("Updating drivers database after cleanup took too lonk. Abandoning operation.");
      printf("Ruby: Updating drives database took too long. Abandoning operation.\n");
      fflush(stdout);
   }
   #endif

   if ( 0 == hardware_install_driver_rtl8812au(iEchoToConsole) )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: Installing RTL8812AU driver failed.\n");
         fflush(stdout);
      }
   }
   else
   {
      log_line("[HardwareRadio] Installed RTL8812AU driver");
      if ( iEchoToConsole )
      {
         printf("Ruby: Installed RTL8812AU driver.\n");
         fflush(stdout);
      }
   }

   if ( 0 == hardware_install_driver_rtl8812eu(iEchoToConsole) )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: Installing RTL8812EU driver failed.\n");
         fflush(stdout);
      }
   }
   else
   {
      log_line("[HardwareRadio] Installed RTL8812EU driver");
      if ( iEchoToConsole )
      {
         printf("Ruby: Installed RTL8812EU driver.\n");
         fflush(stdout);
      }
   }

   if ( 0 == hardware_install_driver_rtl8733bu(iEchoToConsole) )
   {
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: Installing RTL8733BU driver failed.\n");
         fflush(stdout);
      }
   }
   else
   {
      log_line("[HardwareRadio] Installed RTL8733BU driver");
      if ( iEchoToConsole )
      {
         printf("Ruby: Installed RTL8733BU driver.\n");
         fflush(stdout);
      }
   }

   if ( 0 == hardware_install_driver_rtl8814au(iEchoToConsole) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to install RTL8814AU driver.");
      if ( iEchoToConsole )
      {
         printf("Ruby: ERROR: Installing RTL8814AU driver failed.\n");
         fflush(stdout);
      }
   }
   else
   {
      log_line("[HardwareRadio] Installed RTL8814AU driver");
      if ( iEchoToConsole )
      {
         printf("Ruby: Installed RTL8814AU driver.\n");
         fflush(stdout);
      }
   }

   #if defined HW_PLATFORM_RADXA

   hw_execute_bash_command("lsusb", NULL);
   hw_execute_bash_command("sudo modprobe -r aic8800_fdrv 2>&1 1>/dev/null", NULL);
   hw_execute_bash_command("sudo modprobe -r aic8800_bsp 2>&1 1>/dev/null", NULL);
   hw_execute_bash_command("lsusb", NULL);
   hw_execute_bash_command("lsmod", NULL);
   hw_execute_bash_command("ip link", NULL);
   if ( iEchoToConsole )
   {
      printf("Ruby: Updating drivers database. It might take a while. Please wait...\n");
      fflush(stdout);
   }
   int iRes2 = hw_execute_bash_command_timeout("depmod -a", NULL, 60000);
   if ( (-1 == iRes2) && iEchoToConsole )
   {
      log_softerror_and_alarm("Updating drivers database after cleanup took too lonk. Abandoning operation.");
      printf("Ruby: Updating drives database took too long. Abandoning operation.\n");
      fflush(stdout);
   }
   hw_execute_bash_command("sync", NULL);

   #endif

   log_line("[HardwareRadio] Done installing drivers.");
   if ( iEchoToConsole )
   {
      printf("Ruby: Done installing drivers.\n");
      fflush(stdout);
   }
}


int _configure_radio_interface_atheros(int iInterfaceIndex, radio_hw_info_t* pRadioHWInfo, u32 uDelayMS)
{
   if ( (NULL == pRadioHWInfo) || (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return 0;

   char szComm[128];
   char szOutput[2048];

   #ifdef HW_PLATFORM_OPENIPC_CAMERA

   //sprintf(szComm, "iwconfig %s mode monitor", pRadioHWInfo->szName);
   sprintf(szComm, "iw dev %s set type monitor", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   //sprintf(szComm, "ifconfig %s up", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   return 1;
   #endif

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   int dataRateMb = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS/1000/1000;
   // To fix
   //if ( ! s_bIsStation )
   //if ( giDataRateMbAtheros > 0 )
   //   dataRateMb = giDataRateMbAtheros;
   
   if ( dataRateMb == 0 )
      dataRateMb = DEFAULT_RADIO_DATARATE_VIDEO/1000/1000;
   if ( dataRateMb > 0 )
      sprintf(szComm, "iw dev %s set bitrates legacy-2.4 %d lgi-2.4", pRadioHWInfo->szName, dataRateMb );
   else
      sprintf(szComm, "iw dev %s set bitrates ht-mcs-2.4 %d lgi-2.4", pRadioHWInfo->szName, -dataRateMb-1 );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   //sprintf(szComm, "ifconfig %s down 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);
   
   sprintf(szComm, "iw dev %s set monitor none", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);
   
   pRadioHWInfo->iCurrentDataRateBPS = dataRateMb*1000*1000;

   return 1;
}

int _configure_radio_interface_realtek(int iInterfaceIndex, radio_hw_info_t* pRadioHWInfo, u32 uDelayMS)
{
   if ( (NULL == pRadioHWInfo) || (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return 0;

   char szComm[128];
   char szOutput[2048];

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   
   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iwconfig %s mode monitor", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);
   
   return 1;

   #endif

   sprintf(szComm, "ip link set dev %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("[HardwareRadio] Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor none 2>&1", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "iw dev %s set monitor fcsfail", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   hardware_sleep_ms(uDelayMS);

   sprintf(szComm, "ip link set dev %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   return 1;
}

int hardware_initialize_radio_interface(int iInterfaceIndex, u32 uDelayMS)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return 0;

   char szComm[128];
   char szOutput[2048];
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_error_and_alarm("[HardwareRadio] Failed to get radio info for interface %d.", iInterfaceIndex+1);
      return 0;
   }

   log_line("[HardwareRadio] Initialize radio interface %d: %s, (%s)", iInterfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver);

   sprintf(szComm, "ip link set dev %s down", pRadioHWInfo->szName );

   pRadioHWInfo->iCurrentDataRateBPS = 0;
   //sprintf(szComm, "ifconfig %s mtu 2304 2>&1", pRadioHWInfo->szName );
   sprintf(szComm, "ip link set dev %s mtu 1400", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 != szOutput[0] )
      log_softerror_and_alarm("[HardwareRadio] Unexpected result: [%s]", szOutput);
   hardware_sleep_ms(uDelayMS);

   if ( pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS )
      _configure_radio_interface_atheros(iInterfaceIndex, pRadioHWInfo, uDelayMS);
   else
      _configure_radio_interface_realtek(iInterfaceIndex, pRadioHWInfo, uDelayMS);


   pRadioHWInfo->uCurrentFrequencyKhz = 0;
   pRadioHWInfo->lastFrequencySetFailed = 1;
   pRadioHWInfo->uFailedFrequencyKhz = DEFAULT_FREQUENCY;

   sprintf(szComm, "iwconfig %s rts off 2>&1", pRadioHWInfo->szName);
   hw_execute_bash_command(szComm, szOutput);
   log_line("[HardwareRadio] Initialized radio interface %d: %s", iInterfaceIndex+1, pRadioHWInfo->szName);
   return 1;
}

int hardware_get_radio_interfaces_count()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   return s_iHwRadiosCount;
}

int hardware_get_supported_radio_interfaces_count()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   return s_iHwRadiosSupportedCount;
}

int hardware_add_radio_interface_info(radio_hw_info_t* pRadioInfo)
{
   if ( (NULL == pRadioInfo) || (s_iHwRadiosCount >= MAX_RADIO_INTERFACES) )
      return 0;

   memcpy((u8*)&(sRadioInfo[s_iHwRadiosCount]), pRadioInfo, sizeof(radio_hw_info_t));
   s_iHwRadiosCount++;
   return 1;
}

int hardware_get_radio_index_by_name(const char* szName)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( NULL == szName || 0 == szName[0] )
      return -1;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szName, sRadioInfo[i].szName ) )
         return i;
   return -1;
}

int hardware_get_radio_index_from_mac(const char* szMAC)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return -1;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szMAC, sRadioInfo[i].szMAC ) )
         return i;
   return -1;
}


radio_hw_info_t* hardware_get_radio_info(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return &(sRadioInfo[iRadioIndex]);
}

const char* hardware_get_radio_name(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return sRadioInfo[iRadioIndex].szName;
}

const char* hardware_get_radio_description(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return sRadioInfo[iRadioIndex].szDescription;
}

int hardware_radio_has_low_capacity_links()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( ! sRadioInfo[i].isHighCapacityInterface )
         return 1;
   return 0;
}

int hardware_radio_has_rtl8812au_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( hardware_radio_driver_is_rtl8812au_card(sRadioInfo[i].iRadioDriver) )
            iCount++;
      }
   }

   if ( iCount > 0 )
      return iCount;

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( hardware_radio_driver_is_rtl8812au_card(s_USB_RadioInterfacesInfo[i].iDriver) )
          iCount++;
   }
   return iCount;
}

int hardware_radio_has_rtl8812eu_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
            iCount++;
      }
   }

   if ( iCount > 0 )
      return iCount;

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
          iCount++;
   }
   return iCount;
}

int hardware_radio_has_rtl8733bu_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_8733BU )
            iCount++;
      }
   }

   if ( iCount > 0 )
      return iCount;
   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_8733BU )
          iCount++;
   }
   return iCount;
}

int hardware_radio_has_atheros_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_ATHEROS )
            iCount++;
      }
      return iCount;
   }

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_ATHEROS )
          iCount++;
   }
   return iCount;
}

int hardware_radio_driver_is_rtl8812au_card(int iDriver)
{
   if ( (iDriver == RADIO_HW_DRIVER_REALTEK_8812AU) ||
        (iDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU) ||
        (iDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU) ||
        (iDriver == RADIO_HW_DRIVER_REALTEK_RTL88X2BU) ||
        (iDriver == RADIO_HW_DRIVER_MEDIATEK) )
      return 1;
   return 0;
}

int hardware_radio_driver_is_rtl8812eu_card(int iDriver)
{
   if ( iDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
      return 1;
   return 0;
}

int hardware_radio_driver_is_rtl8733bu_card(int iDriver)
{
   if ( iDriver == RADIO_HW_DRIVER_REALTEK_8733BU )
      return 1;
   return 0; 
}

int hardware_radio_driver_is_rtl8814au_card(int iDriver)
{
   if ( iDriver == RADIO_HW_DRIVER_REALTEK_RTL8814AU )
      return 1;
   return 0;
}

int hardware_radio_driver_is_atheros_card(int iDriver)
{
   if ( iDriver == RADIO_HW_DRIVER_ATHEROS )
      return 1;
   return 0;
}

int hardware_radio_type_is_ieee(int iRadioType)
{
   if ( iRadioType == RADIO_TYPE_RALINK ||
        iRadioType == RADIO_TYPE_ATHEROS ||
        iRadioType == RADIO_TYPE_REALTEK ||
        iRadioType == RADIO_TYPE_MEDIATEK )
      return 1;

   return 0;
}

int hardware_radio_type_is_sikradio(int iRadioType)
{
   if ( iRadioType == RADIO_TYPE_SIK )
      return 1;
   return 0;
}

int hardware_radio_is_wifi_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   return hardware_radio_type_is_ieee(pRadioInfo->iRadioType);
}

int hardware_radio_is_serial_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( pRadioInfo->isSerialRadio )
      return 1;
   return 0;
}

int hardware_radio_is_elrs_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( ! hardware_radio_is_serial_radio(pRadioInfo) )
      return 0;
   if ( pRadioInfo->iCardModel != CARD_MODEL_SERIAL_RADIO_ELRS )
      return 0;
     
   return 1;
}

int hardware_radio_index_is_high_capacity(int iRadioIndex)
{
   if ( (iRadioIndex <0) || (iRadioIndex >= s_iHwRadiosCount) )
      return 0;
   return sRadioInfo[iRadioIndex].isHighCapacityInterface;
}

int hardware_radio_index_is_wifi_radio(int iRadioIndex)
{
   if ( (iRadioIndex <0) || (iRadioIndex >= s_iHwRadiosCount) )
      return 0;
   return hardware_radio_is_wifi_radio(&sRadioInfo[iRadioIndex]);
}

int hardware_radio_index_is_serial_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_serial_radio(pRadioHWInfo);
}

int hardware_radio_index_is_elrs_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_elrs_radio(pRadioHWInfo);
}

int hardware_radio_index_is_sik_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_sik_radio(pRadioHWInfo);
}

int hardware_radio_is_sik_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( pRadioInfo->iRadioType == RADIO_TYPE_SIK )
      return 1;

   return 0;
}

int hardware_radioindex_supports_frequency(int iRadioIndex, u32 freqKhz)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return 0;

   if ( sRadioInfo[iRadioIndex].isSerialRadio )
   if ( ! sRadioInfo[iRadioIndex].isConfigurable )
   if ( sRadioInfo[iRadioIndex].uCurrentFrequencyKhz == freqKhz )
      return 1;
   int band = getBand(freqKhz);
   
   if ( band & sRadioInfo[iRadioIndex].supportedBands )
      return 1;
    
   return 0;
}

int hardware_radio_supports_frequency(radio_hw_info_t* pRadioInfo, u32 freqKhz)
{
   if ( 0 == freqKhz )
      return 0;
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   int band = getBand(freqKhz);
   
   if ( band & pRadioInfo->supportedBands )
      return 1;
    
   return 0;
}

int hardware_get_radio_count_opened_for_read()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   int count = 0;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( sRadioInfo[i].openedForRead )
         count++;
   return count;
}

radio_hw_info_t* hardware_get_radio_info_for_usb_port(const char* szUSBPort)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( (NULL == szUSBPort) || (0 == szUSBPort[0]) )
      return NULL;

   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( strcmp(szUSBPort, sRadioInfo[i].szUSBPort) == 0 )
         return &(sRadioInfo[i]);
   return NULL;
}

radio_hw_info_t* hardware_get_radio_info_from_mac(const char* szMAC)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return NULL;

   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szMAC, sRadioInfo[i].szMAC) )
         return &(sRadioInfo[i]);
   return NULL;
}
