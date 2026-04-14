#include "hardware_cam_backend.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static t_hwcam_backend s_Backend = HWCAM_BE_UNKNOWN;

#define HWCAM_BE_OVERRIDE_FILE "/etc/ruby_encoder"

static t_hwcam_backend _read_override()
{
   if ( access(HWCAM_BE_OVERRIDE_FILE, R_OK) != 0 )
      return HWCAM_BE_UNKNOWN;
   FILE* fp = fopen(HWCAM_BE_OVERRIDE_FILE, "r");
   if ( NULL == fp )
      return HWCAM_BE_UNKNOWN;
   char sz[32] = {0};
   if ( NULL == fgets(sz, sizeof(sz)-1, fp) )
   {
      fclose(fp);
      return HWCAM_BE_UNKNOWN;
   }
   fclose(fp);
   for ( int i = 0; i < (int)sizeof(sz); i++ )
      if ( sz[i] == '\n' || sz[i] == '\r' || sz[i] == ' ' || sz[i] == '\t' ) sz[i] = 0;
   if ( 0 == strcasecmp(sz, "venc") || 0 == strcasecmp(sz, "waybeam") )
      return HWCAM_BE_WAYBEAM;
   if ( 0 == strcasecmp(sz, "majestic") )
      return HWCAM_BE_MAJESTIC;
   return HWCAM_BE_UNKNOWN;
}

t_hwcam_backend hwcam_be_detect()
{
   if ( s_Backend != HWCAM_BE_UNKNOWN )
      return s_Backend;

   t_hwcam_backend ov = _read_override();
   if ( ov != HWCAM_BE_UNKNOWN )
   {
      s_Backend = ov;
      log_line("[HwCamBE] Backend from %s: %s", HWCAM_BE_OVERRIDE_FILE, hwcam_be_name());
      return s_Backend;
   }

   if ( access("/usr/bin/venc", X_OK) == 0 )
      s_Backend = HWCAM_BE_WAYBEAM;
   else
      s_Backend = HWCAM_BE_MAJESTIC;

   log_line("[HwCamBE] Detected encoder backend: %s", hwcam_be_name());
   return s_Backend;
}

t_hwcam_backend hwcam_be_get()
{
   if ( s_Backend == HWCAM_BE_UNKNOWN )
      return hwcam_be_detect();
   return s_Backend;
}

const char* hwcam_be_name()
{
   switch ( hwcam_be_get() )
   {
      case HWCAM_BE_WAYBEAM: return "waybeam";
      case HWCAM_BE_MAJESTIC: return "majestic";
      default: return "unknown";
   }
}

const char* hwcam_be_binary_path()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "/usr/bin/venc" : "/usr/bin/majestic";
}

const char* hwcam_be_process_name()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "venc" : "majestic";
}

const char* hwcam_be_config_path()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "/etc/venc.json" : "/etc/majestic.yaml";
}

const char* hwcam_be_config_backup_path()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "/etc/venc.json.org" : "/etc/majestic.yaml.org";
}

const char* hwcam_be_kill9_cmd()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "killall -9 venc" : "killall -9 majestic";
}

void hwcam_be_format_start_cmd(char* dst, int dst_sz, bool bLogToFile, const char* szLogPath)
{
   if ( NULL == dst || dst_sz <= 0 )
      return;
   const char* szBin = hwcam_be_binary_path();
   // Majestic needs '-s' flag to read stdin/config; waybeam takes no flags (reads /etc/venc.json).
   const char* szFlags = (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "" : "-s ";
   // Waybeam dlopens MI vendor libs at runtime; on systems where they're not in
   // the default loader path we ship them under /usr/lib/venc/ to avoid
   // overwriting the system-installed majestic-compatible libs.
   const char* szEnv = (hwcam_be_get() == HWCAM_BE_WAYBEAM) ? "LD_LIBRARY_PATH=/usr/lib/venc " : "";
   if ( bLogToFile && (NULL != szLogPath) && (0 != szLogPath[0]) )
      snprintf(dst, dst_sz, "%s%s %s2>/dev/null 1>%s &", szEnv, szBin, szFlags, szLogPath);
   else
      snprintf(dst, dst_sz, "%s%s %s2>/dev/null 1>/dev/null &", szEnv, szBin, szFlags);
}

const char* hwcam_be_reload_cmd()
{
   if ( hwcam_be_get() == HWCAM_BE_WAYBEAM )
      return "curl -s localhost/api/v1/restart";
   return "killall -1 majestic";
}

void hwcam_be_format_cli_set(char* dst, int dst_sz, const char* szDotPath, const char* szValue, bool bQuoteString)
{
   if ( NULL == dst || dst_sz <= 0 || NULL == szDotPath || NULL == szValue )
      return;
   if ( hwcam_be_get() == HWCAM_BE_WAYBEAM )
   {
      if ( bQuoteString )
         snprintf(dst, dst_sz, "json_cli -s %s '\"%s\"' -i %s", szDotPath, szValue, hwcam_be_config_path());
      else
         snprintf(dst, dst_sz, "json_cli -s %s %s -i %s", szDotPath, szValue, hwcam_be_config_path());
   }
   else
      snprintf(dst, dst_sz, "cli -s %s %s", szDotPath, szValue);
}

bool hwcam_be_supports_idr_request()
{
   return (hwcam_be_get() == HWCAM_BE_WAYBEAM);
}

bool hwcam_be_needs_sighup_after_http()
{
   // Majestic historically needed SIGHUP for some /api/v1/set fields to take effect.
   // Waybeam applies live fields immediately via the /api/v1/set handler; no signal needed.
   return (hwcam_be_get() != HWCAM_BE_WAYBEAM);
}
