#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>

bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestCAMERA");
   log_enable_stdout();
   log_line("\nStarted.\n");

   bool bCameraFound = false;
   char szOutput[4096];
   szOutput[0] = 0;
   hw_execute_bash_command_raw("i2cdetect -y 0 2>/dev/null | grep -iF 3b", szOutput);
   if ( 0 < strlen(szOutput) )
   {
      log_line("Hardware: Veye Camera detected.");
      bCameraFound = true;
   }
   szOutput[0] = 0;
   hw_execute_bash_command_raw("i2cdetect -y 0 2>/dev/null | grep -iF 0f", szOutput);
   if ( 0 < strlen(szOutput) )
   {
      log_line("Hardware: HDMI Camera detected.");
      bCameraFound = true;
   }
   if ( ! bCameraFound )
   {
      char szBuff[1024];
      szBuff[0] = 0;
      hw_execute_bash_command("/usr/bin/vcgencmd get_camera | python -c 'import sys, re; s = sys.stdin.read(); s=re.sub(\"supported=\\d+ detected=\", \"\", s); print(s);'", szBuff);
      if ( strcmp(szBuff, "1") == 0 )
      {
         log_line("Hardware: CSI Camera detected.");
         bCameraFound = true;
      }
   }
   if ( ! bCameraFound )
      log_line("Hardware: No camera detected.");
   exit(0);
}
