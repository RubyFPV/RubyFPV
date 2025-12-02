#pragma once

void close_and_mark_sik_interfaces_to_reopen();
void reopen_marked_sik_interfaces();
void flag_update_sik_interface(int iInterfaceIndex);
void flag_reinit_sik_interface(int iInterfaceIndex);
void reinit_radio_interfaces();
void send_radio_config_to_controller();
void send_radio_reinitialized_message();
void checkDeveloperFlagsChanges(u32 uOldDeveloperFlags, u32 uNewDeveloperFlags);
int process_and_send_packets(bool bIsEndOfTransmissionFrame);

void signal_start_long_op();
void signal_end_long_op();
