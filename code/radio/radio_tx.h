#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"


#ifdef __cplusplus
extern "C" {
#endif

int radio_tx_start_tx_thread();
void radio_tx_stop_tx_thread();
void radio_tx_mark_quit();
void radio_tx_set_custom_thread_raw_priority(int iPriority);
void radio_tx_set_dev_mode(int iDevMode);
void radio_tx_pause_radio_interface(int iRadioInterfaceIndex, const char* szReason);
void radio_tx_resume_radio_interface(int iRadioInterfaceIndex);
void radio_tx_set_sik_packet_size(int iSiKPacketSize);
void radio_tx_set_serial_packet_size(int iRadioInterfaceIndex, int iSerialPacketSize);

// Sends a regular radio packet to serial radios.
// Returns 1 for success.
int radio_tx_send_serial_radio_packet(int iRadioInterfaceIndex, u8* pData, int iDataLength);

#ifdef __cplusplus
}  
#endif
