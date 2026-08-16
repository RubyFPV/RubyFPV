#pragma once

#include "../base/base.h"

void rx_video_recording_init();
void rx_video_recording_uninit();

void rx_video_recording_start();
bool rx_video_recording_stop();
bool rx_video_is_recording();
u32  rx_video_recording_get_last_start_stop_time();

void rx_video_recording_on_new_data(u8* pData, int iLength, int iFrameIndex);

void rx_video_recording_periodic_loop();

