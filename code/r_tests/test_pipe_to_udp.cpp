#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define RUBY_PIPES_EXTRA_FLAGS O_NONBLOCK

bool bQuit = false;
bool bNoOutput = false;
int g_iPipeFD = -1;
char szPipeName[64];


int pipe_exists()
{
   struct stat statsBuff;
   memset(&statsBuff, 0, sizeof(statsBuff));
   int iRes = stat(szPipeName, &statsBuff);
   if ( 0 != iRes )
   {
      log_line("Failed to stat pipe [%s], error: %d, [%s]", szPipeName, iRes, strerror(errno));
      return 0;
   }

   if ( S_ISFIFO(statsBuff.st_mode) )
   if ( access( szPipeName, R_OK ) != -1 )
      return 1;
   return 0;
}

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

   if ( argc < 2 )
   {
      printf("\nUsage: test_pipe_to_udp [udp-port] [pipename or \"tmp/ruby/fifocam1\"] [-no-outup]\n");
      return 0;
   }

   log_init("TestPipeToUDP");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = 5600;

   strcpy(szPipeName, "FIFO_RUBY_CAMERA1");

   udpport = atoi(argv[1]);

   if ( argc >= 3 )
      strcpy(szPipeName, argv[2]);

   bNoOutput = false;
   if ( argc >= 4 )
   if ( 0 == strcmp(argv[3], "-no-output") )
      bNoOutput = true;

   g_iPipeFD = -1;

   struct addrinfo hints;
   memset(&hints,0,sizeof(hints));
   hints.ai_family=AF_UNSPEC;
   hints.ai_socktype=SOCK_DGRAM;
   hints.ai_protocol=0;
   hints.ai_flags=AI_ADDRCONFIG;
   struct addrinfo* res=0;

   int socketfd;
   struct sockaddr_in server_addr;

   socketfd = socket(AF_INET , SOCK_DGRAM, 0);
   if (socketfd == -1)
   {
      log_line("Can't create socket.");
      exit(-1);
   }

   memset(&server_addr, 0, sizeof(server_addr));

   server_addr.sin_family = AF_INET;
   //server_addr.sin_addr.s_addr = inet_addr("192.168.42.129");
   server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
   server_addr.sin_port = htons( udpport );

   log_line("Sending data from [%s] to udp port %d ...", szPipeName, udpport);
   if ( bNoOutput )
      log_line("Output is disabled");
   else
      log_line("Output is enabled");
   u8 uBuffer[1600];
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
         if ( -1 == g_iPipeFD )
            log_line("Read (NO) pipe: %u bps, %u reads/sec", uBytesRead*8, uCountReads);
         else
            log_line("Read pipe: %u bps, %u reads/sec", uBytesRead*8, uCountReads);
         log_line("Write UDP: %u bps, %u writes/sec", uBytesWritten*8, uCountWrites);
         uLastTimeSecond = uTimeNow;
         uCountReads = 0;
         uCountWrites = 0;
         uBytesRead = 0;
         uBytesWritten = 0;
         uLoopsPerSec = 0;
      }

      if ( g_iPipeFD == -1 )
      {
         hardware_sleep_ms(10);
         if ( ! pipe_exists() )
            continue;
         log_line("Pipe [%s] exists. Open it to read from it.", szPipeName);
         g_iPipeFD = open( szPipeName, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);
         if ( g_iPipeFD < 0 )
         {
             printf("Can't open input pipe [%s]\n", szPipeName);
             return -1;
         }
         log_line("Pipe [%s] opened to read from it.", szPipeName);
      }

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(g_iPipeFD, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 100; // .1 miliseconds timeout

      int exceptResult = select(g_iPipeFD+1, NULL, NULL, &readset, &timePipeInput);
      if ( exceptResult > 0 )
      {
         log_softerror_and_alarm("Exception on pipe. Close and reopen it.");
         g_iPipeFD = -1;
         continue;
      }

      FD_ZERO(&readset);
      FD_SET(g_iPipeFD, &readset);
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 10*1000; // 10 miliseconds timeout

      int selectResult = select(g_iPipeFD+1, &readset, NULL, NULL, &timePipeInput);
      if ( selectResult < 0 )
         break;
      if ( selectResult == 0 )
         continue;
      if( 0 == FD_ISSET(g_iPipeFD, &readset) )
         continue;

      int iRead = read(g_iPipeFD, uBuffer, 1500);
      if ( iRead < 0 )
      {
         log_error_and_alarm("Failed to read from video input fifo: %s, returned code: %d, error: %s", szPipeName, iRead, strerror(errno));
         break;
      }
      if ( iRead == 0 )
      {
         //log_error_and_alarm("Failed to read from video input fifo: %s, zero read, returned code: %d, error: %s", szPipeName, iRead, strerror(errno));
         continue;
      }

      uCountReads++;
      uBytesRead += iRead;

      if ( bNoOutput )
         continue;
      int iRes = sendto(socketfd, uBuffer, iRead, 0, (struct sockaddr *)&server_addr, sizeof(server_addr) );
      if ( iRes < 0 )
      {
         log_error_and_alarm("Failed to write to socket returned code: %d, error: %s", iRes, strerror(errno));
         break;
      }
      if ( iRes != iRead )
         log_softerror_and_alarm("Failed to write %d bytes, written %d bytes", iRead, iRes);

      uCountWrites++;
      uBytesWritten += iRes;
   }

   close(g_iPipeFD);
   close(socketfd);
   log_line("\nEnded\n");
   exit(0);
}
