#pragma once

void link_watch_init();
void link_watch_uninit();
void link_watch_reset();

void link_watch_remove_popups();

void link_watch_loop();

bool link_has_received_videostream(u32 uVehicleId);
bool link_has_received_vehicle_telemetry_info(u32 uVehicleId);
bool link_has_received_main_vehicle_ruby_telemetry();
bool link_has_received_relayed_vehicle_telemetry_info();
bool link_has_received_any_telemetry_info();
bool link_has_paired_with_main_vehicle();

bool link_is_relayed_vehicle_online();
bool link_is_vehicle_online_now(u32 uVehicleId);

void link_set_is_reconfiguring_radiolink(int iRadioLink);
void link_set_is_reconfiguring_radiolink(int iRadioLink, bool bConfirmFlagsChanges, bool bWaitVehicleConfirmation, bool bWaitControllerConfirmation);
void link_reset_reconfiguring_radiolink();
bool link_is_reconfiguring_radiolink();
u32 link_get_last_reconfiguration_end_time();


