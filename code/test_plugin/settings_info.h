#pragma once

#include <stdint.h>

#define PLUGIN_VAR __attribute__ ((visibility("hidden")))

#define MAX_PLUGIN_SETTINGS 10

#define PLUGIN_SETTING_TYPE_BOOL 1
#define PLUGIN_SETTING_TYPE_INT 2
#define PLUGIN_SETTING_TYPE_ENUM 3

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef struct
{
   u32 uFlags;
   int nSettingsValues[MAX_PLUGIN_SETTINGS];
   float fLineThicknessPx;
   float fOutlineThicknessPx;
   float fBackgroundAlpha;
   void* pExtraInfo; // pointer to extra info structure below
} __attribute__((packed)) plugin_settings_info_t2;


typedef struct
{
   int iMeasureUnitsType;
   // 0 - metric, km
   // 1 - imperial, mi
   // 2 - metric, meters
   // 3 - imperial, ft
} __attribute__((packed)) plugin_settings_info_t2_extra;

#ifdef __cplusplus
}
#endif
