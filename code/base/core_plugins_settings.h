#pragma once

#include "../base/hardware.h"

#define CORE_PLUGINS_SETTINGS_STAMP_ID "vVII.0"

#define MAX_CORE_PLUGINS_COUNT 7

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   void* pLibrary;
   int (*pFunctionCoreInit)(u32, u32);
   void (*pFunctionCoreUninit)(void);
   int (*pFunctionCoreGetVersion)(void);
   u32 (*pFunctionCoreRequestCapab)(void);
   const char* (*pFunctionCoreGetName)(void);
   const char* (*pFunctionCoreGetUID)(void);

   char szFile[256];
   char szName[128];
   char szGUID[128];

} CorePluginRuntimeInfo;

typedef struct
{
   char szName[64];
   char szGUID[32];
   int iEnabled;
   int iVersion;
   u32 uRequestedCapabilities;
   u32 uAllocatedCapabilities;
} CorePluginSettings;

int save_CorePluginsSettings();
int load_CorePluginsSettings();
void reset_CorePluginsSettings();

CorePluginSettings* get_CorePluginSettings(char* szPluginGUID);

void load_CorePlugins(int iEnumerateOnly);
void unload_CorePlugins();
void refresh_CorePlugins(int iEnumerateOnly);
void delete_CorePlugin(char* szGUID);

int get_CorePluginsCount();
char* get_CorePluginName(int iPluginIndex);
char* get_CorePluginGUID(int iPluginIndex);

#ifdef __cplusplus
}
#endif