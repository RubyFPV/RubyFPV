#pragma once

#include "hardware_radio.h"
#include "models.h"

// Max deviation to show a value in ui as a standard tx value, in percentages
#define MAX_TX_POWER_UI_DEVIATION_FROM_STANDARD 12

const int* tx_powers_get_raw_radio_power_values(int* piOutputCount);
const int* tx_powers_get_ui_levels_mw(int* piOutputCount);
const int* tx_powers_get_raw_measurement_intervals(int* piOutputCount);

int tx_powers_get_mw_boosted_value_from_mw(int imWValue, bool bBoost2W, bool bBoost4W);

int tx_powers_get_max_usable_power_mw_for_card(u32 uBoardType, int iCardModel);
int tx_powers_get_max_usable_power_raw_for_card(u32 uBoardType, int iCardModel);
int tx_powers_convert_raw_to_mw(u32 uBoardType, int iCardModel, int iRawPower);
int tx_powers_convert_mw_to_raw(u32 uBoardType, int iCardModel, int imWPower);

void tx_power_get_current_mw_powers_for_model(Model* pModel, int* piOutputArray);
int tx_power_compute_uplink_power_for_model_link(Model* pModel, int iVehicleRadioLink, int iLocalRadioInterfaceIndex, int iCardModel);

int get_vehicle_radio_link_current_tx_power_mw(Model* pModel, int iRadioLinkIndex);
