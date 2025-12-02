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


#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_procs.h"
#include <ctype.h>


void getSystemVersionString(char* p, u32 swversion)
{
   if ( NULL == p )
      return;
   u32 val = swversion;
   u32 major = (val >> 8) & 0xFF;
   u32 minor = val & 0xFF;
   if ( minor >= 10 )
      minor /= 10;
   sprintf(p, "%u.%u", major, minor);
}

int config_file_get_value(const char* szPropName)
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOut[1024];
   char* szTmp = NULL;
   int value = 0;

   sprintf(szComm, "grep '#%s!=' /boot/config.txt", szPropName);
   hw_execute_bash_command_silent(szComm, szOut);
   if ( strlen(szOut) > 5 )
   {
      szTmp = strchr(szOut, '=');
      if ( NULL != szTmp )
         sscanf(szTmp+1, "%d", &value);
      if ( value > 0 )
         value = -value;
   }

   sprintf(szComm, "grep '%s=' /boot/config.txt", szPropName);
   hw_execute_bash_command_silent(szComm, szOut);
   if ( strlen(szOut) > 5 )
   {
      szTmp = strchr(szOut, '=');
      if ( NULL != szTmp )
         sscanf(szTmp+1, "%d", &value);
      if ( value < 0 )
         value = -value;
   }
   return value;
}


void config_file_add_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];
   sprintf(szComm, "cat %s | grep %s", szFile, szPropName);
   hw_execute_bash_command(szComm, szOutput);
   if ( strlen(szOutput) >= strlen(szPropName) )
      return;

   u8 data[4096];
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return;
   int nSize = fread(data, 1, 4095, fd);
   fclose(fd);
   if ( nSize <= 0 )
      return;

   fd = fopen(szFile, "w");
   if ( NULL == fd )
      return;

   fprintf(fd, "%s=%d\n", szPropName, value);
   fwrite(data, 1, nSize, fd);
   fclose(fd);
}


void config_file_set_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[MAX_FILE_PATH_SIZE];
   if ( value <= 0 )
   {
      sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/#%s!=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "sed -i 's/%s=[-0-9]*/#%s!=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);
   }
   else
   {
      sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "sed -i 's/%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);
   }
}

void config_file_force_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[MAX_FILE_PATH_SIZE];
   sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/#%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);
}


void save_simple_config_fileU(const char* fileName, u32 value)
{
   FILE* fd = fopen(fileName, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to simple configuration to file: %s",fileName);
      return;
   }
   fprintf(fd, "%u\n", value);
   fclose(fd);
   log_line("Saved value %u to file: %s", value, fileName);
}

u32 load_simple_config_fileU(const char* fileName, u32 defaultValue)
{
   u32 returnValue = defaultValue;

   FILE* fd = fopen(fileName, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (missing file)",fileName);
      return defaultValue;
   }
   if ( 1 != fscanf(fd, "%u", &returnValue) )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (invalid config file)",fileName);
      fclose(fd);
      return defaultValue;
   }   
   
   fclose(fd);
   log_line("Loaded value %u from file: %s", returnValue, fileName);
   return returnValue;
}

void save_simple_config_fileI(const char* fileName, int value)
{
   FILE* fd = fopen(fileName, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save simple configuration to file: %s",fileName);
      return;
   }
   fprintf(fd, "%d\n", value);
   fclose(fd);
}

int load_simple_config_fileI(const char* fileName, int defaultValue)
{
   int returnValue = defaultValue;

   FILE* fd = fopen(fileName, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (missing file)",fileName);
      return defaultValue;
   }
   if ( 1 != fscanf(fd, "%d", &returnValue) )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (invalid config file)",fileName);
      log_line("Saving a default value (%d) and file.", defaultValue);
      save_simple_config_fileI(fileName, defaultValue);
      return defaultValue;
   }   
   
   fclose(fd);
   return returnValue;
}

FILE* try_open_base_version_file(char* szOutputFile)
{
   char szFile[MAX_FILE_PATH_SIZE];

   if ( NULL != szOutputFile )
      szOutputFile[0] = 0;

   strcpy(szFile, FOLDER_BINARIES);
   strcat(szFile, FILE_INFO_VERSION);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( NULL != szOutputFile )
         strcpy(szOutputFile, szFile);
      return fd;
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_INFO_VERSION);
   fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( NULL != szOutputFile )
         strcpy(szOutputFile, szFile);
      return fd;
   }

   strcpy(szFile, "/root/");
   strcat(szFile, FILE_INFO_VERSION);
   fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( NULL != szOutputFile )
         strcpy(szOutputFile, szFile);
      return fd;
   }

   fd = fopen("ruby_ver.txt", "r");
   if ( NULL != fd )
   {
      if ( NULL != szOutputFile )
         strcpy(szOutputFile, szFile);
      return fd;
   }

   return fd;
}

u32 s_uBaseRubyVersion = 0;

void get_Ruby_BaseVersion(int* pMajor, int* pMinor)
{
   if ( NULL != pMajor )
      *pMajor = 0;
   if ( NULL != pMinor )
      *pMinor = 0;

   if ( 0 != s_uBaseRubyVersion )
   {
      if ( NULL != pMajor )
         *pMajor = (s_uBaseRubyVersion >> 8) & 0xFF;
      if ( NULL != pMinor )
         *pMinor = s_uBaseRubyVersion & 0xFF;
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   szFile[0] = 0;

   FILE* fd = try_open_base_version_file(szFile);
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Config] Failed to open base Ruby version file (%s).", szFile);
      return;
   }
   char szBuff[64];
   if ( 1 != fscanf(fd, "%s", szBuff) )
   {
      fclose(fd);
      log_softerror_and_alarm("[Config] Failed to read base Ruby version file (%s).", szFile);
      return;
   }
   fclose(fd);
   log_line("[Config] Read raw base Ruby version: [%s] from file (%s)", szBuff, szFile);

   for( int i=0; i<(int)strlen(szBuff); i++ )
   {
      if ( szBuff[i] == '.' )
      {
         szBuff[i] = 0;
         int iMajor = 0;
         int iMinor = 0;
         sscanf(szBuff, "%d", &iMajor);
         sscanf(&szBuff[i+1], "%d", &iMinor);
         s_uBaseRubyVersion = (((u32)iMajor) << 8) | ((u32)iMinor);
         log_line("[Config] Parsed base Ruby version: %u.%u", (s_uBaseRubyVersion>>8) & 0xFF, s_uBaseRubyVersion & 0xFF);

         if ( NULL != pMajor )
            *pMajor = (s_uBaseRubyVersion >> 8) & 0xFF;
         if ( NULL != pMinor )
            *pMinor = s_uBaseRubyVersion & 0xFF;

         return;
      }
   }
   log_softerror_and_alarm("[Config] Failed to parse base Ruby version from file (%s).", szFile);
}

void get_Ruby_UpdatedVersion(int* pMajor, int* pMinor)
{
   if ( NULL != pMajor )
      *pMajor = 0;
   if ( NULL != pMinor )
      *pMinor = 0;

   char szBuff[64];
   szBuff[0] = 0;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_INFO_LAST_UPDATE);
   
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return;

   if ( 1 != fscanf(fd, "%s", szBuff) )
   {
      fclose(fd);
      return;
   }
   fclose(fd);

   log_line("[Config] Read update Ruby version: [%s] from file (%s)", szBuff, szFile);

   for( int i=0; i<(int)strlen(szBuff); i++ )
   {
      if ( szBuff[i] == '.' )
      {
         szBuff[i] = 0;
         int iMajor = 0;
         int iMinor = 0;
         sscanf(szBuff, "%d", &iMajor);
         sscanf(&szBuff[i+1], "%d", &iMinor);
         log_line("[Config] Parsed updated Ruby version: %u.%u", iMajor, iMinor);

         if ( NULL != pMajor )
            *pMajor = iMajor;
         if ( NULL != pMinor )
            *pMinor = iMinor;

         return;
      }
   }
}
