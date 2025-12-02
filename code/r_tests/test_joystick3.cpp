#include "../base/base.h"

#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>

bool g_bQuit = false;

struct js_event s_JoystickEvent;

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   g_bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("Test JOYSTICK");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int fJ = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);

   char name[256];
   if (ioctl(fJ, JSIOCGNAME(sizeof(name)), name) < 0)
      strncpy(name, "Unknown", sizeof(name));
   log_line("Name: %s", name);

   u32 uid = 0;
   if (ioctl(fJ, JSIOCGVERSION, &uid) == -1)
      log_line("Error to query for UID");

   u8 count_axes = 0;
   if (ioctl(fJ, JSIOCGAXES, &count_axes) == -1)
      log_line("Error to query for axes count");

   u8 count_buttons = 0;
   if (ioctl(fJ, JSIOCGBUTTONS, &count_buttons) == -1)
      log_line("Error to query for buttons count");
   
   log_line("UID: %u", uid);
   log_line("Has %d axes and %d buttons", count_axes, count_buttons);

   while ( ! g_bQuit )
   { 
      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 50000;

      
      int nRead = read(fJ, &s_JoystickEvent, sizeof(s_JoystickEvent));
      if ( nRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ) )
      {
          log_line("Would block.continue");
          continue;
      }
      else if ( nRead < 0 )
      {
         log_line("Invalid read error. Continuing.");
         continue;
      }
      if ( nRead == 0 )
         continue;

      char szVal[32];
      szVal[0] = 0;
      if ( s_JoystickEvent.type && JS_EVENT_INIT )
         strcpy(szVal, "[Init]");
 
      //if ( s_JoystickEvent.type && JS_EVENT_BUTTON )
      //   log_line("%s Button %u %d\n", szVal, s_JoystickEvent.number, s_JoystickEvent.value);
      if ( s_JoystickEvent.number == 1 )
      if ( s_JoystickEvent.type && JS_EVENT_AXIS )
         log_line("Axis %zu at %6d\n", s_JoystickEvent.number, s_JoystickEvent.value);
   }
   log_line("Done. Exiting.");
   exit(0);
}  