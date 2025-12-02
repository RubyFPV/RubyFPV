#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiopackets2.h"

void parse_telemetry_init(u32 vehicleMavId, bool bShowLocalVerticalSpeed);
void parse_telemetry_allow_any_sysid(int iAllow);
void parse_telemetry_set_show_local_vspeed(bool bShowLocalVerticalSpeed);
void parse_telemetry_remove_duplicate_messages(bool bRemove);
void parse_telemetry_force_always_armed(bool bForce);

bool parse_telemetry_from_fc( u8* buffer, int length, t_packet_header_fc_telemetry* pphfct, t_packet_header_ruby_telemetry_extended_v6* pPHRTE, u8 vehicleType, int telemetry_type );
bool has_received_gps_info();
bool has_received_flight_mode();
u32  get_last_message_time();
char* get_last_message();
u32 get_time_last_mavlink_message_from_fc();
void set_time_last_mavlink_message_from_fc(u32 uTime);

int* get_mavlink_rc_channels();

int get_heartbeat_msg_count();
int get_system_msg_count();
void reset_heartbeat_msg_count();
void reset_system_msg_count();
