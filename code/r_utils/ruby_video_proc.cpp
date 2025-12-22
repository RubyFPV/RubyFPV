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
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_files.h"
#include "../base/models.h"
#include "../base/flags_video.h"
#include "../common/string_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

Model* g_pCurrentModel = NULL;
int g_iBootCount = 0;

bool gbQuit = false;
int niceValue = 10;

void _store_error(const char* szErrorMsg)
{
   if ( (NULL == szErrorMsg) || (0 == szErrorMsg[0]) )
      return;
   char szFileError[MAX_FILE_PATH_SIZE];
   strcpy(szFileError, FOLDER_RUBY_TEMP);
   strcat(szFileError, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
   FILE* fd = fopen(szFileError, "a");
   if ( NULL != fd )
   {
      fprintf(fd, "%s\n", szErrorMsg);
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to store error in file: [%s], [%s]", szFileError, szErrorMsg);
}

bool store_video()
{
   char szFileInInfo[MAX_FILE_PATH_SIZE];
   char szFileInVideo[MAX_FILE_PATH_SIZE];
   char szComm[1024];
   //char szCommOutput[4096];

   int fps = 0;
   int length = 0;
   int width, height;
   int iVideoType = VIDEO_TYPE_H264;
   int iFC = 0;
   int iOSDFont = 0;
   int iMSPCols = 0;
   int iMSPRows = 0;

   strcpy(szFileInInfo, FOLDER_RUBY_TEMP);
   strcat(szFileInInfo, FILE_TEMP_VIDEO_FILE_INFO);
   FILE* fd = fopen(szFileInInfo, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open video recording info file: %s", szFileInInfo);
      _store_error("Failed to open video info file");
      return false;
   }
   if ( 3 != fscanf(fd, "%s %d %d", szFileInVideo, &fps, &length) )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to read video recording info file 1/2: %s", szFileInInfo);
      _store_error("Failed to read video recording info file. Invalid file.");
      return false;
   }
   if ( 2 != fscanf(fd, "%d %d", &width, &height) )
   {
      log_softerror_and_alarm("Failed to read video recording info file 2/2: %s", szFileInInfo);
      fclose(fd);
      _store_error( "Failed to read video recording info file. Invalid file.");
      return false;
   }
   if ( 1 != fscanf(fd, "%d", &iVideoType) )
   {
      log_softerror_and_alarm("Failed to read video recording info file video type from %s", szFileInInfo);
      iVideoType = VIDEO_TYPE_H264;
   }
   if ( 4 != fscanf(fd, "%d %d %d %d", &iFC, &iOSDFont, &iMSPCols, &iMSPRows) )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to read video recording info file 3/2: %s", szFileInInfo);
      _store_error("Failed to read video recording info file. Invalid file.");
      return false;
   }
   fclose(fd);

   log_line("Read video info file: fps: %d, length: %d, w x h: %d x %d, type: %d, in video file: [%s]",
      fps, length, width, height, iVideoType, szFileInVideo);

   if ( NULL != strstr(szFileInVideo, FOLDER_TEMP_VIDEO_MEM) )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "nice -n %d mv %s %s%s", niceValue, szFileInVideo, FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
      hw_execute_bash_command(szComm, NULL);

      strcpy(szFileInVideo, FOLDER_RUBY_TEMP);
      strcat(szFileInVideo, FILE_TEMP_VIDEO_FILE);
   }

   long lSizeVideo = 0;
   fd = fopen(szFileInVideo, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSizeVideo = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
   }

   if ( lSizeVideo < 100000 )
   {
      log_softerror_and_alarm("Input video file %s is too small (%d bytes)", szFileInVideo, (int)lSizeVideo);
      _store_error( "Invalid video file. Too small. Discard it.");
      return false;
   }

   if ( length < 3 )
   {
      log_softerror_and_alarm("Input video file %s is too small (%d sec)", szFileInVideo, (int)length);
      _store_error( "Invalid video file. Too short. Discard it.");
      return false;
   }

   log_line("Video input file size: %d bytes", (int) lSizeVideo);

   char szOutFileVideo[MAX_FILE_PATH_SIZE];
   char szOutFileInfo[MAX_FILE_PATH_SIZE];
   char szFullOutFileInfo[MAX_FILE_PATH_SIZE];
   szOutFileVideo[0] = 0;
   szOutFileInfo[0] = 0;

   char vehicle_name[MAX_VEHICLE_NAME_LENGTH+1];

   strcpy(vehicle_name, "none");
   if ( NULL != g_pCurrentModel )
      strcpy(vehicle_name, g_pCurrentModel->vehicle_name);
   if ( 0 == strlen(vehicle_name) || (1 == strlen(vehicle_name) && vehicle_name[0] == ' ') )
      strcpy(vehicle_name, "none");

   str_sanitize_filename(vehicle_name);

   u32 timeNow = get_current_timestamp_ms();
   sprintf(szOutFileInfo, FILE_FORMAT_VIDEO_INFO, vehicle_name, g_iBootCount, (int)timeNow/1000, (int)timeNow%1000 );

   strncpy(szOutFileVideo, szOutFileInfo, sizeof(szOutFileVideo)/sizeof(szOutFileVideo[0]));
   if ( iVideoType == VIDEO_TYPE_H265 )
      hardware_file_replace_extension(szOutFileVideo, "h265");
   else
      hardware_file_replace_extension(szOutFileVideo, "h264");
   
   snprintf(szFullOutFileInfo, sizeof(szFullOutFileInfo)/sizeof(szFullOutFileInfo[0]), "%s%s", FOLDER_MEDIA, szOutFileInfo);

   log_line("Built output file names: out video file: [%s], out info file: [%s]", szOutFileVideo, szOutFileInfo);
   log_line("Output info file: [%s]", szFullOutFileInfo);
   
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);

   fd = fopen(szFullOutFileInfo, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to write video output info file: %s", szFullOutFileInfo);
      _store_error( "Failed to write video output information file.");
      return false;
   }
   fprintf(fd, "%s\n", szOutFileVideo);
   fprintf(fd, "%d %d\n", fps, length );
   fprintf(fd, "%d %d\n", width, height );
   fprintf(fd, "%d\n", iVideoType );
   fprintf(fd, "%d %d %d %d\n", iFC, iOSDFont, iMSPCols, iMSPRows);
   fclose(fd);

   log_line("Moving temp video file [%s] to: [%s%s]", szFileInVideo, FOLDER_MEDIA, szOutFileVideo);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "nice -n %d mv %s %s%s", niceValue, szFileInVideo, FOLDER_MEDIA, szOutFileVideo);
   hw_execute_bash_command(szComm, NULL);
   //launch_set_proc_priority("cp", 10,0,1);

   hardware_file_replace_extension(szOutFileVideo, "osd");
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mv %s%s %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_OSD, FOLDER_MEDIA, szOutFileVideo);
   hw_execute_bash_command(szComm, NULL);

   hardware_file_replace_extension(szOutFileVideo, "srt");
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mv %s%s %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_SRT, FOLDER_MEDIA, szOutFileVideo);
   hw_execute_bash_command(szComm, NULL);

   return true;
}


bool process_video(char* szFileInfo, char* szFileOut)
{
   char szFileInVideo[MAX_FILE_PATH_SIZE];
   char szComm[1024];
   int fps = 0;
   int length = 0;
   int iWidth, iHeight;
   int iVideoType = VIDEO_TYPE_H264;

   FILE* fd = fopen(szFileInfo, "r");
   if ( NULL == fd )
      return false;
   if ( 3 != fscanf(fd, "%s %d %d", szFileInVideo, &fps, &length) )
   {
      log_softerror_and_alarm("Failed to read video info1 from info file: %s", szFileInfo);
      fclose(fd);
      return false;
   }
   if ( 2 != fscanf(fd, "%d %d", &iWidth, &iHeight) )
   {
       log_softerror_and_alarm("Failed to read video info2 from info file: %s", szFileInfo);
       fclose(fd);
       return false;
   }
   if ( 1 != fscanf(fd, "%d",&iVideoType) )
   {
       log_softerror_and_alarm("Failed to read video type from info file: %s", szFileInfo);
       iVideoType = VIDEO_TYPE_H264;
   }
   fclose(fd);

   log_line("Processing video file: %s, fps: %d, length: %d seconds, video type: %d", szFileInVideo, fps, length, iVideoType);
   if ( length < 2 )
   {
      log_softerror_and_alarm("Empty video file (less than 3 seconds) received for processing. Ignoring it.");
      return true;
   }

   // Convert input file to output file
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ffmpeg -framerate %d -y -i %s%s -c:v copy %s 2>&1 1>/dev/null", fps, FOLDER_MEDIA, szFileInVideo, szFileOut);
   log_line("Execute conversion: %s", szComm);
   //hw_execute_bash_command(szComm, NULL);
   //launcher_set_proc_priority("ffmpeg", 10,0,1);
   system(szComm);
   log_line("Finished processing video to mp4: %s", szFileOut);

   long lSizeVideo = 0;
   fd = fopen(szFileOut, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSizeVideo = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
   }
   log_line("Output video file [%s] size: %d bytes", szFileOut, (int)lSizeVideo);
   return true;
}


void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyVideoProcessor");

   hardware_detectBoardAndSystemType();

   g_pCurrentModel = new Model();
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! g_pCurrentModel->loadFromFile(szFile) )
   {
      log_softerror_and_alarm("Failed to load current model.");
      g_pCurrentModel = new Model();
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &g_iBootCount) )
         g_iBootCount = 0;
      fclose(fd);
   }


   char szFileInfo[MAX_FILE_PATH_SIZE];
   char szFileOut[MAX_FILE_PATH_SIZE];
   szFileInfo[0] = 0;
   szFileOut[0] = 0;

   bool bStoreOnly = true;
   // Default, when no params:
   // Just store the temporary recording to media folder

   if ( argc >= 2 )
   {
      strncpy(szFileInfo, argv[1], sizeof(szFileInfo)/sizeof(szFileInfo[0]));
      strncpy(szFileOut, argv[2], sizeof(szFileOut)/sizeof(szFileOut[0]));
      bStoreOnly = false;
   }

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      log_enable_stdout();
   
   char szComm[1024];
   sprintf(szComm, "chmod 777 %s 2>&1 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s* 2>&1 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   
   if ( bStoreOnly )
   {
      log_line("Processing input video file to store it to media folder...");
      store_video();
      log_line("Remove temporary recording files...");

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_OSD);
      hw_execute_bash_command(szComm, NULL );
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_SRT);
      hw_execute_bash_command(szComm, NULL );

   }
   else
   {
      log_line("Processing info file %s to output video file %s ...", szFileInfo, szFileOut);
      process_video(szFileInfo, szFileOut);
   }

   sprintf(szComm, "chmod 777 %s 2>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   hw_execute_bash_command("sync", NULL);
   log_line("Finished processing video file.");
   return 0;
} 