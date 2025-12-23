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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_procs.h"


bool gbQuit = false;
char g_szUpdateZipFileFullPath[MAX_FILE_PATH_SIZE];
char g_szUpdateZipFileName[MAX_FILE_PATH_SIZE];
char g_szUpdateUnpackFolder[MAX_FILE_PATH_SIZE];
bool bIsOnyx = false;

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

void process_custom_commands_file()
{
   log_line("Try to execute custom commands in file: %s", FILE_UPDATE_CMD_LIST);

   if ( access( FILE_UPDATE_CMD_LIST, R_OK ) == -1 )
   {
      log_line("No custom commands file: %s", FILE_UPDATE_CMD_LIST);
      return;
   }
   FILE* fd = fopen(FILE_UPDATE_CMD_LIST, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Can't open custom commands file: %s", FILE_UPDATE_CMD_LIST);
      return;
   }

   char szComm[256];

   while ( true )
   {
      szComm[0] = 0;
      if ( 1 != fscanf(fd, "%s", szComm) )
         break;

      if ( 0 == strcmp(szComm, "cp") )
      {
         char szFileIn[256];
         char szFileOut[256];
         char szFolder[256];
         if ( 3 != fscanf(fd, "%s %s %s", szFileIn, szFolder, szFileOut) )
         {
            log_softerror_and_alarm("Invalid copy comand");
            continue;
         }
         char szCommand[1024];
         sprintf(szCommand, "cp -rf %s %s/%s", szFileIn, szFolder, szFileOut);
         //hw_execute_process_wait(szCommand);
         hw_execute_bash_command(szCommand, NULL);
      }
      if ( 0 == strcmp(szComm, "mv") )
      {
         char szFileIn[256];
         char szFileOut[256];
         char szFolder[256];
         if ( 3 != fscanf(fd, "%s %s %s", szFileIn, szFolder, szFileOut) )
         {
            log_softerror_and_alarm("Invalid copy comand");
            continue;
         }
         char szCommand[1024];
         sprintf(szCommand, "mv -f %s %s/%s", szFileIn, szFolder, szFileOut);
         hw_execute_process_wait(szCommand);
      }
      else if ( 0 == strcmp(szComm, "cmd") )
      {
         char * line = NULL;
         size_t len = 0;
         ssize_t readline;
         readline = getline(&line, &len, fd);
         if ( readline < 1 )
         {
            log_softerror_and_alarm("Invalid comand");
            continue;
         }        
         char szCommand[1024];
         sprintf(szCommand, "%s", line);
         len = strlen(szCommand)-1;
         while ( (len > 0) && (szCommand[len] == 10 || szCommand[len] == 13 || szCommand[len] == '\r' || szCommand[len] == '\t' || szCommand[len] == ' ' ) )
         {
            szCommand[len] = 0;
            len--;
         }
         log_line("Execute custom command: [%s]", szCommand);
         hw_execute_process_wait(szCommand);
      }
   }
      
   fclose(fd);
   log_line("Done executing custom commands from file: %s", FILE_UPDATE_CMD_LIST);

   // Do not delete the commands file, can be used to be sent to vehicle as on the fly archive.
   //sprintf(szComm, "rm -rf %s", FILE_UPDATE_CMD_LIST );
   //hw_execute_process_wait(szCommand);
}

long compute_file_sizes()
{
   long lSize = 0;
   long lTotalSize = 0;

   lSize = get_filesize("ruby_central");
   if ( -1 == lSize )
      return -1;    

   lTotalSize += lSize;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rt_station");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_update");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rt_vehicle");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_start");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_update");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rx_telemetry");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_tx_telemetry");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_tx_rc");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_i2c");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   return lTotalSize;
}

void _write_return_code(int iCode, const char* szText)
{
   char szComm[256];
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_UPDATE_CONTROLLER_PROGRESS);

   if ( access(szFile, R_OK) != -1 )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s", szFile);
      hw_execute_process_wait(szComm);
   }
   FILE* fd = fopen(szFile, "wb");
   if ( fd != NULL )
   {
      fprintf(fd, "%d\n", iCode);
      if ( (NULL != szText) && (0 != szText[0]) )
         fprintf(fd, "%s\n", szText);
      else
         fprintf(fd, " \n");
      fclose(fd);
      log_line("Wrote status code %d and status text: (%s)", iCode, (NULL != szText)?szText:"empty");
   }
   else
     log_softerror_and_alarm("Failed to write update result to file (%s)", szFile);

   // Write legacy code too for older controllers
   hw_execute_process_wait("chmod 777 tmp/tmp_update_result.txt");
   fd = fopen("tmp/tmp_update_result.txt", "wb");
   if ( fd != NULL )
   {
      fprintf(fd, "%d\n", iCode);
      fclose(fd);
   }
   else
     log_softerror_and_alarm("Failed to write update result to legacy file (tmp/tmp_update_result.txt)");
}

int _replace_runtime_binary_files()
{
   if ( (0 == g_szUpdateUnpackFolder[0]) )
   {
      log_line("Invalid function param for copy binary files.");
      _write_return_code(-10, "Invalid unpacked update");
      return -1;
   }

   log_line("Binaries versions before replacing:");
   char szOutput[4096];
   hw_execute_ruby_process_wait(NULL, "ruby_start", "-ver", szOutput, 1);
   log_line("ruby_start: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_rt_vehicle", "-ver", szOutput, 1);
   log_line("ruby_rt_vehicle: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_rt_station", "-ver", szOutput, 1);
   log_line("ruby_rt_station: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_central", "-ver", szOutput, 1);
   log_line("ruby_central: [%s]", szOutput);
 
   char szComm[MAX_FILE_PATH_SIZE];
   char szSrcBinariesFolder[MAX_FILE_PATH_SIZE];
   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   #endif
   #ifdef HW_PLATFORM_RADXA
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_OIPC);
   #endif

   log_line("Check for binary files in unzipped folder [%s] ...", szSrcBinariesFolder);

   char szFile[MAX_FILE_PATH_SIZE];
   bIsOnyx = false;
   strcpy(szFile, szSrcBinariesFolder);
   strcat(szFile, "onyxfpv_start");
   if ( access(szFile, R_OK) != -1 )
      bIsOnyx = true;

   // Check if ruby binaries are present in folder
   if ( ! bIsOnyx )
   {
      strcpy(szFile, szSrcBinariesFolder);
      strcat(szFile, "ruby_start");
      if ( access(szFile, R_OK) == -1 )
      {
         char szOutput[4096];
         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ls %s", g_szUpdateUnpackFolder);
         hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
         log_line("Content of tmp update folder:");
         log_line("[%s]", szOutput);
         log_line("Found zip archive with no valid ruby_start file in binaries folder. Ignoring it.");
         _write_return_code(-10, "Invalid update archive");
         return -1;
      }
   }


   #if defined (HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   if ( bIsOnyx )
   {
      hw_execute_bash_command("chmod 777 /root/.profile 2>/dev/null", NULL);
      hw_execute_bash_command("sed -i -e 's/ruby/onyxfpv/g' /root/.profile", NULL);
      hw_execute_bash_command("chmod 777 /root/.profile 2>/dev/null", NULL);
   }
   #endif

   /*
   log_line("Binaries version in the unzipped folder:");
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), ".%s%sruby_start -ver", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
   log_line("ruby_start: [%s]", szOutput);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), ".%s%/ruby_rt_vehicle -ver", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
   log_line("ruby_rt_vehicle: [%s]", szOutput);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), ".%s%sruby_rt_station -ver", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
   log_line("ruby_rt_station: [%s]", szOutput);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), ".%s%sruby_central -ver", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
   log_line("ruby_central: [%s]", szOutput);
   */

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ls %s",szSrcBinariesFolder);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
   log_line("Content of unpacked folder: [%s]", szOutput);

   log_line("Copying binary files from unzipped folder [%s] ...", szSrcBinariesFolder);

   if ( bIsOnyx )
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sonyxfpv_* %s 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   else
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sruby_* %s 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(200);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sstop* %s  2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(50);

   if ( bIsOnyx )
   {
      hw_execute_bash_command("chown root onyx* 2>/dev/null", NULL);
      hw_execute_bash_command("chgrp root onyx* 2>/dev/null", NULL);
      hw_execute_bash_command("chmod 777 onyx* 2>/dev/null", NULL);
      hardware_sleep_ms(100);
      log_line("Binaries versions after replacing:");
      hw_execute_ruby_process_wait(NULL, "onyxfpv_start", "-ver", szOutput, 1);
      log_line("onyxfpv_start: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "onyxfpv_router_s", "-ver", szOutput, 1);
      log_line("onyxfpv_router_s: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "onyxfpv_router_v", "-ver", szOutput, 1);
      log_line("onyxfpv_router_v: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "onyxfpv_central", "-ver", szOutput, 1);
      log_line("onyxfpv_central: [%s]", szOutput);
   }
   else
   {
      hw_execute_bash_command("chown root ruby* 2>/dev/null", NULL);
      hw_execute_bash_command("chgrp root ruby* 2>/dev/null", NULL);
      hw_execute_bash_command("chmod 777 ruby* 2>/dev/null", NULL);
      hardware_sleep_ms(100);
      log_line("Binaries versions after replacing:");
      hw_execute_ruby_process_wait(NULL, "ruby_start", "-ver", szOutput, 1);
      log_line("ruby_start: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_rt_vehicle", "-ver", szOutput, 1);
      log_line("ruby_rt_vehicle: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_rt_station", "-ver", szOutput, 1);
      log_line("ruby_rt_station: [%s]", szOutput);
      hw_execute_ruby_process_wait(NULL, "ruby_central", "-ver", szOutput, 1);
      log_line("ruby_central: [%s]", szOutput);
   }
 

   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sraspi* %s", szSrcBinariesFolder, FOLDER_BINARIES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   if ( access( "ruby_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);
   if ( access( "ruby_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye /usr/local/bin/veye_raspivid", NULL);
   if ( access( "onyxfpv_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf onyxfpv_capture_raspi /opt/vc/bin/raspivid", NULL);
   if ( access( "onyxfpv_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf onyxfpv_capture_veye /usr/local/bin/veye_raspivid", NULL);
   #endif

   return 0;
}

void _copy_libraries()
{
   #if defined (HW_PLATFORM_RADXA)
   int iMajor = 0;
   int iMinor = 0;
   get_Ruby_BaseVersion(&iMajor, &iMinor);
   if ( iMinor >= 10 )
      iMinor /= 10;

   if ( (iMajor > 11) || ((iMajor == 11) && (iMinor > 4)) )
   {
      log_line("No libraries update to do on Radxa as we are already on version: %d.%d", iMajor, iMinor);
      return;
   }
   log_line("Do libraries update on Radxa as we are on version: %d.%d", iMajor, iMinor);

   char szOutput[4096];
   szOutput[0] = 0;
   hw_execute_bash_command("find /lib/aarch64-linux-gnu/libSDL.so 2>/dev/null", szOutput);
   if ( NULL != strstr(szOutput, "libSDL.so") )
   {
      szOutput[0] = 0;
      hw_execute_bash_command("find /lib/aarch64-linux-gnu/libSDL2.so 2>/dev/null", szOutput);
      if ( NULL != strstr(szOutput, "libSDL2.so") )
      {
         log_line("SDL libraries are already present: [%s]", szOutput);
         return;
      }
   }

   char szComm[512];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%slibs/* /lib/aarch64-linux-gnu/ 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   hw_execute_bash_command(szComm, NULL);
   hw_execute_bash_command("chmod 777 /lib/aarch64-linux-gnu/libSDL*", NULL);
   hw_execute_bash_command("ln -s /lib/aarch64-linux-gnu/libSDL-1.2.so.0.11.4 /lib/aarch64-linux-gnu/libSDL.so 2>/dev/null", NULL);
   hw_execute_bash_command("ln -s /lib/aarch64-linux-gnu/libSDL-1.2.so.0.11.4 /lib/aarch64-linux-gnu/libSDL-1.2.so.0 2>/dev/null", NULL);
   hw_execute_bash_command("ln -s /lib/aarch64-linux-gnu/libSDL2-2.0.so.0.14.0 /lib/aarch64-linux-gnu/libSDL2.so 2>/dev/null", NULL);
   hw_execute_bash_command("ln -s /lib/aarch64-linux-gnu/libSDL2-2.0.so.0.14.0 /lib/aarch64-linux-gnu/libSDL2-2.0.so 2>/dev/null", NULL);
   hw_execute_bash_command("ln -s /lib/aarch64-linux-gnu/libSDL2-2.0.so.0.14.0 /lib/aarch64-linux-gnu/libSDL2-2.0.so.0 2>/dev/null", NULL);
   hw_execute_bash_command("chmod 777 /lib/aarch64-linux-gnu/libSDL*", NULL);
   log_line("Done updating Radxa libraries.");
   #endif
}

int _copy_update_binary_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy update binary files.");
      _write_return_code(-10, "Invalid unpacketd achive");
      return -1;
   }

   char szComm[MAX_FILE_PATH_SIZE];

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %sbin/", FOLDER_UPDATES);
   hw_execute_process_wait(szComm);
   
   sprintf(szComm, "mkdir -p %sbin/", FOLDER_UPDATES);
   hw_execute_process_wait(szComm);

   /*
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
   hw_execute_process_wait(szComm);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   hw_execute_process_wait(szComm);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
   hw_execute_process_wait(szComm);
   */
   
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sbin/* %sbin", g_szUpdateUnpackFolder, FOLDER_UPDATES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(500);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s* 2>/dev/null", FOLDER_UPDATES, SUBFOLDER_UPDATES_DRIVERS);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   _copy_libraries();

   hardware_sleep_ms(50);

   return 0;
}


int _copy_res_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy res files.");
      _write_return_code(-10, "Invalid unpacked archive");
      return -1;
   }

   char szComm[256];

   if ( bIsOnyx )
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sonyxfpv_update.log %sonyxfpv_update.log", g_szUpdateUnpackFolder, FOLDER_CONFIG);
   else
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s %s%s", g_szUpdateUnpackFolder, FILE_INFO_SHORT_LAST_UPDATE, FOLDER_CONFIG, FILE_INFO_LAST_UPDATE);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sres/* %sres/", g_szUpdateUnpackFolder, FOLDER_BINARIES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   return 0;
}

int _copy_plugin_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy plugin files.");
      _write_return_code(-10, "Invalid unpacked archive");
      return -1;
   }

   char szComm[MAX_FILE_PATH_SIZE];
   char szSrcPluginsFolder[MAX_FILE_PATH_SIZE];

   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   #endif
   #ifdef HW_PLATFORM_RADXA
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_OIPC);
   #endif

   log_line("Copying plugins files from source folder: (%s)", szSrcPluginsFolder);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s* %splugins/  2>/dev/null", szSrcPluginsFolder, FOLDER_BINARIES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("chmod 777 plugins/* 2>/dev/null", NULL);
   hw_execute_bash_command("chmod 777 plugins/osd/* 2>/dev/null", NULL);
   hw_execute_bash_command("chmod 777 plugins/core/* 2>/dev/null", NULL);
   return 0;
}


int _copy_config_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy config files.");
      _write_return_code(-10, "Invalid unpacked archive");
      return -1;
   }

   #ifdef HW_PLATFORM_RASPBERRY
   char szSourceFile[MAX_FILE_PATH_SIZE];
   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sruby_profile", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /root/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /root/.profile");
   }

   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sonyxfpv_profile", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /root/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /root/.profile");
   }

   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sruby_config.txt", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      hardware_mount_boot();
      hardware_sleep_ms(200);
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /boot/config.txt", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /boot/config.txt");
   }

   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sonyxfpv_config.txt", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      hardware_mount_boot();
      hardware_sleep_ms(200);
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /boot/config.txt", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /boot/config.txt");
   }
   #endif

   #if defined(HW_PLATFORM_RADXA)
   char szSourceFile[MAX_FILE_PATH_SIZE];
   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sruby_profile", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /home/radxa/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /home/radxa/.profile");
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /root/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /root/.profile");
   }

   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sonyxfpv_profile", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   if ( access( szSourceFile, R_OK ) != -1 )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /home/radxa/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /home/radxa/.profile");
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /root/.profile", szSourceFile);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_process_wait("chmod 777 /root/.profile");
   }
   #endif
   return 0;
}


int _copy_update_drivers()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy update drivers files.");
      _write_return_code(-10, "Invalid unpacked archive");
      return -1;
   }

   char szDrivers[MAX_FILE_PATH_SIZE];

   snprintf(szDrivers, sizeof(szDrivers)/sizeof(szDrivers[0]), "%s%s*", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_DRIVERS);
   
   char szComm[MAX_FILE_PATH_SIZE];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", FOLDER_DRIVERS);
   hw_execute_process_wait(szComm);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s 2>/dev/null", szDrivers, FOLDER_DRIVERS);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s* 2>/dev/null", FOLDER_DRIVERS);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   // Drivers are installed after reboot
   //char szOutput[2048];
   //szOutput[0] = 0;
   //snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "find %s.ko", szDrivers);
   //hw_execute_process(szComm, szOutput);
   //hw_execute_bash_command(szComm, szOutput);
   //log_line("Partial output of find: [%s]", szOutput);
   //if ( (0 < strlen(szOutput)) && (NULL != strstr(szOutput, ".ko")) )
   //   hardware_install_drivers(0);
   return 0;
}

bool _download_update(const char* szDownloadURL)
{
   char szDownloadOutputFile[MAX_FILE_PATH_SIZE];
   strcpy(szDownloadOutputFile, FOLDER_RUBY_TEMP);
   strcat(szDownloadOutputFile, "ruby_update_net.zip");

   char szComm[512];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szDownloadOutputFile);
   hw_execute_process_wait(szComm);

   log_line("Download URL: [%s]", szDownloadURL);
   log_line("Download file to: (%s)", szDownloadOutputFile);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "/usr/bin/wget --no-check-certificate -q %s -O %s", szDownloadURL, szDownloadOutputFile);
   log_line("Downloading update command: [%s]", szComm);
   //system(szComm);
   hw_execute_process_wait(szComm);
   log_line("Finished download.");

   long lSize = hardware_file_get_file_size(szDownloadOutputFile);
   log_line("Downloaded file size: %d", (int) lSize);
   if ( lSize < 100000 )
      return false;

   strncpy(g_szUpdateZipFileFullPath, szDownloadOutputFile, MAX_FILE_PATH_SIZE-1);
   strncpy(g_szUpdateZipFileName, strstr(szDownloadOutputFile, "ruby_update"), MAX_FILE_PATH_SIZE-1);

   log_line("Will use zip archive full path: [%s]", g_szUpdateZipFileFullPath);
   log_line("Will use zip archive filename: [%s]", g_szUpdateZipFileName);

   return true;
}

bool _find_update_partial_zip_filename(const char* szPartialName, char* szOutputFulLPath, char* szOutputFileName)
{
   if ( (NULL == szPartialName) || (0 == szPartialName[0]) || (NULL == szOutputFulLPath) || (NULL == szOutputFileName) )
      return false;

   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "find %s%s*.zip", FOLDER_USB_MOUNT, szPartialName);
   hw_execute_bash_command(szComm, szOutput);
   log_line("Partial output of finding [%s]: [%s]", szPartialName, szOutput);

   if ( (0 == strlen(szOutput)) || (NULL == strstr(szOutput, szPartialName)) || (NULL != strstr(szOutput, "No such")) )
      return false;
  
   int iLen = strlen(szOutput);
   for( int i=0; i<iLen; i++ )
   {
      if ( (szOutput[i] == ' ') || (szOutput[i] == 10) || (szOutput[i] == 13) )
      {
         szOutput[i] = 0;
         break;
      }
   }
   strncpy(szOutputFulLPath, szOutput, MAX_FILE_PATH_SIZE-1);
   if ( NULL != strstr(szOutput, szPartialName) )
      strncpy(szOutputFileName, strstr(szOutput, szPartialName), MAX_FILE_PATH_SIZE-1);

   log_line("Found zip archive full path: [%s]", szOutputFulLPath);
   log_line("Found zip archive filename: [%s]", szOutputFileName);

   if ( (0 == szOutputFileName[0]) || (0 == szOutputFulLPath[0]) || (strlen(szOutputFileName) < 6) || (NULL == strstr(szOutputFileName, "update")) )
      return false;
   return true;
}

bool _find_update_zip_file()
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];

   g_szUpdateZipFileFullPath[0] = 0;
   g_szUpdateZipFileName[0] = 0;

   if ( ! _find_update_partial_zip_filename("onyxfpv_update", g_szUpdateZipFileFullPath, g_szUpdateZipFileName) )
   {
      g_szUpdateZipFileFullPath[0] = 0;
      g_szUpdateZipFileName[0] = 0;
      _find_update_partial_zip_filename("ruby_update", g_szUpdateZipFileFullPath, g_szUpdateZipFileName);
   }

   if ( (0 == g_szUpdateZipFileName[0]) || (0 == g_szUpdateZipFileFullPath[0]) || (strlen(g_szUpdateZipFileName) < 6) || (NULL == strstr(g_szUpdateZipFileName, "update")) )
      return false;
   return true;
}

void _step_copy_and_extract_zip()
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[48000];

   //-----------------------------------------------------
   // Begin - Copy update zip file to updates folder

   sprintf(szComm, "mkdir -p %s", FOLDER_UPDATES);
   hw_execute_process_wait(szComm);

   sprintf(szComm, "chmod 777 %s 2>/dev/null", FOLDER_UPDATES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);
   
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s", g_szUpdateZipFileFullPath, FOLDER_UPDATES);
   //hw_execute_process_wait(szComm);
   hw_execute_bash_command(szComm, NULL);

   // End - Copy update zip file to updates folder
   //------------------------------------------------------

   //------------------------------------------------------
   // Begin - Extract archive to a temp folder
   
   strcpy(g_szUpdateUnpackFolder, FOLDER_RUBY_TEMP);
   strcat(g_szUpdateUnpackFolder, "tmpUpdate/");

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", g_szUpdateUnpackFolder);
   hw_execute_process_wait(szComm);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s 2>/dev/null", g_szUpdateUnpackFolder);
   hw_execute_process_wait(szComm);
   if ( 0 < strlen(g_szUpdateUnpackFolder) )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s/*", g_szUpdateUnpackFolder);
      //hw_execute_process_wait(szComm);
      hw_execute_bash_command(szComm, NULL);
   }

   hardware_sleep_ms(100);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "unzip %s%s -d %s", FOLDER_UPDATES, g_szUpdateZipFileName, g_szUpdateUnpackFolder);
   //hw_execute_process_wait(szComm);
   hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));

   log_line("Done extracting update archive to folder: (%s)", g_szUpdateUnpackFolder);

   log_line("Unzip output size: %d bytes", strlen(szOutput));
   szOutput[512] = 0;
   log_line("Unzip output: [%s]", szOutput);

   // End - Extract archive to a temp folder
   //------------------------------------------------------
}

bool _find_update_info_file()
{
   char szFile[MAX_FILE_PATH_SIZE];
   char szComm[MAX_FILE_PATH_SIZE];

   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", g_szUpdateUnpackFolder, FILE_INFO_SHORT_LAST_UPDATE);
   if( access( szFile, R_OK ) == -1 )
   {
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", g_szUpdateUnpackFolder, "onyxfpv_update.log");
      if( access( szFile, R_OK ) == -1 )
      {
         char szOutput[4096];
         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ls %s", g_szUpdateUnpackFolder);
         hw_execute_process(szComm, 0, szOutput, sizeof(szOutput)/sizeof(szOutput[0]));
         log_line("Content of tmp update folder:");
         log_line("[%s]", szOutput);
         log_line("Found zip archive with no valid update info file (missing update file: [%s]). Ignoring it.", szFile);
         _write_return_code(-10, "Missing update info from the update archive");
         return false;
      }
   }
   log_line("Found update info file in zip update (%s)", g_szUpdateZipFileName);
   return true;
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
   
   log_init("RubyUpdateWorker");

   if ( argc > 1 )
      log_line("Update parameter: (%s)", argv[argc-1]);

   hardware_detectBoardAndSystemType();

   hardware_sleep_ms(500);

   char szComm[MAX_FILE_PATH_SIZE];

   g_szUpdateZipFileFullPath[0] = 0;
   g_szUpdateZipFileName[0] = 0;
   g_szUpdateUnpackFolder[0] = 0;

   if ( (argc > 1) && (argv[argc-1][0] != 0) )
   {
      if ( ! _download_update(argv[argc-1]) )
      {
         log_softerror_and_alarm("Failed to download update from internet. Update url: (%s)", argv[argc-1]);
         _write_return_code(-11, "Failed to download the update.");
         return -1;       
      }
   }
   else if ( ! _find_update_zip_file() )
   {
      log_line("There is no update update archive on the USB stick.");

      _write_return_code(-1, "No update found");
      return -1;
   }

   _write_return_code(0, "Unpacking update");

   hardware_sleep_ms(300);
   _step_copy_and_extract_zip();
   hardware_sleep_ms(300);
  
   _write_return_code(0, "Checking update content");

   if ( ! _find_update_info_file() )
      return -1;

   _write_return_code(0, "Updating software");

   g_TimeNow = get_current_timestamp_ms();

   if ( _replace_runtime_binary_files() < 0 )
      return -1;

   hardware_sleep_ms(300);
   if ( _copy_update_binary_files() < 0 )
      return -1;
   hardware_sleep_ms(300);

   _write_return_code(0, "Updating resources");

   if ( _copy_res_files() < 0 )
      return -1;

   _copy_plugin_files();
   _copy_config_files();

   _write_return_code(0, "Updating drivers");

   _copy_update_drivers();

   g_TimeNow = get_current_timestamp_ms();

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", g_szUpdateUnpackFolder);
   hw_execute_process_wait(szComm);

   process_custom_commands_file();

   _write_return_code(0, "Executing update pre config");

   if ( bIsOnyx )
   {
      if( access( "onyxfpv_update", R_OK ) != -1 )
         hw_execute_process_wait("./onyxfpv_update -pre");
   }
   else if( access( "ruby_update", R_OK ) != -1 )
      hw_execute_process_wait("./ruby_update -pre");

   _write_return_code(0, "Finishing up");

   log_line("Update controller finished.");

   _write_return_code(0, "Finished Successfully. Please wait.");
   for( int i=0; i<5; i++ )
      hardware_sleep_ms(300);
   _write_return_code(1, "Completed. Please wait.");
   hw_execute_process_wait("sync");
   log_line("Process finished.");
   return (0);
} 