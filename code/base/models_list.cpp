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
#include "hardware.h"
#include "hardware_procs.h"
#include "models.h"

Model* s_pModelsSpectator[MAX_MODELS_SPECTATOR];
int s_iModelsSpectatorCount = 0;

Model* s_pModels[MAX_MODELS];
int s_iModelsCount = 0;

Model* s_pCurrentModel = NULL;

static bool s_bLoadedAllModels = false;

bool loadAllModels()
{
   log_line("Loading all models from storage...");
   s_bLoadedAllModels = true;
  
   bool bSucceeded = true;

   if ( NULL != s_pCurrentModel )
      delete s_pCurrentModel;

   s_pCurrentModel = new Model();

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);

   char szFileBackup[MAX_FILE_PATH_SIZE];
   strcpy(szFileBackup, FOLDER_CONFIG);
   strcat(szFileBackup, FILE_CONFIG_CURRENT_VEHICLE_MODEL_BACKUP);

   if ( ! s_pCurrentModel->loadFromFile(szFile, true) )
   {
      log_softerror_and_alarm("Failed to load current model.");
      if( access(szFile, R_OK) == -1 )
      if( access(szFileBackup, R_OK) == -1 )
         log_softerror_and_alarm("No current vehicle model file present.");
      s_pCurrentModel->resetToDefaults(true);
      bSucceeded = false;
   }

   log_line("Current model: [%s], VID: %u", s_pCurrentModel->getLongName(), s_pCurrentModel->uVehicleId);

   if ( hardware_is_vehicle() )
      return bSucceeded;

   s_iModelsCount = 0;
   s_iModelsSpectatorCount = 0;

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_COUNT);
   int count = load_simple_config_fileI(szFile, 0);
   if (count < 0 )
      count = 0;
   log_line("Loading %d controller models...", count);
   for( int i=0; i<count; i++ )
   {
      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_CONTROLL);
      sprintf(szFile, szFolderM, i);
      s_pModels[i] = new Model();
      if ( ! s_pModels[i]->loadFromFile(szFile, true) )
         break;

      if ( s_pCurrentModel->uVehicleId == s_pModels[i]->uVehicleId )
         s_pModels[i] = s_pCurrentModel;
      s_iModelsCount++;
   }
   log_line("Loaded %d controller models.", s_iModelsCount);

   s_iModelsSpectatorCount = 0;
   while( s_iModelsSpectatorCount < MAX_MODELS_SPECTATOR )
   {
      char szBuff[256];
      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
      sprintf(szBuff, szFolderM, s_iModelsSpectatorCount);
      s_pModelsSpectator[s_iModelsSpectatorCount] = new Model();
      if( access( szBuff, R_OK ) == -1 )
         break;
      if ( ! s_pModelsSpectator[s_iModelsSpectatorCount]->loadFromFile(szBuff, true) )
         break;
      if ( s_pCurrentModel->uVehicleId == s_pModelsSpectator[s_iModelsSpectatorCount]->uVehicleId )
         s_pModelsSpectator[s_iModelsSpectatorCount] = s_pCurrentModel;
      s_iModelsSpectatorCount++;
   }
   log_line("Loaded %d spectator models.", s_iModelsSpectatorCount);

   log_line("Loaded controller models (%d):", s_iModelsCount);
   for( int i=0; i<s_iModelsCount; i++ )
      log_line("Controller model %d: [%s], VID: %u, freq1: %d Mhz, ptr: %X", i+1, s_pModels[i]->getLongName(), s_pModels[i]->uVehicleId, s_pModels[i]->radioLinksParams.link_frequency_khz[0]/1000, s_pModels[i]);

   return true;
}

bool saveCurrentModel()
{
   if ( NULL == s_pCurrentModel )
   {
      log_softerror_and_alarm("Current model is NULL. Can't save it.");
      return false;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);

   log_line("Saving model VID %u as current model", s_pCurrentModel->uVehicleId);
   s_pCurrentModel->saveToFile(szFile, hardware_is_station());

   if ( hardware_is_vehicle() )
      return true;

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->uVehicleId != s_pCurrentModel->uVehicleId )
         continue;
      char szBuff[256];
      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
      sprintf(szBuff, szFolderM, i);
      s_pModelsSpectator[i]->saveToFile(szBuff, hardware_is_station());
   }

   log_line("Saving to controller models.", s_iModelsCount);
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_COUNT);
   save_simple_config_fileI(szFile, s_iModelsCount);
   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->uVehicleId != s_pCurrentModel->uVehicleId )
         continue;
      log_line("Saving model VID %u as controller model index %d", s_pCurrentModel->uVehicleId, i);
      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_CONTROLL);
      sprintf(szFile, szFolderM, i);
      s_pModels[i]->saveToFile(szFile, hardware_is_station());
   }

   log_line("Saved controller models (%d):", s_iModelsCount);
   for( int i=0; i<s_iModelsCount; i++ )
      log_line("Controller model %d: [%s], VID: %u, freq1: %d Mhz, ptr: %X", i+1, s_pModels[i]->getLongName(), s_pModels[i]->uVehicleId, s_pModels[i]->radioLinksParams.link_frequency_khz[0]/1000, s_pModels[i]);

   return true;
}

Model* getCurrentModel()
{
   if ( NULL == s_pCurrentModel )
      s_pCurrentModel = new Model();
   return s_pCurrentModel;
}

bool reloadCurrentModel()
{
   if ( NULL == s_pCurrentModel )
      return false;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! s_pCurrentModel->loadFromFile(szFile) )
      return false;

   return true;
}

void setCurrentModel(u32 uVehicleId)
{
   for( int i=0; i<s_iModelsCount; i++ )
   {
       if ( (NULL != s_pModels[i]) && (s_pModels[i]->uVehicleId == uVehicleId) )
       {
          log_line("Set current vehicle: Controller's vehicle index %d (VID %u)", i, uVehicleId);
          s_pCurrentModel = s_pModels[i];
          return;
       }
   }
   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
       if ( (NULL != s_pModelsSpectator[i]) && (s_pModelsSpectator[i]->uVehicleId == uVehicleId) )
       {
          log_line("Set current vehicle: Controller's spectator vehicle index %d (VID %u)", i, uVehicleId);
          s_pCurrentModel = s_pModelsSpectator[i];
          return;
       }
   }
}

int getControllerModelsCount()
{
   return s_iModelsCount;
}

int getControllerModelsSpectatorCount()
{
   return s_iModelsSpectatorCount;
}

void deleteAllModels()
{
   log_line("Deleted all controller models.");
   s_iModelsSpectatorCount = 0;
   s_iModelsCount = 0;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_COUNT);
   save_simple_config_fileI(szFile, s_iModelsCount);
}


Model* getSpectatorModel(int iIndex)
{
   if ( iIndex < 0 || iIndex > s_iModelsSpectatorCount )
      return NULL;
   return s_pModelsSpectator[iIndex];
}


Model* addSpectatorModel(u32 vehicleId)
{
   int index = 0;
   for( index = 0; index < s_iModelsSpectatorCount; index++ )
   {
      if ( s_pModelsSpectator[index]->uVehicleId == vehicleId )
      {
         // Move it to top of the list;
         Model* tmp = s_pModelsSpectator[index];
         for( int i=index-1; i >=0; i-- )
         {
            s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
         }
         s_pModelsSpectator[0] = tmp;
         return s_pModelsSpectator[0];
      }
   }
   // New vehicle, add it on top of the list, move the other ones down the list.

   for( int i=s_iModelsSpectatorCount-1; i >= 0; i-- )
   {
      if ( i < MAX_MODELS_SPECTATOR-1 )
         s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
   }
   s_pModelsSpectator[0] = new Model();
   s_pModelsSpectator[0]->resetToDefaults(false);
   s_pModelsSpectator[0]->uVehicleId = vehicleId;

   s_iModelsSpectatorCount++;
   if ( s_iModelsSpectatorCount > MAX_MODELS_SPECTATOR )
      s_iModelsSpectatorCount = MAX_MODELS_SPECTATOR;

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      char szBuff[256];
      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
      sprintf(szBuff, szFolderM, i);
      s_pModelsSpectator[i]->saveToFile(szBuff, true);
   }

   return s_pModelsSpectator[0];
}

void moveSpectatorModelToTop(int index)
{
   if ( index < 0 || index >= s_iModelsSpectatorCount )
      return;
        
   Model* tmp = s_pModelsSpectator[index];
   for( int i=index-1; i >=0; i-- )
   {
      s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
   }
   s_pModelsSpectator[0] = tmp;
}

Model* getModelAtIndex(int index) 
{
   if ( index < 0 || index >= MAX_MODELS )
      return NULL;
   return s_pModels[index];
}

Model* addNewModel(u32 uVehicleId, int iVersionMajor, int iVersionMinor)
{
   if ( s_iModelsCount >= MAX_MODELS-1 )
      return NULL;
   log_line("Adding a new model in the controller's models list for VID %u, SW: %d.%d ...",
      uVehicleId, iVersionMajor, iVersionMinor);

   s_pModels[s_iModelsCount] = new Model();
   s_pModels[s_iModelsCount]->resetToDefaults(false);
   s_pModels[s_iModelsCount]->uVehicleId = uVehicleId;
   s_pModels[s_iModelsCount]->sw_version = (iVersionMajor * 256 + iVersionMinor);

   char szBuff[256];
   char szFolderM[MAX_FILE_PATH_SIZE];
   strcpy(szFolderM, FOLDER_CONFIG_MODELS);
   strcat(szFolderM, FILE_VEHICLE_CONTROLL);
   sprintf(szBuff, szFolderM, s_iModelsCount);
   s_pModels[s_iModelsCount]->saveToFile(szBuff, true);
   s_iModelsCount++;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_COUNT);
   save_simple_config_fileI(szFile, s_iModelsCount);
   
   log_line("Added a new model in the controller's models list, VID: %u, software version: %d.%d, b-%d", s_pModels[s_iModelsCount-1]->uVehicleId, get_sw_version_major(s_pModels[s_iModelsCount-1]), get_sw_version_minor(s_pModels[s_iModelsCount-1]), get_sw_version_build(s_pModels[s_iModelsCount-1]));
   return s_pModels[s_iModelsCount-1];
}

void replaceModel(int index, Model* pModel)
{
   if ( index < 0 || index >= MAX_MODELS-1 )
      return;

   if ( (NULL != s_pCurrentModel) && (NULL != s_pModels[index]) )
   if ( s_pCurrentModel->uVehicleId == s_pModels[index]->uVehicleId )
      s_pCurrentModel = pModel;
   
   if ( NULL == s_pModels[index] )
      log_line("Replacing controller model %d, which is NULL, with new VID %u, ptr: %X",
          index, pModel->uVehicleId, pModel);
   else
      log_line("Replacing controller model %d, VID %u, ptr: %X with new VID %u, ptr: %X",
          index, s_pModels[index]->uVehicleId, s_pModels[index],
          pModel->uVehicleId, pModel);

   s_pModels[index] = pModel;

   if ( NULL == s_pCurrentModel )
      log_line("Current model is NULL");
   else
      log_line("Current model VID: %u, ptr: %X", s_pCurrentModel->uVehicleId, s_pCurrentModel);
}

Model* findModelWithId(u32 uVehicleId, u32 uSrcId)
{
   if ( ! s_bLoadedAllModels )
   {
      log_line("Models not loaded. Loading models first.");
      loadAllModels();
   }
   if ( NULL != s_pCurrentModel )
   if ( s_pCurrentModel->uVehicleId == uVehicleId )
      return s_pCurrentModel;

   for( int i=0; i<s_iModelsCount; i++ )
      if ( s_pModels[i]->uVehicleId == uVehicleId )
         return s_pModels[i];

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
      if ( s_pModelsSpectator[i]->uVehicleId == uVehicleId )
         return s_pModelsSpectator[i];

   log_softerror_and_alarm("Tried to find an inexistent VID: %u (source id: %u). Current loaded vehicles:", uVehicleId, uSrcId);
   for( int i=0; i<s_iModelsCount; i++ )
      log_softerror_and_alarm("Vehicle Ctrlr %d: %u", i, s_pModels[i]->uVehicleId);
   for( int i=0; i<s_iModelsSpectatorCount; i++ )
      log_softerror_and_alarm("Vehicle Spect %d: %u", i, s_pModelsSpectator[i]->uVehicleId);
   if ( NULL == s_pCurrentModel )
      log_softerror_and_alarm("Current vehicle: NULL");
   else
      log_softerror_and_alarm("Current vehicle: %u", s_pCurrentModel->uVehicleId);
   return NULL;
}

bool modelIsInControllerList(u32 uVehicleId)
{
   if ( ! s_bLoadedAllModels )
   {
      log_line("Models not loaded. Loading models first.");
      loadAllModels();
   }
   for( int i=0; i<s_iModelsCount; i++ )
      if ( s_pModels[i]->uVehicleId == uVehicleId )
         return true;
   return false;
}

bool modelIsInSpectatorList(u32 uVehicleId)
{
   if ( ! s_bLoadedAllModels )
   {
      log_line("Models not loaded. Loading models first.");
      loadAllModels();
   }
   for( int i=0; i<s_iModelsSpectatorCount; i++ )
      if ( s_pModelsSpectator[i]->uVehicleId == uVehicleId )
         return true;
   return false;
}

bool controllerHasModelWithId(u32 uVehicleId)
{
   if ( ! s_bLoadedAllModels )
   {
      log_line("Models not loaded. Loading models first.");
      loadAllModels();
   }

   if ( modelIsInControllerList(uVehicleId) )
      return true;
   if ( modelIsInSpectatorList(uVehicleId) )
      return true;
   return false;
}

// Returns the current selected model, if one still remains

Model* deleteModel(Model* pModel)
{
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Tried to delete a NULL model object.");
      return s_pCurrentModel;
   }

   if ( hardware_is_vehicle() )
   {
      log_softerror_and_alarm("Tried to delete a model (VID %u, ptr: %X) on vehicle side.", pModel->uVehicleId, pModel);
      return s_pCurrentModel;
   }

   if ( (0 == s_iModelsCount) && (0 == s_iModelsSpectatorCount) )
   {
      log_softerror_and_alarm("Tried to delete a model when there are none in the list.");
      return s_pCurrentModel;
   }

   char szFile[MAX_FILE_PATH_SIZE];      
   bool bDeletedController = false;
   bool bDeletedSpectator = false;
   int pos = 0;

   for( pos=0; pos<s_iModelsCount; pos++ )
   {
      if ( s_pModels[pos]->uVehicleId != pModel->uVehicleId )
         continue;

      log_line("Deleting controller model index %d: VID %u, ptr: %X", pos, pModel->uVehicleId, pModel);
      
      if ( (NULL != s_pCurrentModel) && (s_pModels[pos]->uVehicleId == s_pCurrentModel->uVehicleId) )
      {
         log_line("Model to delete is also the current model. Delete it too.");
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL_BACKUP);
         unlink(szFile);
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         unlink(szFile);
         log_line("Deleted current vehicle (VID %u, ptr: %X) model file: %s", s_pCurrentModel->uVehicleId, s_pCurrentModel, szFile);
         s_pCurrentModel = NULL;
      }
      
      for( int i=pos; i<s_iModelsCount-1; i++ )
         s_pModels[i] = s_pModels[i+1];
      s_iModelsCount--;

      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_CONTROLL);
      sprintf(szFile, szFolderM, s_iModelsCount);
      log_line("Remove model file: (%s)", szFile);
      unlink(szFile);
      szFile[strlen(szFile)-3] = 0;
      strcat(szFile, "bak");
      log_line("Remove model file: (%s)", szFile);
      unlink(szFile);
      
      log_line("Saving %d controller models.", s_iModelsCount);
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_COUNT);
      save_simple_config_fileI(szFile, s_iModelsCount);
      for( int i=pos; i<s_iModelsCount; i++ )
      {
         strcpy(szFolderM, FOLDER_CONFIG_MODELS);
         strcat(szFolderM, FILE_VEHICLE_CONTROLL);
         sprintf(szFile, szFolderM, i);
         s_pModels[i]->saveToFile(szFile, hardware_is_station());
      }
      bDeletedController = true;
      break;
   }

   pos = 0;
   for( pos=0; pos<s_iModelsSpectatorCount; pos++ )
   {
      if ( s_pModelsSpectator[pos]->uVehicleId != pModel->uVehicleId )
         continue;

      log_line("Deleting spectator model index %d: VID %u, ptr: %X", pos, pModel->uVehicleId, pModel);
      
      if ( (NULL != s_pCurrentModel) && (s_pModelsSpectator[pos]->uVehicleId == s_pCurrentModel->uVehicleId) )
      {
         log_line("Model to delete is also the current spectator model. Delete it too.");
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL_BACKUP);
         unlink(szFile);
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         unlink(szFile);
         log_line("Deleted current vehicle (VID %u, ptr: %X) model file: %s", s_pCurrentModel->uVehicleId, s_pCurrentModel, szFile);
         s_pCurrentModel = NULL;
      }
      
      for( int i=pos; i<s_iModelsSpectatorCount-1; i++ )
         s_pModelsSpectator[i] = s_pModelsSpectator[i+1];
      s_iModelsSpectatorCount--;

      char szFolderM[MAX_FILE_PATH_SIZE];
      strcpy(szFolderM, FOLDER_CONFIG_MODELS);
      strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
      sprintf(szFile, szFolderM, s_iModelsSpectatorCount);
      log_line("Remove model file: (%s)", szFile);
      unlink(szFile);
      szFile[strlen(szFile)-3] = 0;
      strcat(szFile, "bak");
      log_line("Remove model file: (%s)", szFile);
      unlink(szFile);
      
      log_line("Saving %d spectator models.", s_iModelsSpectatorCount);
      for( int i=pos; i<s_iModelsSpectatorCount; i++ )
      {
         strcpy(szFolderM, FOLDER_CONFIG_MODELS);
         strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
         sprintf(szFile, szFolderM, i);
         s_pModelsSpectator[i]->saveToFile(szFile, hardware_is_station());
      }
      bDeletedSpectator = true;
      break;
   }

   if ( (!bDeletedSpectator) && (!bDeletedController) )
      log_softerror_and_alarm("Tried to delete a model that is not in the list.");
   return s_pCurrentModel;
}

void saveControllerModel(Model* pModel)
{
   if ( NULL == pModel )
      return;

   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->uVehicleId == pModel->uVehicleId )
      if ( s_pModels[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching vehicle (ptr:%X) in controller's list (pos %d of %d) while saving the model VID %u [%s], ptr: %X (mode: %s). Update it in controller's models list.", s_pModels[i], i+1, s_iModelsCount, pModel->uVehicleId, pModel->getLongName(), pModel, pModel->is_spectator?"spectator mode":"control mode");
         char szFile[MAX_FILE_PATH_SIZE];
         char szFolderM[MAX_FILE_PATH_SIZE];
         strcpy(szFolderM, FOLDER_CONFIG_MODELS);
         strcat(szFolderM, FILE_VEHICLE_CONTROLL);
         sprintf(szFile, szFolderM, i);
         pModel->saveToFile(szFile, true);
         s_pModels[i]->loadFromFile(szFile, true);
         break;
      }
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->uVehicleId == pModel->uVehicleId )
      if ( s_pModelsSpectator[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching vehicle (ptr: %X) in spectator's list (pos %d of %d) while saving the model VID %u [%s], ptr: %X (mode: %s). Update it in controller's models list.", s_pModelsSpectator[i], i+1, s_iModelsSpectatorCount, pModel->uVehicleId, pModel->getLongName(), pModel, pModel->is_spectator?"spectator mode":"control mode");
         char szFile[MAX_FILE_PATH_SIZE];
         char szFolderM[MAX_FILE_PATH_SIZE];
         strcpy(szFolderM, FOLDER_CONFIG_MODELS);
         strcat(szFolderM, FILE_VEHICLE_SPECTATOR);
         sprintf(szFile, szFolderM, i);
         pModel->saveToFile(szFile, true);
         s_pModelsSpectator[i]->loadFromFile(szFile, true);
         break;
      }
   }

   if ( NULL != s_pCurrentModel )
   if ( pModel->uVehicleId == s_pCurrentModel->uVehicleId )
   {
      log_line("Saving model VID %u, [%s] ptr: %X, as current model (current model ptr: %X)", pModel->uVehicleId, pModel->getLongName(), pModel, s_pCurrentModel);
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      pModel->saveToFile(szFile, true);
      s_pCurrentModel->loadFromFile(szFile, true);
   }

   log_line("Saved/Updated controller models (%d):", s_iModelsCount);
   for( int i=0; i<s_iModelsCount; i++ )
      log_line("Controller model %d: [%s], VID: %u, freq1: %d Mhz, ptr: %X", i+1, s_pModels[i]->getLongName(), s_pModels[i]->uVehicleId, s_pModels[i]->radioLinksParams.link_frequency_khz[0]/1000, s_pModels[i]);

}

Model* setControllerCurrentModel(u32 uVehicleId)
{
   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->uVehicleId == uVehicleId )
      {
         s_pCurrentModel = s_pModels[i];
         log_line("Set VID %u, index %d as current controller model", uVehicleId, i);
         return s_pCurrentModel;
      }
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->uVehicleId == uVehicleId )
      {
         s_pCurrentModel = s_pModelsSpectator[i];
         log_line("Set VID %u, index %d as current spectator model", uVehicleId, i);
         return s_pCurrentModel;
      }
   }
   return NULL;
}

void logControllerModels()
{
   log_line("Controller models: %d controller mode models, %d spectator mode models, has current model: %s",
      s_iModelsCount, s_iModelsSpectatorCount, s_pCurrentModel != NULL? "yes": "no");

   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( NULL == s_pModels[i] )
         log_error_and_alarm("Controller model %d: is NULL.", i+1);
      else
         log_line("Controller model %d: VID %u, ptr: %X, mode: %s, must sync: %s",
            i+1, s_pModels[i]->uVehicleId, s_pModels[i], s_pModels[i]->is_spectator?"spectator mode":"control mode", s_pModels[i]->b_mustSyncFromVehicle?"yes":"no");
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( NULL == s_pModels[i] )
         log_error_and_alarm("Spectator model %d: is NULL.", i+1);
      else
         log_line("Spectator model %d: VID %u, ptr: %X, mode: %s, must sync: %s",
         i+1, s_pModelsSpectator[i]->uVehicleId, s_pModelsSpectator[i], s_pModelsSpectator[i]->is_spectator?"spectator mode":"control mode", s_pModelsSpectator[i]->b_mustSyncFromVehicle?"yes":"no");
   }
   
   if ( NULL != s_pCurrentModel )
      log_line("Current model: VID %u, ptr: %X, mode: %s, must sync: %s",
         s_pCurrentModel->uVehicleId, s_pCurrentModel, s_pCurrentModel->is_spectator?"spectator mode":"control mode", s_pCurrentModel->b_mustSyncFromVehicle?"yes":"no");
}