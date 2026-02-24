/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/utils.h"
#include "../base/hardware_procs.h"
#include "../base/ctrl_preferences.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"

void test_link_state_get_state_string(int iState, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   strcpy(szBuffer, "N/A");
   if ( iState == TEST_LINK_STATE_NONE )
      strcpy(szBuffer, "None");
   if ( iState == TEST_LINK_STATE_START )
      strcpy(szBuffer, "Start");
   if ( iState == TEST_LINK_STATE_APPLY_RADIO_PARAMS )
      strcpy(szBuffer, "Apply Radio Params");
   if ( iState == TEST_LINK_STATE_PING_UPLINK )
      strcpy(szBuffer, "Ping Uplink");
   if ( iState == TEST_LINK_STATE_PING_DOWNLINK )
      strcpy(szBuffer, "Ping Downlink");
   if ( iState == TEST_LINK_STATE_REVERT_RADIO_PARAMS )
      strcpy(szBuffer, "Revert radio params");
   if ( iState == TEST_LINK_STATE_END )
      strcpy(szBuffer, "End");
   if ( iState == TEST_LINK_STATE_ENDED )
      strcpy(szBuffer, "Ended");
}


void update_atheros_card_datarate(Model* pModel, int iInterfaceIndex, int iDataRateBPS, shared_mem_process_stats* pProcessStats)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return;
   if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
        (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
      return;

   if ( pRadioHWInfo->iCurrentDataRateBPS == iDataRateBPS )
   {
      log_line("Atheros/RaLink radio interface %d datarate is already at %d bps.", iInterfaceIndex+1, pRadioHWInfo->iCurrentDataRateBPS );
      return;
   }
   log_line("Must update Atheros/RaLink radio interface %d datarate from %d to %d bps.", iInterfaceIndex+1, pRadioHWInfo->iCurrentDataRateBPS, iDataRateBPS );

   radio_rx_pause_interface(iInterfaceIndex, "Update Atheros datarate");
   radio_tx_pause_radio_interface(iInterfaceIndex, "Update Atheros datarate");

   bool bWasOpenedForWrite = false;
   bool bWasOpenedForRead = false;
   if ( pRadioHWInfo->openedForWrite )
   {
      bWasOpenedForWrite = true;
      radio_close_interface_for_write(iInterfaceIndex);
   }
   if ( pRadioHWInfo->openedForRead )
   {
      bWasOpenedForRead = true;
      radio_close_interface_for_read(iInterfaceIndex);
   }
   u32 uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   radio_utils_set_datarate_atheros(pModel, iInterfaceIndex, iDataRateBPS, 0);

   uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   if ( bWasOpenedForRead )
      radio_open_interface_for_read(iInterfaceIndex, RADIO_PORT_ROUTER_DOWNLINK);

   if ( bWasOpenedForWrite )
      radio_open_interface_for_write(iInterfaceIndex);

   hardware_save_radio_info();

   uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   radio_tx_resume_radio_interface(iInterfaceIndex);
   radio_rx_resume_interface(iInterfaceIndex);
}


