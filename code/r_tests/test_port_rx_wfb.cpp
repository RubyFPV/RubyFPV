#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/zfec.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/encr.h"
#include "../base/enc.h"
#include "../base/base.h"
#include "../base/utils.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"

#include <time.h>
#include <sys/resource.h>
#include <sodium.h>
#include <endian.h>

bool quit = false;

bool bSummary = false;
bool bOnlyErrors = false;
   
fd_set g_Readset;

u32 g_uStreamsLastPacketIndex[MAX_RADIO_STREAMS];
u32 g_uStreamsTotalLostPackets[MAX_RADIO_STREAMS];
u32 g_uLastVideoBlock = 0;
u32 g_uTotalLostPackets = 0;
u32 g_uTotalRecvPackets = 0;
u32 g_uTotalRecvPacketsTypes[256];
u32 g_uTotalRecvPacketsOnStreams[256];
u32 uSummaryLastUpdateTime = 0;
int g_iOnlyStreamId = -1;

shared_mem_radio_stats g_SM_RadioStats;

uint8_t rx_secretkey[crypto_box_SECRETKEYBYTES];
uint8_t tx_publickey[crypto_box_PUBLICKEYBYTES];
uint8_t session_key[crypto_aead_chacha20poly1305_KEYBYTES];
fec_t* pFEC = NULL; 

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
} 

void process_packet(int iInterfaceIndex)
{
   //printf("x");
   fflush(stdout);
         
   int nLength = 0;
   u8* pBuffer = radio_process_wlan_data_in(iInterfaceIndex, &nLength, NULL, g_TimeNow); 
   if ( NULL == pBuffer )
   {
      printf("NULL receive buffer. Ignoring...\n");
      fflush(stdout);
      return;
   }

   if ( pBuffer[0] == PACKET_TYPE_WFB_KEY )
   {
      type_wfb_session_data sessionData;

      printf("\nFound key packet, %d bytes\n", nLength);
      if ( nLength != sizeof(type_wfb_session_header) + sizeof(type_wfb_session_data) + crypto_box_MACBYTES )
      {
         printf("Invalid key packet size!\n");
         return;
      }

      if(crypto_box_open_easy((uint8_t*)&sessionData,
                                pBuffer + sizeof(type_wfb_session_header),
                                sizeof(type_wfb_session_data) + crypto_box_MACBYTES,
                                ((type_wfb_session_header*)pBuffer)->sessionNonce,
                                tx_publickey, rx_secretkey) != 0)
      {
          printf("Unable to decrypt session key!\n");
          return;
      } 
      printf("EC scheme: %d/%d\n", sessionData.uK, sessionData.uN);

      if ( 0 != memcmp(session_key, sessionData.sessionKey, sizeof(session_key)) )
      {
         memcpy(session_key, sessionData.sessionKey, sizeof(session_key));

         if ( NULL != pFEC )
            zfec_free(pFEC);

         pFEC = zfec_new(sessionData.uK, sessionData.uN); 
         printf("Created new EC, %d/%d", sessionData.uK, sessionData.uN);
      } 
   }
   else
   {
      uint8_t decrypted[2000];
      long long unsigned int decrypted_len;
      type_wfb_block_header *block_hdr = (type_wfb_block_header*)pBuffer;

      if (crypto_aead_chacha20poly1305_decrypt(decrypted, &decrypted_len,
                                             NULL,
                                             pBuffer + sizeof(type_wfb_block_header), nLength - sizeof(type_wfb_block_header),
                                             pBuffer,
                                             sizeof(type_wfb_block_header),
                                             (uint8_t*)(&(block_hdr->uDataNonce)), session_key) != 0)
    {
        printf("Can't decode regular data block!\n");
        return;
    }

    u32 u = be64toh(block_hdr->uDataNonce);
 
    printf("bytes: %d, block %u, fragment %u\n", nLength, u>>8, u & 0xFF);
   }
}

int try_read_packets(int iInterfaceIndex)
{
   long miliSec = 500;
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = miliSec*1000;
            
   int maxfd = -1;
   FD_ZERO(&g_Readset);
   for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
   {
      if ( i != iInterfaceIndex )
         continue;
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( pNICInfo->openedForRead )
      {
         FD_SET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &g_Readset);
         if ( pNICInfo->runtimeInterfaceInfoRx.selectable_fd > maxfd )
            maxfd = pNICInfo->runtimeInterfaceInfoRx.selectable_fd;
      } 
   }

   int nResult = select(maxfd+1, &g_Readset, NULL, NULL, &to);
   return nResult;
}

int main(int argc, char *argv[])
{
   if ( argc < 4 )
   {
      printf("\nYou must specify network, frequency and a channel id:  test_port_rx_wfb wlan0 5745 linkid portid [-full]\n");
      printf("\n-full : shows everything received\n");
      printf(" default link id is  7669206\n");
      printf(" default port id is 0\n");
      return -1;
   }
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   char* szCard = argv[1];

   int iFreq = atoi(argv[2]);
   int iLinkId = 7669206;
   int iPortId = 0;

   if ( argc > 2 )
   if ( atoi(argv[3]) > 0 )
      iLinkId = atoi(argv[3]);
   
   if ( argc > 3 )
   if ( atoi(argv[4]) > 0 )
      iPortId = atoi(argv[4]);

   
   log_init("RX_TEST_PORT_WFB");
   log_enable_stdout();
   hardware_enumerate_radio_interfaces();
   int iInterfaceIndex = -1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); ++i)
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( 0 == strcmp(szCard, pNICInfo->szName ) )
      {
         //radio_utils_set_interface_frequency(i, freq, NULL, 0);
       iInterfaceIndex = i;
       break;
      }
   }
   hardware_sleep_ms(100);

   radio_init_link_structures();
   radio_enable_crc_gen(1);


   if (sodium_init() < 0)
   {
      printf("\nCan't init sodium.\n\n");
      return 0;
   }

   if ( load_openipc_keys(FILE_DEFAULT_OPENIPC_KEYS) != 1 )
   {
      printf("\nCan't load openIpc key.\n\n");
      return 0;
   }

   memcpy( rx_secretkey, get_openipc_key1(), crypto_box_SECRETKEYBYTES);
   memcpy( tx_publickey, get_openipc_key2(), crypto_box_SECRETKEYBYTES);
   memset(session_key, 0, sizeof(session_key)); 

   printf("\nLoaded keys.\n");
   if ( -1 == iInterfaceIndex )
   {
      printf("\nCan't find interface %s\n", szCard);
      return 0;
   }

   if ( iFreq < 10000 )
      iFreq *= 1000;

   radio_utils_set_interface_frequency(NULL, iInterfaceIndex, -1, iFreq, NULL, 0);

   printf("\nStart receiving on lan: %s (interface %d), %d Mhz, link id: %d, port id: %d\n", szCard, iInterfaceIndex+1, iFreq, iLinkId, iPortId);

   int pcapR = -1;
   if ( pcapR < 0 )
   {
      printf("\nFailed to open port.\n");
      return -1;
   }

   
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      g_uStreamsLastPacketIndex[i] = MAX_U32;
      g_uStreamsTotalLostPackets[i] = 0;
   }

   g_uTotalRecvPackets = 0;
   for( int i=0; i<256; i++ )
   {
      g_uTotalRecvPacketsTypes[i] = 0;
      g_uTotalRecvPacketsOnStreams[i] = 0;
   }

   radio_stats_reset(&g_SM_RadioStats, 100);

   g_SM_RadioStats.radio_interfaces[0].assignedLocalRadioLinkId = 0;
   g_SM_RadioStats.countLocalRadioInterfaces = 1;
   g_SM_RadioStats.countLocalRadioLinks = 1;

   printf("\nStart receiving on lan: %s (interface %d), %d Mhz, link id: %d, port id: %d\n", szCard, iInterfaceIndex+1, iFreq, iLinkId, iPortId);
   printf("\nLooking for any stream\n");
  
   printf("\n\n\nStarted.\nWaiting for data...\n\n\n");
   fflush(stdout);
   
   while (!quit)
   {
      g_TimeNow = get_current_timestamp_ms();
      radio_stats_periodic_update(&g_SM_RadioStats, g_TimeNow);

      int nResult = try_read_packets(iInterfaceIndex);

      if( nResult > 0 )
         process_packet(iInterfaceIndex);
   }
   //fclose(fd);
   radio_close_interface_for_read(iInterfaceIndex);
   printf("\nFinised receiving test.\n");
   return (0);
}
