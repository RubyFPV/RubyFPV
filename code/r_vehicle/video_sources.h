#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"

void video_sources_init();
void video_sources_uninit();

void video_sources_start_capture();
void video_sources_stop_capture();
bool video_sources_is_caputure_process_running();

u32  video_sources_get_capture_start_time();
void video_sources_flush_discard_all_pending_data();
bool video_sources_try_read_camera_frame(bool* pbOutEndOfFrameDetected);
bool video_sources_has_stream_data();
u32  video_sources_last_stream_data();

void video_sources_apply_all_parameters();
void video_sources_on_changed_camera_params(type_camera_parameters* pNewCamParams, type_camera_parameters* pOldCamParams);
void video_sources_on_changed_developer_flags(u32 uOldDeveloperFlags, u32 uNewDeveloperFlags);
void video_sources_on_changed_video_params(camera_profile_parameters_t* pOldCameraProfileParams, video_parameters_t* pOldVideoParams, type_video_link_profile* pOldVideoProfiles);
void video_sources_set_video_bitrate(u32 uVideoBitrateBPS, int iIPQDelta, const char* szReason);
u32 video_sources_get_last_set_video_bitrate();
int video_source_get_last_set_ipqdelta();
void video_sources_set_keyframe(int iKeyframeMs);
int video_sources_get_last_set_keyframe();

void video_sources_set_temporary_bw_mode(bool bTurnOn);

// Returns true if full restart is needed
bool video_sources_periodic_health_checks();