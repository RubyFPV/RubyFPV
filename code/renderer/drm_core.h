#pragma once
#ifndef RUBY_DRM_CORE
#define RUBY_DRM_CORE

#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int iWidth;
  int iHeight;
  int iBPP;
  int iRefreshRate;
  int iInterleaved;
  drmModeModeInfo currentMode;
} type_drm_display_attributes;

typedef struct
{
   uint32_t uObjType;
   uint32_t uObjId;
   int iObjIndex;
   drmModeObjectProperties *pProperties;
   drmModePropertyRes **ppPropertiesInfo;
} type_drm_object_info;

typedef struct
{
  uint32_t uWidth;
  uint32_t uHeight;
  uint32_t uStride;
  uint32_t uSize;
  uint32_t uHandle;
  uint8_t *pData;
  uint32_t uBufferId;
} type_drm_buffer;

typedef struct
{
   drmModeRes* pAllDRMResources;

   drmModeConnector* pConnector;
   type_drm_object_info objInfoConnector;
   drmModeModeInfo targetModeInfo;
   uint32_t uModeIdBlob;

   drmModeEncoder* pEncoder;

   drmModeCrtc* pOriginalCRTc;
   drmModeCrtc* pCRTc;
   type_drm_object_info objInfoCRTc;

   drmModePlaneResPtr pPlanesResources;
   drmModePlanePtr pPlane;
   type_drm_object_info objInfoPlane;
   uint32_t uPlaneFormat;
   int iPlaneFormatIndex;

   type_drm_buffer drawBuffers[2];
   int iActiveOnScreenDrawBuffer;

   drmModeAtomicReq* pAtomicRequest;

   int iVideoSourceWidth;
   int iVideoSourceHeight;
} type_drm_runtime_state;

int ruby_drm_core_is_display_connected();
int ruby_drm_core_wait_for_display_connected();

int ruby_drm_core_init(int iPlaneIndex, uint32_t uFormat, int iWidth, int iHeight, int iRefreshRate);
int ruby_drm_core_uninit();
int ruby_drm_core_get_fd();

type_drm_display_attributes* ruby_drm_get_main_display_info();

type_drm_buffer* ruby_drm_core_get_main_draw_buffer();
type_drm_buffer* ruby_drm_core_get_back_draw_buffer();
uint32_t ruby_drm_core_get_main_draw_buffer_id();
uint32_t ruby_drm_core_get_back_draw_buffer_id();
int ruby_drm_swap_mainback_buffers();

int ruby_drm_core_set_plane_properties_and_buffer(uint32_t uBufferId);
int ruby_drm_core_set_plane_buffer(uint32_t uBufferId);

type_drm_object_info* ruby_drm_get_plane_info();
int ruby_drm_set_object_property(type_drm_object_info* pObject, const char *szName, uint64_t uValue);

void ruby_drm_set_video_source_size(int iWidth, int iHeight);
void ruby_drm_enable_vsync(int iEnableVSync);

#ifdef __cplusplus
}
#endif

#endif