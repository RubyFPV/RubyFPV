#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include <time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <getopt.h>
#include <poll.h>

#define RUBY_PIPES_EXTRA_FLAGS O_NONBLOCK

bool bQuit = false;
int g_iPipeFD = -1;
char szPipeName[64];


// Extract SO_RXQ_OVFL counter
uint32_t extract_rxq_overflow(struct msghdr *msg)
{
    struct cmsghdr *cmsg;
    uint32_t rtn;

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_RXQ_OVFL) {
            memcpy(&rtn, CMSG_DATA(cmsg), sizeof(rtn));
            return rtn;
        }
    }
    return 0;
}

int read_async_udp(int fSocket, u8* pOutput)
{
   static uint8_t bufvideoin[1200];
   static ssize_t rsize = 0;
   static uint8_t cmsgbuf[CMSG_SPACE(sizeof(uint32_t))];

   static int nfds = 1;
   static struct pollfd fds[2];
   static bool s_bFirstVideoReadSetup = true;

   if ( s_bFirstVideoReadSetup )
   {
      s_bFirstVideoReadSetup = false;
      memset(fds, '\0', sizeof(fds));
      if ( fcntl(fSocket, F_SETFL, fcntl(fSocket, F_GETFL, 0) | O_NONBLOCK) < 0 )
         log_softerror_and_alarm("Unable to set socket into nonblocked mode: %s", strerror(errno));

      fds[0].fd = fSocket;
      fds[0].events = POLLIN;

      log_line("Done first time video read setup.");
   }

   int res = poll(fds, nfds, 1);
   if ( res < 0 )
   {
      log_error_and_alarm("Failed to poll UDP socket.");
      return -1;
   }
   if ( 0 == res )
      return 0;

   if (fds[0].revents & (POLLERR | POLLNVAL))
      log_softerror_and_alarm("socket error: %s", strerror(errno));

   if ( ! (fds[0].revents & POLLIN) )
      return 0;


   struct iovec iov = { .iov_base = (void*)bufvideoin,
                                      .iov_len = sizeof(bufvideoin) };

   struct msghdr msghdr = { .msg_name = NULL,
                            .msg_namelen = 0,
                            .msg_iov = &iov,
                            .msg_iovlen = 1,
                            .msg_control = &cmsgbuf,
                            .msg_controllen = sizeof(cmsgbuf),
                            .msg_flags = 0 };

   memset(cmsgbuf, '\0', sizeof(cmsgbuf));

   rsize = recvmsg(fSocket, &msghdr, 0);
   if ( rsize < 0 )
   {
      log_softerror_and_alarm("Failed to recvmsg, error: %s", strerror(errno));
      return -1;
   }

   if ( rsize > 1200 )
   {
      log_softerror_and_alarm("Read too much data %d bytes", rsize);
      rsize = 1200;
   }

   static uint32_t rxq_overflow = 0;
   uint32_t cur_rxq_overflow = extract_rxq_overflow(&msghdr);
   if (cur_rxq_overflow != rxq_overflow)
   {
       log_softerror_and_alarm("UDP rxq overflow: %u packets dropped (from %u to %u)", cur_rxq_overflow - rxq_overflow, rxq_overflow, cur_rxq_overflow);
       rxq_overflow = cur_rxq_overflow;
   }

   memcpy(pOutput, bufvideoin, rsize);

   return (int)rsize;
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

   if ( argc < 4 )
   {
      printf("\nUsage: test_udp_to_radio [udp-port] freq port (14 uplink) [-async] [-double]\n");
      return 0;
   }

   log_init("TestUDPToRadio");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int udpport = 5600;
   udpport = atoi(argv[1]);
   int iFreq = atoi(argv[2]);
   int iPort = atoi(argv[3]);


   bool bAsync = false;

   if ( argc >= 5 )
   if ( strcmp(argv[argc-1], "-async") == 0 )
   {
      bAsync = true;
      log_line("Use async read of UDP socket.");
   }

   bool bDouble = false;
   if ( argc >= 5 )
   if ( strcmp(argv[4], "-double") == 0 )
   {
      bDouble = true;
      log_line("Use double output");
   }
   if ( argc >= 6 )
   if ( strcmp(argv[argc-1], "-double") == 0 )
   {
      bDouble = true;
      log_line("Use double output");
   }

   hardware_enumerate_radio_interfaces();
   radio_init_link_structures();
   radio_enable_crc_gen(1);
   radio_set_out_datarate(12, 0, get_current_timestamp_ms());
   radio_set_frames_flags(RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA, get_current_timestamp_ms());
   radio_utils_set_interface_frequency(NULL, 0, -1, iFreq*1000, NULL, 0);

   int pcapW = radio_open_interface_for_write(0);

   log_line("Opened wlan interface for write.");

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
         log_line("Write radio: %u bps, %u writes/sec", uBytesWritten*8, uCountWrites);
         uLastTimeSecond = uTimeNow;
         uCountReads = 0;
         uCountWrites = 0;
         uBytesRead = 0;
         uBytesWritten = 0;
         uLoopsPerSec = 0;
      }

      u8 uBuffer[2025];
      int nRecv = 0;

      if ( bAsync )
      {
         nRecv = read_async_udp(socket_server, uBuffer);
      }
      else
      {
         fd_set readset;
         FD_ZERO(&readset);
         FD_SET(socket_server, &readset);

         struct timeval timePipeInput;
         timePipeInput.tv_sec = 0;
         timePipeInput.tv_usec = 5*1000; // 10 miliseconds timeout

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


         socklen_t len = sizeof(client_addr);
         nRecv = recvfrom(socket_server, uBuffer, 1200,
                   MSG_WAITALL, ( struct sockaddr *) &client_addr,
                   &len);
         //int nRecv = recv(socket_server, szBuff, 1024, )
      }

      if ( nRecv < 0 )
      {
         log_error_and_alarm("Failed to receive from UDP port.");
         break;
      }

      if ( nRecv == 0 )
         continue;

      uCountReads++;
      uBytesRead += nRecv;

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_RUBY;
      PH.vehicle_id_src = 0;
      PH.vehicle_id_dest = PH.vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + nRecv;
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), uBuffer, nRecv);

      u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
      int totalLength = radio_build_new_raw_ieee_packet(0, rawPacket, packet, PH.total_length, iPort, 0);

      if ( 1 != radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
      {
         log_line("Failed to write to radio interface.");
         continue;
      }

      uCountWrites++;
      uBytesWritten += nRecv;

      if ( bDouble )
      {
         if ( 1 != radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
         {
            log_line("Failed to write to radio interface.");
            continue;
         }
         uCountWrites++;
         uBytesWritten += nRecv;
      }
   }
   radio_close_interface_for_write(0);
   close(socket_server);
   log_line("\nEnded\n");
   exit(0);
}
