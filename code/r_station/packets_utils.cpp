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

#include "packets_utils.h"
#include "../base/config.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/models_list.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../base/commands.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radio_tx.h"
#include "../base/ctrl_interfaces.h"
#include "../base/tx_powers.h"
#include "../utils/utils_vehicle.h"

#include "radio_links_sik.h"
#include "processor_rx_video.h"
#include "shared_vars.h"
#include "shared_vars_state.h"
#include "timers.h"
#include "test_link_params.h"
#include "ruby_rt_station.h"

u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];
u32 s_TimeLastLogAlarmNoInterfacesCanSend = 0;

u32 s_StreamsTxPacketIndex[MAX_RADIO_STREAMS];
u16 s_StreamsLastTxTime[MAX_RADIO_STREAMS];
int s_LastSetAtherosCardsDatarates[MAX_RADIO_INTERFACES];
int s_iLastComputedTxPowerMwPerRadioInterface[MAX_RADIO_INTERFACES];

bool s_bFirstTimeLogTxAssignment = true;

static pthread_t s_pThreadSetTxPower;
static volatile bool s_bThreadSetTxPowerRunning = false;
static pthread_mutex_t s_MutexSetRadioTxPower = PTHREAD_MUTEX_INITIALIZER;
static sem_t* s_pSemaphoreSetTxPowerWrite = NULL;
static sem_t* s_pSemaphoreSetTxPowerRead = NULL;
static volatile int s_iLastRawTxPowerPerRadioInterface[MAX_RADIO_INTERFACES];


void* _thread_set_tx_power_async(void *argument)
{
   sched_yield();
   hw_log_current_thread_attributes("set tx power");

   int iTmpRawTxPowerPerRadioInterface[MAX_RADIO_INTERFACES];
   int iLastRawTxPowerPerRadioInterface[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iLastRawTxPowerPerRadioInterface[i] = 0;

   while ( s_bThreadSetTxPowerRunning )
   {
      int iRes = sem_wait(s_pSemaphoreSetTxPowerRead);

      if ( 0 != iRes )
      {
         if ( ! s_bThreadSetTxPowerRunning )
            break;
         if ( errno != ETIMEDOUT )
            log_softerror_and_alarm("Failed to wait tx pwoer semaphore, %d, %d, %s", iRes, errno, strerror(errno));
         continue;
      }
      pthread_mutex_lock(&s_MutexSetRadioTxPower);
      if ( ! s_bThreadSetTxPowerRunning )
      {
         pthread_mutex_unlock(&s_MutexSetRadioTxPower);
         break;
      }
      memcpy((u8*)(&iTmpRawTxPowerPerRadioInterface[0]), (u8*)(&s_iLastRawTxPowerPerRadioInterface[0]), MAX_RADIO_INTERFACES*sizeof(int));
      pthread_mutex_unlock(&s_MutexSetRadioTxPower);

      for( int iInterface=0; iInterface<MAX_RADIO_INTERFACES; iInterface++ )
      {
         if ( 0 == iTmpRawTxPowerPerRadioInterface[iInterface] )
            continue;

         int iRawPower = iTmpRawTxPowerPerRadioInterface[iInterface];
         if ( iRawPower < 0 )
            iRawPower = -iRawPower;
         if ( iRawPower == iLastRawTxPowerPerRadioInterface[iInterface] )
            continue;

         iLastRawTxPowerPerRadioInterface[iInterface] = iRawPower;
         log_line("[ThreadTxPower] Set radio interface %d raw tx power to %d", iInterface, iRawPower);

         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
         if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->isConfigurable) )
            continue;

         if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
            hardware_radio_set_txpower_raw_rtl8812au(iInterface, iRawPower);
         if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
            hardware_radio_set_txpower_raw_rtl8812eu(iInterface, iRawPower);
         if ( hardware_radio_driver_is_rtl8733bu_card(pRadioHWInfo->iRadioDriver) )
            hardware_radio_set_txpower_raw_rtl8733bu(iInterface, iRawPower);
         if ( hardware_radio_driver_is_atheros_card(pRadioHWInfo->iRadioDriver) )
            hardware_radio_set_txpower_raw_atheros(iInterface, iRawPower);
      }
   }
   log_line("[ThreadTxPower] Thread ended.");
   return NULL;
}

void packet_utils_init()
{
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      s_StreamsTxPacketIndex[i] = 0;
      s_StreamsLastTxTime[i] = 0;
   }
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_LastSetAtherosCardsDatarates[i] = 5000;
      s_iLastRawTxPowerPerRadioInterface[i] = 0;
      s_iLastComputedTxPowerMwPerRadioInterface[i] = 0;
   }

   s_pSemaphoreSetTxPowerWrite = sem_open(SEMAPHORE_SET_RADIO_TX_POWER, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pSemaphoreSetTxPowerWrite) || (SEM_FAILED == s_pSemaphoreSetTxPowerWrite) )
   {
      log_error_and_alarm("Failed to create write semaphore: %s, try alternative.", SEMAPHORE_SET_RADIO_TX_POWER);
      s_pSemaphoreSetTxPowerWrite = sem_open(SEMAPHORE_SET_RADIO_TX_POWER, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_pSemaphoreSetTxPowerWrite) || (SEM_FAILED == s_pSemaphoreSetTxPowerWrite) )
      {
         log_error_and_alarm("Failed to create write semaphore: %s", SEMAPHORE_SET_RADIO_TX_POWER);
         s_pSemaphoreSetTxPowerWrite = NULL;
         return;
      }
   }
   if ( (NULL != s_pSemaphoreSetTxPowerWrite) && (SEM_FAILED != s_pSemaphoreSetTxPowerWrite) )
      log_line("Opened semaphore for signaling set radio tx power: (%s)", SEMAPHORE_SET_RADIO_TX_POWER);

   s_pSemaphoreSetTxPowerRead = sem_open(SEMAPHORE_SET_RADIO_TX_POWER, O_RDWR);
   if ( (NULL == s_pSemaphoreSetTxPowerRead) || (SEM_FAILED == s_pSemaphoreSetTxPowerRead) )
   {
      log_error_and_alarm("Failed to create read semaphore: %s, try alternative.", SEMAPHORE_SET_RADIO_TX_POWER);
      s_pSemaphoreSetTxPowerRead = sem_open(SEMAPHORE_SET_RADIO_TX_POWER, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_pSemaphoreSetTxPowerRead) || (SEM_FAILED == s_pSemaphoreSetTxPowerRead) )
      {
         log_error_and_alarm("Failed to create read semaphore: %s", SEMAPHORE_SET_RADIO_TX_POWER);
         s_pSemaphoreSetTxPowerRead = NULL;
         return;
      }
   }
   if ( (NULL != s_pSemaphoreSetTxPowerRead) && (SEM_FAILED != s_pSemaphoreSetTxPowerRead) )
      log_line("Opened semaphore for checking set radio tx power: (%s)", SEMAPHORE_SET_RADIO_TX_POWER);

   int iSemVal = 0;
   if ( 0 == sem_getvalue(s_pSemaphoreSetTxPowerRead, &iSemVal) )
      log_line("Semaphore set radio tx power initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("Failed to get semaphore set radio tx power initial value.");

   pthread_attr_t attr;
   int iPrio = g_pControllerSettings->iThreadPriorityOthers;
   if ( ! g_pControllerSettings->iPrioritiesAdjustment )
      iPrio = -1;

   if ( (iPrio > 1) && (iPrio < 100) )
      hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_FIFO, iPrio, "set tx power");
   else
      hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "set tx power");
   if ( 0 != pthread_create(&s_pThreadSetTxPower, &attr, &_thread_set_tx_power_async, NULL) )
   {
      s_bThreadSetTxPowerRunning = false;
      log_softerror_and_alarm("Failed to create thread to set tx power.");
   }
   else
   {
      s_bThreadSetTxPowerRunning = true;
      log_line("Started thread to set tx power");
   }
   pthread_attr_destroy(&attr);
}

void packet_utils_uninit()
{
   if ( s_bThreadSetTxPowerRunning )
   {
      pthread_mutex_lock(&s_MutexSetRadioTxPower);
      s_bThreadSetTxPowerRunning = false;
      pthread_mutex_unlock(&s_MutexSetRadioTxPower);

      if ( (NULL != s_pSemaphoreSetTxPowerWrite) && (0 != sem_post(s_pSemaphoreSetTxPowerWrite)) )
         log_softerror_and_alarm("Failed to signal semaphore for quiting radio tx power.");

      hardware_sleep_ms(50);
   }

   if ( NULL != s_pSemaphoreSetTxPowerWrite )
      sem_close(s_pSemaphoreSetTxPowerWrite);
   if ( NULL != s_pSemaphoreSetTxPowerRead )
      sem_close(s_pSemaphoreSetTxPowerRead);
   s_pSemaphoreSetTxPowerWrite = NULL;
   s_pSemaphoreSetTxPowerRead = NULL;
   sem_unlink(SEMAPHORE_SET_RADIO_TX_POWER);
}

void _computeBestTXCardsForEachLocalRadioLink(int* pIndexCardsForRadioLinks)
{
   if ( NULL == pIndexCardsForRadioLinks )
      return;

   int iBestRXQualityForRadioLink[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iBestRXQualityForRadioLink[i] = -1000000;

   u32 uBoardType = hardware_getBoardType();

   // Compute desired tx powers

   for( int iRadioInterfaceIndex=0; iRadioInterfaceIndex<MAX_RADIO_INTERFACES; iRadioInterfaceIndex++ )
   {
      if ( (NULL == g_pCurrentModel) || g_bSearching )
         pIndexCardsForRadioLinks[iRadioInterfaceIndex] = iRadioInterfaceIndex;
      else
         pIndexCardsForRadioLinks[iRadioInterfaceIndex] = -1;
      s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex] = 0;

      if ( ! hardware_radio_index_is_wifi_radio(iRadioInterfaceIndex) )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( (NULL == pRadioHWInfo) || (NULL == pCRII) )
         continue;

      if ( ! pRadioHWInfo->isConfigurable )
         continue;

      ControllerSettings* pCS = get_ControllerSettings();
      if ( (NULL == pCS) || (NULL == g_pCurrentModel) )
         continue;

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;

      int iVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].assignedVehicleRadioLinkId;
      int iCardModel = pCRII->cardModel;

      s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex] = tx_powers_convert_raw_to_mw(uBoardType, iCardModel, pCRII->iRawPowerLevel);
      if ( g_bSearching )
         s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex] = tx_powers_convert_raw_to_mw(uBoardType, iCardModel, DEFAULT_RADIO_TX_POWER_CONTROLLER);
      else if ( ! pCS->iFixedTxPower )
         s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex] = tx_power_compute_uplink_power_for_model_link(g_pCurrentModel, iVehicleRadioLinkId, iRadioInterfaceIndex, iCardModel);
      if ( isNegociatingRadioLink() )
         s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex] = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO * 4;
   }

   int iCountRadioLinks = g_SM_RadioStats.countLocalRadioLinks;
   if ( iCountRadioLinks < 1 )
      iCountRadioLinks = 1;
   for( int iRadioLink = 0; iRadioLink < iCountRadioLinks; iRadioLink++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLink].matchingVehicleRadioLinkId;

      if ( (NULL == g_pCurrentModel) || g_bSearching )
         continue;

      // Radio link is downlink only or a relay link ? Controller can't send data on it (uplink)

      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      
      // Find max power cards for this link
      int iMaxTxPowerMw = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         int iRadioLinkForCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
         if ( (iRadioLinkForCard < 0) || (iRadioLinkForCard != iRadioLink) )
            continue;

         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
             continue;
         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

         if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
            continue;

         if ( (0 == iMaxTxPowerMw) || (s_iLastComputedTxPowerMwPerRadioInterface[i] > iMaxTxPowerMw) )
            iMaxTxPowerMw = s_iLastComputedTxPowerMwPerRadioInterface[i];
      }

      // Iterate all radio interfaces assigned to this local radio link, take into account only ones with high power
      int iCountInterfaceMaxPower = 0;
      int iIndexInterfacesMaxPower = -1;
      int iMinPrefferedIndex = 10000;
      int iPrefferedCardIndex = -1;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
             continue;
         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

         if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
            continue;

         if ( ! pRadioHWInfo->isTxCapable )
            continue;

         int iRadioLinkForCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
         if ( (iRadioLinkForCard < 0) || (iRadioLinkForCard != iRadioLink) )
            continue;

         int iCardTxIndex = controllerIsCardTXPreferred(pRadioHWInfo->szMAC);
         if ( (iCardTxIndex > 0) && (iCardTxIndex < iMinPrefferedIndex) )
         {
            iMinPrefferedIndex = iCardTxIndex;
            iPrefferedCardIndex = i;
         }

         if ( (0 != iMaxTxPowerMw) && (s_iLastComputedTxPowerMwPerRadioInterface[i] >= (iMaxTxPowerMw*9)/10) )
         {
            iCountInterfaceMaxPower++;
            iIndexInterfacesMaxPower = i;
         }
      }

      if ( -1 == iPrefferedCardIndex )
      if ( (1 == iCountInterfaceMaxPower) && (iIndexInterfacesMaxPower >= 0) )
         iPrefferedCardIndex = iIndexInterfacesMaxPower;

      if ( iPrefferedCardIndex >= 0 )
      {
         pIndexCardsForRadioLinks[iRadioLink] = iPrefferedCardIndex;
         if ( s_bFirstTimeLogTxAssignment )
            log_line("Assigned preferred Tx radio interface %d as Tx card for radio link %d.", iPrefferedCardIndex+1, iRadioLink+1);
         continue;
      }
   
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
             continue;

         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

         if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
            continue;

         if ( ! pRadioHWInfo->isTxCapable )
            continue;

         int iRadioLinkForCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
         if ( (iRadioLinkForCard < 0) || (iRadioLinkForCard != iRadioLink) )
            continue;

         if ( (0 == iMaxTxPowerMw) || (s_iLastComputedTxPowerMwPerRadioInterface[i] < (iMaxTxPowerMw*9)/10) )
            continue;

         if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
         {
            pIndexCardsForRadioLinks[iRadioLink] = i;
            iBestRXQualityForRadioLink[iRadioLink] = g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality;
         }
         else if ( g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality > iBestRXQualityForRadioLink[iRadioLink] )
         {
            pIndexCardsForRadioLinks[iRadioLink] = i;
            iBestRXQualityForRadioLink[iRadioLink] = g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality;
         }
      }
      if ( s_bFirstTimeLogTxAssignment )
      {
         if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
            log_softerror_and_alarm("No Tx radio interface was assigned to local radio link %d.", iRadioLink+1);
         else
            log_line("Assigned radio interface %d as best Tx card for local radio link %d.", pIndexCardsForRadioLinks[iRadioLink]+1, iRadioLink+1);
      }
   }
   s_bFirstTimeLogTxAssignment = false;
}

// Return used datarate
int _compute_packet_uplink_datarate_radioflags_tx_power(int iVehicleRadioLink, int iRadioInterfaceIndex, u8* pPacketData)
{
   // Compute radio flags, datarate then tx power

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return DEFAULT_RADIO_DATARATE_LOWEST;
   t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL == pCRII )
      return DEFAULT_RADIO_DATARATE_LOWEST;

   if ( ! pRadioHWInfo->isConfigurable )
      return DEFAULT_RADIO_DATARATE_LOWEST;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( (NULL == pCS) || (NULL == g_pCurrentModel) || (NULL == pPacketData) )
      return DEFAULT_RADIO_DATARATE_LOWEST;

   t_packet_header* pPH = (t_packet_header*)pPacketData;

   bool bUseLowest = false;
   int iRuntimeIndex = getVehicleRuntimeIndex(g_pCurrentModel->uVehicleId);
   if ( -1 != iRuntimeIndex )
   {
      if ( g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost )
         bUseLowest = true;
      if ( g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeReceivedAckFromVehicle > g_TimeNow + 1000 )
         bUseLowest = true;
   }

   if ( (pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS) ||
        (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST) ||
        (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
        test_link_is_in_progress() )
      bUseLowest = true;

   //--------------------------------------------
   // Radio flags - begin

   u32 uRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink];

   /*
   if ( bUseLowest )
   {
      if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
         uRadioFlags = RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      else
         uRadioFlags = RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
   }
   */
   radio_set_frames_flags(uRadioFlags, g_TimeNow);
   
   // Radio flags - end

   //--------------------------------------------
   // Datarates - begin

   int iDataRateTx = g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iVehicleRadioLink];

   if ( (0 == iDataRateTx) || (-100 == iDataRateTx) || bUseLowest )
   {
      if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
         iDataRateTx = -1;
      else
         iDataRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
   }

   radio_set_out_datarate(iDataRateTx, pPH->packet_type, g_TimeNow);

   if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
        (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
   {
      if ( s_LastSetAtherosCardsDatarates[iRadioInterfaceIndex] != iDataRateTx )
      {
         s_LastSetAtherosCardsDatarates[iRadioInterfaceIndex] = iDataRateTx;
         update_atheros_card_datarate(g_pCurrentModel, iRadioInterfaceIndex, iDataRateTx, g_pProcessStats);
      }
      g_TimeNow = get_current_timestamp_ms();
   }

   // Datarates - end

   //------------------------------------------
   // Tx power - begin

   int iCardModel = pCRII->cardModel;
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;

   int iRadioInterfaceRawTxPowerToUse = tx_powers_convert_mw_to_raw(hardware_getBoardType(), iCardModel, s_iLastComputedTxPowerMwPerRadioInterface[iRadioInterfaceIndex]);
   
   //-----------------------------------------------
   // Tx power, apply it

   if ( iRadioInterfaceRawTxPowerToUse == s_iLastRawTxPowerPerRadioInterface[iRadioInterfaceIndex] )
      return iDataRateTx;

   pthread_mutex_lock(&s_MutexSetRadioTxPower);
   s_iLastRawTxPowerPerRadioInterface[iRadioInterfaceIndex] = iRadioInterfaceRawTxPowerToUse;
   pthread_mutex_unlock(&s_MutexSetRadioTxPower);
   if ( 0 != sem_post(s_pSemaphoreSetTxPowerWrite) )
      log_softerror_and_alarm("Failed to signal semaphore for updating radio tx power.");

   return iDataRateTx;
}

bool _send_packet_to_serial_radio_interface(int iLocalRadioLinkId, int iRadioInterfaceIndex, u8* pPacketData, int nPacketLength)
{
   if ( (NULL == pPacketData) || (nPacketLength <= 0) || (NULL == g_pCurrentModel) )
      return false;
    
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;
     
   // Do not send packet if the link is overloaded
   int iAirRate = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLinkId]/8;
   if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      iAirRate = hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex);

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   u8 uPacketType = pPH->packet_type;
   if ( ! radio_can_send_packet_on_slow_link(iLocalRadioLinkId, uPacketType, 1, g_TimeNow) )
      return false;
      
   if ( iAirRate > 0 )
   if ( g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec >= (DEFAULT_RADIO_SERIAL_MAX_TX_LOAD * (u32)iAirRate) / 100 )
   {
      static u32 sl_uLastTimeInterfaceTxOverloaded = 0;
      if ( g_TimeNow > sl_uLastTimeInterfaceTxOverloaded + 20000 )
      {
         sl_uLastTimeInterfaceTxOverloaded = g_TimeNow;
         log_line("Radio interface %d is tx overloaded: sending %d bytes/sec and air data rate is %d bytes/sec", iRadioInterfaceIndex+1, (int)g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec, iAirRate);
         send_alarm_to_central(ALARM_ID_RADIO_LINK_DATA_OVERLOAD, (g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec & 0xFFFFFF) | (((u32)iRadioInterfaceIndex)<<24), (u32)iAirRate);
      }
      return false;
   }

   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u16 uRadioLinkPacketIndex = radio_get_next_radio_link_packet_index(iLocalRadioLinkId);
   pPH->radio_link_packet_index = uRadioLinkPacketIndex;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      radio_packet_compute_crc((u8*)pPH, sizeof(t_packet_header));
   else
      radio_packet_compute_crc((u8*)pPH, pPH->total_length);

   if ( 1 != pRadioHWInfo->openedForWrite )
   {
      log_softerror_and_alarm("Radio serial interface %d is not opened for write. Can't send packet on it.", iRadioInterfaceIndex+1);
      return false;
   }

   int iWriteResult = radio_tx_send_serial_radio_packet(iRadioInterfaceIndex, pPacketData, nPacketLength);
   if ( iWriteResult > 0 )
   {
      int iTotalSent = nPacketLength;
      if ( g_pCurrentModel->radioLinksParams.iSiKPacketSize > 0 )
         iTotalSent += sizeof(t_packet_header_short) * (int) (nPacketLength / g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, iTotalSent);
      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, nPacketLength);
      return true;
   }

   log_softerror_and_alarm("Failed to write to serial radio interface %d.", iRadioInterfaceIndex+1);
   if ( iWriteResult == -2 )
   if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      radio_links_flag_reinit_sik_interface(iRadioInterfaceIndex);
   return false;
}

bool _send_packet_to_wifi_radio_interface(int iLocalRadioLinkId, int iRadioInterfaceIndex, u8* pPacketData, int nPacketLength)
{
   if ( (NULL == pPacketData) || (nPacketLength <= 0) || (NULL == g_pCurrentModel) )
      return false;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;
   
   t_packet_header* pPH = (t_packet_header*)pPacketData;

   u32 microT = get_current_timestamp_micros();
     
   int be = 0;
   if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
   if ( hpp() )
      be = 1;

   int iDataRateTx = _compute_packet_uplink_datarate_radioflags_tx_power(iVehicleRadioLinkId, iRadioInterfaceIndex, pPacketData);
   int totalLength = radio_build_new_raw_ieee_packet(iLocalRadioLinkId, s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_UPLINK, be);

   bool bShouldDuplicate = false;

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS )
      bShouldDuplicate = true;
   if ( pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS )
   {
      Model* pModel = findModelWithId(pPH->vehicle_id_dest, 7);
      if ( NULL == pModel )
         pModel = g_pCurrentModel;
      if ( (NULL != pModel) && (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE) )
         bShouldDuplicate = true;
      u8 uFlags = pPacketData[sizeof(t_packet_header) + sizeof(u32) + sizeof(u8)];
      if ( uFlags & 0x01 )
         bShouldDuplicate = true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( pPH->packet_type == PACKET_TYPE_COMMAND )
   {
      Model* pModel = findModelWithId(pPH->vehicle_id_dest, 7);
      if ( (NULL != pModel) && (pModel->uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK) )
      {
         t_packet_header_command* pPHC = (t_packet_header_command*)(pPacketData + sizeof(t_packet_header));
         if ( pPHC->command_type == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
            bShouldDuplicate = true;
      }
   }

   int iRepeatCount = 0;
   if ( bShouldDuplicate )
      iRepeatCount++;

   if ( radio_write_raw_ieee_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength, iRepeatCount) )
   {
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);
      radio_stats_set_tx_radio_datarate_for_packet(&g_SM_RadioStats, iRadioInterfaceIndex, iLocalRadioLinkId, iDataRateTx, 0);
      
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      
      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, pPH->total_length);
      s_StreamsLastTxTime[uStreamId] = get_current_timestamp_micros() - microT;

      if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
      {
         u8 uVehicleLinkId = *(pPacketData + sizeof(t_packet_header));
         u8 uCommandId = *(pPacketData + sizeof(t_packet_header) + sizeof(u8));
         log_line("Sent radio packet to vehicle to configure SiK vehicle radio link %d, command: %d", (int) uVehicleLinkId+1, (int)uCommandId);
      }
      return true;
   }
   
   log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   return false;
}

// Returns -1 on error, 0 on success
int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink, int iRepeatCount, int iTraceSource)
{
   if ( nPacketLength <= 0 )
      return -1;

   // Figure out the best radio interface to use for Tx for each radio link;

   int iTXInterfaceIndexForLocalRadioLinks[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iTXInterfaceIndexForLocalRadioLinks[i] = -1;

   _computeBestTXCardsForEachLocalRadioLink( &iTXInterfaceIndexForLocalRadioLinks[0] );

   bool bIsPingPacket = false;
   int iPingOnLocalRadioLinkId = -1;
   
   // Set packet index

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   u8 uPacketFlags = pPH->packet_flags;
   u8 uPacketType = pPH->packet_type;
   int iPacketLength = pPH->total_length;
   u32 uDestVehicleId = pPH->vehicle_id_dest;
   u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      u8 uLocalRadioLinkId = 0;
      memcpy( &uLocalRadioLinkId, pPacketData + sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
      iPingOnLocalRadioLinkId = (int)uLocalRadioLinkId;
      bIsPingPacket = true;
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      int iModelRadioLinkIndex = pPacketData[sizeof(t_packet_header)+2];
      int iCmdType = pPacketData[sizeof(t_packet_header)+4];
      for( int iRadioLink = 0; iRadioLink < g_SM_RadioStats.countLocalRadioLinks; iRadioLink++ )
      {
         int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLink].matchingVehicleRadioLinkId;
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
         if ( iVehicleRadioLinkId == iModelRadioLinkIndex )
         {
            iSendToSingleRadioLink = iRadioLink;
         }
      }
   }
     
   if ( uPacketType != PACKET_TYPE_RUBY_PING_CLOCK )
   if ( uPacketType != PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      s_StreamsTxPacketIndex[uStreamId]++;

   pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsTxPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
   
   // Send the packet to each local radio link

   bool bPacketSent = false;

   for( int iLocalRadioLinkId=0; iLocalRadioLinkId<g_SM_RadioStats.countLocalRadioLinks; iLocalRadioLinkId++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
      int iRadioInterfaceIndex = iTXInterfaceIndexForLocalRadioLinks[iLocalRadioLinkId];
      if ( iRadioInterfaceIndex < 0 )
      {
         if ( g_TimeNow > s_TimeLastLogAlarmNoInterfacesCanSend + 20000 )
         {
            s_TimeLastLogAlarmNoInterfacesCanSend = g_TimeNow;
            log_softerror_and_alarm("No radio interfaces on controller can send data on local radio link %d", iLocalRadioLinkId+1);
         }
         continue;
      }

      if ( (-1 != iSendToSingleRadioLink) && (iLocalRadioLinkId != iSendToSingleRadioLink) )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Do not try to send packets on the vehicle radio link that is assigned for relaying, if any
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
        
      // Send update packets only on a single radio interface, not on multiple radio links
      if ( g_bUpdateInProgress && bPacketSent )
         break;

      // Send Ping packets only to the assigned radio link
      if ( bIsPingPacket )
      if ( iLocalRadioLinkId != iPingOnLocalRadioLinkId )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;

      radio_stats_set_tx_card_for_radio_link(&g_SM_RadioStats, iLocalRadioLinkId, iRadioInterfaceIndex);

      for( int i=0; i<=iRepeatCount; i++ )
      {
         if ( hardware_radio_index_is_serial_radio(iRadioInterfaceIndex) )
         {
            if ( g_bUpdateInProgress )
               continue;
            bPacketSent |= _send_packet_to_serial_radio_interface(iLocalRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength);
         }
         else
            bPacketSent |= _send_packet_to_wifi_radio_interface(iLocalRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength);
      
         if ( i < iRepeatCount )
            hardware_sleep_micros(400);
      }
   }


   if ( bPacketSent )
   {
      g_SMControllerRTInfo.uTxLastDeltaTime[g_SMControllerRTInfo.iCurrentIndex] = g_TimeNow - g_SMControllerRTInfo.uCurrentSliceStartTime;
      if ( uPacketFlags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
         g_SMControllerRTInfo.uTxHighPriorityPackets[g_SMControllerRTInfo.iCurrentIndex]++;
      else
         g_SMControllerRTInfo.uTxPackets[g_SMControllerRTInfo.iCurrentIndex]++;
      
      radio_stats_update_on_packet_sent_for_radio_stream(&g_SM_RadioStats, g_TimeNow, uDestVehicleId, uStreamId, uPacketType, iPacketLength);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastRadioTxTime = g_TimeNow;

      #ifdef LOG_RAW_TELEMETRY
      if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_UPLOAD )
      {
         t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pPacketData + sizeof(t_packet_header));
         log_line("[Raw_Telem] Send raw telemetry packet to radio interfaces, index %u, %d / %d bytes", pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
      }
      #endif
      return 0;
   }

   log_softerror_and_alarm("Packet not sent! No radio interface could send it. Packet type: %s", str_get_packet_type(uPacketType));
   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   char szTmp[256];
   strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[2]));

   log_softerror_and_alarm("Current local radio links: %d, current model links frequencies: 1: %s, 2: %s, 3: %s", g_SM_RadioStats.countLocalRadioLinks, szFreq1, szFreq2, szFreq3 );
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId;
      int iRadioInterfaceIndex = iTXInterfaceIndexForLocalRadioLinks[i];
      if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex > hardware_get_radio_interfaces_count()) )
      {
         log_softerror_and_alarm("No radio interfaces assigned for Tx on local radio link %d.", i+1);
         continue;          
      }
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( NULL == pRadioHWInfo )
      {
         log_softerror_and_alarm("Can't get NIC info for radio interface %d", iRadioInterfaceIndex+1);
         continue;
      }
      log_softerror_and_alarm("Current radio interface used for TX on local radio link %d, vehicle radio link %d: %d, freq: %s",
         i+1, iVehicleRadioLinkId+1, iRadioInterfaceIndex+1, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));    
      str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId], szTmp);
      log_softerror_and_alarm("Current vehicle radio link %d capabilities: %s", iVehicleRadioLinkId+1, szTmp);
   }

   return -1;
}

int get_controller_radio_interface_index_for_radio_link(int iLocalRadioLinkId)
{
   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= g_SM_RadioStats.countLocalRadioLinks) )
      return -1;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
          continue;

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;

      if ( ! pRadioHWInfo->isTxCapable )
         continue;

      int iRadioLinkCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
      if ( (iRadioLinkCard < 0) || (iRadioLinkCard != iLocalRadioLinkId) )
         continue;
      return i;
   }
   return -1;
}