#pragma once
#include "../base/base.h"
#include "../base/models.h"


void controller_launch_router();
void controller_stop_router();

void controller_launch_video_player();
void controller_stop_video_player();

void controller_launch_rx_telemetry();
void controller_stop_rx_telemetry();

void controller_launch_tx_rc();
void controller_stop_tx_rc();

void controller_start_audio(Model* pModel);
void controller_stop_audio();

void controller_start_i2c();
void controller_stop_i2c();

const char* controller_validate_radio_settings(Model* pModel, int* pVehicleNICFreq, int* pVehicleNICFlags, int* pVehicleNICRadioFlags, int* pControllerNICFlags, int* pControllerNICRadioFlags);

void controller_wait_for_stop_all();


void controller_check_update_processes_affinities();
