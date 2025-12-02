/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include "../base/base.h"
#include "../base/encr.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_serial.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"
#include "radiotap.h"
#include <time.h>
#include <sys/resource.h>
#include "radiolink.h"
#include "radiopackets2.h"
#include "radio_rx.h"

//#define DEBUG_PACKET_RECEIVED
//#define DEBUG_PACKET_SENT

int s_bRadioDebugFlag = 0;
int s_iUsePCAPForTx = DEFAULT_USE_PPCAP_FOR_TX;
int s_iBypassSocketBuffers = DEFAULT_BYPASS_SOCKET_BUFFERS;
int s_iRadioInterfacesBroken = 0;
int s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;
int s_iVehicleBehindMilisec = 0;

u8 sPayloadBufferRead[MAX_PACKET_LENGTH_PCAP];
int sEnableCRCGen = 0;

int sRadioDataRate_bps = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS; // positive: clasic in bps; negative MCS (starts from -1)
u32 sRadioFrameFlags = RADIO_FLAGS_FRAME_TYPE_DATA;
int s_iLastRadioDataRatesPerPackets[256];

int sAutoIncrementPacketCounter = 1;
u32 sRadioReceivedFramesType = RADIO_FLAGS_FRAME_TYPE_DATA;
u32 sRadioLastReceivedHeadersLength = 0;

int s_iLastProcessingErrorCode = 0;

int s_iLogCount_RadioRate = 0;
int s_iLogCount_RadioFlags = 0;
u32 s_uLastTimeLog_RadioRate = 0;
u32 s_uLastTimeLog_RadioFlags = 0;
u32 s_uPacketsSentUsingCurrent_RadioRate = 0;
u32 s_uPacketsSentUsingCurrent_RadioFlags = 0;
u32 s_uLastPacketSentRadioTapHeaderLength = 0;
u32 s_uLastPacketSentIEEEHeaderLength = 0;

u32 s_uNextRadioPacketIndexes[MAX_RADIO_INTERFACES];

u32 s_uTimesLastRadioPacketsOnSlowLink[256][MAX_RADIO_INTERFACES];
u32 s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[256]; // in milisec, how often or none can send a short packet of this type from vehicle to controller, MAX_U32 for none
u32 s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[256]; // in milisec, how often or none can send a short packet of this type from controller to vehicle, MAX_U32 for none

#define MAX_LOW_DATALINK_PACKETS_HISTORY 100

u32 s_uTimesLastLowDataLinkPacketsOnInterfaces[MAX_RADIO_INTERFACES][MAX_LOW_DATALINK_PACKETS_HISTORY];
u32 s_uSizesLastLowDataLinkPacketsOnInterfaces[MAX_RADIO_INTERFACES][MAX_LOW_DATALINK_PACKETS_HISTORY];
u32 s_uIndexLastLowDataLinkPackets[MAX_RADIO_INTERFACES];

pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
int s_iMutexRadioSyncRxTxThreadsInitialized = 0;

u8 s_uLastPacketBuilt[MAX_PACKET_TOTAL_SIZE];
u32 s_uLastRadioPingSentTime = 0;
u8 s_uLastRadioPingId = 0;

u8 s_uRadiotapHeaderLegacy[] = {
	0x00, 0x00, // <-- radiotap version
	0x0c, 0x00, // <- radiotap header length (2 bytes), 12 bytes (this header)
	0x04, 0x80, 0x00, 0x00, // <-- radiotap presence flags (rate + tx flags)
	0x00, 0x08, // datarate and tx flags (data rate will be overwritten later in packet_header_init) (tx flags was 0x08)
	0x00, 0x00 // ??
};

u8 s_uRadiotapHeaderMCS[] = {
    0x00, 0x00,             // <-- radiotap version
    0x0d, 0x00,             // <- radiotap header length (2 bytes), 13 bytes (this header)
    0x00, 0x80, 0x08, 0x00, // <-- radiotap presence flags (tx flags (0x8000), mcs (0x080000)) (4 bytes)
    0x08, 0x00,             // tx-flags (2 bytes) (has IEEE80211_RADIOTAP_F_TX_NOACK = 0x0008)
    0x37,                   // byte 11: mcs have: bw, gi, stbc ,fec (known flags, 1 byte)
    0x30,                   // byte 12: mcs: 20MHz bw, long guard interval, stbc, ldpc (flags, 1 byte)
    0x00,                   // byte 13: mcs index 0 (speed level, will be overwritten later)
};
 

u8 s_uIEEEHeaderData[] = {
	0x08, 0x01, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
	0x13, 0x12, 0x34, 0x56, 0x78, 0x90, // mac
	0x13, 0x12, 0x34, 0x56, 0x78, 0x90, // mac
	0x00, 0x00 // IEEE802.11 seqnum, (will be overwritten later by Atheros firmware/wifi chip)
};
 
u8 s_uIEEEHeaderRTS[] = {
	0xb4, 0x01, 0x00, 0x00, // frame control field (2 bytes), duration (2 bytes)
	0xff, //  port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
};

u8 s_uIEEEHeaderData_short[] = {
	0x08, 0x01, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
	0xff // port =  1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
};

uint16_t uIEEEE80211SeqNb = 0; 

int _radio_encode_port(int port)
{
   //return (port * 2) + 1;
   return ((port<<4) | 0x0F);
}

void radio_init_link_structures()
{
   log_line("[Radio] Initialize.");

   radio_packets_log_sizes();
   radio_packets_short_init();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_uNextRadioPacketIndexes[i] = 0;

   radio_reset_packets_default_frequencies(0);

   for( int i=0; i<256; i++ )
   {
      s_iLastRadioDataRatesPerPackets[i] = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         s_uTimesLastRadioPacketsOnSlowLink[i][k] = 0;
   }

   // Init packets history on low data links

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_uIndexLastLowDataLinkPackets[i] = 0;
      for( int k=0; k<MAX_LOW_DATALINK_PACKETS_HISTORY; k++ )
      {
         s_uTimesLastLowDataLinkPacketsOnInterfaces[i][k] = 0;
         s_uSizesLastLowDataLinkPacketsOnInterfaces[i][k] = 0;
      }
   }

#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   log_line("[Radio] Initialize and use radio rxtx threads synchronization.");
   if ( 0 == s_iMutexRadioSyncRxTxThreadsInitialized )
   {
      if ( 0 != pthread_mutex_init(&s_pMutexRadioSyncRxTxThreads, NULL) )
         log_softerror_and_alarm("[Radio] Failed to init RxTx threads sync mutex.");
      else
         s_iMutexRadioSyncRxTxThreadsInitialized = 1;
   }
#else
   log_line("[Radio] Do not use radio rxtx threads synchronization.");
#endif
}

void radio_link_cleanup()
{
   if ( s_iMutexRadioSyncRxTxThreadsInitialized )
   {
      pthread_mutex_destroy(&s_pMutexRadioSyncRxTxThreads);
      s_iMutexRadioSyncRxTxThreadsInitialized = 0;
   }
}

void radio_reset_packets_default_frequencies(int iRCEnabled)
{
   for( int i=0; i<256; i++ )
   {
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[i] = 500;
      s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[i] = 500;
   }


   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PING_CLOCK] = 100;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PING_CLOCK_REPLY] = 100;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PING_CLOCK] = 100;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PING_CLOCK_REPLY] = 100;

   // Controller to vehicle

   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PAIRING_REQUEST] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_COMMAND] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS] = 50;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RC_FULL_FRAME] = 50;

   
   // Vehicle to controller

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PAIRING_CONFIRMATION] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_COMMAND_RESPONSE] = 200;
   
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_SHORT] = 300;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_EXTENDED] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY_EXTENDED] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_RC_CHANNELS] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RC_TELEMETRY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY] = 400;
   
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_ALARM] = 100;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK] = 50;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_MODEL_SETTINGS] = 0; // Send at any rate

   if( iRCEnabled )
   {
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_SHORT] = 330;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_EXTENDED] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY] = 330;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY_EXTENDED] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_RC_CHANNELS] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RC_TELEMETRY] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY] = 400;
      
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_ALARM] = 100;
   }
}

void radio_enable_crc_gen(int enable)
{
   sEnableCRCGen = enable;
   log_line("Set radio enable crc generation to: %d", sEnableCRCGen);
}


void radio_set_debug_flag(int iDebugFlag)
{
   s_bRadioDebugFlag = iDebugFlag;
   log_line("Set radio link debug flag: %d", s_bRadioDebugFlag);
}

void radio_set_link_clock_delta(int iVehicleBehindMilisec)
{
   s_iVehicleBehindMilisec = iVehicleBehindMilisec;
}

int  radio_get_link_clock_delta()
{
   return s_iVehicleBehindMilisec;
}

void radio_set_use_pcap_for_tx(int iEnablePCAPTx)
{
   s_iUsePCAPForTx = iEnablePCAPTx;
   if ( s_iUsePCAPForTx )
      log_line("[Radio] Set using ppcap for radio tx");
   else
      log_line("[Radio] Set using sockets for radio tx");
}

void radio_set_bypass_socket_buffers(int iBypass)
{
   s_iBypassSocketBuffers = iBypass;
   if ( s_iBypassSocketBuffers )
      log_line("[Radio] Set bypass radio sockets buffers.");
   else
      log_line("[Radio] Unset bypass radio sockets buffers.");
}

// Returns 0 if the packet can't be sent (right now or ever)

int radio_can_send_packet_on_slow_link(int iLinkId, int iPacketType, int iFromController, u32 uTimeNow)
{
   if ( (iPacketType < 1) || (iPacketType > 254) )
      return 0;
   if ( (iLinkId < 0) || (iLinkId >= MAX_RADIO_INTERFACES) )
      return 0;

   if ( (iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] != MAX_U32) )
   if ( uTimeNow >= s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] + s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }

   if ( (iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] == 0) )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }
   
   if ( (! iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] != MAX_U32) )
   if ( uTimeNow >= s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] + s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }

   if ( (! iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] == 0) )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }
   return 0;
}

int radio_get_last_second_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iTotalBPS = 0;
   int iCount = 0;
   int iIndex = s_uIndexLastLowDataLinkPackets[iInterfaceIndex];
   uTimeNow -= 1000;

   while ( (s_uTimesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex] >= uTimeNow) && (iCount<MAX_LOW_DATALINK_PACKETS_HISTORY) )
   {
      iTotalBPS += 8 * s_uSizesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex];
      iCount++;
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_LOW_DATALINK_PACKETS_HISTORY-1;
   }
   return iTotalBPS;
}

int radio_get_last_500ms_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iTotalBPS = 0;
   int iCount = 0;
   int iIndex = s_uIndexLastLowDataLinkPackets[iInterfaceIndex];
   uTimeNow -= 500;

   while ( (s_uTimesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex] >= uTimeNow) && (iCount<MAX_LOW_DATALINK_PACKETS_HISTORY) )
   {
      iTotalBPS += 8 * s_uSizesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex];
      iCount++;
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_LOW_DATALINK_PACKETS_HISTORY-1;
   }
   return iTotalBPS;
}

int radio_set_out_datarate(int rate_bps, u8 uPacketType, u32 uTimeNow)
{
   int nReturn = 0;

   if ( rate_bps == 0 )
      rate_bps = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;

   if ( s_iLastRadioDataRatesPerPackets[uPacketType] != rate_bps )
   {
      s_iLastRadioDataRatesPerPackets[uPacketType] = rate_bps;
      //char szBuff[64];
      //str_getDataRateDescription(rate_bps, 0, szBuff);
      //log_line("[Radio] Radio Tx DR has changed to %s for %s", szBuff, str_get_packet_type(uPacketType));
   }

   if ( sRadioDataRate_bps != rate_bps )
   {
      if ( uTimeNow > s_uLastTimeLog_RadioRate + 2000 )
         s_iLogCount_RadioRate = 0;
      if ( s_iLogCount_RadioRate < 6 )
      {
         s_uLastTimeLog_RadioRate = get_current_timestamp_ms();
         char szBuff[64];
         char szBuff2[64];
         str_getDataRateDescription(rate_bps, 0, szBuff);
         str_getDataRateDescription(sRadioDataRate_bps, 0, szBuff2);
         log_line("[Radio] Set Tx DR (%d) to: %s (to send %s) (prev was: %s, %u pckts sent on it), current radio frame flags: %s",
           s_iLogCount_RadioRate, szBuff, str_get_packet_type(uPacketType), szBuff2, s_uPacketsSentUsingCurrent_RadioRate, str_get_radio_frame_flags_description2(sRadioFrameFlags));
         if ( s_iLogCount_RadioRate == 5 )
            log_line("[Radio] Too many radio datarate changes, pausing log.");
         s_iLogCount_RadioRate++;
      }
      s_uPacketsSentUsingCurrent_RadioRate = 0;
      nReturn = 1;
   }
   else if ( uTimeNow > s_uLastTimeLog_RadioRate + 5000 )
   {
      char szBuff[64];
      str_getDataRateDescription(sRadioDataRate_bps, 0, szBuff);
      log_line("[Radio] Tx DR is unchanged: %s", szBuff);

      s_uLastTimeLog_RadioRate = uTimeNow;
   }

   sRadioDataRate_bps = rate_bps;

   if ( sRadioDataRate_bps > 0 )
   {
      sRadioFrameFlags &= ~RADIO_FLAGS_USE_MCS_DATARATES;
      sRadioFrameFlags |= RADIO_FLAGS_USE_LEGACY_DATARATES;
      if ( sRadioDataRate_bps > 56 )
      {
         s_uRadiotapHeaderLegacy[8] = (uint8_t)((int)(sRadioDataRate_bps/1000/1000) * 2);
      }
      else
      {
         s_uRadiotapHeaderLegacy[8] = (uint8_t)(sRadioDataRate_bps*2);
         if ( 5 == sRadioDataRate_bps )
            s_uRadiotapHeaderLegacy[8] = (uint8_t)11;
      }
   }
   else
   {
      sRadioFrameFlags |= RADIO_FLAGS_USE_MCS_DATARATES;
      sRadioFrameFlags &= ~RADIO_FLAGS_USE_LEGACY_DATARATES;
      int mcsRate = -sRadioDataRate_bps-1;
      if ( mcsRate < 0 )
         mcsRate = 0;
      s_uRadiotapHeaderMCS[12] = (uint8_t)mcsRate;
   }

   return nReturn;
}

u32 radio_get_current_frames_flags()
{
   return sRadioFrameFlags;
}

u32 radio_get_current_frames_flags_datarate()
{
   if ( sRadioFrameFlags & RADIO_FLAGS_USE_LEGACY_DATARATES )
      return RADIO_FLAGS_USE_LEGACY_DATARATES;
   if ( sRadioFrameFlags & RADIO_FLAGS_USE_MCS_DATARATES )
      return RADIO_FLAGS_USE_MCS_DATARATES;
   return 0;
}

void radio_set_frames_flags(u32 frameFlags, u32 uTimeNow)
{
   u32 uFrameFlagsToSet = frameFlags | RADIO_FLAGS_FRAME_TYPE_DATA;
   
   if ( sRadioDataRate_bps > 0 )
   {
      uFrameFlagsToSet &= ~RADIO_FLAGS_USE_MCS_DATARATES;
      uFrameFlagsToSet |= RADIO_FLAGS_USE_LEGACY_DATARATES;
   }
   else
   {
      uFrameFlagsToSet |= RADIO_FLAGS_USE_MCS_DATARATES;
      uFrameFlagsToSet &= ~RADIO_FLAGS_USE_LEGACY_DATARATES;
   }

   if ( uFrameFlagsToSet & RADIO_FLAGS_USE_MCS_DATARATES )
   {
      u8 mcs_flags = 0;
      u8 mcs_known = (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW | IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC);
      mcs_known |= IEEE80211_RADIOTAP_MCS_HAVE_FEC;
      int mcsRate = -sRadioDataRate_bps-1;
      if ( mcsRate < 0 )
         mcsRate = 0;

      if ( hardware_is_station() )
      {
         if ( uFrameFlagsToSet & RADIO_FLAG_HT40_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_40;
         else
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_20;

         if ( uFrameFlagsToSet & RADIO_FLAG_LDPC_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_FEC_LDPC; 
         if ( uFrameFlagsToSet & RADIO_FLAG_SGI_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_SGI;
         if ( uFrameFlagsToSet & RADIO_FLAG_STBC_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_STBC_1 << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
      }
      else
      {
         if ( uFrameFlagsToSet & RADIO_FLAG_HT40_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_40;
         else
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_20;

         if ( uFrameFlagsToSet & RADIO_FLAG_LDPC_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_FEC_LDPC; 
         if ( uFrameFlagsToSet & RADIO_FLAG_SGI_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_SGI;
         if ( uFrameFlagsToSet & RADIO_FLAG_STBC_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_STBC_1 << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;       
      }
      s_uRadiotapHeaderMCS[10] = mcs_known;
      s_uRadiotapHeaderMCS[11] = mcs_flags;
      s_uRadiotapHeaderMCS[12] = (uint8_t)mcsRate;
   }

   if ( sRadioFrameFlags != uFrameFlagsToSet )
   {
      if ( uTimeNow > s_uLastTimeLog_RadioFlags + 2000 )
         s_iLogCount_RadioFlags = 0;
      if ( s_iLogCount_RadioFlags < 10 )
      {
         s_uLastTimeLog_RadioFlags = uTimeNow;
   
         char szBuff[128];
         char szBuff2[128];
         str_get_radio_frame_flags_description(uFrameFlagsToSet, szBuff);
         str_get_radio_frame_flags_description(sRadioFrameFlags, szBuff2);
         log_line("[Radio] Set radio frames flags to %s = %s (previous was: %s, %u packets sent on it)",
            str_format_binary_number(uFrameFlagsToSet), szBuff, szBuff2, s_uPacketsSentUsingCurrent_RadioFlags);
         if ( s_iLogCount_RadioFlags == 9 )
            log_line("[Radio] Too many radio flags changes, pausing log.");
         s_iLogCount_RadioFlags++;
      }
   }
   if ( sRadioFrameFlags != uFrameFlagsToSet )
      s_uPacketsSentUsingCurrent_RadioFlags = 0;
   sRadioFrameFlags = uFrameFlagsToSet;
}


u32 radio_get_received_frames_type()
{
   return sRadioReceivedFramesType;
}

int radio_interfaces_broken()
{
   return s_iRadioInterfacesBroken;
}

int radio_get_last_read_error_code()
{
   return s_iRadioLastReadErrorCode; 
}

int _radio_open_interface_for_read_with_filter(int interfaceIndex, char* szFilter, char* szFilterPrism)
{
   s_iRadioInterfacesBroken = 0;

   struct bpf_program bpfprogram;
   char szProgram[512];
   char szErrbuf[PCAP_ERRBUF_SIZE];

   if ( (NULL == szFilter) || (NULL == szFilterPrism) )
   {
      log_softerror_and_alarm("Invalid filters for opening radio interface for read.");
      return -1;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Invalid radio interface hardware info to open for interface index %d", interfaceIndex+1);
      return -1;
   }

   if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("Tried to open a non 2.4/5.8 radio in high capacity 2.4/5.8 mode.");
      return -1;
   }

   pRadioHWInfo->openedForRead = 0;
   pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd = -1;
   pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount = 0;

   szErrbuf[0] = '\0';
   //pRadioHWInfo->runtimeInterfaceInfoRx.ppcap = pcap_open_live(pRadioHWInfo->szName, 4096, 1, 1, szErrbuf);
   pRadioHWInfo->runtimeInterfaceInfoRx.ppcap = pcap_create(pRadioHWInfo->szName, szErrbuf);

   if (pRadioHWInfo->runtimeInterfaceInfoRx.ppcap == NULL)
   {
      log_softerror_and_alarm("Unable to open [%s]: %s", pRadioHWInfo->szName, szErrbuf);
      return -1;
   }

   log_line("pcap snapshot length: %d bytes", pcap_snapshot(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
   //int iLen = 2096;
   int iLen = MAX_PACKET_TOTAL_SIZE*6;
   //int iLen = MAX_PACKET_TOTAL_SIZE*30;
   if ( pcap_set_snaplen(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, iLen) !=0 )
      log_softerror_and_alarm("Error setting [%s] snap buffer length: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
   else
      log_line("Set pcap len to %d", iLen);
   if ( pcap_set_promisc(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, 1) != 0 )
      log_softerror_and_alarm("Error setting [%s] to promiscous mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));

   if ( pcap_set_timeout(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, -1) !=0) 
      log_softerror_and_alarm("Error setting [%s] timeout: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
   
   if ( pcap_set_immediate_mode(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, 1) != 0 )
      log_softerror_and_alarm("Error setting [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
   
   if ( pcap_activate(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap) !=0) 
      log_softerror_and_alarm("Error setting [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
    
   if ( pcap_setnonblock(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, 1, szErrbuf) < 0 )
      log_softerror_and_alarm("Error setting [%s] to nonblocking mode: %s", pRadioHWInfo->szName, szErrbuf);
        
   //if ( pcap_setdirection(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, PCAP_D_IN) < 0 )
   //   log_softerror_and_alarm("Error setting [%s] direction", pRadioHWInfo->szName);

   log_line("pcap snapshot length: %d bytes", pcap_snapshot(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));

   int nLinkEncap = pcap_datalink(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);

   if (nLinkEncap == DLT_IEEE802_11_RADIO)
      sprintf(szProgram, "%s", szFilter);
   else if (nLinkEncap == DLT_PRISM_HEADER)
      sprintf(szProgram, "%s", szFilterPrism);
   else
   {
      log_softerror_and_alarm("ERROR: unknown encapsulation on [%s]! check if monitor mode is supported and enabled", pRadioHWInfo->szName);
      return -1;
   }

   if (pcap_compile(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, &bpfprogram, szProgram, 1, 0) == -1)
   {
      puts(szProgram);
      puts(pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
      log_softerror_and_alarm("ERROR: compiling program for interface [%s]", pRadioHWInfo->szName);
      return -1;
   }
   else
   {
      if (pcap_setfilter(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, &bpfprogram) == -1)
      {
         log_softerror_and_alarm("Failed to set pcap filter: %s", szProgram);
         log_softerror_and_alarm("Failed to set pacp filter: %s", pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap));
      }
      pcap_freecode(&bpfprogram);
   }
   pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd = pcap_get_selectable_fd(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);
   reset_runtime_radio_rx_info(&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo));

   pRadioHWInfo->openedForRead = 1;

   log_line("Opened radio interface %d (%s) for reading on %s, filter: [%s]. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFilter, pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);

   return pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
}

int radio_open_interface_for_read(int interfaceIndex, int portNumber)
{
   char szFilter[256];
   char szFilterPrism[256];

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
      return -1;

   int port_encoded = _radio_encode_port(portNumber);
   sprintf(szFilter, "ether[0x00:2] == 0x0801 && ether[0x0a:4] == 0x13123456 && ether[0x04:1] == 0x%.2x", port_encoded);
   sprintf(szFilterPrism, "radio[0x40:2] == 0x0801 && radio[0x4a:4] == 0x13123456 && radio[0x44:1] == 0x%.2x", port_encoded);

   int iResult = _radio_open_interface_for_read_with_filter(interfaceIndex, szFilter, szFilterPrism);
   
   if ( iResult < 0 )
      return iResult;

   pRadioHWInfo->runtimeInterfaceInfoRx.nPort = portNumber;

   if ( portNumber == RADIO_PORT_ROUTER_UPLINK )
      log_line("Opened radio interface %d (%s) for reading on uplink on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);
   else if ( portNumber == RADIO_PORT_ROUTER_DOWNLINK )
      log_line("Opened radio interface %d (%s) for reading on downlink on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);
   else
      log_line("Opened radio interface %d (%s) for reading on custom port %d on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, portNumber, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);

   return pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
}


int radio_open_interface_for_write(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
      return -1;

   if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
   // Bad karma
   {
      log_softerror_and_alarm("Tried to open a non 2.4/5.8 radio in high capacity 2.4/5.8 mode.");
      return -1;
   }

   log_line("Opened radio interface %d (%s) for writing...", interfaceIndex+1, pRadioHWInfo->szName);

   pRadioHWInfo->openedForWrite = 0;
   pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = -1;
   pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount = 0;

   if ( s_iUsePCAPForTx )
   {
      log_line("Using ppcap for tx packets.");
      char errbuf[PCAP_ERRBUF_SIZE];

      pRadioHWInfo->runtimeInterfaceInfoTx.ppcap = pcap_create(pRadioHWInfo->szName, errbuf);
      if (pRadioHWInfo->runtimeInterfaceInfoTx.ppcap == NULL)
      {
         log_error_and_alarm("Failed to get ppcap for write");
         return -1;
      }
      log_line("Created ppcap interface. Setting params...");

      if (pcap_set_snaplen(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, 4096) !=0) log_line("set_snaplen failed");
      if (pcap_set_promisc(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, 1) != 0) log_line("set_promisc failed");
      if (pcap_set_timeout(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, -1) !=0) log_line("set_timeout failed");
      if (pcap_set_immediate_mode(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, 1) != 0) log_line("pcap_set_immediate_mode failed: %s", pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap));
      if (pcap_activate(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap) !=0) log_line("pcap_activate failed: %s", pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap));

      pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = pcap_get_selectable_fd(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap);
      log_line("ppcap returned a selectable fd for write for interface %d: ppcap: %d, fd=%d", interfaceIndex + 1, pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
      
      //if ( pRadioHWInfo->openedForRead )
      //   pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
      //else
      //   log_line("Failed to reuse read interface for write.");
   }
   else
   {   
      log_line("Using socket for tx packets.");
      struct sockaddr_ll ll_addr;
      struct ifreq ifr;
      pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = socket(PF_PACKET, SOCK_RAW, 0);
      if (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd == -1)
      {
         log_error_and_alarm("Error:\tSocket failed");
         return -1;
      }

      if ( s_iBypassSocketBuffers )
      {
          const int iVal = 1;
          if ( 0 != setsockopt(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, SOL_PACKET, PACKET_QDISC_BYPASS, (const void *)&iVal, sizeof(iVal)) )
             log_softerror_and_alarm("Failed to set PACKET_QDISC_BYPASS option on socket fd %d, error code: %d, error: (%s)", pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, errno, strerror(errno));
          else
             log_line("Did set PACKET_QDISC_BYPASS option on socket fd %d", pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
      }
      ll_addr.sll_family = AF_PACKET;
      ll_addr.sll_protocol = 0;

      if ( strlen(pRadioHWInfo->szName) < IFNAMSIZ-1 )
         strcpy(ifr.ifr_name, pRadioHWInfo->szName);
      else
      {
         char szBuff[64];
         strcpy(szBuff, pRadioHWInfo->szName);
         szBuff[IFNAMSIZ-1] = 0;
         strcpy(ifr.ifr_name, szBuff);
      }
      if (ioctl(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, SIOCGIFINDEX, &ifr) < 0)
      {
        	log_error_and_alarm("Error:\tioctl(SIOCGIFINDEX) failed");
        	return -1;
      }
      ll_addr.sll_ifindex = ifr.ifr_ifindex;
      if (ioctl(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, SIOCGIFHWADDR, &ifr) < 0)
      {
         	log_error_and_alarm("Error:\tioctl(SIOCGIFHWADDR) failed");
         	return -1;
      }
      memcpy(ll_addr.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
      if (bind (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, (struct sockaddr *)&ll_addr, sizeof(ll_addr)) == -1)
      {
        	log_error_and_alarm("Error:\tbind failed");
        	close(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
         pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = -1;
   	     return -1;
      }
      if (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd == -1 ) 
      {
          log_error_and_alarm("Error:\tCannot open socket.\tInfo: Must be root with an 802.11 card with RFMON enabled");
          return -1;
      }
      log_line("Opened socket for write fd=%d.", pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
   }

   pRadioHWInfo->openedForWrite = 1;
   log_line("Opened radio interface %d (%s) for writing on %s. Returned fd=%d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
   return pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd;
}

void radio_close_interface_for_read(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Trying to close invalid interface index. Interface index: %d", interfaceIndex+1);         
      return;
   }

   radio_rx_pause_interface(interfaceIndex, "Close radio interface");
   
   if ( NULL != pRadioHWInfo->runtimeInterfaceInfoRx.ppcap )
   {
      log_line("Closed radio interface %d [%s] that was used for read, selectable read fd was: %d, ppcap was: %d", interfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);
      pcap_close(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap);
   }
   else
      log_line("Radio interface %d was not opened for read.", interfaceIndex+1);

   pRadioHWInfo->runtimeInterfaceInfoRx.ppcap = NULL;
   pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd = -1;
   pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount = 0;
   pRadioHWInfo->openedForRead = 0;

   radio_rx_resume_interface(interfaceIndex);
}

void radio_close_interfaces_for_read()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL != pRadioHWInfo) && pRadioHWInfo->openedForRead )
         radio_close_interface_for_read(i);
   }
   log_line("Closed all radio interfaces used for read (in pcap mode).");
}

void radio_close_interface_for_write(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Trying to close invalid interface index. Interface index: %d", interfaceIndex+1);         
      return;
   }

   log_line("Closed radio interface %d (%s) that was used for write. Selectable write fd was: %d, ppcap was: %d", interfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, pRadioHWInfo->runtimeInterfaceInfoTx.ppcap);

   if ( s_iUsePCAPForTx )
   {
      if ( NULL != pRadioHWInfo->runtimeInterfaceInfoTx.ppcap )
         pcap_close(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap);
      else
         log_line("Radio interface %d was not opened for read.", interfaceIndex+1);
   }
   else
   {
      if ( -1 != pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd )
         close(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd);
      else
         log_line("Radio interface %d was not opened for write.", interfaceIndex+1);
   }

   pRadioHWInfo->runtimeInterfaceInfoTx.ppcap = NULL;
   pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd = -1;
   pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount = 0;
   pRadioHWInfo->openedForWrite = 0;
}


u8* radio_process_wlan_data_in(int interfaceNumber, int* piOutPacketLength, int* piOutRxDatarate, u32 uTimeNow)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceNumber);

   s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;

   if ( NULL != piOutPacketLength )
      *piOutPacketLength = 0;


#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
#endif

   struct pcap_pkthdr * ppcapPacketHeader = NULL;
   struct ieee80211_radiotap_iterator rti;
   u8 *pRadioPayload = sPayloadBufferRead;
   int payloadLength = 0;
   int n = 0;

   /*   
   int retval = pcap_next_ex(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, &ppcapPacketHeader, (const u_char**)&pRadioPayload);
   if (retval < 0)
   {
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_INTERFACE_BROKEN;
      if (strcmp("The interface went down",pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap)) == 0)
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount++;

         log_softerror_and_alarm("rx pcap ERROR: Radio interface %d (%s) went down. Error count: %d, ppcap = %x, fd = %d", 
            interfaceNumber+1,
            pRadioHWInfo->szName,
            pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount,
            pRadioHWInfo->runtimeInterfaceInfoRx.ppcap,
            pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd );

         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      else
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount++;
         log_softerror_and_alarm("rx pcap ERROR: %s, error count: %d", pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap), pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount);
         s_iRadioInterfacesBroken++;
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   if (retval != 1)
   {
      if ( 0 == retval )
      {
         //s_iRadioLastReadErrorCode = RADIO_READ_ERROR_TIMEDOUT;
         //log_softerror_and_alarm("Rx ppcap timedout reading a packet.");
         s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;
         
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_READ_ERROR;
      log_softerror_and_alarm("rx pcap ERROR getting received data on ppcap: %x; retval != 1; retval is: %d", pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, retval);
      log_softerror_and_alarm("pcap error: %s", pcap_geterr(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap)); 
      log_line("rx pcap received: %d - %d", ppcapPacketHeader->caplen, ppcapPacketHeader->len);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }
   */
   struct pcap_pkthdr pcapHeader;
   ppcapPacketHeader = &pcapHeader;
   pRadioPayload = (u8*) pcap_next(pRadioHWInfo->runtimeInterfaceInfoRx.ppcap, ppcapPacketHeader); 
   if ( NULL == pRadioPayload )
      return NULL;
   #ifdef DEBUG_PACKET_RECEIVED
   log_line("RX Buffer: caplen: %d bytes, len: %d", ppcapPacketHeader->caplen, ppcapPacketHeader->len);
   #endif

   if (ieee80211_radiotap_iterator_init(&rti,(struct ieee80211_radiotap_header *)pRadioPayload, ppcapPacketHeader->len) < 0)
   {
      log_softerror_and_alarm("rx pcap ERROR: radiotap_iterator_init < 0");
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   pRadioHWInfo->runtimeInterfaceInfoRx.iErrorCount = 0;
   pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nRadiotapFlags = 0;
   int iAntennaCount = 0;
   int idBm = 0;
   int iAntennaDBM[MAX_RADIO_ANTENNAS];
   int iAntennaDBMNoise[MAX_RADIO_ANTENNAS];
   int iAntennaRate = 0;

   for( int i=0; i<MAX_RADIO_ANTENNAS; i++ )
   {
      iAntennaDBM[i] = 1000;
      iAntennaDBMNoise[i] = 1000;
   }

   while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0)
   {
      switch (rti.this_arg_index)
      {
         case IEEE80211_RADIOTAP_RATE:
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS = getRealDataRateFromRadioDataRate((int)(*((u8*)(rti.this_arg)))/2, RADIO_FLAGS_USE_LEGACY_DATARATES, 0);
            iAntennaRate = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;
            break;

         case IEEE80211_RADIOTAP_MCS:
            {
            //u8 known = rti.this_arg[0];
            //u8 flags = rti.this_arg[1];
            u8 mcs = rti.this_arg[2];
            //log_line("recv MCS, %d, %d, %d", (int)known, (int)flags, (int)mcs);
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS = -mcs-1;
            iAntennaRate = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;
            break;
            }

         case IEEE80211_RADIOTAP_CHANNEL:
            //pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nChannel = le16_to_cpu(*((u16 *)rti.this_arg));
            //pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nChannelFlags = le16_to_cpu(*((u16 *)(rti.this_arg + 2)));
            //pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nFreq = le32toh(*(uint32_t*)(rti.this_arg)) & 0xffff;
            break;
	
         case IEEE80211_RADIOTAP_ANTENNA:
            //pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nAntenna = (*rti.this_arg) + 1;
            iAntennaCount++;
            break;
		
         case IEEE80211_RADIOTAP_FLAGS:
             pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nRadiotapFlags = (int)(*(uint8_t*)(rti.this_arg));
             //log_line("radio tap flags: %d", pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nRadiotapFlags);
             break;

         case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
             {
                idBm = *((int8_t*)(rti.this_arg));
                if ( idBm < 10 )
                {
                   if ( iAntennaDBM[iAntennaCount] > 500 )
                      iAntennaDBM[iAntennaCount] = idBm;
                   else if ( idBm > iAntennaDBM[iAntennaCount] )
                      iAntennaDBM[iAntennaCount] = idBm;
                }
                break;
             }

         case IEEE80211_RADIOTAP_DBM_ANTNOISE:
             {
                idBm = *((int8_t*)(rti.this_arg));
                if ( idBm < 0 )
                {
                   if ( iAntennaDBMNoise[iAntennaCount] > 500 )
                      iAntennaDBMNoise[iAntennaCount] = idBm;
                   if ( idBm < iAntennaDBMNoise[iAntennaCount] )
                      iAntennaDBMNoise[iAntennaCount] = idBm;
                }
                break;
             }
      }
   }

   sRadioLastReceivedHeadersLength = rti.max_length + sizeof(s_uIEEEHeaderData);
   pRadioPayload += sRadioLastReceivedHeadersLength;
   payloadLength = ppcapPacketHeader->len - sRadioLastReceivedHeadersLength;
   // Ralink and Atheros both always supply the FCS to userspace at the end, so remove it from size
   if (pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS)
      payloadLength -= 4;

   #ifdef DEBUG_PACKET_RECEIVED
   log_line("ieee iterator length: %d, iee header: %d", rti.max_length,sizeof(s_uIEEEHeaderData) );
   if ( ppcapPacketHeader->caplen <= 96 )
   {
      log_line("Received buffer over the air (%d bytes):", ppcapPacketHeader->caplen );
      log_buffer2(pRadioPayload, ppcapPacketHeader->caplen, rti.max_length, sizeof(s_uIEEEHeaderData));
   }
   #endif

   if ( NULL != piOutPacketLength )
      *piOutPacketLength = payloadLength;
   if ( NULL != piOutRxDatarate )
      *piOutRxDatarate = iAntennaRate;

   if ( payloadLength >= (int)sizeof(t_packet_header) )
   {
      t_packet_header* pPH = (t_packet_header*)pRadioPayload;

      pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.uLastTimeCapture = uTimeNow;
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.uLastTimeCapture = uTimeNow;
      else
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.uLastTimeCapture = uTimeNow;

      int iMaxDBM = 1000;
      int iMaxDBMNoise = 1000;
      int iMaxSNR = 1000;
      int iMaxDBMThresh = 1000;
      int iMaxDBMThreshDatarate = 0;
      int iMaxSNRThresh = 1000;
      int iMaxSNRThreshDatarate = 0;
      int iMaxSNRThreshDBMValue = 1000;
      int iMaxSNRThreshSNRValue = 1000;

      static int siDBGCountRx = 0;
      siDBGCountRx++;
      for( int i=0; i<iAntennaCount; i++ )
      {
         if ( iAntennaDBM[i] < 500 )
         {
            if ( (iMaxDBM == 1000) || (iAntennaDBM[i] > iMaxDBM) )
               iMaxDBM = iAntennaDBM[i];
            if ( iAntennaDBMNoise[i] < 500 )
            {
               int iSNR = iAntennaDBM[i] - iAntennaDBMNoise[i];
               if ( (iMaxSNR == 1000) || (iSNR > iMaxSNR) )
                  iMaxSNR = iSNR;

               if ( iAntennaRate != 0 )
               {
                  int iDBMThresh = iAntennaDBM[i] - getRadioMinimDBMForDataRate(iAntennaRate);
                  if ( (iMaxDBMThresh == 1000) || (iDBMThresh > iMaxDBMThresh) )
                  {
                     iMaxDBMThresh = iDBMThresh;
                     iMaxDBMThreshDatarate = iAntennaRate;
                  }
                  int iSNRThresh = iSNR - getRadioMinimSNRForDataRate(iAntennaRate);
                  if ( (iMaxSNRThresh == 1000) || (iSNRThresh > iMaxSNRThresh) )
                  {
                     iMaxSNRThresh = iSNRThresh;
                     iMaxSNRThreshDatarate = iAntennaRate;
                     iMaxSNRThreshDBMValue = iAntennaDBM[i];
                     iMaxSNRThreshSNRValue = iAntennaDBMNoise[i];
                  }
               }
            }
         }

         if ( iAntennaDBMNoise[i] < 500 )
         {
            if ( (iMaxDBMNoise == 1000) || (iAntennaDBMNoise[i] > iMaxDBMNoise) )
               iMaxDBMNoise = iAntennaDBMNoise[i];
         }
      }

      pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmLast = iMaxDBM;
      pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseLast = iMaxDBMNoise;
      pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRLast = iMaxSNR;
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmLast = iMaxDBM;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseLast = iMaxDBMNoise;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRLast = iMaxSNR;
      }
      else
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmLast = iMaxDBM;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseLast = iMaxDBMNoise;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRLast = iMaxSNR;
      }

      if ( iMaxDBM < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmMin )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmMin = iMaxDBM;
      if ( iMaxDBM < 1000 )
      if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmMax) || (iMaxDBM > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmMax) )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmMax = iMaxDBM;

      if ( iMaxDBMNoise < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseMin )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseMin = iMaxDBMNoise;
      if ( iMaxDBMNoise < 1000 )
      if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseMax) || (iMaxDBMNoise > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseMax) )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseMax = iMaxDBMNoise;

      if ( iMaxSNR < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRMin )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRMin = iMaxSNR;
      if ( iMaxSNR < 1000 )
      if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRMax) || (iMaxSNR > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRMax) )
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRMax = iMaxSNR;

      if ( iMaxDBMThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmThreshMin )
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmThreshMin = iMaxDBMThresh;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmThreshMinDatarate = iMaxDBMThreshDatarate;
      }

      if ( iMaxSNRThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRThreshMin )
      {
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRThreshMin = iMaxSNRThresh;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRThreshMinDatarate = iMaxSNRThreshDatarate;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRThreshMinDBMValue = iMaxSNRThreshDBMValue;
         pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRThreshMinSNRValue = iMaxSNRThreshSNRValue;
      }

      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
      {
         if ( iMaxDBM < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmMin = iMaxDBM;
         if ( iMaxDBM < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmMax) || (iMaxDBM > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmMax = iMaxDBM;

         if ( iMaxDBMNoise < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseMin = iMaxDBMNoise;
         if ( iMaxDBMNoise < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseMax) || (iMaxDBMNoise > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmNoiseMax = iMaxDBMNoise;

         if ( iMaxSNR < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRMin = iMaxSNR;
         if ( iMaxSNR < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRMax) || (iMaxSNR > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRMax = iMaxSNR;

         if ( iMaxDBMThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmThreshMin )
         {
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmThreshMin = iMaxDBMThresh;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iDbmThreshMinDatarate = iMaxDBMThreshDatarate;
         }
         if ( iMaxSNRThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRThreshMin )
         {
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRThreshMin = iMaxSNRThresh;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRThreshMinDatarate = iMaxSNRThreshDatarate;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRThreshMinDBMValue = iMaxSNRThreshDBMValue;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo.iSNRThreshMinSNRValue = iMaxSNRThreshSNRValue;
         }
      }
      else
      {
         if ( iMaxDBM < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmMin = iMaxDBM;
         if ( iMaxDBM < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmMax) || (iMaxDBM > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmMax = iMaxDBM;

         if ( iMaxDBMNoise < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseMin = iMaxDBMNoise;
         if ( iMaxDBMNoise < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseMax) || (iMaxDBMNoise > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmNoiseMax = iMaxDBMNoise;

         if ( iMaxSNR < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRMin )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRMin = iMaxSNR;
         if ( iMaxSNR < 1000 )
         if ( (1000 == pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRMax) || (iMaxSNR > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRMax) )
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRMax = iMaxSNR;

         if ( iMaxDBMThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmThreshMin )
         {
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmThreshMin = iMaxDBMThresh;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iDbmThreshMinDatarate = iMaxDBMThreshDatarate;
         }
         if ( iMaxSNRThresh < pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRThreshMin )
         {
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRThreshMin = iMaxSNRThresh;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRThreshMinDatarate = iMaxSNRThreshDatarate;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRThreshMinDBMValue = iMaxSNRThreshDBMValue;
            pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData.iSNRThreshMinSNRValue = iMaxSNRThreshSNRValue;
         }
      }
   }
   if ( iAntennaCount > pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nAntennaCount )
      pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nAntennaCount = iAntennaCount;


   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   #ifdef DEBUG_PACKET_RECEIVED
   if ( payloadLength <= 96 )
   {
      log_line("Processed received packet over the air (%d bytes):", payloadLength );
      log_buffer4(pRadioPayload, payloadLength, sizeof(s_uIEEEHeaderData)-5, 10, 6, 8);
   }
   #endif

   //return pRadioPayload;
   memcpy(sPayloadBufferRead, pRadioPayload, payloadLength);
   return sPayloadBufferRead;
}

// returns 0 for failure, total length of packet for success

int packet_process_and_check(int interfaceNb, u8* pPacketBuffer, int iBufferLength, int* pbCRCOk)
{
   //log_line("Check radio packet: %d", length);

   s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_NO_ERROR;

   if ( NULL != pbCRCOk )
      *pbCRCOk = 0;

   if ( NULL == pPacketBuffer || 0 == iBufferLength )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_INVALID_PARAMETERS;
      return 0;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( iBufferLength < (int)sizeof(t_packet_header) )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL;

      #ifdef DEBUG_PACKET_RECEIVED
      printf("Received packet to small: packet length: %d bytes, header minimum: %d\n", iBufferLength, (int)sizeof(t_packet_header));
      log_line("Received packet to small: packet length: %d bytes, header minimum: %d", iBufferLength, (int)sizeof(t_packet_header));
      #endif
      return 0;
   }


   int iPacketLength = pPH->total_length;
   u8 uPacketFlags = pPH->packet_flags;

   if ( iPacketLength > iBufferLength )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL;

      #ifdef DEBUG_PACKET_RECEIVED
      printf("Received packet to small: packet length: %d bytes, buffer length is: %d\n", iPacketLength, iBufferLength);
      log_line("Received packet to small: packet length: %d bytes, buffer length is: %d", iPacketLength, iBufferLength);
      #endif
      return 0;
   }

   if ( uPacketFlags & PACKET_FLAGS_BIT_HAS_ENCRYPTION )
   {
      #ifdef DEBUG_PACKET_RECEIVED
      log_line("enc detected");
      #endif
      int dx = sizeof(t_packet_header);
      int l = iPacketLength-dx;
      dpp(pPacketBuffer + dx, l);
   }

   u32 uCRC = 0;
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), sizeof(t_packet_header)-sizeof(u32));
   else
      uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), pPH->total_length-sizeof(u32));

   if ( (uCRC & 0x00FFFFFF) != (pPH->uCRC & 0x00FFFFFF) )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED;
      #ifdef DEBUG_PACKET_RECEIVED
      log_line("Received packet bad headers, CRC headers: %u, computed CRC(%s): %u, packet length: %d bytes", pPH->uCRC, (pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC)?"headers only":"full", uCRC, iPacketLength);
      if ( packetLength <= 48 )
         log_buffer(pPacketBuffer, iPacketLength);
      #endif
      if ( NULL != pbCRCOk )
         *pbCRCOk = 0;
      return 0;
   }

   if ( NULL != pbCRCOk )
      *pbCRCOk = 1;

   //#ifdef DEBUG_PACKET_RECEIVED
   //printf("Received packet correctly. Packet length: %d bytes\n", packetLength);
   //log_line("Received packet correctly. Packet length: %d bytes", packetLength);
   //#endif

   s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_NO_ERROR;
   return iPacketLength;     
}

int get_last_processing_error_code()
{
   return s_iLastProcessingErrorCode;
}

u32 radio_get_next_radio_link_packet_index(int iLocalRadioLinkId)
{
   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u32 uRadioLinkPacketIndex = s_uNextRadioPacketIndexes[iLocalRadioLinkId];
   s_uNextRadioPacketIndexes[iLocalRadioLinkId]++;

   return uRadioLinkPacketIndex;
}

int radio_build_new_raw_ieee_packet(int iLocalRadioLinkId, u8* pRawPacket, u8* pPacketData, int nInputLength, int portNb, int bEncrypt)
{
   int totalRadioLength = 0;

   s_uIEEEHeaderData_short[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderData[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderRTS[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderData[22] = uIEEEE80211SeqNb & 0xff;
   s_uIEEEHeaderData[23] = (uIEEEE80211SeqNb >> 8) & 0xff;
   uIEEEE80211SeqNb += 16;
   
   if ( (sRadioFrameFlags & RADIO_FLAGS_USE_MCS_DATARATES) || (sRadioDataRate_bps < 0) )
   {
      memcpy(pRawPacket, s_uRadiotapHeaderMCS, sizeof(s_uRadiotapHeaderMCS));
      pRawPacket += sizeof(s_uRadiotapHeaderMCS);
      totalRadioLength += sizeof(s_uRadiotapHeaderMCS);
      s_uLastPacketSentRadioTapHeaderLength = sizeof(s_uRadiotapHeaderMCS);
   }
   else
   {
      memcpy(pRawPacket, s_uRadiotapHeaderLegacy, sizeof(s_uRadiotapHeaderLegacy));
      pRawPacket += sizeof(s_uRadiotapHeaderLegacy);
      totalRadioLength += sizeof(s_uRadiotapHeaderLegacy);
      s_uLastPacketSentRadioTapHeaderLength = sizeof(s_uRadiotapHeaderLegacy);
   }

   //if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_DATA )
   {
      memcpy(pRawPacket, s_uIEEEHeaderData, sizeof (s_uIEEEHeaderData));
      pRawPacket += sizeof(s_uIEEEHeaderData);
      totalRadioLength += sizeof(s_uIEEEHeaderData);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData);
   }
   /*
   else if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_RTS )
   {   
      memcpy(pRawPacket, s_uIEEEHeaderRTS, sizeof (s_uIEEEHeaderRTS));
      pRawPacket += sizeof(s_uIEEEHeaderRTS);
      totalRadioLength += sizeof(s_uIEEEHeaderRTS);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderRTS);
   }	
   else if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_DATA_SHORT )
   {
      memcpy(pRawPacket, s_uIEEEHeaderData_short, sizeof (s_uIEEEHeaderData_short));
      pRawPacket += sizeof(s_uIEEEHeaderData_short);
      totalRadioLength += sizeof(s_uIEEEHeaderData_short);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData_short);
   }
   else
   {
      memcpy(pRawPacket, s_uIEEEHeaderData, sizeof (s_uIEEEHeaderData));
      pRawPacket += sizeof(s_uIEEEHeaderData);
      totalRadioLength += sizeof(s_uIEEEHeaderData);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData);
   }
   */

   memcpy(pRawPacket, pPacketData, nInputLength);
   totalRadioLength += nInputLength;

   if ( s_bRadioDebugFlag )
      memcpy(s_uLastPacketBuilt, pPacketData, nInputLength);
   
   #ifdef DEBUG_PACKET_SENT
   log_line("Building a composed packet of total size: %d, extra data: %d", nInputLength + iExtraData, iExtraData);
   #endif

   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u16 uRadioLinkPacketIndex = radio_get_next_radio_link_packet_index(iLocalRadioLinkId);

   // Compute CRC/encrypt packet
  
   t_packet_header* pPH = (t_packet_header*)pRawPacket;
   pPH->radio_link_packet_index = uRadioLinkPacketIndex;
   if ( bEncrypt )
      pPH->packet_flags |= PACKET_FLAGS_BIT_HAS_ENCRYPTION;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      radio_packet_compute_crc((u8*)pPH, sizeof(t_packet_header));
   else
      radio_packet_compute_crc((u8*)pPH, pPH->total_length);

   if ( bEncrypt )
   {
      int dx = sizeof(t_packet_header);
      epp(pRawPacket+dx, pPH->total_length-dx);
   }
   return totalRadioLength;
}


int radio_write_raw_ieee_packet(int interfaceIndex, u8* pData, int dataLength, int iRepeatCount)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a radio message to an invalid interface (%d).", interfaceIndex+1);
      return 0;
   }

   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return 0;
   }

   if ( s_bRadioDebugFlag )
   {
      t_packet_header* pPH = (t_packet_header*)&s_uLastPacketBuilt[0];
      if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
      {
         s_uLastRadioPingSentTime = get_current_timestamp_ms();
         s_uLastRadioPingId = s_uLastPacketBuilt[sizeof(t_packet_header)];
      }
   }

   // Extract packet header based on ieee headers
   /*
   t_packet_header* pPH = NULL;
   if ( pData[2] == 0x0C )
     pPH = (t_packet_header*)(pData + sizeof(s_uRadiotapHeaderLegacy) + sizeof(s_uIEEEHeaderData));
   else if ( pData[2] == 0x0D )
     pPH = (t_packet_header*)(pData + sizeof(s_uRadiotapHeaderMCS) + sizeof(s_uIEEEHeaderData));
   else
     pPH = NULL;
   */

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   s_uPacketsSentUsingCurrent_RadioRate++;
   s_uPacketsSentUsingCurrent_RadioFlags++;

   int len = 0;

   for( int k=0; k<=iRepeatCount; k++ )
   {
      if ( s_iUsePCAPForTx )
      {
         len = pcap_inject(pRadioHWInfo->runtimeInterfaceInfoTx.ppcap, pData, dataLength);
         if ( len < dataLength )
         {
            log_softerror_and_alarm("RadioError: tx ppcap failed to send radio message (%d bytes sent of %d bytes).", len, dataLength);
            pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount++;
            #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
            if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
               pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
            #endif
            return 0;
         }
         else
            pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount = 0;
      }
      else
      {
         len = write(pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, pData, dataLength);
         if ( len < dataLength )
         {
            log_softerror_and_alarm("RadioError: Failed to send radio message on radio interface %d, fd=%d (%d bytes sent of %d bytes).",
              interfaceIndex+1, pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd, len, dataLength);
            pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount++;
            #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
            if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
               pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
            #endif
            return 0;
         }
         else
            pRadioHWInfo->runtimeInterfaceInfoTx.iErrorCount = 0;
      }
      if ( k < iRepeatCount )
         hardware_sleep_ms(1);
   }
   
   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   #ifdef DEBUG_PACKET_SENT
   if ( dataLength <= 96 )
   {
      log_line("Sent buffer over the radio (%d bytes [%d headers, %d data]):", dataLength, s_uLastPacketSentRadioTapHeaderLength + s_uLastPacketSentIEEEHeaderLength, dataLength - s_uLastPacketSentRadioTapHeaderLength - s_uLastPacketSentIEEEHeaderLength);
      log_buffer5(pData, dataLength, s_uLastPacketSentRadioTapHeaderLength, s_uLastPacketSentIEEEHeaderLength, 10,6,8 ); // 24 is size of Ruby packet header
   }
   #endif

   return 1;
}


// Returns the number of bytes written or -1 for error, -2 for write error

int radio_write_serial_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow)
{
   if ( (interfaceIndex < 0) || (interfaceIndex >= MAX_RADIO_INTERFACES) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an invalid interface index %d.", interfaceIndex+1);
      return -1;    
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an invalid interface (%d).", interfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_is_serial_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an interface that is not a serial radio (%d).", interfaceIndex+1);
      return -1;    
   }
   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return -1;
   }

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( ! hardware_radio_serial_write_packet(interfaceIndex, pData, dataLength) )
   {
      log_softerror_and_alarm("RadioError: Failed to write a serial radio message (%d bytes) to serial radio interface %d.", dataLength, interfaceIndex+1);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return (-2);
   }

   s_uIndexLastLowDataLinkPackets[interfaceIndex]++;
   if ( s_uIndexLastLowDataLinkPackets[interfaceIndex] >= MAX_LOW_DATALINK_PACKETS_HISTORY )
      s_uIndexLastLowDataLinkPackets[interfaceIndex] = 0;

   s_uTimesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = uTimeNow;
   s_uSizesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = dataLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   return dataLength;
}

// Returns the number of bytes written or -1 for error, -2 for write error

int radio_write_sik_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow)
{
   if ( (interfaceIndex < 0) || (interfaceIndex >= MAX_RADIO_INTERFACES) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an invalid interface index %d.", interfaceIndex+1);
      return -1;    
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->runtimeInterfaceInfoTx.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an invalid interface (%d).", interfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an interface that is not SiK radio (%d).", interfaceIndex+1);
      return -1;    
   }
   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return -1;
   }

   // Do not send packets that contain "+++" or "OK\n"
   int iOk = 1;
   u8* pTmp = pData;
   for( int i=0; i<dataLength-2; i++ )
   {
      if ( (*pTmp) == '+' )
      if ( (*(pTmp+1)) == '+' )
      if ( (*(pTmp+2)) == '+' )
      {
         iOk = 0;
         break;
      }
      if ( ((*pTmp) == 'O') || ((*pTmp) == 'o') )
      if ( ((*(pTmp+1)) == 'K') || ((*(pTmp+1)) == 'k') )
      if ( ((*(pTmp+2)) == 10)  || ((*(pTmp+2)) == 13) )
      {
         iOk = 0;
         break;
      }
      pTmp++;
   }
   if ( ! iOk )
   {
      log_softerror_and_alarm("RadioError: Ignored sending a SiK radio message (%d bytes) as it contained control codes.", dataLength);
      return -1;
   }

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( ! hardware_radio_sik_write_packet(interfaceIndex, pData, dataLength) )
   {
      log_softerror_and_alarm("RadioError: Failed to write a SiK radio message (%d bytes) to SiK radio interface %d.", dataLength, interfaceIndex+1);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return (-2);
   }

   s_uIndexLastLowDataLinkPackets[interfaceIndex]++;
   if ( s_uIndexLastLowDataLinkPackets[interfaceIndex] >= MAX_LOW_DATALINK_PACKETS_HISTORY )
      s_uIndexLastLowDataLinkPackets[interfaceIndex] = 0;

   s_uTimesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = uTimeNow;
   s_uSizesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = dataLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   return dataLength;
}
