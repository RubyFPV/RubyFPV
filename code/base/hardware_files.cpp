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

#include "base.h"
#include "config.h"
#include "hardware_files.h"
#include "hardware_procs.h"
#include <pthread.h>
#include <ctype.h>

int s_iUSBMounted = 0;
char s_szUSBMountName[64];

pthread_t s_pThreadGetFreeSpaceAsync;
static int s_iGetFreeSpaceAsyncResultValueKb = -1;

bool hardware_file_check_and_fix_access(char* szFullFileName)
{
   if ( (NULL == szFullFileName) || (0 == szFullFileName[0]) )
      return false;
   if ( access(szFullFileName, F_OK) == -1 )
      return false;

   bool bUpdate = false;
   if ( access(szFullFileName, R_OK) == -1 )
      bUpdate = true;
   if ( access(szFullFileName, X_OK) == -1 )
      bUpdate = true;

   if ( bUpdate )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s", szFullFileName);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( access(szFullFileName, R_OK) != -1 )
   if ( access(szFullFileName, X_OK) != -1 )
      return true;
   return false;
}

long hardware_file_get_file_size(const char* szFullFileName)
{
   if ( (NULL == szFullFileName) || (0 == szFullFileName[0]) )
      return -1;
   if ( access(szFullFileName, R_OK) == -1 )
      return -2;

   long lSize = 0;
   FILE* fd = fopen(szFullFileName, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSize = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
      return lSize;
   }
   return -3;
}

void hardware_file_replace_extension(const char* szFile, const char* szNewExtension)
{
   if ( (NULL == szFile) || (NULL == szNewExtension) || (0 == szFile[0]) || (0 == szNewExtension[0]) )
      return;
   // Code modifies input filename buffer! Enough room is assumed.

   int iLen = strlen(szFile)-1;
   while ( (iLen > 0) && (szFile[iLen] != '.') )
      iLen--;

   if ( iLen <= 0 )
      return;

   char* pFileName = (char*)szFile;
   pFileName[iLen] = '.';
   pFileName[iLen+1] = 0;
   strcat(pFileName, szNewExtension);
}

void hardware_files_check_config_folder()
{
   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s* 2>/dev/null", FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s* 2>/dev/null", FOLDER_CONFIG_MODELS);
   hw_execute_bash_command(szComm, NULL); 
}

void hardware_files_init()
{
   s_szUSBMountName[0] = 0;
}

void hardware_mount_root()
{
   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("sudo mount -o remount,rw /", NULL);
   #endif

   #if defined(HW_PLATFORM_RADXA)
   hw_execute_bash_command("sudo mount -o remount,rw /", NULL);
   #endif
}

void hardware_mount_boot()
{
   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("sudo mount -o remount,rw /boot", NULL);
   #endif
}

int hardware_get_free_space_kb()
{
   char szOutput[2048];
   szOutput[0] = 0;

   #if defined(HW_PLATFORM_RASPBERRY)
   if ( 1 != hw_execute_bash_command_raw("df . | grep root", szOutput) )
      return -1;
   #endif
   #if defined(HW_PLATFORM_RADXA)
   if ( 1 != hw_execute_bash_command_raw("df / | grep dev/", szOutput) )
      return -1;
   #endif
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   //if ( 1 != hw_execute_bash_command_raw("df . | grep overlay 2>/dev/null", szOutput) )
   //   return -1;
   //if ( strlen(szOutput) < 10 )
   if ( 1 != hw_execute_bash_command_raw("df /tmp | grep tmpfs 2>/dev/null", szOutput) )
      return -1;
   #endif

   if ( strlen(szOutput) < 10 )
      return -1;
   char szTemp[1024];
   long lb = 0, lu = 0, lf = 0;
   sscanf(szOutput, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
   return (int)lf;
}

void* _thread_get_free_space_async(void *argument)
{
   sched_yield();
   hw_log_current_thread_attributes("get free space");
   s_iGetFreeSpaceAsyncResultValueKb = -1;
   hardware_sleep_ms(100);
   int iFreeSpaceKb = hardware_get_free_space_kb();
   log_line("Done getting free space async");
   s_iGetFreeSpaceAsyncResultValueKb = iFreeSpaceKb;
   return NULL;
}

int hardware_get_free_space_kb_async(int iCPUCore)
{
   s_iGetFreeSpaceAsyncResultValueKb = -1;
   pthread_attr_t attr;
   hw_init_worker_thread_attrs(&attr, iCPUCore, -1, SCHED_OTHER, 0, "get free space async");
   if ( 0 != pthread_create(&s_pThreadGetFreeSpaceAsync, &attr, &_thread_get_free_space_async, NULL) )
   {
      pthread_attr_destroy(&attr);
      return -1;
   }
   pthread_attr_destroy(&attr);
   return 0;
}

int hardware_has_finished_get_free_space_kb_async()
{
   return s_iGetFreeSpaceAsyncResultValueKb;
}


int hardware_try_mount_usb()
{
   if ( s_iUSBMounted )
      return 1;

   log_line("[Hardware] Try mounting USB storage...");

   char szBuff[128];
   char szCommand[128];
   char szOutput[2048];
   s_iUSBMounted = 0;
   sprintf(szCommand, "mkdir -p %s", FOLDER_USB_MOUNT); 
   hw_execute_bash_command(szCommand, NULL);

   if ( 0 < strlen(FOLDER_USB_MOUNT) )
   {
      sprintf(szCommand, "rm -rf %s*", FOLDER_USB_MOUNT); 
      hw_execute_bash_command(szCommand, NULL);
   }
   hw_execute_bash_command_raw("lsblk -l -n -o NAME | grep sd 2>&1", szOutput);
   if ( 0 == szOutput[0] )
   {
      log_softerror_and_alarm("[Hardware] USB memory stick could NOT be mounted! Failed to iterate block devices.");
      return 0;
   }
   log_line("[Hardware] Block devices found: [%s]", szOutput);
   char* szToken = strtok(szOutput, " \n\r");
   if ( NULL == szToken )
   {
      log_softerror_and_alarm("[Hardware] USB memory stick could NOT be mounted! Failed to iterate block devices result.");
      return 0;    
   }

   int iFoundUSBDevice = 0;
   while ( NULL != szToken )
   {
       int iValid = 0;
       if ( NULL != strstr(szToken, "sda") )
          iValid = 1;
       if ( NULL != strstr(szToken, "sdb") )
          iValid = 1;
       if ( NULL != strstr(szToken, "sdc") )
          iValid = 1;
       if ( NULL != strstr(szToken, "sdd") )
          iValid = 1;

       if ( strlen(szToken) < 4 )
          iValid = 0;

       if ( iValid )
       if ( ! isdigit(szToken[strlen(szToken)-1]) )
          iValid = 0;

       if ( 0 == iValid )
       {
          log_line("[Hardware] Device [%s] is not valid USB device, skipping it.", szToken);
          szToken = strtok(NULL, " \n\r");
          continue;
       }
       log_line("[Hardware] Device [%s] is valid.", szToken);
       iFoundUSBDevice = 1;
       break;
   }

   if ( (0 == iFoundUSBDevice) || (NULL == szToken) )
   {
      log_softerror_and_alarm("[Hardware] USB memory stick could NOT be mounted! No USB block devices found.");
      return 0;    
   }

   log_line("[Hardware] Found USB block device: [%s], Mounting it...", szToken );

   sprintf(szBuff, "/dev/%s", szToken);
   removeTrailingNewLines(szBuff);
   sprintf(szCommand, "mount /dev/%s %s 2>&1", szToken, FOLDER_USB_MOUNT);
   szOutput[0] = 0;
   if( access( szBuff, R_OK ) != -1 )
      hw_execute_bash_command_timeout(szCommand, szOutput, 6000);
   else
   {
      log_softerror_and_alarm("[Hardware] Can't access USB device block [/dev/%s]. USB not mounted.", szToken);
      return 0;
   }

   log_line("[Hardware] USB mount result: [%s]", szOutput);
   if ( (0 < strlen(szOutput)) &&
        ((NULL != strstr(szOutput, "Fixing")) || (NULL != strstr(szOutput, "unclean")) || (NULL != strstr(szOutput, "closed"))) )
   {
      s_iUSBMounted = 0;
      log_line("[Hardware] Can't mount USB stick. Could retry.");

      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "umount %s 2>/dev/null", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, NULL);

      return -1;
   }
   if ( 5 < strlen(szOutput) )
   {
      s_iUSBMounted = 0;
      log_line("[Hardware] Can't mount USB stick");

      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "umount %s 2>/dev/null", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, NULL);
      return -2;
   }

   s_iUSBMounted = 1;
   log_line("[Hardware] USB memory stick was mounted.");


   strcpy(s_szUSBMountName, "UnknownUSB");
   szOutput[0] = 0;
   sprintf(szCommand, "ls -Al /dev/disk/by-label | grep %s", szToken);
   hw_execute_bash_command_raw(szCommand, szOutput);

   if ( 0 != szOutput[0] )
   {
      int iCountChars = 0;
      char* pTmp = &szOutput[0];
      while ((*pTmp) != 0 )
      {
         if ( (*pTmp) < 32 )
            *pTmp = ' ';
         pTmp++;
         iCountChars++;
      }

      log_line("[Hardware] USB stick name mapping: [%s], %d chars", szOutput, iCountChars);
      szToken = strstr(szOutput, "->");
      if ( NULL != szToken )
      {
         char* p = szToken;
         p--;
         *p = 0;
         int iCount = 0;
         while ((p != szOutput) && (iCount <= 48) )
         {
            if ( *p == ' ' )
               break;
            p--;
            iCount++;
         }
         if ( 0 < strlen(p) )
         {
            char szTmp[64];
            strncpy(szTmp, p, sizeof(szTmp)/sizeof(szTmp[0]));
            szTmp[48] = 0;
            for( int i=0; i<(int)strlen(szTmp); i++ )
            {
               if ( szTmp[i] == '\\' )
               {
                  szTmp[i] = ' ';
                  if ( (szTmp[i+1] == 'n') || (szTmp[i+1] == 't') )
                     szTmp[i+1] = ' ';
                  if ( szTmp[i+1] == 'x' )
                  {
                     szTmp[i+1] = ' ';
                     if ( isdigit(szTmp[i+2]) )
                        szTmp[i+2] = ' ';
                     if ( isdigit(szTmp[i+3]) )
                        szTmp[i+3] = ' ';
                  }
               }
            }
            strcpy(s_szUSBMountName, szTmp);
         }
      }
   }

   // Cleanup usb name
   strcpy(szBuff, s_szUSBMountName);

   char* pStart = szBuff;
   while ((*pStart) == ' ')
   {
      pStart++;
   }
   strcpy(s_szUSBMountName, pStart);
   
   log_line("[Hardware] USB stick name [%s]", s_szUSBMountName);

   return s_iUSBMounted;
}

void hardware_unmount_usb()
{
   if ( 0 == s_iUSBMounted )
      return;
   log_line("[Hardware] Unmounting USB folder (%s)...", FOLDER_USB_MOUNT);
   char szCommand[128];
   snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "umount %s 2>/dev/null", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szCommand, NULL);

   hardware_sleep_ms(200);

   s_iUSBMounted = 0;
   log_line("[Hardware] USB memory stick [%s] has been unmounted!", s_szUSBMountName );
   s_szUSBMountName[0] = 0;
}

char* hardware_get_mounted_usb_name()
{
   static const char* s_szUSBNotMountedLabel = "Not Mounted";
   if ( 0 == s_iUSBMounted )
      return (char*)s_szUSBNotMountedLabel;
   return s_szUSBMountName;
}

