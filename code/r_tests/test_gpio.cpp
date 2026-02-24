#include <stdio.h>
#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/gpio.h"

bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}

int main (void)
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestGPIO");
   log_enable_stdout();
   log_line("\nStarted.\n");


   init_hardware();

   u32 uTimeLastCheck = get_current_timestamp_ms();
   int iCounter = 0;
   while ( ! bQuit )
   {
      hardware_loop();
      int iValue = GPIORead(GPIOGetPinMenu());
      log_line("Menu: %d", iValue);
      iCounter++;
      u32 uTime = get_current_timestamp_ms();
      if ( uTime > uTimeLastCheck + 1000 )
      {
         log_line("Reads/sec: %d", iCounter);
         iCounter = 0;
         uTimeLastCheck = uTime;
      }
   }
}