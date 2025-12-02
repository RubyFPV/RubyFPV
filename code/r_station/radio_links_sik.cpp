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

#include "radio_links_sik.h"
#include "timers.h"
#include "shared_vars.h"
#include "../base/ctrl_settings.h"
#include "../base/models.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_procs.h"
#include "../common/radio_stats.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "ruby_rt_station.h"

void radio_links_close_and_mark_sik_interfaces_to_reopen()
{
   int iCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( pRadioHWInfo->openedForWrite || pRadioHWInfo->openedForRead )
      if ( (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == -1 ) ||
           (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == i) )
      {
         radio_rx_pause_interface(i, "SiK config start, close interfaces");
         radio_tx_pause_radio_interface(i, "SiK config start, close interfaces");
         g_SiKRadiosState.bInterfacesToReopen[i] = true;
         hardware_radio_sik_close(i);
         iCount++;
      }
   }
   log_line("[Router] Closed SiK radio interfaces (%d closed) and marked them for reopening.", iCount);
}

void radio_links_reopen_marked_sik_interfaces()
{
   int iCount = 0;
   int iCountSikInterfacesOpened = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( g_SiKRadiosState.bInterfacesToReopen[i] )
      {
         if ( hardware_radio_sik_open_for_read_write(i) <= 0 )
            log_softerror_and_alarm("[Router] Failed to reopen SiK radio interface %d", i+1);
         else
         {
            iCount++;
            iCountSikInterfacesOpened++;
            radio_tx_resume_radio_interface(i);
            radio_rx_resume_interface(i);
         }
      }
      g_SiKRadiosState.bInterfacesToReopen[i] = false;
   }

   log_line("[Router] Reopened SiK radio interfaces (%d reopened).", iCount);
}

void radio_links_flag_update_sik_interface(int iInterfaceIndex)
{
   if ( g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex >= 0 )
   {
      log_line("Router was re-flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
      return;
   }
   log_line("Router was flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;
   radio_links_close_and_mark_sik_interfaces_to_reopen();

   //send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0);
}


void radio_links_flag_reinit_sik_interface(int iInterfaceIndex)
{
   // Already flagged ?

   if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
      return;

   log_softerror_and_alarm("Router was flagged to reinit SiK radio interfaces.");
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = -1;

   radio_links_close_and_mark_sik_interfaces_to_reopen();

   g_SiKRadiosState.bMustReinitSiKInterfaces = true;
   g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown = (u32) iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;
   send_alarm_to_central(ALARM_ID_RADIO_INTERFACE_DOWN, g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown, 0);
}

static void * _reinit_sik_thread_func(void *ignored_argument)
{
   log_line("[Router-SiKThread] Reinitializing SiK radio interfaces...");
   hw_log_current_thread_attributes("sik reinit");

   // radio serial ports are already closed at this point

   if ( g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex >= 0 )
   {
      log_line("[Router-SiKThread] Must reconfigure and reinitialize SiK radio interface %d...", g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex+1 );
      if ( ! hardware_radio_index_is_sik_radio(g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex) )
         log_softerror_and_alarm("[Router-SiKThread] Radio interface %d is not a SiK radio interface.", g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex+1 );
      else
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex);
         if ( NULL == pRadioHWInfo )
            log_softerror_and_alarm("[Router-SiKThread] Failed to get radio hw info for radio interface %d.", g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex+1);
         else
         {
            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);

            u32 uFreqKhz = pRadioHWInfo->uHardwareParamsList[8];
            u32 uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
            u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
            if ( NULL != pCRII )
               uTxPower = pCRII->iRawPowerLevel;
            u32 uLBT = 0;
            u32 uECC = 0;
            u32 uMCSTR = 0;

            if ( NULL != g_pCurrentModel )
            {
               int iRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex];
               if ( (iRadioLink >= 0) && (iRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
               {
                  uFreqKhz = g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink];
                  uDataRate = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink];
                  uECC = (g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAGS_SIK_ECC)? 1:0;
                  uLBT = (g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAGS_SIK_LBT)? 1:0;
                  uMCSTR = (g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAGS_SIK_MCSTR)? 1:0;
               
                  bool bDataRateOk = false;
                  for( int i=0; i<getSiKAirDataRatesCount(); i++ )
                  {
                     if ( (int)uDataRate == getSiKAirDataRates()[i] )
                     {
                        bDataRateOk = true;
                        break;
                     }
                  }

                  if ( ! bDataRateOk )
                  {
                     log_softerror_and_alarm("[Router-SiKThread] Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
                     uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
                  }
               }
            }
            int iRes = hardware_radio_sik_set_params(pRadioHWInfo, 
                   uFreqKhz,
                   DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                   DEFAULT_RADIO_SIK_NETID,
                   uDataRate, uTxPower, 
                   uECC, uLBT, uMCSTR,
                   NULL);
            if ( iRes != 1 )
            {
               log_softerror_and_alarm("[Router-SiKThread] Failed to reconfigure SiK radio interface %d", g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex+1);
               if ( g_SiKRadiosState.iThreadRetryCounter < 3 )
               {
                  log_line("[Router-SiKThread] Will retry to reconfigure radio. Retry counter: %d", g_SiKRadiosState.iThreadRetryCounter);
               }
               else
               {
                  send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED, 0);
                  radio_links_reopen_marked_sik_interfaces();
  
                  g_SiKRadiosState.bMustReinitSiKInterfaces = false;
                  g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = -1;
                  g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown = MAX_U32;
               }
               log_line("[Router-SiKThread] Finished.");
               g_SiKRadiosState.bConfiguringSiKThreadWorking = false;
               return NULL;
            }
            else
            {
               log_line("[Router-SiKThread] Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex+1,
                   uTxPower, uDataRate, uECC, uLBT, uMCSTR);
               radio_stats_set_card_current_frequency(&g_SM_RadioStats, g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex, uFreqKhz);
               if ( NULL != g_pSM_RadioStats )
                  memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
            }
         }
      }
   }
   else if ( 1 != hardware_radio_sik_reinitialize_serial_ports() )
   {
      log_line("[Router-SiKThread] Reinitializing of SiK radio interfaces failed (not the same ones are present yet).");
      // Will restart the thread to try again
      log_line("[Router-SiKThread] Finished.");
      g_SiKRadiosState.bConfiguringSiKThreadWorking = false;
      return NULL;
   }
   
   log_line("[Router-SiKThread] Reinitialized SiK radio interfaces successfully.");
   
   radio_links_reopen_marked_sik_interfaces();

   if ( g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex >= 0 )
      send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE, 0);
   else
      send_alarm_to_central(ALARM_ID_RADIO_INTERFACE_REINITIALIZED, g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown, 0);
   
   g_SiKRadiosState.bMustReinitSiKInterfaces = false;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = -1;
   g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown = MAX_U32;
   log_line("[Router-SiKThread] Finished.");
   g_SiKRadiosState.bConfiguringSiKThreadWorking = false;
   radio_rx_reset_interfaces_broken_state();

   return NULL;
}

int radio_links_check_reinit_sik_interfaces()
{
   if ( g_SiKRadiosState.bConfiguringToolInProgress && (g_SiKRadiosState.uTimeStartConfiguring != 0) )
   if ( g_TimeNow >= g_SiKRadiosState.uTimeStartConfiguring+500 )
   {
      if ( hw_process_exists("ruby_sik_config") )
      {
         g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow - 400;
      }
      else
      {
         int iResult = -1;
         char szFile[128];
         strcpy(szFile, FOLDER_RUBY_TEMP);
         strcat(szFile, FILE_TEMP_SIK_CONFIG_FINISHED);
         FILE* fd = fopen(szFile, "rb");
         if ( NULL != fd )
         {
            if ( 1 != fscanf(fd, "%d", &iResult) )
               iResult = -2;
            fclose(fd);
         }
         log_line("SiK radio configuration tool completed. Result: %d.", iResult);
         sprintf(szFile, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
         hw_execute_bash_command(szFile, NULL);
         g_SiKRadiosState.bConfiguringToolInProgress = false;
         radio_links_reopen_marked_sik_interfaces();
         send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE, 0);
         return 0;
      }
   }
   
   if ( (! g_SiKRadiosState.bMustReinitSiKInterfaces) && (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == -1) )
      return 0;

   if ( g_SiKRadiosState.bConfiguringToolInProgress )
      return 0;

   if ( g_SiKRadiosState.bConfiguringSiKThreadWorking )
      return 0;
   
   if ( g_TimeNow < g_SiKRadiosState.uTimeLastSiKReinitCheck + g_SiKRadiosState.uTimeIntervalSiKReinitCheck )
      return 0;

   g_SiKRadiosState.uTimeLastSiKReinitCheck = g_TimeNow;
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck += 200;

   g_SiKRadiosState.bConfiguringSiKThreadWorking = true;
   static pthread_t pThreadSiKReinit;

   if ( 0 != pthread_create(&pThreadSiKReinit, NULL, &_reinit_sik_thread_func, NULL) )
   {
      log_softerror_and_alarm("[Router] Failed to create worker thread to reinit SiK radio interfaces.");
      g_SiKRadiosState.bConfiguringSiKThreadWorking = false;
      return 0;
   }
   else
      pthread_detach(pThreadSiKReinit);

   log_line("[Router] Created thread to reinit SiK radio interfaces.");
   if ( 0 == g_SiKRadiosState.iThreadRetryCounter )
      send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0);
   g_SiKRadiosState.iThreadRetryCounter++;
   return 1;
}
