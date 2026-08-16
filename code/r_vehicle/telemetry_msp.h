#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiopackets2.h"

void telemetry_msp_on_open_port(int iSerialPortFile);
void telemetry_msp_on_close();
void telemetry_msp_periodic_loop();
void telemetry_msp_on_second_lapse();
u32  telemetry_msp_get_last_command_received_time();
void telemetry_msp_set_last_command_received_time(u32 uTime);

// Returns true if a new message was found
bool telemetry_msp_on_new_serial_data(u8* pData, int iDataLength);
void telemetry_msp_send_to_controller();

// FC type flag (one of MSP_FLAGS_FC_TYPE_* values masked by MSP_FLAGS_FC_TYPE_MASK).
// Returns 0 until the vehicle receives enough MSP to identify the FC variant.
u32 telemetry_msp_get_fc_type_flag();

// MSP telemetry header sync procedure (the whole header like GS does)
const t_packet_header_telemetry_msp* telemetry_msp_get_header();
