/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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
#include "core_plugins_settings.h"
#include "../public/ruby_core_plugin.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "flags.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

int s_CorePluginsSettingsLoaded = 0;

CorePluginRuntimeInfo s_CorePluginsRuntimeInfo[MAX_CORE_PLUGINS_COUNT];
int s_iCorePluginsRuntimeCount = 0;

CorePluginSettings s_CorePluginsSettings[MAX_CORE_PLUGINS_COUNT];
int s_iCorePluginsSettingsCount = 0;

void reset_CorePluginsSettings()
{
   
   log_line("Reseted core plugins settings.");
}

int save_CorePluginsSettings()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CORE_PLUGINS_SETTINGS);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save core plugins settings to file: %s", szFile);
      return 0;
   }
   fprintf(fd, "%s\n", CORE_PLUGINS_SETTINGS_STAMP_ID);
   fprintf(fd, "%d\n", s_iCorePluginsSettingsCount);
   
   for( int i=0; i<s_iCorePluginsSettingsCount; i++ )
   {
      char szBuff[256];
      strcpy(szBuff, s_CorePluginsSettings[i].szName);
      for( int k=0; k<(int)strlen(szBuff); k++ )
      if ( szBuff[k] == ' ' || (szBuff[k] < 32) )
         szBuff[k] = '*';

      fprintf(fd, "%s\n", s_CorePluginsSettings[i].szGUID);
      fprintf(fd, "%s\n", szBuff);
      fprintf(fd, "%d\n", s_CorePluginsSettings[i].iVersion);
      fprintf(fd, "%d %u %u\n", s_CorePluginsSettings[i].iEnabled, s_CorePluginsSettings[i].uRequestedCapabilities, s_CorePluginsSettings[i].uAllocatedCapabilities);
   }
   fclose(fd);

   log_line("Saved core plugins settings to file: %s", szFile);
   return 1;
}

int load_CorePluginsSettings()
{
   s_CorePluginsSettingsLoaded = 1;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CORE_PLUGINS_SETTINGS);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load core plugins settings from file: %s (missing file). Reseted core plugins settings to default.", szFile);
      reset_CorePluginsSettings();
      save_CorePluginsSettings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, CORE_PLUGINS_SETTINGS_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to load core plugins settings from file: %s (invalid config file version). Reseted core plugins settings to default.", szFile);
      reset_CorePluginsSettings();
      save_CorePluginsSettings();
      return 0;
   }

   if ( 1 != fscanf(fd, "%d", &s_iCorePluginsSettingsCount) )
      { s_iCorePluginsSettingsCount = 0; failed = 10; }
   
   for( int i=0; i<s_iCorePluginsSettingsCount; i++ )
   {
      if ( (!failed) && (1 != fscanf(fd, "%s", s_CorePluginsSettings[i].szGUID)) )
         { failed = 11; }
      if ( (!failed) && (1 != fscanf(fd, "%s", s_CorePluginsSettings[i].szName)) )
         { failed = 11; }
      if ( (!failed) && (strlen(s_CorePluginsSettings[i].szGUID) < 6) )
         { failed = 15; }
      if ( (!failed) && (1 != fscanf(fd, "%d", &s_CorePluginsSettings[i].iVersion)) )
         { failed = 11; }
      if ( (!failed) && (3 != fscanf(fd, "%d %u %u", &s_CorePluginsSettings[i].iEnabled, &s_CorePluginsSettings[i].uRequestedCapabilities, &s_CorePluginsSettings[i].uAllocatedCapabilities)) )
         { failed = 12; }
      if ( failed )
      {
         s_iCorePluginsSettingsCount = i;
         break;
      }
      for( int k=0; k<(int)strlen(s_CorePluginsSettings[i].szName); k++ )
         if ( s_CorePluginsSettings[i].szName[k] == '*' )
            s_CorePluginsSettings[i].szName[k] = ' ';
   }

   fclose(fd);

   // Validate settings
 
   if ( s_iCorePluginsSettingsCount < 0 || s_iCorePluginsSettingsCount > MAX_CORE_PLUGINS_COUNT )
   {
      s_iCorePluginsSettingsCount = 0;
      failed = 20;
   }

   if ( failed )
   {
      log_line("Incomplete/Invalid core plugins settings file %s, error code: %d. Reseted to default.", szFile, failed);
      reset_CorePluginsSettings();
      save_CorePluginsSettings();
   }
   else
      log_line("Loaded core plugins settings from file: %s", szFile);
   return 1;
}

CorePluginSettings* get_CorePluginSettings(char* szPluginGUID)
{
   if ( 0 == s_CorePluginsSettingsLoaded )
      load_CorePluginsSettings();

   if ( NULL == szPluginGUID || 0 == szPluginGUID[0] || 0 == s_iCorePluginsSettingsCount )
      return NULL;

   for( int i=0; i<s_iCorePluginsSettingsCount; i++ )
      if ( 0 == strcmp(szPluginGUID, s_CorePluginsSettings[i].szGUID) )
         return &(s_CorePluginsSettings[i]);

   return NULL;
}

int _load_CorePlugin(char* szFileName, int iEnumerateOnly)
{
   char szFile[256];
   int iIsNew = 0;

   if ( NULL == szFileName || 0 == szFileName[0] )
      return -1;
   if ( s_iCorePluginsRuntimeCount >= MAX_CORE_PLUGINS_COUNT )
      return -2;

   sprintf(szFile, "%s%s", FOLDER_CORE_PLUGINS, szFileName);

   long lSize = get_filesize(szFile);
   log_line("Try to load plugin from file: [%s], %u bytes", szFile, lSize);

   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary = dlopen(szFile, RTLD_LAZY | RTLD_GLOBAL);
   if ( NULL == s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary )
   {
      log_softerror_and_alarm("[CorePlugins] Can't load plugin from file [%s]", szFile);
      return -3;
   }

   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreInit = (int (*)(u32, u32)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_init");
   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreRequestCapab = (u32 (*)(void)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_on_requested_capabilities");
   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetName = (const char* (*)(void)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_get_name");
   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetUID = (const char* (*)(void)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_get_guid");

   if ( NULL == s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreInit ||
        NULL == s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreRequestCapab ||
        NULL == s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetName ||
        NULL == s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetUID )
   {
      log_softerror_and_alarm("[CorePlugins] Can't load plugin from file [%s], does not export the required functions.", szFile);
      dlclose(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary);
      s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary = NULL;
      return -5;
   }

   const char* szPluginName = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetName))();
   const char* szPluginGUID = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetUID))();
   
   if ( NULL == szPluginName || NULL == szPluginGUID )
   {
      log_softerror_and_alarm("[CorePlugins] Can't load plugin from file [%s], does not export a valid GUID and name.", szFile);
      dlclose(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary);
      s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary = NULL;
      return -6;
   }

   strncpy(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName, szPluginName, 64);
   strncpy(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID, szPluginGUID, 32);

   for( int i=0; i<(int)strlen(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName); i++ )
      if ( s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName[i] == ' ' ||
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName[i] == 10 ||
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName[i] == 13 )
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName[i] = '_';

   for( int i=0; i<(int)strlen(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID); i++ )
      if ( s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID[i] == ' ' ||
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID[i] == 10 ||
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID[i] == 13 )
         s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID[i] = '_';

   int bDuplicateGUID = 0;
   for( int i=0; i<s_iCorePluginsRuntimeCount; i++ )
      if ( 0 == strcmp(s_CorePluginsRuntimeInfo[i].szGUID, s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID) )
         bDuplicateGUID = 1;

   if ( bDuplicateGUID )
   {
      log_softerror_and_alarm("[CorePlugins] Duplicate GUID for plugin found. Plugin not loaded [%s]", szFile);
      dlclose(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary);
      s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary = NULL;
      return -7;
   }

   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreUninit = (void (*)(void)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_uninit");
   s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetVersion = (int (*)(void)) dlsym(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary, "core_plugin_get_version");
   strcpy(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szFile, szFile);

   if ( NULL == get_CorePluginSettings(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID) )
   {
      if ( s_iCorePluginsSettingsCount < MAX_CORE_PLUGINS_COUNT )
      {
         iIsNew = 1;
         strncpy(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szName, s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName, sizeof(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szName)/sizeof(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szName[0])-1); 
         strncpy(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szGUID, s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID, sizeof(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szGUID)/sizeof(s_CorePluginsSettings[s_iCorePluginsSettingsCount].szGUID[0])-1); 
         s_CorePluginsSettings[s_iCorePluginsSettingsCount].iEnabled = 1;
         s_CorePluginsSettings[s_iCorePluginsSettingsCount].iVersion = 0;
         
         if ( NULL != s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetVersion )
            s_CorePluginsSettings[s_iCorePluginsSettingsCount].iVersion = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetVersion))();

         if ( NULL != s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreRequestCapab )
            s_CorePluginsSettings[s_iCorePluginsSettingsCount].uRequestedCapabilities = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreRequestCapab))();
   
         s_CorePluginsSettings[s_iCorePluginsSettingsCount].uAllocatedCapabilities = s_CorePluginsSettings[s_iCorePluginsSettingsCount].uRequestedCapabilities;
         s_iCorePluginsSettingsCount++;
         save_CorePluginsSettings();
      }
   }

   u32 uRequestedCapabilities = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreRequestCapab))();
   CorePluginSettings* pSettings = get_CorePluginSettings(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID);
   if ( NULL != pSettings )
   {
      pSettings->uRequestedCapabilities = uRequestedCapabilities;
      pSettings->uAllocatedCapabilities = uRequestedCapabilities;
      if ( NULL != s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetVersion )
         pSettings->iVersion = (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreGetVersion))();
   }

   if ( ! iEnumerateOnly )
      (*(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pFunctionCoreInit))(CORE_PLUGIN_RUNTIME_LOCATION_CONTROLLER, uRequestedCapabilities);
   else
   {
      dlclose(s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary);
      s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].pLibrary = NULL;
   }
   log_line("[CorePlugins] Loaded plugin%s: name: [%s], GUID: [%s], file: [%s]", (iEnumerateOnly?" in enumeration mode only:":""), s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szName, s_CorePluginsRuntimeInfo[s_iCorePluginsRuntimeCount].szGUID, szFile);
   s_iCorePluginsRuntimeCount++;
   return iIsNew;
}

void load_CorePlugins(int iEnumerateOnly)
{
   DIR *d;
   struct dirent *dir;
   int iSave = 0;

   log_line("[CorePlugins] Searching for plugins%s...", (iEnumerateOnly?" (in enumeration mode only)":""));
   
   unload_CorePlugins();

   load_CorePluginsSettings();

   d = opendir(FOLDER_CORE_PLUGINS);
   if (!d)
   {
      log_softerror_and_alarm("[CorePlugins] Failed to search for plugins.");
      return;
   }

   while ((dir = readdir(d)) != NULL)
   {
      if ( strlen(dir->d_name) < 4 )
         continue;
      if ( NULL == strstr(dir->d_name, ".so") )
         continue;
      if ( 1 == _load_CorePlugin(dir->d_name, iEnumerateOnly) )
         iSave = 1;
   }
   closedir(d);

   log_line("[CorePlugins] Loaded %d core plugins.%s", s_iCorePluginsRuntimeCount, iEnumerateOnly?" (Enumeration mode only)":"");
   if ( iSave )
      save_CorePluginsSettings();
}


void unload_CorePlugins()
{
   log_line("[CorePlugins] Unloading %d plugins...", s_iCorePluginsRuntimeCount);
   
   for( int i=0; i<s_iCorePluginsRuntimeCount; i++ )
   {
      if ( NULL == s_CorePluginsRuntimeInfo[i].pLibrary )
         continue;
      if ( NULL != s_CorePluginsRuntimeInfo[i].pFunctionCoreUninit )
         (*(s_CorePluginsRuntimeInfo[i].pFunctionCoreUninit))();
   
      dlclose(s_CorePluginsRuntimeInfo[i].pLibrary);
      s_CorePluginsRuntimeInfo[i].pLibrary = NULL;
      log_line("[CorePlugins] Unloaded plugin [%s]", s_CorePluginsRuntimeInfo[i].szName);
   }

   s_iCorePluginsRuntimeCount = 0;
   log_line("[CorePlugins] Unloaded plugins.");
}


void refresh_CorePlugins(int iEnumerateOnly)
{
   unload_CorePlugins();
   load_CorePlugins(iEnumerateOnly);
}

void delete_CorePlugin(char* szGUID)
{
   if ( NULL == szGUID || 0 == szGUID[0] )
      return;

   int iIndex = -1;
   for( int i=0; i<s_iCorePluginsRuntimeCount; i++ )
      if ( 0 == strcmp(szGUID, s_CorePluginsRuntimeInfo[i].szGUID) )
      {
         iIndex = i;
         break;
      }

   if ( -1 == iIndex )
      return;

   if ( NULL != s_CorePluginsRuntimeInfo[iIndex].pLibrary )
   {
      if ( NULL != s_CorePluginsRuntimeInfo[iIndex].pFunctionCoreUninit )
         (*(s_CorePluginsRuntimeInfo[iIndex].pFunctionCoreUninit))();
   
      dlclose(s_CorePluginsRuntimeInfo[iIndex].pLibrary);
      s_CorePluginsRuntimeInfo[iIndex].pLibrary = NULL;
   }
      
   log_line("[CorePlugins] Deleted plugin [%s]", s_CorePluginsRuntimeInfo[iIndex].szName);

   char szComm[256];
   sprintf(szComm, "rm -rf %s", s_CorePluginsRuntimeInfo[iIndex].szFile);
   hw_execute_bash_command(szComm, NULL);

   for( int i=iIndex; i<s_iCorePluginsRuntimeCount-1; i++ )
      memcpy((u8*)&(s_CorePluginsRuntimeInfo[i]), (u8*)&(s_CorePluginsRuntimeInfo[i+1]), sizeof(CorePluginRuntimeInfo));
   s_iCorePluginsRuntimeCount--;

   for( int i=0; i<s_iCorePluginsSettingsCount; i++ )
   {
      if ( 0 == strcmp(szGUID, s_CorePluginsSettings[i].szGUID) )
      {
         for( int k=i; k<s_iCorePluginsSettingsCount-1; k++ )
            memcpy((u8*)&(s_CorePluginsSettings[k]), (u8*)&(s_CorePluginsSettings[k+1]), sizeof(CorePluginSettings));
         s_iCorePluginsSettingsCount--;
         save_CorePluginsSettings();
         break;
      }
   }
}

int get_CorePluginsCount()
{
   return s_iCorePluginsRuntimeCount;
}

char* get_CorePluginName(int iPluginIndex)
{
   if ( iPluginIndex < 0 || iPluginIndex >= s_iCorePluginsRuntimeCount )
      return NULL;

   return s_CorePluginsRuntimeInfo[iPluginIndex].szName;
}
char* get_CorePluginGUID(int iPluginIndex)
{
   if ( iPluginIndex < 0 || iPluginIndex >= s_iCorePluginsRuntimeCount )
      return NULL;

   return s_CorePluginsRuntimeInfo[iPluginIndex].szGUID;
}

