#pragma once
#include "../base/base.h"

void video_playback_play_file(const char* szVideoInfoFile);
void video_playback_stop();
void video_playback_periodic_loop();
void video_playback_render();
