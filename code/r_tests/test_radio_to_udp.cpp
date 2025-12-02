#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

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

   if ( argc < 4 )
   {
      printf("\nUsage: test_radio_to_udp udp-port freq radio-port (15 for downlink) [-no-outup]\n");
      return 0;
   }

   log_init("TestRadioToUDP");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = 5600;
   udpport = atoi(argv[1]);
   int iFreq = atoi(argv[2]);
   int iPort = atoi(argv[3]);
   bNoOutput = false;
   if ( argc >= 5 )
   if ( 0 == strcmp(argv[4], "-no-output") )
      bNoOutput = true;
 
   
   hardware_enumerate_radio_interfaces();
   radio_init_link_structures();
   radio_enable_crc_gen(1);

   radio_utils_set_interface_frequency(NULL, 0, -1, iFreq*1000, NULL, 0);

   int pcapR =  radio_open_interface_for_read(0, iPort);
   if ( pcapR < 0 )
   {
      printf("\nFailed to open port.\n");
      return -1;
   }

   printf("\nStart receiving on radio %d Mhz, port: %d\n", iFreq, iPort);

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
	
   log_line("Sending data from radio to udp port %d ...", udpport);
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
         log_line("Read radio: %u bps, %u reads/sec", uBytesRead*8, uCountReads);
         log_line("Write UDP: %u bps, %u writes/sec", uBytesWritten*8, uCountWrites);
         uLastTimeSecond = uTimeNow;
         uCountReads = 0;
         uCountWrites = 0;
         uBytesRead = 0;
         uBytesWritten = 0;
         uLoopsPerSec = 0;
      }

      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 2000;
               
      FD_ZERO(&readset);
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(0);
      FD_SET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &readset);
    
      int nResult = select(pNICInfo->runtimeInterfaceInfoRx.selectable_fd+1, &readset, NULL, NULL, &to);

      if ( nResult < 0 )
         break;

      if ( nResult == 0 )
         continue;

      int nPacketLength = 0;
      u8* pPacketBuffer = radio_process_wlan_data_in(0, &nPacketLength, NULL, g_TimeNow); 
      if ( NULL == pPacketBuffer )
      {
         log_line("NULL receive buffer. Ignoring...\n");
         continue;
      }

      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      int iDataSize = pPH->total_length - sizeof(t_packet_header);
      //log_line("Recv radio %d bytes, %d data bytes", nPacketLength, iDataSize);

      uCountReads++;
      uBytesRead += iDataSize;

      if ( bNoOutput )
         continue;
      int iRes = sendto(socketfd, pPacketBuffer + sizeof(t_packet_header), iDataSize, 0, (struct sockaddr *)&server_addr, sizeof(server_addr) );
      if ( iRes < 0 )
      {
         log_error_and_alarm("Failed to write to socket returned code: %d, error: %s", iRes, strerror(errno));
         break;
      }
      if ( iRes != iDataSize )
         log_softerror_and_alarm("Failed to write %d bytes, written %d bytes", iDataSize, iRes);

      uCountWrites++;
      uBytesWritten += iRes;
   }

   radio_close_interface_for_read(0);
   close(socketfd);
   log_line("\nEnded\n");
   exit(0);
}
