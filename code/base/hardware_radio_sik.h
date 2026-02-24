#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_radio.h"
#include "../base/shared_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

int hardware_radio_has_sik_radios();
int hardware_radio_sik_firmware_is_old();

int hardware_radio_sik_reinitialize_serial_ports();

int hardware_radio_sik_detect_interfaces();
radio_hw_info_t* hardware_radio_sik_get_from_serial_port(const char* szSerialPort);
radio_hw_info_t* hardware_radio_sik_try_detect_on_port(const char* szSerialPort);

int hardware_radio_sik_load_configuration();
int hardware_radio_sik_save_configuration();
int hardware_radio_sik_get_air_baudrate_in_bytes(int iRadioInterfaceIndex);

int hardware_radio_sik_send_command(int iSerialPortFile, const char* szCommand, u8* bufferResponse, int iMaxResponseLength);
int hardware_radio_sik_get_real_serial_baudrate(int iSiKBaudRate);
int hardware_radio_sik_get_encoded_serial_baudrate(int iRealSerialBaudRate);
int hardware_radio_sik_get_real_air_baudrate(int iSiKAirBaudRate);
int hardware_radio_sik_get_encoded_air_baudrate(int iRealAirBaudRate);
int hardware_radio_sik_get_all_params_async(int iRadioInterfaceIndex, int* piFinished);
int hardware_radio_sik_get_all_params(radio_hw_info_t* pRadioInfo, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_set_serial_speed(radio_hw_info_t* pRadioInfo, int iSerialSpeedToConnect, int iNewSerialSpeed, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_set_frequency(radio_hw_info_t* pRadioInfo, u32 uFrequencyKhz, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_set_tx_power(radio_hw_info_t* pRadioInfo, u32 uTxPower, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(radio_hw_info_t* pRadioInfo, u32 uFrequency, u32 uTxPower, u32 uAirSpeed, u32 uECC, u32 uLBT, u32 uMCSTR, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_set_params(radio_hw_info_t* pRadioInfo, u32 uFrequencyKhz, u32 uFreqSpread, u32 uChannels, u32 uNetId, u32 uAirSpeed, u32 uTxPower, u32 uECC, u32 uLBT, u32 uMCSTR, shared_mem_process_stats* pProcessStats);

int hardware_radio_sik_open_for_read_write(int iHWRadioInterfaceIndex);
int hardware_radio_sik_close(int iHWRadioInterfaceIndex);

int hardware_radio_sik_enter_command_mode(int iSerialPortFile, int iBaudRate, shared_mem_process_stats* pProcessStats);
int hardware_radio_sik_save_settings_to_flash(int iSerialPortFile);

int hardware_radio_sik_write_packet(int iHWRadioInterfaceIndex, u8* pData, int iLength);

#ifdef __cplusplus
}
#endif