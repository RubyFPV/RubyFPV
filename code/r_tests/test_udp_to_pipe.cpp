#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define RUBY_PIPES_EXTRA_FLAGS O_NONBLOCK

bool bQuit = false;
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
   if (  access( szPipeName, W_OK ) != -1 )
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
      printf("\nUsage: test_udp_to_pipe [udp-port] [pipename or \"tmp/ruby/fifocam1\"]\n");
      return 0;
   }

   log_init("TestUDPToPipe");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = 5600;
   strcpy(szPipeName, "FIFO_RUBY_CAMERA1");

   udpport = atoi(argv[1]);

   if ( argc >= 3 )
      strcpy(szPipeName, argv[2]);

   if ( ! pipe_exists() )
   {
      log_line("Pipe [%s] does not exists. Create it.", szPipeName);
      char szComm[256];
      sprintf(szComm, "mkfifo %s", szPipeName );
      hw_execute_bash_command(szComm, NULL);
   }

   log_line("Pipe [%s] exists now.", szPipeName);

   g_iPipeFD = -1;

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
   server_addr.sin_port = htons( udpport );

   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( udpport );

   if( bind(socket_server,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_line("Failed to bind socket.");
      close(socket_server);
      exit(-1);
   }

   log_line("Waiting for UDP data on port %d ...", udpport);

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
      FD_SET(socket_server, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 10*1000; // 10 miliseconds timeout

      int selectResult = select(socket_server+1, &readset, NULL, NULL, &timePipeInput);
      if ( selectResult < 0 )
      {
         log_error_and_alarm("Failed to select socket.");
         continue;
      }
      if ( selectResult == 0 )
         continue;

      if( 0 == FD_ISSET(socket_server, &readset) )
         continue;


      u8 uBuffer[2025];
      socklen_t len = sizeof(client_addr);
      int nRecv = recvfrom(socket_server, uBuffer, 1500,
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      //int nRecv = recv(socket_server, szBuff, 1024, )
      if ( nRecv < 0 )
      {
         log_error_and_alarm("Failed to receive from client.");
         break;
      }
      if ( nRecv == 0 )
         continue;

      uCountReads++;
      uBytesRead += nRecv;

      if ( g_iPipeFD == -1 )
      {
         hardware_sleep_ms(10);
         if ( ! pipe_exists() )
            continue;

         log_line("Pipe [%s] exists. Open it to write to it.", szPipeName);

        g_iPipeFD = open(szPipeName, O_WRONLY | (RUBY_PIPES_EXTRA_FLAGS & (~O_NONBLOCK)));
        //g_iPipeFD = open(szPipeName, O_APPEND);
        if ( g_iPipeFD < 0 )
        {
            printf("Can't open input pipe [%s]\n", szPipeName);
            continue;
        }
        //if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
        //if ( 0 != fcntl(g_iPipeFD, F_SETFL, O_NONBLOCK) )
        //   log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s pipe write endpoint.", szPipeName);

        log_line("[IPC] FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(g_iPipeFD, F_GETFL)));
      }

      int iWrite = write(g_iPipeFD, uBuffer, nRecv);

      if ( iWrite < 0 )
      {
         log_error_and_alarm("Failed to write to pipe, error: %s", strerror(errno));
         g_iPipeFD = -1;
         continue;
      }

      if ( iWrite < nRecv )
         log_softerror_and_alarm("Failed to write %d bytes. Only %d bytes written to pipe.", nRecv, iWrite);

      uCountWrites++;
      uBytesWritten += iWrite;
   }
   close(g_iPipeFD);
   close(socket_server);
   log_line("\nEnded\n");
   exit(0);
}
