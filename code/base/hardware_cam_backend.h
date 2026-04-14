#pragma once
#include "base.h"

/*
   OpenIPC camera/encoder backend selector.

   Ruby historically spoke only to majestic on OpenIPC. waybeam_venc
   (OpenIPC/waybeam_venc) is an alternative encoder that is 1:1 RTP
   compatible with majestic and accepts the same /api/v1/set?... HTTP
   calls (camelCase alias table). Process name, binary path, config
   file, reload mechanism and pre-start config tool differ.

   Detection:
     1. If /etc/ruby_encoder exists, read one token (majestic|venc).
     2. Else if /usr/bin/venc exists, waybeam.
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

const char* hwcam_be_binary_path();       // e.g. "/usr/bin/majestic"
const char* hwcam_be_process_name();      // e.g. "majestic"
const char* hwcam_be_config_path();       // e.g. "/etc/majestic.yaml"
const char* hwcam_be_config_backup_path();// e.g. "/etc/majestic.yaml.org"
const char* hwcam_be_kill9_cmd();         // "killall -9 majestic"

// Start command. Writes into dst. bLogToFile=true routes stdout to szLogPath.
void hwcam_be_format_start_cmd(char* dst, int dst_sz, bool bLogToFile, const char* szLogPath);

// Reload/reinit. For majestic this is "killall -1 majestic" (SIGHUP).
// For waybeam this is "curl -s localhost/api/v1/restart".
const char* hwcam_be_reload_cmd();

// Pre-start set of a config field (used when encoder is NOT running).
// For majestic: "cli -s .foo.bar VALUE"
// For waybeam:  "json_cli -s .foo.bar VALUE -i /etc/venc.json"  (numeric/bool)
//               "json_cli -s .foo.bar '\"VALUE\"' -i /etc/venc.json"  (string)
// bQuoteString controls JSON quoting on the waybeam path.
void hwcam_be_format_cli_set(char* dst, int dst_sz, const char* szDotPath, const char* szValue, bool bQuoteString);

// Capability probes.
bool hwcam_be_supports_idr_request();     // waybeam: true, majestic: false
bool hwcam_be_needs_sighup_after_http();  // majestic: true for some fields, waybeam: false

#ifdef __cplusplus
}
#endif
