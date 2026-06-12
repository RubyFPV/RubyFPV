#pragma once
#include "../base/base.h"

// Phase 2: onboard OSD recording on the vehicle.
//
// Writes the same 40-byte-header-then-frames .osd format used by the GS
// (code/r_station/rx_video_recording_data.cpp) so VueOSD can open onboard
// recordings directly without post-processing.
//
// Frame format: u32 timestamp_ms (since start) + DEFAULT_MSPOSD_RECORDING_COLS
// * DEFAULT_MSPOSD_RECORDING_ROWS u16 character codes, row-major.
//
// Data source: local type_msp_parse_state populated by a tap in
// telemetry_msp.cpp (see rx_osd_recording_vehicle_feed_msp).

void rx_osd_recording_vehicle_start(const char* szBasePath); // szBasePath = "/mnt/mmcblk0p1/rec_..."; writer appends ".osd"
void rx_osd_recording_vehicle_stop();
bool rx_osd_recording_vehicle_is_started();

// Called by telemetry_msp.cpp on every incoming MSP byte burst so the
// vehicle maintains its own character-grid state for the writer.
void rx_osd_recording_vehicle_feed_msp(u8* pData, int iLen, bool bAllowScreenUpdate);

// Called by telemetry_msp periodic loop; polls for the control file (written
// by ruby_start's ONBOARD_REC_CONTROL handler from a different process) and
// starts/stops the writer accordingly. Also emits an OSD frame ~50ms.
void rx_osd_recording_vehicle_periodic_loop();

// Cross-process signaling helpers. ruby_start writes the control file on
// start (ONBOARD_REC_CONTROL cmd=1) with the base path; it's unlinked on
// stop (cmd=0). ruby_tx_telemetry polls the file in its periodic loop.
// File lives in FOLDER_RUBY_TEMP (per-platform tmpfs Ruby already uses).
#define RX_OSD_REC_CONTROL_FILENAME "onboard_osd_rec.path"
void rx_osd_recording_vehicle_signal_start(const char* szBasePath);
void rx_osd_recording_vehicle_signal_stop();
