#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>


bool bQuit = false;
bool bFile = false;
bool bSummary = false;
FILE* fOut = NULL;

   u32 uCountReads = 0;
   u32 uCountWrites = 0;
   u32 uBytesRead = 0;
   u32 uBytesWritten = 0;

u16 uLastSeqNumber = 0;

u32 reverseBytes(u32 a)
{
return (((a<<24) & 0xFF000000) |
((a<<8) & 0x00FF0000) |
((a>>8) & 0x0000FF00) |
((a>>24) & 0x000000FF));
}

bool _parse_udp_packet(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength < 12) )
   {
      log_softerror_and_alarm("Invalid input");
      return false;
   }

   u8* pData = pBuffer;
   u8 h[6];
   h[0] = *pData;
   pData++;
   h[1] = *pData;
   pData++;
   h[2] = *pData;
   pData++;
   h[3] = *pData;
   h[4] = *(pBuffer+12);
   h[5] = *(pBuffer+13);
   u16 uSeqNb = (((u16)h[2]) << 8) | h[3];
   u8 uNALHeader = h[4];
   u8 uNALType = h[4] & 0x1F;
   u8 uFUIndicator = h[4];
   u8 uFUHeader = h[5];


   u8 e[4];
   e[0] = *(pBuffer+iLength-4);
   e[1] = *(pBuffer+iLength-3);
   e[2] = *(pBuffer+iLength-2);
   e[3] = *(pBuffer+iLength-1);

   if ( 0 != uLastSeqNumber )
   if ( (uLastSeqNumber+1) != uSeqNb )
      log_softerror_and_alarm("Invalid seq nb, from %d to %d", uLastSeqNumber, uSeqNb);

   uLastSeqNumber = uSeqNb;

   if ( ! bSummary )
   {
      if ( uNALType < 24 )
      log_line("Seq %d, len %d, %02X %02X %02X %02X, nal: %02X (%d or %d, %d%d%d%d%d%d%d%d)",
          (int)uSeqNb, iLength, e[0], e[1], e[2], e[3], h[4], h[4] >> 3, uNALType,
            h[4] & 0x01,
            (h[4]>>1) & 0x01,
            (h[4]>>2) & 0x01,
            (h[4]>>3) & 0x01,
            (h[4]>>4) & 0x01,
            (h[4]>>5) & 0x01,
            (h[4]>>6) & 0x01,
            (h[4]>>7) & 0x01);
     else
      log_line("Seq %d, len %d, %02X %02X %02X %02X, nal: %02X (%d or %d, %d%d%d%d%d%d%d%d), FU type: %d, SER: %d%d%d",
          (int)uSeqNb, iLength, e[0], e[1], e[2], e[3], h[4], h[4] >> 3, uNALType,
            h[4] & 0x01,
            (h[4]>>1) & 0x01,
            (h[4]>>2) & 0x01,
            (h[4]>>3) & 0x01,
            (h[4]>>4) & 0x01,
            (h[4]>>5) & 0x01,
            (h[4]>>6) & 0x01,
            (h[4]>>7) & 0x01,
            uFUHeader & 0x1F,
            uFUHeader >> 7,
            (uFUHeader>>6) & 0x01,
            (uFUHeader>>5) & 0x01);
   }

   if ( iLength <= 12 )
      return true;
   if ( ! bFile )
      return true;

   if ( NULL == fOut )
   {
      log_softerror_and_alarm("Error file.");
      return false;
   }

   u8 uNALStart[4];
   uNALStart[0] = 0;
   uNALStart[1] = 0;
   uNALStart[2] = 0;
   uNALStart[3] = 0x01;


   static bool bFirstWrite = true;
   if ( bFirstWrite )
   //if ( uBytesWritten > 10000 )
   //if ( uBytesWritten < 12000 )
   {
      bFirstWrite = false;
      u8 buff[5];
      for(int i=0; i<5; i++ )
         buff[i] = 32+i;
      fwrite(buff, 1, 5, fOut);
   }
   /**/

   /*
   int iCountEsc = 0;
   for( int i=12; i<iLength-3; i++ )
   {
      if ( *(pBuffer+i) == 0 )
      if ( *(pBuffer+i+1) == 0 )
      if ( (*(pBuffer+i+2) == 0) || (*(pBuffer+i+2) == 0x01) || (*(pBuffer+i+2) == 0x02) || (*(pBuffer+i+2) == 0x03) )
      {
         iCountEsc++;
         log_line("Found escape seq %d: %02x %02x at pos %d", iCountEsc, *(pBuffer+i+2), *(pBuffer+i+3), i);
         for( int k=iLength-1; k>=i; k-- )
         {
             *(pBuffer+k) = *(pBuffer+k-1);
         }
         *(pBuffer+i+2) = 0x03;
         iLength++;
      }
   }
   */

   if ( uNALType < 24 )
   {
      //uNALType = uNALType << 3;
      //fwrite(&uNALType, 1, 1, fOut);
      fwrite(uNALStart, 1, 4, fOut);
      fwrite(pBuffer+12, 1, iLength-12, fOut);
      uCountWrites++;
      uBytesWritten += iLength-12;
   }
   else if ( uFUHeader >> 7 ) // Start
   {
      //u8 uNALType = uNALHeader & 0xE0;
      uNALType = 0;
      uNALType = 0x60 | (uFUHeader & 0x1F);
      //uNALType = uNALType << 3;

      fwrite(uNALStart, 1, 4, fOut);
      fwrite(&uNALType, 1, 1, fOut);
      fwrite(pBuffer+14, 1, iLength-14, fOut);
      uCountWrites++;
      uBytesWritten += iLength-14;
   }
   else if ( (uFUHeader>>6) & 0x01 ) // End
   {
      fwrite(pBuffer+14, 1, iLength-14, fOut);
      uCountWrites++;
      uBytesWritten += iLength-14;
   }
   else // Middle
   {
      fwrite(pBuffer+14, 1, iLength-14, fOut);
      uCountWrites++;
      uBytesWritten += iLength-14;
   }

   return true;
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
      printf("\nUsage: test_rtpudp_read udp-port [-file] [-summary] \n");
      return 0;
   }

   log_init("TestReadRTPUDP");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = atoi(argv[1]);

   for( int i=2; i<5; i++ )
   {
      if ( argc > i )
      if ( 0 == strcmp(argv[i], "-file") )
         bFile = true;

      if ( argc > i )
      if ( 0 == strcmp(argv[i], "-summary") )
         bSummary = true;
   }

   log_line("Use file: %s", bFile?"Yes":"No");
   log_line("Summary: %s", bSummary?"Yes":"No");

   if ( bFile )
   {
      hw_execute_bash_command("rm -rf fileout.h264", NULL);
      fOut = fopen("fileout.h264", "wb");
   }

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
   u32 uLoopsPerSec = 0;

   while (!bQuit)
   {
      uTimeNow = get_current_timestamp_ms();
      uLoopsPerSec++;

      if ( uTimeNow >= uLastTimeSecond+1000 )
      {
         log_line("Loops: %u", uLoopsPerSec);
         log_line("Read UDP: %u bps, %u reads/sec", uBytesRead*8, uCountReads);
         log_line("Write output: %u bps, %u writes/sec", uBytesWritten*8, uCountWrites);
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

      _parse_udp_packet(uBuffer, nRecv);
   }
   if ( NULL != fOut )
      fclose(fOut);

   close(socket_server);
   log_line("\nEnded\n");
   exit(0);
}
