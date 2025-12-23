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
#include "../base/shared_mem.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem_i2c.h"
#include "ruby_i2c.h"

#include <time.h>
#include <sys/resource.h>
#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RASPBERRY)
#include <wiringPiI2C.h>
#endif
#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RADXA)
#include "../base/wiringPiI2C_radxa.h"
#endif
#include <math.h>


bool g_bQuit = false;
u32 g_TimeLastReloadCheck = 0;
u32 g_TimeLastINARead = 0;
u32 g_TimeLastRCInRead = 0;
u32 g_TimeLastRCInFrameChange = 0;
u32 g_TimeLastRCInReadFull = 0;

u32 g_SleepTime = 50;

bool g_bHasINA = false;
int g_nINAAddress = 0;
int g_nINAFd = 0;
int g_nFileRCIn = 0;
int g_nFilePicoExtender = 0;

int g_nListFilesExternalDevices[MAX_I2C_DEVICES];
int g_nCountExternalDevices = 0;
bool g_bHasExternalRotaryDevice = false;
int g_iHasExternalRCInputDevice = 0; // Count of external devices that have RC input flag
int g_iReadRCInConsecutiveFailCount = 0;

t_i2c_device_settings* g_pDeviceInfoINA = NULL;
t_i2c_device_settings* g_pDeviceInfoRCIn = NULL;
t_i2c_device_settings* g_pDeviceInfoPicoExtender = NULL;
t_i2c_device_settings* g_pListExternalDevices[MAX_I2C_DEVICES];

u32 g_uListExternalDevicesFlags[MAX_I2C_DEVICES];
bool g_bListExternalDevicesSetupCorrectly[MAX_I2C_DEVICES];

t_shared_mem_i2c_current* g_pSMCurrent = NULL;
t_shared_mem_i2c_controller_rc_in* g_pSMRCIn = NULL;
t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents = NULL;

u16 s_lastRCReadVals[I2C_DEVICE_PARAM_MAX_CHANNELS];
u8 s_uLastFrameNumber = 0;

void close_files()
{
   if ( g_nINAFd > 0 )
      close(g_nINAFd);
   g_nINAFd = 0;

   if ( g_nFileRCIn > 0 )
      close(g_nFileRCIn);
   g_nFileRCIn = 0;

   if ( g_nFilePicoExtender > 0 )
      close(g_nFilePicoExtender);
   g_nFilePicoExtender = 0;

   for( int i=0; i<MAX_I2C_DEVICES; i++ )
   {
      if ( g_nListFilesExternalDevices[i] > 0 )
      {
         close(g_nListFilesExternalDevices[i]);
         g_nListFilesExternalDevices[i] = -1;
      }
      g_bListExternalDevicesSetupCorrectly[i] = false;
   }
}

void _init_INA()
{
#ifdef HW_CAPABILITY_I2C
   if ( NULL != g_pSMCurrent )
   {
      g_pSMCurrent->lastSetTime = MAX_U32;
      g_pSMCurrent->voltage = MAX_U32;
      g_pSMCurrent->current = MAX_U32;
      g_pSMCurrent->uParam = 0;
   }

   g_bHasINA = false;
   g_nINAAddress = 0;
   if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_INA219_1) )
   {
      g_bHasINA = true;
      g_nINAAddress = I2C_DEVICE_ADDRESS_INA219_1;
   }
   if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_INA219_2) )
   {
      g_bHasINA = true;
      g_nINAAddress = I2C_DEVICE_ADDRESS_INA219_2;
   }

   if ( g_bHasINA )
   {
      g_pDeviceInfoINA = hardware_i2c_get_device_settings((u8)g_nINAAddress);
      if ( NULL == g_pDeviceInfoINA || (!g_pDeviceInfoINA->bEnabled) )
         g_bHasINA = false;
   }

   if ( g_bHasINA )
   {
      if ( NULL != g_pSMCurrent )
         g_pSMCurrent->uParam = g_pDeviceInfoINA->uParams[0];
      g_nINAFd = wiringPiI2CSetup(g_nINAAddress);
      if ( g_nINAFd > 0 )
      {
         log_line("Write INA219 calibration");
         u32 val = 4096;
         wiringPiI2CWriteReg16 (g_nINAFd, 5, val); 

         val = (0x2000) | (0x1800) | (0x0180) | (0x0018) | (0x07);
         wiringPiI2CWriteReg16 (g_nINAFd, 0, val);
      }
   }
#endif
}

bool _setup_external_device(int iIndex)
{
#ifdef HW_CAPABILITY_I2C
   u8 bufferOut[32];
   u8 bufferIn[64];

   if ( g_nListFilesExternalDevices[iIndex] <= 0 )
      return false;
   if ( g_pListExternalDevices[iIndex] == NULL )
      return false;

   // Get device capabilities flags

   bool bSucceeded = false;
   for( int iRetry=0; iRetry<10; iRetry++ )
   {
      bufferOut[0] = I2C_COMMAND_START_FLAG;
      bufferOut[1] = I2C_COMMAND_ID_GET_FLAGS;
      bufferOut[2] = base_compute_crc8(bufferOut,2);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[0]);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[1]);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[2]);
      g_uListExternalDevicesFlags[iIndex] = 0;
      int res1 = wiringPiI2CRead(g_nListFilesExternalDevices[iIndex]);
      int res2 = wiringPiI2CRead(g_nListFilesExternalDevices[iIndex]);
      int res3 = wiringPiI2CRead(g_nListFilesExternalDevices[iIndex]);
      if ( res1 < 0 || res2 < 0 || res3 < 0 )
      {
         log_softerror_and_alarm("Failed to get I2C external device flags at address 0x%02X (external module), got: %d, %d, %d. Ignoring device.", g_pListExternalDevices[iIndex]->nI2CAddress, res1, res2, res3);
         continue;
      }
      log_line("Got I2C external device (0x%02X) flags: %d, %d", g_pListExternalDevices[iIndex]->nI2CAddress, res1, res2);

      bufferIn[0] = res1;
      bufferIn[1] = res2;
      bufferIn[2] = base_compute_crc8(bufferIn,2);

      if ( bufferIn[2] != res3 )
      {
         log_softerror_and_alarm("Received incorrect CRC on I2C command flags response.");
         continue;
      }
      g_uListExternalDevicesFlags[iIndex] = ((u32)res1) | (((u32)res2)<<8);
        
      log_line("Got I2C external device 0x%02X flags: %u. ", g_pListExternalDevices[iIndex]->nI2CAddress, g_uListExternalDevicesFlags[iIndex]);
      log_line("0x%02X supported flags: rotary: %s, buttons: %s", g_pListExternalDevices[iIndex]->nI2CAddress, (g_uListExternalDevicesFlags[iIndex] & I2C_CAPABILITY_FLAG_ROTARY)?"yes":"no", (g_uListExternalDevicesFlags[iIndex] & I2C_CAPABILITY_FLAG_BUTTONS)?"yes":"no");
      bSucceeded = true;
      break;
   }
   if ( ! bSucceeded )
   {
      log_softerror_and_alarm("Failed to get I2C device capabilities even after retries.");
      return false;
   }


   if ( g_uListExternalDevicesFlags[iIndex] & I2C_CAPABILITY_FLAG_ROTARY )
   {
      g_bHasExternalRotaryDevice = true;
      if ( NULL != g_pSMRotaryEncoderButtonsEvents )
         g_pSMRotaryEncoderButtonsEvents->uHasRotaryEncoder = 1;
   }

   if ( g_uListExternalDevicesFlags[iIndex] & I2C_CAPABILITY_FLAG_ROTARY2 )
   {
      g_bHasExternalRotaryDevice = true;
      if ( NULL != g_pSMRotaryEncoderButtonsEvents )
         g_pSMRotaryEncoderButtonsEvents->uHasSecondaryRotaryEncoder = 1;
   }
   // Set device RC flags if it supports RC

   if ( g_uListExternalDevicesFlags[iIndex] & I2C_CAPABILITY_FLAG_RC_INPUT )
   {
      bSucceeded = false;
      for( int iRetry=0; iRetry<10; iRetry++ )
      {
         bufferOut[0] = I2C_COMMAND_START_FLAG;
         bufferOut[1] = I2C_COMMAND_ID_SET_RC_INPUT_FLAGS;
         bufferOut[2] = 0;
         if ( g_pListExternalDevices[iIndex]->uParams[0] == 1 )
            bufferOut[2] |= I2C_COMMAND_RC_FLAG_SBUS;
         if ( g_pListExternalDevices[iIndex]->uParams[0] == 2 )
            bufferOut[2] |= I2C_COMMAND_RC_FLAG_IBUS;
         if ( g_pListExternalDevices[iIndex]->uParams[1] )
            bufferOut[2] |= I2C_COMMAND_RC_FLAG_INVERT_UART;

         bufferOut[3] = base_compute_crc8(bufferOut,3);

         wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[0]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[1]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[2]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[iIndex], bufferOut[3]);
         int res1 = wiringPiI2CRead(g_nListFilesExternalDevices[iIndex]);
         int res2 = wiringPiI2CRead(g_nListFilesExternalDevices[iIndex]);
         if ( res1 < 0 || res2 < 0 )
         {
            log_softerror_and_alarm("Failed to get response to RC setup from I2C external device at address 0x%02X (external module), got: %d, %d.", g_pListExternalDevices[iIndex]->nI2CAddress, res1, res2);
            continue;
         }
         else
         {
         	  bufferIn[0] = res1;
            bufferIn[1] = base_compute_crc8(bufferIn,1);

            if ( bufferIn[1] != res2 )
            {
               log_softerror_and_alarm("Received incorrect CRC on I2C command set RC flags response.");
               continue;
            }
            else
            {
               if ( res1 != 0 )
               {
                  log_softerror_and_alarm("Response to RC setup from I2C external device at address 0x%02X (external module) was: failed.", g_pListExternalDevices[iIndex]->nI2CAddress);
                  continue;
               }
            }
         }
         bSucceeded = true;
         break;
      }
      if ( bSucceeded )
      {
         g_iHasExternalRCInputDevice++;
         log_line("Set up device for RC input succeeded.");
      }
      else
         log_softerror_and_alarm("Failed to setup the device for RC Input. Ignoring device as RC input.");
   }
	  g_bListExternalDevicesSetupCorrectly[iIndex] = true;
	  return true;
#else
   return false;
#endif
}

void _init_external_device(u8 i2cAddress)
{
   log_line("Checking device id 0x%02X...", i2cAddress);
   g_pListExternalDevices[g_nCountExternalDevices] = hardware_i2c_get_device_settings(i2cAddress);

   if ( NULL == g_pListExternalDevices[g_nCountExternalDevices] )
      return;

   if ( ! g_pListExternalDevices[g_nCountExternalDevices]->bEnabled )
      return;

   #ifdef HW_CAPABILITY_I2C

   g_nListFilesExternalDevices[g_nCountExternalDevices] = wiringPiI2CSetup(i2cAddress);
   if ( g_nListFilesExternalDevices[g_nCountExternalDevices] <= 0 )
   {
      log_softerror_and_alarm("Failed to open I2C address 0x%02X to external module.", i2cAddress);
      return;
   }
   log_line("Opened I2C device at address 0x%02X (external module).", i2cAddress);

   _setup_external_device(g_nCountExternalDevices);
 
   g_nCountExternalDevices++;
   
   #endif
}

void _init_external_devices()
{
   log_line("Searching and init external I2C devices...");
   g_nCountExternalDevices = 0;
   g_bHasExternalRotaryDevice = false;
   g_iHasExternalRCInputDevice = 0;
 
   if ( NULL != g_pSMRotaryEncoderButtonsEvents )
   {
      g_pSMRotaryEncoderButtonsEvents->uHasRotaryEncoder = 0;
      g_pSMRotaryEncoderButtonsEvents->uHasSecondaryRotaryEncoder = 0;
   }

   if ( ! hardware_i2c_has_external_extenders() )
   {
      for( int i=0; i<MAX_I2C_DEVICES; i++ )
      {
         g_pListExternalDevices[i] = NULL;
         g_nListFilesExternalDevices[i] = -1;
         g_uListExternalDevicesFlags[i] = 0;
      }
      log_line("No external I2C devices.");
      return;
   }

   for( u8 addr = I2C_DEVICE_MIN_ADDRESS_RANGE; addr <= I2C_DEVICE_MAX_ADDRESS_RANGE; addr++ )
   {
      if ( g_nCountExternalDevices >= MAX_I2C_DEVICES )
         break;
      if ( ! hardware_i2c_has_device_id(addr) )
         continue;
      _init_external_device(addr);
   }
   log_line("Done searching and init external I2C devices. Found %d external configurable I2C devices.", g_nCountExternalDevices);

}

void load_settings()
{
   g_SleepTime = 100;

   hardware_i2c_load_device_settings();
   load_ControllerSettings();

   _init_INA();
   _init_external_devices();

   if ( g_iHasExternalRCInputDevice > 0 )
   if ( g_SleepTime > 20 )
      g_SleepTime = 20;

#ifdef HW_CAPABILITY_I2C
 
   if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN) )
   {
      g_pDeviceInfoRCIn = hardware_i2c_get_device_settings(I2C_DEVICE_ADDRESS_PICO_RC_IN);
      if ( NULL == g_pDeviceInfoRCIn || (!g_pDeviceInfoRCIn->bEnabled) )
         g_pDeviceInfoRCIn = NULL;

      if ( NULL != g_pDeviceInfoRCIn )
      {
         g_nFileRCIn = wiringPiI2CSetup(I2C_DEVICE_ADDRESS_PICO_RC_IN);
         if ( g_nFileRCIn <= 0 )
            log_softerror_and_alarm("Failed to open I2C address 0x%02X to Pico RC In module.", I2C_DEVICE_ADDRESS_PICO_RC_IN);
         else
            log_line("Opened I2C device at address 0x%02X (Pico RC In module).", I2C_DEVICE_ADDRESS_PICO_RC_IN);
      }
      if ( g_SleepTime > 25 )
         g_SleepTime = 25;
   }

   if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER) )
   {
      g_pDeviceInfoPicoExtender = hardware_i2c_get_device_settings(I2C_DEVICE_ADDRESS_PICO_EXTENDER);
      if ( NULL == g_pDeviceInfoPicoExtender || (!g_pDeviceInfoPicoExtender->bEnabled) )
         g_pDeviceInfoPicoExtender = NULL;

      if ( NULL != g_pDeviceInfoPicoExtender )
      {
         g_nFilePicoExtender = wiringPiI2CSetup(I2C_DEVICE_ADDRESS_PICO_EXTENDER);
         if ( g_nFilePicoExtender <= 0 )
            log_softerror_and_alarm("Failed to open I2C address 0x%02X to Pico Extender module.", I2C_DEVICE_ADDRESS_PICO_EXTENDER);
         else
            log_line("Opened I2C device at address 0x%02X (Pico Extender module).", I2C_DEVICE_ADDRESS_PICO_EXTENDER);
      }
      if ( g_SleepTime > 20 )
         g_SleepTime = 20;
   }

   if ( g_nFileRCIn > 0 || g_nFilePicoExtender > 0 )
   {
      int file = g_nFileRCIn;
      t_i2c_device_settings* pSettings = g_pDeviceInfoRCIn;
      if ( g_nFilePicoExtender > 0 )
      {
         pSettings = g_pDeviceInfoPicoExtender;
         file = g_nFilePicoExtender;
      }

      log_line("Getting Pico Extender version...");
      int iVersion = wiringPiI2CReadReg8(file, I2C_DEVICE_COMMAND_ID_GET_VERSION);
      log_line("Got Pico Extender version: %d.%d", iVersion>>4, iVersion & 0x0F);

      if ( NULL != g_pSMRCIn )
         g_pSMRCIn->version = (u8)iVersion;

      if ( 0 == pSettings->uParams[0] )
         wiringPiI2CWriteReg8(file, I2C_DEVICE_COMMAND_ID_RC_IN_SET_STREAM_TYPE_SBUS, 0);
      else
         wiringPiI2CWriteReg8(file, I2C_DEVICE_COMMAND_ID_RC_IN_SET_STREAM_TYPE_IBUS, 0);

      if ( 0 == pSettings->uParams[1] )
         wiringPiI2CWriteReg8(file, I2C_DEVICE_COMMAND_ID_RC_IN_SET_UN_INVERTED, 0);
      else
         wiringPiI2CWriteReg8(file, I2C_DEVICE_COMMAND_ID_RC_IN_SET_INVERTED, 0);
   }
#endif
}

void checkReadINA()
{
#ifdef HW_CAPABILITY_I2C
   if (  g_TimeNow < g_TimeLastINARead )
      g_TimeLastINARead = g_TimeNow;

   if ( g_TimeNow < g_TimeLastINARead + 300 )
      return;

   g_TimeLastINARead = g_TimeNow;

   if ( 0 < g_nINAFd )
   if ( g_pDeviceInfoINA->uParams[0] == 0 || g_pDeviceInfoINA->uParams[0] == 2 )
   {
      u32 valV  = wiringPiI2CReadReg16(g_nINAFd, 2); 
      valV = revert_word(valV);
      valV = (valV>>3)*4;
      if ( NULL != g_pSMCurrent )
      {
         g_pSMCurrent->voltage = valV;
         g_pSMCurrent->lastSetTime = g_TimeNow;
      }
   }
   if ( 0 < g_nINAFd )
   if ( g_pDeviceInfoINA->uParams[0] == 1 || g_pDeviceInfoINA->uParams[0] == 2 )
   {
      u32 val = 4096;
      val = ((val>>8) & 0xFF) | ((val & 0xFF) << 8);
      wiringPiI2CWriteReg16 (g_nINAFd, 5, val); 

      u32 valC  = wiringPiI2CReadReg16(g_nINAFd, 4); 
      valC = revert_word(valC);
      if ( NULL != g_pSMCurrent )
      {
         g_pSMCurrent->current = valC;
         g_pSMCurrent->lastSetTime = g_TimeNow;
      }
   }
#endif
}

void _read_RCIn_OldMethod()
{
#ifdef HW_CAPABILITY_I2C
   if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN) )
   if ( g_nFileRCIn <= 0 )
   if ( NULL != g_pSMRCIn )
   {
      g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
      return;
   }

   if (	hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER) )
   if ( g_nFilePicoExtender <= 0 )
   if ( NULL != g_pSMRCIn )
   {
      g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
      return;
   }

   int file = g_nFileRCIn;
   if ( g_nFilePicoExtender > 0 )
      file = g_nFilePicoExtender;

   int iFrameNumber = wiringPiI2CReadReg8(file, I2C_DEVICE_COMMAND_ID_RC_IN_GET_FRAME_NUMBER);

   if ( iFrameNumber < 0 )
   {
      g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
      return;
   }

   s_uLastFrameNumber = (u8)iFrameNumber;

   if ( (u8)iFrameNumber == g_pSMRCIn->uFrameIndex )
   {
      if ( g_TimeNow > g_TimeLastRCInFrameChange + DEFAULT_RC_FAILSAFE_TIME )
      {
         g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
      }
      return;
   }

   g_pSMRCIn->uFlags |= RC_IN_FLAG_HAS_INPUT; // Has input
   g_TimeLastRCInFrameChange =  g_TimeNow;


   int chToRead = 10;
   if ( chToRead > I2C_DEVICE_PARAM_MAX_CHANNELS )
      chToRead = I2C_DEVICE_PARAM_MAX_CHANNELS;

   for( int i=0; i<chToRead; i++ )
   {
      s_lastRCReadVals[i] = wiringPiI2CReadReg16(file, I2C_DEVICE_COMMAND_ID_RC_IN_GET_CHANNEL+i);
      if ( NULL != g_pDeviceInfoPicoExtender && 0 == g_pDeviceInfoPicoExtender->uParams[0] ) // SBUS
         s_lastRCReadVals[i] = 1000 + 1000 * (((int)s_lastRCReadVals[i])-200) / 1600;
   }
   if ( g_TimeNow >= g_TimeLastRCInReadFull + 100 )
   {
      g_TimeLastRCInReadFull = g_TimeNow;
      for( int i=chToRead; i<I2C_DEVICE_PARAM_MAX_CHANNELS; i++ )
      {
         s_lastRCReadVals[i] = wiringPiI2CReadReg16(file, I2C_DEVICE_COMMAND_ID_RC_IN_GET_CHANNEL+i);
         if ( NULL != g_pDeviceInfoPicoExtender && 0 == g_pDeviceInfoPicoExtender->uParams[0] ) // SBUS
            s_lastRCReadVals[i] = 1000 + 1000 * (((int)s_lastRCReadVals[i])-200) / 1600;
      }
   }

   if ( NULL != g_pSMRCIn )
   {
      int nCh = I2C_DEVICE_PARAM_MAX_CHANNELS;
      if ( nCh > MAX_RC_CHANNELS )
         nCh = MAX_RC_CHANNELS;

      g_pSMRCIn->uTimeStamp = g_TimeNow;
      g_pSMRCIn->uFrameIndex = (u8)iFrameNumber;
      g_pSMRCIn->uChannelsCount = (u8)nCh;
      for( int i=0; i<nCh; i++ )
      {
         if ( s_lastRCReadVals[i] < 4000 )
            g_pSMRCIn->uChannels[i] = s_lastRCReadVals[i];
         //else
         //   log_line("%d: %u", i, (u32) s_lastRCReadVals[i]);
      }
   }

   /*
   char szTmp[256];
   char szOut[256];
   sprintf(szOut, "Fr %d: ", iFrameNumber);
   for( int i=0; i<12; i++ )
   {
      sprintf(szTmp, "%d ", (int)g_pSMRCIn->uChannels[i] );
      strcat(szOut, szTmp);
   }
   log_line(szOut);
   */
#endif
}

void checkReadRCIn()
{
#ifdef HW_CAPABILITY_I2C
   if ( NULL == g_pDeviceInfoRCIn && NULL == g_pDeviceInfoPicoExtender && (g_iHasExternalRCInputDevice==0) )
      return;

   if (  g_TimeNow < g_TimeLastRCInRead )
      g_TimeLastRCInRead = g_TimeNow;

   if ( g_TimeNow < g_TimeLastRCInRead + g_SleepTime )
      return;

   g_TimeLastRCInRead = g_TimeNow;
 
   if ( NULL == g_pSMRCIn )
      return;

   if ( (g_nFileRCIn > 0) || (g_nFilePicoExtender > 0) )
   {
   	  _read_RCIn_OldMethod();
      return;
   }

   u8 bufferOut[64];
   u8 bufferIn[64];
   for( int iDevice=0; iDevice<g_nCountExternalDevices; iDevice++ )
   {
     	if ( ! (g_uListExternalDevicesFlags[iDevice] & I2C_CAPABILITY_FLAG_RC_INPUT) )
         continue;
   	  if ( g_pListExternalDevices[iDevice]->uParams[0] == 0 )
   	     continue;
   	  if ( g_nListFilesExternalDevices[iDevice] <= 0 )
   	  {
   	     g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
   	     return;
      }

      // Get device RC channels
      bufferOut[0] = I2C_COMMAND_START_FLAG;
      bufferOut[1] = I2C_COMMAND_ID_RC_GET_CHANNELS;
      bufferOut[2] = base_compute_crc8(bufferOut,2);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iDevice], bufferOut[0]);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iDevice], bufferOut[1]);
      wiringPiI2CWrite(g_nListFilesExternalDevices[iDevice], bufferOut[2]);
      for( int i=0; i<27; i++ )
      {
         int res = wiringPiI2CRead(g_nListFilesExternalDevices[iDevice]);
         if ( res < 0 )
         {
            log_softerror_and_alarm("Failed to get I2C external device RC channels at address 0x%02X (external module), got: %d. ", g_pListExternalDevices[iDevice]->nI2CAddress, res);
            g_iReadRCInConsecutiveFailCount++;
            return;
         }
         bufferIn[i] = res;
      }
      u8 uCRC = base_compute_crc8(bufferIn,26);
      if ( uCRC != bufferIn[26] )
      {
         //log_softerror_and_alarm("Failed to get I2C external device RC channels at address 0x%02X (external module), invalid CRC in response.", g_pListExternalDevices[iDevice]->nI2CAddress);
         g_iReadRCInConsecutiveFailCount++;
         return;      	
      }

      g_iReadRCInConsecutiveFailCount = 0;

      if ( NULL != g_pSMRCIn )
      {
         int nCh = 16;
         if ( nCh > MAX_RC_CHANNELS )
            nCh = MAX_RC_CHANNELS;

         g_pSMRCIn->uTimeStamp = g_TimeNow;
         g_pSMRCIn->uFrameIndex = bufferIn[1];
         if ( bufferIn[0] & 0x01 )
            g_pSMRCIn->uFlags &= (~RC_IN_FLAG_HAS_INPUT); // No input
         else
            g_pSMRCIn->uFlags |= RC_IN_FLAG_HAS_INPUT;

         g_pSMRCIn->uChannelsCount = (u8)nCh;
         for( int i=0; i<nCh; i++ )
         {
            u16 val = bufferIn[2+i];
            if ( (i%2) == 0 )
               val += (bufferIn[18+i/2] & 0x0F)*256;
            else
               val += (bufferIn[18+i/2]>>4)*256;
            if ( (val > 0) && (val <= 4000) )
               g_pSMRCIn->uChannels[i] = val;
         }
         /*
         char szBuffD[256];
         szBuffD[0] = 0;
         for( int i=0; i<nCh; i++ )
         {
            u16 val = bufferIn[2+i];
            if ( (i%2) == 0 )
               val += (bufferIn[18+i/2] & 0x0F)*256;
            else
               val += (bufferIn[18+i/2]>>4)*256;
            if ( val > 500 && val < 2500 )
               g_pSMRCIn->uChannels[i] = val;

            char szTmp[32];
            sprintf(szTmp, "%d ", val);
            if ( i < 8 )
               strcat(szBuffD, szTmp);
         }
         log_line("%s", szBuffD);
         */
      }
   }
#endif
}

void checkReadRotaryEncoderAndButtons()
{
#ifdef HW_CAPABILITY_I2C
   if ( NULL == g_pSMRotaryEncoderButtonsEvents )
      return;

   if ( NULL == g_pDeviceInfoPicoExtender && (! g_bHasExternalRotaryDevice) )
      return;

   ControllerSettings* pCS = get_ControllerSettings();

   int iDevicesSignaledCount = 0;

   for( int i=0; i<g_nCountExternalDevices; i++ )
   {
      if ( ! (g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_ROTARY2) )
      if ( ! (g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_ROTARY) )
      if ( ! (g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_BUTTONS) )
         continue;
      if ( g_nListFilesExternalDevices[i] <= 0 )
         continue;

      u8 bufferIn[8];
      u8 bufferOut[8];
      bool bGotRotaryEvents = false;
      bool bGotRotaryEvents2 = false;
      bool bGotButtonsEvents = false;
      u32 uRotaryEvents = 0;
      u32 uRotaryEvents2 = 0;
      u32 uButtonsEvents = 0;

      if ( g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_ROTARY2 )
      {
         bufferOut[0] = I2C_COMMAND_START_FLAG;
         bufferOut[1] = I2C_COMMAND_ID_GET_ROTARY_EVENTS2;
         bufferOut[2] = base_compute_crc8(bufferOut,2);

         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[0]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[1]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[2]);
         int res1 = wiringPiI2CRead(g_nListFilesExternalDevices[i]);
         int res2 = wiringPiI2CRead(g_nListFilesExternalDevices[i]);
         if ( res1 < 0 || res2 < 0 )
         {
            log_softerror_and_alarm("Failed to get rotary events2 from I2C external device at address 0x%02X (external module), got: %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res1, res2);
            continue;
         }
         bufferIn[0] = res1;
         bufferIn[1] = base_compute_crc8(bufferIn,1);

         if ( bufferIn[1] != res2 )
         {
            //log_softerror_and_alarm("Got invalid CRC on get rotary events from I2C external device at address 0x%02X (external module), got: %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res1, res2);
            continue;
         }

         // Has events?
         if ( bufferIn[0] != 0 )
         {
            bGotRotaryEvents2 = true;
            uRotaryEvents2 = bufferIn[0];
         }
      }

      if ( g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_ROTARY )
      {
         bufferOut[0] = I2C_COMMAND_START_FLAG;
         bufferOut[1] = I2C_COMMAND_ID_GET_ROTARY_EVENTS;
         bufferOut[2] = base_compute_crc8(bufferOut,2);

         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[0]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[1]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[2]);
         int res1 = wiringPiI2CRead(g_nListFilesExternalDevices[i]);
         int res2 = wiringPiI2CRead(g_nListFilesExternalDevices[i]);
         if ( res1 < 0 || res2 < 0 )
         {
            log_softerror_and_alarm("Failed to get rotary events from I2C external device at address 0x%02X (external module), got: %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res1, res2);
            continue;
         }
         bufferIn[0] = res1;
         bufferIn[1] = base_compute_crc8(bufferIn,1);

         if ( bufferIn[1] != res2 )
         {
            //log_softerror_and_alarm("Got invalid CRC on get rotary events from I2C external device at address 0x%02X (external module), got: %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res1, res2);
            continue;
         }

         // Has events?
         if ( bufferIn[0] != 0 )
         {
            bGotRotaryEvents = true;
            uRotaryEvents = bufferIn[0];
         }
      }

      if ( g_uListExternalDevicesFlags[i] & I2C_CAPABILITY_FLAG_BUTTONS )
      {
         bufferOut[0] = I2C_COMMAND_START_FLAG;
         bufferOut[1] = I2C_COMMAND_ID_GET_BUTTONS_EVENTS;
         bufferOut[2] = base_compute_crc8(bufferOut,2);

         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[0]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[1]);
         wiringPiI2CWrite(g_nListFilesExternalDevices[i], bufferOut[2]);
         int res[5];
         for( int k=0; k<5; k++ )
            res[k] = wiringPiI2CRead(g_nListFilesExternalDevices[i]);

         bool bFailed = false;

         for( int k=0; k<5; k++ )
         {
            bufferIn[k] = (u8)res[k];
            if ( res[k] < 0 )
               bFailed = true;
         }
         if ( bFailed )
         {
            log_softerror_and_alarm("Failed to get buttons events from I2C external device at address 0x%02X (external module), got: %d, %d, %d, %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res[0], res[1], res[2], res[3], res[4]);
            continue;
         }

         u8 uCRC = base_compute_crc8(bufferIn,4);

         if ( bufferIn[4] != uCRC )
         {
            //log_softerror_and_alarm("Got invalid CRC on get buttons events from I2C external device at address 0x%02X (external module), got: %d, %d, %d, %d, %d.", g_pListExternalDevices[i]->nI2CAddress, res[0], res[1], res[2], res[3], res[4]);
            continue;
         }

         // Has events?

         bool bHasEvents = false;
         for( int k=0; k<4; k++ )
         {
            if ( res[k] > 0 )
               bHasEvents = true;
         }
         if ( bHasEvents)
         {
            bGotButtonsEvents = true;
            memcpy((u8*)&uButtonsEvents, bufferIn, 4);
         }
      }

      if ( bGotRotaryEvents || bGotButtonsEvents || bGotRotaryEvents2 )
      {
         iDevicesSignaledCount++;
         if ( 1 == iDevicesSignaledCount )
         {
            g_pSMRotaryEncoderButtonsEvents->uButtonsEvents = uButtonsEvents;
            g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents = uRotaryEvents;
            g_pSMRotaryEncoderButtonsEvents->uRotaryEncoder2Events = uRotaryEvents2;
         }
         else
         {
            g_pSMRotaryEncoderButtonsEvents->uButtonsEvents |= uButtonsEvents;
            g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= uRotaryEvents;
            g_pSMRotaryEncoderButtonsEvents->uRotaryEncoder2Events |= uRotaryEvents2;
         }
      }
   }

   if ( iDevicesSignaledCount > 0 )
   {
      g_pSMRotaryEncoderButtonsEvents->uEventIndex++;
      g_pSMRotaryEncoderButtonsEvents->uTimeStamp = g_TimeNow;
      g_pSMRotaryEncoderButtonsEvents->uCRC = base_compute_crc32((u8*)g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));
      return;
   }

   if ( g_nFilePicoExtender <= 0 )
      return;


   int iValues = -1;
   if ( pCS->nRotaryEncoderSpeed == 0 )
      iValues = wiringPiI2CReadReg8(g_nFilePicoExtender, I2C_DEVICE_COMMAND_ID_PICO_EXTENDER_GET_ROTARY_ENCODER_ACTIONS);
   else
      iValues = wiringPiI2CReadReg8(g_nFilePicoExtender, I2C_DEVICE_COMMAND_ID_PICO_EXTENDER_GET_ROTARY_ENCODER_ACTIONS_SLOW);

   if ( iValues < 0 )
   {
      //log_line("Failed to read rotary encoder");
      return;
   }

   // No events ?
   if ( iValues == 0 || (iValues & 0xFF) == 0x80 )
      return;

   g_pSMRotaryEncoderButtonsEvents->uButtonsEvents = 0;
   g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents = 0;
   g_pSMRotaryEncoderButtonsEvents->uRotaryEncoder2Events = 0;
   
   if ( iValues & (0x01<<1) )
      g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= (1<<1);
   else if ( iValues & 0x01 )
      g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= 1;

   if ( iValues & (0x01<<2) )
   {
      g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= (1<<3);
      if ( iValues & (0x01<<4) )
         g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= (1<<5);
   }
   if ( iValues & (0x01<<3) )
   {
      g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= (1<<2);
      if ( iValues & (0x01<<5) )
         g_pSMRotaryEncoderButtonsEvents->uRotaryEncoderEvents |= (1<<4);
   }
         
   g_pSMRotaryEncoderButtonsEvents->uEventIndex++;
   g_pSMRotaryEncoderButtonsEvents->uTimeStamp = g_TimeNow;
   g_pSMRotaryEncoderButtonsEvents->uCRC = base_compute_crc32((u8*)g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));


   /*
   char szBuff[32];
   szBuff[0] = 0;
   for( int i=0; i<8; i++ )
   {
      if ( iValues & (0x01<<(7-i)) )
         strcat(szBuff,"1");
      else
         strcat(szBuff,"0");
   }
   log_line(szBuff);
   */
#endif
}

void handle_sigint(int sig) 
{ 
   g_bQuit = true;
   log_line("Caught signal to stop: %d\n", sig);
} 


int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
   
   log_init("RubyI2C");
   //log_enable_stdout();

   #if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RASPBERRY)
   log_line("Using Raspberry built in wiringPi library.");
   #endif
   #if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RADXA)
   log_line("Using Radxa custom wiringPi library.");
   #endif

   hardware_detectBoardAndSystemType();
   
   hardware_i2c_enumerate_busses(0);

   for( int i=0; i<MAX_I2C_DEVICES; i++ )
   {
      g_pListExternalDevices[i] = NULL;
      g_nListFilesExternalDevices[i] = -1;
      g_uListExternalDevicesFlags[i] = 0;
      g_bListExternalDevicesSetupCorrectly[i] = false;
   }
   g_nCountExternalDevices = 0;

   g_pSMCurrent = shared_mem_i2c_current_open_for_write();
   g_pSMRCIn = shared_mem_i2c_controller_rc_in_open_for_write();
   g_pSMRotaryEncoderButtonsEvents = shared_mem_i2c_rotary_encoder_buttons_events_open_for_write();

   if ( NULL != g_pSMRCIn )
   {
      g_pSMRCIn->version = 0;
      g_pSMRCIn->uFlags = 0; // no input
      g_pSMRCIn->uTimeStamp = 0;
      g_pSMRCIn->uFrameIndex = 0;
      g_pSMRCIn->uChannelsCount = 0;
      for( int i=0; i<MAX_RC_CHANNELS; i++ )
         g_pSMRCIn->uChannels[i] = 0;
      log_line("Openened shared mem [%s] for writing.", SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN);
   }
   else
      log_softerror_and_alarm("Failed to open shared mem [%s] for writing.", SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN);

   if ( NULL != g_pSMRotaryEncoderButtonsEvents )
   {
      g_pSMRotaryEncoderButtonsEvents->uTimeStamp = 0;
      g_pSMRotaryEncoderButtonsEvents->uEventIndex = 0;
      g_pSMRotaryEncoderButtonsEvents->uCRC = 0;
      g_pSMRotaryEncoderButtonsEvents->uRotaryEncoder2Events = 0;
      g_pSMRotaryEncoderButtonsEvents->uHasRotaryEncoder = 0;
      g_pSMRotaryEncoderButtonsEvents->uHasSecondaryRotaryEncoder = 0;
   }

   for( int i=0; i<I2C_DEVICE_PARAM_MAX_CHANNELS; i++ )
      s_lastRCReadVals[i] = 0;

   load_settings();

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iCoresAdjustment )
      hw_set_current_thread_affinity("ruby_i2c", CORE_AFFINITY_I2C, CORE_AFFINITY_I2C);
   int iPrio = pCS->iThreadPriorityCentral;
   if ( pCS->iPrioritiesAdjustment && (iPrio > 1) && (iPrio < 100) )
   {
      if ( iPrio > 2 )
         iPrio--;
      hw_set_priority_current_proc(iPrio); 
   }

   char szFile[128];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_I2C_UPDATED);

   log_line("----------------------------------------------");
   log_line("Initialization complete. Starting main loop...");

   g_TimeLastReloadCheck = g_TimeNow = get_current_timestamp_ms();

   while ( !g_bQuit )
   {
      g_uLoopCounter++;
      hardware_sleep_ms(g_SleepTime);
      if ( g_bQuit )
         break;

      g_TimeNow = get_current_timestamp_ms();

      if (  g_TimeNow >= g_TimeLastReloadCheck+500 )
      {
         g_TimeLastReloadCheck = g_TimeNow;
         if ( access(szFile, R_OK) != -1 )
         {
         	  log_line("I2C devices settings changed. Reloading settings and setting up devices.");
            close_files();
            load_settings();
            char szBuff[128];
            sprintf(szBuff, "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_I2C_UPDATED);
            hw_execute_bash_command_silent(szBuff, NULL);
         }
      }

      for( int i=0; i<g_nCountExternalDevices; i++ )
         if ( ! g_bListExternalDevicesSetupCorrectly[i] )
            _setup_external_device(i);
      checkReadINA();
      checkReadRCIn();
      checkReadRotaryEncoderAndButtons();

      if ( g_iReadRCInConsecutiveFailCount > 10 )
      {
          g_iReadRCInConsecutiveFailCount = 0;
          close_files();
          load_settings();            
      }
   }

   close_files();
   shared_mem_i2c_current_close(g_pSMCurrent);
   shared_mem_i2c_controller_rc_in_close(g_pSMRCIn);
   shared_mem_i2c_rotary_encoder_buttons_events_close(g_pSMRotaryEncoderButtonsEvents);
   log_line("Finished execution.Exit");
   return 0;
} 