#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem.h"

void vehicle_launch_tx_telemetry(Model* pModel);
void vehicle_stop_tx_telemetry();

void vehicle_launch_rx_rc(Model* pModel);
void vehicle_stop_rx_rc();

void vehicle_launch_rx_commands(Model* pModel);
void vehicle_stop_rx_commands();

void vehicle_launch_tx_router(Model* pModel);
void vehicle_stop_tx_router();

bool vehicle_is_audio_capture_started();
void vehicle_launch_audio_capture(Model* pModel);
void vehicle_stop_audio_capture(Model* pModel);
