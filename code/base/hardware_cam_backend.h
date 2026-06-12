#pragma once
#include "base.h"

/*
   OpenIPC camera/encoder backend selector.

   Ruby historically spoke only to majestic on OpenIPC. waybeam
   (OpenIPC/waybeam_venc) is an alternative encoder that is 1:1 RTP
   compatible with majestic and accepts the same /api/v1/set?... HTTP
   calls (camelCase alias table). Process name, binary path, config
   file, reload mechanism and pre-start config tool differ.

   waybeam v0.10.11+ renamed the binary venc -> waybeam and the config
   /etc/venc.json -> /etc/waybeam.json. Both layouts are supported here;
   the legacy venc paths are detected at runtime and used as fallback.

   Detection:
     1. If /etc/ruby_encoder exists, read one token (majestic|waybeam|venc).
     2. Else if /usr/bin/waybeam or /usr/bin/venc exists, waybeam.
     3. Else majestic.
   Runs once; cached. (/etc is persistent on OpenIPC overlay; /boot
   does not exist on SSC338Q-style flash layouts.)
*/

typedef enum
{
   HWCAM_BE_UNKNOWN = 0,
   HWCAM_BE_MAJESTIC = 1,
   HWCAM_BE_WAYBEAM = 2,
} t_hwcam_backend;

#ifdef __cplusplus
extern "C" {
#endif

t_hwcam_backend hwcam_be_detect();
t_hwcam_backend hwcam_be_get();
const char* hwcam_be_name();

const char* hwcam_be_binary_path();       // e.g. "/usr/bin/majestic", "/usr/bin/waybeam"
const char* hwcam_be_process_name();      // e.g. "majestic", "waybeam"
const char* hwcam_be_config_path();       // e.g. "/etc/majestic.yaml", "/etc/waybeam.json"
const char* hwcam_be_config_backup_path();// e.g. "/etc/majestic.yaml.org"
const char* hwcam_be_kill9_cmd();         // "killall -9 majestic"

// Start command. Writes into dst. bLogToFile=true routes stdout to szLogPath.
void hwcam_be_format_start_cmd(char* dst, int dst_sz, bool bLogToFile, const char* szLogPath);

// Reload/reinit. For majestic this is "killall -1 majestic" (SIGHUP).
// For waybeam this is "curl -s localhost/api/v1/restart" which reloads the
// JSON config and rebuilds the full pipeline INCLUDING sensor mode
// re-selection (v0.9.0+ fork+exec respawn). Use after restart-class config
// changes such as a resolution change.
const char* hwcam_be_reload_cmd();

// Pre-start set of a config field (used when encoder is NOT running).
// For majestic: "cli -s .foo.bar VALUE"
// For waybeam:  "json_cli -s .foo.bar VALUE -i /etc/waybeam.json"  (numeric/bool)
//               "json_cli -s .foo.bar '\"VALUE\"' -i /etc/waybeam.json"  (string)
// bQuoteString controls JSON quoting on the waybeam path.
void hwcam_be_format_cli_set(char* dst, int dst_sz, const char* szDotPath, const char* szValue, bool bQuoteString);

// Runtime set of a config field over HTTP (encoder running). Same URL scheme
// on both backends: "curl -s localhost/api/v1/set?KEY=VALUE".
// On waybeam, live fields apply immediately; restart-class fields persist to
// the JSON config and trigger an automatic pipeline restart.
void hwcam_be_format_http_set(char* dst, int dst_sz, const char* szKey, const char* szValue);

// Onboard SD recording control (waybeam only; callers must gate on
// hwcam_be_supports_onboard_recording()).
const char* hwcam_be_record_start_cmd();  // "curl -s 'localhost/api/v1/record/start?dir=...'"
const char* hwcam_be_record_stop_cmd();   // "curl -s localhost/api/v1/record/stop"
const char* hwcam_be_record_status_cmd(); // "curl -s localhost/api/v1/record/status"

// Capability probes.
bool hwcam_be_supports_idr_request();        // waybeam: true, majestic: false
bool hwcam_be_needs_sighup_after_http();     // majestic: true for some fields, waybeam: false
bool hwcam_be_supports_onboard_recording();  // waybeam: true, majestic: false

#ifdef __cplusplus
}
#endif
