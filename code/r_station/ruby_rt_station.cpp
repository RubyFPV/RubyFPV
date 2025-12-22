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

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/commands.h"
#include "../base/encr.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parse_fc_telemetry.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "../utils/utils_controller.h"
#include "../base/controller_rt_info.h"
#include "../base/vehicle_rt_info.h"
#include "../base/core_plugins_settings.h"
#include "../common/models_connect_frequencies.h"

#include "ruby_rt_station.h"
#include "shared_vars.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "rx_video_recording.h"
#include "process_radio_in_packets.h"
#include "process_radio_out_packets.h"
#include "process_local_packets.h"
#include "packets_utils.h"
#include "test_link_params.h"
#include "periodic_loop.h"

#include "timers.h"
#include "radio_links.h"
#include "radio_links_sik.h"
#include "adaptive_video.h"

u8 s_BufferCommands[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferCommands[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferCommandsPos = 0;

u8 s_BufferMessageFromTelemetry[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferTelemetryUplink[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferTelemetryUplinkPos = 0;  

u8 s_BufferRCUplink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferRCUplink[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferRCUplinkPos = 0;  

t_packet_queue s_QueueRadioPacketsHighPrio;
t_packet_queue s_QueueRadioPacketsRegPrio;
t_packet_queue s_QueueControlPackets;

int s_iCountCPULoopOverflows = 0;

int s_iSearchSikAirRate = -1;
int s_iSearchSikECC = -1;
int s_iSearchSikLBT = -1;
int s_iSearchSikMCSTR = -1;

u32 s_uTimeLastTryReadIPCMessages = 0;
u32 s_uTimeLastCheckForVideoPackets = 0;
u32 s_uAlarmIndexToCentral = 0;

static u32 s_uLastDetectedFrameStartTime = 0;
static u32 s_uLastDetectedFrameEndTime = 0;
bool s_bIsEOFDetected = false;

u32  router_get_last_time_checked_for_video_packets()
{
   return s_uTimeLastCheckForVideoPackets;
}

bool router_is_eof()
{
   return s_bIsEOFDetected;
}


void _router_update_frame_eof()
{
   if ( g_bSearching || (NULL == g_pCurrentModel) || (! is_sw_version_atleast(g_pCurrentModel, 11, 7)) )
   {
      s_bIsEOFDetected = true;
      return;
   }
   type_global_state_vehicle_runtime_info* pRTInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
   if ( (NULL == pRTInfo) || (! pRTInfo->bIsPairingDone) )
      return;

   static u32 s_uLastTimeCheckedForFrameEOF = 0;
   if ( g_TimeNow == s_uLastTimeCheckedForFrameEOF )
      return;
   s_uLastTimeCheckedForFrameEOF = g_TimeNow;

   s_uLastDetectedFrameStartTime = radio_rx_get_current_frame_start_time();
   s_uLastDetectedFrameEndTime = radio_rx_get_current_frame_end_time();
   u32 uTimeGuard = ((((u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags) & VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK)>>8);
   u32 uTimeEOFWithGuard = s_uLastDetectedFrameEndTime + uTimeGuard;
   g_TimeNow = get_current_timestamp_ms();

   u32 uMilisPerFrame = 33;
   if ( g_pCurrentModel->video_params.iVideoFPS > 0 )
      uMilisPerFrame = 1000/g_pCurrentModel->video_params.iVideoFPS;

   if ( (g_TimeNow < uTimeEOFWithGuard) || (g_TimeNow > s_uLastDetectedFrameEndTime + g_pCurrentModel->getCurrentVideoProfileMaxRetransmissionWindow() + uMilisPerFrame) ||
        (g_TimeNow < s_uLastDetectedFrameStartTime) || (g_TimeNow > s_uLastDetectedFrameStartTime + g_pCurrentModel->getCurrentVideoProfileMaxRetransmissionWindow() + 2*uMilisPerFrame) )
   {
      s_bIsEOFDetected = false;
      return;
   }

   bool bIsEOF = false;

   if ( g_TimeNow >= uTimeEOFWithGuard )
   if ( g_TimeNow < s_uLastDetectedFrameStartTime + uMilisPerFrame )
   if ( s_uLastDetectedFrameStartTime <= s_uLastDetectedFrameEndTime )
      bIsEOF = true;

   if ( (! bIsEOF) && (g_TimeNow >= uTimeEOFWithGuard) && (g_TimeNow <= s_uLastDetectedFrameEndTime + g_pCurrentModel->getCurrentVideoProfileMaxRetransmissionWindow()) )
   {
      /*
      log_line("DBG check for EOF update on older frames (last detected frame start was %u ms ago, last EOF with guard was %u ms ago, last detected EOF was %u ms ago, max retr window is %d ms)...",
          g_TimeNow - s_uLastDetectedFrameStartTime,
          g_TimeNow - uTimeEOFWithGuard, g_TimeNow - s_uLastDetectedFrameEndTime,
          g_pCurrentModel->getCurrentVideoProfileMaxRetransmissionWindow());
      */
      u32 uTimeFrameStart = s_uLastDetectedFrameStartTime;
      u32 uTimeFrameEnd = uTimeEOFWithGuard;
      if ( uTimeFrameEnd < uTimeFrameStart + 5 )
         uTimeFrameEnd = uTimeFrameStart+5;
      int iCount = 0;
      uTimeFrameStart += uMilisPerFrame;
      uTimeFrameEnd += uMilisPerFrame;
      while ( (uTimeFrameStart < g_TimeNow) && (iCount < 10) )
      {
         /*
         if ( g_TimeNow >= uTimeFrameEnd )
            log_line("DBG check foe EOF update on older frame %d: started %u ms ago, ended %u ms ago",
               iCount, g_TimeNow - uTimeFrameStart, g_TimeNow - uTimeFrameEnd);
         else
            log_line("DBG check foe EOF update on older frame %d: will start %u ms from now, will end %u ms from now",
               iCount, uTimeFrameStart - g_TimeNow, uTimeFrameEnd - g_TimeNow);
         */
         if ( g_TimeNow >= uTimeFrameEnd )
         if ( g_TimeNow < uTimeFrameStart + uMilisPerFrame )
         {
            bIsEOF = true;
            break;
         }
         iCount++;
         uTimeFrameStart += uMilisPerFrame;
         uTimeFrameEnd += uMilisPerFrame;
      }
   }
   //if ( s_bIsEOFDetected != bIsEOF )
   //   log_line("DBG EOF changed to %d", bIsEOF);
   s_bIsEOFDetected = bIsEOF;
}

void send_alarm_to_central(u32 uAlarm, u32 uFlags1, u32 uFlags2)
{
   s_uAlarmIndexToCentral++;
  
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_RUBY_ALARM, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &s_uAlarmIndexToCentral, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uAlarm, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uFlags1, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u32), &uFlags2, sizeof(u32));
   radio_packet_compute_crc(packet, PH.total_length);

   char szAlarm[256];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szAlarm);

   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent local alarm to central: [%s], alarm index: %u;", szAlarm, s_uAlarmIndexToCentral);
   else
   {
      log_softerror_and_alarm("Can't send alarm to central, no pipe. Alarm: [%s], alarm index: %u", szAlarm, s_uAlarmIndexToCentral);
      log_ipc_send_central_error(packet, PH.total_length);
   }
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;   
}

void log_ipc_send_central_error(u8* pPacket, int iLength)
{
   u32 uLastCentralActiveTime = 0;
   u32 uLastCentralIPCIncomingTime = 0;
   if ( NULL != g_pProcessStatsCentral )
   {
      uLastCentralActiveTime = g_pProcessStatsCentral->lastActiveTime;
      uLastCentralIPCIncomingTime = g_pProcessStatsCentral->lastIPCIncomingTime;
   }
   if ( NULL == pPacket )
      log_softerror_and_alarm("Failed to send IPC message to central: pipe fd: %d, message: NULL, size: %d, central last active time: %u ms ago, central last IPC incoming time: %u ms ago",
        g_fIPCToCentral, iLength, g_TimeNow - uLastCentralActiveTime, g_TimeNow - uLastCentralIPCIncomingTime);
   else
   {
      t_packet_header* pPH = (t_packet_header*)pPacket;
      log_softerror_and_alarm("Failed to send IPC message to central: pipe fd: %d, message: %s, size: %d, central last active time: %u ms ago, central last IPC incoming time: %u ms ago",
         g_fIPCToCentral, str_get_packet_type(pPH->packet_type), iLength, g_TimeNow - uLastCentralActiveTime, g_TimeNow - uLastCentralIPCIncomingTime);
   }
}

void _broadcast_radio_interface_init_failed(int iInterfaceIndex)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = iInterfaceIndex;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   radio_packet_compute_crc(buffer, PH.total_length);
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_ipc_send_central_error(buffer, PH.total_length);

   log_line("Sent message to central that radio interface %d failed to initialize.", iInterfaceIndex+1);
}


void _compute_radio_interfaces_assignment()
{
   log_line("------------------------------------------------------------------");

   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for assigning radio interfaces");
      return;
   }

   g_SM_RadioStats.countVehicleRadioLinks = 0;
   g_SM_RadioStats.countVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;
 
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = -1;
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = -1;
      g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = -1;
   }

   //---------------------------------------------------------------
   // See how many active radio links the vehicle has

   u32 uStoredMainFrequencyForModel = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
   int iStoredMainRadioLinkForModel = -1;

   int iCountAssignedVehicleRadioLinks = 0;

   int iCountVehicleActiveUsableRadioLinks = 0;
   u32 uConnectFirstUsableFrequency = 0;
   int iConnectFirstUsableRadioLinkId = 0;

   log_line("Computing local radio interfaces assignment to vehicle radio links...");
   log_line("Vehicle (%u, %s) main 'connect to' frequency: %s, vehicle has a total of %d radio links.",
      g_pCurrentModel->uVehicleId, g_pCurrentModel->getLongName(), str_format_frequency(uStoredMainFrequencyForModel), g_pCurrentModel->radioLinksParams.links_count);

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_line("Vehicle's radio link %d is disabled. Skipping it.", i+1);
         continue;
      }

      // Ignore vehicle's relay radio links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         log_line("Vehicle's radio link %d is used for relaying. Skipping it.", i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         log_line("Vehicle's radio link %d is used for relaying. Skipping it.", i+1);
         continue;       
      }      

      log_line("Vehicle's radio link %d is usable, current frequency: %s", i+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[i]));
      iCountVehicleActiveUsableRadioLinks++;
      if ( 0 == uConnectFirstUsableFrequency )
      {
         uConnectFirstUsableFrequency = g_pCurrentModel->radioLinksParams.link_frequency_khz[i];
         iConnectFirstUsableRadioLinkId = i;
      }
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[i] == uStoredMainFrequencyForModel )
      {
         iStoredMainRadioLinkForModel = i;
         log_line("Vehicle's radio link %d is the main connect to radio link.", i+1);
      }
   }

   log_line("Vehicle has %d active (enabled and not relay) radio links (out of %d radio links)", iCountVehicleActiveUsableRadioLinks, g_pCurrentModel->radioLinksParams.links_count);
   if ( -1 == iStoredMainRadioLinkForModel )
      log_line("Could not find vehicle's main connect radio link for vehicle, main connect frequency is: %s", str_format_frequency(uStoredMainFrequencyForModel));
   else
      log_line("Found vehicle's main connect radio link for frequency %s: vehicle radio link %d", str_format_frequency(uStoredMainFrequencyForModel), iStoredMainRadioLinkForModel+1);
   if ( 0 == iCountVehicleActiveUsableRadioLinks )
   {
      log_error_and_alarm("Vehicle has no active (enabled and not relay) radio links (out of %d radio links)", g_pCurrentModel->radioLinksParams.links_count);
      return;
   }

   //--------------------------------------------------------------------------
   // Begin - Check what vehicle radio links are supported by each radio interface.

   bool bInterfaceSupportsVehicleLink[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
   bool bInterfaceSupportsMainConnectLink[MAX_RADIO_INTERFACES];
   int iInterfaceSupportedLinksCount[MAX_RADIO_INTERFACES];
   
   bool bCtrlInterfaceWasAssigned[MAX_RADIO_INTERFACES];
   bool bVehicleLinkWasAssigned[MAX_RADIO_INTERFACES];
   int  iVehicleLinkWasAssignedToControllerLinkIndex[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      bCtrlInterfaceWasAssigned[i] = false;
      bVehicleLinkWasAssigned[i] = false;
      iVehicleLinkWasAssignedToControllerLinkIndex[i] = -1;

      bInterfaceSupportsMainConnectLink[i] = false;
      iInterfaceSupportedLinksCount[i] = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         bInterfaceSupportsVehicleLink[i][k] = false;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
      {
         log_softerror_and_alarm("Failed to get controller's radio interface %d hardware info. Skipping it.", i+1);
         continue;
      }
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Controller's radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

      for( int iRadioLink=0; iRadioLink<g_pCurrentModel->radioLinksParams.links_count; iRadioLink++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            continue;

         // Uplink type radio link and RX only radio interface

         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            continue;

         // Downlink type radio link and TX only radio interface

         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
            continue;

         bool bDoesMatch = false;

         // Match ELRS serial radio links
         if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS ) ||
              (pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
         {
            if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS ) &&
                 (pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
               bDoesMatch = true;
         }
         else if ( hardware_radio_supports_frequency(pRadioHWInfo, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink]) )
            bDoesMatch = true;

         if ( bDoesMatch )
         {
            log_line("Controller's radio interface %d does support vehicle's radio link %d (%s).", i+1, iRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink]));
            bInterfaceSupportsVehicleLink[i][iRadioLink] = true;
            iInterfaceSupportedLinksCount[i]++;

            if ( uStoredMainFrequencyForModel == g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink] )
               bInterfaceSupportsMainConnectLink[i] = true;
         }
      }
   }

   // End - Check what vehicle radio links are supported by each radio interface.
   //--------------------------------------------------------------------------

   //---------------------------------------------------------------
   // Begin - Model with a single active radio link

   if ( 1 == iCountVehicleActiveUsableRadioLinks )
   {
      log_line("Computing controller's radio interfaces assignment to vehicle's radio link %d (vehicle has a single active (enabled and not relay) radio link on %s)", iConnectFirstUsableRadioLinkId+1, str_format_frequency(uConnectFirstUsableFrequency));
      
      int iCountInterfacesAssigned = 0;

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         {
            log_line("  * Radio interface %d is disabled, do not assign it.", i+1);
            continue;
         }
         if ( ! hardware_radio_supports_frequency(pRadioHWInfo, uConnectFirstUsableFrequency) )
         {
            log_line("  * Radio interface %d does not support %s, do not assign it.", i+1, str_format_frequency(uConnectFirstUsableFrequency));
            continue;
         }
         g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = 0;
         g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iConnectFirstUsableRadioLinkId;
         g_SM_RadioStats.radio_links[0].matchingVehicleRadioLinkId = iConnectFirstUsableRadioLinkId;
         iCountInterfacesAssigned++;
         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL != pCardInfo )  
            log_line("  * Assigned radio interface %d (%s) to vehicle's radio link %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), iConnectFirstUsableRadioLinkId+1);
         else
            log_line("  * Assigned radio interface %d (%s) to vehicle's radio link %d", i+1, "Unknown Type", iConnectFirstUsableRadioLinkId+1);
      }
      iCountAssignedVehicleRadioLinks = 1;
      g_SM_RadioStats.countLocalRadioLinks = 1;
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
      if ( 0 == iCountInterfacesAssigned )
         send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK,iConnectFirstUsableRadioLinkId, 0);
      
      log_line("Controller will have %d radio links active/connected to vehicle.", iCountAssignedVehicleRadioLinks);
      log_line("Done computing radio interfaces assignment to radio links.");
      log_line("------------------------------------------------------------------");
      return;
   }

   // End - Model with a single active radio link
   //---------------------------------------------------------------


   //---------------------------------------------------------------
   // Begin - Model with a multiple active radio links

   log_line("Computing controller's radio interfaces assignment to vehicle's radio links (vehicle has %d active radio links (enabled and not relay), main controller connect frequency is: %s)", iCountVehicleActiveUsableRadioLinks, str_format_frequency(uStoredMainFrequencyForModel));

   //---------------------------------------------------------------
   // Begin - First, assign the radio interfaces that supports only a single radio link

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (controllerIsCardDisabled(pRadioHWInfo->szMAC)) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] != 1 )
         continue;

      int iSupportedVehicleLinkByInterface = -1;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         if ( bInterfaceSupportsVehicleLink[i][k] )
         {
            iSupportedVehicleLinkByInterface = k;
            break;
         }
      }
      if ( (-1 == iSupportedVehicleLinkByInterface) || (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iSupportedVehicleLinkByInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         continue;
      
      if ( ! bVehicleLinkWasAssigned[iSupportedVehicleLinkByInterface] )
      {
         iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface] = iCountAssignedVehicleRadioLinks;
         iCountAssignedVehicleRadioLinks++;
      }
      bVehicleLinkWasAssigned[iSupportedVehicleLinkByInterface] = true;
      bCtrlInterfaceWasAssigned[i] = true;
      
      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface];
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iSupportedVehicleLinkByInterface;
      g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface]].matchingVehicleRadioLinkId = iSupportedVehicleLinkByInterface;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL != pCardInfo )  
         log_line("  * Step A) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's radio link %d, %s, as it supports a single radio link from vehicle.", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iSupportedVehicleLinkByInterface+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iSupportedVehicleLinkByInterface]));
      else
         log_line("  * Step A) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's radio link %d, %s, as it supports a single radio link from vehicle.", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iSupportedVehicleLinkByInterface+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iSupportedVehicleLinkByInterface]));
   }

   //---------------------------------------------------------------
   // End - First, assign the radio interfaces that supports only a single radio link

   //---------------------------------------------------------------
   // Assign at least one radio interface to the main connect radio link

   if ( (iStoredMainRadioLinkForModel != -1) && (! bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel]) )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
            continue;
         if ( bCtrlInterfaceWasAssigned[i] )
            continue;
         if ( ! bInterfaceSupportsMainConnectLink[i] )
            continue;

         if ( ! bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel] )
         {
            iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel] = iCountAssignedVehicleRadioLinks;
            iCountAssignedVehicleRadioLinks++;
         }
         bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel] = true;
         bCtrlInterfaceWasAssigned[i] = true;
         
         g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel];
         g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iStoredMainRadioLinkForModel;
         g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel]].matchingVehicleRadioLinkId = iStoredMainRadioLinkForModel;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL != pCardInfo )  
            log_line("  * Step B) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's main connect radio link %d, %s", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iStoredMainRadioLinkForModel+1, str_format_frequency(uStoredMainFrequencyForModel));
         else
            log_line("  * Step B) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's main connect radio link %d, %s", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iStoredMainRadioLinkForModel+1, str_format_frequency(uStoredMainFrequencyForModel));
         break;
      }
   }

   //---------------------------------------------------------------
   // Assign alternativelly each remaining radio interfaces to one radio link

   // Assign to the first vehicle's radio link that has no cards assigned to

   int iVehicleRadioLinkIdToAssign = 0;
   int iSafeCounter = 10;
   while ( true && (iSafeCounter > 0) )
   {
      iSafeCounter--;
      if ( ! bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] )
         break;       

      iVehicleRadioLinkIdToAssign++;
      if ( iVehicleRadioLinkIdToAssign >= g_pCurrentModel->radioLinksParams.links_count )
      {
         iVehicleRadioLinkIdToAssign = 0;
         break;
      }
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] < 2 )
         continue;
      if ( bCtrlInterfaceWasAssigned[i] )
         continue;

      int k=0;
      do
      {
         if ( bInterfaceSupportsVehicleLink[i][iVehicleRadioLinkIdToAssign] )
         {
            if ( ! bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] )
            {
               iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign] = iCountAssignedVehicleRadioLinks;
               iCountAssignedVehicleRadioLinks++;
            }
            bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] = true;
            bCtrlInterfaceWasAssigned[i] = true;
            
            g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign];
            g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iVehicleRadioLinkIdToAssign;
            g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign]].matchingVehicleRadioLinkId = iVehicleRadioLinkIdToAssign;

            t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL != pCardInfo )  
               log_line("  * C) Assigned controller's radio interface %d (%s) to controller local radio link %d, radio link %d, %s", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iVehicleRadioLinkIdToAssign+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkIdToAssign]));
            else
               log_line("  * C) Assigned controller's radio interface %d (%s) to controller local radio link %d, radio link %d, %s", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iVehicleRadioLinkIdToAssign+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkIdToAssign]));
         }
         k++;
         iVehicleRadioLinkIdToAssign++;
         if ( iVehicleRadioLinkIdToAssign >= g_pCurrentModel->radioLinksParams.links_count )
            iVehicleRadioLinkIdToAssign = 0;
      }
      while ( (! bCtrlInterfaceWasAssigned[i]) && (k <= MAX_RADIO_INTERFACES) );
   }

   g_SM_RadioStats.countLocalRadioLinks = iCountAssignedVehicleRadioLinks;
   log_line("Assigned %d controller local radio links to vehicle's radio links (vehicle has %d active radio links)", iCountAssignedVehicleRadioLinks, iCountVehicleActiveUsableRadioLinks);
   
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

   //---------------------------------------------------------------
   // Log errors

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("  * Radio interface %d is disabled. It was not assigned to any radio link.", i+1 );
         continue;
      }
      if ( iInterfaceSupportedLinksCount[i] == 0 )
      {
         log_line("  * Radio interface %d does not support any radio links.", i+1 );
         continue;
      }
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Ignore vehicle's relay radio links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( ! bVehicleLinkWasAssigned[i] )
      {
         log_softerror_and_alarm("  * No controller radio interfaces where assigned to vehicle's radio link %d !", i+1);
         int iCountAssignableRadioInterfaces = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, i);
         if ( 0 == iCountAssignableRadioInterfaces )
            send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK, (u32)i, 0);
      }
   }
   log_line("Radio links mapping (from controller's %d local radio link(s) to vehicle's %d radio link(s):", g_SM_RadioStats.countLocalRadioLinks, g_pCurrentModel->radioLinksParams.links_count);
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      log_line("* Local radio link %d mapped to vehicle's radio link %d;", i+1, g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId+1);

   log_line("Done computing radio interfaces assignment to radio links.");
   log_line("------------------------------------------------------------------");
}


bool links_set_cards_frequencies_for_search( u32 uSearchFreq, bool bSiKSearch, int iAirDataRate, int iECC, int iLBT, int iMCSTR )
{
   log_line("Links: Set all cards frequencies for search mode to %s", str_format_frequency(uSearchFreq));
   if ( bSiKSearch )
      log_line("Search SiK mode update. Change all cards frequencies and update SiK params: Airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
         iAirDataRate, iECC, iLBT, iMCSTR);
   else
      log_line("No SiK mode update. Just change all interfaces frequencies");
   
   Preferences* pP = get_Preferences();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      u32 flags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      char szFlags[128];
      szFlags[0] = 0;
      str_get_radio_capabilities_description(flags, szFlags);
         
      log_line("Checking controller radio interface %d (%s) settings: MAC: [%s], flags: %s",
            i+1, pRadioHWInfo->szName, pRadioHWInfo->szMAC, szFlags );

      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Links: Radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      if ( ! pRadioHWInfo->isConfigurable )
      {
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Links: Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }

      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, uSearchFreq ) )
      {
         log_line("Links: Radio interface %d does not support search frequency %s. Skipping it.", i+1, str_format_frequency(uSearchFreq));
         continue;
      }

      if ( ! (flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      {
         log_line("Links: Radio interface %d can't Rx. Skipping it.", i+1);
         continue;
      }

      if ( ! (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         log_line("Links: Radio interface %d can't be used for data Rx. Skipping it.", i+1);
         continue;
      }

      if ( bSiKSearch && hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         u32 uFreqKhz = uSearchFreq;
         u32 uDataRate = iAirDataRate;
         u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
         if ( NULL != pCRII )
            uTxPower = pCRII->iRawPowerLevel;
         u32 uECC = iECC;
         u32 uLBT = iLBT;
         u32 uMCSTR = iMCSTR;

         bool bDataRateOk = false;
         for( int k=0; k<getSiKAirDataRatesCount(); k++ )
         {
            if ( (int)uDataRate == getSiKAirDataRates()[k] )
            {
               bDataRateOk = true;
               break;
            }
         }

         if ( ! bDataRateOk )
         {
            log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
            uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         }
         
         int iRetry = 0;
         while ( iRetry < 2 )
         {
            int iRes = hardware_radio_sik_set_params(pRadioHWInfo, 
                   uFreqKhz,
                   DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                   DEFAULT_RADIO_SIK_NETID,
                   uDataRate, uTxPower, 
                   uECC, uLBT, uMCSTR,
                   NULL);
            if ( iRes != 1 )
            {
               log_softerror_and_alarm("Failed to configure SiK radio interface %d", i+1);
               iRetry++;
            }
            else
            {
               log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                   i+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
               radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uSearchFreq);
               break;
            }
         }
      }
      else
      {
         if ( radio_utils_set_interface_frequency(NULL, i, -1, uSearchFreq, g_pProcessStats, pP->iDebugWiFiChangeDelay) )
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uSearchFreq);
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Links: Set all cards frequencies for search mode to %s. Completed.", str_format_frequency(uSearchFreq));
   return true;
}

bool links_set_cards_frequencies_and_params(int iVehicleLinkId)
{
   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for setting radio interfaces frequencies");
      return false;
   }

   if ( (iVehicleLinkId < 0) || (iVehicleLinkId >= hardware_get_radio_interfaces_count()) )
      log_line("Links: Setting all cards frequencies and params according to vehicle radio links...");
   else
      log_line("Links: Setting cards frequencies and params only for vehicle radio link %d", iVehicleLinkId+1);

   Preferences* pP = get_Preferences();
         
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( ! pRadioHWInfo->isConfigurable )
      {
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Links: Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Links: Radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      int nAssignedVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
      if ( (nAssignedVehicleRadioLinkId < 0) || (nAssignedVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_line("Links: Radio interface %d is not assigned to any vehicle radio link. Skipping it.", i+1);
         continue;
      }

      if ( ( iVehicleLinkId >= 0 ) && (nAssignedVehicleRadioLinkId != iVehicleLinkId) )
         continue;

      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId] ) )
      {
         log_line("Links: Radio interface %d does not support vehicle radio link %d frequency %s. Skipping it.", i+1, nAssignedVehicleRadioLinkId+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]));
         continue;
      }

      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         if ( iVehicleLinkId >= 0 )
            radio_links_flag_update_sik_interface(i);
         else
         {
            u32 uFreqKhz = g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId];
            u32 uDataRate = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[nAssignedVehicleRadioLinkId];
            u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL != pCRII )
               uTxPower = pCRII->iRawPowerLevel;
            u32 uECC = (g_pCurrentModel->radioLinksParams.link_radio_flags[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_ECC)? 1:0;
            u32 uLBT = (g_pCurrentModel->radioLinksParams.link_radio_flags[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_LBT)? 1:0;
            u32 uMCSTR = (g_pCurrentModel->radioLinksParams.link_radio_flags[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_MCSTR)? 1:0;

            bool bDataRateOk = false;
            for( int k=0; k<getSiKAirDataRatesCount(); k++ )
            {
               if ( (int)uDataRate == getSiKAirDataRates()[k] )
               {
                  bDataRateOk = true;
                  break;
               }
            }

            if ( ! bDataRateOk )
            {
               log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
               uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
            }
            
            int iRetry = 0;
            while ( iRetry < 2 )
            {
               int iRes = hardware_radio_sik_set_params(pRadioHWInfo, 
                      uFreqKhz,
                      DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                      DEFAULT_RADIO_SIK_NETID,
                      uDataRate, uTxPower, 
                      uECC, uLBT, uMCSTR,
                      NULL);
               if ( iRes != 1 )
               {
                  log_softerror_and_alarm("Failed to configure SiK radio interface %d", i+1);
                  iRetry++;
               }
               else
               {
                  log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                     i+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
                  radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]);
                  break;
               }
            }
         }
      }
      else
      {
         if ( radio_utils_set_interface_frequency(g_pCurrentModel, i, iVehicleLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId], g_pProcessStats, pP->iDebugWiFiChangeDelay) )
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]);
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

   hardware_save_radio_info();

   if ( (iVehicleLinkId < 0) || (iVehicleLinkId >= hardware_get_radio_interfaces_count()) )
      log_line("Links: Done setting all cards frequencies and params according to vehicle radio links.");
   else
      log_line("Links: Done setting cards frequencies and params only for vehicle radio link %d", iVehicleLinkId+1);

   return true;
}

void reasign_radio_links(bool bSilent)
{
   log_line("ROUTER REASIGN LINKS START -----------------------------------------------------");
   log_line("Router: Reasigning radio interfaces to radio links (close, reasign, reopen radio interfaces)...");

   if ( ! bSilent )
      send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_START, 0);

   radio_rx_stop_rx_thread();
   radio_links_close_rxtx_radio_interfaces();

   if ( g_pControllerSettings->iRadioTxUsesPPCAP )
      radio_set_use_pcap_for_tx(1);
   else
      radio_set_use_pcap_for_tx(0);

   if ( g_pControllerSettings->iRadioBypassSocketBuffers )
      radio_set_bypass_socket_buffers(1);
   else
      radio_set_bypass_socket_buffers(0);

   _compute_radio_interfaces_assignment();
   links_set_cards_frequencies_and_params(-1);
   radio_links_open_rxtx_radio_interfaces();

   radio_rx_start_rx_thread(&g_SM_RadioStats, (int)g_bSearching, g_uAcceptedFirmwareType);

   if ( ! bSilent )
      send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_END, 0);

   log_line("Router: Reasigning radio interfaces to radio links complete.");
   log_line("ROUTER REASIGN LINKS END -------------------------------------------------------");
}

void broadcast_router_ready()
{
   send_message_to_central(PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY, 0, true);
   log_line("Broadcasted that router is ready.");
}

void send_message_to_central(u32 uPacketType, u32 uParam, bool bTelemetryToo)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, uPacketType, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = uParam;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   radio_packet_compute_crc(buffer, PH.total_length);
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_ipc_send_central_error(buffer, PH.total_length);
   else
      log_line("Sent message %s to central.", str_get_packet_type(uPacketType));
   if ( bTelemetryToo )
   if ( -1 != g_fIPCToTelemetry )
      ruby_ipc_channel_send_message(g_fIPCToTelemetry, buffer, PH.total_length);
}

void send_adaptive_video_paused_to_central(u32 uVehicleId, bool bPaused)
{
   if ( -1 == g_fIPCToCentral )
      return;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, STREAM_ID_DATA);
   PH.vehicle_id_src = uVehicleId;
   PH.vehicle_id_dest = bPaused?1:0;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   radio_packet_compute_crc(buffer, PH.total_length);
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_ipc_send_central_error(buffer, PH.total_length);
   else
      log_line("Sent message %s to central, VID: %d, adaptive is %s", str_get_packet_type(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE), uVehicleId, bPaused?"paused":"resumed");
}

void _check_for_atheros_datarate_change_command_to_vehicle(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_COMMANDS )
      return;

   if ( pPH->packet_type != PACKET_TYPE_COMMAND )
      return;

   int iParamsLength = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command);
   t_packet_header_command* pPHC = (t_packet_header_command*)(pPacketBuffer + sizeof(t_packet_header));

   if ( (pPHC->command_type != COMMAND_ID_SET_RADIO_LINK_FLAGS) ||
         (iParamsLength != (int)(2*sizeof(u32)+2*sizeof(int))) ) 
      return;

   u32* pInfo = (u32*)(pPacketBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
   u32 linkIndex = *pInfo;
   pInfo++;
   u32 linkFlags = *pInfo;
   pInfo++;
   int* piInfo = (int*)pInfo;
   int datarateVideo = *piInfo;
   piInfo++;
   int datarateData = *piInfo;

   log_line("Intercepted Set Radio Links Flags command to vehicle: Link %d, Link flags: %s, Datarate %d/%d", linkIndex+1, str_get_radio_frame_flags_description2(linkFlags), datarateVideo, datarateData);
   g_TimeLastSetRadioFlagsCommandSent = g_TimeNow;
   g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand = linkIndex;
   g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand = datarateData;
}


void _process_and_send_packet(u8* pPacketBuffer, int iPacketLength, u32 uTimeAdded)
{
   _check_for_atheros_datarate_change_command_to_vehicle(pPacketBuffer);

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   pPH->vehicle_id_src = g_uControllerId;

   preprocess_radio_out_packet(pPacketBuffer, iPacketLength);

   int send_count = 1;

   if ( (pPH->packet_type == PACKET_TYPE_VEHICLE_RECORDING) )
      send_count = 2;
  
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      // Store info about this ping
      g_TimeNow = get_current_timestamp_ms();
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != 0 )
         {
            u8 uLocalRadioLinkId = 0;
            memcpy(&uLocalRadioLinkId, pPacketBuffer+sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
            if ( uLocalRadioLinkId < MAX_RADIO_INTERFACES )
               g_State.vehiclesRuntimeInfo[i].uTimeLastPingSentToVehicleOnLocalRadioLinks[uLocalRadioLinkId] = g_TimeNow;
         }
      }
   }

   if ( 0 != uTimeAdded )
   {
      //u32 uTime = get_current_timestamp_micros();
      //log_line("DBG queue and send packet took %u microsec for packet %s", uTime - uTimeAdded, str_get_packet_type(pPH->packet_type));
   }
   static int s_iTxErrorsCount = 0;
   for( int i=0; i<send_count; i++ )
   {
      if ( send_packet_to_radio_interfaces(pPacketBuffer, iPacketLength, -1, send_count-1, 11) < 0 )
          s_iTxErrorsCount++;
      else
          s_iTxErrorsCount = 0;
   }
   if ( s_iTxErrorsCount > 10 )
   {
      send_alarm_to_central(ALARM_ID_RADIO_INTERFACE_DOWN, 0xFF, 0);
      radio_links_reinit_radio_interfaces();
      s_iTxErrorsCount = 0;
   }
}

void _process_and_send_packets_individually(t_packet_queue* pRadioQueue)
{
   if ( NULL == pRadioQueue )
      return;
   if ( g_bQuit || g_bSearching || (NULL == g_pCurrentModel) || (g_pCurrentModel->is_spectator) )
   {
      // Empty queue
      packets_queue_init(pRadioQueue);
      return;
   }

   while ( packets_queue_has_packets(pRadioQueue) )
   {
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int iPacketLength = -1;
      u32 uTimeAdded = 0;
      u8* pPacketBuffer = packets_queue_pop_packet_and_time(pRadioQueue, &iPacketLength, &uTimeAdded);
      if ( NULL == pPacketBuffer || -1 == iPacketLength )
         break;

      _process_and_send_packet(pPacketBuffer, iPacketLength, uTimeAdded);
   }
}

void _preprocess_central_ipc_packet(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   if ( pPH->total_length >= (int)sizeof(t_packet_header) + 2*(int)sizeof(u8) )
   {
      // uCommand is second byte after header
      u8 uCommand = pPacketBuffer[sizeof(t_packet_header)+1];
      log_line("Received negociate radio packet from central. Negociate command: %d", uCommand);
   }
}

void _read_ipc_pipes(u32 uTimeNow)
{
   s_uTimeLastTryReadIPCMessages = uTimeNow;
   int maxToRead = 10;
   int maxPacketsToRead = maxToRead;

   maxPacketsToRead += DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(g_fIPCFromCentral, s_PipeBufferCommands, &s_PipeBufferCommandsPos, s_BufferCommands)) )
   {
      maxPacketsToRead--;

      _preprocess_central_ipc_packet(s_BufferCommands);
      t_packet_header* pPH = (t_packet_header*)s_BufferCommands;
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferCommands); 
      else
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, s_BufferCommands); 
   }
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from central msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(g_fIPCFromTelemetry, s_PipeBufferTelemetryUplink, &s_PipeBufferTelemetryUplinkPos, s_BufferMessageFromTelemetry)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferMessageFromTelemetry;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferMessageFromTelemetry); 
      else
      {
         if ( ! isPairingDoneWithVehicle(pPH->vehicle_id_dest) )
            continue;
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, s_BufferMessageFromTelemetry);
      }
   }
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from telemetry msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(g_fIPCFromRC, s_PipeBufferRCUplink, &s_PipeBufferRCUplinkPos, s_BufferRCUplink)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferRCUplink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferRCUplink); 
      else
      {
         if ( ! isPairingDoneWithVehicle(pPH->vehicle_id_dest) )
            continue;
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, s_BufferRCUplink);
      }
   }
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from RC msgqueue.", maxToRead - maxPacketsToRead);
}

void init_shared_memory_objects()
{
   g_TimeNow = get_current_timestamp_ms();
   
   g_pSMControllerRTInfo = controller_rt_info_open_for_write();
   if ( NULL == g_pSMControllerRTInfo )
      log_softerror_and_alarm("Failed to open shared mem to controller runtime info for writing: %s", SHARED_MEM_CONTROLLER_RUNTIME_INFO);
   else
      log_line("Opened shared mem to controller runtime info for writing.");

   g_pSMControllerDebugVideoRTInfo = controller_debug_video_rt_info_open_for_write();
   if ( NULL == g_pSMControllerDebugVideoRTInfo )
      log_softerror_and_alarm("Failed to open shared mem to controller debug video runtime info for writing: %s", SHARED_MEM_CONTROLLER_DEBUG_VIDEO_RUNTIME_INFO);
   else
      log_line("Opened shared mem to controller debug video runtime info for writing.");

   g_pSM_RadioStats = shared_mem_radio_stats_open_for_write();
   if ( NULL == g_pSM_RadioStats )
      log_softerror_and_alarm("Failed to open radio stats shared memory for write.");
   else
      log_line("Opened radio stats shared memory for write: success.");


   g_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_write();

   if ( NULL == g_pSM_HistoryRxStats )
      log_softerror_and_alarm("Failed to open history radio rx stats shared memory for write.");
   else
      shared_mem_radio_stats_rx_hist_reset(&g_SM_HistoryRxStats);
  
   if ( NULL == g_pCurrentModel )
      radio_stats_reset(&g_SM_RadioStats, DEFAULT_OSD_RADIO_GRAPH_REFRESH_PERIOD_MS);
   else
      radio_stats_reset(&g_SM_RadioStats, g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs);

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

   g_pSM_VideoDecodeStats = shared_mem_video_stream_stats_rx_processors_open_for_write();
   if ( NULL == g_pSM_VideoDecodeStats )
      log_softerror_and_alarm("Failed to open video decoder stats shared memory for write.");
   else
      log_line("Opened video decoder stats shared memory for write: success.");
   memset((u8*)&g_SM_VideoDecodeStats, 0, sizeof(shared_mem_video_stream_stats_rx_processors));

   g_pSM_RadioRxQueueInfo = shared_mem_radio_rx_queue_info_open_for_write();
   if ( NULL == g_pSM_RadioRxQueueInfo )
      log_softerror_and_alarm("Failed to open radio rx queue info shared memory for write.");
   else
      log_line("Opened radio rx queue info shared memory for write: success.");
   memset((u8*)&g_SM_RadioRxQueueInfo, 0, sizeof(shared_mem_radio_rx_queue_info));
   g_SM_RadioRxQueueInfo.uMeasureIntervalMs = 100;

   g_pSM_VideoFramesStatsOutput = shared_mem_video_frames_stats_open_for_write();
   if ( NULL == g_pSM_VideoFramesStatsOutput )
      log_softerror_and_alarm("Failed to open shared mem video info stats output for writing: %s", SHARED_MEM_VIDEO_FRAMES_STATS);
   else
      log_line("Opened shared mem video info stats stats for writing.");

   //g_pSM_VideoInfoStatsRadioIn = shared_mem_video_frames_stats_radio_in_open_for_write();
   //if ( NULL == g_pSM_VideoInfoStatsRadioIn )
   //   log_softerror_and_alarm("Failed to open shared mem video info radio in stats for writing: %s", SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_IN);
   //else
   //   log_line("Opened shared mem video info radio in stats stats for writing.");

   memset(&g_SM_VideoFramesStatsOutput, 0, sizeof(shared_mem_video_frames_stats));
   //memset(&g_SM_VideoInfoStatsRadioIn, 0, sizeof(shared_mem_video_frames_stats));

   g_pSM_RouterVehiclesRuntimeInfo = shared_mem_router_vehicles_runtime_info_open_for_write();
   if ( NULL == g_pSM_RouterVehiclesRuntimeInfo )
      log_softerror_and_alarm("Failed to open shared mem controller vehicles runtime info for writing: %s", SHARED_MEM_CONTROLLER_ROUTER_VEHICLES_INFO);
   else
      log_line("Opened shared mem controller adaptive video info for writing.");

   g_TimeNow = get_current_timestamp_ms();

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      resetVehicleRuntimeInfo(i);

   if ( NULL != g_pCurrentModel )
   {
      g_State.vehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->uVehicleId;
   }

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_ROUTER_RX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for video rx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_ROUTER_RX);
   else
      log_line("Opened shared mem for video rx process watchdog stats for writing.");

   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->alarmFlags = 0;
      g_pProcessStats->alarmTime = 0;
   }

   g_pProcessStatsCentral = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_CENTRAL);
   if ( NULL == g_pProcessStatsCentral )
      log_softerror_and_alarm("Failed to open shared mem for ruby_central process watchdog for reading: %s", SHARED_MEM_WATCHDOG_CENTRAL);
   else
      log_line("Opened shared mem for ruby_centrall process watchdog for reading.");

}

int open_pipes()
{
   g_fIPCFromRC = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( g_fIPCFromRC < 0 )
      return -1;

   g_fIPCToRC = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( g_fIPCToRC < 0 )
      return -1;
   
   g_fIPCFromCentral = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER);
   if ( g_fIPCFromCentral < 0 )
      return -1;

   g_fIPCToCentral = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL);
   if ( g_fIPCToCentral < 0 )
      return -1;
   
   g_fIPCToTelemetry = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( g_fIPCToTelemetry < 0 )
      return -1;
   
   g_fIPCFromTelemetry = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( g_fIPCFromTelemetry < 0 )
      return -1;
   
   if ( NULL == g_pCurrentModel || (!g_pCurrentModel->audio_params.enabled) )
   {
      log_line("Audio is disabled on current vehicle.");
      return 0;
   }
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->audio_params.has_audio_device) )
   {
      log_line("No audio capture device on current vehicle.");
      return 0;
   }

   return 0;
}
     
int _consume_ipc_messages()
{
   int iConsumed = 0;
   int iMaxToConsume = 20;

   while ( packets_queue_has_packets(&s_QueueControlPackets) && (iMaxToConsume > 0) )
   {
      iMaxToConsume--;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueControlPackets, &length);
      if ( (NULL == pBuffer) || (-1 == length) )
         break;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      log_line("Received local packet from central, type: %s", str_get_packet_type(pPH->packet_type));
      process_local_control_packet(pBuffer);
      log_line("Done handling received local packet from central, type: %s", str_get_packet_type(pPH->packet_type));
      iConsumed++;
   }
   return iConsumed;
}

int _try_read_consume_rx_packets(bool bHighPriority, int iCountMax, u32 uTimeoutMicrosec)
{
   int iCountConsumed = 0;
   int iPacketLength = 0;
   int iPacketIsShort = 0;
   int iRadioInterfaceIndex = 0;
   u8* pPacket = NULL;

   while ( (iCountConsumed < iCountMax) && (!g_bQuit) )
   {
      if ( bHighPriority )
         pPacket = radio_rx_wait_get_next_received_high_prio_packet(uTimeoutMicrosec, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
      else
         pPacket = radio_rx_wait_get_next_received_reg_prio_packet(uTimeoutMicrosec, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);

      if ( NULL == pPacket )
         break;
      iCountConsumed++;
      if ( g_bQuit )
         break;
      
      g_SMControllerRTInfo.uRxLastDeltaTime[g_SMControllerRTInfo.iCurrentIndex][0] = g_TimeNow - g_SMControllerRTInfo.uCurrentSliceStartTime;
      process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);      
      shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);
      g_SMControllerRTInfo.uRxProcessedPackets[g_SMControllerRTInfo.iCurrentIndex]++;
   }

   return iCountConsumed;
}

void _check_send_packets(bool bDoTxSync)
{
   bool bSendNow = false;

   type_global_state_vehicle_runtime_info* pRTInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
   if ( (NULL == pRTInfo) || (! pRTInfo->bIsPairingDone) )
      bSendNow = true;
   else if ( (!bDoTxSync) || g_bUpdateInProgress || (!g_pCurrentModel->hasCamera()) || (g_TimeNow > s_QueueRadioPacketsRegPrio.timeFirstPacket + 55) )
      bSendNow = true;
   else if ( router_is_eof() )
      bSendNow = true;

   if ( ! bSendNow )
      return;

   _process_and_send_packets_individually(&s_QueueRadioPacketsHighPrio);
   _process_and_send_packets_individually(&s_QueueRadioPacketsRegPrio);
}

void _main_loop_searching();
void _main_loop_simple(bool bDoBasicTxSync);
void _main_loop_adv_sync();

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   radio_rx_mark_quit();
   g_bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
         
   log_init("Router");
   log_arguments(argc, argv);
   log_line_forced_to_file("Linux mem page size: %d bytes", getpagesize());
   
   hardware_detectBoardAndSystemType();

   g_bSearching = false;
   g_uSearchFrequency = 0;
   if ( argc >= 3 )
   if ( strcmp(argv[1], "-search") == 0 )
   {
      g_bSearching = true;
      g_uSearchFrequency = atoi(argv[2]);
   }
   
   if ( argc >= 5 )
   if ( strcmp(argv[3], "-firmware") == 0 )
   {
      g_uAcceptedFirmwareType = (u32) atoi(argv[4]);
   }

   if ( argc >= 8 )
   if ( strcmp(argv[1], "-search") == 0 )
   if ( strcmp(argv[3], "-sik") == 0 )
   {
      g_bSearching = true;
      g_uSearchFrequency = atoi(argv[2]);
      s_iSearchSikAirRate = atoi(argv[4]);
      s_iSearchSikECC = atoi(argv[5]);
      s_iSearchSikLBT = atoi(argv[6]);
      s_iSearchSikMCSTR = atoi(argv[7]);

      log_line ("Searching in Sik Mode: air rate: %d, ECC/LBT/MCSTR: %d/%d/%d",
         s_iSearchSikAirRate, s_iSearchSikECC, s_iSearchSikLBT, s_iSearchSikMCSTR);
   }

   g_bDebugState = false;

   if ( argc >= 1 )
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebugState = true;

   if ( access(CONFIG_FILENAME_DEBUG, R_OK) != -1 )
      g_bDebugState = true;
   if ( g_bDebugState )
      log_line("Starting in debug mode.");

   if ( g_bSearching )
      log_line("Launched router in search mode, search frequency: %s, search firmware type: %s", str_format_frequency(g_uSearchFrequency), str_format_firmware_type(g_uAcceptedFirmwareType));


   int iCPUCoresCount = hw_procs_get_cpu_count();
   log_line("Detected CPU with %d cores.", iCPUCoresCount);   

   utils_log_radio_packets_sizes();
   radio_init_link_structures();
   radio_enable_crc_gen(1);
   hardware_enumerate_radio_interfaces(); 

   init_radio_rx_structures();
   reset_sik_state_info(&g_SiKRadiosState);

   load_Preferences();   
   load_ControllerSettings();
   load_ControllerInterfacesSettings();
   controllerRadioInterfacesLogInfo();

   g_pControllerSettings = get_ControllerSettings();
   g_pControllerInterfaces = get_ControllerInterfacesSettings();
   Preferences* pP = get_Preferences();   
   if ( pP->nLogLevel != 0 )
      log_only_errors();
 
   if ( g_pControllerSettings->iCoresAdjustment )
   {
      hw_set_current_thread_affinity("station", CORE_AFFINITY_STATION_GS, CORE_AFFINITY_STATION_GS);
      radio_rx_set_cpu_affinity(CORE_AFFINITY_RX_RADIO);
   }
   if ( NULL != g_pControllerSettings )
      radio_rx_set_timeout_interval(g_pControllerSettings->iDevRxLoopTimeout);
     
   if ( g_pControllerSettings->iRadioBypassSocketBuffers )
      radio_set_bypass_socket_buffers(1);
   else
      radio_set_bypass_socket_buffers(0);

   if ( g_pControllerSettings->iRadioTxUsesPPCAP )
      radio_set_use_pcap_for_tx(1);
   else
      radio_set_use_pcap_for_tx(0);

   g_uControllerId = controller_utils_getControllerId();
   log_line("Controller UID: %u", g_uControllerId);

   g_pCurrentModel = NULL;
   if ( ! g_bSearching )
   {
      loadAllModels();

      g_pCurrentModel = getCurrentModel();
      if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
         lpp(NULL, 0);
      g_pCurrentModel->logVehicleRadioInfo();

      g_uAcceptedFirmwareType = g_pCurrentModel->getVehicleFirmwareType();
   }

   if ( NULL != g_pCurrentModel )
   if ( g_pControllerSettings->iDeveloperMode )
   {
      radio_tx_set_dev_mode(1);
      radio_rx_set_dev_mode(1);
      radio_set_debug_flag(1);
   }

   packet_utils_init();

   if ( (NULL != g_pControllerSettings) && g_pControllerSettings->iPrioritiesAdjustment )
   {
      hw_set_priority_current_proc(g_pControllerSettings->iThreadPriorityRouter);
      radio_rx_set_custom_thread_raw_priority(g_pControllerSettings->iThreadPriorityRadioRx);
      radio_tx_set_custom_thread_raw_priority(g_pControllerSettings->iThreadPriorityRadioTx);
   }

   controller_rt_info_init(&g_SMControllerRTInfo);
   init_shared_memory_objects();

   log_line("Init shared mem objects: done");

   if ( -1 == open_pipes() )
   {
      g_bQuit = true;
      packet_utils_uninit();
      radio_link_cleanup();
      log_error_and_alarm("Failed to open required pipes. Exit.");
      return -1;
   }
      
   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   if ( NULL != pP )
      delayMs = (u32) pP->iDebugWiFiChangeDelay;
   if ( delayMs<1 || delayMs > 200 )
      delayMs = DEFAULT_DELAY_WIFI_CHANGE;

   hardware_sleep_ms(delayMs);

   g_SM_RadioStats.countLocalRadioInterfaces = hardware_get_radio_interfaces_count();
  
   if ( g_bSearching )
   {
      if ( s_iSearchSikAirRate > 0 )
         links_set_cards_frequencies_for_search(g_uSearchFrequency, true, s_iSearchSikAirRate, s_iSearchSikECC, s_iSearchSikLBT, s_iSearchSikMCSTR );
      else
         links_set_cards_frequencies_for_search(g_uSearchFrequency, false, -1,-1,-1,-1 );
      hardware_save_radio_info();
      radio_links_open_rxtx_radio_interfaces_for_search(g_uSearchFrequency);
   }
   else
   {
      _compute_radio_interfaces_assignment();

      
      for( int iRadioLink=0; iRadioLink<g_pCurrentModel->radioLinksParams.links_count; iRadioLink++ )
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( ! pRadioHWInfo->isConfigurable )
            continue;
         if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
              (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
            continue;

         int nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         nRateTx = g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink];
         if ( (0 == nRateTx) || (-100 == nRateTx) )
         {
            if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
               nRateTx = -1;
            else
               nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         }
         update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }

      links_set_cards_frequencies_and_params(-1);
      radio_links_open_rxtx_radio_interfaces();
   }

   packets_queue_init(&s_QueueRadioPacketsHighPrio);
   packets_queue_init(&s_QueueRadioPacketsRegPrio);
   packets_queue_init(&s_QueueControlPackets);

   log_line("IPC Queues Init Complete.");
   
   g_TimeStart = get_current_timestamp_ms();

   if ( NULL != g_pCurrentModel )
   {
      char szBuffF[128];
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         str_get_radio_frame_flags_description(g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[i], szBuffF);
         log_line("Radio frame flags for radio interface %d: %u, %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[i], szBuffF);
      }
   }

   g_bFirstModelPairingDone = false;
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FIRST_PAIRING_DONE); 
   if ( access(szFile, R_OK) != -1 )
      g_bFirstModelPairingDone = true;

   if ( g_bSearching )
      log_line("Router started in search mode");
   else if ( g_bFirstModelPairingDone )
      log_line("Router started with a valid user controll/spectator model (first model pairing was already completed)");
   else
      log_line("Router started with the default model (first model pairing was never completed)");

   adaptive_video_init();
   video_processors_init();
   if ( ! g_bSearching )
   if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
   if ( ! is_audio_processing_started() )
      init_processing_audio();

   load_CorePlugins(0);

   radio_duplicate_detection_init();
   radio_rx_start_rx_thread(&g_SM_RadioStats, (int)g_bSearching, g_uAcceptedFirmwareType);
   
   log_line("Broadcasting that router is ready.");
   broadcast_router_ready();

   if ( radio_links_has_failed_interfaces() >= 0 )
      _broadcast_radio_interface_init_failed(radio_links_has_failed_interfaces());

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->uExtraFlags & RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD )
         send_alarm_to_central(ALARM_ID_FIRMWARE_OLD, i, 0);
   }

   log_line("");
   log_line("");
   log_line("----------------------------------------------");
   log_line("         Started all ok. Running now.");
   log_line("----------------------------------------------");
   log_line("");
   log_line("");

   // -----------------------------------------------------------
   // Main loop here

   if ( g_bSearching )
      log_line("Running main loop for searching");
   else
      log_line("Running main loop for sync type: %d", g_pCurrentModel->rxtx_sync_type);

   int iLoopTimeErrorsCount = 0;
   g_TimeNow = get_current_timestamp_ms();
   u32 uLastLoopTime = g_TimeNow;
   g_pProcessStats->uLoopTimer1 = g_pProcessStats->uLoopTimer2 = g_TimeNow;

   while ( !g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->uLoopCounter++;
      g_pProcessStats->uLoopSubStep = 0;
      u32 uMaxLoopTime = 15;
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->video_params.iVideoFPS > 0) )
         uMaxLoopTime = 800 / g_pCurrentModel->video_params.iVideoFPS;
      if ( g_TimeNow - uLastLoopTime >= uMaxLoopTime )
      {
         iLoopTimeErrorsCount++;
         log_softerror_and_alarm("Main loop took too long (max %u ms, fps: %d): %u ms (=%u + %u + %u) cnt2: %u, cnt3: %u",
            uMaxLoopTime, (g_pCurrentModel!= NULL)?g_pCurrentModel->video_params.iVideoFPS:-1,
            g_TimeNow - uLastLoopTime,
            g_pProcessStats->uLoopTimer1 - uLastLoopTime,
            g_pProcessStats->uLoopTimer2 - g_pProcessStats->uLoopTimer1,
            g_TimeNow - g_pProcessStats->uLoopTimer2,
            g_pProcessStats->uLoopCounter2, g_pProcessStats->uLoopCounter3);
      }
      g_uLoopCounter++;

      if ( (g_TimeNow - uLastLoopTime >= 70) && (! g_bSearching) )
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
  
      uLastLoopTime = g_TimeNow;

      if ( g_bSearching )
      {
         static u32 s_uTimeLastSearchAliveLog = 0;
         if ( g_TimeNow > s_uTimeLastSearchAliveLog + 500 )
         {
            s_uTimeLastSearchAliveLog = g_TimeNow;
            log_line("Still in search mode, all active...");
         }
      }
      
      if ( g_bSearching )
         _main_loop_searching();
      else if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_ADV )
         _main_loop_adv_sync();
      else if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_BASIC )
         _main_loop_simple(true);
      else if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_NONE )
         _main_loop_simple(false);
      else
         _main_loop_simple(false);

      if ( g_bQuit )
         break;
   }

   // End main loop
   //------------------------------------------------------------


   log_line("Stopping...");

   packet_utils_uninit();
   radio_rx_stop_rx_thread();
   radio_link_cleanup();
   unload_CorePlugins();

   video_processors_cleanup();
   if ( is_audio_processing_started() )
      uninit_processing_audio();

   controller_rt_info_close(g_pSMControllerRTInfo);
   controller_debug_video_rt_info_close(g_pSMControllerDebugVideoRTInfo);
   
   shared_mem_radio_stats_rx_hist_close(g_pSM_HistoryRxStats);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_RX, g_pProcessStats);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, g_pProcessStatsCentral);
   shared_mem_video_stream_stats_rx_processors_close(g_pSM_VideoDecodeStats);
   shared_mem_radio_rx_queue_info_close(g_pSM_RadioRxQueueInfo);
   g_pSM_RadioRxQueueInfo = NULL;
   shared_mem_radio_stats_close(g_pSM_RadioStats);
   shared_mem_video_frames_stats_close(g_pSM_VideoFramesStatsOutput);
   //shared_mem_video_frames_stats_radio_in_close(g_pSM_VideoInfoStatsRadioIn);
   shared_mem_router_vehicles_runtime_info_close(g_pSM_RouterVehiclesRuntimeInfo);

   shared_mem_ctrl_ping_stats_info_close(g_pSMDbgPingStats);
   radio_links_close_rxtx_radio_interfaces(); 
  
   ruby_close_ipc_channel(g_fIPCFromCentral);
   ruby_close_ipc_channel(g_fIPCToCentral);
   ruby_close_ipc_channel(g_fIPCFromTelemetry);
   ruby_close_ipc_channel(g_fIPCToTelemetry);
   ruby_close_ipc_channel(g_fIPCFromRC);
   ruby_close_ipc_channel(g_fIPCToRC);

   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( ! (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) )
   {
      g_pCurrentModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
      saveControllerModel(g_pCurrentModel);
   }

   g_fIPCFromCentral = -1;
   g_fIPCToCentral = -1;
   g_fIPCToTelemetry = -1;
   g_fIPCFromTelemetry = -1;
   g_fIPCToRC = -1;
   g_fIPCFromRC = -1;
   log_line("---------------------------");
   log_line("Execution Finished. Exit.");
   log_line("---------------------------");
 
   return 0;
}

void video_processors_init()
{
   if ( ! g_bSearching )
   {
      rx_video_output_init();
      
      rx_video_output_start_video_streamer();

      #ifdef HW_PLATFORM_RASPBERRY
      rx_video_output_enable_streamer_output();
      #endif
      #ifdef HW_PLATFORM_RADXA
      //rx_video_output_enable_local_player_udp_output();
      rx_video_output_enable_streamer_output();
      #endif

      log_line("Do one time init of processors rx video...");
      ProcessorRxVideo::oneTimeInit();
   }
}

void video_processors_cleanup()
{
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      {
         g_pVideoProcessorRxList[i]->uninit();
         delete g_pVideoProcessorRxList[i];
         g_pVideoProcessorRxList[i] = NULL;
      }
   }

   if ( ! g_bSearching )
      rx_video_output_uninit();
}

static u32 uMaxLoopTime = DEFAULT_MAX_LOOP_TIME_MILISECONDS;

void _main_loop_try_recevive_data()
{
   g_pProcessStats->uLoopCounter2 = g_pProcessStats->uLoopCounter3 = 0;

   u32 uTimeStart = g_TimeNow;
   int iTotalConsumedHighPriority = 0;
   int iTotalConsumedRegPriority = 0;
   int iTotalConsumeLoops = 0;
   int iMaxCountToConsumeOnce = 100;
   u32 uReadTimeoutMicrosVideo = 200;
   u32 uReadTimeoutMicrosHigh = 200;

   do
   {
      s_uTimeLastCheckForVideoPackets = g_TimeNow;
      iTotalConsumeLoops++;

      //---------------------------------------------
      // Check and process first:
      //  Retransmissions received and pings received and other high priority radio messages
      //  Video data and other data
      int iConsumedHigh = _try_read_consume_rx_packets(true, iMaxCountToConsumeOnce, uReadTimeoutMicrosHigh);
      int iConsumedReg = _try_read_consume_rx_packets(false, iMaxCountToConsumeOnce, uReadTimeoutMicrosVideo);
      iTotalConsumedHighPriority += iConsumedHigh;
      iTotalConsumedRegPriority += iConsumedReg;
      g_pProcessStats->uLoopCounter2 += iConsumedHigh;
      g_pProcessStats->uLoopCounter3 += iConsumedReg;
      if ( (0 == iConsumedReg) && (0 == iConsumedHigh) )
         break;

      if ( 0 != iConsumedReg )
      {
         uReadTimeoutMicrosVideo = 1000;
         uReadTimeoutMicrosHigh = 0;
      }
      else
      {
         uReadTimeoutMicrosVideo = 0;
         uReadTimeoutMicrosHigh = 500;
      }
   } while ( (!g_bQuit) && (g_TimeNow < uTimeStart+15) );

   _router_update_frame_eof();

   static int s_iCountConsumePacketsLogError = 0;
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->video_params.iVideoFPS > 0) )
   if ( g_TimeNow >= uTimeStart + 1000/g_pCurrentModel->video_params.iVideoFPS )
   {
      //if ( 0 == (s_iCountConsumePacketsLogError % 10) )
         log_softerror_and_alarm("Reading and consuming %d video/reg packets and %d high priority packets in %d consume loops took too long (fps %d): %u ms", iTotalConsumedRegPriority, iTotalConsumedHighPriority, iTotalConsumeLoops, g_pCurrentModel->video_params.iVideoFPS, g_TimeNow - uTimeStart);
      s_iCountConsumePacketsLogError++;
   }

}

void _main_loop_searching()
{
   g_pProcessStats->uLoopTimer1 = g_TimeNow = get_current_timestamp_ms();

   _main_loop_try_recevive_data();

   g_TimeNow = g_pProcessStats->uLoopTimer2 = get_current_timestamp_ms();

   router_periodic_loop();

   _read_ipc_pipes(g_TimeNow);
   _consume_ipc_messages();

   s_iCountCPULoopOverflows = 0;
   u32 uTimeNow = get_current_timestamp_ms();
   if ( NULL != g_pProcessStats )
   {
      if ( g_pProcessStats->uMaxLoopTimeMs < uTimeNow - g_TimeNow )
         g_pProcessStats->uMaxLoopTimeMs = uTimeNow - g_TimeNow;
      g_pProcessStats->uTotalLoopTime += uTimeNow - g_TimeNow;
      if ( 0 != g_pProcessStats->uLoopCounter )
         g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
   }   
}


void _main_loop_simple(bool bDoBasicTxSync)
{
   g_TimeNow = get_current_timestamp_ms();

   u32 tTime0 = g_TimeNow;

   _main_loop_try_recevive_data();

   g_TimeNow = get_current_timestamp_ms();
   g_pProcessStats->uLoopTimer1 = g_TimeNow;
   u32 tTime1 = g_TimeNow;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( g_pVideoProcessorRxList[i] != NULL )
         g_pVideoProcessorRxList[i]->periodicLoopProcessor(g_TimeNow, false);
   }
   
   if ( controller_rt_info_will_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
      adaptive_video_periodic_loop(false);

   router_periodic_loop();

   _read_ipc_pipes(g_TimeNow);

   _consume_ipc_messages();

   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->hasCamera() )
      rx_video_output_periodic_loop();
   
   g_TimeNow = g_pProcessStats->uLoopTimer2 = get_current_timestamp_ms();
   u32 tTime2 = g_TimeNow;

   if ( controller_rt_info_will_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         memcpy((u8*)&(g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[g_SMControllerRTInfo.iCurrentIndex][i]), (u8*)&(g_SM_RadioStats.radio_interfaces[i].signalInfo.signalInfoVideo), sizeof(type_runtime_radio_rx_signal_info));
         memcpy((u8*)&(g_SMControllerRTInfo.radioInterfacesSignalInfoData[g_SMControllerRTInfo.iCurrentIndex][i]), (u8*)&(g_SM_RadioStats.radio_interfaces[i].signalInfo.signalInfoData), sizeof(type_runtime_radio_rx_signal_info));
      }
      radio_rx_reset_signal_info();
      radio_stats_reset_rx_signal_info(&g_SM_RadioStats);
   }

   g_TimeNow = get_current_timestamp_ms();
   u32 tTime3 = g_TimeNow;

   _check_send_packets(bDoBasicTxSync);

   g_TimeNow = get_current_timestamp_ms();
   u32 tTime4 = g_TimeNow;
   if ( (g_TimeNow > g_TimeStart + 10000) && (tTime4 > tTime0 + uMaxLoopTime) )
   {
      log_softerror_and_alarm("Router %s main loop took too long to complete (loop count: %u) (%d milisec: %u + %u + %u + %u) recording: %s, repeat count: %u!!!",
        bDoBasicTxSync?"basic":"simple", g_pProcessStats->uLoopCounter, tTime4 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3,
        rx_video_is_recording()?"yes":"no", s_iCountCPULoopOverflows+1);
      if ( ! test_link_is_in_progress() )
      if ( g_pControllerSettings->iDeveloperMode )
      if ( g_TimeNow > rx_video_recording_get_last_start_stop_time() + 2000 )
      {
         s_iCountCPULoopOverflows++;
         if ( rx_video_is_recording() )
         {
            if ( (tTime4 > tTime0 + uMaxLoopTime*2) )
            {
               if ( s_iCountCPULoopOverflows >= 1 )
               if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
                  send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING,(tTime4-tTime0), 0);

               if ( tTime4 >= tTime0 + 300 )
               if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
                  send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING,(tTime4-tTime0)<<16, 0);
            }
         }
         else
         {
            if ( s_iCountCPULoopOverflows > 5 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime4-tTime0), 0);

            if ( tTime4 >= tTime0 + 300 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime4-tTime0)<<16, 0);
         }
      }
   }
   else
   {
      s_iCountCPULoopOverflows = 0;
   }

   if ( controller_rt_info_check_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
   {
      radio_rx_set_packet_counter_output(&(g_SMControllerRTInfo.uRxHighPriorityPackets[g_SMControllerRTInfo.iCurrentIndex][0]),
          &(g_SMControllerRTInfo.uRxDataPackets[g_SMControllerRTInfo.iCurrentIndex][0]), &(g_SMControllerRTInfo.uRxMissingPackets[g_SMControllerRTInfo.iCurrentIndex][0]), &(g_SMControllerRTInfo.uRxMissingPacketsMaxGap[g_SMControllerRTInfo.iCurrentIndex][0]));
   
      if ( g_pControllerSettings->iDeveloperMode )
         radio_rx_set_air_gap_track_output(&(g_SMControllerRTInfo.uRxMaxAirgapSlots[g_SMControllerRTInfo.iCurrentIndex]));
   }

   if ( NULL != g_pProcessStats )
   {
      if ( g_pProcessStats->uMaxLoopTimeMs < tTime4 - tTime0 )
         g_pProcessStats->uMaxLoopTimeMs = tTime4 - tTime0;
      g_pProcessStats->uTotalLoopTime += tTime4 - tTime0;
      if ( 0 != g_pProcessStats->uLoopCounter )
         g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
   }
}

void _main_loop_adv_sync()
{
   g_TimeNow = get_current_timestamp_ms();
   u32 tTime0 = g_TimeNow;

   _main_loop_try_recevive_data();

   // To fix
   /*
   if ( bEndOfVideoFrameDetected && (!g_pCurrentModel->isVideoLinkFixedOneWay()))
   {
      t_packet_header_compressed PHC;
      radio_packet_compressed_init(&PHC, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_ACK);
      PHC.vehicle_id_src = g_uControllerId;
      PHC.vehicle_id_dest = g_pCurrentModel->uVehicleId;
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PHC, sizeof(t_packet_header_compressed));
      send_packet_to_radio_interfaces(packet, PHC.total_length, -1, 0, 501);
   }
   */

   g_TimeNow = get_current_timestamp_ms();
   bool bAnyVehicleMustSyncNow = false;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_State.vehiclesRuntimeInfo[i].uVehicleId == 0) || (g_State.vehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;
      Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 28);

      if ( (NULL == pModel) || pModel->isVideoLinkFixedOneWay() || (! pModel->hasCamera()) )
         continue;

      ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 0);
      if ( (NULL != pProcessorRxVideo) && (NULL != pProcessorRxVideo->m_pVideoRxBuffer) )
      {
         bool bSyncNow = false;
         if ( g_TimeNow >= pProcessorRxVideo->getLastestVideoPacketReceiveTime() + ((((u32)pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags) & VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK)>>8) )
             bSyncNow = true;
         pProcessorRxVideo->periodicLoopProcessor(g_TimeNow, bSyncNow);

         if ( bSyncNow )
            bAnyVehicleMustSyncNow = true;
      }
   }

   u32 tTime1 = g_TimeNow;

   if ( bAnyVehicleMustSyncNow || controller_rt_info_will_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
      adaptive_video_periodic_loop(bAnyVehicleMustSyncNow);

   router_periodic_loop();
   
   _read_ipc_pipes(tTime1);
   _consume_ipc_messages();

   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->hasCamera() )
      rx_video_output_periodic_loop();

   g_TimeNow = get_current_timestamp_ms();
   u32 tTime2 = g_TimeNow;

   if ( controller_rt_info_will_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         memcpy((u8*)&(g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[g_SMControllerRTInfo.iCurrentIndex][i]), (u8*)&(g_SM_RadioStats.radio_interfaces[i].signalInfo.signalInfoVideo), sizeof(type_runtime_radio_rx_signal_info));
         memcpy((u8*)&(g_SMControllerRTInfo.radioInterfacesSignalInfoData[g_SMControllerRTInfo.iCurrentIndex][i]), (u8*)&(g_SM_RadioStats.radio_interfaces[i].signalInfo.signalInfoData), sizeof(type_runtime_radio_rx_signal_info));
      }
      radio_rx_reset_signal_info();
      radio_stats_reset_rx_signal_info(&g_SM_RadioStats);
   }

   g_TimeNow = get_current_timestamp_ms();
   u32 tTime3 = g_TimeNow;

   _check_send_packets(true);

   g_TimeNow = get_current_timestamp_ms();
   u32 tTime4 = g_TimeNow;
   if ( (g_TimeNow > g_TimeStart + 10000) && (tTime4 > tTime0 + uMaxLoopTime) )
   {
      log_softerror_and_alarm("Router adv sync main loop took too long to complete (loop counter: %u) (%d milisec: %u + %u + %u + %u), recording: %s, repeat count: %u!!!",
         g_pProcessStats->uLoopCounter, tTime4 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3,
         rx_video_is_recording()?"yes":"no", s_iCountCPULoopOverflows+1);
      if ( ! test_link_is_in_progress() )
      if ( g_pControllerSettings->iDeveloperMode )
      {
         s_iCountCPULoopOverflows++;
         if ( rx_video_is_recording() )
         {
            if ( s_iCountCPULoopOverflows > 1 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING,(tTime4-tTime0), 0);

            if ( tTime4 >= tTime0 + 300 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING,(tTime4-tTime0)<<16, 0);
         }
         else
         {
            if ( s_iCountCPULoopOverflows > 5 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime4-tTime0), 0);

            if ( tTime4 >= tTime0 + 300 )
            if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
               send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime4-tTime0)<<16, 0);
         }
      }
   }
   else
   {
      s_iCountCPULoopOverflows = 0;
   }

   if ( controller_rt_info_check_advance_index(&g_SMControllerRTInfo, g_TimeNow) )
   {
      radio_rx_set_packet_counter_output(&(g_SMControllerRTInfo.uRxHighPriorityPackets[g_SMControllerRTInfo.iCurrentIndex][0]),
          &(g_SMControllerRTInfo.uRxDataPackets[g_SMControllerRTInfo.iCurrentIndex][0]), &(g_SMControllerRTInfo.uRxMissingPackets[g_SMControllerRTInfo.iCurrentIndex][0]), &(g_SMControllerRTInfo.uRxMissingPacketsMaxGap[g_SMControllerRTInfo.iCurrentIndex][0]));
      if ( g_pControllerSettings->iDeveloperMode )
         radio_rx_set_air_gap_track_output(&(g_SMControllerRTInfo.uRxMaxAirgapSlots[g_SMControllerRTInfo.iCurrentIndex]));
   }

   if ( NULL != g_pProcessStats )
   {
      if ( g_pProcessStats->uMaxLoopTimeMs < tTime4 - tTime0 )
         g_pProcessStats->uMaxLoopTimeMs = tTime4 - tTime0;
      g_pProcessStats->uTotalLoopTime += tTime4 - tTime0;
      if ( 0 != g_pProcessStats->uLoopCounter )
         g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
   }
}
