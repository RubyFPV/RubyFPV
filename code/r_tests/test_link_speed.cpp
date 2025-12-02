#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/radio_utils.h"

#include <time.h>
#include <sys/resource.h>

bool quit = false;

#define UPLINK 1
#define DOWNLINK 2

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
} 


int main(int argc, char *argv[])
{
   if ( argc < 2 )
   {
      printf("\nYou must specify rx or tx: test_link_speed [rx/tx] [delay ms]\n");
      return -1;
   }
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   char* szType = argv[1];
   bool bIsTx = false;

   if ( 0 == strcmp(szType, "tx") )
      bIsTx = true;
   int delay = 500;
   if ( argc > 2 )
      delay = atoi(argv[2]);

   log_init("TEST_LINK_SPEED");
   //log_enable_stdout();
   hardware_enumerate_radio_interfaces();

   radio_init_link_structures();
   radio_enable_crc_gen(1);

   int pcapI = 0;
   if ( bIsTx )
      pcapI = init_wlan_interface_for_read("wlan0", DOWNLINK);
   else
      pcapI = init_wlan_interface_for_read("wlan0", UPLINK);
   if ( 0 > pcapI )
   {
      printf("\nFailed to open port.\n");
      return -1;
   }

   int sock = open_wlan_socket_for_write("wlan0");

   log_line("Opened wlan port\n");

   u32 uPingId = 0;
   u32 uLastPingSentTime = 0;

   if ( bIsTx )
      log_line("\nRunning in Tx mode\n");
   else
      log_line("\nRunning in Rx mode\n");

   while (!quit)
   {
      //fflush(stdout);
      g_TimeNow = get_current_timestamp_ms();

      if ( bIsTx && (g_TimeNow > uLastPingSentTime + delay) )
      {
         uLastPingSentTime = g_TimeNow;
         uPingId++;
         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_RUBY;
         PH.packet_type =  PACKET_TYPE_RUBY_PING_CLOCK;
         PH.vehicle_id_src = 0;
         PH.vehicle_id_dest = 0;
         PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8);
         PH.tx_time = 0;
         int nLinkId = STREAM_ID_DATA;
         PH.packet_index = (((u32)nLinkId)<<24) | uPingId;
         u8 uLink = 0;
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memset(packet, 0, MAX_PACKET_TOTAL_SIZE);
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uPingId, sizeof(u8));
         radio_set_out_datarate(DEFAULT_RADIO_DATARATE, 0, get_current_timestamp_ms());
         static u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
         int totalLength = radio_build_packet(rawPacket, packet, PH.total_length, UPLINK, 0);
         write_packet_to_radio(sock, rawPacket, totalLength);
         log_line("Sent Ping, counter: %d", uPingId); 
      }

      long miliSec = 2;
      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = miliSec*1000;
         
      int maxfd = -1;
      FD_ZERO(&readset);
      for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( pNICInfo->openedForRead )
         {
            FD_SET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &readset);
            if ( pNICInfo->runtimeInterfaceInfoRx.selectable_fd > maxfd )
               maxfd = pNICInfo->runtimeInterfaceInfoRx.selectable_fd;
         } 
      }
      int n = select(maxfd+1, &readset, NULL, NULL, &to);
      if(n == 0)
      {
         //printf(".");
         continue;
      }
      if ( n < 0)
      {
         //printf("*");
         continue;
      }

      for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if(FD_ISSET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &readset))
         {
            //fflush(stdout);
            int length = 0;
            u8* pBuffer = NULL;
            int bCRCOk = 0;
            int okPacket = 0;
            pBuffer = radio_process_wlan_data_in(i, &length, NULL, g_TimeNow); 
            if ( NULL == pBuffer )
            {
               printf("NULL receive buffer. Ignoring...");
               continue;
            }
            int lastDBM = pNICInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDbm;
            int lastDataRate = pNICInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;

            t_packet_header* pPH = (t_packet_header*)pBuffer; 
            
            //okPacket = packet_process_and_check(i, pBuffer, length, NULL, &bCRCOk);

            if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
            {
              u32 pingId = 0;
              u32 vehicleTime = 0;
              memcpy(&pingId, pBuffer + sizeof(t_packet_header), sizeof(u32));
              memcpy(&vehicleTime, pBuffer + sizeof(t_packet_header)+sizeof(u32), sizeof(u32));
              if ( pingId == uPingId )
              {
                 u32 totalTime = g_TimeNow - uLastPingSentTime;
                 log_line("Received PING response (counter: %u), PING roundtrip: %d ms", pingId, totalTime);
              }
            }

            if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
            {
               u32 pingId = 0;
               u32 timeNow = get_current_timestamp_ms();
               memcpy( &pingId, pBuffer + sizeof(t_packet_header), sizeof(u32));
               //log_line("Received PING, counter: %d", pingId);
               t_packet_header PH;
               PH.packet_flags = PACKET_COMPONENT_RUBY;
               PH.packet_type =  PACKET_TYPE_RUBY_PING_CLOCK_REPLY;
               PH.vehicle_id_src = 0;
               PH.vehicle_id_dest = 0;
               PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u32);
               u8 packet[MAX_PACKET_TOTAL_SIZE];
               memset(packet, 0, MAX_PACKET_TOTAL_SIZE);
               memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
               memcpy(packet+sizeof(t_packet_header), &pingId, sizeof(u32));
               memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &timeNow, sizeof(u32));

               radio_set_out_datarate(DEFAULT_RADIO_DATARATE, 0, get_current_timestamp_ms());
               u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
               int totalLength = radio_build_packet(rawPacket, packet, PH.total_length, DOWNLINK, 0);
               write_packet_to_radio(sock, rawPacket, totalLength);
               //log_line("Sent Ping response, counter: %d", pingId); 
            }
         }
      }
   }
   close(sock);
   close_wlan_interface_for_read(pcapI);
   log_line("\nFinised test.\n");
   return (0);
}
