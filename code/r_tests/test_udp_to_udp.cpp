#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define RUBY_PIPES_EXTRA_FLAGS O_NONBLOCK

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

   if ( argc < 3 )
   {
      printf("\nUsage: test_udp_to_udp [udp-port] [udp-port2]\n");
      return 0;
   }

   log_init("TestUDPToPipe");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = 5600;
   int udpportout = 5601;

   udpport = atoi(argv[1]);
   udpportout = atoi(argv[2]);


   int socket_input;
   struct sockaddr_in server_addr_input, client_addr_input;

   socket_input = socket(AF_INET , SOCK_DGRAM, 0);
   if (socket_input == -1)
   {
      log_line("Can't create input socket.");
      exit(-1);
   }

   memset(&server_addr_input, 0, sizeof(server_addr_input));
   memset(&client_addr_input, 0, sizeof(client_addr_input));

   server_addr_input.sin_family = AF_INET;
   server_addr_input.sin_addr.s_addr = inet_addr("192.168.1.2");//INADDR_ANY;
   server_addr_input.sin_port = htons( udpport );

   client_addr_input.sin_family = AF_INET;
   client_addr_input.sin_addr.s_addr = inet_addr("192.168.1.2");//INADDR_ANY;
   client_addr_input.sin_port = htons( udpport );

   /*
   if( bind(socket_input,(struct sockaddr *)&server_addr_input, sizeof(server_addr_input)) < 0 )
   {
      log_line("Failed to bind socket.");
      close(socket_input);
      exit(-1);
   }
   */
   int socketfd_output;
   struct sockaddr_in server_addr_output;

   socketfd_output = socket(AF_INET , SOCK_DGRAM, 0);
   if (socketfd_output == -1)
   {
      log_line("Can't create output socket.");
      exit(-1);
   }

   memset(&server_addr_output, 0, sizeof(server_addr_output));

   server_addr_output.sin_family = AF_INET;
   server_addr_output.sin_addr.s_addr = inet_addr("127.0.0.1");
   server_addr_output.sin_port = htons( udpportout );

   log_line("Waiting for UDP data on port %d to send to port %d ...", udpport, udpportout);

   u32 uTimeNow;
   u32 uLastTimeSecond = get_current_timestamp_ms();
   u32 uCountReads = 0;
   u32 uCountWrites = 0;
   u32 uBytesRead = 0;
   u32 uBytesWritten = 0;
   u32 uLoopsPerSec = 0;

   while (!bQuit)
   {
      uTimeNow = get_current_timestamp_ms();
      uLoopsPerSec++;

      if ( uTimeNow >= uLastTimeSecond+1000 )
      {
         log_line("Loops: %u", uLoopsPerSec);
         log_line("Read UDP: %u bps, %u reads/sec", uBytesRead*8, uCountReads);
         log_line("Write pipe: %u bps, %u writes/sec", uBytesWritten*8, uCountWrites);
         uLastTimeSecond = uTimeNow;
         uCountReads = 0;
         uCountWrites = 0;
         uBytesRead = 0;
         uBytesWritten = 0;
         uLoopsPerSec = 0;
      }

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(socket_input, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 10*1000; // 10 miliseconds timeout

      int selectResult = select(socket_input+1, &readset, NULL, NULL, &timePipeInput);
      if ( selectResult < 0 )
      {
         log_error_and_alarm("Failed to select socket.");
         continue;
      }
      if ( selectResult == 0 )
         continue;

      if( 0 == FD_ISSET(socket_input, &readset) )
         continue;


      u8 uBuffer[2025];
      socklen_t len = sizeof(client_addr_input);
      int nRecv = recvfrom(socket_input, uBuffer, 1500,
                MSG_WAITALL, ( struct sockaddr *) &client_addr_input,
                &len);
      //int nRecv = recv(socket_input, szBuff, 1024, )
      if ( nRecv < 0 )
      {
         log_error_and_alarm("Failed to receive from client.");
         break;
      }
      if ( nRecv == 0 )
         continue;

      uCountReads++;
      uBytesRead += nRecv;

      int iRes = sendto(socketfd_output, uBuffer, nRecv, 0, (struct sockaddr *)&server_addr_output, sizeof(server_addr_output) );
      if ( iRes < 0 )
      {
         log_error_and_alarm("Failed to write to socket returned code: %d, error: %s", iRes, strerror(errno));
         break;
      }
      if ( iRes != nRecv )
         log_softerror_and_alarm("Failed to write %d bytes, written %d bytes", nRecv, iRes);

      uCountWrites++;
      uBytesWritten += iRes;

   }
   close(socketfd_output);
   close(socket_input);
   log_line("\nEnded\n");
   exit(0);
}
