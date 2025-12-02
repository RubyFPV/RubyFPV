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

#define MAX_BLOCKS_BUFFER 128

t_packet_header_video_full_77 s_BufferVideoPackets[MAX_BLOCKS_BUFFER][MAX_TOTAL_PACKETS_IN_BLOCK];
u32 s_BufferBlockIndexes[MAX_BLOCKS_BUFFER];

int s_iCurrentEncodingRecvPackets = 0;
int s_iCurrentEncodingScheme = 0;
u32 s_uCurrentEncodingExtraInfo = 0;
int s_iCurrentEncodingBlockPackets = 0;
int s_iCurrentEncodingBlockFECs = 0;
int s_iCurrentEncodingBlockRetrans = 0;

void check_encoding_change(t_packet_header_video_full_77* pdpv)
{
   bool schemeChanged = false;
   if ( s_iCurrentEncodingScheme != pdpv->encoding_scheme )
      schemeChanged = true;
   if ( s_uCurrentEncodingExtraInfo != pdpv->uProfileEncodingFlags )
      schemeChanged = true;
   if ( s_iCurrentEncodingBlockPackets != pdpv->iBlockPackets )
      schemeChanged = true;
   if ( s_iCurrentEncodingBlockFECs != pdpv->iBlockECs )
      schemeChanged = true;

   if ( ! schemeChanged )
      return;

   s_iCurrentEncodingRecvPackets = 0;

   s_iCurrentEncodingScheme = pdpv->encoding_scheme;
   s_uCurrentEncodingExtraInfo = pdpv->uProfileEncodingFlags;
   s_iCurrentEncodingBlockPackets = pdpv->iBlockPackets;
   s_iCurrentEncodingBlockFECs = pdpv->iBlockECs;

   for( int i=0; i<MAX_BLOCKS_BUFFER; i++ )
   {
      s_BufferBlockIndexes[i] = MAX_U32;
      for( int j=0; j<MAX_TOTAL_PACKETS_IN_BLOCK; j++ )
          memset(&(s_BufferVideoPackets[i][j]), 0, sizeof(t_packet_header_video_full_77));
   }
   log_line("---Encoding scheme changed to:");
   log_line("    Blocks: packets/fecs/retr: %d/%d/%d", s_iCurrentEncodingBlockPackets, s_iCurrentEncodingBlockFECs, s_iCurrentEncodingBlockRetrans);
   log_line("    Scheme: %d, extra flags: 0x%08X", s_iCurrentEncodingScheme, s_uCurrentEncodingExtraInfo);
}

void parse_video_packet(u8* pBuffer, int payloadLength)
{
   t_packet_header* pdph = NULL;
   t_packet_header_video_full_77* pdpv = NULL;
   pdph = (t_packet_header*)pBuffer;
   pdpv = (t_packet_header_video_full_77*)(pBuffer + sizeof(t_packet_header));

   printf(" Recv block %d, packet %d\n", pdpv->video_block_index, pdpv->video_block_packet_index);
   
   check_encoding_change(pdpv);

   s_iCurrentEncodingRecvPackets++;

   // Find the block in buffer
   int index = 0;
   for( ; index<MAX_BLOCKS_BUFFER; index++ )
      if ( s_BufferBlockIndexes[index] == pdpv->video_block_index )
         break;

   // Find an empty block in buffer
   if ( index == MAX_BLOCKS_BUFFER )
   {
      index = 0;
      for( ; index<MAX_BLOCKS_BUFFER; index++ )
         if ( s_BufferBlockIndexes[index] == MAX_U32 )
            break;
   }

   // No room found in buffer. Need to overwrite or push something out   
   if ( index == MAX_BLOCKS_BUFFER )
   {
      log_line("Pussing incomplete block out: block index: %d", s_BufferBlockIndexes[0]);
      index = 0;
      s_BufferBlockIndexes[index] = MAX_U32;
      for( int j=0; j<MAX_TOTAL_PACKETS_IN_BLOCK; j++ )
          memset(&(s_BufferVideoPackets[index][j]), 0, sizeof(t_packet_header_video_full_77));
   }

   memcpy(&(s_BufferVideoPackets[index][pdpv->video_block_packet_index]), pdpv, sizeof(t_packet_header_video_full_77));
   s_BufferBlockIndexes[index] = pdpv->video_block_index;

   // received a full block?
   int countPackets = 0;
   for( int j=0; j<MAX_TOTAL_PACKETS_IN_BLOCK; j++ )
      if ( 0 < s_BufferVideoPackets[index][j].video_width )
         countPackets++;

   if ( countPackets >= pdpv->iBlockPackets + pdpv->iBlockECs )
   {
      log_line("Out block index: %d", pdpv->video_block_index);
      s_BufferBlockIndexes[index] = MAX_U32;
      for( int j=0; j<MAX_TOTAL_PACKETS_IN_BLOCK; j++ )
          memset(&(s_BufferVideoPackets[index][j]), 0, sizeof(t_packet_header_video_full_77));
   }

}

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
} 


int main(int argc, char *argv[])
{
   if ( argc < 2 )
   {
      printf("\nYou must specify network and optionaly a frequency:  test_port_rx wlan0 [2417]\n");
      return -1;
   }
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   int freq = -1;
   if ( argc > 2 )
      freq = atoi(argv[2]);
   int port = RADIO_PORT_ROUTER_DOWNLINK;

   log_init("RX_TEST_VIDEO");
   log_enable_stdout();
   hardware_enumerate_radio_interfaces();
   
   if ( freq != -1 )
   {
      radio_utils_set_interface_frequency(-1, freq, -1, NULL, 0);
      hardware_sleep_ms(100);
   }

   radio_init_link_structures();
   radio_enable_crc_gen(1);

   log_line("\nStart receiving on lan: %s\n", argv[1]);

   int pcapI =  init_wlan_interface_for_read(argv[1], port);
   if ( 0 > pcapI )
   {
      printf("\nFailed to open port.\n");
      return -1;
   }
   log_line("Opened wlan port\n");

   log_line("\nWaiting for data ...");

   while (!quit)
   {
      fflush(stdout);
      long miliSec = 200;
      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = miliSec*1000;
      t_packet_header* pdph = NULL;
         
      int maxfd = -1;
      FD_ZERO(&readset);
      for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( pNICInfo->openedForRead )
         {
            FD_SET(pNICInfo->monitor_interface.selectable_fd, &readset);
            if ( pNICInfo->monitor_interface.selectable_fd > maxfd )
               maxfd = pNICInfo->monitor_interface.selectable_fd;
         } 
      }
      maxfd = 20;
      int n = select(maxfd+1, &readset, NULL, NULL, &to);
      if(n == 0)
      {
         printf(".");
         continue;
      }
      if ( n < 0)
      {
         printf("*");
         continue;
      }

      for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if(FD_ISSET(pNICInfo->monitor_interface.selectable_fd, &readset))
         {
         
         int payloadLength = 0;
         u8* pBuffer = NULL;
         int totalBytes = 0;
         int bCRCOk = 0;
         int okPacket = 0;
         pBuffer = radio_process_wlan_data_in(i, &payloadLength, NULL, g_TimeNow); 
         if ( NULL == pBuffer )
         {
            printf("NULL receive buffer. Ignoring...\n");
            fflush(stdout);
            continue;
         }
         pdph = (t_packet_header*)pBuffer; 
         
         if ( payloadLength < (int)(sizeof(t_packet_header)) )
         {
            printf("Packet too small, ignoring.\n");
            fflush(stdout);
            continue;
         }
         okPacket = packet_process_and_check(i, pBuffer, payloadLength, NULL, &bCRCOk);
         if ( ! okPacket )
         {
            printf("Received back packet, ignoring.\n");
            fflush(stdout);
            continue;
         }

         if ( pdph->packet_type == PACKET_TYPE_VIDEO_DATA_FULL )
            parse_video_packet(pBuffer, payloadLength);
         }
      }
   }
   close_wlan_interface_for_read(pcapI);
   log_line("\nFinised receiving test.\n");
   return (0);
}
