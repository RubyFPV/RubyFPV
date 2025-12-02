#pragma once
#include "../base/base.h"
#include "popup.h"

void warnings_on_changed_vehicle();
void warnings_remove_all();
void warnings_periodic_loop();

void warnings_replace(const char* szSource, const char* szDest);

void warnings_add(u32 uVehicleId, const char* szTitle);
void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId);
void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId, const double* pColorIcon, int timeout=8);

void warnings_add_error_null_model(int code);
void warnings_add_radio_reinitialized();
void warnings_add_too_much_data(u32 uVehicleId, int current_kbps, int max_kbps);
void warnings_add_vehicle_overloaded(u32 uVehicleId, bool bEC, bool bTx, u32 uECMsPerSec, u32 uTxMsPerSec);

void warnings_add_link_to_controller_lost(u32 uVehicleId);
void warnings_add_link_to_controller_recovered(u32 uVehicleId);

void warnings_add_configuring_radio_link(int iRadioLink, const char* szMessage);
void warnings_add_configuring_radio_link_line(const char* szLine);
void warnings_remove_configuring_radio_link(bool bSucceeded);
Popup* warnings_get_configure_radio_link_popup();
char* warnings_get_last_message_configure_radio_link();

void warnings_add_unsupported_video(u32 uVehicleId, u32 uType);


void warnings_add_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz);
void warnings_remove_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz, bool bSucceeded);

void warnings_add_input_device_added(char* szDeviceName);
void warnings_add_input_device_removed(char* szDeviceName);
void warnings_add_input_device_unknown_key(int iKeyCode);

