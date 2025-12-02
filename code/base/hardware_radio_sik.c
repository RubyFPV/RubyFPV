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

#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../common/string_utils.h"
#include "hardware.h"
#include "hardware_radio.h"
#include "hardware_radio_sik.h"
#include "hardware_serial.h"
#include "hardware_procs.h"

#define SIK_PARAM_INDEX_LOCAL_SPEED 1
#define SIK_PARAM_INDEX_NETID 3
#define SIK_PARAM_INDEX_AIRSPEED 2
#define SIK_PARAM_INDEX_TXPOWER 4
#define SIK_PARAM_INDEX_ECC 5
#define SIK_PARAM_INDEX_FREQ_MIN 8
#define SIK_PARAM_INDEX_FREQ_MAX 9
#define SIK_PARAM_INDEX_CHANNELS 10
#define SIK_PARAM_INDEX_DUTYCYCLE 11
#define SIK_PARAM_INDEX_LBT 12
#define SIK_PARAM_INDEX_MCSTR 13


radio_hw_info_t s_SiKRadioLastKnownInfo[MAX_RADIO_INTERFACES];
int s_iSiKRadioLastKnownCount = 0;
int s_iSiKRadioCount = 0;
int s_iSiKFirmwareIsOld = 0;

int s_iGetSiKConfigAsyncRunning = 0;
int s_iGetSiKConfigAsyncInterfaceIndex = -1;
pthread_t s_pThreadGetSiKConfigAsync;


int hardware_radio_sik_firmware_is_old()
{
   return s_iSiKFirmwareIsOld;
}

int hardware_radio_has_sik_radios()
{
   if ( s_iSiKRadioCount > 0 )
      return s_iSiKRadioCount;

   int iCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         iCount++;
   }
   return iCount;
}

radio_hw_info_t* hardware_radio_sik_get_from_serial_port(const char* szSerialPort)
{
   if ( (NULL == szSerialPort) || (0 == szSerialPort[0]) )
      return NULL;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
         continue;
      if ( 0 == pRadioInfo->szDriver[0] )
         continue;
      if ( 0 != strcmp(pRadioInfo->szDriver, szSerialPort) )
         continue;
      return pRadioInfo;
   }
   return NULL;
}

int hardware_radio_sik_reinitialize_serial_ports()
{
   log_line("[HW-RSK] Reinitializing SiK serial ports...");

   int iCurrentSiKInterfacesCount = 0;
   char szSiKInterfacesPorts[MAX_RADIO_INTERFACES][256];

   log_line("[HW-RSK] Current SiK interfaces (before re-initialization):");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
    
      log_line("[HW-RSK] SiK radio interface %d, name %s, driver: %s, MAC: %s", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, pRadioHWInfo->szMAC);
      strcpy(&(szSiKInterfacesPorts[iCurrentSiKInterfacesCount][0]), pRadioHWInfo->szDriver);
      iCurrentSiKInterfacesCount++;
   }

   int iCurrentSiKSerialPorts = 0;
   char szSiKSerialPorts[MAX_RADIO_INTERFACES][256];

   log_line("[HW-RSK] Current SiK serial ports (before re-initialization):");
   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info(i);
      if ( NULL == pSerialPort )
         continue;
      if ( pSerialPort->iPortUsage != SERIAL_PORT_USAGE_SIK_RADIO )
         continue;
      radio_hw_info_t* pSiKRadio = hardware_radio_sik_get_from_serial_port(pSerialPort->szPortDeviceName); 
      if ( NULL == pSiKRadio )
         continue;
      log_line("[HW-RSK] Serial port %d: %s used for SiK radio interface %s", i+1, pSerialPort->szPortDeviceName, pSiKRadio->szName);
      strcpy(&(szSiKSerialPorts[iCurrentSiKSerialPorts][0]), pSerialPort->szPortDeviceName);
      iCurrentSiKSerialPorts++;
   }

   char szOutput[4096];
   char szDrivers[1024];
   int bTryDefault = 0;
   szDrivers[0] = 0;

   hw_execute_bash_command("cat /proc/tty/drivers | grep ttyUSB", szOutput);
   if ( 0 == szOutput[0] )
      bTryDefault = 1;
   if ( szOutput != strstr(szOutput, "usbserial") )
      bTryDefault = 1;

   if ( ! bTryDefault )
   {
      szOutput[0] = 0;
      hw_execute_bash_command("lsmod | grep usbserial", szOutput);
      if ( 0 == szOutput[0] )
         bTryDefault = 1;
      if ( ! bTryDefault )
      {
         int iIndex = strlen(szOutput)-1;
         while ( (iIndex > 0) && (szOutput[iIndex] != ' '))
            iIndex--;

         if ( iIndex == 0 )
            bTryDefault = 1;
         if ( iIndex > (int)strlen(szOutput)-3 )
            bTryDefault = 1;

         if ( ! bTryDefault )
            strcpy(szDrivers, &(szOutput[iIndex+1]) );
      }
   }

   if ( (! bTryDefault) && (0 != szDrivers[0]) )
   {
      int iIndex = strlen(szDrivers)-1;
      while( iIndex >= 0 )
      {
         if ( (iIndex != 0) && (szDrivers[iIndex] != ',') )
         {
            iIndex--;
            continue;
         }
         char* pToken = &(szDrivers[iIndex]);
         if ( *pToken == ',' )
            pToken++;

         char szComm[256];
         sprintf(szComm, "modprobe -r %s 2>&1", pToken);
         hw_execute_bash_command(szComm, szOutput);
         if ( NULL != strstr(szOutput, "is in use") )
         {
            hw_execute_bash_command("modprobe -r usbserial 2>&1", NULL);
            hardware_sleep_ms(20);
            hw_execute_bash_command(szComm, NULL);
         }
         hardware_sleep_ms(20);
         szOutput[0] = 0;
         sprintf(szComm, "modprobe %s", pToken);
         hw_execute_bash_command(szComm, szOutput);
         if ( strlen(szOutput) > 10 )
            bTryDefault = 1;

         szDrivers[iIndex] = 0;
         iIndex--;
      }
   }
   
   if ( bTryDefault || (0 == szDrivers[0]) )
   {
      log_line("[Hardware Radio] Doing default USB serial reinitialization.");

      hw_execute_bash_command("modprobe -r pl2303 2>&1", szOutput);
      if ( NULL != strstr(szOutput, "is in use") )
      {
         hw_execute_bash_command("modprobe -r usbserial 2>&1", NULL);
         hardware_sleep_ms(20);
         hw_execute_bash_command("modeprobe -r pl2303 2>&1", NULL);
      }

      hw_execute_bash_command("modprobe -r cp210x 2>&1", szOutput);
      if ( NULL != strstr(szOutput, "is in use") )
      {
         hw_execute_bash_command("modprobe -r usbserial 2>&1", NULL);
         hardware_sleep_ms(20);
         hw_execute_bash_command("modeprobe -r cp210x 2>&1", NULL);
      }

      hardware_sleep_ms(20);
      hw_execute_bash_command("modprobe cp210x 2>&1", NULL);
      hw_execute_bash_command("modprobe pl2303 2>&1", NULL);
   }
   
   log_line("[HW-RSK] Reinitialized serial ports drivers.");

   int iAllTheSame = 1;

   for( int i=0; i<iCurrentSiKSerialPorts; i++ )
   {
      if( access( szSiKSerialPorts[i], R_OK ) == -1 )
      {
         log_line("[HW-RSK] Reinitialization error: serial port %s is not present!", szSiKSerialPorts[i]);
         iAllTheSame = 0;
         break;
      }
   }

   if ( iAllTheSame )
   {
      log_line("[HW-RSK] Done. All SiK serial ports have been reinitialized.");
      return 1;
   }
   else
   {
      log_line("[HW-RSK] Not all SiK serial ports are present. Reinitialization failed.");
      return 0;
   }
}

int hardware_radio_sik_enter_command_mode(int iSerialPortFile, int iBaudRate, shared_mem_process_stats* pProcessStats)
{
   if ( iSerialPortFile <= 0 )
      return 0;

   // First, empty the receiving buffers for any accumulated pending data in it, if any data can be received.
   
   u8 bufferResponse[4024];
   int iBufferSize = 4023;
   
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 50;

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(iSerialPortFile, &readset);
   
   int res = select(iSerialPortFile+1, &readset, NULL, NULL, &to);
   if ( res > 0 )
   if ( FD_ISSET(iSerialPortFile, &readset) )
   {
      int iRead = read(iSerialPortFile, bufferResponse, iBufferSize-1);
      log_line("[HW-RSK] SiK flushed Rx buffer first. %d bytes flushed.", iRead);
   }

   // Send command

   if ( ! hardware_serial_send_sik_command(iSerialPortFile, "+++") )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to send to SiK radio AT command mode change.");
      return 0;
   }
   
   log_line("[HW-RSK] Sent SiK command to enter AT command mode...");

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   // Wait for "OK\n" response
   u32 uTimeStart = get_current_timestamp_ms();
   u32 uTimeEnd = uTimeStart + 2500;
   u32 uTimeLastRecvData = 0;
   int iReceivedAnyData = 0;
   int iFoundResponse = 0;
   int iBufferPos = 0;
   
   memset(bufferResponse, 0, iBufferSize);

   int iRetryCounter = 0;
   while ( 1 )
   {
      iRetryCounter++;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow >= uTimeEnd )
            break;
      
      // Wait max 200 milisec
      to.tv_sec = 0;
      to.tv_usec = (uTimeEnd - uTimeNow)*1000;
      if ( to.tv_usec > (int)200000 )
         to.tv_usec = (int)200000;

      if ( (iRetryCounter % 5) == 0 )
         log_line("[HW-RSK] Waiting (try %d) for read data for %d milisec, at buffer pos %d (of %d bytes)...", iRetryCounter, to.tv_usec/1000, iBufferPos, iBufferSize);
      FD_ZERO(&readset);
      FD_SET(iSerialPortFile, &readset);
      
      res = select(iSerialPortFile+1, &readset, NULL, NULL, &to);

      if ( (res <= 0) || (! FD_ISSET(iSerialPortFile, &readset)) )
      {
         uTimeNow = get_current_timestamp_ms();
         if ( iFoundResponse )
         if ( uTimeLastRecvData < uTimeNow-320 )
         {
            log_line("[HW-RSK] No more data read signaled after receiving command response.");
            break;
         }
         if ( (0 == iReceivedAnyData) && (uTimeNow >= (uTimeStart + 1200)) && (iRetryCounter > 4 ) )
         {
            log_line("[HW-RSK] No read signaled and no data received for 1.2 seconds. No more retires, abandon it.");
            break;
         }

         if ( uTimeNow >= uTimeEnd )
            log_line("[HW-RSK] No read signaled. No more retires.");
         else if ( (iRetryCounter % 5) == 0 )
            log_line("[HW-RSK] No read signaled. Try again.");
         continue;
      }

      int iRead = read(iSerialPortFile, (u8*)&(bufferResponse[iBufferPos]), iBufferSize-iBufferPos);
      uTimeNow = get_current_timestamp_ms();
      if ( iRead <= 0 )
      {
         if ( iFoundResponse )
         if ( uTimeLastRecvData < uTimeNow-320 )
         {
            log_line("[HW-RSK] No more data read available after received command response.");
            break;
         }
         log_line("[HW-RSK] No read data. Try again.");
         continue;
      }
      iReceivedAnyData += iRead;
      if ( (iRead < 4) && (uTimeLastRecvData < uTimeNow-330) )
      if ( iFoundResponse )
      {
         log_line("[HW-RSK] No more data read (empty rx buffers, only %d bytes left and read) after receiving command response.", iRead);
         break;
      }

      uTimeLastRecvData = uTimeNow;
      iBufferPos += iRead;

      char szBuff[128];
      szBuff[0] = 0;
      int iLogCount = 6;
      if ( iLogCount > iBufferPos )
         iLogCount = iBufferPos;
      for( int i=0; i<iLogCount; i++ )
      {
         strcat(szBuff, " ");
         char szVal[16];
         char szChar[3];
         szChar[0] = 0;
         szChar[1] = 0;
         if ( isprint(bufferResponse[iBufferPos-iLogCount+i]) )
            szChar[0] = bufferResponse[iBufferPos-iLogCount+i];
         else
            szChar[0] = '?';
         sprintf(szVal, "0x%02X (%s)", bufferResponse[iBufferPos-iLogCount+i], szChar);
         strcat(szBuff, szVal);
      }
      log_line("[HW-RSK] Read %d bytes response, total %d bytes in buffer, (last %d bytes:%s). Parsing it...", iRead, iBufferPos, iLogCount, szBuff);

      int iCount = 0;
      for( int i=iBufferPos-3; i>=0; i-- )
      {
         iCount++;

         if ( bufferResponse[i] == 'O' )
         if ( bufferResponse[i+1] == 'K' )
         if ( (bufferResponse[i+2] == 10) || (bufferResponse[i+2] == 13) )
         {
            if ( iCount > 4 )
               log_line("[HW-RSK] Found tentative valid response 'Ok' at pos %d", i);
            else
            {
               iFoundResponse = 1;
               hardware_sleep_ms(20);
               log_line("[HW-RSK] Found valid response 'Ok'.");
               break;
            }
         }
         if ( bufferResponse[i] == '+' )
         if ( bufferResponse[i+1] == '+' )
         if ( bufferResponse[i+2] == '+' )
         {
            if ( iCount > 4 )
               log_line("[HW-RSK] Found tentative valid response '+++' at pos %d", i);
            else
            {
               iFoundResponse = 1;
               hardware_sleep_ms(20);
               log_line("[HW-RSK] Found valid response '+++'.");
               break;
            }
         }
      }
      if ( iBufferPos > 1000 )
      {
         for( int i=0; i<iBufferPos-990; i++ )
            bufferResponse[i] = bufferResponse[i+990];
         iBufferPos -= 990;
         log_line("[HW-RSK] Reduced input buffer cached data from %d to %d bytes.", iBufferPos+990, iBufferPos );
      }
   }

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   if ( ! iFoundResponse )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to enter SiK radio into command mode (received %d bytes in response to enter AT mode, but not Ok response, at baudrate: %d).", iBufferPos, iBaudRate);
      return 0;
   }

   
   log_line("[HW-RSK] SiK radio entered in AT command mode successfully at bautrate %d.", iBaudRate);
   return 1;
}

int _hardware_radio_sik_get_all_params(radio_hw_info_t* pRadioInfo, int iSerialPortFile, int iBaudRate, shared_mem_process_stats* pProcessStats, int iComputeMAC)
{
   if ( NULL == pRadioInfo || iSerialPortFile <= 0 )
      return 0;

   if ( ! hardware_radio_sik_enter_command_mode(iSerialPortFile, iBaudRate, pProcessStats) )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to enter SiK radio into AT command mode.");
      return 0;
   }
   
   log_line("[HW-RSK] Getting SiK radio info from device...");
   
   u8 bufferResponse[1024];

   if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI", bufferResponse, 255) )
   {
      bufferResponse[62] = 0;
      strcpy(pRadioInfo->szDescription, (char*)bufferResponse);

      s_iSiKFirmwareIsOld = 0;
      pRadioInfo->uExtraFlags &= ~RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD;

      if ( NULL == strstr((char*)bufferResponse, "SiK 2.2") )
      {
         s_iSiKFirmwareIsOld = 1;
         pRadioInfo->uExtraFlags |= RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD;
      }
   }
   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   if ( 1 == iComputeMAC )
   {
      char szMAC[256];
      szMAC[0] = 0;

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI1", bufferResponse, 255) )
         strcat(szMAC, (char*)bufferResponse);
      else
         strcat(szMAC, "X");
      strcat(szMAC, "-");
      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI2", bufferResponse, 255) )
         strcat(szMAC, (char*)bufferResponse);
      else
         strcat(szMAC, "X");
      strcat(szMAC, "-");

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI3", bufferResponse, 255) )
         strcat(szMAC, (char*)bufferResponse);
      else
         strcat(szMAC, "X");
      strcat(szMAC, "-");

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI4", bufferResponse, 255) )
         strcat(szMAC, (char*)bufferResponse);
      else
         strcat(szMAC, "X");
      strcat(szMAC, "-");

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATS8?", bufferResponse, 255) )
      {
         u32 uFreq = (u32)atoi((char*)bufferResponse);
         if ( uFreq < 500000 )
            strcat(szMAC, "433");
         else if ( uFreq < 890000 )
            strcat(szMAC, "868");
         else
            strcat(szMAC, "915");
      }
      else
         strcat(szMAC, "NNN");

      strncpy(pRadioInfo->szMAC, szMAC, MAX_MAC_LENGTH-1);
      pRadioInfo->szMAC[MAX_MAC_LENGTH-1] = 0;
      log_line("[HW-RSK] Computed SiK radio MAC: [%s]", pRadioInfo->szMAC);
   }

   // Get parameters 0 to 15
   
   int iMaxParam = 16;
   if ( iMaxParam > MAX_RADIO_HW_PARAMS )
      iMaxParam = MAX_RADIO_HW_PARAMS;

   for( int i=0; i<iMaxParam; i++ )
   {
      pRadioInfo->uHardwareParamsList[i] = MAX_U32;
      char szComm[32];
      sprintf(szComm, "ATS%d?", i);
      if ( hardware_radio_sik_send_command(iSerialPortFile, szComm, bufferResponse, 255) )
      {
         pRadioInfo->uHardwareParamsList[i] = atoi((char*)bufferResponse);
      }
      else
         pRadioInfo->uHardwareParamsList[i] = MAX_U32;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   // Exit AT command mode
   hardware_radio_sik_send_command(iSerialPortFile, "ATO", bufferResponse, 255);

   log_line("[HW-RSK] Exited AT command mode.");

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   char szTmp[256];
   szTmp[0] = 0;
   for( int i=0; i<iMaxParam; i++ )
   {
      if ( 0 != szTmp[0] )
         strcat(szTmp, ", ");

      char szB[32];
      sprintf(szB, "[%d]=%u", i, pRadioInfo->uHardwareParamsList[i]);
      strcat(szTmp, szB);
   }
   log_line("[HW-RSK]: SiK parameters read from device: %s", szTmp);

   log_line("[HW-RSK] SiK read params from device: Air Speed: %u, ECC/LBT/MCSTR: %u/%u/%u",
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR]);
   return 1;
}

int hardware_radio_sik_detect_interfaces()
{
   log_line("[HW-RSK]: Enumerating Sik radios...");
   
   s_iSiKRadioCount = 0;
   s_iSiKRadioLastKnownCount = 0;

   hardware_radio_sik_load_configuration();

   s_iSiKRadioCount = 0;

   radio_hw_info_t* pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB0");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB1");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB2");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   s_iSiKRadioCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || (pRadioInfo->iRadioType != RADIO_TYPE_SIK) )
         continue;
      s_iSiKRadioCount++;
   }

   log_line("[HW-RSK]: Found %d SiK radios.", s_iSiKRadioCount);

   return hardware_radio_sik_save_configuration();
}

int hardware_radio_sik_save_configuration()
{
   s_iSiKRadioCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || (pRadioInfo->iRadioType != RADIO_TYPE_SIK) )
         continue;
      s_iSiKRadioCount++;
   }

   log_line("[HW-RSK]: Saving current SiK radios configuration (%d SiK radio interfaces).", s_iSiKRadioCount);
   
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
   FILE* fd = fopen(szFile, "wb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HW-RSK]: Failed to write last good SiK radio configuration to file: [%s]", szFile);
      return 0;
   }

   fwrite((u8*)&s_iSiKRadioCount, 1, sizeof(int), fd);

   int iCountWrite = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
         continue;
       fwrite( (u8*)pRadioInfo, 1, sizeof(radio_hw_info_t), fd );

      char szBuff[256];
      sprintf(szBuff, "* SiK radio %d of %d on [%s], id: [%s]", iCountWrite+1, s_iSiKRadioCount, pRadioInfo->szDriver, pRadioInfo->szMAC);
      log_line(szBuff);
      iCountWrite++;
   }
   fclose(fd);

   log_line("[HW-RSK]: Saved current SiK radios configuration to file [%s]:", szFile);
   return 1;
}

int hardware_radio_sik_get_air_baudrate_in_bytes(int iRadioInterfaceIndex)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return -1;

   if ( ! hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      return -1;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);

   if ( NULL == pRadioHWInfo )
      return -1;

   return hardware_radio_sik_get_real_air_baudrate(pRadioHWInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED])/8;
}


int hardware_radio_sik_load_configuration()
{
   s_iSiKRadioCount = 0;
   s_iSiKRadioLastKnownCount = 0;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);

   if ( access(szFile, R_OK) == -1 )
   {
      log_line("[HW-RSK]: No last known good SiK radio configuration file. Nothing to load.");
      return 0;
   }
   log_line("[HW-RSK]: Try to load last known good SiK radio configuration from file [%s].", szFile);

   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HW-RSK]: Failed to load last known good SiK radio configuration from file [%s].", szFile);
      return 0;
   }

   if ( sizeof(int) != fread((u8*)&s_iSiKRadioCount, 1, sizeof(int), fd) )
   {
      fclose(fd);
      s_iSiKRadioCount = 0;
      s_iSiKRadioLastKnownCount = 0;
      log_softerror_and_alarm("[HW-RSK]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format.", szFile);
      return 0;
   }

   if ( s_iSiKRadioCount < 0 || s_iSiKRadioCount >= MAX_RADIO_INTERFACES )
   {
      fclose(fd);
      s_iSiKRadioCount = 0;
      s_iSiKRadioLastKnownCount = 0;
      log_softerror_and_alarm("[HW-RSK]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format (2).", szFile);
      return 0;
   }

   for( int i=0; i<s_iSiKRadioCount; i++ )
   {
      if ( sizeof(radio_hw_info_t) != fread((u8*)&(s_SiKRadioLastKnownInfo[i]), 1, sizeof(radio_hw_info_t), fd) )
      {
         fclose(fd);
         s_iSiKRadioCount = 0;
         s_iSiKRadioLastKnownCount = 0;
         log_softerror_and_alarm("[HW-RSK]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format (3/%d).", szFile, i);
         return 0;
      }
   }
   fclose(fd);
   log_line("[HW-RSK]: Loaded last known good SiK radio configuration from file [%s]. %d SiK radios loaded:", szFile, s_iSiKRadioCount);

   for( int i=0; i<s_iSiKRadioCount; i++ )
   {
      if ( (s_SiKRadioLastKnownInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_433) ||
           (s_SiKRadioLastKnownInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_868) ||
           (s_SiKRadioLastKnownInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_915) )
      if ( s_SiKRadioLastKnownInfo[i].isHighCapacityInterface )
      {
         log_line("Hardware: SiK: Removed invalid high capacity flag from radio interface %d (%s, %s)",
            i+1, s_SiKRadioLastKnownInfo[i].szName, s_SiKRadioLastKnownInfo[i].szDriver);
         s_SiKRadioLastKnownInfo[i].isHighCapacityInterface = 0;
      }
      char szBuff[256];
      sprintf(szBuff, "* SiK radio %d of %d on [%s], id: [%s]", i+1, s_iSiKRadioCount, s_SiKRadioLastKnownInfo[i].szDriver, s_SiKRadioLastKnownInfo[i].szMAC);
      log_line(szBuff);
      log_line(" SiK current params: Air Speed: %u, ECC/LBT/MCSTR: %u/%u/%u",
         s_SiKRadioLastKnownInfo[i].uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED],
         s_SiKRadioLastKnownInfo[i].uHardwareParamsList[SIK_PARAM_INDEX_ECC],
         s_SiKRadioLastKnownInfo[i].uHardwareParamsList[SIK_PARAM_INDEX_LBT],
         s_SiKRadioLastKnownInfo[i].uHardwareParamsList[SIK_PARAM_INDEX_MCSTR]);

   }
   s_iSiKRadioLastKnownCount = s_iSiKRadioCount;
   return 1;
}


radio_hw_info_t* hardware_radio_sik_try_detect_on_port(const char* szSerialPort)
{
   if ( NULL == szSerialPort || 0 == szSerialPort[0] )
      return NULL;

   char szDevName[128];
   strcpy(szDevName, szSerialPort);
   if ( 0 == strstr(szSerialPort, "/dev") )
      sprintf(szDevName, "/dev/%s", szSerialPort);

   log_line("[HW-RSK]: Try to find SiK radio on interface [%s]...", szDevName);
   if ( access( szDevName, R_OK ) == -1 )
   {
      log_line("[HW-RSK]: Device [%s] is not present.", szDevName);
      return NULL;
   }
   
   int iBaudRatesList[64];
   int iBaudRatesCount = 0;

   // Try to find it in the last good known configuration
   for( int i=0; i<s_iSiKRadioLastKnownCount; i++ )
   {
      if ( 0 != strcmp(s_SiKRadioLastKnownInfo[i].szDriver, szDevName) )
         continue;
      iBaudRatesList[0] = hardware_radio_sik_get_real_serial_baudrate(s_SiKRadioLastKnownInfo[i].uHardwareParamsList[1]);
      log_line("[HW-RSK]: Found existing configuration for SiK radio on [%s] at %u bps. Try it first.", szDevName, iBaudRatesList[0]);
      iBaudRatesCount++;
      break;
   }

   for( int i=hardware_serial_get_baud_rates_count()-1; i>0; i-- )
   {
      if ( hardware_serial_get_baud_rates()[i] < 57000 )
         break;
      iBaudRatesList[iBaudRatesCount] = hardware_serial_get_baud_rates()[i];
      iBaudRatesCount++;
   }

   for( int i=0; i<iBaudRatesCount; i++ )
   {
      int iSpeed = iBaudRatesList[i];
      int iSerialPort = hardware_open_serial_port(szDevName, iSpeed);
      if ( iSerialPort <= 0 )
         continue;

      static radio_hw_info_t s_radioHWInfoSikTemp;
      memset((u8*)&s_radioHWInfoSikTemp, 0, sizeof(radio_hw_info_t));
      
      if ( ! _hardware_radio_sik_get_all_params(&s_radioHWInfoSikTemp, iSerialPort, iSpeed, NULL, 1) )
      {
         log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
         close(iSerialPort);
         continue;
      }

      s_radioHWInfoSikTemp.szMAC[MAX_MAC_LENGTH-1] = 0;
      if ( strlen(s_radioHWInfoSikTemp.szMAC) < MAX_MAC_LENGTH-3 )
      {
         strcat(s_radioHWInfoSikTemp.szMAC, "-");
         strcat(s_radioHWInfoSikTemp.szMAC, szSerialPort + (strlen(szSerialPort)-1));
      }
      log_line("[HW-RSK]: Found SiK Radio on port %s, baud: %d, MAC: [%s], firmware is up to date: %s",
         szDevName, iSpeed, s_radioHWInfoSikTemp.szMAC,
         (s_radioHWInfoSikTemp.uExtraFlags & RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD)?"no":"yes");

      char szTmp[256];
      szTmp[0] = 0;
      for( int k=0; k<MAX_RADIO_HW_PARAMS; k++ )
      {
         if ( 0 != szTmp[0] )
            strcat(szTmp, ", ");

         char szB[32];
         sprintf(szB, "[%d]=%u", k, s_radioHWInfoSikTemp.uHardwareParamsList[k]);
         strcat(szTmp, szB);
      }
      log_line("[HW-RSK]: SiK current parameters: %s", szTmp);

      log_line("[HW-RSK] SiK current read params: Air Speed: %u, ECC/LBT/MCSTR: %u/%u/%u",
         s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED],
         s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_ECC],
         s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_LBT],
         s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_MCSTR]);

      // Found SiK device that answered to AT commands mode.
      // Populate info about device
      
      log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
      close(iSerialPort);

      s_radioHWInfoSikTemp.iRadioType = RADIO_TYPE_SIK;
      s_radioHWInfoSikTemp.iRadioDriver = RADIO_HW_DRIVER_SERIAL_SIK;
      s_radioHWInfoSikTemp.iCardModel = CARD_MODEL_SIK_RADIO;
      s_radioHWInfoSikTemp.isSerialRadio = 1;
      s_radioHWInfoSikTemp.isConfigurable = 1;
      s_radioHWInfoSikTemp.isSupported = 1;
      s_radioHWInfoSikTemp.isHighCapacityInterface = 0;
      s_radioHWInfoSikTemp.isEnabled = 1;
      s_radioHWInfoSikTemp.isTxCapable = 1;
      
      strcpy(s_radioHWInfoSikTemp.szName, "SiK Radio");
      strcpy(s_radioHWInfoSikTemp.szUSBPort, "T");
      strcpy(s_radioHWInfoSikTemp.szDriver, szDevName);
      sprintf(s_radioHWInfoSikTemp.szProductId, "%u", s_radioHWInfoSikTemp.uHardwareParamsList[0]);
      
      s_radioHWInfoSikTemp.uCurrentFrequencyKhz = s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN];
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] >= 400000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] <= 460000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_433;

      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] >= 800000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] <= 890000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_868;

      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] >= 891000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] <= 950000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_915;


      // Mark the serial port as used for a hardware radio interface

      log_line("[HW-RSK]: Mark serial device [%s] as used for a radio interface.", szDevName);
      
      for( int k=0; k<hardware_serial_get_ports_count(); k++ )
      {
         hw_serial_port_info_t* pSerial = hardware_get_serial_port_info(k);
         if ( NULL == pSerial )
            continue;
         if ( 0 == strcmp(pSerial->szPortDeviceName, szDevName) )
         {
            pSerial->iPortUsage = SERIAL_PORT_USAGE_SIK_RADIO;
            pSerial->lPortSpeed = iSpeed;
            pSerial->iSupported = 1;
            hardware_serial_save_configuration();
            break;
         }
      }
      return &s_radioHWInfoSikTemp;
   }

   return NULL; 
}

// Returns number of bytes in response

int hardware_radio_sik_send_command(int iSerialPortFile, const char* szCommand, u8* bufferResponse, int iMaxResponseLength)
{
   if ( NULL != bufferResponse )
      bufferResponse[0] = 0;

   if ( iSerialPortFile <= 0 || szCommand == NULL || 0 == szCommand[0] )
   {
      log_softerror_and_alarm("[HW-RSK] Tried to send SiK command to invalid serial port file handle: %d, command: [%s]", iSerialPortFile, (szCommand == NULL)?"NULL":szCommand);
      return 0;
   }

   if ( NULL == bufferResponse || iMaxResponseLength <= 2 )
   {
      log_softerror_and_alarm("[HW-RSK] Tried to send command with no response buffer.");
      return 0;
   }

   int iSent = hardware_serial_send_sik_command(iSerialPortFile, szCommand);
   if ( 0 == iSent )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to send SiK command to serial port.");
      return 0;
   }
   
   log_line("[HW-RSK] Sent SiK radio command: [%s] (%d bytes)...", szCommand, iSent);

   if ( 0 == strcmp(szCommand, "ATZ") )
   {
      strcpy((char*)bufferResponse, "No response required to ATZ command");
      return 1;
   }

   u8 buff[256];
   int iBufferSize = 255;
   int iLines = 2;
   if ( 0 == strcmp(szCommand, "ATO") )
      iLines = 1;

   int iLen = hardware_serial_wait_sik_response(iSerialPortFile, 2000, iLines, buff, &iBufferSize);
   if ( iLen <= 0 )
   {
      log_softerror_and_alarm("[HW-RSK] No response from SiK radio, error code: %d", iLen);
      return 0;
   }
   if ( iLen <= 2 )
   {
      log_softerror_and_alarm("[HW-RSK] Received invalid SiK radio response, length: %d bytes", iLen);
      return 0;
   }

   // Remove trailing \r\n
   for( int i=0; i<6; i++ )
   {
      if ( (iLen > 0) && (buff[iLen-1] == 10 || buff[iLen-1] == 13) )
      {
         buff[iLen-1] = 0;
         iLen--;
      }
   }

   // Remove prefix \r\n
   for( int i=0; i<6; i++ )
   {
      if ( (buff[0] == 10 || buff[0] == 13) )
      {
         for( int k=0; k<iLen; k++ )
            buff[k] = buff[k+1];
         buff[iLen-1] = 0;
         iLen--;
      }
   }

   // Keep only last line from response
   
   for( int i=iLen-1; i>=0; i-- )
   {
      if ( buff[i] == 10 || buff[i] == 13 )
      {
          for( int k=0; k<iLen-i-1; k++ )
             buff[k] = buff[i+k+1];
          iLen = iLen-i-1;
          buff[iLen] = 0;
          break;
      }
   }
   
   if ( iLen >= iMaxResponseLength )
      iLen = iMaxResponseLength-1;
   memcpy(bufferResponse, buff, iLen);
   bufferResponse[iLen] = 0;

   log_line("[HW-RSK] Received Sik Command [%s] response: [%s], %d bytes", szCommand, (char*)bufferResponse, iLen);
   return iLen;
}

int hardware_radio_sik_get_real_serial_baudrate(int iSiKBaudRate)
{
   if ( iSiKBaudRate == 1 )
      return 1200;
   if ( iSiKBaudRate == 2 )
      return 2400;
   if ( iSiKBaudRate == 4 )
      return 4800;
   if ( iSiKBaudRate == 8 )
      return 8000;
   if ( iSiKBaudRate == 9 )
      return 9600;
   if ( iSiKBaudRate == 16 )
      return 16000;
   if ( iSiKBaudRate == 19 )
      return 19200;
   if ( iSiKBaudRate == 24 )
      return 24000;
   if ( iSiKBaudRate == 32 )
      return 32000;
   if ( iSiKBaudRate == 38 )
      return 38400;
   if ( iSiKBaudRate == 48 )
      return 48000;
   if ( iSiKBaudRate == 57 )
      return 57600;
   if ( iSiKBaudRate == 64 )
      return 64000;
   if ( iSiKBaudRate == 96 )
      return 96000;
   
   if ( iSiKBaudRate == 111 )
      return 111200;
   if ( iSiKBaudRate == 115 )
      return 115200;

   if ( iSiKBaudRate == 128 )
      return 128000;
 
   if ( iSiKBaudRate == 250 )
      return 250000;
   
   return 57600;
}

int hardware_radio_sik_get_encoded_serial_baudrate(int iRealSerialBaudRate)
{
   if ( iRealSerialBaudRate <= 1200 )
      return 1;
   if ( iRealSerialBaudRate <= 2400 )
      return 2;
   if ( iRealSerialBaudRate <= 4800 )
      return 4;
   if ( iRealSerialBaudRate <= 8000 )
      return 8;
   if ( iRealSerialBaudRate <= 9600 )
      return 9;
   if ( iRealSerialBaudRate <= 16000 )
      return 16;
   if ( iRealSerialBaudRate <= 19200 )
      return 19;
   if ( iRealSerialBaudRate <= 24000 )
      return 24;
   if ( iRealSerialBaudRate <= 32000 )
      return 32;
   if ( iRealSerialBaudRate <= 38400 )
      return 38;
   if ( iRealSerialBaudRate <= 48000 )
      return 48;
   if ( iRealSerialBaudRate <= 57600 )
      return 57;
   if ( iRealSerialBaudRate <= 64000 )
      return 64;
   if ( iRealSerialBaudRate <= 96000 )
      return 96;
   if ( iRealSerialBaudRate <= 111200 )
      return 111;
   if ( iRealSerialBaudRate <= 115200 )
      return 115;
   if ( iRealSerialBaudRate <= 128000 )
      return 128;
   if ( iRealSerialBaudRate <= 250000 )
      return 250;

   return 57;
}


int hardware_radio_sik_get_real_air_baudrate(int iSiKAirBaudRate)
{
   return iSiKAirBaudRate*1000;
}

int hardware_radio_sik_get_encoded_air_baudrate(int iRealAirBaudRate)
{
   return iRealAirBaudRate/1000;
}

static void * _thread_sik_get_all_params_async(void *argument)
{
   int* piResult = (int*)argument;
   hw_log_current_thread_attributes("sik get all params");

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(s_iGetSiKConfigAsyncInterfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      *piResult = -1;
      return NULL;
   }

   log_line("[HardwareSiKConfig] Started thread to get config async.");
         
   int iResult = hardware_radio_sik_get_all_params(pRadioHWInfo, NULL);
   if ( 1 != iResult )
      iResult = hardware_radio_sik_get_all_params(pRadioHWInfo, NULL);
   
   if ( iResult != 1 )
      *piResult = -1;
   else
      *piResult = 1;
            
   log_line("[HardwareSiKConfig] Finished thread to get config async.");

   s_iGetSiKConfigAsyncRunning = 0;
   return NULL;
}

int hardware_radio_sik_get_all_params_async(int iRadioInterfaceIndex, int* piFinished)
{
   if ( s_iGetSiKConfigAsyncRunning )
      return 0;

   if ( NULL == piFinished )
      return 0;

   if ( 0 != pthread_create(&s_pThreadGetSiKConfigAsync, NULL, &_thread_sik_get_all_params_async, (void*)piFinished) )
   {
      log_error_and_alarm("[HardwareSiKConfig] Failed to create thread.");
      *piFinished = -1;
      return 0;
   }
   pthread_detach(s_pThreadGetSiKConfigAsync);
   s_iGetSiKConfigAsyncInterfaceIndex = iRadioInterfaceIndex;
   s_iGetSiKConfigAsyncRunning = 1;
   return 1;
}

int hardware_radio_sik_get_all_params(radio_hw_info_t* pRadioInfo, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
  
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HW-RSK] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HW-RSK] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   if ( ! _hardware_radio_sik_get_all_params(pRadioInfo, iSerialPort, pSerialPort->lPortSpeed, pProcessStats, 0) )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to get all params for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
      close(iSerialPort);
      return 0;
   }

   char szTmp[256];
   szTmp[0] = 0;
   for( int i=0; i<16; i++ )
   {
      if ( 0 != szTmp[0] )
         strcat(szTmp, ", ");

      char szB[32];
      sprintf(szB, "[%d]=%u", i, pRadioInfo->uHardwareParamsList[i]);
      strcat(szTmp, szB);
   }
   log_line("[HW-RSK]: SiK interface [%s] current parameters: %s", pRadioInfo->szDriver, szTmp);
   log_line("[HW-RSK]: SiK interface [%s] current params: NetId: %u, ECC/LBT/MCSTR: %u/%u/%u",
      pRadioInfo->szDriver,
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_NETID],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR]);
   log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
   close(iSerialPort);
   return 1;
}

int hardware_radio_sik_save_settings_to_flash(int iSerialPortFile)
{
   if ( iSerialPortFile <= 0 )
      return 0;

   u8 bufferResponse[256];

   if ( hardware_radio_sik_send_command(iSerialPortFile, "AT&W", bufferResponse, 255) )
      log_line("[HW-RSK]: Wrote SiK parameters to flash, response: [%s]", bufferResponse);
   else
      log_softerror_and_alarm("[HW-RSK]: Failed to write SiK parameters to flash, response: [%s]", bufferResponse);

   if ( hardware_radio_sik_send_command(iSerialPortFile, "ATZ", bufferResponse, 255) )
      log_line("[HW-RSK]: Restart SiK radio, response: [%s]", bufferResponse);
   else
      log_softerror_and_alarm("[HW-RSK]: Failed to restart SiK radio, response: [%s]", bufferResponse);

   return 1;
}

int _hardware_radio_sik_set_parameter( int iSerialPortFile, u32 uParamIndex, u32 uParamValue )
{
   if ( iSerialPortFile <= 0 )
      return 0;
   if ( uParamIndex >= 16 )
      return 0;

   char szComm[32];
   u8 bufferResponse[256];

   sprintf(szComm, "ATS%u=%u", uParamIndex, uParamValue);

   int iLen = hardware_radio_sik_send_command(iSerialPortFile, szComm, bufferResponse, 255);
   if ( iLen >= 2 )
   {
      if ( NULL != strstr((char*)bufferResponse, "OK"))
         return 1;
      if ( NULL != strstr((char*)bufferResponse, "Ok"))
         return 1;
      if ( NULL != strstr((char*)bufferResponse, "oK"))
         return 1;
      if ( NULL != strstr((char*)bufferResponse, "ok"))
         return 1;
   }
   return 0;
}

int hardware_radio_sik_set_serial_speed(radio_hw_info_t* pRadioInfo, int iSerialSpeedToConnect, int iNewSerialSpeed, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LOCAL_SPEED] == (u32)hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed) )
   {
      log_line("[HW-RSK]: SiK Radio serial speed is unchanged. SiK radio interface %d is already at serial speed %d bps.",
         pRadioInfo->phy_index+1, iNewSerialSpeed );
      return 1;
   }

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
   
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HW-RSK] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, iSerialSpeedToConnect);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HW-RSK] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   // Enter AT mode
   if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, iSerialSpeedToConnect, pProcessStats) )
   {
      log_softerror_and_alarm("[HW-RSK] Failed to enter SiK radio into AT command mode.");
      log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
      close(iSerialPort);
      return 0;
   }
   
   // Send params

   if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_LOCAL_SPEED, hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed)) )
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LOCAL_SPEED] = hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed);
   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   // Exit AT command mode
   
   hardware_radio_sik_save_settings_to_flash(iSerialPort);

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
   close(iSerialPort);

   log_line("[HW-RSK]: Did set SiK radio interface %d to serial speed %d bps;",
      pRadioInfo->phy_index+1, iNewSerialSpeed );
   return 1;
}

int hardware_radio_sik_set_frequency(radio_hw_info_t* pRadioInfo, u32 uFrequencyKhz, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;
   
   if ( uFrequencyKhz == 0 )
      uFrequencyKhz = pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN];
   
   int iRes = hardware_radio_sik_set_params(pRadioInfo, 
       uFrequencyKhz,
       DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS, DEFAULT_RADIO_SIK_NETID,
       hardware_radio_sik_get_real_air_baudrate(pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED]),
       pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_TXPOWER], 
       pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC],
       pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT],
       pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR],
       pProcessStats);
   return iRes;
}


int hardware_radio_sik_set_tx_power(radio_hw_info_t* pRadioInfo, u32 uTxPower, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;
   if ( (uTxPower == 0) || (uTxPower > 30) )
      return 0;
   int iRes = hardware_radio_sik_set_params(pRadioInfo,
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX]-pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_CHANNELS],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_NETID],
      hardware_radio_sik_get_real_air_baudrate(pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED]),
      uTxPower, 
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT],
      pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR], pProcessStats);
   return iRes;
}

int hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(radio_hw_info_t* pRadioInfo, u32 uFrequency, u32 uTxPower, u32 uAirSpeed, u32 uECC, u32 uLBT, u32 uMCSTR, shared_mem_process_stats* pProcessStats)
{
    if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;
   
   if ( uFrequency == 0 )
      uFrequency = pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN];
   else if ( uFrequency < 10000 )
      uFrequency *= 1000;

   if ( (uTxPower == 0) || (uTxPower > 30) )
      uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
   
   int iRes = hardware_radio_sik_set_params(pRadioInfo,
      uFrequency,
      DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS, DEFAULT_RADIO_SIK_NETID,
      uAirSpeed,
      uTxPower, 
      uECC, uLBT, uMCSTR, pProcessStats);
   return iRes;
}

int hardware_radio_sik_set_params(radio_hw_info_t* pRadioInfo, u32 uFrequencyKhz, u32 uFreqSpread, u32 uChannels, u32 uNetId, u32 uAirSpeed, u32 uTxPower, u32 uECC, u32 uLBT, u32 uMCSTR, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   if ( uLBT != 0 )
      uLBT = 50;
   int iAnyChanged = 0;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED] != (u32)hardware_radio_sik_get_encoded_air_baudrate(uAirSpeed) )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_NETID] != uNetId )
      iAnyChanged = 1;
   if ( (uTxPower > 0) && (uTxPower <= 30) )
   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_TXPOWER] != uTxPower )
      iAnyChanged = 1;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC] != uECC )
      iAnyChanged = 1;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_CHANNELS] != uChannels )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] != uFrequencyKhz )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] != (uFrequencyKhz + uFreqSpread) )
      iAnyChanged = 1;
   
   // Duty Cycle
   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_DUTYCYCLE] != 100 )
      iAnyChanged = 1;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT] != uLBT )
      iAnyChanged = 1;

   if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR] != uMCSTR )
       iAnyChanged = 1;

   // Max Window
   if ( pRadioInfo->uHardwareParamsList[15] != 50 )
      iAnyChanged = 1;
   
   if ( ! iAnyChanged )
   {
      log_line("[HW-RSK]: SiK Radio all params are unchanged. Set SiK radio interface %d to frequency %s, channels: %u, freq spread: %.1f Mhz, NetId: %u, AirSpeed: %u bps, ECC/LBT/MCSTR: %u/%u/%u, param[6]=%u",
         pRadioInfo->phy_index+1, str_format_frequency(uFrequencyKhz),
         uChannels, (float)uFreqSpread/1000.0, uNetId, uAirSpeed, uECC, uLBT, uMCSTR, pRadioInfo->uHardwareParamsList[6] );
      return 1;
   }

   //hardware_radio_sik_get_all_params(pRadioInfo, pProcessStats);

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
   
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HW-RSK] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HW-RSK] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   int iRetryCounter = 0;
   int iFailed = 1;

   while ( iFailed && (iRetryCounter<3) )
   {
      iRetryCounter++;

      // Enter AT mode
      if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, pSerialPort->lPortSpeed, pProcessStats) )
      if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, pSerialPort->lPortSpeed, pProcessStats) )
      {
         log_softerror_and_alarm("[HW-RSK] Failed to enter SiK radio into AT command mode.");
         log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
         close(iSerialPort);
         return 0;
      }

      // Send params
      
      iFailed = 0;

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED] != (u32)hardware_radio_sik_get_encoded_air_baudrate(uAirSpeed) )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_AIRSPEED, hardware_radio_sik_get_encoded_air_baudrate(uAirSpeed)) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_AIRSPEED] = hardware_radio_sik_get_encoded_air_baudrate(uAirSpeed);
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_NETID] != uNetId )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_NETID, uNetId) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_NETID] = uNetId;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( (uTxPower > 0) && (uTxPower <= 30) )
      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_TXPOWER] != uTxPower )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_TXPOWER, uTxPower) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_TXPOWER] = uTxPower;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC] != uECC )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_ECC, uECC) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_ECC] = uECC;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }
      
      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] != uFrequencyKhz )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_FREQ_MIN, uFrequencyKhz) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MIN] = uFrequencyKhz;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] != uFrequencyKhz + uFreqSpread )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_FREQ_MAX, uFrequencyKhz + uFreqSpread ) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_FREQ_MAX] = uFrequencyKhz + uFreqSpread;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_CHANNELS] != uChannels )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_CHANNELS, uChannels) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_CHANNELS] = uChannels;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      // Duty cycle to 100 % ( percentage of time allowed to transmit )

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_DUTYCYCLE] != 100 )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_DUTYCYCLE, 100 ) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_DUTYCYCLE] = 100;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT] != uLBT )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_LBT, uLBT) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_LBT] = uLBT;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }

      if ( pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR] != uMCSTR )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, SIK_PARAM_INDEX_MCSTR, uMCSTR) )
            pRadioInfo->uHardwareParamsList[SIK_PARAM_INDEX_MCSTR] = uMCSTR;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }
      // Max Window

      if ( pRadioInfo->uHardwareParamsList[15] != 50 )
      {
         if ( _hardware_radio_sik_set_parameter(iSerialPort, 15, 50 ) )
            pRadioInfo->uHardwareParamsList[15] = 50;
         else
            iFailed = 1;
         if ( NULL != pProcessStats )
            pProcessStats->lastActiveTime = get_current_timestamp_ms();
      }
      if ( iFailed && (iRetryCounter<3) )
         log_line("Will retry to set SiK parameters...");
   }

   // Exit AT command mode
   hardware_radio_sik_save_settings_to_flash(iSerialPort);

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   log_line("[HW-RSK] Closed serial port fd %d", iSerialPort);
   close(iSerialPort);

   if ( iFailed )
   {
      log_softerror_and_alarm("[HW-RSK]: Failed to set SiK radio interface %d to frequency %s, channels: %u, freq spread: %.1f Mhz, NetId: %u, AirSpeed: %u bps, ECC/LBT/MCSTR: %u/%u/%u",
      pRadioInfo->phy_index+1, str_format_frequency(uFrequencyKhz),
      uChannels, (float)uFreqSpread/1000.0, uNetId, uAirSpeed,
      uECC, uLBT, uMCSTR );
      return 0;
   }

   log_line("[HW-RSK]: Did set SiK radio interface %d to frequency %s, channels: %u, freq spread: %.1f Mhz, NetId: %u, AirSpeed: %u bps, ECC/LBT/MCSTR: %u/%u/%u",
      pRadioInfo->phy_index+1, str_format_frequency(uFrequencyKhz),
      uChannels, (float)uFreqSpread/1000.0, uNetId, uAirSpeed,
      uECC, uLBT, uMCSTR );

   hardware_radio_sik_save_configuration();
   hardware_save_radio_info();
   return 1;
}

int hardware_radio_sik_open_for_read_write(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RSK] Open: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RSK] Open: Tried to open hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
  
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HW-RSK] Open: Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPortFD = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPortFD <= 0 )
   {
      log_error_and_alarm("[HW-RSK] Open: Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }
   pRadioInfo->openedForWrite = 1;
   pRadioInfo->openedForRead = 1;
   pRadioInfo->runtimeInterfaceInfoRx.selectable_fd = iSerialPortFD;
   pRadioInfo->runtimeInterfaceInfoTx.selectable_fd = iSerialPortFD;

   log_line("[HW-RSK] Opened SiK radio interface %d for read/write. fd=%d", iHWRadioInterfaceIndex+1, iSerialPortFD);
   return 1;
}

int hardware_radio_sik_close(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RSK] Close: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RSK] Close: Tried to close hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   if ( pRadioInfo->openedForWrite || pRadioInfo->openedForRead )
   {
      if ( pRadioInfo->runtimeInterfaceInfoRx.selectable_fd > 0 )
         close(pRadioInfo->runtimeInterfaceInfoRx.selectable_fd);
      else if ( pRadioInfo->runtimeInterfaceInfoTx.selectable_fd > 0 )
         close(pRadioInfo->runtimeInterfaceInfoTx.selectable_fd);
      pRadioInfo->runtimeInterfaceInfoRx.selectable_fd = -1;
      pRadioInfo->runtimeInterfaceInfoTx.selectable_fd = -1;
      pRadioInfo->openedForWrite = 0;
      pRadioInfo->openedForRead = 0;
   }
   return 1;
}

int hardware_radio_sik_write_packet(int iHWRadioInterfaceIndex, u8* pData, int iLength)
{
   if ( (NULL == pData) || (iLength <= 0) )
   {
      log_softerror_and_alarm("[HW-RSK] Write: Invalid parameters: NULL buffer or 0 length.");
      return 0;
   }

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RSK] Write: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RSK] Write: Tried to write to hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   if ( (! pRadioInfo->openedForWrite) || (pRadioInfo->runtimeInterfaceInfoTx.selectable_fd <= 0) )
   {
      log_softerror_and_alarm("[HW-RSK] Write: Interface is not opened for write.");
      return 0;
   }

   int iRes = write(pRadioInfo->runtimeInterfaceInfoTx.selectable_fd, pData, iLength);
   if ( iRes != iLength )
   {
      log_softerror_and_alarm("[HW-RSK] Write: Failed to write. Written %d bytes of %d bytes.", iRes, iLength);
      return 0;
   }

   //log_line("[HW-RSK] Write: wrote %d bytes to SiK radio.", iLength);
   return 1;
}

