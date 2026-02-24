#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"


#ifdef __cplusplus
extern "C" {
#endif

void radio_duplicate_detection_init();
void radio_duplicate_detection_log_info();

int radio_dup_detection_is_duplicate_on_stream(int iRadioInterfaceIndex, u8* pPacketBuffer, int iPacketLength, u32 uTimeNow);
void radio_duplicate_detection_remove_data_for_all_except(u32 uVehicleId);
void radio_duplicate_detection_remove_data_for_vid(u32 uVehicleId);
int radio_dup_detection_is_vehicle_restarted(u32 uVehicleId);
void radio_dup_detection_set_vehicle_restarted_flag(u32 uVehicleId);
void radio_dup_detection_reset_vehicle_restarted_flag(u32 uVehicleId);

u32 radio_dup_detection_get_max_received_packet_index_for_stream(u32 uVehicleId, u32 uStreamId);

#ifdef __cplusplus
}
#endif
