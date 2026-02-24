#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/socket.h>


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

   log_init("TestSocketIn");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int port = 5600;
   bool bSummary = false;

   if ( argc >= 2 )
      port = atoi(argv[1]);

   if ( argc >= 3 )
   if ( strcmp(argv[argc-1], "-summary") == 0 )
      bSummary = true;

   int socket_server, socket_client;
   struct sockaddr_in server_addr, client_addr;

   socket_server = socket(AF_INET , SOCK_DGRAM, 0);
   if (socket_server == -1)
   {
      log_line("Can't create socket.");
      exit(-1);
   }

   memset(&server_addr, 0, sizeof(server_addr));
   memset(&client_addr, 0, sizeof(client_addr));

   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_port = htons( port );

   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( port );

   if( bind(socket_server,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_line("Failed to bind socket.");
      close(socket_server);
      exit(-1);
   }

   log_line("Waiting for clients on port %d ...", port);

   u32 uLastTimeSecond = get_current_timestamp_ms();
   u32 uLastSecondbps = 0;
   int iCountPackets = 0;
   while (!bQuit)
   {
      char szBuff[2025];
      socklen_t len = sizeof(client_addr);
      int nRecv = recvfrom(socket_server, szBuff, 1600,
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      //int nRecv = recv(socket_server, szBuff, 1024, )
      if ( nRecv > 0 )
      {
         uLastSecondbps += nRecv;
         iCountPackets++;
         if ( ! bSummary )
         {
            szBuff[nRecv] = '\0';
            log_line("Recv %d bytes", nRecv);
         }
      }
      else
         log_line("Failed to receive from client.");

      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow >= uLastTimeSecond + 1000 )
      {
          log_line("Recv %u bytes/sec, %u bps, %d pack/sec", uLastSecondbps, uLastSecondbps*8, iCountPackets);
          uLastTimeSecond = uTimeNow;
          uLastSecondbps = 0;
          iCountPackets = 0;
      }
   }

   close(socket_server);
   log_line("\nEnded\n");
   exit(0);
}
