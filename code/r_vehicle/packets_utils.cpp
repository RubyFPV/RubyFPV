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

#include <pthread.h>
#include "packets_utils.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/commands.h"
#include "../base/hardware_procs.h"
#include "../base/tx_powers.h"
#include "../base/radio_utils.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "ruby_rt_vehicle.h"
#include "shared_vars.h"
#include "timers.h"
#include "processor_tx_video.h"
#include "test_link_params.h"
#include "adaptive_video.h"
#include "negociate_radio.h"
#include "video_sources.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radio_tx.h"

u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_StreamsTxPacketIndex[MAX_RADIO_STREAMS];

int s_LastTxDataRatesVideo[MAX_RADIO_INTERFACES];
int s_LastTxDataRatesVideoMaximum[MAX_RADIO_INTERFACES];
int s_LastTxDataRatesData[MAX_RADIO_INTERFACES];
int s_LastSetAtherosCardsDatarates[MAX_RADIO_INTERFACES];

u32 s_VehicleLogSegmentIndex = 0;


typedef struct
{
   u32 uIndex; // monotonicaly increasing
   u32 uId;
   u32 uFlags1;
   u32 uFlags2;
   u32 uRepeatCount;
   u32 uStartTime;
} ALIGN_STRUCT_SPEC_INFO t_alarm_info;

#define MAX_ALARMS_QUEUE 20

t_alarm_info s_AlarmsQueue[MAX_ALARMS_QUEUE];
int s_AlarmsPendingInQueue = 0;
u32 s_uAlarmsIndex = 0;
u32 s_uTimeLastAlarmSentToRouter = 0;

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
      s_StreamsTxPacketIndex[i] = 0;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_LastTxDataRatesVideo[i] = 0;
      s_LastTxDataRatesVideoMaximum[i] = 0;
      s_LastTxDataRatesData[i] = 0;
      s_LastSetAtherosCardsDatarates[i] = 5000;
      s_iLastRawTxPowerPerRadioInterface[i] = 0;
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
   int iCoreAff = CORE_AFFINITY_OTHERS;
   if ( NULL != g_pCurrentModel )
   {
      if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
         iCoreAff = g_pCurrentModel->processesPriorities.iCoreOthers;
      else
         iCoreAff = -1;
   }
   int iPrio = g_pCurrentModel->processesPriorities.iThreadPriorityOthers;
   if ( ! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS) )
      iPrio = -1;


   if ( (iPrio > 1) && (iPrio < 100) )
      hw_init_worker_thread_attrs(&attr, iCoreAff, -1, SCHED_FIFO, iPrio, "set tx power");
   else
      hw_init_worker_thread_attrs(&attr, iCoreAff, -1, SCHED_OTHER, 0, "set tx power");

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

void packet_utils_reset_last_used_video_datarate()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_LastTxDataRatesVideo[i] = 0;
   }
}

void packet_utils_reset_last_used_max_video_datarate()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_LastTxDataRatesVideo[i] = 0;
      s_LastTxDataRatesVideoMaximum[i] = 0;
   }
}

int get_last_tx_power_used_for_radiointerface(int iRadioInterface)
{
   if ( (iRadioInterface >= 0) && (iRadioInterface <= MAX_RADIO_INTERFACES) )
      return s_iLastRawTxPowerPerRadioInterface[iRadioInterface];
   return 0;
}

// Returns the actual datarate bps used last time for data or video
int get_last_tx_used_datarate_bps_video(int iInterface)
{
   return s_LastTxDataRatesVideo[iInterface];
}

int get_last_tx_used_datarate_bps_data(int iInterface)
{
   return s_LastTxDataRatesData[iInterface];
}

// Returns the actual datarate total mbps used last time for video
u32 get_last_tx_minimum_video_radio_datarate_bps()
{
   u32 nMinRate = MAX_U32;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;
      if ( s_LastTxDataRatesVideo[i] == 0 )
         continue;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || (! pRadioInfo->isHighCapacityInterface) )
         continue;
        
      int iRadioLink = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
      if ( iRadioLink >= 0 )
      if ( getRealDataRateFromRadioDataRate(s_LastTxDataRatesVideo[i], g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink], 1) < nMinRate )
         nMinRate = getRealDataRateFromRadioDataRate(s_LastTxDataRatesVideo[i], g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink], 1);
   }
   if ( nMinRate == MAX_U32 )
      return 0;
   return nMinRate;
}


// Returns the actual datarate total mbps used last time for video
u32 get_last_tx_maximum_video_radio_datarate_bps()
{
   u32 nMaxRate = 0;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;
      if ( s_LastTxDataRatesVideo[i] == 0 )
         continue;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || (! pRadioInfo->isHighCapacityInterface) )
         continue;
        
      int iRadioLink = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
      if ( iRadioLink >= 0 )
      if ( getRealDataRateFromRadioDataRate(s_LastTxDataRatesVideo[i], g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink], 1) > nMaxRate )
         nMaxRate = getRealDataRateFromRadioDataRate(s_LastTxDataRatesVideo[i], g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink], 1);
   }
   if ( nMaxRate == 0 )
      nMaxRate = DEFAULT_RADIO_DATARATE_LOWEST;
   return nMaxRate;
}

bool _check_update_tx_pit_mode()
{
   bool bIsInTxPITMode = false;
   g_pCurrentModel->uModelRuntimeStatusFlags &= ~(MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE | MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE_TEMPERATURE);
   
   // Manual PIT mode?
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE )
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL )
   {
      g_pCurrentModel->uModelRuntimeStatusFlags |= MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE;
      bIsInTxPITMode = true;
   }

   // Arm/disarm PIT mode?
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE )
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_ARMDISARM )
   if ( ! g_bVehicleArmed )
   {
      g_pCurrentModel->uModelRuntimeStatusFlags |= MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE;
      bIsInTxPITMode = true;
   }

   // Temperature PIT mode?
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE )
   if ( g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_TEMP )
   {
      static bool s_bTemperatureTriggeredCutoff = false;

      int iTempThresholdC = (g_pCurrentModel->hwCapabilities.uHWFlags & 0xFF00) >> 8;

      if ( ! s_bTemperatureTriggeredCutoff )
      if ( g_iVehicleSOCTemperatureC >= iTempThresholdC )
         s_bTemperatureTriggeredCutoff = true;

      if ( s_bTemperatureTriggeredCutoff )
      if ( g_iVehicleSOCTemperatureC < iTempThresholdC - 1 )
         s_bTemperatureTriggeredCutoff = false;

      if ( s_bTemperatureTriggeredCutoff )
      {
         g_pCurrentModel->uModelRuntimeStatusFlags |= MODEL_RUNTIME_STATUS_FLAG_IN_PIT_MODE_TEMPERATURE;
         bIsInTxPITMode = true;
      }
   }
   return bIsInTxPITMode;
}

void _compute_packet_tx_power_on_ieee(int iVehicleRadioLinkId, int iRadioInterfaceIndex, int iDataRateTx)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
      return;

   bool bIsInTxPITMode = _check_update_tx_pit_mode();

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceIndex];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;

   int iRadioInterfaceRawTxPowerToUse = g_pCurrentModel->radioInterfacesParams.interface_raw_power[iRadioInterfaceIndex];
   if ( bIsInTxPITMode )
      iRadioInterfaceRawTxPowerToUse = tx_powers_convert_mw_to_raw(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel, 5);
   
   if ( negociate_radio_link_is_in_progress() && (negociate_radio_link_get_txpower_mw() > 0) )
      iRadioInterfaceRawTxPowerToUse = -tx_powers_convert_mw_to_raw(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel, negociate_radio_link_get_txpower_mw());
   else if ( ! bIsInTxPITMode )
   {
      int iMaxUsablePowerMw = 0;
      if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED )
      {
         if ( iDataRateTx < 0 )
         {
            int iIndex = -iDataRateTx-1;
            int iScale = 100;
            iMaxUsablePowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[0][iIndex];
            while ( (iMaxUsablePowerMw <= 0) && (iIndex > 0) )
            {
               iIndex--;
               if ( iScale == 100 )
                  iScale = 30;
               iScale -= 2;
               iMaxUsablePowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[0][iIndex];
            }
            iMaxUsablePowerMw = (iMaxUsablePowerMw * iScale)/100;
         }
         else if ( iDataRateTx > 0 )
         {
            int iIndex = -1;
            int iScale = 100;
            for( int i=0; i<getLegacyDataRatesCount(); i++ )
            {
               if ( getLegacyDataRatesBPS()[i] == iDataRateTx )
               {
                  iIndex = i;
                  iMaxUsablePowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[0][iIndex];
                  break;
               }
            }
            while ( (iMaxUsablePowerMw <= 0) && (iIndex > 0) )
            {
               iIndex--;
               if ( iScale == 100 )
                  iScale = 30;
               iScale -= 2;
               iMaxUsablePowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[0][iIndex];
            }
            iMaxUsablePowerMw = (iMaxUsablePowerMw * iScale)/100;
         }
      }
      else
      {
         iMaxUsablePowerMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);
         if ( iDataRateTx < 0 )
         {
            if ( iDataRateTx < -3 )
                iMaxUsablePowerMw = (iMaxUsablePowerMw * 30)/100;
            if ( iDataRateTx < -4 )
            {
               for( int k=-4; k>iDataRateTx; k-- )
                  iMaxUsablePowerMw = (iMaxUsablePowerMw * 50)/100;
            }
         }
         else if ( iDataRateTx > 0 )
         {
             if ( iDataRateTx > 18000000 )
                iMaxUsablePowerMw = (iMaxUsablePowerMw * 30)/100;
             if ( iDataRateTx > 24000000 )
             {
                for( int k=4; k<getLegacyDataRatesCount(); k++ )
                {
                   if ( iDataRateTx > getLegacyDataRatesBPS()[k] )
                      iMaxUsablePowerMw = (iMaxUsablePowerMw * 50)/100;
                   else
                      break;
                }
             }
         }
         if ( iMaxUsablePowerMw < 1 )
            iMaxUsablePowerMw = 1;
      }
      if ( iMaxUsablePowerMw > 0 )
      {
         int iMaxUsableRawPower = tx_powers_convert_mw_to_raw(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel, iMaxUsablePowerMw);
         if ( iRadioInterfaceRawTxPowerToUse > iMaxUsableRawPower )
            iRadioInterfaceRawTxPowerToUse = -iMaxUsableRawPower;
      }
   }

   if ( iRadioInterfaceRawTxPowerToUse == s_iLastRawTxPowerPerRadioInterface[iRadioInterfaceIndex] )
      return;

   pthread_mutex_lock(&s_MutexSetRadioTxPower);
   s_iLastRawTxPowerPerRadioInterface[iRadioInterfaceIndex] = iRadioInterfaceRawTxPowerToUse;
   pthread_mutex_unlock(&s_MutexSetRadioTxPower);
   if ( 0 != sem_post(s_pSemaphoreSetTxPowerWrite) )
      log_softerror_and_alarm("Failed to signal semaphore for updating radio tx power.");
}

int _compute_packet_downlink_datarate(u8* pPacketData, int iVehicleRadioLink, int iRadioInterfaceIndex, u32 uRadioFlags)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( (NULL == g_pCurrentModel) || (NULL == pPacketData) || (NULL == pRadioHWInfo) || (! pRadioHWInfo->isConfigurable) )
   {
      int iDataRateLowest = DEFAULT_RADIO_DATARATE_LOWEST;
      if ( uRadioFlags & RADIO_FLAGS_USE_MCS_DATARATES )
         iDataRateLowest = -1;
      s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateLowest;
      s_LastTxDataRatesVideo[iRadioInterfaceIndex] = iDataRateLowest;
      return iDataRateLowest;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   bool bIsAudioVideoPacket = false;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) == STREAM_ID_AUDIO )
      bIsAudioVideoPacket = true;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) >= STREAM_ID_VIDEO_1 )
      bIsAudioVideoPacket = true;

   int iDataRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLink];
   if ( bIsAudioVideoPacket )
      iDataRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iVehicleRadioLink];

   if ( test_link_is_in_progress() )
   {
      type_radio_links_parameters* pRP = test_link_get_temp_radio_params();
      iDataRateTx = pRP->downlink_datarate_data_bps[iVehicleRadioLink];
      if ( bIsAudioVideoPacket )
         iDataRateTx = pRP->downlink_datarate_video_bps[iVehicleRadioLink];
   }

   if ( negociate_radio_link_is_in_progress() && bIsAudioVideoPacket )
   {
      int iRate = negociate_radio_link_get_data_rate();
      if ( (iRate != 0) && (iRate != -100) )
         iDataRateTx = iRate;
   }

   // A fixed datarate was set by user or by current flow? Use it
   if ( (iDataRateTx != 0) && (iDataRateTx > -100) )
   {
      if ( bIsAudioVideoPacket )
         s_LastTxDataRatesVideo[iRadioInterfaceIndex] = iDataRateTx;
      else
         s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateTx;
      return iDataRateTx;
   }

   // Lowest datarate set to be used?
   if ( iDataRateTx == -100 )
   {
      iDataRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
      if ( uRadioFlags & RADIO_FLAGS_USE_MCS_DATARATES )
         iDataRateTx = -1;
      if ( bIsAudioVideoPacket )
         s_LastTxDataRatesVideo[iRadioInterfaceIndex] = iDataRateTx;
      else
         s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateTx;
      return iDataRateTx;
   }

   //----------------------------------------
   // Adaptive or auto datarates only from here on

   // Data packet
   if ( ! bIsAudioVideoPacket )
   {
      // No video feed to adjust to? Use fixed rates for data then.
      if ( 0 == s_LastTxDataRatesVideo[iRadioInterfaceIndex] )
      {
         iDataRateTx = -3; // MCS2
         if ( uRadioFlags & RADIO_FLAGS_USE_LEGACY_DATARATES )
            iDataRateTx = 18000000;
         s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateTx;
         return iDataRateTx;
      }

      iDataRateTx = s_LastTxDataRatesVideo[iRadioInterfaceIndex];

      // Data packets are sent on lower rate on auto rates for data
      /*
      bool bUseLowerDatarateForThisPacket = false;
      if ( (! negociate_radio_link_is_in_progress()) && (! test_link_is_in_progress()) )
      if ( (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
           (pPH->packet_type == PACKET_TYPE_COMMAND_RESPONSE) ||
           (pPH->packet_type == PACKET_TYPE_COMMAND) ||
           (pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS) ||
           (pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK) )
         bUseLowerDatarateForThisPacket = true;

      if ( ! bUseLowerDatarateForThisPacket )
      {
         s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateTx;
         return iDataRateTx;
      }
      */
      // If QAM modulations is used on video, don't go lower than QAM modulations
      if ( iDataRateTx < 0 )
      {
         if ( iDataRateTx < -4 )
            iDataRateTx = -4;
         else if ( (iDataRateTx > -4) && (iDataRateTx < -1) )
            iDataRateTx++;
      }
      else if ( iDataRateTx > 0 )
      {
         if ( iDataRateTx > 24000000 )
            iDataRateTx = 24000000;
         else if ( (iDataRateTx < 24000000) && (iDataRateTx > DEFAULT_RADIO_DATARATE_LOWEST) )
            iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, -1);
      }
      s_LastTxDataRatesData[iRadioInterfaceIndex] = iDataRateTx;
      return iDataRateTx;       
   }

   // ------------------------------------
   // Adaptive video packets only from now on

   u32 uCurrentVideoBitrateBPS = 0;
   if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
   {
      t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacketData + sizeof(t_packet_header));
      uCurrentVideoBitrateBPS = pPHVS->uCurrentVideoBitrateBPS;
   }
   if ( 0 == uCurrentVideoBitrateBPS )
      uCurrentVideoBitrateBPS = video_sources_get_last_set_video_bitrate();

   // No video feed bitrate info to adjust to? Use fixed rates for video then.
   if ( 0 == uCurrentVideoBitrateBPS )
   {
      iDataRateTx = -3; // MCS2
      if ( uRadioFlags & RADIO_FLAGS_USE_LEGACY_DATARATES )
         iDataRateTx = 18000000;
      s_LastTxDataRatesVideo[iRadioInterfaceIndex] = iDataRateTx;
      return iDataRateTx;
   }

   iDataRateTx = g_pCurrentModel->getRadioDataRateForVideoBitrate(uCurrentVideoBitrateBPS, iVehicleRadioLink, true);

   static u32 s_uLastTimeSendDRMsg = 0;
   char szTmp[64];
   char szBuff[256];

   int iCurrentDRBoost = adaptive_video_get_current_dr_boost();
   if ( 0 != iCurrentDRBoost )
   {
      if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED )
      if ( ((iDataRateTx < 0) && ((iDataRateTx - iCurrentDRBoost) < g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate)) ||
           ((iDataRateTx > 0) && (iDataRateTx + iCurrentDRBoost > g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate)) )
      if ( g_TimeNow > s_uLastTimeSendDRMsg + 5000 )
      {
         s_uLastTimeSendDRMsg = g_TimeNow;
         str_getDataRateDescriptionNoSufix(iDataRateTx, szTmp);
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Packet utils: Tried to set a DR boost (+%d) to radio rate %s, over max supported radio link rates of %s, on video bitrate %.2f Mbps",
                iCurrentDRBoost, szTmp, str_format_datarate_inline(g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate), (float)uCurrentVideoBitrateBPS/1000.0/1000.0);
         send_message_to_controller(0, 2, szBuff);
      }

      iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, iCurrentDRBoost);
   }

   if ( iDataRateTx < 0 )
   if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED )
   if ( iDataRateTx < g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate )
   if ( g_TimeNow > s_uLastTimeSendDRMsg + 5000 )
   {
      s_uLastTimeSendDRMsg = g_TimeNow;
      strcpy(szTmp, str_format_datarate_inline(iDataRateTx));
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Packet utils: Tried to set datarate (%s) to higher than max supported (%s)", szTmp, str_format_datarate_inline(g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate));
      send_message_to_controller(0, 2, szBuff);
   }

   if ( iDataRateTx > 0 )
   if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED )
   if ( iDataRateTx > g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate )
   if ( g_TimeNow > s_uLastTimeSendDRMsg + 5000 )
   {
      s_uLastTimeSendDRMsg = g_TimeNow;
      strcpy(szTmp, str_format_datarate_inline(iDataRateTx));
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Packet utils: try to set datarate (%s) to higher than max supported (%s)", szTmp, str_format_datarate_inline(g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate));
      send_message_to_controller(0, 2, szBuff);
   }

   if ( ! (g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
   if ( iDataRateTx < -3 )
   if ( g_TimeNow > s_uLastTimeSendDRMsg + 5000 )
   {
      s_uLastTimeSendDRMsg = g_TimeNow;
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Packet utils: try to set datarate (%s) to higher than max default when not negociated radio links.", str_format_datarate_inline(iDataRateTx));
      send_message_to_controller(0, 2, szBuff);
   }

   if ( ! (g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
   if ( iDataRateTx > getLegacyDataRatesBPS()[3] )
   if ( g_TimeNow > s_uLastTimeSendDRMsg + 5000 )
   {
      s_uLastTimeSendDRMsg = g_TimeNow;
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Packet utils: try to set datarate (%s) to higher than max default when not negociated radio links.", str_format_datarate_inline(iDataRateTx));
      send_message_to_controller(0, 2, szBuff);
   }

   if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
   {
      if ( iDataRateTx == -1 )
         iDataRateTx = -2;
      else if ( (iDataRateTx > 0) && (iDataRateTx < getLegacyDataRatesBPS()[1]) )
         iDataRateTx = getLegacyDataRatesBPS()[1];
   }

   bool bUseLowerDatarateForThisPacket = false;

   if ( (! negociate_radio_link_is_in_progress()) && (! test_link_is_in_progress()) )
   {
      if ( (pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS) ||
           (pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK) )
         bUseLowerDatarateForThisPacket = true;

      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
      {
         t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacketData + sizeof(t_packet_header));
         // EC packet?
         if ( pPHVS->uCurrentBlockPacketIndex >= pPHVS->uCurrentBlockDataPackets )
         if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags) & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS )
            bUseLowerDatarateForThisPacket = true;
         // Retr packet?
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags) & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS )
            bUseLowerDatarateForThisPacket = true;
      }
   }

   if ( bUseLowerDatarateForThisPacket )
   {
      // If QAM modulations is used on video, don't go lower than QAM modulations
      if ( iDataRateTx < 0 )
      {
         if ( iDataRateTx < -4 )
            iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, -1);
         else if ( (iDataRateTx > -4) && (iDataRateTx < -1) )
            iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, -1);
      }
      else if ( iDataRateTx > 0 )
      {
         if ( iDataRateTx > 24000000 )
            iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, -1);
         else if ( (iDataRateTx < 24000000) && (iDataRateTx > DEFAULT_RADIO_DATARATE_LOWEST) )
            iDataRateTx = getDataRateShiftedByLevels(iDataRateTx, -1);
      }
   }

   s_LastTxDataRatesVideo[iRadioInterfaceIndex] = iDataRateTx;
   return iDataRateTx;
}

int _compute_packet_downlink_datarate_radioflags_tx_power(u8* pPacketData, int iVehicleRadioLink, int iRadioInterfaceIndex)
{
   // Compute radio flags, datarate then tx power
   // If negociate radio is in progress, use those values
   t_packet_header* pPH = (t_packet_header*)pPacketData;

   bool bIsAudioVideoPacket = false;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) == STREAM_ID_AUDIO )
      bIsAudioVideoPacket = true;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) >= STREAM_ID_VIDEO_1 )
      bIsAudioVideoPacket = true;

   //--------------------------------------------
   // Radio flags - begin

   u32 uRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink];

   if ( test_link_is_in_progress() )
   {
      type_radio_links_parameters* pRP = test_link_get_temp_radio_params();
      uRadioFlags = pRP->link_radio_flags[iVehicleRadioLink];
   }
   if ( negociate_radio_link_is_in_progress() )
   {
      if ( pPH->packet_type != PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
         uRadioFlags = negociate_radio_link_get_radio_flags();
      else
      {
         u8 uCommand = pPacketData[sizeof(t_packet_header) + sizeof(u8)];
         if ( uCommand == NEGOCIATE_RADIO_KEEP_ALIVE )
            uRadioFlags = negociate_radio_link_get_radio_flags();
      }
   }

   radio_set_frames_flags(uRadioFlags, g_TimeNow);
   
   // Radio flags - end
   //--------------------------------------------
   // Datarates - begin

   int iDataRateTx = _compute_packet_downlink_datarate(pPacketData, iVehicleRadioLink, iRadioInterfaceIndex, uRadioFlags);
   if ( bIsAudioVideoPacket )
   {
      if ( 0 == s_LastTxDataRatesVideoMaximum[iRadioInterfaceIndex] )
         s_LastTxDataRatesVideoMaximum[iRadioInterfaceIndex] = iDataRateTx;
      else if ( getRealDataRateFromRadioDataRate(iDataRateTx, uRadioFlags, 1) > getRealDataRateFromRadioDataRate(s_LastTxDataRatesVideo[iRadioInterfaceIndex], uRadioFlags, 1) )
         s_LastTxDataRatesVideoMaximum[iRadioInterfaceIndex] = iDataRateTx;
   }

   radio_set_out_datarate(iDataRateTx, pPH->packet_type, g_TimeNow);

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL != pRadioHWInfo )
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
   // Tx power
   int iDataRateVideo = s_LastTxDataRatesVideoMaximum[iRadioInterfaceIndex];
   if ( 0 == iDataRateVideo )
      iDataRateVideo = s_LastTxDataRatesData[iRadioInterfaceIndex];

   _compute_packet_tx_power_on_ieee(iVehicleRadioLink, iRadioInterfaceIndex, iDataRateVideo);

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
   int iAirRate = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLinkId] /8;
   if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      iAirRate = hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex);

   s_LastTxDataRatesData[iRadioInterfaceIndex] = iAirRate*8;
  
   t_packet_header* pPH = (t_packet_header*)pPacketData;
   if ( ! radio_can_send_packet_on_slow_link(iLocalRadioLinkId, pPH->packet_type, 0, g_TimeNow) )
      return false;

   if ( pPH->total_length > 200 )
      return false;

   if ( iAirRate > 0 )
   if ( g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec >= (DEFAULT_RADIO_SERIAL_MAX_TX_LOAD * (u32)iAirRate) / 100 )
   {
      static u32 sl_uLastTimeInterfaceTxOverloaded = 0;
      if ( g_TimeNow > sl_uLastTimeInterfaceTxOverloaded + 20000 )
      {
         sl_uLastTimeInterfaceTxOverloaded = g_TimeNow;
         log_line("Radio interface %d is tx overloaded: sending %d bytes/sec and air data rate is %d bytes/sec", iRadioInterfaceIndex+1, (int)g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec, iAirRate);
         send_alarm_to_controller(ALARM_ID_RADIO_LINK_DATA_OVERLOAD, (g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec & 0xFFFFFF) | (((u32)iRadioInterfaceIndex)<<24),(u32)iAirRate,0);
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
      
   int iWriteResult = radio_tx_send_serial_radio_packet(iRadioInterfaceIndex, (u8*)pPH, pPH->total_length);
   if ( iWriteResult > 0 )
   {      
      int iTotalSent = pPH->total_length;
      if ( g_pCurrentModel->radioLinksParams.iSiKPacketSize > 0 )
         iTotalSent += sizeof(t_packet_header_short) * (int) (pPH->total_length / g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, iTotalSent);
      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, pPH->total_length);
      return true;
   }
   log_softerror_and_alarm("Failed to write to serial radio interface %d.", iRadioInterfaceIndex+1);
   if ( iWriteResult == -2 )
   if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      flag_reinit_sik_interface(iRadioInterfaceIndex);
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
   u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

   bool bIsAudioVideoPacket = false;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) == STREAM_ID_AUDIO )
      bIsAudioVideoPacket = true;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) >= STREAM_ID_VIDEO_1 )
      bIsAudioVideoPacket = true;

   int be = 0;
   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   if ( hpp() )
   {
      if ( bIsAudioVideoPacket )
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         be = 1;
      if ( ! bIsAudioVideoPacket )
      {
         if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_BEACON) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
            be = 1;
         if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
            be = 1;
      }
   }

   int iDataRateTx = _compute_packet_downlink_datarate_radioflags_tx_power(pPacketData, iVehicleRadioLinkId, iRadioInterfaceIndex);

   if ( bIsAudioVideoPacket && (! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)) )
   if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
   {
      t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacketData + sizeof(t_packet_header));
      if ( pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
      {
         t_packet_header_video_segment_debug_info* pDbgInfo = (t_packet_header_video_segment_debug_info*)(pPacketData + nPacketLength - sizeof(t_packet_header_video_segment_debug_info));
         pDbgInfo->uTime2 = get_current_timestamp_ms();
      }
   }

   int totalLength = radio_build_new_raw_ieee_packet(iLocalRadioLinkId, s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_DOWNLINK, be);

   int iRepeatCount = 0;
   if ( pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK )
      iRepeatCount++;

   if ( radio_write_raw_ieee_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength, iRepeatCount) )
   {       
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);
      radio_stats_set_tx_radio_datarate_for_packet(&g_SM_RadioStats, iRadioInterfaceIndex, iLocalRadioLinkId, iDataRateTx, bIsAudioVideoPacket?1:0);

      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, nPacketLength);
      return true;
   }

   log_softerror_and_alarm("Failed to write to radio interface %d (type %s, size: %d bytes, raw: %d bytes)",
      iRadioInterfaceIndex+1,
      str_get_packet_type(pPH->packet_type), pPH->total_length, nPacketLength);
   return false;
}

// Sends a radio packet to all posible radio interfaces or just to a single radio link

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink)
{
   if ( nPacketLength <= 0 )
      return -1;

   //if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS )
   //if ( ((g_TimeNow/1000/10) % 6) == 0 )
   //   return -1;

   // Set packets indexes and tx times if multiple packets are found in the input buffer

   bool bIsAudioVideoPacket = false;
   bool bHasCommandParamsZipResponse = false;
   bool bHasZipParamsPacket = false;

   
   int iZipParamsPacketSize = 0;
   u32 uZipParamsUniqueIndex = 0;
   u8  uZipParamsFlags = 0;
   u8  uZipParamsSegmentIndex = 0;
   u8  uZipParamsTotalSegments = 0;
   u8  uPingReplySendOnLocalRadioLinkId = 0xFF;

   bool bIsRetransmited = false;
   bool bIsPingReplyPacket = false;
   bool bIsLowCapacityLinkOnlyPacket = false;
   bool bIsPingPacket = false;
   int iPingOnLocalRadioLinkId = -1;
   
   t_packet_header* pPHZipParams = NULL;
   t_packet_header_command_response* pPHCRZipParams = NULL;
   t_packet_header* pPH = (t_packet_header*)pPacketData;
   
   if ( pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_SEND_ON_LOW_CAPACITY_LINK_ONLY )
      bIsLowCapacityLinkOnlyPacket = true;
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      bIsRetransmited = true;


   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      int iModelRadioLinkId = pPacketData[sizeof(t_packet_header)+2];
      int iCmdType = pPacketData[sizeof(t_packet_header)+4];

      for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
      {
         int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLinkId].matchingVehicleRadioLinkId;
         if ( iModelRadioLinkId == iVehicleRadioLinkId )
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
         {
            iSendToSingleRadioLink = iRadioLinkId;
         }
      }
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pPacketData + sizeof(t_packet_header));
      if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      {
         bHasCommandParamsZipResponse = true;
         pPHZipParams = pPH;
         pPHCRZipParams = pPHCR;
      }
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      u8 uLocalRadioLinkId = 0;
      memcpy( &uLocalRadioLinkId, pPacketData + sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
      iPingOnLocalRadioLinkId = (int)uLocalRadioLinkId;
      bIsPingPacket = true;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   {
      bIsPingReplyPacket = true;
      memcpy((u8*)&uPingReplySendOnLocalRadioLinkId, pPacketData + sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32), sizeof(u8));
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   {
      bHasZipParamsPacket = true;

      memcpy((u8*)&uZipParamsUniqueIndex, pPacketData + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
      memcpy(&uZipParamsFlags, pPacketData + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u8));

      if ( 0 == uZipParamsFlags )
         iZipParamsPacketSize = pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u32) - sizeof(u8);
      else
      {
         uZipParamsSegmentIndex = *(pPacketData + sizeof(t_packet_header)+2*sizeof(u32) + sizeof(u8));
         uZipParamsTotalSegments = *(pPacketData + sizeof(t_packet_header)+2*sizeof(u32) + 2*sizeof(u8));
         iZipParamsPacketSize = (int)(*(pPacketData + sizeof(t_packet_header)+2*sizeof(u32) + 3*sizeof(u8)));
      }
   }

   u32 uDestVehicleId = pPH->vehicle_id_dest;      
   u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

   if ( pPH->packet_type != PACKET_TYPE_RUBY_PING_CLOCK )
   if ( pPH->packet_type != PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      s_StreamsTxPacketIndex[uStreamId]++;
   pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsTxPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);


   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) == STREAM_ID_AUDIO )
      bIsAudioVideoPacket = true;
   if ( ((pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) >= STREAM_ID_VIDEO_1 )
      bIsAudioVideoPacket = true;

   // Send packet on all radio links that can send this packet or just to the single radio interface that user wants
   // Exception: Ping reply packet is sent only on the associated radio link for this ping

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLinkId].matchingVehicleRadioLinkId;
      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iVehicleRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      if ( (-1 != iSendToSingleRadioLink) && (iRadioLinkId != iSendToSingleRadioLink) )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( bIsPingReplyPacket && (uPingReplySendOnLocalRadioLinkId != 0xFF) )
      if ( iRadioLinkId != (int) uPingReplySendOnLocalRadioLinkId )
         continue;

      // Do not send regular packets to controller using relay links
      if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) ||
           (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iVehicleRadioLinkId) )
         continue;

      // Send Ping reply packets only to the assigned radio link
      if ( bIsPingPacket )
      if ( iRadioLinkId != iPingOnLocalRadioLinkId )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( bIsAudioVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;

      if ( ! bIsAudioVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      if ( bIsAudioVideoPacket  && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
         continue;
      if ( (! bIsAudioVideoPacket) && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;
      
      if ( hardware_radio_index_is_serial_radio(iRadioInterfaceIndex) )
      {
         if ( (! bIsAudioVideoPacket) && (! bIsRetransmited) )
         if ( _send_packet_to_serial_radio_interface(iRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength) )
         {
            bPacketSent = true;
            if ( bHasZipParamsPacket )
            {
               if ( 0 == uZipParamsFlags )
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as single packet to SiK radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, iRadioInterfaceIndex+1 );  
               else
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as small packets (%u of %u) to SiK radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, uZipParamsSegmentIndex, uZipParamsTotalSegments, iRadioInterfaceIndex+1 );  
            }
         }
      }
      else
      {
         if ( bIsLowCapacityLinkOnlyPacket )
            continue;
         if ( _send_packet_to_wifi_radio_interface(iRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength) )
         {
            bPacketSent = true;
            if ( bHasCommandParamsZipResponse )
            {
               if ( pPHZipParams->total_length > sizeof(t_packet_header) + 200 )
                  log_line("Sent radio packet: model zip params command response (%d bytes) as single zip file to radio interface %d, command index: %d, retry: %d",
                     (int)(pPHZipParams->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response)), 
                     iRadioInterfaceIndex+1,
                     (int)pPHCRZipParams->origin_command_counter, (int)pPHCRZipParams->origin_command_resend_counter );
               else
                  log_line("Sent radio packet: model zip params command response (%d bytes) as small segment (%d of %d, unique id %d) zip file to radio interface %d, command index: %d, retry: %d",
                     (int)(pPHZipParams->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response) - 4*sizeof(u8)),
                     1+(int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response) + sizeof(u8))),
                     (int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response) + 2*sizeof(u8))),
                     (int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response))),
                     iRadioInterfaceIndex+1,
                     (int)pPHCRZipParams->origin_command_counter, (int)pPHCRZipParams->origin_command_resend_counter );
            }
            if ( bHasZipParamsPacket )
            {
               if ( 0 == uZipParamsFlags )
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as single packet to radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, iRadioInterfaceIndex+1 );
               else
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as small packets (%u of %u) to radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, uZipParamsSegmentIndex, uZipParamsTotalSegments, iRadioInterfaceIndex+1 );
            }
         }
      }
   }

   if ( ! bPacketSent )
   {
      if ( bIsLowCapacityLinkOnlyPacket )
         return 0;

      if ( test_link_is_in_progress() )
      if ( 1 == g_pCurrentModel->radioLinksParams.links_count )
      {
         static u32 s_uDebugTimeLastErrorPacketNotSent = 0;
         if ( g_TimeNow > s_uDebugTimeLastErrorPacketNotSent + 100 )
         {
            s_uDebugTimeLastErrorPacketNotSent = g_TimeNow;
            log_line("Packet not sent on tx interface. Test link params is in progress.");
         }
         return 0;
      }
      log_softerror_and_alarm("Packet not sent! No radio interface could send it (%s, type: %d, %s, %d bytes). %d radio links. %s",
         bIsAudioVideoPacket?"audio/video packet":"data packet",
         pPH->packet_type, str_get_packet_type(pPH->packet_type), nPacketLength,
         g_pCurrentModel->radioLinksParams.links_count,
         bIsLowCapacityLinkOnlyPacket?"Low capacity link packet":"No link specific flags (low/high capacity)");
      return 0;
   }

   // Packet sent. Update stats and info

   radio_stats_update_on_packet_sent_for_radio_stream(&g_SM_RadioStats, g_TimeNow, uDestVehicleId, uStreamId, pPH->packet_type, pPH->total_length);

   g_PHVehicleTxStats.tmp_uAverageTxCount++;

   if ( g_PHVehicleTxStats.historyTxPackets[0] < 255 )
      g_PHVehicleTxStats.historyTxPackets[0]++;

   u32 uTxGap = g_TimeNow - g_TimeLastTxPacket;
   g_TimeLastTxPacket = g_TimeNow;

   if ( uTxGap > 254 )
      uTxGap = 254;

   g_PHVehicleTxStats.tmp_uAverageTxSum += uTxGap;

   if ( 0xFF == g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] = uTxGap;
   if ( 0xFF == g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] = uTxGap;


   if ( uTxGap > g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] = uTxGap;
   if ( uTxGap < g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] = uTxGap;

   if ( bIsAudioVideoPacket )
   {
      s_countTXVideoPacketsOutTemp++;
   }
   else
   {
      s_countTXDataPacketsOutTemp++;
      s_countTXCompactedPacketsOutTemp++;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
   

   #ifdef LOG_RAW_TELEMETRY
   t_packet_header* pPH = (t_packet_header*) pPacketData;
   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
   {
      t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pPacketData + sizeof(t_packet_header));
      log_line("[Raw_Telem] Send raw telemetry packet to radio interfaces, index %u, %d bytes", pPHTR->telem_segment_index, pPH->total_length);
   }
   #endif

   return 0;
}

void send_packet_vehicle_log(u8* pBuffer, int length)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_LOG_FILE_SEGMENT, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_file_segment) + (u16)length;

   t_packet_header_file_segment PHFS;
   PHFS.file_id = FILE_ID_VEHICLE_LOG;
   PHFS.segment_index = s_VehicleLogSegmentIndex++;
   PHFS.segment_size = (u32) length;
   PHFS.total_segments = MAX_U32;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&PHFS, sizeof(t_packet_header_file_segment));
   memcpy(packet + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment), pBuffer, length);

   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
}

void _send_alarm_packet_to_radio_queue(u32 uAlarmIndex, u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_ALARM, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &uAlarmIndex, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uAlarm, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uFlags1, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u32), &uFlags2, sizeof(u32));
   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

   char szBuff[128];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
   log_line("Sent alarm packet to router: %s, alarm index: %u, repeat count: %u", szBuff, uAlarmIndex, uRepeatCount);

   s_uTimeLastAlarmSentToRouter = g_TimeNow;
}

void send_message_to_controller(int iType, int iRepeatCount, const char* szMessage)
{
   static u16 s_uMsgToControllerIdCounter = 0;

   if ( (NULL == szMessage) || (0 == szMessage[0]) )
      return;

   log_line("Sending message index %d to controller, type: %d, %d times, msg: [%s]", s_uMsgToControllerIdCounter, iType, iRepeatCount, szMessage);

   int iLen = (int)strlen(szMessage);
   for( int i=0; i<=iRepeatCount; i++ )
   {
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_MESSAGE, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = 0;
      PH.total_length = sizeof(t_packet_header) + sizeof(u16) + sizeof(u8) + iLen + 1;

      u8 uType = (u8)iType;
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uMsgToControllerIdCounter, sizeof(u16));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u16), &uType, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u16) + sizeof(u8), szMessage, iLen+1);
      packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
   }
   s_uMsgToControllerIdCounter++;
}

void send_alarm_to_controller(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount)
{
   char szBuff[128];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);

   s_uAlarmsIndex++;

   if ( test_link_is_in_progress() )
   {
      log_line("Skip sending alarm to controller (test link in progress): %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);
      return;
   }
   if ( negociate_radio_link_is_in_progress() )
   {
      log_line("Skip sending alarm to controller (negociate radio in progress): %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);
      return;
   }
   log_line("Sending alarm to controller: %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);

   _send_alarm_packet_to_radio_queue(s_uAlarmsIndex, uAlarm, uFlags1, uFlags2, uRepeatCount);

   if ( uRepeatCount <= 1 )
      return;

   if ( s_AlarmsPendingInQueue >= MAX_ALARMS_QUEUE )
   {
      log_softerror_and_alarm("Too many alarms in the queue (%d alarms). Can't queue any more alarms.", s_AlarmsPendingInQueue);
      return;
   }
   uRepeatCount--;
   log_line("Queued alarm %s to queue position %d to be sent %d more times", szBuff, s_AlarmsPendingInQueue, uRepeatCount );

   s_AlarmsQueue[s_AlarmsPendingInQueue].uIndex = s_uAlarmsIndex;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uId = uAlarm;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uFlags1 = uFlags1;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uFlags2 = uFlags2;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uRepeatCount = uRepeatCount;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uStartTime = g_TimeNow;
   s_AlarmsPendingInQueue++;
}


void send_alarm_to_controller_now(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount)
{
   char szBuff[128];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);

   s_uAlarmsIndex++;

   if ( test_link_is_in_progress() )
   {
      log_line("Skip sending alarm to controller (test link in progress): %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);
      return;
   }
   if ( negociate_radio_link_is_in_progress() )
   {
      log_line("Skip sending alarm to controller (negociate radio in progress): %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);
      return;
   }
   log_line("Sending alarm to controller: %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);

   for( u32 u=0; u<uRepeatCount; u++ )
   {
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_ALARM, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = 0;
      PH.total_length = sizeof(t_packet_header) + 4*sizeof(u32);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uAlarmsIndex, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uAlarm, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uFlags1, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+3*sizeof(u32), &uFlags2, sizeof(u32));
      send_packet_to_radio_interfaces(packet, PH.total_length, -1);

      alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
      log_line("Sent alarm packet to radio: %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);

      s_uTimeLastAlarmSentToRouter = g_TimeNow;
      hardware_sleep_ms(50);
   }
}

void send_pending_alarms_to_controller()
{
   if ( s_AlarmsPendingInQueue == 0 )
      return;

   if ( g_TimeNow < s_uTimeLastAlarmSentToRouter + 150 )
      return;

   if ( s_AlarmsQueue[0].uRepeatCount == 0 )
   {
      for( int i=0; i<s_AlarmsPendingInQueue-1; i++ )
         memcpy(&(s_AlarmsQueue[i]), &(s_AlarmsQueue[i+1]), sizeof(t_alarm_info));
      s_AlarmsPendingInQueue--;
      if ( s_AlarmsPendingInQueue == 0 )
         return;
   }

   if ( (! test_link_is_in_progress()) && (! negociate_radio_link_is_in_progress()) )
      _send_alarm_packet_to_radio_queue(s_AlarmsQueue[0].uIndex, s_AlarmsQueue[0].uId, s_AlarmsQueue[0].uFlags1, s_AlarmsQueue[0].uFlags2, s_AlarmsQueue[0].uRepeatCount );
   s_AlarmsQueue[0].uRepeatCount--;
}