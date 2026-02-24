#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/commands.h"
#include <time.h>
#include <sys/resource.h>

bool quit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
}

int main(int argc, char *argv[])
{
   setpriority(PRIO_PROCESS, 0, -10);

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RX_Commands");

   commands_reply_start_rx();

   log_line("Started.");
   log_line("------------------------------------------");

   data_packet_header* pdph = NULL;
   data_packet_command_response* pdpcr = NULL;

   while (!quit)
   {
      int length = 0;
      u8* pBuffer = commands_reply_pool_get_response(&length, 50000);
      if ( NULL == pBuffer )
         continue;
      pdph = (data_packet_header*)pBuffer;
      pdpcr = (data_packet_command_response*)(pBuffer + sizeof(data_packet_header));
      log_line("Received command reply nb.%d, command id: %d, extra info size: %d", pdpcr->response_counter, pdpcr->origin_command_id, length-sizeof(data_packet_header)-sizeof(data_packet_command_response));

   }
   commands_reply_stop_rx();

   return (0);
}
