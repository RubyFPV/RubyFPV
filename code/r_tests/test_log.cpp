#include "../base/shared_mem.h"
#include "../public/utils/core_plugins_utils.h"
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

   srand(rand()+get_current_timestamp_micros());
   log_init("TestLOG");
   log_enable_stdout();
   log_line("\nStarted.\n");



   int iPID = hw_process_exists("ruby_central");
   if ( iPID > 0 )
      log_line("Proc exists: %d", iPID);
   else
      log_softerror_and_alarm("Proc does not exists !!!! (result: %d)", iPID);
   hardware_sleep_ms(100);

   int id = rand()%10000;
   int counter = 0;
   core_plugin_util_log_line("TestLog core");
   counter++;
   log_line("%d counter value: %d", id, counter);


   char szOutput32[32];
   char szOutput256[256];
   char szOutput1k[1024];
   char szOutput4k[4096];

   hw_execute_process("iwconfig wlan0 freq 5700000000", 0, szOutput4k, sizeof(szOutput4k)/sizeof(szOutput4k[0]));
   log_line("Output: [%s]", szOutput4k);
   hw_execute_process("iwconfig wlan0 freq 100000000", 0, szOutput4k, sizeof(szOutput4k)/sizeof(szOutput4k[0]));
   log_line("Output: [%s]", szOutput4k);

   hw_execute_process_wait("iw dev wlan0 set txpower fixed -2100");


   hw_execute_process("find *.txt", 0, szOutput32, sizeof(szOutput32)/sizeof(szOutput32[0]));
   log_line("Output: [%s]", szOutput32);

   hw_execute_process("find /home/pi/ruby/*.txt", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);

   hw_execute_process("blabla txt", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);

   hw_execute_process("ls .", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);

   hw_execute_process("ls /home/pi", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);

   hw_execute_process("ls -al .", 0, szOutput1k, sizeof(szOutput1k)/sizeof(szOutput1k[0]));
   log_line("Output: [%s]", szOutput1k);

/*
   hw_execute_process("find .", 0, NULL, 0);
   hw_execute_process("find .", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);
   hw_execute_process("find /home/", 0, NULL, 0);
   hw_execute_process("find /home/", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   log_line("Output: [%s]", szOutput256);
  */
   //hw_execute_process("find /usr/", 0, szOutput256, sizeof(szOutput256)/sizeof(szOutput256[0]));
   //log_line("Output: [%s]", szOutput256);
   //hw_execute_process("find /usr/", 0, szOutput4k, sizeof(szOutput4k)/sizeof(szOutput4k[0]));
   //log_line("Output: [%s]", szOutput4k);

   //hw_execute_process("find /", 1000, szOutput4k, sizeof(szOutput4k)/sizeof(szOutput4k[0]));
   //log_line("Output: [%s]", szOutput4k);

   //hw_execute_process("find /", 500, szOutput4k, sizeof(szOutput4k)/sizeof(szOutput4k[0]));
   //log_line("Output: [%s]", szOutput4k);

   //hw_execute_process_wait("find /usr/");

   log_line("Finished all.");
   exit(0);
}
