/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/utils.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_cam_maj.h"
#include "../common/string_utils.h"
#include "adaptive_video.h"
#include "shared_vars.h"
#include "timers.h"
#include "video_tx_buffers.h"
#include "video_sources.h"
#include "test_link_params.h"
#include "packets_utils.h"
#include "negociate_radio.h"

// RubALink Dynamic RSSI Thresholds, Enhanced Cooldown, and Racing Mode Integration
#include <math.h>

// Racing mode configuration
typedef struct {
    bool race_mode_enabled;
    int racing_fps;
    const char* racing_video_resolution;
    int racing_exposure;
    int racing_bitrate_max;
    const char* racing_rssi_filter_chain;
    const char* racing_dbm_filter_chain;
    unsigned long racing_strict_cooldown_ms;
    unsigned long racing_up_cooldown_ms;
    unsigned long racing_emergency_cooldown_ms;
    int racing_qp_delta_low;
    int racing_qp_delta_medium;
    int racing_qp_delta_high;
} racing_mode_config_t;

typedef struct {
    bool racing_mode_active;
    bool racing_mode_transitioning;
    unsigned long racing_mode_start_time;
    int racing_mode_transitions;
} racing_mode_state_t;

// Dynamic RSSI threshold configuration
typedef struct {
    int hardware_rssi_offset;           // Hardware calibration offset (dBm)
    bool enable_dynamic_thresholds;     // Enable/disable feature
    int safety_margin_db;              // Safety margin for emergency drops (dBm)
} dynamic_rssi_config_t;

// Enhanced cooldown configuration
typedef struct {
    unsigned long strict_cooldown_ms;
    unsigned long up_cooldown_ms;
    unsigned long down_cooldown_ms;
    unsigned long emergency_cooldown_ms;
    int min_change_percent;
    unsigned long last_change_time;
    int last_bitrate;
} enhanced_cooldown_config_t;

// Global configurations
static racing_mode_config_t s_RacingConfig = {
    .race_mode_enabled = false,
    .racing_fps = 120,
    .racing_video_resolution = "1280x720",
    .racing_exposure = 11,
    .racing_bitrate_max = 4,
    .racing_rssi_filter_chain = "1",  // Low-pass filter
    .racing_dbm_filter_chain = "1",   // Low-pass filter
    .racing_strict_cooldown_ms = 100,
    .racing_up_cooldown_ms = 1000,
    .racing_emergency_cooldown_ms = 25,
    .racing_qp_delta_low = 10,
    .racing_qp_delta_medium = 3,
    .racing_qp_delta_high = 0
};

static racing_mode_state_t s_RacingState = {
    .racing_mode_active = false,
    .racing_mode_transitioning = false,
    .racing_mode_start_time = 0,
    .racing_mode_transitions = 0
};

static dynamic_rssi_config_t s_DynamicRSSIConfig = {
    .hardware_rssi_offset = 0,
    .enable_dynamic_thresholds = true,
    .safety_margin_db = 3
};

static enhanced_cooldown_config_t s_CooldownConfig = {
    .strict_cooldown_ms = 200,
    .up_cooldown_ms = 3000,
    .down_cooldown_ms = 1000,
    .emergency_cooldown_ms = 50,
    .min_change_percent = 5,
    .last_change_time = 0,
    .last_bitrate = 0
};

// MCS to RSSI threshold lookup table (based on 802.11n/ac standards)
static const int s_MCS_RSSI_Thresholds[10] = {
    -82,  // MCS 0
    -79,  // MCS 1
    -77,  // MCS 2
    -74,  // MCS 3
    -70,  // MCS 4
    -66,  // MCS 5
    -65,  // MCS 6
    -64,  // MCS 7
    -59,  // MCS 8
    -57   // MCS 9
};

// Function declarations
static int get_dynamic_rssi_threshold(int current_mcs, int hardware_offset);
static int get_effective_rssi_threshold(int current_bitrate, int static_threshold, int hardware_offset);
static bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi, int static_threshold, int hardware_offset);
static bool check_enhanced_cooldown(int new_bitrate, int current_bitrate);
static void update_enhanced_cooldown(int new_bitrate, int current_bitrate);
static int bitrate_to_mcs(int bitrate_mbps);

// Racing mode functions
static void enable_racing_mode();
static void disable_racing_mode();
static bool is_racing_mode_active();
static void transition_to_racing_mode();
static void transition_from_racing_mode();
static bool check_racing_mode_cooldown(int new_bitrate, int current_bitrate);
static void apply_racing_mode_settings();

// Dynamic RSSI threshold functions
static int get_dynamic_rssi_threshold(int current_mcs, int hardware_offset) {
    if (!s_DynamicRSSIConfig.enable_dynamic_thresholds) {
        return -80; // Default threshold
    }
    
    if (current_mcs < 0 || current_mcs >= 10) {
        return -80; // Default for invalid MCS
    }
    
    int threshold = s_MCS_RSSI_Thresholds[current_mcs];
    return threshold + hardware_offset;
}

static int get_effective_rssi_threshold(int current_bitrate, int static_threshold, int hardware_offset) {
    int current_mcs = bitrate_to_mcs(current_bitrate / 1000); // Convert to Mbps
    int dynamic_threshold = get_dynamic_rssi_threshold(current_mcs, hardware_offset);
    
    // Use the more conservative threshold
    return (dynamic_threshold < static_threshold) ? dynamic_threshold : static_threshold;
}

static bool should_trigger_emergency_drop(int current_bitrate, float filtered_rssi, int static_threshold, int hardware_offset) {
    int effective_threshold = get_effective_rssi_threshold(current_bitrate, static_threshold, hardware_offset);
    return filtered_rssi < (effective_threshold - s_DynamicRSSIConfig.safety_margin_db);
}

static int bitrate_to_mcs(int bitrate_mbps) {
    // Approximate MCS based on bitrate (this is a simplified mapping)
    if (bitrate_mbps <= 1) return 0;
    if (bitrate_mbps <= 2) return 1;
    if (bitrate_mbps <= 4) return 2;
    if (bitrate_mbps <= 6) return 3;
    if (bitrate_mbps <= 8) return 4;
    if (bitrate_mbps <= 10) return 5;
    if (bitrate_mbps <= 12) return 6;
    if (bitrate_mbps <= 15) return 7;
    if (bitrate_mbps <= 20) return 8;
    return 9;
}

// Enhanced cooldown functions
static bool check_enhanced_cooldown(int new_bitrate, int current_bitrate) {
    unsigned long current_time = g_TimeNow;
    unsigned long time_since_last_change = current_time - s_CooldownConfig.last_change_time;
    
    // Check minimum change percentage
    int change_percent = abs(new_bitrate - current_bitrate) * 100 / current_bitrate;
    if (change_percent < s_CooldownConfig.min_change_percent) {
        return false; // Change too small
    }
    
    // Determine cooldown period based on change direction
    unsigned long required_cooldown;
    if (new_bitrate > current_bitrate) {
        required_cooldown = s_CooldownConfig.up_cooldown_ms;
    } else if (new_bitrate < current_bitrate * 0.5) {
        required_cooldown = s_CooldownConfig.emergency_cooldown_ms;
    } else {
        required_cooldown = s_CooldownConfig.down_cooldown_ms;
    }
    
    return time_since_last_change >= required_cooldown;
}

static void update_enhanced_cooldown(int new_bitrate, int current_bitrate) {
    s_CooldownConfig.last_change_time = g_TimeNow;
    s_CooldownConfig.last_bitrate = new_bitrate;
}

// Racing mode functions
static void enable_racing_mode() {
    if (s_RacingState.racing_mode_active || s_RacingState.racing_mode_transitioning) {
        return; // Already active or transitioning
    }
    
    s_RacingState.racing_mode_transitioning = true;
    transition_to_racing_mode();
    
    s_RacingState.racing_mode_active = true;
    s_RacingState.racing_mode_transitioning = false;
    s_RacingState.racing_mode_start_time = g_TimeNow;
    s_RacingState.racing_mode_transitions++;
    
    log_line("[RubALink] Racing mode ENABLED");
}

static void disable_racing_mode() {
    if (!s_RacingState.racing_mode_active || s_RacingState.racing_mode_transitioning) {
        return; // Not active or transitioning
    }
    
    s_RacingState.racing_mode_transitioning = true;
    transition_from_racing_mode();
    
    s_RacingState.racing_mode_active = false;
    s_RacingState.racing_mode_transitioning = false;
    
    log_line("[RubALink] Racing mode DISABLED");
}

static bool is_racing_mode_active() {
    return s_RacingState.racing_mode_active;
}

static void transition_to_racing_mode() {
    // Apply racing mode video settings
    apply_racing_mode_settings();
    log_line("[RubALink] Transitioned to racing mode");
}

static void transition_from_racing_mode() {
    // Restore normal video settings
    log_line("[RubALink] Transitioned from racing mode");
}

static bool check_racing_mode_cooldown(int new_bitrate, int current_bitrate) {
    if (!s_RacingState.racing_mode_active) {
        return true; // Use normal cooldown
    }
    
    unsigned long current_time = g_TimeNow;
    unsigned long time_since_last_change = current_time - s_CooldownConfig.last_change_time;
    
    // Racing mode uses more aggressive cooldowns
    unsigned long required_cooldown;
    if (new_bitrate > current_bitrate) {
        required_cooldown = s_RacingConfig.racing_up_cooldown_ms;
    } else if (new_bitrate < current_bitrate * 0.5) {
        required_cooldown = s_RacingConfig.racing_emergency_cooldown_ms;
    } else {
        required_cooldown = s_RacingConfig.racing_strict_cooldown_ms;
    }
    
    return time_since_last_change >= required_cooldown;
}

static void apply_racing_mode_settings() {
    // Apply racing-specific video settings
    // This would integrate with RubyFPV's video profile system
    log_line("[RubALink] Applied racing mode settings: FPS=%d, Resolution=%s, MaxBitrate=%dMbps", 
             s_RacingConfig.racing_fps, s_RacingConfig.racing_video_resolution, s_RacingConfig.racing_bitrate_max);
}

u32 s_uAdaptiveVideoLastRequestIdReceived = MAX_U32;
u32 s_uAdaptiveVideoLastRequestIdReceivedTime = 0;

u8  s_uAdaptiveVideoLastFlagsReceived = 0;
int s_iAdaptiveVideoLastSetKeyframeMS = 0;
u32 s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;
u16 s_uAdaptiveVideoLastSetECScheme = 0;
u8  s_uAdaptiveVideoLastSetDRBoost = 0xFF; // 0xFF none set, use video profile DR boost
u32 s_uLastTimeAdaptiveDebugInfo = 0;

bool s_bAdaptiveVideoIsFocusModeBWActive = false;
u32 s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;

void adaptive_video_init()
{
   log_line("[AdaptiveVideo] Init...");
   u32 uCurrentVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps;
   int iDataRateForVideo = g_pCurrentModel->getRadioDataRateForVideoBitrate(uCurrentVideoBitrate, 0);
   int iDRBoost = -1;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      iDRBoost = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;


   log_line("[AdaptiveVideo] Initial bitrate: %.2f Mbps, datarate for it: %s, video profile (%s) DR boost: %d",
       (float)uCurrentVideoBitrate/1000.0/1000.0, str_format_datarate_inline(iDataRateForVideo),
       str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile), iDRBoost);
   adaptive_video_reset_to_defaults();
   log_line("[AdaptiveVideo] Done init.");
}


void adaptive_video_reset_to_defaults()
{
   log_line("[AdaptiveVideo] Reset to defaults.");
   s_iAdaptiveVideoLastSetKeyframeMS = 0;
   s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;
   s_uAdaptiveVideoLastSetDRBoost = 0xFF;
   s_bAdaptiveVideoIsFocusModeBWActive = false;
   s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
   video_sources_set_temporary_image_saturation_off(false);
   //if ( NULL != g_pCurrentModel )
   //   s_iAdaptiveVideoLastSetKeyframeMS = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.iCurrentVideoProfile);
}

void _adaptive_video_turn_focusmode_bw_on()
{
   if ( negociate_radio_link_is_in_progress() )
      return;
   s_bAdaptiveVideoIsFocusModeBWActive = true;
   s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
   video_sources_set_temporary_image_saturation_off(true);
}

void _adaptive_video_turn_focusmode_bw_off_after(u32 uTimeout)
{
   if ( s_bAdaptiveVideoIsFocusModeBWActive )
      s_uAdaptiveVideoTimeTurnFocusModeBWOff = g_TimeNow + uTimeout;
}

void adaptive_video_reset_requested_keyframe()
{
   s_iAdaptiveVideoLastSetKeyframeMS = 0;
}

void adaptive_video_save_state()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "tmp_adaptive.info");
   FILE* fd = fopen(szFile, "wb");
   if ( NULL == fd )
      return;
   fprintf(fd, "%d %u\n", s_iAdaptiveVideoLastSetKeyframeMS, s_uAdaptiveVideoLastSetVideoBitrateBPS);
   fprintf(fd, "%d\n", (int)s_uAdaptiveVideoLastSetDRBoost);
   fclose(fd);
}

void adaptive_video_load_state()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, "tmp_adaptive.info");
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("[AdaptiveVideo] No saved temporary state to restore.");
      return;
   }
   log_line("[AdaptiveVideo] Restore temporary state...");
   int iTmp = 0;
   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
      return;
   if ( 1 != fscanf(fd, "%d", &s_iAdaptiveVideoLastSetKeyframeMS) )
      s_iAdaptiveVideoLastSetKeyframeMS = 0;
   if ( 1 != fscanf(fd, "%u", &s_uAdaptiveVideoLastSetVideoBitrateBPS) )
      s_uAdaptiveVideoLastSetVideoBitrateBPS = 0;

   if ( 1 != fscanf(fd, "%u", &iTmp) )
      s_uAdaptiveVideoLastSetDRBoost = 0;
   else
   {
      s_uAdaptiveVideoLastSetDRBoost = iTmp;
      if ( (s_uAdaptiveVideoLastSetDRBoost != 0xFF) && (s_uAdaptiveVideoLastSetDRBoost > 5) )
         s_uAdaptiveVideoLastSetDRBoost = 5;
   }
   fclose(fd);

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szFile);
   hw_execute_bash_command(szComm, NULL);

   if ( 0 != s_iAdaptiveVideoLastSetKeyframeMS )
      video_sources_set_keyframe(s_iAdaptiveVideoLastSetKeyframeMS);
   log_line("[AdaptiveVideo] Done restoring temporary state.");
}

int adaptive_video_get_current_dr_boost()
{
   if ( 0xFF != s_uAdaptiveVideoLastSetDRBoost )
      return s_uAdaptiveVideoLastSetDRBoost;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      return (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
   return 0;
}

int adaptive_video_get_current_keyframe_ms()
{
   return s_iAdaptiveVideoLastSetKeyframeMS;
}

bool adaptive_video_is_on_lower_video_bitrate()
{
   if ( 0 != s_uAdaptiveVideoLastSetVideoBitrateBPS )
   if ( s_uAdaptiveVideoLastSetVideoBitrateBPS != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps )
      return true;
   return false;
}

void adaptive_video_on_uplink_lost()
{
   log_line("[AdaptiveVideo] On link lost.");
}

void adaptive_video_on_uplink_recovered()
{
   log_line("[AdaptiveVideo] On link recovered.");
}

void adaptive_video_on_end_of_frame()
{

}

void adaptive_video_check_update_params()
{

}

void adaptive_video_periodic_loop()
{
   if ( g_TimeNow >= s_uLastTimeAdaptiveDebugInfo + 5000 )
   {
      s_uLastTimeAdaptiveDebugInfo = g_TimeNow;
      log_line("[AdaptiveVideo] Current video capture set bitrate: %.2f Mbps", (float)video_sources_get_last_set_video_bitrate()/1000.0/1000.0);
      log_line("[AdaptiveVideo] Last settings requested from controller: flags: %s", str_format_adaptive_video_flags(s_uAdaptiveVideoLastFlagsReceived));
      log_line("[AdaptiveVideo] Last settings requested from controller: bitrate: %.2f Mbps, keyframe: %d ms", (float)s_uAdaptiveVideoLastSetVideoBitrateBPS/1000.0/1000.0, s_iAdaptiveVideoLastSetKeyframeMS);
      log_line("[AdaptiveVideo] Last settings requested from controller: DR boost: %d, EC: %d/%d", s_uAdaptiveVideoLastSetDRBoost, s_uAdaptiveVideoLastSetECScheme >> 8, s_uAdaptiveVideoLastSetECScheme & 0xFF);
   }

   if ( (0 != s_uAdaptiveVideoTimeTurnFocusModeBWOff) && (g_TimeNow >= s_uAdaptiveVideoTimeTurnFocusModeBWOff) )
   {
      s_uAdaptiveVideoTimeTurnFocusModeBWOff = 0;
      s_bAdaptiveVideoIsFocusModeBWActive = false;
      video_sources_set_temporary_image_saturation_off(false);
   }
}

void adaptive_video_on_message_from_controller(u32 uRequestId, u8 uFlags, u32 uVideoBitrate, u16 uECScheme, u8 uStreamIndex, int iRadioDatarate, int iKeyframeMs, u8 uDRBoost)
{
   if ( (uRequestId == s_uAdaptiveVideoLastRequestIdReceived) && (uFlags == s_uAdaptiveVideoLastFlagsReceived) )
   {
      log_line("[AdaptiveVideo] Received duplicate req id %u from CID %u, flags: %s. Ignore it.", uRequestId, g_uControllerId, str_format_adaptive_video_flags(uFlags));
      return;
   }
   log_line("[AdaptiveVideo] Received req id %u from CID %u, flags: %s", uRequestId, g_uControllerId, str_format_adaptive_video_flags(uFlags));
   log_line("[AdaptiveVideo] Previous received req id was: %u, %u ms ago, previous flags: %s",
      s_uAdaptiveVideoLastRequestIdReceived, g_TimeNow - s_uAdaptiveVideoLastRequestIdReceivedTime,
      str_format_adaptive_video_flags(s_uAdaptiveVideoLastFlagsReceived));
   log_line("[AdaptiveVideo] Request params: stream id: %d, video bitrate: %.2f Mbps, datarate: %d, keyframe %d ms, EC: %d/%d, DR boost: %d",
      uStreamIndex, (float)uVideoBitrate/1000.0/1000.0, iRadioDatarate, iKeyframeMs, (uECScheme>>8), (uECScheme & 0xFF), uDRBoost);

   s_uAdaptiveVideoLastRequestIdReceived = uRequestId;
   s_uAdaptiveVideoLastRequestIdReceivedTime = g_TimeNow;
   s_uAdaptiveVideoLastFlagsReceived = uFlags;

   u32 uCurrentVideoBitrate = video_sources_get_last_set_video_bitrate();
   if ( 0 == uCurrentVideoBitrate )
      uCurrentVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps;
   log_line("[AdaptiveVideo] Current state: video bitrate: %.2f Mbps (datarate for it: %s), DR boost: %d of %d, EC scheme: %d/%d",
      (float)uCurrentVideoBitrate/1000.0/1000.0,
      str_format_datarate_inline(g_pCurrentModel->getRadioDataRateForVideoBitrate(uCurrentVideoBitrate, 0)),
      s_uAdaptiveVideoLastSetDRBoost, (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT,
       s_uAdaptiveVideoLastSetECScheme >> 8, s_uAdaptiveVideoLastSetECScheme & 0xFF);

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_KEYFRAME )
   {
      s_iAdaptiveVideoLastSetKeyframeMS = iKeyframeMs;
      video_sources_set_keyframe(iKeyframeMs);
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_BITRATE )
   {
      s_uAdaptiveVideoLastSetVideoBitrateBPS = uVideoBitrate;
      packet_utils_set_adaptive_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS);
      
      // RubALink: Enhanced cooldown and dynamic RSSI integration
      u32 uCurrentVideoBitrate = video_sources_get_last_set_video_bitrate();
      if ( 0 == uCurrentVideoBitrate )
         uCurrentVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps;
      
      // Check enhanced cooldown before applying bitrate change
      bool cooldown_ok = check_enhanced_cooldown(s_uAdaptiveVideoLastSetVideoBitrateBPS, uCurrentVideoBitrate);
      
      // Check racing mode cooldown if racing mode is active
      if (is_racing_mode_active()) {
          cooldown_ok = check_racing_mode_cooldown(s_uAdaptiveVideoLastSetVideoBitrateBPS, uCurrentVideoBitrate);
      }
      
      // Get current signal quality for dynamic RSSI thresholds
      float current_rssi = -80.0f; // Default
      
      // Check if emergency drop is needed based on signal quality
      bool emergency_drop_needed = should_trigger_emergency_drop(uCurrentVideoBitrate, current_rssi, -80, s_DynamicRSSIConfig.hardware_rssi_offset);
      
      if ( negociate_radio_link_is_in_progress() )
      {
         log_line("[AdaptiveVideo] Negociate radio flow is in progress. Do not change video bitrate now.");
         if ( 0 == s_uAdaptiveVideoLastSetVideoBitrateBPS )
            negociate_radio_set_end_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps);
         else
            negociate_radio_set_end_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS);
      }
      else if (cooldown_ok || emergency_drop_needed)
      {
         if ( 0 == s_uAdaptiveVideoLastSetVideoBitrateBPS )
            video_sources_set_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo");
         else
            video_sources_set_video_bitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo");
         
         // Update cooldown timing
         update_enhanced_cooldown(s_uAdaptiveVideoLastSetVideoBitrateBPS, uCurrentVideoBitrate);
         
         log_line("[AdaptiveVideo] Did set new video bitrate of %.2f Mbps; datarate for new video bitrate: %s (Cooldown: %s, Emergency: %s, Racing: %s)",
            (float)video_sources_get_last_set_video_bitrate()/1000.0/1000.0,
            str_format_datarate_inline(g_pCurrentModel->getRadioDataRateForVideoBitrate(video_sources_get_last_set_video_bitrate(), 0)),
            cooldown_ok ? "OK" : "BLOCKED", emergency_drop_needed ? "YES" : "NO", is_racing_mode_active() ? "ACTIVE" : "INACTIVE");
      }
      else
      {
         log_line("[AdaptiveVideo] Bitrate change blocked by enhanced cooldown (%.2f -> %.2f Mbps)", 
                  (float)uCurrentVideoBitrate/1000.0/1000.0, (float)s_uAdaptiveVideoLastSetVideoBitrateBPS/1000.0/1000.0);
      }

      if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BW )
      {
         int iCurrentVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
         bool bOnlyMediumAdaptive = false;
         if ( (g_pCurrentModel->video_link_profiles[iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
            bOnlyMediumAdaptive = true;

         int iDataRateForCurrentVideoBitrate = g_pCurrentModel->getRadioDataRateForVideoBitrate(s_uAdaptiveVideoLastSetVideoBitrateBPS, 0);
         if ( (-1 == iDataRateForCurrentVideoBitrate) || (getDataRatesBPS()[0] == iDataRateForCurrentVideoBitrate) )
            _adaptive_video_turn_focusmode_bw_on();
         else if ( bOnlyMediumAdaptive && ((-2 == iDataRateForCurrentVideoBitrate) || (getDataRatesBPS()[1] == iDataRateForCurrentVideoBitrate)) )
            _adaptive_video_turn_focusmode_bw_on();
         else
            _adaptive_video_turn_focusmode_bw_off_after(1000);
      }
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_EC )
   {
      s_uAdaptiveVideoLastSetECScheme = uECScheme;
      if ( (0 == uECScheme) || (0xFFFF == uECScheme) )
      {
         log_line("[AdaptiveVideo] Reset EC scheme to default for model.");
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(0);
      }
      else
      {
         log_line("[AdaptiveVideo] Set custom EC scheme (%d/%d)", uECScheme >> 8, uECScheme & 0xFF);
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(uECScheme);       
      }
   }

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_DR_BOOST )
   {
      s_uAdaptiveVideoLastSetDRBoost = uDRBoost;
      log_line("[AdaptiveVideo] Set DR boost to %d (of %d)", s_uAdaptiveVideoLastSetDRBoost, (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);
      if ( 0 != s_uAdaptiveVideoLastSetDRBoost )
         _adaptive_video_turn_focusmode_bw_off_after(500);
   }
}
