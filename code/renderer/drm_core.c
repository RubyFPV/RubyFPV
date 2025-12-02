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
        * Military use is not per
        mited.

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


#include "drm_core.h"
#include "../base/base.h"
#include "../base/hardware.h"
#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

int s_fdDRM = -1;
type_drm_display_attributes s_DRMDisplayAttributes;
type_drm_runtime_state s_DRMRuntimeState;


int s_iDRMCoreInitialized = 0;
int s_iDRMEnableVSync = 1;

static const char *_ruby_drm_core_get_connector_str(uint32_t conn_type)
{
   switch (conn_type)
   {
      case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
      case DRM_MODE_CONNECTOR_VGA:         return "VGA";
      case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
      case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
      case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
      case DRM_MODE_CONNECTOR_Composite:   return "Composite";
      case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
      case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
      case DRM_MODE_CONNECTOR_Component:   return "Component";
      case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
      case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
      case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
      case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
      case DRM_MODE_CONNECTOR_TV:          return "TV";
      case DRM_MODE_CONNECTOR_eDP:         return "eDP";
      case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
      case DRM_MODE_CONNECTOR_DSI:         return "DSI";
      default:                             return "Unknown";
   }
   return "N/A";
}

const char* _ruby_drm_fourcc_to_string(uint32_t fourcc)
{
    char* result = malloc(5);
    result[0] = (char)((fourcc >> 0) & 0xFF);
    result[1] = (char)((fourcc >> 8) & 0xFF);
    result[2] = (char)((fourcc >> 16) & 0xFF);
    result[3] = (char)((fourcc >> 24) & 0xFF);
    result[4] = '\0';
    return result;
}

int _ruby_drm_get_object_properties(type_drm_object_info* pObject)
{
   if ( NULL == pObject )
      return -1;
   if ( 0xFFFFFFFF == pObject->uObjId )
      return -2;

   const char *szType;

   switch( pObject->uObjType )
   {
      case DRM_MODE_OBJECT_CONNECTOR: szType = "Connector"; break;
      case DRM_MODE_OBJECT_PLANE: szType = "Plane"; break;
      case DRM_MODE_OBJECT_CRTC: szType = "CRTC"; break;
      default: szType = "unknown type"; break;
   }
   pObject->pProperties = drmModeObjectGetProperties(s_fdDRM, pObject->uObjId, pObject->uObjType);
   if (! pObject->pProperties)
   {
      log_softerror_and_alarm("[DRMCore] Can't get object properties, Object type: %s, id: %u, error: %s",
         szType, pObject->uObjId, strerror(errno));
      return -1;
   }

   pObject->ppPropertiesInfo = calloc( pObject->pProperties->count_props, sizeof(drmModePropertyRes*));
   log_line("[DRMCore] Object %s, id: %u has %d properties", szType, pObject->uObjId, pObject->pProperties->count_props);
   for (int i = 0; i < pObject->pProperties->count_props; i++)
   {
       pObject->ppPropertiesInfo[i] = drmModeGetProperty(s_fdDRM, pObject->pProperties->props[i]);
       //if ( NULL != pObject->ppPropertiesInfo[i] )
       //if ( NULL != pObject->ppPropertiesInfo[i]->name )
       //   log_line("[DRMCore] Object %s property %d: %s", szType, i, pObject->ppPropertiesInfo[i]->name);
   }
   return 0;
}

void _ruby_drm_free_object_properties(type_drm_object_info* pObject)
{
   if ( NULL == pObject )
      return;
   for ( int i = 0; i < pObject->pProperties->count_props; i++ )
      drmModeFreeProperty(pObject->ppPropertiesInfo[i]);
   free(pObject->ppPropertiesInfo);
   drmModeFreeObjectProperties(pObject->pProperties);
}


int64_t _ruby_drm_get_object_property_value(drmModeObjectPropertiesPtr pProps, const char *szName)
{
   if ( NULL == szName )
      return -1;

   drmModePropertyPtr prop;
   uint64_t uValue = 0;
   int iFound = 0;
   
   for (int i = 0; (i < pProps->count_props) && (!iFound); i++)
   {
      prop = drmModeGetProperty(s_fdDRM, pProps->props[i]);
      if ( 0 == strcmp(prop->name, szName) )
      {
         uValue = pProps->prop_values[i];
         iFound = 1;
         log_line("[DRMCore] Got object property %s value: %u", szName, (u32)uValue);
      }
      drmModeFreeProperty(prop);
   }

   if (! iFound)
      return -1;
   return uValue;
}

int _ruby_drm_open_device()
{
   int iRet = -1;
   uint64_t cap;

   s_fdDRM = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
   if ( s_fdDRM < 0 )
   {
      log_softerror_and_alarm("[DRMCore] Failed to open graphics device.");
      return -1;
   }

   iRet = drmSetClientCap(s_fdDRM, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
   if ( iRet )
   {
      log_softerror_and_alarm("[DRMCore] Failed to set universal planes cap, %d", iRet);
      return iRet;
   }

   iRet = drmSetClientCap(s_fdDRM, DRM_CLIENT_CAP_ATOMIC, 1);
   if (iRet)
   {
      log_softerror_and_alarm("[DRMCore] Failed to set atomic cap, %d", iRet);
      return iRet;
   }

   if ( drmGetCap(s_fdDRM, DRM_CAP_DUMB_BUFFER, &cap) < 0 || (!cap) )
   {
      log_softerror_and_alarm("[DRMCore] DRM device does not support dumb buffers");
      close(s_fdDRM);
      s_fdDRM = -1;
      return -EOPNOTSUPP;
   }

   if ( drmGetCap(s_fdDRM, DRM_CAP_CRTC_IN_VBLANK_EVENT, &cap) < 0 || (!cap) )
   {
      log_softerror_and_alarm("[DRMCore] DRM device does not support atomic KMS");
      close(s_fdDRM);
      s_fdDRM = -1;
      return -EOPNOTSUPP;
   }

   log_line("[DRMCore] Opened DRM device.");
   return 0;
}


int _ruby_drm_core_enumerate_find_resources()
{
   if ( s_fdDRM < 0 )
      return -1;
  
   s_DRMRuntimeState.pAllDRMResources = drmModeGetResources(s_fdDRM);
   if ( !s_DRMRuntimeState.pAllDRMResources )
   {
      log_softerror_and_alarm("[DRMCore] Cannot retrieve DRM resources (%d)", errno);
      return -errno;
   }
 
   if ( s_DRMRuntimeState.pAllDRMResources->count_connectors <= 0 )
   {
      log_softerror_and_alarm("[DRMCore] No connectors available (%d)", errno);
      return -1;
   }
 
   log_line("[DRMCore] (Enumerate find resources) Finding resources (%d connectors, %d crtcs)...",
      s_DRMRuntimeState.pAllDRMResources->count_connectors, s_DRMRuntimeState.pAllDRMResources->count_crtcs);

   // Find connectors (displays, aka video hardware connectors (HDMI,DVI...))

   s_DRMRuntimeState.pConnector = NULL;

   for (int i = 0; i < s_DRMRuntimeState.pAllDRMResources->count_connectors; i++)
   {
      s_DRMRuntimeState.pConnector = drmModeGetConnector(s_fdDRM, s_DRMRuntimeState.pAllDRMResources->connectors[i]);
      if (!s_DRMRuntimeState.pConnector)
      {
         log_softerror_and_alarm("[DRMCore] Cannot retrieve DRM connector %u:%u (%d)",
              i, s_DRMRuntimeState.pAllDRMResources->connectors[i], errno);
         continue;
      }

      log_line("[DRMCore] Found connector: %s, %d, %s",
         _ruby_drm_core_get_connector_str(s_DRMRuntimeState.pConnector->connector_type),
         s_DRMRuntimeState.pConnector->connector_type_id,
         s_DRMRuntimeState.pConnector->connection == DRM_MODE_CONNECTED ? "connected" : "disonnected");

      if ( s_DRMRuntimeState.pConnector->count_modes <= 0 )
      {
         log_line("[DRMCore] Connector does not support any modes.");
         drmModeFreeConnector(s_DRMRuntimeState.pConnector);
         s_DRMRuntimeState.pConnector = NULL;
         continue;
      }

      // List supported modes for each display (connector)
      for (int j = 0; j < s_DRMRuntimeState.pConnector->count_modes; j++)
      {
         drmModeModeInfo *mode = &s_DRMRuntimeState.pConnector->modes[j];

         log_line("[DRMCore] Connector mode %d:  %dx%d%s@%d",
            j, mode->hdisplay, mode->vdisplay,
            mode->flags & DRM_MODE_FLAG_INTERLACE ? "i" : "",
            mode->vrefresh);

          if ( (mode->hdisplay == s_DRMDisplayAttributes.iWidth) &&
               (mode->vdisplay == s_DRMDisplayAttributes.iHeight) &&
               (mode->vrefresh == s_DRMDisplayAttributes.iRefreshRate) )
          {
             s_DRMDisplayAttributes.iInterleaved = (mode->flags & DRM_MODE_FLAG_INTERLACE )?1:0;
             s_DRMRuntimeState.objInfoConnector.uObjId = s_DRMRuntimeState.pConnector->connector_id;
             s_DRMRuntimeState.objInfoConnector.iObjIndex = i;
             s_DRMRuntimeState.targetModeInfo = s_DRMRuntimeState.pConnector->modes[j];

             memcpy(&s_DRMDisplayAttributes.currentMode, &s_DRMRuntimeState.pConnector->modes[j], sizeof(s_DRMDisplayAttributes.currentMode));
             
             drmModeCreatePropertyBlob(s_fdDRM, &s_DRMDisplayAttributes.currentMode, sizeof(s_DRMDisplayAttributes.currentMode), &s_DRMRuntimeState.uModeIdBlob);

             log_line("[DRMCore] Using this mode. Index %d", j);
             break;
          }
      }
      // Use first mode if autoselection is set
      if ( s_DRMDisplayAttributes.iWidth == 0 )
      {
         drmModeModeInfo *mode = &s_DRMRuntimeState.pConnector->modes[0];

         log_line("[DRMCore] Using auto mode: connector mode 0:  %dx%d%s@%d",
            mode->hdisplay, mode->vdisplay,
            mode->flags & DRM_MODE_FLAG_INTERLACE ? "i" : "",
            mode->vrefresh);

         s_DRMDisplayAttributes.iWidth = mode->hdisplay;
         s_DRMDisplayAttributes.iHeight = mode->vdisplay;
         s_DRMDisplayAttributes.iRefreshRate = mode->vrefresh;
         s_DRMDisplayAttributes.iInterleaved = (mode->flags & DRM_MODE_FLAG_INTERLACE )?1:0;

         s_DRMRuntimeState.objInfoConnector.uObjId = s_DRMRuntimeState.pConnector->connector_id;
         s_DRMRuntimeState.objInfoConnector.iObjIndex = i;
         s_DRMRuntimeState.targetModeInfo = s_DRMRuntimeState.pConnector->modes[0];

         memcpy(&s_DRMDisplayAttributes.currentMode, &s_DRMRuntimeState.pConnector->modes[0], sizeof(s_DRMDisplayAttributes.currentMode));
         
         drmModeCreatePropertyBlob(s_fdDRM, &s_DRMDisplayAttributes.currentMode, sizeof(s_DRMDisplayAttributes.currentMode), &s_DRMRuntimeState.uModeIdBlob);
      }

      if ( s_DRMRuntimeState.objInfoConnector.uObjId != 0xFFFFFFFF )
         break;
      drmModeFreeConnector(s_DRMRuntimeState.pConnector);
      s_DRMRuntimeState.pConnector = NULL;
   }

   // Find the actual display/CRTc for this connector (connector->encoder->crt/display)

   log_line("[DRMCore] (Enumerate find resources) Finding display...");

   s_DRMRuntimeState.pEncoder = NULL;
   s_DRMRuntimeState.pCRTc = NULL;
   s_DRMRuntimeState.objInfoCRTc.iObjIndex = -1;

   if ( s_DRMRuntimeState.pConnector->encoder_id )
      s_DRMRuntimeState.pEncoder = drmModeGetEncoder(s_fdDRM, s_DRMRuntimeState.pConnector->encoder_id);

   if ( s_DRMRuntimeState.pEncoder && (s_DRMRuntimeState.pEncoder->crtc_id > 0) )
   {
      s_DRMRuntimeState.pCRTc = drmModeGetCrtc(s_fdDRM, s_DRMRuntimeState.pEncoder->crtc_id);
      s_DRMRuntimeState.pOriginalCRTc = s_DRMRuntimeState.pCRTc;
      s_DRMRuntimeState.objInfoCRTc.uObjId = s_DRMRuntimeState.pEncoder->crtc_id;  
      s_DRMRuntimeState.objInfoCRTc.iObjIndex = -1;
      for (int i = 0; i < s_DRMRuntimeState.pAllDRMResources->count_crtcs; i++)
      {
         if ( s_DRMRuntimeState.pAllDRMResources->crtcs[i] == s_DRMRuntimeState.pEncoder->crtc_id )
         {
            s_DRMRuntimeState.objInfoCRTc.iObjIndex = i;
            break;
         }
      } 
      log_line("[DRMCore] Found CRTc for encoder. crtc id: %u, crt index: %d",
         s_DRMRuntimeState.pEncoder->crtc_id, s_DRMRuntimeState.objInfoCRTc.iObjIndex);
   }
   else
   {
      drmModeFreeEncoder(s_DRMRuntimeState.pEncoder);
      s_DRMRuntimeState.pEncoder = NULL;
      s_DRMRuntimeState.objInfoCRTc.iObjIndex = -1;
   }

   if ( (NULL == s_DRMRuntimeState.pEncoder) || (NULL == s_DRMRuntimeState.pCRTc) )
   {
      log_line("[DRMCore] Iterating displays on all encoders...");
      for (int i = 0; i < s_DRMRuntimeState.pConnector->count_encoders; i++)
      {
         s_DRMRuntimeState.pEncoder = drmModeGetEncoder(s_fdDRM, s_DRMRuntimeState.pConnector->encoders[i]);
         if ( ! s_DRMRuntimeState.pEncoder )
         {
            log_softerror_and_alarm("[DRMCore] Cannot retrieve encoder %d: %u (%d)",
                i, s_DRMRuntimeState.pConnector->encoders[i], errno);
            continue;
         }
         
         s_DRMRuntimeState.objInfoCRTc.uObjId = 0xFFFFFFFF;
         for (int j = 0; j < s_DRMRuntimeState.pAllDRMResources->count_crtcs; j++)
         {
            if ( !(s_DRMRuntimeState.pEncoder->possible_crtcs & (1 << j)) )
               continue;

            if ( s_DRMRuntimeState.pAllDRMResources->crtcs[j] > 0 )
            {
               s_DRMRuntimeState.pCRTc = drmModeGetCrtc(s_fdDRM, s_DRMRuntimeState.pAllDRMResources->crtcs[j]);
               s_DRMRuntimeState.pOriginalCRTc = s_DRMRuntimeState.pCRTc;
               s_DRMRuntimeState.objInfoCRTc.iObjIndex = j;
               s_DRMRuntimeState.objInfoCRTc.uObjId = s_DRMRuntimeState.pAllDRMResources->crtcs[j];
               log_line("[DRMCore] Found display for crtc id %u, crtc index %d, for encoder %u",
                  s_DRMRuntimeState.pAllDRMResources->crtcs[j], j, s_DRMRuntimeState.pConnector->encoders[i]);
               break;
            }
         }
         if ( s_DRMRuntimeState.objInfoCRTc.uObjId != 0xFFFFFFFF )
            break;
      }
   }

   if ( NULL == s_DRMRuntimeState.pEncoder )
   {
      log_softerror_and_alarm("[DRMCore] (Enumerate find resources) Could not find an encoder.");
      return -1;
   }
   if ( NULL == s_DRMRuntimeState.pCRTc )
   {
      log_softerror_and_alarm("[DRMCore] (Enumerate find resources) Could not find a display.");
      return -1;
   }
   log_line("[DRMCore] (Enumerate find resources) Completed.");
   return 0;
}

int _ruby_drm_find_target_plane()
{
   // -------------------------------------------------------------------
   // Finding planes supported by currently selected connector/display
   // Select the desired plane as the one to use

   log_line("[DRMCore] Finding planes supported by current display (target plane index is %d, target plane format is: %s)...",
      s_DRMRuntimeState.objInfoPlane.iObjIndex, _ruby_drm_fourcc_to_string(s_DRMRuntimeState.uPlaneFormat) );

   s_DRMRuntimeState.pPlanesResources = drmModeGetPlaneResources(s_fdDRM);
   if ( !s_DRMRuntimeState.pPlanesResources )
   {
      log_softerror_and_alarm("[DRMCore] drmModeGetPlaneResources failed: %s", strerror(errno));
      return -1;
   }

   log_line("[DRMCore] Total supported planes by current display/connector: %d", s_DRMRuntimeState.pPlanesResources->count_planes);
   char szBuff[1024];
   szBuff[0] = 0;
   for (int i = 0; i < s_DRMRuntimeState.pPlanesResources->count_planes; i++)
   {
      int plane_id = s_DRMRuntimeState.pPlanesResources->planes[i];
      char szTmp[16];
      if ( 0 == i )
         sprintf(szTmp, "%d", plane_id);
      else
         sprintf(szTmp, ", %d", plane_id);

      strcat(szBuff, szTmp);
   }
   log_line("[DRMCore] Planes Ids: [%s]", szBuff);

   s_DRMRuntimeState.objInfoPlane.uObjId = 0xFFFFFFFF;
   s_DRMRuntimeState.iPlaneFormatIndex = -1;

   for (int i = 0; i < s_DRMRuntimeState.pPlanesResources->count_planes; i++)
   {
      if ( s_DRMRuntimeState.objInfoPlane.iObjIndex != -1 )
      if ( s_DRMRuntimeState.objInfoPlane.iObjIndex != i )
         continue;

      s_DRMRuntimeState.pPlane = drmModeGetPlane(s_fdDRM, s_DRMRuntimeState.pPlanesResources->planes[i]);
      if (!s_DRMRuntimeState.pPlane)
      {
         log_softerror_and_alarm("[DRMCore] drmModeGetPlane(%u) failed: %s", s_DRMRuntimeState.pPlanesResources->planes[i], strerror(errno));
         continue;
      }

      if ( s_DRMRuntimeState.pPlane->possible_crtcs & (1 << s_DRMRuntimeState.objInfoCRTc.iObjIndex) )
      {
         log_line("[DRMCore] Plane index %d (plane id %u) supports %d formats on current crt/display",
            i, s_DRMRuntimeState.pPlanesResources->planes[i], s_DRMRuntimeState.pPlane->count_formats);
         for (int j=0; j<s_DRMRuntimeState.pPlane->count_formats; j++)
         {
            log_line("[DRMCore] Found plane-%d format %d: %s",
             i, j, _ruby_drm_fourcc_to_string(s_DRMRuntimeState.pPlane->formats[j]));
            if ( s_DRMRuntimeState.pPlane->formats[j] == s_DRMRuntimeState.uPlaneFormat )
            {
               s_DRMRuntimeState.objInfoPlane.uObjId = s_DRMRuntimeState.pPlanesResources->planes[i];
               s_DRMRuntimeState.objInfoPlane.iObjIndex = i;
               s_DRMRuntimeState.iPlaneFormatIndex = j;
               break;
            }
         }
      }
      else
         log_line("[DRMCore] Skipping plane index %d as it's not supported by currently used crtc index %d", i, s_DRMRuntimeState.objInfoCRTc.iObjIndex);

      if ( s_DRMRuntimeState.objInfoPlane.uObjId != 0xFFFFFFFF )
         break;

      drmModeFreePlane(s_DRMRuntimeState.pPlane);
   } 

   if ( s_DRMRuntimeState.objInfoPlane.uObjId != 0xFFFFFFFF )
      log_line("[DRMCore] Found plane format for format %s: plane id %d, plane index %d",
         _ruby_drm_fourcc_to_string(s_DRMRuntimeState.uPlaneFormat),
         s_DRMRuntimeState.objInfoPlane.uObjId, s_DRMRuntimeState.objInfoPlane.iObjIndex);
   else
   {
      log_softerror_and_alarm("[DRMCore] Can't find suitable plane for current display/crt.");   
      return -1;
   }

   return 0;
}

int _ruby_drm_create_drm_surface_buffer(type_drm_buffer* pOutputBufferInfo)
{
   if ( NULL == pOutputBufferInfo )
      return -1;

   memset(pOutputBufferInfo, 0, sizeof(type_drm_buffer));
   struct drm_mode_create_dumb creq;
   struct drm_mode_destroy_dumb dreq;
   struct drm_mode_map_dumb mreq;
 
   int iRet = 0;
   uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

   memset(&creq, 0, sizeof(creq));
   creq.width = s_DRMDisplayAttributes.iWidth;
   creq.height = s_DRMDisplayAttributes.iHeight;
   creq.bpp = s_DRMDisplayAttributes.iBPP;
   iRet = drmIoctl(s_fdDRM, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
   if ( iRet < 0 )
   {
      log_softerror_and_alarm("[DRMCore] Cannot create buffer (%d)", errno);
      return -errno;
   }
   pOutputBufferInfo->uWidth = creq.width;
   pOutputBufferInfo->uHeight = creq.height;
   pOutputBufferInfo->uStride = creq.pitch;
   pOutputBufferInfo->uSize = creq.size;
   pOutputBufferInfo->uHandle = creq.handle;

   handles[0] = pOutputBufferInfo->uHandle;
   pitches[0] = pOutputBufferInfo->uStride;

   iRet = drmModeAddFB2(s_fdDRM, s_DRMDisplayAttributes.iWidth, s_DRMDisplayAttributes.iHeight, DRM_FORMAT_ARGB8888,
       handles, pitches, offsets, &(pOutputBufferInfo->uBufferId), 0);
   if ( iRet )
   {
      log_softerror_and_alarm("[DRMCore] Cannot create framebuffer (%d)", errno);
      memset(&dreq, 0, sizeof(dreq));
      dreq.handle = pOutputBufferInfo->uHandle;
      drmIoctl(s_fdDRM, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
      return -1;
   }

   memset(&mreq, 0, sizeof(mreq));
   mreq.handle = pOutputBufferInfo->uHandle;
   iRet = drmIoctl(s_fdDRM, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
   if ( iRet )
   {
      log_softerror_and_alarm("[DRMCore] Cannot map buffer (%d)", errno);
      drmModeRmFB(s_fdDRM, pOutputBufferInfo->uBufferId); 
      memset(&dreq, 0, sizeof(dreq));
      dreq.handle = pOutputBufferInfo->uHandle;
      drmIoctl(s_fdDRM, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
      return -1;
   }

   pOutputBufferInfo->pData = mmap(0, pOutputBufferInfo->uSize, PROT_READ | PROT_WRITE, MAP_SHARED,
          s_fdDRM, mreq.offset);
   if ( pOutputBufferInfo->pData == MAP_FAILED )
   {
      log_softerror_and_alarm("[DRMCore] Cannot mmap buffer (%d)", errno);
      drmModeRmFB(s_fdDRM, pOutputBufferInfo->uBufferId); 
      memset(&dreq, 0, sizeof(dreq));
      dreq.handle = pOutputBufferInfo->uHandle;
      drmIoctl(s_fdDRM, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
      return -1;
   }

   memset(pOutputBufferInfo->pData, 0, pOutputBufferInfo->uSize);
 
   log_line("[DRMCore] Created new surface buffer, size: %d, w/h: %d/%d, stride: %d, handle: %u, buffer id: %u",
      pOutputBufferInfo->uSize, pOutputBufferInfo->uWidth, pOutputBufferInfo->uHeight,
      pOutputBufferInfo->uStride, pOutputBufferInfo->uHandle, pOutputBufferInfo->uBufferId);
   return 0;
}

int _ruby_drm_destroy_drm_surface_buffer(type_drm_buffer* pBuffer)
{
   if ( NULL == pBuffer )
      return -1;
   if ( (0 == pBuffer->pData) || (0 == pBuffer->uBufferId) )
      return -1;

   log_line("[DRMCore] Destroy frame buffer id %u, handle %u ...", pBuffer->uBufferId, pBuffer->uHandle);

   munmap(pBuffer->pData, pBuffer->uSize);
   drmModeRmFB(s_fdDRM, pBuffer->uBufferId);

   struct drm_mode_destroy_dumb dreq;
   memset(&dreq, 0, sizeof(dreq));
   dreq.handle = pBuffer->uHandle;
   drmIoctl(s_fdDRM, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
   return 0;
}


int _ruby_drm_set_mode()
{
   //if ( (0 == s_iDRMTargetPlaneIndex) || (-1 == s_iDRMTargetPlaneIndex) )
   /*
   {
      int iRet = drmModeSetCrtc(s_fdDRM, s_DRMRuntimeState.objInfoCRTc.uObjId, s_DRMRuntimeState.drawBuffers[s_DRMRuntimeState.iActiveOnScreenDrawBuffer].uBufferId, 0, 0,
         &s_DRMRuntimeState.objInfoConnector.uObjId, 1, &s_DRMRuntimeState.targetModeInfo);
      if ( iRet < 0 )
      {
         log_softerror_and_alarm("[DRMCore] Failed to set new mode.");
         return -1;
      }
      log_line("[DRMCore] Did set DRM mode for CRTc for plane index %d, plane id: %u",
          s_DRMRuntimeState.objInfoPlane.iObjIndex, s_DRMRuntimeState.objInfoPlane.uObjId);
   }
   */
   /*
   else
   {
      drmModeSetPlane( s_fdDRM, (u32)s_iDRMPlaneId, s_uDRMCurrentCrtcId,
                    s_DRMDrawBuffers[s_iDRMActiveOnScreenDrawBuffer].uBufferId,
                    0,
                    0, 0,
                    s_DRMDisplayAttributes.iWidth, s_DRMDisplayAttributes.iHeight,
                    0, 0,
                    ((uint16_t) s_DRMDisplayAttributes.iWidth) << 16, ((uint16_t) s_DRMDisplayAttributes.iHeight) << 16);
      log_line("[DRMCore] Did set plane %d for current CRTc.", s_iDRMTargetPlaneIndex );
   }
   */
   return 0;
}

int ruby_drm_core_is_display_connected()
{
   int iMustCloseDevice = 0;
   if ( s_fdDRM < 0 )
   {
      iMustCloseDevice = 1;
      if ( _ruby_drm_open_device() < 0 )
         return -1;
   }

   drmModeRes* pAllDRMResources = drmModeGetResources(s_fdDRM);
   if ( !pAllDRMResources )
   {
      log_softerror_and_alarm("[DRMCore] Cannot retrieve DRM resources (%d)", errno);
      return -errno;
   }
 
   if ( pAllDRMResources->count_connectors <= 0 )
   {
      log_softerror_and_alarm("[DRMCore] No connectors available (%d)", errno);
      return -1;
   }
 
   log_line("[DRMCore] (Check display connected) Finding resources (%d connectors, %d crtcs)...",
      pAllDRMResources->count_connectors, pAllDRMResources->count_crtcs);

   // Find connectors (displays, aka video hardware connectors (HDMI,DVI...))

   drmModeConnector* pConnector = NULL;

   for (int i = 0; i < pAllDRMResources->count_connectors; i++)
   {
      pConnector = drmModeGetConnector(s_fdDRM, pAllDRMResources->connectors[i]);
      if (!pConnector)
      {
         log_softerror_and_alarm("[DRMCore] Cannot retrieve DRM connector %u:%u (%d)",
              i, pAllDRMResources->connectors[i], errno);
         continue;
      }

      if ( pConnector->count_modes <= 0 )
      {
         drmModeFreeConnector(pConnector);
         continue;
      }
      if ( pConnector->connection == DRM_MODE_CONNECTED )
      {
         drmModeFreeConnector(pConnector);
         drmModeFreeResources(pAllDRMResources);
         if ( iMustCloseDevice )
         {
            close(s_fdDRM);
            s_fdDRM = -1;
         }
         return 1;
      }
      
      drmModeFreeConnector(pConnector);
      pConnector = NULL;
   }

   drmModeFreeResources(pAllDRMResources);

   if ( iMustCloseDevice )
   {
      close(s_fdDRM);
      s_fdDRM = -1;
   }
   return 0;
}

int ruby_drm_core_wait_for_display_connected()
{
   log_line("[DRMCore] Waiting for display to be conncted...");

   if ( ruby_drm_core_is_display_connected() )
   {
      log_line("[DRMCore] Display is connected.");
      return 1;
   }
   do
   {
      hardware_sleep_ms(400);
      log_line("[DRMCore] Waiting again for display to be conncted...");
   } while (ruby_drm_core_is_display_connected() <= 0 );
   hardware_sleep_ms(100);
   log_line("[DRMCore] Display is connected.");
   return 1;
}

int ruby_drm_core_init(int iPlaneIndex, uint32_t uFormat, int iWidth, int iHeight, int iRefreshRate)
{
   log_line("[DRMCore] Init (on plane index %d, format %s, w/h/r: %dx%d@%d)...",
      iPlaneIndex, _ruby_drm_fourcc_to_string(uFormat), iWidth, iHeight, iRefreshRate);

   s_DRMDisplayAttributes.iWidth = iWidth;
   s_DRMDisplayAttributes.iHeight = iHeight;
   s_DRMDisplayAttributes.iRefreshRate = iRefreshRate;
   s_DRMDisplayAttributes.iInterleaved = 0;
   s_DRMDisplayAttributes.iBPP = 32;

   memset(&s_DRMRuntimeState, 0, sizeof(type_drm_runtime_state));
   s_DRMRuntimeState.uPlaneFormat = uFormat;
   s_DRMRuntimeState.objInfoPlane.iObjIndex = iPlaneIndex;
   s_DRMRuntimeState.objInfoPlane.uObjId = 0xFFFFFFFF;
   s_DRMRuntimeState.iPlaneFormatIndex = 0;
   s_DRMRuntimeState.objInfoConnector.uObjType = DRM_MODE_OBJECT_CONNECTOR;
   s_DRMRuntimeState.objInfoConnector.uObjId = 0xFFFFFFFF;
   s_DRMRuntimeState.objInfoCRTc.uObjType = DRM_MODE_OBJECT_CRTC;
   s_DRMRuntimeState.objInfoCRTc.uObjId = 0xFFFFFFFF;
   s_DRMRuntimeState.objInfoPlane.uObjType = DRM_MODE_OBJECT_PLANE;
   s_DRMRuntimeState.objInfoPlane.uObjId = 0xFFFFFFFF;

   s_DRMRuntimeState.iVideoSourceWidth = -1;
   s_DRMRuntimeState.iVideoSourceHeight = -1;

   _ruby_drm_open_device();
   
   _ruby_drm_core_enumerate_find_resources();
   _ruby_drm_find_target_plane();

   log_line("[DRMCore] Init: finding object properties for target plane %d", iPlaneIndex);

   _ruby_drm_get_object_properties(&s_DRMRuntimeState.objInfoConnector);
   _ruby_drm_get_object_properties(&s_DRMRuntimeState.objInfoCRTc);
   _ruby_drm_get_object_properties(&s_DRMRuntimeState.objInfoPlane);

   s_DRMRuntimeState.pAtomicRequest = drmModeAtomicAlloc();
   
   //int64_t iZPos = _ruby_drm_get_object_property_value(s_DRMRuntimeState.objInfoPlane.pProperties, "zpos");

   _ruby_drm_create_drm_surface_buffer(&s_DRMRuntimeState.drawBuffers[0]);
   _ruby_drm_create_drm_surface_buffer(&s_DRMRuntimeState.drawBuffers[1]);

   log_line("[DRMCore] Init: created drm surfaces target plane %d", iPlaneIndex);

   s_DRMRuntimeState.iActiveOnScreenDrawBuffer = 0;
   s_iDRMCoreInitialized = 1;

   log_line("[DRMCore] Init complete (on plane index %d, format %s, w/h/r: %dx%d@%d)",
      iPlaneIndex, _ruby_drm_fourcc_to_string(uFormat), iWidth, iHeight, iRefreshRate);
   return 0;
}

int ruby_drm_core_uninit()
{
   log_line("[DRMCore] Uninit");

   int iRet = drmModeSetCrtc(s_fdDRM, s_DRMRuntimeState.pOriginalCRTc->crtc_id, s_DRMRuntimeState.pOriginalCRTc->buffer_id, s_DRMRuntimeState.pOriginalCRTc->x, s_DRMRuntimeState.pOriginalCRTc->y,
      &s_DRMRuntimeState.objInfoConnector.uObjId, 1, &s_DRMRuntimeState.pOriginalCRTc->mode);
   if ( iRet < 0 )
   {
      log_softerror_and_alarm("[DRMCore] Failed to restore old mode.");
   }

  // int64_t iZPos = _ruby_drm_get_object_property_value(s_DRMRuntimeState.objInfoPlane.pProperties, "zpos");
 /*
 ret = modeset_atomic_prepare_commit(fd, output_list, output_list->video_request, &output_list->video_plane, buf->fb, buf->width, buf->height, zpos);
 if (ret < 0) {
  fprintf(stderr, "prepare atomic commit failed for plane %d, %m\n", output_list->video_plane.id);
  return;
 }
 ret = drmModeAtomicCommit(fd, output_list->video_request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
 if (ret < 0) 
  fprintf(stderr, "modeset atomic commit failed for plane %d, %m\n", output_list->video_plane.id);
*/
//////////

   _ruby_drm_free_object_properties(&s_DRMRuntimeState.objInfoPlane);
   _ruby_drm_free_object_properties(&s_DRMRuntimeState.objInfoCRTc);
   _ruby_drm_free_object_properties(&s_DRMRuntimeState.objInfoConnector);
   
   if ( NULL != s_DRMRuntimeState.pPlanesResources )
      drmModeFreePlaneResources(s_DRMRuntimeState.pPlanesResources);
   s_DRMRuntimeState.pPlanesResources = NULL;

   if ( NULL != s_DRMRuntimeState.pCRTc )
      drmModeFreeCrtc(s_DRMRuntimeState.pCRTc);
   s_DRMRuntimeState.pCRTc = NULL;

   if ( NULL != s_DRMRuntimeState.pEncoder )
      drmModeFreeEncoder(s_DRMRuntimeState.pEncoder);
   s_DRMRuntimeState.pEncoder = NULL;

   if ( NULL != s_DRMRuntimeState.pConnector )
      drmModeFreeConnector(s_DRMRuntimeState.pConnector);
   s_DRMRuntimeState.pConnector = NULL;

   if ( NULL != s_DRMRuntimeState.pAllDRMResources )
      drmModeFreeResources(s_DRMRuntimeState.pAllDRMResources);
   s_DRMRuntimeState.pAllDRMResources = NULL;

   _ruby_drm_destroy_drm_surface_buffer(&s_DRMRuntimeState.drawBuffers[0]);
   _ruby_drm_destroy_drm_surface_buffer(&s_DRMRuntimeState.drawBuffers[1]);

   if ( s_fdDRM >= 0 )
      close(s_fdDRM);
   s_fdDRM = -1;
   s_iDRMCoreInitialized = 0;
   return 0;
}

int ruby_drm_core_get_fd()
{
   return s_fdDRM;
}

type_drm_display_attributes* ruby_drm_get_main_display_info()
{
   return &s_DRMDisplayAttributes;
}

type_drm_buffer* ruby_drm_core_get_main_draw_buffer()
{
   return &(s_DRMRuntimeState.drawBuffers[s_DRMRuntimeState.iActiveOnScreenDrawBuffer]);
}

type_drm_buffer* ruby_drm_core_get_back_draw_buffer()
{
   return &(s_DRMRuntimeState.drawBuffers[1-s_DRMRuntimeState.iActiveOnScreenDrawBuffer]);
}

uint32_t ruby_drm_core_get_main_draw_buffer_id()
{
   return s_DRMRuntimeState.drawBuffers[s_DRMRuntimeState.iActiveOnScreenDrawBuffer].uBufferId;
}
uint32_t ruby_drm_core_get_back_draw_buffer_id()
{
   return s_DRMRuntimeState.drawBuffers[1-s_DRMRuntimeState.iActiveOnScreenDrawBuffer].uBufferId;
}

int ruby_drm_swap_mainback_buffers()
{
   s_DRMRuntimeState.iActiveOnScreenDrawBuffer = 1 - s_DRMRuntimeState.iActiveOnScreenDrawBuffer;
   
   drmModeAtomicSetCursor(s_DRMRuntimeState.pAtomicRequest, 0);

   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "FB_ID", s_DRMRuntimeState.drawBuffers[s_DRMRuntimeState.iActiveOnScreenDrawBuffer].uBufferId );

   int iRet = drmModeAtomicCommit(s_fdDRM, s_DRMRuntimeState.pAtomicRequest, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
   return iRet;

   //if ( (0 == s_iDRMTargetPlaneIndex) || (-1 == s_iDRMTargetPlaneIndex) )
   {
      /*
      int iRet = drmModeSetCrtc(s_fdDRM, s_DRMRuntimeState.objInfoCRTc.uObjId, s_DRMRuntimeState.drawBuffers[s_DRMRuntimeState.iActiveOnScreenDrawBuffer].uBufferId, 0, 0,
         &s_DRMRuntimeState.objInfoConnector.uObjId, 1, &s_DRMRuntimeState.targetModeInfo);
      if ( iRet < 0 )
      {
         log_softerror_and_alarm("[DRMCore] Failed to set new mode.");
         return;
      }
      */
   }
   /*
   else
   {
      drmModeSetPlane( s_fdDRM, (u32)s_iDRMPlaneId, s_uDRMCurrentCrtcId,
                    s_DRMDrawBuffers[s_iDRMActiveOnScreenDrawBuffer].uBufferId,
                    0,
                    0, 0,
                    s_DRMDisplayAttributes.iWidth, s_DRMDisplayAttributes.iHeight,
                    0, 0,
                    ((uint16_t) s_DRMDisplayAttributes.iWidth) << 16, ((uint16_t) s_DRMDisplayAttributes.iHeight) << 16);
   }
   */
}


int ruby_drm_core_set_plane_properties_and_buffer(uint32_t uBufferId)
{
   uint64_t uSrcWidth = s_DRMDisplayAttributes.iWidth;
   uint64_t uSrcHeight = s_DRMDisplayAttributes.iHeight;

   uint64_t zPos = 2;
   uint64_t uCrtX = 0;
   uint64_t uCrtY = 0;
   uint64_t uCrtW = uSrcWidth;
   uint64_t uCrtH = uSrcHeight;
   if ( s_DRMRuntimeState.objInfoPlane.iObjIndex == 0 )
   {
      // OSD plane
      zPos = 4;
   }
   else
   {
      // Video plane
      zPos = 2;

      int iVideoWidth = s_DRMDisplayAttributes.iWidth;
      int iVideoHeight = s_DRMDisplayAttributes.iHeight;
      if ( s_DRMRuntimeState.iVideoSourceWidth > 0 )
         iVideoWidth = s_DRMRuntimeState.iVideoSourceWidth;
      if ( s_DRMRuntimeState.iVideoSourceHeight> 0 )
         iVideoHeight = s_DRMRuntimeState.iVideoSourceHeight;

      uSrcWidth = iVideoWidth;
      uSrcHeight = iVideoHeight;

      // Avoid off by +1/-1 rounding errors
      if ( (iVideoWidth == s_DRMDisplayAttributes.iWidth) && (iVideoHeight == s_DRMDisplayAttributes.iHeight) )
      {
         uCrtX = 0;
         uCrtY = 0;
         uCrtW = iVideoWidth;
         uCrtH = iVideoHeight;
         log_line("[DRMCore] Set video plane 1:1 scalling: srcW: %u, srcH: %u, crtX: %u, crtY: %u, crtW: %u, crtH: %u",
            uSrcWidth, uSrcHeight, uCrtX, uCrtY, uCrtW, uCrtH);
      }
      else
      {
         float fVideoAspectRatio = (float)iVideoWidth/(float)iVideoHeight;
         float fDisplayAspectRatio = (float)s_DRMDisplayAttributes.iWidth/(float)s_DRMDisplayAttributes.iHeight;
         log_line("[DRMCore] Video plane has scalling. screenW: %d, screenH: %d, videoW: %d, videoH: %d",
            s_DRMDisplayAttributes.iWidth, s_DRMDisplayAttributes.iHeight,
            iVideoWidth, iVideoHeight);
         log_line("[DRMCore] Video plane has scalling. video aspect ratio: %f, display aspect ratio: %f",
            fVideoAspectRatio, fDisplayAspectRatio);
         if ( fVideoAspectRatio >= fDisplayAspectRatio )
         {
            uCrtX = 0;
            uCrtW = s_DRMDisplayAttributes.iWidth;
            uCrtH = iVideoHeight * (float) s_DRMDisplayAttributes.iWidth / (float) iVideoWidth;
            uCrtY = (s_DRMDisplayAttributes.iHeight - uCrtH)/2;
         }
         else
         {
            uCrtY = 0;
            uCrtH = s_DRMDisplayAttributes.iHeight;
            uCrtW = iVideoWidth * (float) s_DRMDisplayAttributes.iHeight / (float) iVideoHeight;
            uCrtX = (s_DRMDisplayAttributes.iWidth - uCrtW)/2;
         }
         log_line("[DRMCore] Set video plane custom scalling: srcW: %u, srcH: %u, crtX: %u, crtY: %u, crtW: %u, crtH: %u",
            uSrcWidth, uSrcHeight, uCrtX, uCrtY, uCrtW, uCrtH);

      }
      uSrcWidth = iVideoWidth;
      uSrcHeight = iVideoHeight;
   }
   log_line("[DRMCore] Setting current plane (id: %u, plane index %d) buffer id to %u, zindex %d",
      s_DRMRuntimeState.objInfoPlane.uObjId, s_DRMRuntimeState.objInfoPlane.iObjIndex, uBufferId, (int)zPos);

   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoConnector, "CRTC_ID", s_DRMRuntimeState.objInfoCRTc.uObjId );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoCRTc, "MODE_ID", s_DRMRuntimeState.uModeIdBlob );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoCRTc, "ACTIVE", 1 );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "FB_ID", uBufferId );

   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "CRTC_ID", s_DRMRuntimeState.objInfoCRTc.uObjId );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "CRTC_X", uCrtX );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "CRTC_Y", uCrtY );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "CRTC_W", uCrtW );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "CRTC_H", uCrtH );
   
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "SRC_X", 0 );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "SRC_Y", 0 );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "SRC_W", uSrcWidth<<16 );
   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "SRC_H", uSrcHeight<<16 );

   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "zpos", zPos );

   int iRet = drmModeAtomicCommit(s_fdDRM, s_DRMRuntimeState.pAtomicRequest, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

   log_line("[DRMCore] Done setting current plane (id: %u, index %d) buffer id to %u, zindex %d",
      s_DRMRuntimeState.objInfoPlane.uObjId, s_DRMRuntimeState.objInfoPlane.iObjIndex, uBufferId, (int)zPos);
   return iRet;
}

int ruby_drm_core_set_plane_buffer(uint32_t uBufferId)
{
   drmModeAtomicSetCursor(s_DRMRuntimeState.pAtomicRequest, 0);

   ruby_drm_set_object_property(&s_DRMRuntimeState.objInfoPlane, "FB_ID", uBufferId );
   int iRet = drmModeAtomicCommit(s_fdDRM, s_DRMRuntimeState.pAtomicRequest, s_iDRMEnableVSync?DRM_MODE_ATOMIC_ALLOW_MODESET:DRM_MODE_ATOMIC_NONBLOCK, NULL);
   return iRet;
}


type_drm_object_info* ruby_drm_get_plane_info()
{
  return &s_DRMRuntimeState.objInfoPlane;
}

int ruby_drm_set_object_property(type_drm_object_info* pObject, const char *szName, uint64_t uValue)
{
   if ( (NULL == pObject) || (NULL == szName) )
      return -1;
   uint32_t uPropId = 0;
   int iPropIndex = -1;
   for (int i = 0; i < pObject->pProperties->count_props; i++)
   {
      if ( 0 == strcmp(pObject->ppPropertiesInfo[i]->name, szName) )
      {
         uPropId = pObject->ppPropertiesInfo[i]->prop_id;
         iPropIndex = i;
         break;
      }
   }

   if ( (0 == uPropId) || (-1 == iPropIndex) )
   {
      log_softerror_and_alarm("[DRMCore] Can't set object property. Object id %u has no property named %s", pObject->uObjId, szName);
      return -EINVAL;
   }
 
   if ( 0 != strcmp(szName, "FB_ID") )
      log_line("[DRMCore] Set object id %u property %s (prop index %d) value to: %u",
          pObject->uObjId, szName, iPropIndex, (u32)uValue);
 
   return drmModeAtomicAddProperty(s_DRMRuntimeState.pAtomicRequest, pObject->uObjId, uPropId, uValue);
}

void ruby_drm_set_video_source_size(int iWidth, int iHeight)
{
   s_DRMRuntimeState.iVideoSourceWidth = iWidth;
   s_DRMRuntimeState.iVideoSourceHeight = iHeight;
}

void ruby_drm_enable_vsync(int iEnableVSync)
{
   s_iDRMEnableVSync = iEnableVSync;
}