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

#include "menu.h"
#include "menu_negociate_radio.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "menu_vehicle_radio_rt_capab.h"
#include "../process_router_messages.h"
#include "../warnings.h"
#include "../osd/osd_common.h"
#include "../../base/tx_powers.h"
#include "../../common/models_connect_frequencies.h"
#include <math.h>

#define MAX_FAILING_RADIO_NEGOCIATE_STEPS 4
#define SINGLE_TEST_DURATION 700
#define SINGLE_TEST_DURATION_POWER 1000
#define TEST_DATARATE_QUALITY_THRESHOLD 0.7

//------------------------------------------------------------------
// Test states and actions
//
// NONE
//        do no action, no periodic work

// START_TEST
//        switch it to sending vehicle start command

// TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION
//        periodically send start test to vehicle, untill confirmation or timeout
//        periodically call _currentTestUpdateWhenRunning() to see if we need to change state
//        on timeout, end test
//        on confirmation from vehicle, switch to state TEST_RUNNING

// TEST_RUNNING
//        if waiting for user input: periodically, if we received confirmation from vehicle, compute test metrics
//        if not waiting for user input, periodically call _currentTestUpdateWhenRunning() to see if we need to change state

// TEST_ENDED
//        if waiting for user input: do nothing, just wait
//        if not waiting for user input, advance to next test: _advance_to_next_test()
//------------------------------------------------------------------
#define NEGOCIATE_STATE_NONE 0
#define NEGOCIATE_STATE_START_TEST 10
#define NEGOCIATE_STATE_TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION 20
#define NEGOCIATE_STATE_TEST_RUNNING 30
#define NEGOCIATE_STATE_TEST_ENDED 40
#define NEGOCIATE_STATE_SET_VIDEO_SETTINGS 50
#define NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION 60
#define NEGOCIATE_STATE_SET_RADIO_SETTINGS 70
#define NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_RADIO_SETTINGS_CONFIRMATION 80
#define NEGOCIATE_STATE_END_TESTS 90
#define NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION 100
#define NEGOCIATE_STATE_ENDED 200

#define NEGOCIATE_USER_STATE_NONE 0
#define NEGOCIATE_USER_STATE_WAIT_CANCEL 1
#define NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION 2
#define NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_VIDEO_CONFIRMATION 3
#define NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_RADIO_CONFIRMATION 4
#define NEGOCIATE_USER_STATE_WAIT_VIDEO_CONFIRMATION 5
#define NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION 6
#define NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION 7

char* str_get_negociate_state(int iState)
{
   static char s_szNegociateRadioState[128];
   s_szNegociateRadioState[0] = 0;
   strcpy(s_szNegociateRadioState, "N/A");
   switch ( iState )
   {
      case NEGOCIATE_STATE_NONE: strcpy(s_szNegociateRadioState, "None"); break;
      case NEGOCIATE_STATE_START_TEST: strcpy(s_szNegociateRadioState, "Start Test"); break;
      case NEGOCIATE_STATE_TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION: strcpy(s_szNegociateRadioState, "Wait Start Test Vehicle Confirmation"); break;
      case NEGOCIATE_STATE_TEST_RUNNING: strcpy(s_szNegociateRadioState, "Test Running"); break;
      case NEGOCIATE_STATE_TEST_ENDED: strcpy(s_szNegociateRadioState, "Test Ended"); break;
      case NEGOCIATE_STATE_SET_VIDEO_SETTINGS: strcpy(s_szNegociateRadioState, "Set Vehicle Video Settings"); break;
      case NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION: strcpy(s_szNegociateRadioState, "Wait Vehicle Apply Video Settings"); break;
      case NEGOCIATE_STATE_SET_RADIO_SETTINGS: strcpy(s_szNegociateRadioState, "Set Vehicle Radio Settings"); break;
      case NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_RADIO_SETTINGS_CONFIRMATION: strcpy(s_szNegociateRadioState, "Wait Vehicle Apply Radio Settings"); break;
      case NEGOCIATE_STATE_END_TESTS: strcpy(s_szNegociateRadioState, "End Tests"); break;
      case NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION: strcpy(s_szNegociateRadioState, "Wait Vehicle End Confirmation"); break;
      case NEGOCIATE_STATE_ENDED: strcpy(s_szNegociateRadioState, "Ended"); break;
   }
   return s_szNegociateRadioState;
}

char* str_get_negociate_user_state(int iUserState)
{
   static char s_szNegociateRadioUserState[128];
   s_szNegociateRadioUserState[0] = 0;
   strcpy(s_szNegociateRadioUserState, "N/A");
   switch ( iUserState )
   {
      case NEGOCIATE_USER_STATE_NONE: strcpy(s_szNegociateRadioUserState, "None"); break;
      case NEGOCIATE_USER_STATE_WAIT_CANCEL: strcpy(s_szNegociateRadioUserState, "Wait Cancel"); break;
      case NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait Failed Confirmation"); break;
      case NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_VIDEO_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait Failed Video Apply Confirmation"); break;
      case NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_RADIO_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait Failed Radio Apply Confirmation"); break;
      case NEGOCIATE_USER_STATE_WAIT_VIDEO_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait video apply confirmation"); break;
      case NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait MCS confirmation"); break;
      case NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION: strcpy(s_szNegociateRadioUserState, "Wait finish confirmation"); break;
   }
   return s_szNegociateRadioUserState;
}


MenuNegociateRadio::MenuNegociateRadio(void)
:Menu(MENU_ID_NEGOCIATE_RADIO, L("Initial Auto Radio Link Adjustment"), NULL)
{
   m_Width = 0.72;
   m_xPos = 0.14; m_yPos = 0.26;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   addExtraHeightAtEnd(7.0*height_text + height_text * 1.5 * hardware_get_radio_interfaces_count());
   m_uShowTime = g_TimeNow;
   m_MenuIndexCancel = -1;
   m_iCountInterfacesToTest = 0;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bTestInterfaceIndex[i] = false;
      m_uRadioInterfacesSupportedRadioFlags[i] = 0;
      m_uRadioLinksTxFlagsToApply[i] = 0;
      m_uRadioLinksRxFlagsToApply[i] = 0;
   }
   m_iLoopCounter = 0;
   m_szStatusMessage[0] = 0;
   m_szStatusMessage2[0] = 0;
   m_szStatusMessage3[0] = 0;
   addTopLine(L("Doing the initial radio link parameters adjustment for best performance..."));
   addTopLine(L("(This is done on first installation and on first pairing with a vehicle or when hardware has changed on the vehicle)"));

   _reset_tests_and_state();
}

MenuNegociateRadio::~MenuNegociateRadio()
{
}

void MenuNegociateRadio::_reset_tests_and_state()
{
   log_line("[NegociateRadioLink] Reset state.");

   m_iState = NEGOCIATE_STATE_START_TEST;
   m_iUserState = NEGOCIATE_USER_STATE_NONE;
   m_bCanceled = false;
   m_bFailed = false;
   m_bUpdated = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bTestInterfaceIndex[i] = false;

      m_iIndexFirstRadioFlagsTest[i] = -1;
      m_iIndexLastRadioFlagsTest[i] = -1;
      m_iIndexFirstDatarateLegacyTest[i] = -1;
      m_iIndexFirstDatarateMCSTest[i] = -1;
      m_iIndexLastDatarateLegacyTest[i] = -1;
      m_iIndexLastDatarateMCSTest[i] = -1;
      m_iIndexFirstRadioPowersTest[i] = -1;
      m_iIndexFirstRadioPowersTestMCS[i] = -1;
      m_iIndexLastRadioPowersTestMCS[i] = -1;

      m_iIndexFirstRadioInterfaceTest[i] = -1;
      m_iIndexLastRadioInterfaceTest[i] = -1;
      m_iTestIndexSTBCV[i] = -1;
      m_iTestIndexLDPVV[i] = -1;
      m_iTestIndexSTBCLDPCV[i] = -1;

      m_iCountSucceededTests[i] = 0;
      m_iCountFailedTests[i] = 0;
      m_iCountFailedTestsDatarates[i] = 0;
   }

   for( int i=0; i<MAX_NEGOCIATE_TESTS; i++ )
   {
      memset(&(m_TestsInfo[i]), 0, sizeof(type_negociate_radio_step));
      m_TestsInfo[i].bSkipTest = false;
      m_TestsInfo[i].bMustTestUplink = false;

      m_TestsInfo[i].bHasSubTests = false;
      m_TestsInfo[i].iCurrentSubTest = -1;

      m_TestsInfo[i].iVehicleRadioInterface = -1;
      m_TestsInfo[i].iVehicleRadioLink = -1;
      m_TestsInfo[i].bSucceeded = false;
      m_TestsInfo[i].bReceivedEnoughData = false;
      m_TestsInfo[i].bComputedQualities = false;
      m_TestsInfo[i].fComputedQualityMax = -1.0;
      m_TestsInfo[i].fComputedQualityMin = -1.0;
      m_TestsInfo[i].uTimeStarted = 0;
      m_TestsInfo[i].uTimeEnded = 0;
      m_TestsInfo[i].iCountSendStarts = 0;
      m_TestsInfo[i].uDurationToTest = 1000;
      m_TestsInfo[i].uDurationSubTest = 1000;
      m_TestsInfo[i].uExtraStartDelay = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         m_TestsInfo[i].iRadioInterfacesRXPackets[k] = 0;
         m_TestsInfo[i].iRadioInterfacesRxLostPackets[k] = 0;
         m_TestsInfo[i].fQualityCards[k] = -1.0;
      }
   }
   m_iTestsCount = 0;

   u32 uCtrlSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (!pRadioHWInfo->isSupported) )
         continue;
      if ( ! pRadioHWInfo->isConfigurable )
         continue;
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         continue;

      if ( controllerIsCardTXOnly(pRadioHWInfo->szMAC) )
         continue;
      uCtrlSupportedBands = uCtrlSupportedBands | pRadioHWInfo->supportedBands;
   }

   m_iCountInterfacesToTest = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      m_bTestInterfaceIndex[i] = false;
      if ( ! hardware_radio_type_is_wifi(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]) )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      int iLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( (iLinkId < 0) || (iLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( 0 == (uCtrlSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i]) )
         continue;

      log_line("[NegociateRadioLink] Will test vehicle's radio interface %d (out of %d interfaces), %s, radio link %d, freq: %s, main connect freq: %u Mhz",
         i+1, g_pCurrentModel->radioInterfacesParams.interfaces_count,
         str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[i]),
         iLinkId+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iLinkId]),
         get_model_main_connect_frequency(g_pCurrentModel->uVehicleId)/1000);

      m_bTestInterfaceIndex[i] = true;
      m_iCountInterfacesToTest++;
   }

   if ( 0 == m_iCountInterfacesToTest )
   {
      log_softerror_and_alarm("[NegociateRadioLink] Can't find a valid main vehicle radio interface to test, vehicle has %d radio interfaces and %d radio links.", g_pCurrentModel->radioInterfacesParams.interfaces_count, g_pCurrentModel->radioLinksParams.links_count);
      strcpy(m_szStatusMessage, L("No interfaces to test and optimize."));
      strcpy(m_szStatusMessage2, L("All your radio interfaces are already configured."));
   }
   else
   {
      strcpy(m_szStatusMessage, L("Please wait, it will take a minute."));
      strcpy(m_szStatusMessage2, L("Testing radio modulation schemes"));
   }

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;

      log_line("[NegociateRadioLink] Adding tests for vehicle's radio interface %d: %s", iInt+1, str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iInt]));

      m_iIndexFirstRadioInterfaceTest[iInt] = m_iTestsCount;
      //-----------------------------------------------
      // Radio flags tests

      m_iIndexFirstRadioFlagsTest[iInt] = m_iTestsCount;
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*3;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for MCS rate capability.", m_iTestsCount - m_iIndexFirstRadioFlagsTest[iInt], m_iIndexFirstRadioFlagsTest[iInt], m_iTestsCount-1);

      // Increase first test on each radio as we might change frequency
      m_TestsInfo[m_iIndexFirstRadioInterfaceTest[iInt]].uDurationToTest += 2000;
      m_TestsInfo[m_iIndexFirstRadioInterfaceTest[iInt]].uExtraStartDelay = 1000;

      m_iTestIndexSTBCV[iInt] = m_iTestsCount;
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_STBC;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for STBC flag.", m_iTestsCount - m_iTestIndexSTBCV[iInt], m_iTestIndexSTBCV[iInt], m_iTestsCount-1);

      m_iTestIndexLDPVV[iInt] = m_iTestsCount;
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_LDPC;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for LDPV flag.", m_iTestsCount - m_iTestIndexLDPVV[iInt], m_iTestIndexLDPVV[iInt], m_iTestsCount-1);

      m_iTestIndexSTBCLDPCV[iInt] = m_iTestsCount;
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_STBC | RADIO_FLAG_LDPC;
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for STBC-LDPC flag.", m_iTestsCount - m_iTestIndexSTBCLDPCV[iInt], m_iTestIndexSTBCLDPCV[iInt], m_iTestsCount-1);

      m_iIndexLastRadioFlagsTest[iInt] = m_iTestsCount-1;

      //-----------------------------------------------------
      // Rates tests

      m_iIndexFirstDatarateLegacyTest[iInt] = m_iTestsCount;
      int iCountTestsLegacy = getTestDataRatesCountLegacy();
      if ( iCountTestsLegacy > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
         iCountTestsLegacy = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;

      for( int i=0; i<iCountTestsLegacy; i++ )
      {
         for( int k=0; k<2; k++ )
         {
            m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
            m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesLegacy()[i];
            m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_LEGACY_DATARATES;
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
            m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION;
            if ( ((i == 0) && (k == 0)) || (i == 3) )
               m_TestsInfo[m_iTestsCount].uDurationToTest *=2;
            m_iTestsCount++;
         }
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for Legacy data rates.", m_iTestsCount - m_iIndexFirstDatarateLegacyTest[iInt], m_iIndexFirstDatarateLegacyTest[iInt], m_iTestsCount-1);
      m_iIndexLastDatarateLegacyTest[iInt] = m_iTestsCount-1;

      m_iIndexFirstDatarateMCSTest[iInt] = m_iTestsCount;
      int iCountTestsMCS = getTestDataRatesCountMCS();
      if ( iCountTestsMCS > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
         iCountTestsMCS = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;

      for( int i=0; i<iCountTestsMCS; i++ )
      {
         for( int k=0; k<2; k++ )
         {
            m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
            m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesMCS()[i];
            m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_MCS_DATARATES;
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
            m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION;
            if ( ((i == 0) && (k == 0)) || (i == 3) )
               m_TestsInfo[m_iTestsCount].uDurationToTest *=2;
            m_iTestsCount++;
         }
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for MCS data rates.", m_iTestsCount - m_iIndexFirstDatarateMCSTest[iInt], m_iIndexFirstDatarateMCSTest[iInt], m_iTestsCount-1);
      m_iIndexLastDatarateMCSTest[iInt] = m_iTestsCount-1;


      //-----------------------------------------------
      // Tx powers tests

      m_iIndexFirstRadioPowersTest[iInt] = m_iTestsCount;

      int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[iInt];
      if ( iRadioInterfacelModel < 0 )
         iRadioInterfacelModel = -iRadioInterfacelModel;
      int iRadioInterfacePowerMaxMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);
      log_line("[NegociateRadioLink] Vehicle radio interface %d card type: %s, max power: %d mW", iInt+1, str_get_radio_card_model_string(iRadioInterfacelModel), iRadioInterfacePowerMaxMw);

      for( int i=0; i<6/*getTestDataRatesCountLegacy()*/; i++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesLegacy()[i];
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_LEGACY_DATARATES;
         m_TestsInfo[m_iTestsCount].iTxPowerLastGood = -1;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMin = 1;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax = iRadioInterfacePowerMaxMw;
         if ( i < 3 )
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax*0.9;
         else
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = (m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMin + m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax) / 3;
         m_TestsInfo[m_iTestsCount].bHasSubTests = true;
         m_TestsInfo[m_iTestsCount].iCurrentSubTest = 0;
         m_TestsInfo[m_iTestsCount].uDurationToTest = 100000;
         m_TestsInfo[m_iTestsCount].uDurationSubTest = SINGLE_TEST_DURATION_POWER*1.5;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for Legacy powers.", m_iTestsCount - m_iIndexFirstRadioPowersTest[iInt], m_iIndexFirstRadioPowersTest[iInt], m_iTestsCount-1);
      m_iIndexFirstRadioPowersTestMCS[iInt] = m_iTestsCount;

      for( int i=0; i<iCountTestsMCS; i++ )
      {
         m_TestsInfo[m_iTestsCount].iVehicleRadioInterface = iInt;
         m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesMCS()[i];
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_MCS_DATARATES;
         m_TestsInfo[m_iTestsCount].iTxPowerLastGood = -1;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMin = 1;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax = iRadioInterfacePowerMaxMw;
         if ( i < 3 )
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax*0.9;
         else
            m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = (m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMin + m_TestsInfo[m_iTestsCount].iTxPowerMwToTestMax) / 3;
         m_TestsInfo[m_iTestsCount].bHasSubTests = true;
         m_TestsInfo[m_iTestsCount].iCurrentSubTest = 0;
         m_TestsInfo[m_iTestsCount].uDurationToTest = 100000;
         m_TestsInfo[m_iTestsCount].uDurationSubTest = SINGLE_TEST_DURATION_POWER*1.5;
         m_iTestsCount++;
      }
      log_line("[NegociateRadioLink] Added %d tests (%d to %d) for MCS powers.", m_iTestsCount - m_iIndexFirstRadioPowersTestMCS[iInt], m_iIndexFirstRadioPowersTestMCS[iInt], m_iTestsCount-1);
      m_iIndexLastRadioPowersTestMCS[iInt] = m_iTestsCount-1;
      m_iIndexLastRadioInterfaceTest[iInt] = m_iTestsCount-1;
   }

   for( int i=0; i<m_iTestsCount; i++ )
   {
      m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_FRAME_TYPE_DATA;
      if ( m_TestsInfo[i].iVehicleRadioInterface >= 0 )
         m_TestsInfo[i].iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[m_TestsInfo[i].iVehicleRadioInterface];
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      m_uRadioLinksTxFlagsToApply[i] = g_pCurrentModel->radioLinksParams.link_radio_flags_tx[i];
      m_uRadioLinksRxFlagsToApply[i] = g_pCurrentModel->radioLinksParams.link_radio_flags_rx[i];
   }
   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;
      m_uRadioInterfacesSupportedRadioFlags[iInt] = RADIO_FLAGS_FRAME_TYPE_DATA | RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES;
      m_uRadioInterfacesSupportedRadioFlags[iInt] |= g_pCurrentModel->radioInterfacesParams.interface_supported_radio_flags[iInt];
      m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate[iInt] = 0;
      m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iInt] = 0;

   }
   m_uTimeStartedVehicleOperation = 0;
   m_uLastTimeSentVehicleOperation = 0;
   g_bAskedForNegociateRadioLink = true;

   send_pause_adaptive_to_router(6000);

   m_iCurrentTestIndex = 0;
   m_iCurrentTestRadioInterfaceIndex = m_TestsInfo[0].iVehicleRadioInterface;
   m_bSkipRateTests = false;
   if ( m_bSkipRateTests )
   {
      memcpy(&m_RadioInterfacesRuntimeCapabilitiesToApply, &g_pCurrentModel->radioInterfacesRuntimeCapab, sizeof(type_radio_interfaces_runtime_capabilities_parameters));

      for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
      {
         if ( ! m_bTestInterfaceIndex[iInt] )
            continue;

         for( int i=m_iIndexFirstDatarateLegacyTest[iInt]; i<=m_iIndexLastDatarateLegacyTest[iInt]; i++ )
         {
            int iIndex = i - m_iIndexFirstDatarateLegacyTest[iInt];
            if ( iIndex >= MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
               iIndex = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES-1;
            m_TestsInfo[i].fComputedQualityMin = ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[iInt][iIndex])/1000.0;
            m_TestsInfo[i].fComputedQualityMax = ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[iInt][iIndex])/1000.0;
         }
         for( int i=m_iIndexFirstDatarateMCSTest[iInt]; i<=m_iIndexLastDatarateMCSTest[iInt]; i++ )
         {
            int iIndex = i - m_iIndexFirstDatarateMCSTest[iInt];
            if ( iIndex >= MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
               iIndex = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES-1;
            m_TestsInfo[i].fComputedQualityMin = ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[iInt][iIndex])/1000.0;
            m_TestsInfo[i].fComputedQualityMax = ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[iInt][iIndex])/1000.0;
         }

         for( int i=m_iIndexFirstRadioFlagsTest[iInt]; i<m_iIndexLastRadioFlagsTest[iInt]; i++ )
         {
            m_TestsInfo[i].bSucceeded = true;
            m_TestsInfo[i].uTimeEnded = g_TimeNow;
            for( int j=0; j<hardware_get_radio_interfaces_count(); j++ )
               m_TestsInfo[i].fQualityCards[j] = m_TestsInfo[i].fComputedQualityMax;
         }
         m_iCountSucceededTests[iInt] = m_iIndexLastDatarateMCSTest[iInt]+1;
         m_iCurrentTestIndex = m_iIndexFirstRadioPowersTest[iInt];
      }
   }
   else
      g_pCurrentModel->resetRadioInterfacesRuntimeCapabilities(&m_RadioInterfacesRuntimeCapabilitiesToApply);

   log_line("[NegociateRadioLink] State reseted.");
   log_line("[NegociateRadioLink] Current vehicle radio config after state reset:");
   g_pCurrentModel->logVehicleRadioInfo();
}

void MenuNegociateRadio::_mark_test_as_skipped(int iTestIndex)
{
   if ( (iTestIndex < 0) || (iTestIndex >= m_iTestsCount) )
      return;

   m_TestsInfo[iTestIndex].bSkipTest = true;
   m_TestsInfo[iTestIndex].fComputedQualityMax = -1.0;
   m_TestsInfo[iTestIndex].fComputedQualityMin = -1.0;
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      m_TestsInfo[iTestIndex].iRadioInterfacesRXPackets[i] = 0;
   }
}

void MenuNegociateRadio::_getTestType(int iTestIndex, char* szType)
{
   if ( NULL == szType )
      return;
   strcpy(szType, "N/A");
   if ( iTestIndex < 0 )
      return;

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;

      if ( (iTestIndex >= m_iIndexFirstDatarateLegacyTest[iInt]) && (iTestIndex <= m_iIndexLastDatarateMCSTest[iInt]) )
      {
         strcpy(szType, "DataRate");
         break;
      }
      else if ( (iTestIndex >= m_iIndexFirstRadioFlagsTest[iInt]) && (iTestIndex <= m_iIndexLastRadioFlagsTest[iInt]) )
      {
         strcpy(szType, "RadioFlags");
         break;
      }
      else if ( (iTestIndex >= m_iIndexFirstRadioPowersTest[iInt]) && (iTestIndex <= m_iIndexLastRadioInterfaceTest[iInt]) )
      {
         strcpy(szType, "TxPowers");
         break;
      }
   }
}

char* MenuNegociateRadio::_getTestType(int iTestIndex)
{
   static char s_szNegociateRadioTestType[128];
   s_szNegociateRadioTestType[0] = 0;
   _getTestType(iTestIndex, s_szNegociateRadioTestType);
   return s_szNegociateRadioTestType;
}


void MenuNegociateRadio::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   y = yTop;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();
   y += height_text*0.5;
   g_pRenderEngine->setColors(get_Color_MenuText());

   float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenuLarge, m_szStatusMessage);
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenuLarge, m_szStatusMessage);
   y += height_text*1.4;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, L("(Video can flicker during the tests. That is normal.)"));
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, L("(Video can flicker during the tests. That is normal.)"));

   y += height_text*1.8;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenuLarge, m_szStatusMessage2);
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenuLarge, m_szStatusMessage2);
   char szBuff[128];
   szBuff[0] = 0;
   for(int i=0; i<(m_iLoopCounter%4); i++ )
      strcat(szBuff, ".");
   g_pRenderEngine->drawText(m_RenderXPos + fTextWidth + m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenuLarge, szBuff);

   y += height_text*1.4;

   if ( 0 != m_szStatusMessage3[0] )
   {
      fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, L(m_szStatusMessage3));
      g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, L(m_szStatusMessage3));
   }
   y += height_text*1.8;

   float hBar = height_text*1.5;
   float wBar = (m_RenderWidth - m_sfMenuPaddingX*2.0)/(float)m_iTestsCount;
   if ( wBar < 5.0*wPixel )
      wBar = 5.0*wPixel;
   float fDrawWidthBar = wBar-4.0*wPixel;
   if ( m_iTestsCount > 60 )
      fDrawWidthBar = wBar-2.0*wPixel;

   float x = m_RenderXPos+m_sfMenuPaddingX;

   int iIndexBestQuality = -1;
   float fBestQ = 0;

   for( int iTest=0; iTest<m_iTestsCount; iTest++ )
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(iInt) )
         continue;

      float fQualityCard = m_TestsInfo[iTest].fQualityCards[iInt];

      if ( fQualityCard > fBestQ )
      if ( ! m_TestsInfo[iTest].bSkipTest )
      if ( iTest < m_iCurrentTestIndex )
      if ( ! m_TestsInfo[iTest].bHasSubTests )
      {
         fBestQ = fQualityCard;
         iIndexBestQuality = iTest;
      }

      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + iTest*wBar, y + iInt*(hBar+hPixel*2.0), fDrawWidthBar, hBar-2.0*hPixel);

      float fRectH = 1.0;
      if ( fQualityCard > 0.98 )
         g_pRenderEngine->setFill(0,200,0,1.0);
      else if ( fQualityCard > 0.92 )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else if ( fQualityCard > 0.85 )
         g_pRenderEngine->setFill(200,200,0,1.0);
      else if ( fQualityCard > 0.11 )
         g_pRenderEngine->setFill(200,100,100,1.0);
      else if ( fQualityCard > 0.0001 )
      {
         fRectH = 0.3;
         g_pRenderEngine->setFill(200,0,0,1.0);
      }
      else
      {
         fRectH = 0.0;
         g_pRenderEngine->setFill(0,0,0,0);
      }

      if ( (iTest < m_iCurrentTestIndex) && m_TestsInfo[iTest].bHasSubTests )
      if ( m_TestsInfo[iTest].bSucceeded )
      {
         fRectH = 1.0;
         g_pRenderEngine->setFill(0,200,0,1.0);
      }

      if ( m_TestsInfo[iTest].bSkipTest && (iTest <= m_iCurrentTestIndex) )
      {
         fRectH = 1.0;
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setFill(150,150,150,0.4);
      }

      if ( fRectH > 0.001 )
         g_pRenderEngine->drawRect(x + iTest*wBar + wPixel, y + iInt*(hBar+hPixel*2.0) + hBar - fRectH*hBar - hPixel, fDrawWidthBar - 2.0*wPixel, fRectH*(hBar-2.0*hPixel));
   }

   if ( iIndexBestQuality >= 0 )
   {
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + iIndexBestQuality*wBar - 3.0*wPixel,
                y - hPixel*4.0, wBar+3.0*wPixel, (hBar+2.0*hPixel)*hardware_get_radio_interfaces_count() + 6.0*hPixel);
   }

   if ( m_iCurrentTestIndex >= 0 )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      sprintf(szBuff, "%d/%d %d/%d %.1f/%d", m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest, m_iState, m_iUserState, ((float)(g_TimeNow - m_TestsInfo[m_iCurrentTestIndex].uTimeStarted)/1000.0), m_TestsInfo[m_iCurrentTestIndex].iCountSendStarts);
      g_pRenderEngine->drawTextLeft(m_RenderXPos + m_RenderWidth - 0.8 * m_sfMenuPaddingX, m_RenderYPos + m_RenderTitleHeight + 1.2*m_sfMenuPaddingY, g_idFontMenuSmall, szBuff);
   }
   RenderEnd(yTop);
}

bool MenuNegociateRadio::periodicLoop()
{
   m_iLoopCounter++;
   if ( -1 == m_MenuIndexCancel )
   if ( g_TimeNow > m_uShowTime + 2000 )
   {
      m_MenuIndexCancel = addMenuItem(new MenuItem(L("Cancel"), L("Aborts the autoadjustment procedure without making any changes.")));
      float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
      addExtraHeightAtEnd(7.0*height_text + height_text * 1.5 * hardware_get_radio_interfaces_count() - m_pMenuItems[m_MenuIndexCancel]->getItemHeight(1.0));
      invalidate();
   }

   log_line("[NegociateRadioLink] Periodic loop: State: [%s], User state: [%s]",
      str_get_negociate_state(m_iState), str_get_negociate_user_state(m_iUserState));

   if ( 0 != m_iUserState )
   {
      log_line("[NegociateRadioLink] Periodic loop, waiting user input, user state: %d", m_iUserState);
      static u32 s_uLastAdaptivePauseFromNegociateRadio = 0;
      if ( (g_TimeNow > s_uLastAdaptivePauseFromNegociateRadio + 1000) ||
           (g_TimeNow > m_uLastTimeSentVehicleOperation + 1000) )
      {
         s_uLastAdaptivePauseFromNegociateRadio = g_TimeNow;
         send_pause_adaptive_to_router(4000);
         _send_keep_alive_to_vehicle();
      }

      if ( m_iState == NEGOCIATE_STATE_TEST_RUNNING )
      if ( m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle != 0 )
      {
         _storeCurrentTestDataFromRadioStats();
         _computeQualitiesSoFarForCurrentTest();
      }
   }

   if ( m_iState == NEGOCIATE_STATE_START_TEST )
   {
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Start test -> Starting test.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      _startTest(m_iCurrentTestIndex);
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION )
   {
      if ( g_TimeNow > m_TestsInfo[m_iCurrentTestIndex].uTimeStarted + m_TestsInfo[m_iCurrentTestIndex].uDurationToTest )
      {
         log_line("[NegociateRadioLink] Failed to get a confirmation from vehicle to start a test.");
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Wait vehicle test start -> End test. (no Ack from vehicle)", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         _endCurrentTest(true);
         return true;
      }

      if ( ! _currentTestUpdateWhenRunning() )
      {
         // Resend test to vehicle
         u32 uDelayResend = 60;
         if ( m_TestsInfo[m_iCurrentTestIndex].uExtraStartDelay > 0 )
            uDelayResend += m_TestsInfo[m_iCurrentTestIndex].uExtraStartDelay;

         if ( (g_TimeNow > m_TestsInfo[m_iCurrentTestIndex].uTimeLastSendToVehicle + uDelayResend) )
         {
            if ( m_TestsInfo[m_iCurrentTestIndex].uExtraStartDelay > 0 )
               m_TestsInfo[m_iCurrentTestIndex].uExtraStartDelay = 0;

            log_line("[NegociateRadioLink] Did not received start test confirmation from vehicle for test %d, send start again. Test started %u ms ago, test set duration is %u ms, subtest duration is %u ms, last sent start test message was %u ms ago, test is marked as ended? %s",
               m_iCurrentTestIndex,
               g_TimeNow - m_TestsInfo[m_iCurrentTestIndex].uTimeStarted, m_TestsInfo[m_iCurrentTestIndex].uDurationToTest, m_TestsInfo[m_iCurrentTestIndex].uDurationSubTest,
               g_TimeNow - m_TestsInfo[m_iCurrentTestIndex].uTimeLastSendToVehicle,
               (m_TestsInfo[m_iCurrentTestIndex].uTimeEnded != 0)?"yes":"no");
            _send_start_test_to_vehicle(m_iCurrentTestIndex);
         }
      }
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_TEST_RUNNING )
   {
      _currentTestUpdateWhenRunning();
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_TEST_ENDED )
   {
      if ( m_iUserState != NEGOCIATE_USER_STATE_NONE )
         return true;

      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Test ended -> Advance to next test.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      _advance_to_next_test();
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_SET_VIDEO_SETTINGS )
   {
      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 1000) &&
           (g_TimeNow < m_uTimeStartedVehicleOperation + 2500) )
      {
         m_uLastTimeSentVehicleOperation = g_TimeNow;
         handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&m_VideoParamsToApply, sizeof(video_parameters_t), (u8*)&m_VideoProfilesToApply, MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile));
      }
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION )
   {
      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 2000) &&
           (g_TimeNow < m_uTimeStartedVehicleOperation + 4000) )
      {
         m_uLastTimeSentVehicleOperation = g_TimeNow;
         handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&m_VideoParamsToApply, sizeof(video_parameters_t), (u8*)&m_VideoProfilesToApply, MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile));
      }
      else if (g_TimeNow > m_uTimeStartedVehicleOperation + 4000)
      {
         log_line("[NegociateRadioLink] Failed to get a confirmation from vehicle to set video settings.");
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Wait vehicle set video params ack -> Failed flow.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         addMessage2(0, L("Failed to negociate radio links."), L("You radio links quality is very poor. Please fix the physical radio links quality and try again later."));
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_VIDEO_CONFIRMATION;
         m_iState = NEGOCIATE_STATE_END_TESTS;
         m_bFailed = true;
      }
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_SET_RADIO_SETTINGS )
   {
      m_uTimeStartedVehicleOperation = g_TimeNow;
      m_uLastTimeSentVehicleOperation = 0;
      m_iState = NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_RADIO_SETTINGS_CONFIRMATION;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Set Radio params -> Wait vehicle set radio params ack.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      _send_apply_settings_to_vehicle();
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_RADIO_SETTINGS_CONFIRMATION )
   {
      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 200) &&
           (g_TimeNow < m_uTimeStartedVehicleOperation + 2000) )
         _send_apply_settings_to_vehicle();

      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 200) &&
           (g_TimeNow > m_uTimeStartedVehicleOperation + 2500) )
      {
         log_line("[NegociateRadioLink] Failed to get a confirmation from vehicle to set radio settings.");
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Wait vehicle set radio params ack -> Failed flow.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         addMessage2(0, L("Failed to negociate radio links."), L("You radio links quality is very poor. Please fix the physical radio links quality and try again later."));
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_RADIO_CONFIRMATION;
         m_iState = NEGOCIATE_STATE_END_TESTS;
         m_bFailed = true;
         return true;
      }

      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_END_TESTS )
   {
      if ( m_iUserState != 0 )
      {
         log_line("[NegociateRadioLink] Waiting for user finish confirmation on state end tests...");
         return true;
      }
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) End tests -> Send end tests to vehicle.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

      if ( (! m_bCanceled) && (!m_bFailed) )
         _save_new_settings_to_model();

      m_uTimeStartedVehicleOperation = g_TimeNow;
      m_uLastTimeSentVehicleOperation = 0;
      _send_end_all_tests_to_vehicle(m_bCanceled | m_bFailed);
      return true;
   }

   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION )
   {
      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 60) &&
           (g_TimeNow > m_uTimeStartedVehicleOperation + 500) )
      {
         log_line("[NegociateRadioLink] Failed to get a confirmation from vehicle to end tests.");
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Wait vehicle end tests -> Finish flow.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         _onFinishedFlow();
         return true;
      }
      if ( (g_TimeNow > m_uLastTimeSentVehicleOperation + 60) &&
           (g_TimeNow < m_uTimeStartedVehicleOperation + 400) )
         _send_end_all_tests_to_vehicle(m_bCanceled | m_bFailed);
      return true;
   }

   return false;
}

int MenuNegociateRadio::onBack()
{
   log_line("[NegociateRadioLink] OnBack: Now there are %d failing tests of max %d", m_iCountFailedTests, MAX_FAILING_RADIO_NEGOCIATE_STEPS);
   log_line("[NegociateRadioLink] OnBack: State: [%s], User state: [%s]",
      str_get_negociate_state(m_iState), str_get_negociate_user_state(m_iUserState));

   if ( (-1 != m_MenuIndexCancel) && (m_iUserState == NEGOCIATE_USER_STATE_NONE) )
      _onCancel();

   return 0;
}

void MenuNegociateRadio::onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded)
{
   Menu::onVehicleCommandFinished(uCommandId, uCommandType, bSucceeded);

   log_line("[NegociateRadioLink] On Vehicle Command Response: State: [%s], User state: [%s]",
      str_get_negociate_state(m_iState), str_get_negociate_user_state(m_iUserState));

   if ( uCommandType == COMMAND_ID_SET_VIDEO_PARAMETERS )
   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION )
   {
      log_line("[NegociateRadioLink] Received confirmation from vehicle for set video params.");

      bool bAllHaveMCSRates = true;
      bool bAnyHaveMCSRates = false;
      int iInterfaceNoMCS = -1;
      for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
      {
         if ( ! m_bTestInterfaceIndex[iInt] )
            continue;
         if ( ! (m_uRadioLinksTxFlagsToApply[iInt] & RADIO_FLAGS_USE_MCS_DATARATES) )
         {
            bAllHaveMCSRates = false;
            iInterfaceNoMCS = iInt;
         }
         bAnyHaveMCSRates = true;
      }

      if ( ! bAllHaveMCSRates )
      {
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION;
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> MCS warning wait user", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

         if ( ! bAnyHaveMCSRates )
            addMessage2(0, L("Radio Link Quality Warning"), L("Your vehicle radio links should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."));
         else
         {
            char szBuff[256];
            sprintf(szBuff, L("Your vehicle radio interface %d should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."), iInterfaceNoMCS+1);
            addMessage2(0, L("Radio Link Quality Warning"), szBuff);
         }
         return;
      }

      m_iState = NEGOCIATE_STATE_SET_RADIO_SETTINGS;
      m_uTimeStartedVehicleOperation = 0;
      m_uLastTimeSentVehicleOperation = 0;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (C) -> Set radio settings.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }
}

void MenuNegociateRadio::_storeCurrentTestDataFromRadioStats()
{
   if ( m_iCurrentTestIndex < 0 )
      return;
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioLink )
         continue;
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPackets;
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad + g_SM_RadioStats.radio_interfaces[i].totalRxPacketsLost;
      log_line("[NegociateRadioLink] Store test data from radio stats interface %d (now on test %d): rx/(lost/bad) pckts: %d/%d", i+1, m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i], m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i]);
   }
}

void MenuNegociateRadio::_logTestData(int iTestIndex)
{
   char szRx[256];
   char szLost[256];

   strcpy(szRx, "[");
   strcpy(szLost, "[");
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
   {
      if ( i != 0 )
      {
         strcat(szRx, ", ");
         strcat(szLost, ", ");
      }
      char szTmp[32];
      sprintf(szTmp, "%d", m_TestsInfo[iTestIndex].iRadioInterfacesRXPackets[i]);
      strcat(szRx, szTmp);
      sprintf(szTmp, "%d", m_TestsInfo[iTestIndex].iRadioInterfacesRxLostPackets[i]);
      strcat(szLost, szTmp);

      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != m_TestsInfo[iTestIndex].iVehicleRadioLink )
         continue;
   }
   strcat(szRx, "]");
   strcat(szLost, "]");

   char szTestType[64];
   _getTestType(iTestIndex, szTestType);

   char szState[128];
   strcpy(szState, "not started");
   if ( m_TestsInfo[iTestIndex].uTimeStarted != 0 )
      sprintf(szState, "started %u ms ago", g_TimeNow - m_TestsInfo[iTestIndex].uTimeStarted);
   if ( m_TestsInfo[iTestIndex].uTimeEnded != 0 )
      sprintf(szState, "ended %u ms ago, in %u ms", g_TimeNow - m_TestsInfo[iTestIndex].uTimeEnded, m_TestsInfo[iTestIndex].uTimeEnded - m_TestsInfo[iTestIndex].uTimeStarted);

   log_line("[NegociateRadioLink] Test %d (type: %s) (datarate %s) %s, succeded? %s, computed quality? %s, enough rx data? %s, rx/lost packets: %s/%s",
      iTestIndex, szTestType, str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
      szState,
      m_TestsInfo[iTestIndex].bSucceeded?"yes":"no",
      m_TestsInfo[iTestIndex].bComputedQualities?"yes":"no",
      m_TestsInfo[iTestIndex].bReceivedEnoughData?"yes":"no",
      szRx, szLost);
}

float MenuNegociateRadio::_getMaxComputedQualityForDatarate(int iVehicleRadioInterface, int iDatarate, int* pTestIndex)
{
   if ( ! m_bTestInterfaceIndex[iVehicleRadioInterface] )
      return 0.0;

   float fQuality = 0.0;
   for( int iTest=m_iIndexFirstDatarateLegacyTest[iVehicleRadioInterface]; iTest<=m_iIndexLastDatarateMCSTest[iVehicleRadioInterface]; iTest++ )
   {
      if ( iTest > m_iCurrentTestIndex )
         break;
      if ( m_TestsInfo[iTest].iDataRateToTest != iDatarate )
         continue;

      if ( m_TestsInfo[iTest].fComputedQualityMax > fQuality )
      {
         fQuality = m_TestsInfo[iTest].fComputedQualityMax;
         if ( NULL != pTestIndex )
            *pTestIndex = iTest;
      }
   }
   return fQuality;
}


float MenuNegociateRadio::_getMinComputedQualityForDatarate(int iVehicleRadioInterface, int iDatarate, int* pTestIndex)
{
   if ( ! m_bTestInterfaceIndex[iVehicleRadioInterface] )
      return 0.0;

   float fQuality = 1.0;
   for( int iTest=m_iIndexFirstDatarateLegacyTest[iVehicleRadioInterface]; iTest<=m_iIndexLastDatarateMCSTest[iVehicleRadioInterface]; iTest++ )
   {
      if ( iTest > m_iCurrentTestIndex )
         break;
      if ( m_TestsInfo[iTest].iDataRateToTest != iDatarate )
         continue;

      if ( (m_TestsInfo[iTest].fComputedQualityMin > 0.0) && (m_TestsInfo[iTest].fComputedQualityMin < fQuality) )
      {
         fQuality = m_TestsInfo[iTest].fComputedQualityMin;
         if ( NULL != pTestIndex )
            *pTestIndex = iTest;
      }
   }
   return fQuality;
}

void MenuNegociateRadio::_send_keep_alive_to_vehicle()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8* pBuffer = &(buffer[sizeof(t_packet_header)]);
   *pBuffer = (u8)m_iCurrentTestIndex;
   pBuffer++;
   *pBuffer = NEGOCIATE_RADIO_KEEP_ALIVE;
   pBuffer++;

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
   m_uLastTimeSentVehicleOperation = g_TimeNow;
   log_line("[NegociateRadioLink] Sent keep alive message to vehicle.");
}

void MenuNegociateRadio::_send_start_test_to_vehicle(int iTestIndex)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) + 2*sizeof(int);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8* pBuffer = &(buffer[sizeof(t_packet_header)]);
   *pBuffer = (u8)m_iCurrentTestIndex;
   pBuffer++;
   *pBuffer = NEGOCIATE_RADIO_TEST_PARAMS;
   pBuffer++;
   *pBuffer = (u8) m_TestsInfo[iTestIndex].iVehicleRadioInterface;
   pBuffer++;
   memcpy(pBuffer, &(m_TestsInfo[iTestIndex].iDataRateToTest), sizeof(int));
   pBuffer += sizeof(int);
   memcpy(pBuffer, &(m_TestsInfo[iTestIndex].uRadioFlagsToTest), sizeof(u32));
   pBuffer += sizeof(u32);
   memcpy(pBuffer, &(m_TestsInfo[iTestIndex].iTxPowerMwToTest), sizeof(int));
   pBuffer += sizeof(int);

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
   u32 uPrevTime = m_TestsInfo[iTestIndex].uTimeLastSendToVehicle;
   m_TestsInfo[iTestIndex].uTimeLastSendToVehicle = g_TimeNow;
   m_TestsInfo[iTestIndex].iCountSendStarts++;
   log_line("[NegociateRadioLink] Sent start test message to vehicle (test number: %d, dr: %s, radio flags: %s, tx power: %d mW)",
      iTestIndex, str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
      str_get_radio_frame_flags_description2(m_TestsInfo[iTestIndex].uRadioFlagsToTest),
      m_TestsInfo[iTestIndex].iTxPowerMwToTest );
   log_line("[NegociateRadioLink] Sent start test message for test number %d: %d times, test started %u ms ago, previous sent start time was %u ms ago, last confirmation received %u ms ago, test max duration: %u ms, subtest max duration: %u ms",
      iTestIndex, m_TestsInfo[iTestIndex].iCountSendStarts,
      g_TimeNow - m_TestsInfo[iTestIndex].uTimeStarted,
      g_TimeNow - uPrevTime,
      g_TimeNow - m_TestsInfo[iTestIndex].uTimeLastConfirmationFromVehicle, m_TestsInfo[iTestIndex].uDurationToTest, m_TestsInfo[iTestIndex].uDurationSubTest);
}

void MenuNegociateRadio::_send_end_all_tests_to_vehicle(bool bCanceled)
{
   send_pause_adaptive_to_router(4000);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8* pBuffer = &(buffer[sizeof(t_packet_header)]);
   *pBuffer = (u8)m_iCurrentTestIndex;
   if ( m_iCurrentTestIndex < 0 )
      *pBuffer = 0xFF;
   pBuffer++;
   *pBuffer = NEGOCIATE_RADIO_END_TESTS;
   pBuffer++;
   *pBuffer = bCanceled?1:0;
   pBuffer++;

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);

   if ( m_iState != NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION )
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Send end tests to vehicle -> Wait vehicle end confirmation.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
   m_iState = NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION;
   m_uLastTimeSentVehicleOperation = g_TimeNow;

   log_line("[NegociateRadioLink] Sent end all tests message to vehicle.");
}

void MenuNegociateRadio::_send_revert_flags_to_vehicle()
{
   log_line("[NegociateRadioLink] Send revert flags to vehicle");
   g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      g_pCurrentModel->radioInterfacesRuntimeCapab.uInterfaceFlags[i] &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags &= ~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;

   g_pCurrentModel->validateRadioSettings();
   saveCurrentModel();
   send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
   g_bMustNegociateRadioLinksFlag = true;
}


void MenuNegociateRadio::_send_apply_settings_to_vehicle()
{
   send_pause_adaptive_to_router(6000);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + 3*MAX_RADIO_INTERFACES*sizeof(u32) + sizeof(type_radio_interfaces_runtime_capabilities_parameters);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8* pBuffer = &(buffer[sizeof(t_packet_header)]);
   *pBuffer = 0xFF;
   pBuffer++;
   *pBuffer = NEGOCIATE_RADIO_APPLY_PARAMS;
   pBuffer++;
   memcpy(pBuffer, &m_uRadioInterfacesSupportedRadioFlags, MAX_RADIO_INTERFACES*sizeof(u32));
   pBuffer += MAX_RADIO_INTERFACES*sizeof(u32);
   memcpy(pBuffer, &m_uRadioLinksTxFlagsToApply, MAX_RADIO_INTERFACES*sizeof(u32));
   pBuffer += MAX_RADIO_INTERFACES*sizeof(u32);
   memcpy(pBuffer, &m_uRadioLinksRxFlagsToApply, MAX_RADIO_INTERFACES*sizeof(u32));
   pBuffer += MAX_RADIO_INTERFACES*sizeof(u32);

   memcpy(pBuffer, &m_RadioInterfacesRuntimeCapabilitiesToApply, sizeof(type_radio_interfaces_runtime_capabilities_parameters));

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);

   m_uLastTimeSentVehicleOperation = g_TimeNow;
   log_line("[NegociateRadioLink] Sent [apply settings] message to vehicle. %d bytes:", PH.total_length);

   char szBuff[256];
   char szTmp[64];

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      log_line("[NegociateRadioLink] Sent radio link %d radio tx flags: %s", iLink+1, str_get_radio_frame_flags_description2(m_uRadioLinksTxFlagsToApply[iLink]));
      log_line("[NegociateRadioLink] Sent radio link %d radio rx flags: %s", iLink+1, str_get_radio_frame_flags_description2(m_uRadioLinksRxFlagsToApply[iLink]));
   }

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;

      log_line("[NegociateRadioLink] Sent radio interface %d radio capability flags: %s", iInt+1, str_get_radio_frame_flags_description2(m_uRadioInterfacesSupportedRadioFlags[iInt]));
      log_line("[NegociateRadioLink] Sent radio interface %d max supported rates legacy/MCS: %d/%d", iInt+1, m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate[iInt], m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iInt]);
      szBuff[0] = 0;
      for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
      {
         sprintf(szTmp, "%d mW", m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i]);
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Sent radio interface %d legacy tx powers: %s", iInt+1, szBuff);
      szBuff[0] = 0;
      for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
      {
         sprintf(szTmp, "%d mW", m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i]);
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Sent radio interface %d MCS tx powers: %s", iInt+1, szBuff);

      szBuff[0] = 0;
      for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
      {
         sprintf(szTmp, "%.2f", ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[iInt][i])/1000.0);
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Sent radio interface %d legacy qualities: %s", iInt+1, szBuff);

      szBuff[0] = 0;
      for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
      {
         sprintf(szTmp, "%.2f", ((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[iInt][i])/1000.0);
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Sent radio interface %d MCS qualities: %s", iInt+1, szBuff);
   }
}

void MenuNegociateRadio::_startTest(int iTestIndex)
{
   char szTestType[64];
   _getTestType(m_iCurrentTestIndex, szTestType);

   int iInterfaceIndex = m_TestsInfo[iTestIndex].iVehicleRadioInterface;

   if ( (iTestIndex >= m_iIndexFirstRadioFlagsTest[iInterfaceIndex]) && (iTestIndex <= m_iIndexLastRadioFlagsTest[iInterfaceIndex]) )
      sprintf(m_szStatusMessage3, "(Testing radio flags)");
   else if ( (iTestIndex >= m_iIndexFirstDatarateLegacyTest[iInterfaceIndex]) && (iTestIndex <= m_iIndexLastDatarateLegacyTest[iInterfaceIndex]) )
      sprintf(m_szStatusMessage3, "(Testing quality of %s modulation)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstDatarateMCSTest[iInterfaceIndex]) && (iTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex]) )
      sprintf(m_szStatusMessage3, "(Testing quality of %s datarate)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstRadioPowersTest[iInterfaceIndex]) && (iTestIndex < m_iIndexFirstRadioPowersTestMCS[iInterfaceIndex]) )
      sprintf(m_szStatusMessage3, "(Testing actual usable radio Tx power for %s datarate)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstRadioPowersTestMCS[iInterfaceIndex]) && (iTestIndex <= m_iIndexLastRadioPowersTestMCS[iInterfaceIndex]) )
      sprintf(m_szStatusMessage3, "(Testing actual usable radio Tx power for %s modulation)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else
      strcpy(m_szStatusMessage3, "");

   if ( m_iCountInterfacesToTest > 1 )
   if ( m_szStatusMessage3[0] != 0 )
   {
      bool bEndAc = false;
      if ( m_szStatusMessage3[strlen(m_szStatusMessage3)-1] == ')' )
      {
         m_szStatusMessage3[strlen(m_szStatusMessage3)-1] = 0;
         bEndAc = true;
      }
      char szTmp[128];
      sprintf(szTmp, " for radio link %d", m_TestsInfo[iTestIndex].iVehicleRadioLink+1);
      strcat(m_szStatusMessage3, szTmp);
      if ( bEndAc )
         strcat(m_szStatusMessage3, ")");
   }

   if ( m_TestsInfo[iTestIndex].bHasSubTests )
      log_line("[NegociateRadioLink] Starting test %d, substep %d (type: %s) (current test is %d), for radio interface %d, radio link %d, tx power: %d, datarate: %s, radio flags: %s",
          iTestIndex, m_TestsInfo[iTestIndex].iCurrentSubTest, szTestType, m_iCurrentTestIndex,
          m_TestsInfo[iTestIndex].iVehicleRadioInterface+1,
          m_TestsInfo[iTestIndex].iVehicleRadioLink+1,
          m_TestsInfo[iTestIndex].iTxPowerMwToTest,
          str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
          str_get_radio_frame_flags_description2(m_TestsInfo[iTestIndex].uRadioFlagsToTest) );
   else
      log_line("[NegociateRadioLink] Starting test %d (type: %s) (current test is %d), for radio interface %d, radio link %d, tx power: %d, datarate: %s, radio flags: %s",
          iTestIndex, szTestType, m_iCurrentTestIndex,
          m_TestsInfo[iTestIndex].iVehicleRadioInterface+1,
          m_TestsInfo[iTestIndex].iVehicleRadioLink+1,
          m_TestsInfo[iTestIndex].iTxPowerMwToTest,
          str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
          str_get_radio_frame_flags_description2(m_TestsInfo[iTestIndex].uRadioFlagsToTest) );

   log_line("[NegociateRadioLink] Set status 3 message to: [%s]", m_szStatusMessage3);

   m_iCurrentTestIndex = iTestIndex;
   m_TestsInfo[m_iCurrentTestIndex].uTimeStarted = g_TimeNow;

   m_iState = NEGOCIATE_STATE_TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION;
   send_pause_adaptive_to_router(6000);
   _send_start_test_to_vehicle(m_iCurrentTestIndex);
}

void MenuNegociateRadio::_endCurrentTest(bool bUpdateTestState)
{
   if ( bUpdateTestState && (0 != m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle) )
   {
      _storeCurrentTestDataFromRadioStats();
      _computeQualitiesSoFarForCurrentTest();
   }

   m_TestsInfo[m_iCurrentTestIndex].uTimeEnded = g_TimeNow;
   m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData = false;

   int iInterfaceIndex = m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( !hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioLink )
         continue;

      if ( m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] > 40 )
         m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData = true;
   }

   if ( (0 == m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle) || (! m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData) )
   {
      if ( 0 == m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle )
         log_line("[NegociateRadioLink] Test %d has not received confirmation from vehicle.", m_iCurrentTestIndex);
      if ( ! m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData )
         log_line("[NegociateRadioLink] Test %d has not received enough rx data.", m_iCurrentTestIndex);
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = false;
   }
   else
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = true;

   if ( bUpdateTestState && (m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin < TEST_DATARATE_QUALITY_THRESHOLD) )
   {
      log_line("[NegociateRadioLink] Test %d has poor quality (min %.2f%% < threshold %.2ff)", m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin, TEST_DATARATE_QUALITY_THRESHOLD );
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = false;
   }

   if ( m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
      m_iCountSucceededTests[iInterfaceIndex]++;
   else
   {
      if ( (m_iCurrentTestIndex < m_iIndexFirstRadioFlagsTest[iInterfaceIndex]) || (m_iCurrentTestIndex >m_iIndexLastRadioFlagsTest[iInterfaceIndex]) )
         m_iCountFailedTests[iInterfaceIndex]++;
      if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest[iInterfaceIndex]) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex]) )
      {
         if ( (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest < 0) && (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest >= -3) )
            m_iCountFailedTestsDatarates[iInterfaceIndex]++;
         if ( (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest > 0) && (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest <= getTestDataRatesLegacy()[2]) )
            m_iCountFailedTestsDatarates[iInterfaceIndex]++;
      }
   }

   _logTestData(m_iCurrentTestIndex);

   m_iState = NEGOCIATE_STATE_TEST_ENDED;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (E)* -> Test ended.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);


   // Failed too many tests? Cancel remaining tests for this radio interface
   if ( m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex] )
   if ( (m_iCountFailedTestsDatarates[iInterfaceIndex] >= MAX_FAILING_RADIO_NEGOCIATE_STEPS) && (m_iCountSucceededTests[iInterfaceIndex] < MAX_FAILING_RADIO_NEGOCIATE_STEPS/2) )
   {
      log_softerror_and_alarm("[NegociateRadioLink] Failed %d tests. Skip this vehicle radio interface %d", MAX_FAILING_RADIO_NEGOCIATE_STEPS, iInterfaceIndex);
      for( int i=m_iCurrentTestIndex+1; i<=m_iIndexLastRadioInterfaceTest[iInterfaceIndex]; i++ )
      {
         _mark_test_as_skipped(i);
      }
      return;
   }

   // Increase the next test duration for same datarate if this one failed
   if ( ! m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
   if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest[iInterfaceIndex]) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex]) )
   if ( (m_iCurrentTestIndex % 2) == 0 )
   {
      m_TestsInfo[m_iCurrentTestIndex+1].uDurationToTest *= 2;
      log_line("[NegociateRadioLink] Increased next test duration for same datarate as this one failed.");
   }

   // Mark to skip remaining datarate tests if this one failed and it's a datarate test
   if ( ! m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
   if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest[iInterfaceIndex]) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex]) )
   if ( (m_iCurrentTestIndex % 2) == 1 )
   {
      log_line("[NegociateRadioLink] Datarate %s test %d failed. Skip remaining datarates tests.", str_format_datarate_inline(m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest), m_iCurrentTestIndex);
      if ( m_iCurrentTestIndex <= m_iIndexLastDatarateLegacyTest[iInterfaceIndex] )
      {
         for( int i=m_iCurrentTestIndex+1; i<=m_iIndexLastDatarateLegacyTest[iInterfaceIndex]; i++ )
            _mark_test_as_skipped(i);
      }
      else if ( m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iInterfaceIndex] )
      {
         for( int i=m_iCurrentTestIndex+1; i<=m_iIndexLastDatarateMCSTest[iInterfaceIndex]; i++ )
            _mark_test_as_skipped(i);
      }
   }
}

// returns true if test state was updated
bool MenuNegociateRadio::_currentTestUpdateWhenRunning()
{
   if ( m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle != 0 )
   {
      _storeCurrentTestDataFromRadioStats();
      _computeQualitiesSoFarForCurrentTest();
   }

   if ( ! m_TestsInfo[m_iCurrentTestIndex].bHasSubTests )
   {
      if ( g_TimeNow > m_TestsInfo[m_iCurrentTestIndex].uTimeStarted + m_TestsInfo[m_iCurrentTestIndex].uDurationToTest/2 )
      if ( m_TestsInfo[m_iCurrentTestIndex].uDurationToTest <= SINGLE_TEST_DURATION )
      if ( m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax < 0.5 )
      {
          m_TestsInfo[m_iCurrentTestIndex].uDurationToTest += SINGLE_TEST_DURATION;
          log_line("[NegociateRadioLink] Increase duration of current test %d to %u ms as the quality is too small: %.2f", m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].uDurationToTest, m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax);
      }
      if ( g_TimeNow > m_TestsInfo[m_iCurrentTestIndex].uTimeStarted + m_TestsInfo[m_iCurrentTestIndex].uDurationToTest )
      {
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Test running -> End test (test duration elapsed).", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         _endCurrentTest(true);
         return true;
      }
      return false;
   }

   // Multistep test?

   if ( m_iCurrentTestIndex < m_iIndexFirstRadioPowersTest[m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface] )
      return false;

   return _updateCurrentMultiTest();
}

bool MenuNegociateRadio::_updateCurrentMultiTest()
{
   if ( g_TimeNow <= m_TestsInfo[m_iCurrentTestIndex].uTimeStarted + m_TestsInfo[m_iCurrentTestIndex].uDurationSubTest )
      return false;

   log_line("[NegociateRadioLink] Test %d substep %d finished. Compute result...", m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest);
   if ( m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle != 0 )
   {
      _storeCurrentTestDataFromRadioStats();
      _computeQualitiesSoFarForCurrentTest();
   }
   bool bSubTestSucceeded = false;
   if ( (m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle != 0) && (m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin >= TEST_DATARATE_QUALITY_THRESHOLD) )
   {
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood = m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest;
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = true;
      bSubTestSucceeded = true;
      log_line("[NegociateRadioLink] Power test %d/%d substep %d: measured new power is good quality. Set last good tx power to: %d mW", m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest, m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood);
   }
   else
      log_line("[NegociateRadioLink] Power test %d/%d substep %d: measured new power failed quality test.", m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest);

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;
   int iRadioInterfacePowerMaxMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);

   log_line("[NegociateRadioLink] Power test %d/%d substep %d: finished substep: min/max/mid tx power: %d / %d / %d mW. Succeeded substep? %s, Succeeded globally? %s, last good tx power for test: %d mW, max radio link power: %d mW",
      m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest,
      bSubTestSucceeded?"yes":"no",
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded?"yes":"no",
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood,
      iRadioInterfacePowerMaxMw );

   bool bFinishTest = false;
   if ( (m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest - m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin) < ((float)iRadioInterfacePowerMaxMw)*0.08 )
      bFinishTest = true;
   if ( (m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax - m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest) < ((float)iRadioInterfacePowerMaxMw)*0.05 )
      bFinishTest = true;

   if ( bFinishTest )
   {
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = false;
      if ( m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood > 0 )
         m_TestsInfo[m_iCurrentTestIndex].bSucceeded = true;
      m_TestsInfo[m_iCurrentTestIndex].uDurationToTest = 1;

      log_line("[NegociateRadioLink] Power test %d/%d substep %d: end now, succeeded substep? %s, succeeded: %s, good tx power: %d mw. End test & proceed to next test",
         m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest,
         bSubTestSucceeded?"yes":"no",
         m_TestsInfo[m_iCurrentTestIndex].bSucceeded?"yes":"no", m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood);

      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (P) Multistep Test running -> End test.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      _endCurrentTest(false);
      return true;
   }

   // Keep test running, update to new parameters
   log_line("[NegociateRadioLink] Power test %d/%d substep %d: adjust testing power range.", m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest);
   if ( bSubTestSucceeded )
   {
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin = m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest;
      if ( (fabs(m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin - m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax) < ((float)iRadioInterfacePowerMaxMw)*0.08) ||
           (m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax - m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest) <= 5 )
         m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin = m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax;
   }
   else
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax = m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest;

   m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest = (m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax + m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin) / 2;
   if ( m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
   if ( m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest < m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax )
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest++;

   log_line("[NegociateRadioLink] Power test %d/%d subset %d: start new subtest: min/max/mid tx power: %d / %d / %d mW. Last good tx power for test: %d mW, max radio link power: %d mW",
      m_iCurrentTestIndex, m_iTestsCount-1,
      m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMin,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTestMax,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerMwToTest,
      m_TestsInfo[m_iCurrentTestIndex].iTxPowerLastGood,
      iRadioInterfacePowerMaxMw);

   m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle = 0;
   m_TestsInfo[m_iCurrentTestIndex].uTimeLastSendToVehicle = 0;
   m_TestsInfo[m_iCurrentTestIndex].uTimeEnded = 0;
   m_TestsInfo[m_iCurrentTestIndex].iCountSendStarts = 0;
   m_TestsInfo[m_iCurrentTestIndex].uDurationSubTest = SINGLE_TEST_DURATION_POWER*1.5;
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] = 0;
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i] = 0;
      m_TestsInfo[m_iCurrentTestIndex].fQualityCards[i] = 0.0;
   }
   m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin = -1.0;
   m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax = -1.0;

   m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest++;
   m_iState = NEGOCIATE_STATE_START_TEST;
   log_line("[NegociateRadioLink] Multistep test %d/%d increase substep to: %d", m_iCurrentTestIndex, m_iTestsCount-1, m_TestsInfo[m_iCurrentTestIndex].iCurrentSubTest);
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (Multi step test running) -> Start test.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

   return false;
}

void MenuNegociateRadio::_advance_to_next_test()
{
   char szTmp[128];
   strcpy(szTmp, _getTestType(m_iCurrentTestIndex+1));
   log_line("[NegociateRadioLink] Advancing to next test (current test: %d (%s) -> next test: %d (%s))...", m_iCurrentTestIndex, _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex+1, szTmp);

   // Failed too many tests on each radio interface? Cancel the negociate radio flow
   bool bAllFailed = true;
   int iLastRadioInterfaceToTest = -1;
   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++)
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;
      iLastRadioInterfaceToTest = iInt;
      if ( (m_iCountFailedTestsDatarates[iInt] >= MAX_FAILING_RADIO_NEGOCIATE_STEPS) && (m_iCountSucceededTests[iInt] < MAX_FAILING_RADIO_NEGOCIATE_STEPS/2) )
         continue;
      bAllFailed = false;
   }

   if ( bAllFailed )
   if ( m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface == iLastRadioInterfaceToTest )
   {

      log_softerror_and_alarm("[NegociateRadioLink] Failed %d tests on each radio interface. Aborting operation.", MAX_FAILING_RADIO_NEGOCIATE_STEPS);
      log_line("[NegociateRadioLink] Cancel negociate radio with %d failing steps.", MAX_FAILING_RADIO_NEGOCIATE_STEPS);
      addMessage2(0, L("Failed to negociate radio links."), L("You radio links quality is very poor. Please fix the physical radio links quality and try again later."));
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION;
      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_bFailed = true;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (E)* -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   m_iCurrentTestIndex++;

   // Finished all tests? Apply changes

   if ( m_iCurrentTestIndex >= m_iTestsCount )
   {
      _onFinishedTests();
      return;
   }

   int iRadioInterfaceIndex = m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface;

   // Switched to legacy datarates tests? Update radio flags to use and update status message
   if ( m_iCurrentTestIndex == m_iIndexFirstDatarateLegacyTest[iRadioInterfaceIndex] )
   {
      _compute_radio_flags_to_apply(iRadioInterfaceIndex);
      strcpy(m_szStatusMessage2, L("Computing radio links capabilities"));
      log_line("[NegociateRadioLink] Switching to legacy datarates tests.");
   }

   // Switched to MCS radio datarates tests?
   // Update radio flags tests based on radio flags tests results
   if ( m_iCurrentTestIndex == m_iIndexFirstDatarateMCSTest[iRadioInterfaceIndex] )
   {
      strcpy(m_szStatusMessage2, L("Computing radio links capabilities"));
      log_line("[NegociateRadioLink] Switching to MCS datarates tests.");

      log_line("[NegociateRadioLink] Updated MCS datarates tests radio flags to: %s", str_get_radio_frame_flags_description2(m_uRadioLinksTxFlagsToApply[iRadioInterfaceIndex]));

      for( int i=m_iIndexFirstDatarateMCSTest[iRadioInterfaceIndex]; i<=m_iIndexLastDatarateMCSTest[iRadioInterfaceIndex]; i++ )
      {
         m_TestsInfo[i].uRadioFlagsToTest = m_uRadioLinksTxFlagsToApply[iRadioInterfaceIndex];
         m_TestsInfo[i].uRadioFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
         m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      }
      for( int i=m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex]; i<=m_iIndexLastRadioPowersTestMCS[iRadioInterfaceIndex]; i++ )
      {
         m_TestsInfo[i].uRadioFlagsToTest = m_uRadioLinksTxFlagsToApply[iRadioInterfaceIndex];
         m_TestsInfo[i].uRadioFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
         m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      }
   }

   // Switched to tx powers tests?
   // Update which tx power tests to run
   if ( m_iCurrentTestIndex == m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex] )
   {
      strcpy(m_szStatusMessage2, L("Testing radio powers"));
      log_line("[NegociateRadioLink] Switching to tx powers tests: %d legacy power tests, %d MCS power tests.",
         m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] - m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex],
         m_iIndexLastRadioPowersTestMCS[iRadioInterfaceIndex] - m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] + 1);
      for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] - m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex]); i++ )
      {
         if ( m_TestsInfo[m_iIndexFirstDatarateLegacyTest[iRadioInterfaceIndex] + i].bSkipTest || ( (((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[iRadioInterfaceIndex][i])/1000.0) < TEST_DATARATE_QUALITY_THRESHOLD) )
         {
            m_TestsInfo[m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex] + i].bSkipTest = true;
            log_line("[NegociateRadioLink] Mark power test %d (datarate %s) to be skipped due to low legacy datarate quality.", m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex] + i, str_format_datarate_inline(m_TestsInfo[m_iIndexFirstRadioPowersTest[iRadioInterfaceIndex] + i].iDataRateToTest));
         }
      }
      for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS[iRadioInterfaceIndex] - m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex]); i++ )
      {
         if ( m_TestsInfo[m_iIndexFirstDatarateMCSTest[iRadioInterfaceIndex] + i].bSkipTest || ( (((float)m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[iRadioInterfaceIndex][i])/1000.0) < TEST_DATARATE_QUALITY_THRESHOLD) )
         {
            m_TestsInfo[m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] + i].bSkipTest = true;
            log_line("[NegociateRadioLink] Mark power test %d (datarate %s) to be skipped due to low MCS datarate quality.", m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] + i, str_format_datarate_inline(m_TestsInfo[m_iIndexFirstRadioPowersTestMCS[iRadioInterfaceIndex] + i].iDataRateToTest));
         }
      }
   }

   // Skip test?
   if ( m_TestsInfo[m_iCurrentTestIndex].bSkipTest )
   {
      log_line("[NegociateRadioLink] Test %d (type: %s, datarate: %s, radio flags: %s) is marked as to be skipped. Skip it.",
         m_iCurrentTestIndex, _getTestType(m_iCurrentTestIndex),
         str_format_datarate_inline(m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest),
         str_get_radio_frame_flags_description2(m_TestsInfo[m_iCurrentTestIndex].uRadioFlagsToTest) );
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = true;
      m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData = true;
      m_TestsInfo[m_iCurrentTestIndex].uTimeStarted = g_TimeNow;
      m_TestsInfo[m_iCurrentTestIndex].uTimeEnded = g_TimeNow;
      _advance_to_next_test();
      return;
   }

   m_iState = NEGOCIATE_STATE_START_TEST;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (A)* -> Start test.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
}

void MenuNegociateRadio::_save_new_settings_to_model()
{
   log_line("[NegociateRadioLink] Save new settings and flags to model. Updated settings? %s", m_bUpdated?"yes":"no");
   u8 uFlagsRuntimeCapab = g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab;

   if ( m_bUpdated )
   {
      // Apply new settings
      for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
      {
         g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iLink] = m_uRadioLinksTxFlagsToApply[iLink];
         g_pCurrentModel->radioLinksParams.link_radio_flags_rx[iLink] = m_uRadioLinksRxFlagsToApply[iLink];
      }
      for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
      {
         if ( ! m_bTestInterfaceIndex[iInt] )
            continue;
         g_pCurrentModel->radioInterfacesParams.interface_supported_radio_flags[iInt] = m_uRadioInterfacesSupportedRadioFlags[iInt];
      }
      memcpy(&g_pCurrentModel->radioInterfacesRuntimeCapab, &m_RadioInterfacesRuntimeCapabilitiesToApply, sizeof(type_radio_interfaces_runtime_capabilities_parameters));
   }

   g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab = uFlagsRuntimeCapab | MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      g_pCurrentModel->radioInterfacesRuntimeCapab.uInterfaceFlags[i] |= MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
       g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i] = 0;
       g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[i] = 0;
   }

   g_pCurrentModel->validateRadioSettings();
   log_line("[NegociateRadioLink] Current global model: VID: %u, %x; current selected model: VID: %u, %x",
      g_pCurrentModel->uVehicleId, g_pCurrentModel, getCurrentModel()->uVehicleId, getCurrentModel());
   saveCurrentModel();
   log_line("[NegociateRadioLink] Current vehicle radio links settings after updating current vehicle:");
   g_pCurrentModel->logVehicleRadioInfo();
   send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
   g_bMustNegociateRadioLinksFlag = false;
}

void MenuNegociateRadio::_onFinishedTests()
{
   log_line("[NegociateRadioLink] Finished running all tests.");
   m_bUpdated = _compute_settings_to_apply();

   for( int i=0; i<m_iTestsCount; i++ )
   {
      log_line("Test %d r-int %d %s, %s, DR %d, tx-pwr %d, Q: %.2f-%.2f%% %s",
          i, m_TestsInfo[i].iVehicleRadioInterface+1,
          m_TestsInfo[i].bSkipTest? "skiped": (m_TestsInfo[i].bSucceeded?"succes":"failed"),
          _getTestType(i), m_TestsInfo[i].iDataRateToTest, m_TestsInfo[i].iTxPowerMwToTest, m_TestsInfo[i].fComputedQualityMin, m_TestsInfo[i].fComputedQualityMax,
          str_get_radio_frame_flags_description2(m_TestsInfo[i].uRadioFlagsToTest));
   }
   log_line("[NegociateRadioLink] Current vehicle radio links configuration after finishing all tests:");
   g_pCurrentModel->logVehicleRadioInfo();

   m_iCurrentTestIndex = -1;
   if ( ! m_bUpdated )
   {
      log_line("[NegociateRadioLink] No updates where done to radio datarates or radio flags or radio qualities." );
      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION;
      addMessage2(0, L("Finished optimizing radio links."), L("No changes were done."));
      warnings_add(g_pCurrentModel->uVehicleId, L("Finished optimizing radio links."), g_idIconRadio);

      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (A)* -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   if ( ! m_bCanceled )
      _save_new_settings_to_model();

   // Check for new constrains on video bitrate based on new current supported datarates;

   u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( m_bTestInterfaceIndex[i] )
         continue;
      log_line("[NegociateRadioLink] Max supported video bitrate: %u bps; For max supported datarates on radio interface %d (radio link %d): legacy: %d, MCS: MCS-%d, DR boost: %d",
         uMaxVideoBitrate, i+1, g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1,
         g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxSupportedLegacyDataRate[i], -g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxSupportedMCSDataRate[i]-1, ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT));
   }
   log_line("[NegociateRadioLink] Current video profile video bitrate: %u bps", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS);

   bool bAllRadioInterfacesSupportHighRate = false;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! m_bTestInterfaceIndex[i] )
         continue;
      if ( (g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxSupportedMCSDataRate[i] < -4) &&
           (g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxSupportedLegacyDataRate[i] > 36000000) )
           continue;
      bAllRadioInterfacesSupportHighRate = false;
      break;
   }

   bool bUpdatedVideoBitrate = false;
   u32 uVideoBitrateToSet = 0;

   // Check and update video bitrates for all video profiles to make sure they fit in supported radio datarates

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( 0 == g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS )
         continue;
      log_line("[NegociateRadioLink] Video profile %s video bitrate: %u bps", str_get_video_profile_name(i), g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS);
      if ( g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS > uMaxVideoBitrate )
      {
         log_line("[NegociateRadioLink] Must decrease video bitrate (%.2f Mbps) for video profile %s to max allowed on current links: %.2f Mbps",
            (float)g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS/1000.0/1000.0,
            str_get_video_profile_name(i),
            (float)uMaxVideoBitrate/1000.0/1000.0);
         uVideoBitrateToSet = uMaxVideoBitrate;
         bUpdatedVideoBitrate = true;
      }

      if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200) || ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210) )
         continue;
      if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) || ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) )
         continue;
      if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_NONE) )
         continue;
      if ( bAllRadioInterfacesSupportHighRate && (g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS < 9000000 ) )
      {
         log_line("[NegociateRadioLink] Must increase video bitrate (%.2f Mbps) for video profile %s as radio supports higher datarates.",
            (float)g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS/1000.0/1000.0,
            str_get_video_profile_name(i));
         uVideoBitrateToSet = 9000000;
         bUpdatedVideoBitrate = true;
      }
   }

   if ( bUpdatedVideoBitrate && (0 != uVideoBitrateToSet) )
   {
      memcpy(&m_VideoParamsToApply, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&(m_VideoProfilesToApply[0]), &(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));

      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         if ( 0 != m_VideoProfilesToApply[i].uTargetVideoBitrateBPS )
            m_VideoProfilesToApply[i].uTargetVideoBitrateBPS = uVideoBitrateToSet;
      }
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_VIDEO_CONFIRMATION;
      m_iState = NEGOCIATE_STATE_SET_VIDEO_SETTINGS;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> Set video settings wait user", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

      addMessage2(0, L("Video bitrate updated"), L("Your video bitrate was updated to match the currently supported radio links."));
      return;
   }

   bool bAllInterfacesUseMCS = true;
   bool bAnyInterfaceHasMCS = false;
   int iInterfaceNoMCS = -1;
   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;
      if ( ! (m_uRadioLinksTxFlagsToApply[iInt] & RADIO_FLAGS_USE_MCS_DATARATES) )
      {
         bAllInterfacesUseMCS = false;
         iInterfaceNoMCS = iInt;
      }
      bAnyInterfaceHasMCS = true;
   }

   if ( ! bAllInterfacesUseMCS )
   {
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> MCS warning wait user", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

      if ( ! bAnyInterfaceHasMCS )
         addMessage2(0, L("Radio Link Quality Warning"), L("Your vehicle radio links should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."));
      else
      {
         char szBuff[256];
         sprintf(szBuff, L("Your vehicle radio interface %d should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."), iInterfaceNoMCS+1);
         addMessage2(0, L("Radio Link Quality Warning"), szBuff);
      }
      return;
   }

   m_iState = NEGOCIATE_STATE_SET_RADIO_SETTINGS;
   m_uTimeStartedVehicleOperation = 0;
   m_uLastTimeSentVehicleOperation = 0;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> Set radio settings.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
}

void MenuNegociateRadio::_computeQualitiesSoFarForCurrentTest()
{
   if ( m_iCurrentTestIndex < 0 )
      return;

   // Store/update supported datarates

   m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax = -1.0;
   m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin = -1.0;

   if ( ! m_TestsInfo[m_iCurrentTestIndex].bSkipTest )
   {
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
      {
         if ( !hardware_radio_index_is_wifi_radio(iInt) )
            continue;
         if ( g_SM_RadioStats.radio_interfaces[iInt].assignedVehicleRadioLinkId != m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioLink )
            continue;
         m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt] = 0.1;
         if ( m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[iInt] + m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[iInt] > 50 )
         {
            m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt] = (float)m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[iInt]/((float)m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[iInt] + (float)m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[iInt]);
            if ( (m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax < 0.0) || (m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt] > m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax) )
              m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax = m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt];
            if ( (m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin < 0.0) || (m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt] < m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin) )
              m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin = m_TestsInfo[m_iCurrentTestIndex].fQualityCards[iInt];
         }
         else if ( m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[iInt] > 0 )
             m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin = 0.1;
         else
             m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin = 0.1;
      }
      m_TestsInfo[m_iCurrentTestIndex].bComputedQualities = true;
      log_line("[NegociateRadioLink] Computed qualities for test %d: min: %.2f%%, max: %.2f%%", m_iCurrentTestIndex,
         m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin, m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMax);
   }

   int iVehicleRadioInterface = m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioInterface;
   if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest[iVehicleRadioInterface]) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest[iVehicleRadioInterface]) )
   if ( m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin >= TEST_DATARATE_QUALITY_THRESHOLD )
   {
      if ( m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest > 0 )
      if ( m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest > m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate[iVehicleRadioInterface] )
         m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate[iVehicleRadioInterface] = m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest;
      if ( m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest < 0 )
      if ( m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest < m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iVehicleRadioInterface] )
         m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iVehicleRadioInterface] = m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest;
   }

   //------------------------------------------------
   // Store radio data rates qualities

   if ( ! m_bSkipRateTests )
   {
      for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
      {
         if ( ! m_bTestInterfaceIndex[iInt] )
            continue;

         int iMaxTests = getTestDataRatesCountLegacy();
         if ( iMaxTests > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
            iMaxTests = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;
         for( int i=0; i<iMaxTests; i++ )
         {
            int iDataRate = getTestDataRatesLegacy()[i];
            int iTest = 0;
            float fQuality = _getMaxComputedQualityForDatarate(iInt, iDataRate, &iTest);
            m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[iInt][i] = (int)(fQuality*1000.0);
         }

         iMaxTests = getTestDataRatesCountMCS();
         if ( iMaxTests > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
            iMaxTests = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;
         for( int i=0; i<iMaxTests; i++ )
         {
            int iDataRate = getTestDataRatesMCS()[i];
            int iTest = 0;
            float fQuality = _getMaxComputedQualityForDatarate(iInt, iDataRate, &iTest);
            m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[iInt][i] = (int)(fQuality*1000.0);
         }
      }
   }

   //----------------------------------------
   // Store tx powers

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;

      int iIndex = m_iIndexFirstRadioPowersTest[iInt];
      for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTest[iInt]); i++ )
      {
         int iPower = -1;
         if ( ! m_TestsInfo[iIndex].bSkipTest )
         if ( m_TestsInfo[iIndex].bSucceeded )
            iPower = m_TestsInfo[iIndex].iTxPowerLastGood;

         m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] = iPower;
         iIndex++;
      }

      iIndex = m_iIndexFirstRadioPowersTestMCS[iInt];
      for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTestMCS[iInt]); i++ )
      {
         int iPower = -1;
         if ( ! m_TestsInfo[iIndex].bSkipTest )
         if ( m_TestsInfo[iIndex].bSucceeded )
            iPower = m_TestsInfo[iIndex].iTxPowerLastGood;

         m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] = iPower;
         iIndex++;
      }

      for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES-1; i++ )
      {
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] <= 0 )
         {
            if ( i == 0 )
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][0] = m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][1];
            else if ( ((i+1) <= (m_iIndexFirstRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTest[iInt])) && (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i+1] > 0) )
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] = (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1] + m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i+1])/2;
            else if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1] > 0)
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] = m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1] / 2;
         }
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] <= 0 )
         {
            if ( i == 0 )
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][0] = m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][1];
            else if ( ((i+1) <= (m_iIndexLastRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTestMCS[iInt])) && (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i+1] > 0) )
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] = (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1]+m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i+1])/2;
            else if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1] > 0)
               m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] = m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1] / 2;
         }

         if ( i > 3 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] > 0 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1] > 0 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] > (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1]*12)/10 )
            m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i] = (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[iInt][i-1]*12)/10;

         if ( i > 3 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] > 0 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1] > 0 )
         if ( m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] > (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1]*12)/10 )
            m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i] = (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[iInt][i-1]*12)/10;
      }
   }
}

// Returns true if radio flags to apply are different than model
bool MenuNegociateRadio::_compute_radio_flags_to_apply(int iVehicleRadioInterfaceIndex)
{
   bool bUpdatedRadioFlags = false;
   bool bUpdatedRadioCapabilities = false;

   if ( -1 == iVehicleRadioInterfaceIndex )
      log_line("[NegociateRadioLink] Compute radio flags to apply & supported by all vehicle's radio links and to all radio interfaces...");
   else
      log_line("[NegociateRadioLink] Compute radio flags to apply and supported for vehicle's radio link on radio interface %d", iVehicleRadioInterfaceIndex+1);

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;
      if ( (iVehicleRadioInterfaceIndex != -1) && (iInt != iVehicleRadioInterfaceIndex) )
         continue;
      float fQualityLegacyMin = 1.0;
      float fQualityLegacyMax = 0.0;
      for( int i=m_iIndexFirstDatarateLegacyTest[iInt]; i<=m_iIndexLastDatarateLegacyTest[iInt]; i++)
      {
         if ( m_TestsInfo[i].fComputedQualityMin < fQualityLegacyMin )
            fQualityLegacyMin = m_TestsInfo[i].fComputedQualityMin;

         if ( m_TestsInfo[i].fComputedQualityMax > fQualityLegacyMax )
            fQualityLegacyMax = m_TestsInfo[i].fComputedQualityMax;
      }

      float fQualityMCSMin = 1.0;
      float fQualityMCSMax = 0.0;
      if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]].fComputedQualityMin < fQualityMCSMin )
         fQualityMCSMin = m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]].fComputedQualityMin;
      if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]+1].fComputedQualityMin < fQualityMCSMin )
         fQualityMCSMin = m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]+1].fComputedQualityMin;

      if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]].fComputedQualityMax > fQualityMCSMax )
         fQualityMCSMax = m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]].fComputedQualityMax;
      if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]+1].fComputedQualityMax > fQualityMCSMax )
         fQualityMCSMax = m_TestsInfo[m_iIndexFirstRadioFlagsTest[iInt]+1].fComputedQualityMax;

      float fQualitySTBCMin = 1.0;
      if ( m_TestsInfo[m_iTestIndexSTBCV[iInt]].fComputedQualityMin < fQualitySTBCMin )
         fQualitySTBCMin = m_TestsInfo[m_iTestIndexSTBCV[iInt]].fComputedQualityMin;
      if ( m_TestsInfo[m_iTestIndexSTBCV[iInt]+1].fComputedQualityMin < fQualitySTBCMin )
         fQualitySTBCMin = m_TestsInfo[m_iTestIndexSTBCV[iInt]+1].fComputedQualityMin;

      float fQualitySTBCMax = 0.0;
      if ( m_TestsInfo[m_iTestIndexSTBCV[iInt]].fComputedQualityMax > fQualitySTBCMax )
         fQualitySTBCMax = m_TestsInfo[m_iTestIndexSTBCV[iInt]].fComputedQualityMax;
      if ( m_TestsInfo[m_iTestIndexSTBCV[iInt]+1].fComputedQualityMax > fQualitySTBCMax )
         fQualitySTBCMax = m_TestsInfo[m_iTestIndexSTBCV[iInt]+1].fComputedQualityMax;

      float fQualitySTBCLDPCMin = 1.0;
      if ( m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]].fComputedQualityMin < fQualitySTBCLDPCMin )
         fQualitySTBCLDPCMin = m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]].fComputedQualityMin;
      if ( m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]+1].fComputedQualityMin < fQualitySTBCLDPCMin )
         fQualitySTBCLDPCMin = m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]+1].fComputedQualityMin;

      float fQualitySTBCLDPCMax = 0.0;
      if ( m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]].fComputedQualityMax > fQualitySTBCLDPCMax )
         fQualitySTBCLDPCMax = m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]].fComputedQualityMax;
      if ( m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]+1].fComputedQualityMax > fQualitySTBCLDPCMax )
         fQualitySTBCLDPCMax = m_TestsInfo[m_iTestIndexSTBCLDPCV[iInt]+1].fComputedQualityMax;

      int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInt];
      if ( (iVehicleRadioLink < 0) || (iVehicleRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_softerror_and_alarm("[NegociateRadioLink] Invalid vehicle radio link (%d) assigned to vehicle radio interface %d", iVehicleRadioLink, iInt+1);
         continue;
      }
      log_line("[NegociateRadioLink] Compute radio link %d flags: Legacy min/max quality: %.3f / %.3f", iVehicleRadioLink+1, fQualityLegacyMin, fQualityLegacyMax);
      log_line("[NegociateRadioLink] Compute radio link %d flags: MCS min/max quality: %.3f / %.3f", iVehicleRadioLink+1, fQualityMCSMin, fQualityMCSMax);
      log_line("[NegociateRadioLink] Compute radio link %d flags: MCS STBC min/max quality: %.3f / %.3f", iVehicleRadioLink+1, fQualitySTBCMin, fQualitySTBCMax);
      log_line("[NegociateRadioLink] Compute radio link %d flags: MCS STBC-LDPC min/max quality: %.3f / %.3f", iVehicleRadioLink+1, fQualitySTBCLDPCMin, fQualitySTBCLDPCMax);

      if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAGS_USE_LEGACY_DATARATES )
      if ( !(g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & RADIO_FLAGS_USE_LEGACY_DATARATES) )
      {
         log_line("[NegociateRadioLink] Compute radio link %d flags: Model will switch from MCS to legacy data rates.", iVehicleRadioLink+1);
         bUpdatedRadioFlags = true;
      }

      if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
      if ( !(g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES) )
      {
         log_line("[NegociateRadioLink] Compute radio link %d flags: Model will switch from legacy to MCS data rates.", iVehicleRadioLink+1);
         bUpdatedRadioFlags = true;
      }

      if ( m_uRadioInterfacesSupportedRadioFlags[iInt] & RADIO_FLAGS_USE_LEGACY_DATARATES)
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_supported_radio_flags[iInt] & RADIO_FLAGS_USE_LEGACY_DATARATES) )
         bUpdatedRadioCapabilities = true;

      if ( m_uRadioInterfacesSupportedRadioFlags[iInt] & RADIO_FLAGS_USE_MCS_DATARATES)
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_supported_radio_flags[iInt] & RADIO_FLAGS_USE_MCS_DATARATES) )
         bUpdatedRadioCapabilities = true;

      if ( (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & (RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_USE_LEGACY_DATARATES)) ==
           (g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & (RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_USE_LEGACY_DATARATES)) )
         log_line("[NegociateRadioLink] Compute radio link %d flags: Model will not switch legacy or MCS data rates. Keep using %s data rates.", iVehicleRadioLink+1, (g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES)?"MCS":"legacy");

      if ( fQualitySTBCMin >= TEST_DATARATE_QUALITY_THRESHOLD )
      {
         if ( ! (m_uRadioInterfacesSupportedRadioFlags[iInt] & RADIO_FLAG_STBC) )
            bUpdatedRadioCapabilities = true;
         m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAG_STBC;
      }
      if ( fQualitySTBCLDPCMin >= TEST_DATARATE_QUALITY_THRESHOLD )
      {
         if ( ! (m_uRadioInterfacesSupportedRadioFlags[iInt] & RADIO_FLAG_LDPC) )
            bUpdatedRadioCapabilities = true;
         m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAG_LDPC;
      }

      if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
      {
         if ( fQualitySTBCMin >= fQualityMCSMin*0.95 )
         {
            log_line("[NegociateRadioLink] Compute radio link %d flags: STBC quality greater than no STBC.", iVehicleRadioLink+1);
            if ( ! (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_STBC) )
               bUpdatedRadioFlags = true;
            m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] |= RADIO_FLAG_STBC;
            m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAG_STBC;
         }
         else
         {
            log_line("[NegociateRadioLink] Compute radio link %d flags: STBC quality lower than no STBC.", iVehicleRadioLink+1);
            if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_STBC )
               bUpdatedRadioFlags = true;
            m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] &= ~RADIO_FLAG_STBC;
         }

         if ( (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_STBC) != (g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & RADIO_FLAG_STBC) )
         {
            if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_STBC )
               log_line("[NegociateRadioLink] Compute radio link %d flags: STBC flag was added. Will apply settings.", iVehicleRadioLink+1);
            else
               log_line("[NegociateRadioLink] Compute radio link %d flags: STBC flag was removed. Will apply settings.", iVehicleRadioLink+1);
            bUpdatedRadioFlags = true;
         }
         else
            log_line("[NegociateRadioLink] Compute radio flags: STBC flag was not changed.");

         bool bUseLDPC = false;
         if ( (fQualitySTBCLDPCMin >= fQualitySTBCMin*1.001) && (fQualitySTBCLDPCMin >= fQualityMCSMin*1.001) )
            bUseLDPC = true;
         if ( (fQualitySTBCLDPCMin >= fQualitySTBCMin*1.0001) && (fQualitySTBCLDPCMin >= fQualityMCSMin*1.0001) )
         if ( (fQualitySTBCLDPCMax >= fQualitySTBCMax*1.002) && (fQualitySTBCLDPCMax >= fQualityMCSMax*1.002) )
            bUseLDPC = true;

         if ( bUseLDPC && (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_STBC) )
         {
            log_line("[NegociateRadioLink] Compute radio link %d flags: STBC-LDPC quality greater than STBC and MCS.", iVehicleRadioLink+1);
            m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] |= RADIO_FLAG_LDPC;
            m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAG_LDPC;
         }
         else
         {
            log_line("[NegociateRadioLink] Compute radio link %d flags: STBC-LDPC quality lower than STBC or MCS.", iVehicleRadioLink+1);
            m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] &= ~RADIO_FLAG_LDPC;
         }

         if ( (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_LDPC) != (g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & RADIO_FLAG_LDPC) )
         {
            if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & RADIO_FLAG_LDPC )
               log_line("[NegociateRadioLink] Compute radio link %d flags: LDPC flag was added. Will apply settings.", iVehicleRadioLink+1);
            else
               log_line("[NegociateRadioLink] Compute radio link %d flags: LDPC flag was removed. Will apply settings.", iVehicleRadioLink+1);
            bUpdatedRadioFlags = true;
         }
         else
            log_line("[NegociateRadioLink] Compute radio link %d flags: STBC-LDPC flag was not changed.", iVehicleRadioLink+1);
      }

      char szFlagsBefore[256];
      char szFlagsAfter[256];
      str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink], szFlagsBefore);
      if ( m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] == g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] )
      {
         bUpdatedRadioFlags = false;
         log_line("[NegociateRadioLink] Compute radio link %d flags: No change in radio flags (now: %s)", iVehicleRadioLink+1, szFlagsBefore);
      }
      else
      {
         str_get_radio_frame_flags_description(m_uRadioLinksTxFlagsToApply[iVehicleRadioLink], szFlagsAfter);
         log_line("[NegociateRadioLink] Compute radio link %d flags: Radio flags will be updated from model: %s to %s",
            iVehicleRadioLink+1, szFlagsBefore, szFlagsAfter);
      }
   }

   log_line("[NegociateRadioLink] Compute flags: Updates where done? %s", (bUpdatedRadioFlags | bUpdatedRadioCapabilities)?"yes":"no");
   if ( -1 == iVehicleRadioInterfaceIndex )
   {
      for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
      {
         if ( ! m_bTestInterfaceIndex[iInt] )
            continue;

         int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInt];
         log_line("[NegociateRadioLink] Computed radio flags to apply to vehicle radio link %d: %s", iVehicleRadioLink+1, str_get_radio_frame_flags_description2(m_uRadioLinksTxFlagsToApply[iVehicleRadioLink]));
         log_line("[NegociateRadioLink] Computed supported radio flags for vehicle radio interface %d: %s", iInt+1, str_get_radio_frame_flags_description2(m_uRadioInterfacesSupportedRadioFlags[iInt]));
      }
   }
   else
   {
      int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iVehicleRadioInterfaceIndex];
      log_line("[NegociateRadioLink] Computed radio flags to apply to vehicle radio link %d: %s", iVehicleRadioLink+1, str_get_radio_frame_flags_description2(m_uRadioLinksTxFlagsToApply[iVehicleRadioLink]));
      log_line("[NegociateRadioLink] Computed supported radio flags for vehicle radio interface %d: %s", iVehicleRadioInterfaceIndex+1, str_get_radio_frame_flags_description2(m_uRadioInterfacesSupportedRadioFlags[iVehicleRadioInterfaceIndex]));
   }
   return bUpdatedRadioFlags | bUpdatedRadioCapabilities;
}

// Returns true if settings to apply are different than model
bool MenuNegociateRadio::_compute_settings_to_apply()
{
   _computeQualitiesSoFarForCurrentTest();

   m_RadioInterfacesRuntimeCapabilitiesToApply.uFlagsRuntimeCapab = g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab;
   bool bUpdatedRadioFlags = false;

   for( int iInt=0; iInt<g_pCurrentModel->radioInterfacesParams.interfaces_count; iInt++ )
   {
      if ( ! m_bTestInterfaceIndex[iInt] )
         continue;

      int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInt];
      if ( (iVehicleRadioLink < 0) || (iVehicleRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_softerror_and_alarm("[NegociateRadioLink] Compute settings to apply: Invalid vehicle radio link (%d) assigned to vehicle radio interface %d", iVehicleRadioLink, iInt+1);
         continue;
      }
      int iTest6 = -1, iTest12 = -1, iTest18 = -1, iTestMCS0Max = -1, iTestMCS1Max = -1, iTestMCS2Max = -1, iTestMCS0Min = -1, iTestMCS1Min = -1, iTestMCS2Min = -1;
      float fQualityMax6 = _getMaxComputedQualityForDatarate(iInt, 6000000, &iTest6);
      float fQualityMax12 = _getMaxComputedQualityForDatarate(iInt, 12000000, &iTest12);
      float fQualityMax18 = _getMaxComputedQualityForDatarate(iInt, 18000000, &iTest18);
      float fQualityMaxMCS0 = _getMaxComputedQualityForDatarate(iInt, -1, &iTestMCS0Max);
      float fQualityMaxMCS1 = _getMaxComputedQualityForDatarate(iInt, -2, &iTestMCS1Max);
      float fQualityMaxMCS2 = _getMaxComputedQualityForDatarate(iInt, -3, &iTestMCS2Max);

      float fQualityMin6 = _getMinComputedQualityForDatarate(iInt, 6000000, &iTest6);
      float fQualityMin12 = _getMinComputedQualityForDatarate(iInt, 12000000, &iTest12);
      float fQualityMin18 = _getMinComputedQualityForDatarate(iInt, 18000000, &iTest18);
      float fQualityMinMCS0 = _getMinComputedQualityForDatarate(iInt, -1, &iTestMCS0Min);
      float fQualityMinMCS1 = _getMinComputedQualityForDatarate(iInt, -2, &iTestMCS1Min);
      float fQualityMinMCS2 = _getMinComputedQualityForDatarate(iInt, -3, &iTestMCS2Min);

      float fQualMinLegacy = 1.0;
      float fQualMinMCS = 1.0;
      float fQualMaxLegacy = 0.0;
      float fQualMaxMCS = 0.0;

      if ( (iTestMCS0Min >= 0) && m_TestsInfo[iTestMCS0Min].bSucceeded )
      if ( fQualityMinMCS0 < fQualMinMCS )
         fQualMinMCS = fQualityMinMCS0;
      if ( (iTestMCS1Min >= 0) && m_TestsInfo[iTestMCS1Min].bSucceeded && (! m_TestsInfo[iTestMCS1Min].bSkipTest) )
      if ( fQualityMinMCS1 < fQualMinMCS )
         fQualMinMCS = fQualityMinMCS1;
      if ( (iTestMCS2Min >= 0) && m_TestsInfo[iTestMCS2Min].bSucceeded && (! m_TestsInfo[iTestMCS2Min].bSkipTest) )
      if ( fQualityMinMCS2 < fQualMinMCS )
         fQualMinMCS = fQualityMinMCS2;

      if ( (iTest6 >= 0) && m_TestsInfo[iTest6].bSucceeded )
      if ( fQualityMin6 < fQualMinLegacy )
         fQualMinLegacy = fQualityMin6;
      if ( (iTest12 >= 0) && m_TestsInfo[iTest12].bSucceeded && (! m_TestsInfo[iTest12].bSkipTest) )
      if ( fQualityMin12 < fQualMinLegacy )
         fQualMinLegacy = fQualityMin12;
      if ( (iTest18 >= 0) && m_TestsInfo[iTest18].bSucceeded && (! m_TestsInfo[iTest18].bSkipTest) )
      if ( fQualityMin18 < fQualMinLegacy )
         fQualMinLegacy = fQualityMin18;


      if ( (iTestMCS0Max >= 0) && m_TestsInfo[iTestMCS0Max].bSucceeded )
      if ( fQualityMaxMCS0 > fQualMaxMCS )
         fQualMaxMCS = fQualityMaxMCS0;
      if ( (iTestMCS1Max >= 0) && m_TestsInfo[iTestMCS1Max].bSucceeded )
      if ( fQualityMaxMCS1 > fQualMaxMCS )
         fQualMaxMCS = fQualityMaxMCS1;
      if ( (iTestMCS2Max >= 0) && m_TestsInfo[iTestMCS2Max].bSucceeded )
      if ( fQualityMaxMCS2 > fQualMaxMCS )
         fQualMaxMCS = fQualityMaxMCS2;

      if ( (iTest6 >= 0) && m_TestsInfo[iTest6].bSucceeded )
      if ( fQualityMax6 > fQualMaxLegacy )
         fQualMaxLegacy = fQualityMax6;
      if ( (iTest12 >= 0) && m_TestsInfo[iTest12].bSucceeded )
      if ( fQualityMax12 > fQualMaxLegacy )
         fQualMaxLegacy = fQualityMin12;
      if ( (iTest18 >= 0) && m_TestsInfo[iTest18].bSucceeded )
      if ( fQualityMax18 > fQualMaxLegacy )
         fQualMaxLegacy = fQualityMax18;


      char szBuff[512];
      szBuff[0] = 0;
      int iCountRatesLegacy = getTestDataRatesCountLegacy();
      if ( iCountRatesLegacy > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
         iCountRatesLegacy = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;
      for( int i=0; i<iCountRatesLegacy; i++ )
      for( int k=0; k<2; k++ )
      {
         int iTestIndex = i*2 + k + m_iIndexFirstDatarateLegacyTest[iInt];
         char szTmp[64];
         sprintf(szTmp, "[%d: (%.2f-%.2f) %s]",
             getTestDataRatesLegacy()[i], m_TestsInfo[iTestIndex].fComputedQualityMin, m_TestsInfo[iTestIndex].fComputedQualityMax, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"));
         if ( i != 0 )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Compute settings to apply: Test rates results for radio interface %d (legacy): %s", iInt+1, szBuff);

      szBuff[0] = 0;
      int iCountRatesMCS = getTestDataRatesCountMCS();
      if ( iCountRatesMCS > MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES )
         iCountRatesMCS = MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES;
      for( int i=0; i<iCountRatesMCS; i++ )
      for( int k=0; k<2; k++ )
      {
         int iTestIndex = i*2 + k + m_iIndexFirstDatarateMCSTest[iInt];
         char szTmp[64];
         sprintf(szTmp, "[%d: (%.2f-%.2f) %s]",
             -getTestDataRatesMCS()[i]-1, m_TestsInfo[iTestIndex].fComputedQualityMin, m_TestsInfo[iTestIndex].fComputedQualityMax, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"));

         if ( i != 0 )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      log_line("[NegociateRadioLink] Compute settings to apply: Test rates results for radio interface %d (MCS): %s", iInt+1, szBuff);

      log_line("[NegociateRadioLink] Computed max usable rates for radio interface %d: legacy: %d, MCS: MCS-%d", iInt+1, m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate[iInt], -m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iInt]-1);

      szBuff[0] = 0;
      int iTestIndex = m_iIndexFirstRadioPowersTest[iInt];
      for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTest[iInt]); i++ )
      {
         if ( (i >= getTestDataRatesCountLegacy()) || (i >= MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES) )
            break;
         char szTmp[64];
         sprintf(szTmp, "[%d: (%s %d mW)]",
             getTestDataRatesLegacy()[i], m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"), m_TestsInfo[iTestIndex].iTxPowerLastGood);
         if ( i != 0 )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
         iTestIndex++;
      }
      log_line("[NegociateRadioLink] Compute settings to apply: Test powers results for radio interface %d (legacy): %s", iInt+1, szBuff);

      szBuff[0] = 0;
      iTestIndex = m_iIndexFirstRadioPowersTestMCS[iInt];
      for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS[iInt] - m_iIndexFirstRadioPowersTestMCS[iInt]); i++ )
      {
         if ( i >= getTestDataRatesCountMCS() || (i >= MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES))
            break;
         char szTmp[64];
         sprintf(szTmp, "[MCS-%d: (%s %d mW)]",
             -getTestDataRatesMCS()[i]-1, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"), m_TestsInfo[iTestIndex].iTxPowerLastGood);
         if ( i != 0 )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
         iTestIndex++;
      }
      log_line("[NegociateRadioLink] Compute settings to apply: Test powers results for radio interface %d (MCS): %s", iInt+1, szBuff);

      hardware_sleep_ms(5);
      log_line("[NegociateRadioLink] Checkpoint.");
      hardware_sleep_ms(50);
      bool bUseMCSRates = false;

      if ( (iTestMCS2Max >= 0) && m_TestsInfo[iTestMCS2Max].bSucceeded && (! m_TestsInfo[iTestMCS2Max].bSkipTest) )
      if ( fQualMaxMCS >= fQualMaxLegacy*0.98 )
      //if ( fabs(fQualMaxMCS-fQualMaxLegacy) < 0.2 )
         bUseMCSRates = true;

      if ( (iTestMCS2Max >= 0) && m_TestsInfo[iTestMCS2Max].bSucceeded && (! m_TestsInfo[iTestMCS2Max].bSkipTest) )
      //if ( fabs(fQualMinMCS-fQualMinLegacy) < 0.1 )
      if ( fabs(fQualMaxMCS-fQualMaxLegacy) < 0.02 )
         bUseMCSRates = true;

      log_line("[NegociateRadioLink] Checkpoint2.");
      hardware_sleep_ms(50);

      if ( bUseMCSRates && (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate[iVehicleRadioLink] <= -1) )
      {
         log_line("[NegociateRadioLink] Compute settings: Radio interface %d MCS rx quality (min: %.3f max: %.3f) is greater than Legacy rx quality (min: %.3f max: %.3f)", iInt+1, fQualMinMCS, fQualMaxMCS, fQualMinLegacy, fQualMaxLegacy);
         m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] = RADIO_FLAGS_USE_MCS_DATARATES;
         m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAG_HT40;
      }
      else
      {
         log_line("[NegociateRadioLink] Compute settings: Radio interface %d Legacy rx quality (min: %.3f max: %.3f) is greater than MCS rx quality (min: %.3f max: %.3f)", iInt+1, fQualMinLegacy, fQualMaxLegacy, fQualMinMCS, fQualMaxMCS);
         m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] = RADIO_FLAGS_USE_LEGACY_DATARATES;
         m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAGS_USE_LEGACY_DATARATES;
      }

      m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] |= RADIO_FLAGS_FRAME_TYPE_DATA;
      m_uRadioInterfacesSupportedRadioFlags[iInt] |= RADIO_FLAGS_FRAME_TYPE_DATA;
      log_line("[NegociateRadioLink] Checkpoint3.");
      hardware_sleep_ms(50);

      if ( (m_uRadioLinksTxFlagsToApply[iVehicleRadioLink] & (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES)) !=
           (g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iVehicleRadioLink] & (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES)) )
          bUpdatedRadioFlags = true;
   }

   bUpdatedRadioFlags |= _compute_radio_flags_to_apply(-1);

   bool bUpdatedPowers = false;

   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
   for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
   {
      if ( (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[k][i] != g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxTxPowerMwLegacy[k][i]) ||
           (m_RadioInterfacesRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[k][i] != g_pCurrentModel->radioInterfacesRuntimeCapab.iMaxTxPowerMwMCS[k][i]) )
      {
         bUpdatedPowers = true;
         log_line("[NegociateRadioLink] Compute settings: Tx powers values are different and must be stored and updated.");
         break;
      }
   }
   log_line("[NegociateRadioLink] Checkpoint4.");
   hardware_sleep_ms(50);

   bool bUpdatedDatarates = false;
   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
   for( int i=0; i<MODEL_MAX_STORED_RADIO_INTERFACE_QUALITIES_VALUES; i++ )
   {
      if ( (fabs(m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesLegacy[k][i] - g_pCurrentModel->radioInterfacesRuntimeCapab.iQualitiesLegacy[k][i]) > 30) ||
           (fabs(m_RadioInterfacesRuntimeCapabilitiesToApply.iQualitiesMCS[k][i] - g_pCurrentModel->radioInterfacesRuntimeCapab.iQualitiesMCS[k][i]) > 30) )
      {
         bUpdatedDatarates = true;
         log_line("[NegociateRadioLink] Compute settings: Supported datarates values are different and must be stored and updated.");
         break;
      }
   }
   log_line("[NegociateRadioLink] Checkpoint 5.");
   hardware_sleep_ms(50);

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
      if ( (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i] != 0) ||
           (g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[i] != 0) )
      {
         bUpdatedDatarates = true;
         log_line("[NegociateRadioLink] Compute settings: Vehicle radio link %d is using fixed datarates. Must switch it to auto datarates.", i+1);
         log_line("[NegociateRadioLink] Compute settings: Vehicle radio link %d datarate video: %d, radio link flags: %s", i+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i], str_get_radio_frame_flags_description2(g_pCurrentModel->radioLinksParams.link_radio_flags_tx[i]));
         log_line("[NegociateRadioLink] Will apply datarate: auto");
      }
   }
   log_line("[NegociateRadioLink] Checkpoint 6.");
   hardware_sleep_ms(50);

   if ( ! m_bCanceled )
   {
       m_RadioInterfacesRuntimeCapabilitiesToApply.uFlagsRuntimeCapab |= MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
       for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
           m_RadioInterfacesRuntimeCapabilitiesToApply.uInterfaceFlags[i] |= MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   }
   if ( (! bUpdatedRadioFlags) && (!bUpdatedDatarates) && (!bUpdatedPowers) )
   {
      log_line("[NegociateRadioLink] Compute settings: No change detected in computed radio links flags or datarates or powers.");
      return false;
   }

   log_line("[NegociateRadioLink] Updates detected for: datarates qualities: %s, radio links flags: %s, tx powers: %s",
      bUpdatedDatarates?"yes":"no",
      bUpdatedRadioFlags?"yes":"no",
      bUpdatedPowers?"yes":"no");

   g_pCurrentModel->validateRadioSettings();
   return true;
}


void MenuNegociateRadio::onReceivedVehicleResponse(u8* pPacketData, int iPacketLength)
{
   if ( NULL == pPacketData )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketData;

   if ( pPH->packet_type != PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
      return;

   int iRecvTestIndex = (int)(pPacketData[sizeof(t_packet_header)]);
   u8 uCommand = pPacketData[sizeof(t_packet_header) + sizeof(u8)];

   log_line("[NegociateRadioLink] On Vehicle Response message: State: [%s], User state: [%s]",
      str_get_negociate_state(m_iState), str_get_negociate_user_state(m_iUserState));
   log_line("[NegociateRadioLink] Received response from vehicle to test: %d, command: %d, current negociate state: %s, is flow in canceled state: %s", iRecvTestIndex, uCommand, str_get_negociate_state(m_iState), m_bCanceled?"yes":"no");

   if ( m_iState == NEGOCIATE_STATE_TEST_RUNNING_WAIT_VEHICLE_START_TEST_CONFIRMATION )
   {
      if ( m_iCurrentTestIndex != iRecvTestIndex )
      {
         log_line("[NegociateRadioLink] Ignore received vehicle response for wrong test %d, controller is now on test %d", iRecvTestIndex, m_iCurrentTestIndex);
         return;
      }
      if ( uCommand == NEGOCIATE_RADIO_TEST_PARAMS )
      {
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (V) Wait vehicle start test -> Test running.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
         if ( m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle == 0 )
            m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle = g_TimeNow;
         m_iState = NEGOCIATE_STATE_TEST_RUNNING;
      }
      return;
   }

   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_END_CONFIRMATION )
   if ( uCommand == NEGOCIATE_RADIO_END_TESTS )
   {
      log_line("[NegociateRadioLink] Received confirmation from vehicle to end tests. End state: %s", m_bCanceled?"canceled":"finished");
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (V) Wait vehicle end tests -> Finish flow.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      _onFinishedFlow();
      return;
   }

   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_RADIO_SETTINGS_CONFIRMATION )
   {
      _save_new_settings_to_model();

      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (V) Wait vehicle apply radio settings confirmation -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION;

      addMessage2(0, L("Finished optimizing radio links."), L("Radio links configuration was updated."));
      warnings_add(g_pCurrentModel->uVehicleId, L("Finished optimizing radio links."), g_idIconRadio);
   }

   log_line("[NegociateRadioLink] Ignored received radio test %d confirmation, command: %d, is state canceled: %d", iRecvTestIndex, uCommand, m_bCanceled);
}

void MenuNegociateRadio::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   log_line("[NegociateRadioLink] OnReturnFromChild: child menu id: %d, return value: %d, current state: %d, current user state: %d",
      iChildMenuId, returnValue, m_iState, m_iUserState);

   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_CANCEL )
   {
      log_line("[NegociateRadioLink] Canceled negociate radio links.");

      m_iUserState = NEGOCIATE_USER_STATE_NONE;
      if ( 0 == returnValue )
         return;

      if ( g_pControllerSettings->iDeveloperMode )
         _send_revert_flags_to_vehicle();

      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_bCanceled = true;
      m_iCurrentTestIndex = -1;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (R) User wait cancel -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   if ( (m_iUserState == NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION) ||
        (m_iUserState == NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_VIDEO_CONFIRMATION) ||
        (m_iUserState == NEGOCIATE_USER_STATE_WAIT_FAILED_APPLY_RADIO_CONFIRMATION) )
   {
      log_line("[NegociateRadioLink] Finished waiting for user confirmation on: Failed negociate radio links.");

      m_iUserState = NEGOCIATE_USER_STATE_NONE;
      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_bFailed = true;
      m_iCurrentTestIndex = -1;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (R) User wait failed confirmation -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_VIDEO_CONFIRMATION )
   {
      log_line("[NegociateRadioLink] Finished waiting for user confirmation to start set video settings.");
      m_iUserState = NEGOCIATE_USER_STATE_NONE;
      m_uTimeStartedVehicleOperation = g_TimeNow;
      m_uLastTimeSentVehicleOperation = g_TimeNow;
      g_pCurrentModel->logVideoSettingsDifferences(&m_VideoParamsToApply, &(m_VideoProfilesToApply[m_VideoParamsToApply.iCurrentVideoProfile]), false);
      if ( handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&m_VideoParamsToApply, sizeof(video_parameters_t), (u8*)&m_VideoProfilesToApply, MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
      {
         m_iState = NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION;
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F)(U) -> Wait set video settings ack.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      }
      else
      {
         m_iState = NEGOCIATE_STATE_SET_VIDEO_SETTINGS;
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F)(U) -> Set video settings", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      }
      return;
   }

   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION )
   {
      m_iUserState = NEGOCIATE_USER_STATE_NONE;
      m_iState = NEGOCIATE_STATE_SET_RADIO_SETTINGS;
      m_uTimeStartedVehicleOperation = g_TimeNow;
      m_uLastTimeSentVehicleOperation = g_TimeNow;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F)(U)(MCS) -> Set radio settings.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION )
   {
      m_iUserState = NEGOCIATE_USER_STATE_NONE;
      return;
   }

}

void MenuNegociateRadio::_onCancel()
{
   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_CANCEL )
      return;

   if ( (m_iState == NEGOCIATE_STATE_END_TESTS) || (m_iState == NEGOCIATE_STATE_ENDED) )
   {
      _onFinishedFlow();
      return;
   }

   m_iUserState = NEGOCIATE_USER_STATE_WAIT_CANCEL;
   MenuConfirmation* pMenu = new MenuConfirmation("Cancel Radio Link Adjustment","Are you sure you want to cancel the radio config adjustment wizard?", 11);
   pMenu->addTopLine("If you cancel the wizard your radio links might not be optimally configured, resulting in potential lower radio link quality.");
   pMenu->m_yPos = 0.3;
   add_menu_to_stack(pMenu);
}

void MenuNegociateRadio::_onFinishedFlow()
{
   log_line("[NegociateRadioLink] OnFinishedFlow");
   m_iState = NEGOCIATE_STATE_ENDED;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): * -> State ended.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
   _close();
}

void MenuNegociateRadio::_close()
{
   log_line("[NegociateRadioLink] Closing...");
   if ( -1 != m_MenuIndexCancel )
   {
      removeMenuItem(m_pMenuItems[0]);
      m_MenuIndexCancel = -1;
   }

   menu_rearrange_all_menus_no_animation();
   menu_loop(true);

   menu_stack_pop_no_delete(0);
   setModal(false);
   if ( m_bCanceled )
      warnings_add(g_pCurrentModel->uVehicleId, L("Canceled optmizie radio links."), g_idIconRadio);
   else if ( m_bFailed )
      warnings_add(g_pCurrentModel->uVehicleId, L("Failed to optmizie radio links."), g_idIconRadio);
   if ( onEventPairingTookUIActions() )
      onEventFinishedPairUIAction();
   log_line("[NegociateRadioLink] Closed.");

   if ( g_pControllerSettings->iDeveloperMode )
      add_menu_to_stack(new MenuVehicleRadioRuntimeCapabilities());

}

void MenuNegociateRadio::onSelectItem()
{
   Menu::onSelectItem();
   if ( -1 == m_SelectedIndex )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_iUserState == NEGOCIATE_USER_STATE_NONE )
   if ( m_iCurrentTestIndex < m_iTestsCount-1 )
   if ( (-1 != m_MenuIndexCancel) && (m_MenuIndexCancel == m_SelectedIndex) )
   {
      _onCancel();
   }
}
