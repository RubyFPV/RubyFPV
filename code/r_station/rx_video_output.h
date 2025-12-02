#pragma once

#include "../base/base.h"

void rx_video_output_init();
void rx_video_output_uninit();

void rx_video_output_enable_streamer_output();
void rx_video_output_disable_streamer_output();
bool rx_video_out_is_stream_output_disabled();
void rx_video_output_start_video_streamer();
void rx_video_output_stop_video_streamer();
void rx_video_output_enable_local_player_udp_output();
void rx_video_output_disable_local_player_udp_output();

void rx_video_output_discard_cached_data();
void rx_video_output_video_data(u32 uVehicleId, t_packet_header_video_segment* pPHVS, int width, int height, u8* pBuffer, int video_data_length, int packet_length, bool bWaitFullFrame);
void rx_video_output_on_controller_settings_changed();
void rx_video_output_on_changed_video_params(video_parameters_t* pOldVideoParams, type_video_link_profile* pOldVideoProfiles, video_parameters_t* pNewVideoParams, type_video_link_profile* pNewVideoProfiles);

void rx_video_output_signal_restart_streamer();
void rx_video_output_periodic_loop();

