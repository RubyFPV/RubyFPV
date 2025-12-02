#pragma once
#include "../base/base.h"

void video_source_csi_flush_discard();
int video_source_csi_get_buffer_size();
void video_source_csi_log_input_data();
u32 video_source_csi_get_debug_videobitrate();

// Returns the buffer and number of bytes read
u8* video_source_csi_read(int* piReadSize, u32* puOutTimeDataAvailable);
// Returns initial set video bitrate
u32 video_source_csi_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int* pInitialKFSet);
void video_source_csi_stop_program();
u32 video_source_cs_get_program_start_time();
void video_source_csi_request_restart_program();

void video_source_csi_send_control_message(u8 parameter, u16 value1, u16 value2);
bool video_source_csi_periodic_health_checks();

void video_source_csi_update_camera_params(Model* pModel, int iCameraIndex);
