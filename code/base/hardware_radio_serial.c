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
#include "hardware_radio_serial.h"
#include "hardware_serial.h"
#include "hardware_procs.h"

// Returns 1 if a hardware radio info was added for this port, 0 if not

int hardware_radio_serial_add_radio_interface_for_port(hw_serial_port_info_t* pSerialPortInfo)
{
   if ( NULL == pSerialPortInfo )
      return 0;

   int iPositionToAddTo = hardware_get_radio_interfaces_count();
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if ( 0 == strcmp(pRadioHWInfo->szDriver, pSerialPortInfo->szPortDeviceName) )
      {
         iPositionToAddTo = i;
         break;
      }
   }

   if ( iPositionToAddTo >= MAX_RADIO_INTERFACES )
      return 0;

   if ( iPositionToAddTo >= hardware_get_radio_interfaces_count() )
   {
      radio_hw_info_t radioInfo;
      memset(&radioInfo, 0, sizeof(radio_hw_info_t));
      hardware_add_radio_interface_info(&radioInfo);
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iPositionToAddTo);
   pRadioHWInfo->isSupported = 1;
   pRadioHWInfo->isEnabled = 1;
   pRadioHWInfo->isHighCapacityInterface = 0;
   pRadioHWInfo->isSerialRadio = 1;
   pRadioHWInfo->isConfigurable = 0;
   pRadioHWInfo->iCurrentDataRateBPS = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
   pRadioHWInfo->isTxCapable = 1;
   pRadioHWInfo->iCardModel = CARD_MODEL_SERIAL_RADIO_ELRS;
   pRadioHWInfo->iRadioType = RADIO_TYPE_SERIAL;
   pRadioHWInfo->iRadioDriver = RADIO_HW_DRIVER_SERIAL;

   if ( pSerialPortInfo->iPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433 )
   {
      pRadioHWInfo->supportedBands = RADIO_HW_SUPPORTED_BAND_433;
      pRadioHWInfo->uCurrentFrequencyKhz = 433000;
   }
   if ( pSerialPortInfo->iPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868 )
   {
      pRadioHWInfo->supportedBands = RADIO_HW_SUPPORTED_BAND_868;
      pRadioHWInfo->uCurrentFrequencyKhz = 868000;
   }
   if ( pSerialPortInfo->iPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915 )
   {
      pRadioHWInfo->supportedBands = RADIO_HW_SUPPORTED_BAND_915;
      pRadioHWInfo->uCurrentFrequencyKhz = 915000;
   }
   if ( pSerialPortInfo->iPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24 )
   {
      pRadioHWInfo->supportedBands = RADIO_HW_SUPPORTED_BAND_24;
      pRadioHWInfo->uCurrentFrequencyKhz = 2400000;
   }
   pRadioHWInfo->lastFrequencySetFailed = 0;
   strncpy(pRadioHWInfo->szDriver, pSerialPortInfo->szPortDeviceName, sizeof(pRadioHWInfo->szDriver)/sizeof(pRadioHWInfo->szDriver[0]));
   strncpy(pRadioHWInfo->szName, "ELRS", sizeof(pRadioHWInfo->szName)/sizeof(pRadioHWInfo->szName[0]));
   strncpy(pRadioHWInfo->szDescription, "ELRS radio", sizeof(pRadioHWInfo->szDescription)/sizeof(pRadioHWInfo->szDescription[0]));
   strncpy(pRadioHWInfo->szUSBPort, "X", sizeof(pRadioHWInfo->szUSBPort)/sizeof(pRadioHWInfo->szUSBPort[0]));

   char szSufix[32];
   int iLen = (int) strlen(pSerialPortInfo->szPortDeviceName);
   if ( iLen >= 4 )
      strncpy(szSufix, &(pSerialPortInfo->szPortDeviceName[iLen-4]), sizeof(szSufix)/sizeof(szSufix[0]));
   else
      strncpy(szSufix, pSerialPortInfo->szPortDeviceName, sizeof(szSufix)/sizeof(szSufix[0]));
   snprintf(pRadioHWInfo->szMAC, sizeof(pRadioHWInfo->szMAC)/sizeof(pRadioHWInfo->szMAC[0]), "ELRS%s", szSufix);
   return 1;
}

int hardware_radio_serial_parse_and_add_from_serial_ports_config()
{
   log_line("[HW-RS] Parsing serial ports and adding hardware radio info...");
   int iCountAdded = 0;

   int iCountSerialPorts = hardware_serial_get_ports_count();
   for( int i=0; i<iCountSerialPorts; i++ )
   {
      hw_serial_port_info_t* pSerialPortInfo = hardware_get_serial_port_info(i);
      if ( NULL == pSerialPortInfo )
         continue;
      log_line("[HW-RS] Inspecting serial port %d: %s %s...", i, pSerialPortInfo->szName, pSerialPortInfo->szPortDeviceName);
      if ( pSerialPortInfo->iPortUsage != SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433 )
      if ( pSerialPortInfo->iPortUsage != SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868 )
      if ( pSerialPortInfo->iPortUsage != SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915 )
      if ( pSerialPortInfo->iPortUsage != SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24 )
         continue;
      log_line("[HW-RS] Port %s is used for ELRS radio", pSerialPortInfo->szPortDeviceName);
      iCountAdded += hardware_radio_serial_add_radio_interface_for_port(pSerialPortInfo);
   }
   log_line("[HW-RS] Parsed serial ports. Added %d hardware radio serial interfaces.", iCountAdded);
   
   if ( iCountAdded > 0 )
   {
      hardware_save_radio_info();
      hardware_log_radio_info(NULL, 0);
   }
   return iCountAdded;
}

int hardware_radio_serial_open_for_read_write(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RS] Open: Failed to get serial radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_serial_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RS] Open: Tried to open hardware radio interface %d which is not a serial radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
  
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HW-RS] Open: Failed to find serial port configuration for serial radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPortFD = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPortFD <= 0 )
   {
      log_error_and_alarm("[HW-RS] Open: Failed to open serial port for serial radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }
   pRadioInfo->openedForWrite = 1;
   pRadioInfo->openedForRead = 1;
   pRadioInfo->runtimeInterfaceInfoRx.selectable_fd = iSerialPortFD;
   pRadioInfo->runtimeInterfaceInfoTx.selectable_fd = iSerialPortFD;

   log_line("[HW-RS] Opened serial radio interface %d for read/write. fd=%d", iHWRadioInterfaceIndex+1, iSerialPortFD);
   return 1;
}

int hardware_radio_serial_close(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RS] Close: Failed to get serial radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_serial_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RS] Close: Tried to close hardware radio interface %d which is not a serial radio.", iHWRadioInterfaceIndex+1);
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


int hardware_radio_serial_write_packet(int iHWRadioInterfaceIndex, u8* pData, int iLength)
{
   if ( (NULL == pData) || (iLength <= 0) )
   {
      log_softerror_and_alarm("[HW-RS] Write Serial: Invalid parameters: NULL buffer or 0 length.");
      return 0;
   }

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HW-RS] Write Serial: Failed to get serial radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_serial_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HW-RS] Write Serial: Tried to write to hardware radio interface %d which is not a serial radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   if ( (! pRadioInfo->openedForWrite) || (pRadioInfo->runtimeInterfaceInfoTx.selectable_fd <= 0) )
   {
      log_softerror_and_alarm("[HW-RS] Write Serial: Interface is not opened for write.");
      return 0;
   }

   int iRes = write(pRadioInfo->runtimeInterfaceInfoTx.selectable_fd, pData, iLength);

   if ( iRes != iLength )
   {
      log_softerror_and_alarm("[HW-RS] Write Serial: Failed to write. Written %d bytes of %d bytes.", iRes, iLength);
      return 0;
   }

   return 1;
}
