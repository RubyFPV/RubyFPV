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
bool g_bIsVehicle = false;
u32  g_uBitrate = 1000;
int g_iUsePCAPForTx = 1;
int g_iInterfaceRead = -1;
int g_iInterfaceWrite = -1;
int g_iPortRx = 0;
int g_iPortTx = 0;
u32 g_uComponentID = 0;
int g_iPacketsInterval = 10;
int g_iPacketsCount = 3;
int g_iPacketSize = 100;
int g_iDatarate = 18;

fd_set g_Readset;

u32 g_uTimeLastPingSent = 0;
u8  g_uLastPingIdSent = 0;
u8  g_uLastPingIdReceivedBack = 0;
u32 g_uMinimRoundTripTime = 1000;
int g_iRemoteClockIsBehindMilisecs = 0;

u32 g_uTimeLastBPSSentCalculation = 0;
u32 g_uTimeLastBPSRecvCalculation = 0;
u32 g_uTotalBPSSentLastSecond = 0;
u32 g_uTotalBPSRecvLastSecond = 0;
u32 g_uTotalPacketsRecvLastSecond = 0;
u32 g_uTotalPacketsSentLastSecond = 0;

u32 g_uBPSRecv = 0;
u32 g_uPacketsRecv = 0;

u32 g_uTimeLastSentData = 0;
u32 g_uDataPacketId = 0;

int iTransmitTimeAvg = 0;
int iTransmitTimeMin = 0;
int iTransmitTimeMax = 0;

void _send_data()
{
   if ( g_TimeNow < g_uTimeLastSentData + g_iPacketsInterval )
      return;

   g_uTimeLastSentData = g_TimeNow;

   for( int p=0; p<g_iPacketsCount; p++ )
   {
   g_uDataPacketId++;
   g_uTotalPacketsSentLastSecond++;
   g_TimeNow = get_current_timestamp_ms();
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_VIDEO_DATA, STREAM_ID_VIDEO_1);
   PH.vehicle_id_src = g_uComponentID;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + g_iPacketSize*sizeof(u8);
   
   //u8 uZero = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   for( int i=0; i<MAX_PACKET_TOTAL_SIZE; i++ )
      packet[i] = rand() % 256;
   
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_TimeNow, sizeof(u32));

   u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
   int totalLength = radio_build_new_raw_ieee_packet(0, rawPacket, packet, PH.total_length, g_iPortTx, 0);
   if ( 0 == radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
      log_line("Failed to write video packet to radio interface.\n");
   g_uTotalBPSSentLastSecond += totalLength;

   if ( g_TimeNow >= g_uTimeLastBPSSentCalculation + 1000 )
   {
      g_uTimeLastBPSSentCalculation = g_TimeNow;
      log_line("Sent %u bps, %d packets/sec", g_uTotalBPSSentLastSecond*8, g_uTotalPacketsSentLastSecond);

      g_uTotalBPSSentLastSecond = 0;
      g_uTotalPacketsSentLastSecond = 0;
   }
  }
}

void _send_ping()
{
   if ( g_TimeNow < g_uTimeLastPingSent + 2000 )
      return;

   g_uTimeLastPingSent = g_TimeNow;
   
   g_uLastPingIdSent++;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PING_CLOCK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uComponentID;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8);
   
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8 uZero = 0;
   // u8 ping id, u8 radio link id, u8 relay flags for destination vehicle
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_uLastPingIdSent, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u8), &uZero, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u8), &uZero, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u8), &uZero, sizeof(u8));
 
   u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
   int totalLength = radio_build_new_raw_ieee_packet(0, rawPacket, packet, PH.total_length, g_iPortTx, 0);

   if ( 0 == radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
      log_line("Failed to write PING to radio interface.\n");
   //else
   //   log_line("Sent PING");
}

void _process_rx_packet(u8* pPacketBuffer, int nLength, int iCount)
{
   if ( (NULL == pPacketBuffer) || (nLength < 10) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   //u32 uStreamId = pPH->stream_packet_idx >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   //log_line("Received packet from CID %u, stream %u, %d bytes, type: %s",
   //   uStreamId, pPH->vehicle_id_src, pPH->total_length, str_get_packet_type(pPH->packet_type));

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   if ( pPH->total_length >= sizeof(t_packet_header) + 4*sizeof(u8) )
   {
      u8 uPingId = 0;
      memcpy( &uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PING_CLOCK_REPLY, STREAM_ID_DATA);
      PH.vehicle_id_src = g_uComponentID;
      PH.vehicle_id_dest = 0;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32);
      
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      u8 uZero = 0;
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uPingId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8), &g_TimeNow, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8)+sizeof(u32), &uZero, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+2*sizeof(u8)+sizeof(u32), &uZero, sizeof(u8));

      u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
      int totalLength = radio_build_new_raw_ieee_packet(0, rawPacket, packet, PH.total_length, g_iPortTx, 0);

      if ( 0 == radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
         log_line("Failed to write PING_REPLY to radio interface.\n");
      //else
      //   log_line("Sent PING_REPLY");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   if ( pPH->total_length >= sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) )
   {
      u8 uPingId = 0;
      u32 uRemoteLocalTimeMs = 0;
      
      memcpy(&uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy(&uRemoteLocalTimeMs, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u32));
      
      log_line("Received PING_REPLY: remote-time: %u, local-time: %u", uRemoteLocalTimeMs, g_TimeNow);
       
      if ( uPingId == g_uLastPingIdSent )
      if ( uPingId != g_uLastPingIdReceivedBack )
      {
         u32 uRoundtripMs = g_TimeNow - g_uTimeLastPingSent;
         g_uLastPingIdReceivedBack = uPingId;

         if ( uRoundtripMs < g_uMinimRoundTripTime )
         {
            g_uMinimRoundTripTime = uRoundtripMs;
            g_iRemoteClockIsBehindMilisecs = (int)g_TimeNow - (int)uRemoteLocalTimeMs;
            if ( g_iRemoteClockIsBehindMilisecs > 0 )
               g_iRemoteClockIsBehindMilisecs -= (int)g_uMinimRoundTripTime/2;
            else
               g_iRemoteClockIsBehindMilisecs += (int)g_uMinimRoundTripTime/2;
         }
         log_line("Link RT time: %u ms, minim: %u ms, remote clock is behind %d ms",
            uRoundtripMs, g_uMinimRoundTripTime, g_iRemoteClockIsBehindMilisecs);
      }
      return ;
   }

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
   {
      u32 uRemoteLocalTimeMs = 0;
      memcpy(&uRemoteLocalTimeMs, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      
      u32 uRemoteTimeNow = uRemoteLocalTimeMs + g_iRemoteClockIsBehindMilisecs;
      int iTransmitTime = (int)g_TimeNow - (int)uRemoteTimeNow;

      if ( iTransmitTimeAvg == 0 )
      if ( iTransmitTimeMin == 0 )
      if ( iTransmitTimeMax == 0 )
      {
         iTransmitTimeAvg = iTransmitTimeMax = iTransmitTimeMin = iTransmitTime;
      }
      if ( iTransmitTime > iTransmitTimeMax )
         iTransmitTimeMax = iTransmitTime;
      if ( iTransmitTime < iTransmitTimeMin )
         iTransmitTimeMin = iTransmitTime;

      if ( iTransmitTime > 10 )
         log_line("Recv%d: r-time: %u l-time: %u, trans time (now,avg): %d, %d, dclk: %d ms (%u bps, %u pkts)",
           iCount, uRemoteLocalTimeMs, g_TimeNow,
          iTransmitTime, iTransmitTimeAvg,
           g_iRemoteClockIsBehindMilisecs,
           g_uBPSRecv*8, g_uPacketsRecv);

      iTransmitTimeAvg = (iTransmitTimeAvg*99 + iTransmitTime)/100;
      iTransmitTimeMax--;
      g_uTotalBPSRecvLastSecond += pPH->total_length;
      g_uTotalPacketsRecvLastSecond++;
      if ( g_TimeNow >= g_uTimeLastBPSRecvCalculation + 1000 )
      {
         g_uTimeLastBPSRecvCalculation = g_TimeNow;
         log_line("Recv %u bps, %d packets/sec", g_uTotalBPSRecvLastSecond*8, g_uTotalPacketsRecvLastSecond);

         g_uBPSRecv = g_uTotalBPSRecvLastSecond;
         g_uPacketsRecv = g_uTotalPacketsRecvLastSecond;

         g_uTotalBPSRecvLastSecond = 0;
         g_uTotalPacketsRecvLastSecond = 0;
      }
   }

}

void _try_receive_packets()
{
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 100;
            
   
   int maxfd = -1;
   FD_ZERO(&g_Readset);
   for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( pNICInfo->openedForRead )
      {
         FD_SET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &g_Readset);
         if ( pNICInfo->runtimeInterfaceInfoRx.selectable_fd > maxfd )
            maxfd = pNICInfo->runtimeInterfaceInfoRx.selectable_fd;
      } 
   }

   int nResult = select(maxfd+1, &g_Readset, NULL, NULL, &to);
   
   /*
    struct pollfd fds[5];
    memset(fds, '\0', sizeof(fds)); 
    radio_hw_info_t* pNICInfo = hardware_get_radio_info(0);
    fds[0].fd = pNICInfo->runtimeInterfaceInfoRx.selectable_fd;
    fds[0].events = POLLIN; 
    
   int nResult = poll(fds, 1, 1); 
   if ( ! (fds[0].revents & POLLIN) )
      return;

   */

   if ( nResult <= 0 )
   {
      return;
   }
   int iCount = 0;
   int nLength = 0;
   u8* pBuffer = NULL;
   do
   {
      nLength = 0;
      pBuffer = radio_process_wlan_data_in(0, &nLength, NULL, g_TimeNow); 
      if ( NULL == pBuffer )
         break;

      iCount++;
      g_TimeNow = get_current_timestamp_ms();

      while ( nLength > 0 )
      { 
         t_packet_header* pPH = (t_packet_header*)pBuffer;
         _process_rx_packet(pBuffer, nLength, iCount);
         pBuffer += pPH->total_length;
         nLength -= pPH->total_length;
      }
   }
   while (true);
}

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   g_bQuit = true;
} 
  

int main(int argc, char *argv[])
{
   if ( argc < 7 )
   {
      printf("\ntest_link [-veh/-con] [-soc/-pcap] datarate send_interval send_count packet_size [-txport nb] [-rxport nb]\n");
      return -1;
   }

   g_bIsVehicle = true;
   g_uComponentID = 1;
   if ( strcmp(argv[1], "-con") == 0 )
   {
      g_bIsVehicle = false;
      g_uComponentID = 2;
   }
   g_iUsePCAPForTx = 1;
   if ( strcmp(argv[2], "-soc") == 0 )
      g_iUsePCAPForTx = 0;

   //g_uBitrate = (u32)atoi(argv[3]);
   g_iDatarate = (int)atoi(argv[3]);
   g_iPacketsInterval = atoi(argv[4]);
   g_iPacketsCount = atoi(argv[5]);
   g_iPacketSize = atoi(argv[6]);

   if ( g_bIsVehicle )
   {
      g_iPortRx = RADIO_PORT_ROUTER_UPLINK;
      g_iPortTx = RADIO_PORT_ROUTER_DOWNLINK;
   }
   else
   {
      g_iPortRx = RADIO_PORT_ROUTER_DOWNLINK;
      g_iPortTx = RADIO_PORT_ROUTER_UPLINK;
   }

   if ( strcmp(argv[argc-2], "-txport") == 0 )
      g_iPortTx = atoi(argv[argc-1]);
   if ( strcmp(argv[argc-4], "-txport") == 0 )
      g_iPortTx = atoi(argv[argc-3]);

   if ( strcmp(argv[argc-2], "-rxport") == 0 )
      g_iPortRx = atoi(argv[argc-1]);
   if ( strcmp(argv[argc-4], "-rxport") == 0 )
      g_iPortRx = atoi(argv[argc-3]);

   //setpriority(PRIO_PROCESS, 0, -10);
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestLink"); 
   log_enable_stdout();

   radio_init_link_structures();
   radio_enable_crc_gen(1);

   radio_set_use_pcap_for_tx(g_iUsePCAPForTx);

   g_iInterfaceWrite = radio_open_interface_for_write(0);
   if ( g_iInterfaceWrite <= 0 )
   {
      printf("\nFailed to open interface for write.\n");
      return -1;
   }

   g_iInterfaceRead = radio_open_interface_for_read(0, g_iPortRx);
   if ( g_iInterfaceRead < 0 )
   {
      printf("\nFailed to open interface for read.\n");
      return -1;
   }
   
   log_line("Started as %s, use %s, datarate: %d, component id: %u",
       g_bIsVehicle?"vehicle":"controller",
       g_iUsePCAPForTx?"ppcap":"sockets",
        g_iDatarate, g_uComponentID);

   log_line("Started on ports: tx port: %d, rx port: %d", g_iPortTx, g_iPortRx);
   log_line("Send %d packets every %d ms, %d bytes/packet, for a total of %d bps",
      g_iPacketsCount, g_iPacketsInterval, g_iPacketSize,
      8000 * (g_iPacketsCount * g_iPacketSize / g_iPacketsInterval));

   radio_set_out_datarate(6, 0, get_current_timestamp_ms());
   radio_set_out_datarate(12, 0, get_current_timestamp_ms());
   if ( g_iDatarate > 0 )
      radio_set_out_datarate(g_iDatarate*1000*1000, 0, get_current_timestamp_ms());
   else
      radio_set_out_datarate(g_iDatarate, 0, get_current_timestamp_ms());

   radio_set_frames_flags(RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA, get_current_timestamp_ms());
    
   while ( ! g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();
      _send_ping();
      if( g_bIsVehicle )
         _send_data();
      _try_receive_packets();
   }

   hardware_sleep_ms(50);   
   radio_close_interface_for_read(0);
   radio_close_interface_for_write(0);
   return (0);
}
