#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Power is 0..71
void hardware_radio_set_txpower_raw_rtl8812au(int iCardIndex, int iTxPower);
void hardware_radio_set_txpower_raw_rtl8812eu(int iCardIndex, int iTxPower);
void hardware_radio_set_txpower_raw_rtl8733bu(int iCardIndex, int iTxPower);
void hardware_radio_set_txpower_raw_atheros(int iCardIndex, int iTxPower);

#ifdef __cplusplus
}
#endif
