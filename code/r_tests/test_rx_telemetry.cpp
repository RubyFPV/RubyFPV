#include "../base/shared_mem.h"
#include "../radio/radiolink.h"

#include <time.h>
#include <sys/resource.h>

data_packet_ruby_telemetry DPRT;
data_packet_header DPH;

int main(int argc, char *argv[])
{
   setpriority(PRIO_PROCESS, 0, -10);
   log_init("Test");
   log_line("\nStarted.\n");


   exit(0);
}
