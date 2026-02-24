#pragma once

#include "../base/hardware.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../public/settings_info.h"


#define PLUGINS_SETTINGS_STAMP_ID "vVII.3"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char szName[MAX_PLUGIN_NAME_LENGTH];
   char szUID[MAX_PLUGIN_NAME_LENGTH];
   int nEnabled;
   int nSettingsCount;
   int nModels;
   u32 uModelsIds[MAX_MODELS];
   float fXPos[MAX_MODELS][MODEL_MAX_OSD_SCREENS], fYPos[MAX_MODELS][MODEL_MAX_OSD_SCREENS], fWidth[MAX_MODELS][MODEL_MAX_OSD_SCREENS], fHeight[MAX_MODELS][MODEL_MAX_OSD_SCREENS]; // 0...1
   int nSettings[MAX_MODELS][MODEL_MAX_OSD_SCREENS][MAX_PLUGIN_SETTINGS];
} SinglePluginSettings;

typedef struct
{
   SinglePluginSettings* pPlugins[MAX_OSD_PLUGINS];
   int nPluginsCount;

} PluginsSettings;

int save_PluginsSettings();
int load_PluginsSettings();
void reset_PluginsSettings();
PluginsSettings* get_PluginsSettings();
SinglePluginSettings* pluginsGetByUID(const char* szUID);
SinglePluginSettings* pluginsSettingsAddNew(const char* szName, const char* szUID);
void delete_PluginByUID(const char* szUID);

int getPluginModelSettingsIndex(SinglePluginSettings* pPlugin, Model* pModel);
int doesPluginHasModelSettings(SinglePluginSettings* pPlugin, Model* pModel);
void deletePluginModelSettings(u32 uModelId);

#ifdef __cplusplus
}
#endif