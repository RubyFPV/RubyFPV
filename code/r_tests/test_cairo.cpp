#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../common/string_utils.h"

#include <time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <poll.h>

bool g_bQuit = false;


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

   log_init("TestCairo");
   log_enable_stdout();
   log_line("Test Cairo UI");
   return (0);
}
