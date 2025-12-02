#pragma once
#include "../base/base.h"
#include "../base/models.h"

void controller_launch_router(bool bSearchMode, int iFirmwareType);
void controller_stop_router();

void controller_launch_rx_telemetry();
void controller_stop_rx_telemetry();

void controller_launch_tx_rc();
void controller_stop_tx_rc();

void controller_start_i2c();
void controller_stop_i2c();

const char* controller_validate_radio_settings(Model* pModel, u32* pVehicleNICFreq, int* pVehicleNICFlags, int* pVehicleNICRadioFlags, int* pControllerNICFlags, int* pControllerNICRadioFlags);

void controller_wait_for_stop_all();
