#include "hdmi.h"
#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_files.h"
#include "hardware_procs.h"
#include <errno.h>
#include <unistd.h>
#if defined(HW_PLATFORM_RADXA)
#include <linux/videodev2.h>
#include <rockchip/rk_mpi.h>

extern "C" {
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h> 
}
#endif

#define MAX_HDMI_RESOLUTIONS 30
#define MAX_RESOLUTION_REFRESH_RATES_COUNT 10
#define HDMI_ASPECT_MODE_4_3 0
#define HDMI_ASPECT_MODE_16_9 1
#define HDMI_ASPECT_MODE_16_10 2
#define HDMI_ASPECT_MODE_21_9 3

void hardware_mount_root();

int s_nHDMI_CurrentResolutionIndex = -1;
int s_nHDMI_CurrentResolutionRefreshIndex = -1;
int s_nHDMI_ResolutionCount = 0;
int s_nHDMI_ResolutionWidth[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionHeight[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionRefreshCount[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionRefresh[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionGroup[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionMode[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionAspect[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];

void _hdmi_add_resolution(int group, int mode, int w, int h, int refresh_rate, int aspect)
{
   if ( s_nHDMI_ResolutionCount >= MAX_HDMI_RESOLUTIONS )
      return;

   int index = -1;

   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      if ( s_nHDMI_ResolutionWidth[i] == w && s_nHDMI_ResolutionHeight[i] == h )
      {
         index = i;
         break;
      }
   }

   if ( index == -1 )
   {
      index = s_nHDMI_ResolutionCount;
      s_nHDMI_ResolutionWidth[index] = w;
      s_nHDMI_ResolutionHeight[index] = h;
      s_nHDMI_ResolutionRefreshCount[index] = 0;
      s_nHDMI_ResolutionCount++;
   }

   int indexR = -1;

   for( int i=0; i<s_nHDMI_ResolutionRefreshCount[index]; i++ )
   {
      if ( s_nHDMI_ResolutionRefresh[index][i] == refresh_rate )
      {
         indexR = i;
         break;
      }
   }

   if ( indexR != -1 )
      return;
   if ( s_nHDMI_ResolutionRefreshCount[index] >= MAX_RESOLUTION_REFRESH_RATES_COUNT )
      return;

   indexR = s_nHDMI_ResolutionRefreshCount[index];
   s_nHDMI_ResolutionRefreshCount[index]++;

   s_nHDMI_ResolutionRefresh[index][indexR] = refresh_rate;
   s_nHDMI_ResolutionGroup[index][indexR] = group;
   s_nHDMI_ResolutionMode[index][indexR] = mode;
   s_nHDMI_ResolutionAspect[index][indexR] = aspect;
}

int _hdmi_detect_current_mode()
{
   log_line("[HDMI] Detecting current HDMI mode...");

   #if defined (HW_PLATFORM_RASPBERRY)
   char szBuff[1024];
   int hgroup = 0;
   int hmode = 0;
   szBuff[0] = 0;
   hw_execute_bash_command_silent("cat /boot/config.txt | grep hdmi_group | sed -r 's/[=]+//g' | sed -r 's/[hdmi_group]+//g'", szBuff);
   if ( 0 != szBuff[0] )
      sscanf(szBuff, "%d", &hgroup);

   szBuff[0] = 0;   
   hw_execute_bash_command_silent("cat /boot/config.txt | grep hdmi_mode | sed -r 's/[=]+//g' | sed -r 's/[hdmi_mode]+//g'", szBuff);
   if ( 0 != szBuff[0] )
      sscanf(szBuff, "%d", &hmode);

   if ( (hgroup <= 0) || (hmode <= 0) )
   {
      log_softerror_and_alarm("[HDMI] Failed to detect current HDMI mode (g:%d, m:%d)", hgroup, hmode);
      return -1;
   }
   if ( (hgroup != 1) && (hgroup != 2) )
   {
      log_softerror_and_alarm("[HDMI] Failed to detect current HDMI mode. (g:%d, m:%d)", hgroup, hmode);
      return -1;
   }

   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      for( int k=0; k<s_nHDMI_ResolutionRefreshCount[i]; k++ )
      {
         if ( s_nHDMI_ResolutionGroup[i][k] == hgroup )
         if ( s_nHDMI_ResolutionMode[i][k] == hmode )
         {
            s_nHDMI_CurrentResolutionIndex = i;
            s_nHDMI_CurrentResolutionRefreshIndex = k;
            log_line("[HDMI] Current HDMI mode: (g:%d, m:%d), width, height: %d x %d, refresh: %d Hz", hgroup, hmode, s_nHDMI_ResolutionWidth[s_nHDMI_CurrentResolutionIndex], s_nHDMI_ResolutionHeight[s_nHDMI_CurrentResolutionIndex], s_nHDMI_ResolutionRefresh[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex]);
            return 0;
         }
      }
   }
   log_softerror_and_alarm("[HDMI] Failed to detect current HDMI mode. (g:%d, m:%d)", hgroup, hmode);
   return -1;
   #endif

   #if defined (HW_PLATFORM_RADXA)

   // Mode[0] is always the current display mode
   s_nHDMI_CurrentResolutionIndex = 0;
   s_nHDMI_CurrentResolutionRefreshIndex = 0;

   int fdDRM = -1;

   fdDRM = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
   if ( fdDRM < 0 )
   {
      log_softerror_and_alarm("[HDMI] Failed to open graphics device.");
      return -1;
   }

   drmModeRes* pAllDRMResources = drmModeGetResources(fdDRM);
   if ( !pAllDRMResources )
   {
      log_softerror_and_alarm("[HDMI] Cannot retrieve DRM resources (%d)", errno);
      return -errno;
   }
 
   if ( pAllDRMResources->count_connectors <= 0 )
   {
      log_softerror_and_alarm("[HDMI] No connectors available (%d)", errno);
      return -1;
   }
 
   log_line("[HDMI] Finding resources (%d connectors, %d crtcs)...",
      pAllDRMResources->count_connectors, pAllDRMResources->count_crtcs);

   // Find connectors (displays, aka video hardware connectors (HDMI,DVI...))

   drmModeConnector* pConnector = NULL;

   for (int i = 0; i < pAllDRMResources->count_connectors; i++)
   {
      pConnector = drmModeGetConnector(fdDRM, pAllDRMResources->connectors[i]);
      if (!pConnector)
      {
         log_softerror_and_alarm("[HDMI] Cannot retrieve DRM connector %u:%u (%d)",
              i, pAllDRMResources->connectors[i], errno);
         continue;
      }

      if ( pConnector->count_modes <= 0 )
      {
         drmModeFreeConnector(pConnector);
         pConnector = NULL;
         continue;
      }
      if ( pConnector->connection != DRM_MODE_CONNECTED )
      {
         drmModeFreeConnector(pConnector);
         pConnector = NULL;
         continue;
      }
      break;
   }

   if ( NULL == pConnector )
   {
      log_softerror_and_alarm("[HDMI] Can't find any usable and connected connector.");
      drmModeFreeResources(pAllDRMResources);
      return -1;
   }

   // Iterate supported modes for each display (connector)
   for (int j = 0; j < pConnector->count_modes; j++)
   {
      drmModeModeInfo *mode = &pConnector->modes[j];

      log_line("[HDMI] Connector mode %d:  %dx%d%s@%d",
         j, mode->hdisplay, mode->vdisplay,
         mode->flags & DRM_MODE_FLAG_INTERLACE ? "i" : "",
         mode->vrefresh);

      _hdmi_add_resolution(1, s_nHDMI_ResolutionCount+1, mode->hdisplay, mode->vdisplay, mode->vrefresh, HDMI_ASPECT_MODE_16_9);
   }

   drmModeFreeConnector(pConnector);
   drmModeFreeResources(pAllDRMResources);
   close(fdDRM);
   return 0;
   #endif
   return -1;
}

int hdmi_enum_modes()
{
   s_nHDMI_ResolutionCount = 0;
   s_nHDMI_CurrentResolutionIndex = -1;
   s_nHDMI_CurrentResolutionRefreshIndex = -1;

   #if defined (HW_PLATFORM_RASPBERRY)
   _hdmi_add_resolution(1,1, 640, 480, 60, HDMI_ASPECT_MODE_4_3);
   _hdmi_add_resolution(2,9, 800, 600, 60, HDMI_ASPECT_MODE_4_3);
   _hdmi_add_resolution(2,16, 1024, 768, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(1,61, 1280, 720, 25, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,62, 1280, 720, 30, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,19, 1280, 720, 50, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,4,  1280, 720, 60, HDMI_ASPECT_MODE_16_9);

   _hdmi_add_resolution(2,28,  1280, 800, 60, HDMI_ASPECT_MODE_16_10);

   _hdmi_add_resolution(2,32,  1280, 960, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(2,35,  1280, 1024, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(2,51,  1600, 1200, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(1,33,  1920, 1080, 25, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,34,  1920, 1080, 30, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,31,  1920, 1080, 50, HDMI_ASPECT_MODE_16_9);

   _hdmi_add_resolution(2,69,  1920, 1200, 60, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(2,70,  1920, 1200, 75, HDMI_ASPECT_MODE_16_9);
   return 0;
   #endif

   #if defined (HW_PLATFORM_RADXA)
   return _hdmi_detect_current_mode();
   #endif
}

int hdmi_load_current_mode()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "hdmi_mode.cfg");
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("[HDMI] No HDMI mode set by the user. Use default.");
      return -1;
   }
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HDMI] Can't read user set HDMI mode.");
      return -1;
   }

   int w,h,r;
   if ( 3 != fscanf(fd, "%d %d %d", &w, &h, &r) )
   {
      log_softerror_and_alarm("[HDMI] Can't read user set HDMI mode. Invalid file.");
      fclose(fd);
      return -1;    
   }
   fclose(fd);

   if ( (w < 200) || (w > 8000) || (h < 200) || (h > 8000) || (r < 30) || (r > 200) )
   {
      log_softerror_and_alarm("[HDMI] Can't read user set HDMI mode. Invalid params (%d x %d @ %d)", w,h,r);
      return -1;
   }
   log_line("[HDMI] Read user set HDMI mode: %dx%d@%d", w,h,r);
   return hdmi_get_best_resolution_index_for(w, h, r);
}

int hdmi_get_resolutions_count()
{
   return s_nHDMI_ResolutionCount;
}

int hdmi_get_current_resolution_index()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   return s_nHDMI_CurrentResolutionIndex;
}

int hdmi_get_current_resolution_group()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionGroup[0][0];

   return s_nHDMI_ResolutionGroup[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}

int hdmi_get_current_resolution_mode()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionMode[0][0];

   return s_nHDMI_ResolutionMode[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}


int hdmi_get_current_resolution_width()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionWidth[0];

   return s_nHDMI_ResolutionWidth[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_height()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionHeight[0];

   return s_nHDMI_ResolutionHeight[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_refresh()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionRefresh[0][0];

   return s_nHDMI_ResolutionRefresh[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}

int hdmi_get_current_resolution_refresh_count()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionRefreshCount[0];

   return s_nHDMI_ResolutionRefreshCount[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_refresh_index()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return 0;
   return s_nHDMI_CurrentResolutionRefreshIndex;
}


int hdmi_get_resolution_width(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionWidth[0];
   return s_nHDMI_ResolutionWidth[index];
}

int hdmi_get_resolution_height(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionHeight[0];
   return s_nHDMI_ResolutionHeight[index];
}

int hdmi_get_resolution_refresh_count(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionRefreshCount[0];
   return s_nHDMI_ResolutionRefreshCount[index];
}

int hdmi_get_resolution_refresh_rate(int index, int index2)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionRefresh[0][0];
   if ( index2 < 0 || index2 >= s_nHDMI_ResolutionRefreshCount[index] )
      return s_nHDMI_ResolutionRefresh[index][0];
   return s_nHDMI_ResolutionRefresh[index][index2];
}

int hdmi_set_current_resolution(int width, int height, int refresh)
{
   int indexResolution = -1;
   int indexRefresh = -1;
   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      for( int k=0; k<s_nHDMI_ResolutionRefreshCount[i]; k++ )
      {
         if ( s_nHDMI_ResolutionWidth[i] == width )
         if ( s_nHDMI_ResolutionHeight[i] == height )
         if ( s_nHDMI_ResolutionRefresh[i][k] == refresh )
         {
            indexResolution = i;
            indexRefresh = k;
            break;
         }
      }
   }
   if ( (indexResolution == -1) || (indexRefresh == -1) )
   {
      log_softerror_and_alarm("[HDMI] Failed to change HDMI mode. Invalid mode: %d x %d, refresh: %d Hz", width, height, refresh);
      return -1;
   }

   log_line("[HDMI] User changed HDMI mode to: (g:%d, m:%d): %d x %d, refresh: %d Hz", s_nHDMI_ResolutionGroup[indexResolution][indexRefresh], s_nHDMI_ResolutionMode[indexResolution][indexRefresh], width, height, refresh);

   s_nHDMI_CurrentResolutionIndex = indexResolution;
   s_nHDMI_CurrentResolutionRefreshIndex = indexRefresh;

   #if defined (HW_PLATFORM_RASPBERRY)
   hardware_mount_boot();
   hardware_sleep_ms(50);
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   char szBuff[256];
   sprintf(szBuff, "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", s_nHDMI_ResolutionGroup[indexResolution][indexRefresh]);
   hw_execute_bash_command(szBuff, NULL);
   sprintf(szBuff, "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", s_nHDMI_ResolutionMode[indexResolution][indexRefresh]);
   hw_execute_bash_command(szBuff, NULL);
   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
   #endif

   #if defined (HW_PLATFORM_RADXA)
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "hdmi_mode.cfg");
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HDMI] Can't open file to write user set HDMI mode.");
      return -1;
   }

   fprintf(fd, "%d %d %d\n", width, height, refresh);
   fclose(fd);
   log_line("[HDMI] Stored used selected HDMI mode (%d x %d @ %d)", width, height, refresh);
   #endif
   return 0;
}


int hdmi_get_best_resolution_index_for(int iWidth, int iHeight, int iRefresh)
{
   log_line("[HDMI] Finding best supported display resolution for %dx%d@%d...", iWidth, iHeight, iRefresh);
   log_line("[HDMI] Supported resolutions (%d):", s_nHDMI_ResolutionCount);
   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
      log_line("[HDMI] Resolution index %d: %d x %d", i, s_nHDMI_ResolutionWidth[i], s_nHDMI_ResolutionHeight[i]);
   
   int indexResolution = -1;
   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      if ( s_nHDMI_ResolutionWidth[i] == iWidth )
      if ( s_nHDMI_ResolutionHeight[i] == iHeight )
      {
         indexResolution = i;
         break;
      }
   }
   if ( indexResolution == -1 )
   {
      log_softerror_and_alarm("[HDMI] Failed to change HDMI mode. Mode not supported: %d x %d", iWidth, iHeight);
      if ( iWidth > 1280 )
      {
         log_line("[HDMI] Trying a lower resolution...");
         return hdmi_get_best_resolution_index_for(1280, 720, 60);
      }
      // Return mode 0
      s_nHDMI_CurrentResolutionIndex = 0;
      s_nHDMI_CurrentResolutionRefreshIndex = 0;
      return 0;
   }

   if ( s_nHDMI_ResolutionRefreshCount[indexResolution] <= 0 )
   {
      log_softerror_and_alarm("[HDMI] Failed to find HDMI mode. This mode does not support any refresh rates: %d x %d", iWidth, iHeight);
      if ( iWidth > 1280 )
      {
         log_line("[HDMI] Trying a lower resolution...");
         return hdmi_get_best_resolution_index_for(1280, 720, 60);
      }
      // Return mode 0
      s_nHDMI_CurrentResolutionIndex = 0;
      s_nHDMI_CurrentResolutionRefreshIndex = 0;
      return 0;    
   }

   int indexRefresh = -1;
   int iMaxRefresh = 0;
   for( int k=0; k<s_nHDMI_ResolutionRefreshCount[indexResolution]; k++ )
   {
      if ( s_nHDMI_ResolutionRefresh[indexResolution][k] > iMaxRefresh )
      {
         indexRefresh = k;
         iMaxRefresh = s_nHDMI_ResolutionRefresh[indexResolution][k];
      }
      if ( s_nHDMI_ResolutionRefresh[indexResolution][k] == iRefresh )
      {
         indexRefresh = k;
         break;
      }
   }

   if ( indexRefresh == -1 )
   {
      log_softerror_and_alarm("[HDMI] Failed to change HDMI mode. This mode does not support any refresh rates: %d x %d, refresh: %d Hz", iWidth, iHeight, iRefresh);
      if ( iWidth > 1280 )
      {
         log_line("[HDMI] Trying a lower resolution...");
         return hdmi_get_best_resolution_index_for(1280, 720, 60);
      }
      // Return mode 0;
      s_nHDMI_CurrentResolutionIndex = 0;
      s_nHDMI_CurrentResolutionRefreshIndex = 0;
      return 0;
   }

   log_line("[HDMI] Chosen best mode for %dx%d@%d:  %dx%d@%d, res index %d-%d",
      iWidth, iHeight, iRefresh,
      s_nHDMI_ResolutionWidth[indexResolution],
      s_nHDMI_ResolutionHeight[indexResolution],
      s_nHDMI_ResolutionRefresh[indexResolution][indexRefresh],
      indexResolution, indexRefresh);

   s_nHDMI_CurrentResolutionIndex = indexResolution;
   s_nHDMI_CurrentResolutionRefreshIndex = indexRefresh;
   return s_nHDMI_CurrentResolutionIndex;
}