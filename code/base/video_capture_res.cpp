#include "base.h"
#include "config.h"
#include "hardware.h"
#include "video_capture_res.h"

type_video_capture_resolution_info g_listCaptureResolutions[] =
{
   {569,320, 90, "320p"},
   {560,420, 90, "SD 4:3"},
   {746,420, 90, "SD"},
   {980,420, 90, "SD 21:9"},
   {800,420, 90, "480 DSI"},
   {853,480, 90, "480p"},
   {1120,480, 90, "480p 21:9"},
   {1024,576, 90, "576p"},
   {1280,720, 90, "720p"},
   {1680,720, 40, "720p 21:9"},
   {1296,730, 40, "730p"},
   {1200,900, 40, "900p 4:3"},
   {1635,920, 40, "920p"},
   {1280,960, 40, "960p 4:3"},
   {1920,1080, 40, "1080p"}
};
int g_iListCaptureResolutionsCount = 0;


type_video_capture_resolution_info g_listCaptureResolutionsVeye[] =
{
   {1280,720, 30, "720p"},
   {1920,1080, 30, "1080p"}
};
int g_iListCaptureResolutionsVeyeCount = 2;


type_video_capture_resolution_info g_listCaptureResolutionsVeye307[] =
{
   {1280,720, 60, "720p"},
   {1920,1080, 30, "1080p"}
};
int g_iListCaptureResolutionsVeyeCount307 = 2;


type_video_capture_resolution_info g_listCaptureResolutionsOpenIPC[] =
{
   {1024,576, 120, "SD 576"},
   {832, 624, 120, "SD 624 4:3"},
   {960, 720, 120, "720p 4:3"},
   {1280,720, 120, "720p"},
   {1456,816, 120, "820p"},
   {1440, 1080, 90, "1080p 4:3"},
   {1920,1080, 90, "1080p"},
   {1920, 1440, 90, "1.4k 4:3"},
   {2240,1264, 60, "2K"},
   {3200,1800, 30, "3K"},
   {3840,2160, 20, "4k"}
};
int g_iListCaptureResolutionsOpenIPCCount = 11;


type_video_capture_resolution_info* getOptionsVideoResolutions(int iCameraType)
{
   if ( iCameraType == CAMERA_TYPE_VEYE290 ||
        iCameraType == CAMERA_TYPE_VEYE327 )
      return g_listCaptureResolutionsVeye;
   if ( iCameraType == CAMERA_TYPE_VEYE307 )
      return g_listCaptureResolutionsVeye307;
   if ( iCameraType == CAMERA_TYPE_OPENIPC_IMX307 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX335 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX415 )
      return g_listCaptureResolutionsOpenIPC;

   if ( 0 == g_iListCaptureResolutionsCount )
      g_iListCaptureResolutionsCount = sizeof(g_listCaptureResolutions)/sizeof(g_listCaptureResolutions[0]);

   return g_listCaptureResolutions;
}

int getOptionsVideoResolutionsCount(int iCameraType)
{
   if ( iCameraType == CAMERA_TYPE_VEYE290 ||
        iCameraType == CAMERA_TYPE_VEYE327 )
      return g_iListCaptureResolutionsVeyeCount;
   if ( iCameraType == CAMERA_TYPE_VEYE307 )
      return g_iListCaptureResolutionsVeyeCount307;
   if ( iCameraType == CAMERA_TYPE_OPENIPC_IMX307 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX335 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX415 )
      return g_iListCaptureResolutionsOpenIPCCount;

   if ( 0 == g_iListCaptureResolutionsCount )
      g_iListCaptureResolutionsCount = sizeof(g_listCaptureResolutions)/sizeof(g_listCaptureResolutions[0]);
   return g_iListCaptureResolutionsCount;
}

int getOptionsVideoResolutionMaxFPS(int iCameraType, int w, int h)
{
   if ( iCameraType == CAMERA_TYPE_VEYE290 ||
        iCameraType == CAMERA_TYPE_VEYE327 )
   {
      for( int i=0; i<g_iListCaptureResolutionsVeyeCount; i++ )
      {
         if ( g_listCaptureResolutionsVeye[i].iWidth == w )
         if ( g_listCaptureResolutionsVeye[i].iHeight == h )
            return g_listCaptureResolutionsVeye[i].iMaxFPS;
      }
      return 30;
   }
   if ( iCameraType == CAMERA_TYPE_VEYE307 )
   {
      for( int i=0; i<g_iListCaptureResolutionsVeyeCount307; i++ )
      {
         if ( g_listCaptureResolutionsVeye307[i].iWidth == w )
         if ( g_listCaptureResolutionsVeye307[i].iHeight == h )
            return g_listCaptureResolutionsVeye307[i].iMaxFPS;
      }
      return 30;
   }
   if ( iCameraType == CAMERA_TYPE_OPENIPC_IMX307 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX335 ||
        iCameraType == CAMERA_TYPE_OPENIPC_IMX415 )
   {
      for( int i=0; i<g_iListCaptureResolutionsOpenIPCCount; i++ )
      {
         if ( g_listCaptureResolutionsOpenIPC[i].iWidth == w )
         if ( g_listCaptureResolutionsOpenIPC[i].iHeight == h )
            return g_listCaptureResolutionsOpenIPC[i].iMaxFPS;
      }
      return 90;
   }

   if ( 0 == g_iListCaptureResolutionsCount )
      g_iListCaptureResolutionsCount = sizeof(g_listCaptureResolutions)/sizeof(g_listCaptureResolutions[0]);

   for( int i=0; i<g_iListCaptureResolutionsCount; i++ )
   {
      if ( g_listCaptureResolutions[i].iWidth == w )
      if ( g_listCaptureResolutions[i].iHeight == h )
         return g_listCaptureResolutions[i].iMaxFPS;
   }
   return 60;
}

char* getOptionVideoResolutionName(int w, int h)
{
   if ( 0 == g_iListCaptureResolutionsCount )
      g_iListCaptureResolutionsCount = sizeof(g_listCaptureResolutions)/sizeof(g_listCaptureResolutions[0]);

   for( int i=0; i<g_iListCaptureResolutionsCount; i++ )
   {
      if ( g_listCaptureResolutions[i].iWidth == w )
      if ( g_listCaptureResolutions[i].iHeight == h )
         return g_listCaptureResolutions[i].szName;
   }
   for( int i=0; i<g_iListCaptureResolutionsVeyeCount; i++ )
   {
      if ( g_listCaptureResolutionsVeye[i].iWidth == w )
      if ( g_listCaptureResolutionsVeye[i].iHeight == h )
         return g_listCaptureResolutionsVeye[i].szName;
   }
   for( int i=0; i<g_iListCaptureResolutionsOpenIPCCount; i++ )
   {
      if ( g_listCaptureResolutionsOpenIPC[i].iWidth == w )
      if ( g_listCaptureResolutionsOpenIPC[i].iHeight == h )
         return g_listCaptureResolutionsOpenIPC[i].szName;
   }
   static char s_szVideoResolutionNA[] = "N/A";
   return s_szVideoResolutionNA;
}