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

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "base.h"
#include "hardware.h"
#include "hardware_radio.h"
#include "hardware_radio_sik.h"
#include "hardware_serial.h"
#include "config.h"
#include "hardware_procs.h"
#include "../common/string_utils.h"

int s_OptionsSerialBaudRatesC[] = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };

int s_iHardwareSerialPortsWasInitialized = 0;

int s_iCountHardwareSerialPorts = 0;
hw_serial_port_info_t s_HardwareSerialPortsInfo[MAX_SERIAL_PORTS];
int s_iCountLoadedSerialPorts = 0;
hw_serial_port_info_t s_LoadedSerialPortsInfo[MAX_SERIAL_PORTS];

int* hardware_serial_get_baud_rates()
{
   return s_OptionsSerialBaudRatesC;
}

int hardware_serial_get_baud_rates_count()
{
   return sizeof(s_OptionsSerialBaudRatesC) / sizeof(s_OptionsSerialBaudRatesC[0]);
}

void _hardware_enumerate_serial_ports()
{
   s_iCountHardwareSerialPorts = 0;

   #ifdef HW_PLATFORM_RASPBERRY
   strcpy(s_HardwareSerialPortsInfo[0].szName, "Serial-0");
   strcpy(s_HardwareSerialPortsInfo[0].szPortDeviceName, "/dev/serial0");
   s_HardwareSerialPortsInfo[0].iSupported = 1;
   s_HardwareSerialPortsInfo[0].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
   s_HardwareSerialPortsInfo[0].iPortUsage = SERIAL_PORT_USAGE_NONE;
   s_iCountHardwareSerialPorts = 1;
   #endif
   
   #ifdef HW_PLATFORM_OPENIPC_CAMERA

   strcpy(s_HardwareSerialPortsInfo[0].szName, "Serial-0");
   strcpy(s_HardwareSerialPortsInfo[0].szPortDeviceName, "/dev/ttyAMA0");
   if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
      strcpy(s_HardwareSerialPortsInfo[0].szPortDeviceName, "/dev/ttyS0");
   s_HardwareSerialPortsInfo[0].iSupported = 1;
   s_HardwareSerialPortsInfo[0].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
   s_HardwareSerialPortsInfo[0].iPortUsage = SERIAL_PORT_USAGE_NONE;
   s_iCountHardwareSerialPorts = 1;

   //if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   {
      for( int i=1; i<5; i++ )
      {
         sprintf(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szPortDeviceName, "/dev/ttyS%d", i);
         if ( access(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szPortDeviceName, R_OK) != -1 )
         {
            sprintf(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szName, "Serial-%d", i);
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 1;
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iPortUsage = SERIAL_PORT_USAGE_NONE;
            s_iCountHardwareSerialPorts++;
         }
      }
   }
   #endif

   for( int i=0; i<MAX_SERIAL_PORTS-4; i++ )
   {
      char szBuff[128];
      char szOutput[1024];
      sprintf(szBuff, "/dev/ttyUSB%d", i);
      if( access( szBuff, R_OK ) == -1 )
         continue;
      if ( s_iCountHardwareSerialPorts >= MAX_SERIAL_PORTS )
         continue;

      sprintf(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szName, "Serial-USB%d",i);
      strcpy(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szPortDeviceName, szBuff);
      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 1;
      sprintf(szBuff, "ls /dev/serial/by-id/ -al | grep ttyUSB%d", i);
      hw_execute_bash_command_raw_silent(szBuff, szOutput);
   
      if ( NULL != strstr(szOutput, "CP2102") )
         s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 1;
      else if ( NULL != strstr(szOutput, "HL-340") )
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 0;
      else
      {
         hw_execute_bash_command_raw_silent("lsusb | grep Serial | grep 340 | grep USB", szOutput);
         if ( NULL != strstr(szOutput, "340") )
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 0;
      }

      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iPortUsage = SERIAL_PORT_USAGE_NONE;
      s_iCountHardwareSerialPorts++;
   }
}

int hardware_serial_init_ports()
{
   if ( s_iHardwareSerialPortsWasInitialized )
      return s_iCountHardwareSerialPorts;

   log_line("[HW-S] Initializing serial ports...");
   log_line("[HW-S] Enumerating hardware serial ports...");
   _hardware_enumerate_serial_ports();
   s_iHardwareSerialPortsWasInitialized = 1;
   log_line("[HW-S] Enumerated hardware serial ports. %d serial ports found.", s_iCountHardwareSerialPorts);

   int iCountRestored = hardware_serial_reload_ports_settings();

   if ( iCountRestored < s_iCountHardwareSerialPorts )
   {
      for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      {
         int iFoundSettings = 0;
         for( int k=0; k<s_iCountLoadedSerialPorts; k++ )
         {
            if ( 0 == strcmp(s_HardwareSerialPortsInfo[i].szPortDeviceName, s_LoadedSerialPortsInfo[k].szPortDeviceName) )
            {
               iFoundSettings = 1;
               break;
            }
         }
         if ( iFoundSettings )
            continue;

         memcpy(&(s_LoadedSerialPortsInfo[s_iCountLoadedSerialPorts]), &(s_HardwareSerialPortsInfo[i]), sizeof(hw_serial_port_info_t));
         s_iCountLoadedSerialPorts++;
         log_line("[HW-S] Added serial port info settings for port %s", s_HardwareSerialPortsInfo[i].szPortDeviceName);
      }
      log_line("[HW-S] Current serial ports configuration has been updated. Saving current configuration.");
      hardware_serial_save_configuration();
   }

   log_line("[HW-S] Initialized %d serial ports:", s_iCountHardwareSerialPorts);
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      log_line("[HW-S] Serial port %d: [%s] [%s], speed: %ld bps, usage: %d (%s), supported: %d", i+1,
         s_HardwareSerialPortsInfo[i].szName,
         s_HardwareSerialPortsInfo[i].szPortDeviceName, s_HardwareSerialPortsInfo[i].lPortSpeed,
         s_HardwareSerialPortsInfo[i].iPortUsage,
         str_get_serial_port_usage(s_HardwareSerialPortsInfo[i].iPortUsage),
         s_HardwareSerialPortsInfo[i].iSupported);

   return s_iCountHardwareSerialPorts;
}

// Returns the number of hardware serial ports that had settings restored

int hardware_serial_reload_ports_settings()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
   {
      log_line("[HW-S] Enumerating hardware serial ports...");
      _hardware_enumerate_serial_ports();
      log_line("[HW-S] Enumerated hardware serial ports. %d serial ports found.", s_iCountHardwareSerialPorts);
      s_iHardwareSerialPortsWasInitialized = 1;
   }

   log_line("[HW-S] Loading and validating serial ports settings...");

   int iFailed = 1;
      
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_HW_SERIAL_PORTS);

   if ( access(szFile, R_OK ) == -1 )
   {
      log_line("[HW-S] No serial ports configuration file [%s]", szFile);
      iFailed = 0;
      s_iCountLoadedSerialPorts = 0;
   }
   else
   {
      FILE* fd = fopen(szFile, "r");
      if ( NULL != fd )
      {
         iFailed = 0;
         if ( 1 != fscanf(fd, "%d", &s_iCountLoadedSerialPorts) )
            iFailed = 1;
         if ( s_iCountLoadedSerialPorts < 0 || s_iCountLoadedSerialPorts > MAX_SERIAL_PORTS )
         {
            s_iCountLoadedSerialPorts = 0;
            iFailed = 2;
         }
         for( int i=0; i<s_iCountLoadedSerialPorts; i++ )
         {
            if ( (!iFailed) && (1 != fscanf(fd, "%s", s_LoadedSerialPortsInfo[i].szName)) )
               iFailed = 5;
            if ( (!iFailed) && (1 != fscanf(fd, "%s", s_LoadedSerialPortsInfo[i].szPortDeviceName)) )
               iFailed = 3;
            for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
               if ( s_LoadedSerialPortsInfo[i].szName[k] == '*' )
                  s_LoadedSerialPortsInfo[i].szName[k] = ' ';
            for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
               if ( s_LoadedSerialPortsInfo[i].szPortDeviceName[k] == '*' )
                  s_LoadedSerialPortsInfo[i].szPortDeviceName[k] = ' ';

            s_LoadedSerialPortsInfo[i].szName[MAX_SERIAL_PORT_NAME-1] = 0;
            s_LoadedSerialPortsInfo[i].szPortDeviceName[MAX_SERIAL_PORT_NAME-1] = 0;
            if ( (!iFailed) && (3 != fscanf(fd, "%d %ld %d", &(s_LoadedSerialPortsInfo[i].iSupported), &(s_LoadedSerialPortsInfo[i].lPortSpeed),&(s_LoadedSerialPortsInfo[i].iPortUsage))) )
               iFailed = 4;
         }
         if ( iFailed )
            s_iCountLoadedSerialPorts = 0;
         fclose(fd);
      }

      if ( iFailed )
         log_softerror_and_alarm("[HW-S] Failed to load existing serial ports configuration from file [%s]. 0 serial ports loaded.", szFile);
      else
      {
         log_line("[HW-S] Loaded existing serial ports configuration from file [%s]. Loaded %d serial ports: ", szFile, s_iCountLoadedSerialPorts);
      
         for( int i=0; i<s_iCountLoadedSerialPorts; i++ )
            log_line("[HW-S] Serial port %d: [%s] [%s], speed: %ld bps, usage: %d (%s), supported: %d", i+1,
               s_LoadedSerialPortsInfo[i].szName,
               s_LoadedSerialPortsInfo[i].szPortDeviceName, s_LoadedSerialPortsInfo[i].lPortSpeed,
               s_LoadedSerialPortsInfo[i].iPortUsage,
               str_get_serial_port_usage(s_LoadedSerialPortsInfo[i].iPortUsage),
               s_LoadedSerialPortsInfo[i].iSupported);
      }
   }

   // Copy settings from the loaded serial ports to the detected serial ports

   int iCountMatched = 0;
   int iCountUpdated = 0;
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      for( int k=0; k<s_iCountLoadedSerialPorts; k++ )
      {
         if ( 0 == strcmp(s_HardwareSerialPortsInfo[i].szPortDeviceName, s_LoadedSerialPortsInfo[k].szPortDeviceName) )
         {
            iCountMatched++;
            strcpy(s_HardwareSerialPortsInfo[i].szName, s_LoadedSerialPortsInfo[k].szName);
            int iUpdated = 0;
            if ( s_HardwareSerialPortsInfo[i].lPortSpeed != s_LoadedSerialPortsInfo[k].lPortSpeed ) 
               iUpdated = 1;
            if ( s_HardwareSerialPortsInfo[i].iPortUsage != s_LoadedSerialPortsInfo[k].iPortUsage )
               iUpdated = 1;
            s_HardwareSerialPortsInfo[i].lPortSpeed = s_LoadedSerialPortsInfo[k].lPortSpeed;
            s_HardwareSerialPortsInfo[i].iPortUsage = s_LoadedSerialPortsInfo[k].iPortUsage;
            
            if ( iUpdated )
               iCountUpdated++;

            if ( iUpdated )
               log_line("[HW-S] Serial port %d [%s] settings have been restored and updated.", i+1, s_HardwareSerialPortsInfo[i].szName);
            else
               log_line("[HW-S] Serial port %d [%s] settings have been restored.", i+1, s_HardwareSerialPortsInfo[i].szName);
            break;
         }
      }
   }
   log_line("[HW-S] Restored hw serial ports settings: %d of %d hw serial ports have been restored from stored configuration. %d where updated.", iCountMatched, s_iCountHardwareSerialPorts, iCountUpdated);
   return iCountMatched;
}

void hardware_serial_save_configuration()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      return;

   // First copy current hw serial ports settings to the ones to be saved

   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      for( int k=0; k<s_iCountLoadedSerialPorts; k++ )
      {
         if ( 0 == strcmp(s_HardwareSerialPortsInfo[i].szPortDeviceName, s_LoadedSerialPortsInfo[k].szPortDeviceName) )
            memcpy(&(s_LoadedSerialPortsInfo[k]), &(s_HardwareSerialPortsInfo[i]), sizeof(hw_serial_port_info_t));
      }
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_HW_SERIAL_PORTS);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HW-S] Failed to save hardware serial ports configuration.");
      return;
   }

   fprintf(fd, "%d\n", s_iCountLoadedSerialPorts);
   for( int i=0; i<s_iCountLoadedSerialPorts; i++ )
   {
      char szBuff[MAX_SERIAL_PORT_NAME+1];
      strcpy(szBuff, s_LoadedSerialPortsInfo[i].szName);
      for( int k=0; k<(int)strlen(szBuff); k++ )
         if ( szBuff[k] == ' ' )
           szBuff[k] = '*';
      fprintf(fd, "%s\n", szBuff);
      
      strcpy(szBuff, s_LoadedSerialPortsInfo[i].szPortDeviceName);
      for( int k=0; k<(int)strlen(szBuff); k++ )
         if ( szBuff[k] == ' ' )
           szBuff[k] = '*';
      fprintf(fd, "%s\n", szBuff);
      fprintf(fd, "%d %ld %d\n", s_LoadedSerialPortsInfo[i].iSupported, s_LoadedSerialPortsInfo[i].lPortSpeed, s_LoadedSerialPortsInfo[i].iPortUsage );
   }
   fclose(fd);
   log_line("[HW-S] Saved serial ports configuration to file [%s]. %d serial ports:", szFile, s_iCountLoadedSerialPorts);
   for( int i=0; i<s_iCountLoadedSerialPorts; i++ )
      log_line("[HW-S] Saved Serial port %d: [%s] [%s], speed: %ld bps, usage: %d (%s), supported: %d", i+1,
         s_LoadedSerialPortsInfo[i].szName,
         s_LoadedSerialPortsInfo[i].szPortDeviceName, s_LoadedSerialPortsInfo[i].lPortSpeed,
         s_LoadedSerialPortsInfo[i].iPortUsage,
         str_get_serial_port_usage(s_LoadedSerialPortsInfo[i].iPortUsage),
         s_LoadedSerialPortsInfo[i].iSupported);
}

int hardware_serial_has_unsupported_ports()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();

   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      if ( s_HardwareSerialPortsInfo[i].iSupported == 0 )
         return 1;
   return 0;
}

int hardware_serial_get_ports_count()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();
   return s_iCountHardwareSerialPorts;
}

hw_serial_port_info_t* hardware_get_serial_port_info(int iPortIndex)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();

   if ( iPortIndex >= 0 && iPortIndex < s_iCountHardwareSerialPorts )
      return &(s_HardwareSerialPortsInfo[iPortIndex]);
   return NULL;
}

hw_serial_port_info_t* hardware_get_serial_port_info_from_serial_port_name(char* szPortName)
{
   if ( (NULL == szPortName) || (0 == szPortName[0]) )
      return NULL;
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();

   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      if ( 0 == strcmp(szPortName, s_HardwareSerialPortsInfo[i].szPortDeviceName) )
         return &(s_HardwareSerialPortsInfo[i]);
   }
   return NULL;
}

int hardware_configure_serial(const char* szDevName, long baudRate)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();
   return 1;

   if ( access(szDevName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("Hardware: Invalid serial port device to configure [%s]. Can't access it.", szDevName);
      return -1;    
   }

   //char szBuff[1024];
   //sprintf(szBuff, "stty -F %s -icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon %ld", szDevName, baudRate);
   //execute_bash_command(szBuff, NULL);
   
   int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );
   if ( -1 == fPort )
   {
      log_softerror_and_alarm("Failed to open serial port %s", szDevName);
      return 0;
   }
   
   struct termios options;
   tcgetattr(fPort, &options);
   cfmakeraw(&options);

   switch(baudRate)
   {
      case 1200:  cfsetospeed(&options, B1200); break;
      case 2400:  cfsetospeed(&options, B2400); break;
      case 4800:  cfsetospeed(&options, B4800); break;
      case 9600:  cfsetospeed(&options, B9600); break;
      case 19200: cfsetospeed(&options, B19200); break;
      case 38400: cfsetospeed(&options, B38400); break;
      case 57600: cfsetospeed(&options, B57600); break;
      case 115200: cfsetospeed(&options, B115200); break;
      default:    cfsetospeed(&options, B57600); break;
   }

   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_lflag &= ~ECHO; // no echo
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CLOCAL; // Set local mode on
   
   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;
   //options.c_cc[VMIN] = 0;
   //options.c_cc[VTIME] = 5;
   
   tcsetattr(fPort, TCSANOW, &options); //write options 
   close(fPort);
   

   log_line("Configured serial port %s to baudrate %ld bps.", szDevName, baudRate);
   return 1;

}

int hardware_open_serial_port(const char* szDevName, long baudRate)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();
   if ( NULL == szDevName || 0 == szDevName[0] )
   {
      log_softerror_and_alarm("Hardware: Invalid serial port name to open.");
      return -1;
   }

   if ( access(szDevName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("Hardware: Invalid serial port device to open [%s]. Can't access it.", szDevName);
      return -1;    
   }

   //int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NONBLOCK );
   int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );
   //int fPort = open ("/dev/serial0", O_RDONLY | O_NOCTTY | O_NDELAY );

   // working:
   //int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );

   if ( -1 == fPort )
   {
      log_softerror_and_alarm("Failed to open serial port %s", szDevName);
      return -1;
   }
   
   struct termios options;
   tcgetattr(fPort, &options);
   cfmakeraw(&options);

   int iUsedDefaultRate = 0;
   switch(baudRate)
   {
      case 1200:  cfsetospeed(&options, B1200); break;
      case 2400:  cfsetospeed(&options, B2400); break;
      case 4800:  cfsetospeed(&options, B4800); break;
      case 9600:  cfsetospeed(&options, B9600); break;
      case 19200: cfsetospeed(&options, B19200); break;
      case 38400: cfsetospeed(&options, B38400); break;
      case 57600: cfsetospeed(&options, B57600); break;
      case 115200: cfsetospeed(&options, B115200); break;
      default:    cfsetospeed(&options, B57600); iUsedDefaultRate = 1; break;
   }

   /*
   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CREAD | CLOCAL; // Set local mode on
   
   options.c_lflag &= ~ECHO; // no echo
   options.c_lflag &= ~ECHOE; // no echo
   options.c_lflag &= ~ECHONL; // no echo
   options.c_lflag &= ~ICANON;
   options.c_lflag &= ~ISIG;

   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;
   options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

   options.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
   options.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed 
   */

   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_lflag &= ~ECHO; // no echo
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CLOCAL; // Set local mode on
   
   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;

   //options.c_cc[VMIN] = 0;
   //options.c_cc[VTIME] = 5;

   tcsetattr(fPort, TCSANOW, &options); //write options 

   if ( iUsedDefaultRate )
      log_line("[HW-S]: Opened serial port %s with default baudrate %ld bps (%ld bps was invalid). fd=%d", szDevName, 57600, baudRate, fPort);
   else
      log_line("[HW-S]: Opened serial port %s with baudrate %ld bps. fd=%d", szDevName, baudRate, fPort);

   return fPort;
}

int hardware_serial_is_sik_radio(const char* szDevName)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_serial_init_ports();
   if ( NULL == szDevName || 0 == szDevName[0] )
   {
      log_softerror_and_alarm("Hardware: Invalid serial port name to check.");
      return 0;
   }
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( 0 != strcmp(szDevName, pRadioHWInfo->szDriver) )
         continue;
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         return 1;
   }
   return 0;
}


int hardware_serial_send_sik_command(int iSerialPortFD, const char* szCommand)
{
   if ( (iSerialPortFD <= 0) || (NULL == szCommand) || (0 == szCommand[0]) )
      return 0;

   char szBuff[256];
   strcpy(szBuff, szCommand);
   if ( 0 != strcmp(szCommand, "+++") )
      strcat(szBuff, "\r");
   int iLen = strlen(szBuff);
   if ( iLen != write(iSerialPortFD, szBuff, iLen) )
   {
      return 0;
   }
   return iLen;
}

// Returns the number of bytes read
int hardware_serial_wait_sik_response(int iSerialPortFD, int iTimeoutMS, int iMinimumLines, u8* pOutputBuffer, int* pInOutputLength)
{
   if ( (iSerialPortFD <= 0) || (NULL == pOutputBuffer) || (NULL == pInOutputLength) || ((*pInOutputLength)<=0) )
      return 0;

   if ( iTimeoutMS < 0 )
      iTimeoutMS = 0;

   u32 uTimeStart = get_current_timestamp_ms();
   u8 bufferRead[1024];

   int iMaxRead = *pInOutputLength;
   *pInOutputLength = 0;
   pOutputBuffer[0] = 0;

   int iCountLines = 0;
   while ( 1 )
   {
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = iTimeoutMS*1000;

      fd_set readset;   
      FD_ZERO(&readset);
      FD_SET(iSerialPortFD, &readset);
      
      int res = select(iSerialPortFD+1, &readset, NULL, NULL, &to);
      if ( res > 0 )
      if ( FD_ISSET(iSerialPortFD, &readset) )
      {
         int iRead = read(iSerialPortFD, bufferRead, 1024);
         if ( iRead > 0 )
         {
            if ( iRead < (iMaxRead-(*pInOutputLength)-1) )
            {
               memcpy(pOutputBuffer + (*pInOutputLength), bufferRead, iRead);
               (*pInOutputLength) = (*pInOutputLength) + iRead;
            }

            int iEnd = 0;
            if ( bufferRead[0] == 10 || bufferRead[0] == 13 )
               iEnd = 2;
            for( int i=iRead-1; i>=iEnd; i-- )
            {
               if ( bufferRead[i] == 13 )
                  iCountLines++;
            }

            if ( iCountLines >= iMinimumLines )
               break;
         }
      }
      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow >= uTimeStart + iTimeoutMS )
         break;

      iTimeoutMS = (uTimeStart+iTimeoutMS) - uTimeNow;
      uTimeStart = uTimeNow;
   }

   pOutputBuffer[(*pInOutputLength)] = 0;
   return *pInOutputLength;
}
