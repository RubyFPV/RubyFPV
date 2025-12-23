#pragma once
#include "base.h"
#include "config.h"
#include "models.h"


bool loadAllModels();
bool saveCurrentModel();
Model* getCurrentModel();
bool reloadCurrentModel();
void setCurrentModel(u32 uVehicleId);

int getControllerModelsCount();
int getControllerModelsSpectatorCount();

void deleteAllModels();

Model* getSpectatorModel(int iIndex);
Model* addSpectatorModel(u32 vehicleId);
void moveSpectatorModelToTop(int index);

Model* getModelAtIndex(int index);
Model* addNewModel(u32 uVehicleId, int iVersionMajor, int iVersionMinor);
void replaceModel(int index, Model* pModel);
Model* findModelWithId(u32 uVehicleId, u32 uSrcId);
bool modelIsInControllerList(u32 uVehicleId);
bool modelIsInSpectatorList(u32 uVehicleId);
bool controllerHasModelWithId(u32 uVehicleId);
Model* deleteModel(Model* pModel);
void saveControllerModel(Model* pModel);
Model* setControllerCurrentModel(u32 uVehicleId);

void logControllerModels();

