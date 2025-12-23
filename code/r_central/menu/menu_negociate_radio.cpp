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
#define NEGOCIATE_USER_STATE_WAIT_VIDEO_CONFIRMATION 3
#define NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION 4
#define NEGOCIATE_USER_STATE_WAIT_FINISH_CONFIRMATION 5


MenuNegociateRadio::MenuNegociateRadio(void)
:Menu(MENU_ID_NEGOCIATE_RADIO, L("Initial Auto Radio Link Adjustment"), NULL)
{
   m_Width = 0.72;
   m_xPos = 0.14; m_yPos = 0.26;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   addExtraHeightAtEnd(7.0*height_text + height_text * 1.5 * hardware_get_radio_interfaces_count());
   m_uShowTime = g_TimeNow;
   m_MenuIndexCancel = -1;
   m_iLoopCounter = 0;
   m_iMainVehicleLinkIndexToTest = 0;
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

void MenuNegociateRadio::valuesToUI()
{
}

void MenuNegociateRadio::_reset_tests_and_state()
{
   m_iState = NEGOCIATE_STATE_START_TEST;
   m_iUserState = NEGOCIATE_USER_STATE_NONE;
   m_bCanceled = false;
   m_bFailed = false;
   m_bUpdated = false;

   u32 uMainConnectFreq = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
   m_iMainVehicleLinkIndexToTest = 0;
   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
   {
      m_iMainVehicleLinkIndexToTest = -1;
      for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
      {
         if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
         if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
            continue;
         if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink] == uMainConnectFreq )
         {
            m_iMainVehicleLinkIndexToTest = iLink;
            break;
         }
      }
      if ( -1 == m_iMainVehicleLinkIndexToTest )
      {
         log_softerror_and_alarm("[NegociateRadioLink] Can't find a valid main vehicle radio link to use, vehicle has %d radio links.", g_pCurrentModel->radioLinksParams.links_count);
         m_iMainVehicleLinkIndexToTest = 0;
      }
   }

   log_line("[NegociateRadioLink] Will test vehicle's radio link %d (out of %d links), link freq: %s, main connect freq: %uMhz",
      m_iMainVehicleLinkIndexToTest+1, g_pCurrentModel->radioLinksParams.links_count,
      str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iMainVehicleLinkIndexToTest]),
      uMainConnectFreq/1000);

   strcpy(m_szStatusMessage, L("Please wait, it will take a minute."));
   strcpy(m_szStatusMessage2, L("Testing radio modulation schemes"));

   log_line("[NegociateRadioLink] Reset state.");

   for( int i=0; i<MAX_NEGOCIATE_TESTS; i++ )
   {
      memset(&(m_TestsInfo[i]), 0, sizeof(type_negociate_radio_step));
      m_TestsInfo[i].bSkipTest = false;
      m_TestsInfo[i].bMustTestUplink = false;

      m_TestsInfo[i].bHasSubTests = false;
      m_TestsInfo[i].iCurrentSubTest = -1;

      m_TestsInfo[i].iVehicleRadioLink = m_iMainVehicleLinkIndexToTest;
      m_TestsInfo[i].bSucceeded = false;
      m_TestsInfo[i].bReceivedEnoughData = false;
      m_TestsInfo[i].fComputedQualityMax = -1.0;
      m_TestsInfo[i].fComputedQualityMin = -1.0;
      m_TestsInfo[i].uTimeStarted = 0;
      m_TestsInfo[i].uTimeEnded = 0;
      m_TestsInfo[i].iCountSendStarts = 0;
      m_TestsInfo[i].uDurationToTest = 1000;
      m_TestsInfo[i].uDurationSubTest = 1000;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         m_TestsInfo[i].iRadioInterfacesRXPackets[k] = 0;
         m_TestsInfo[i].iRadioInterfacesRxLostPackets[k] = 0;
         m_TestsInfo[i].fQualityCards[k] = -1.0;
      }
   }
   m_iTestsCount = 0;

   //-----------------------------------------------
   // Radio flags tests

   m_iIndexFirstRadioFlagsTest = m_iTestsCount;
   for( int k=0; k<2; k++ )
   {
      m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
      m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
      m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*3;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      m_iTestsCount++;
   }

   m_iTestIndexSTBCV = m_iTestsCount;
   for( int k=0; k<2; k++ )
   {
      m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
      m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
      m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_STBC_VEHICLE;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      m_iTestsCount++;
   }

   m_iTestIndexLDPVV = m_iTestsCount;
   for( int k=0; k<2; k++ )
   {
      m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
      m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
      m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_LDPC_VEHICLE;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      m_iTestsCount++;
   }
   m_iTestIndexSTBCLDPCV = m_iTestsCount;
   for( int k=0; k<2; k++ )
   {
      m_TestsInfo[m_iTestsCount].iDataRateToTest = -3; // MCS-2
      m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
      m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION*2;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAG_STBC_VEHICLE | RADIO_FLAG_LDPC_VEHICLE;
      m_TestsInfo[m_iTestsCount].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      m_iTestsCount++;
   }
   m_iIndexLastRadioFlagsTest = m_iTestsCount-1;

   //-----------------------------------------------------
   // Rates tests

   m_iIndexFirstDatarateLegacyTest = m_iTestsCount;
   int iCountTestsLegacy = getTestDataRatesCountLegacy();
   if ( iCountTestsLegacy > MODEL_MAX_STORED_QUALITIES_VALUES )
      iCountTestsLegacy = MODEL_MAX_STORED_QUALITIES_VALUES;
   for( int i=0; i<iCountTestsLegacy; i++ )
   {
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesLegacy()[i];
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_LEGACY_DATARATES;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION;
         if ( ((i == 0) && (k == 0)) || (i == 3) )
            m_TestsInfo[m_iTestsCount].uDurationToTest *=2;
         m_iTestsCount++;
      }
   }
   m_iIndexLastDatarateLegacyTest = m_iTestsCount-1;
   m_iIndexFirstDatarateMCSTest = m_iTestsCount;
   int iCountTestsMCS = getTestDataRatesCountMCS();
   if ( iCountTestsMCS > MODEL_MAX_STORED_QUALITIES_VALUES )
      iCountTestsMCS = MODEL_MAX_STORED_QUALITIES_VALUES;
   for( int i=0; i<iCountTestsMCS; i++ )
   {
      for( int k=0; k<2; k++ )
      {
         m_TestsInfo[m_iTestsCount].iDataRateToTest = getTestDataRatesMCS()[i];
         m_TestsInfo[m_iTestsCount].uRadioFlagsToTest = RADIO_FLAGS_USE_MCS_DATARATES;
         m_TestsInfo[m_iTestsCount].iTxPowerMwToTest = DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO;
         m_TestsInfo[m_iTestsCount].uDurationToTest = SINGLE_TEST_DURATION;
         if ( ((i == 0) && (k == 0)) || (i == 3) )
            m_TestsInfo[m_iTestsCount].uDurationToTest *=2;
         m_iTestsCount++;
      }
   }
   m_iIndexLastDatarateMCSTest = m_iTestsCount-1;


   //-----------------------------------------------
   // Tx powers tests

   m_iIndexFirstRadioPowersTest = m_iTestsCount;

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[0];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;
   int iRadioInterfacePowerMaxMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);

   log_line("[NegociateRadioLink] Vehicle radio link 1 card type: %s, max power: %d mW", str_get_radio_card_model_string(iRadioInterfacelModel), iRadioInterfacePowerMaxMw);
   
   for( int i=0; i<6/*getTestDataRatesCountLegacy()*/; i++ )
   {
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

   m_iIndexFirstRadioPowersTestMCS = m_iTestsCount;
   
   for( int i=0; i<iCountTestsMCS; i++ )
   {
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

   m_iIndexLastRadioPowersTestMCS = m_iTestsCount-1;

   for( int i=0; i<m_iTestsCount; i++ )
   {
      m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_FRAME_TYPE_DATA;
   }

   m_iTxPowerMwToApply = 0;
   m_uRadioFlagsToApply = g_pCurrentModel->radioLinksParams.link_radio_flags[0];

   m_uTimeStartedVehicleOperation = 0;
   m_uLastTimeSentVehicleOperation = 0;
   g_bAskedForNegociateRadioLink = true;

   send_pause_adaptive_to_router(6000);

   m_iCurrentTestIndex = 0;
   m_iCountSucceededTests = 0;
   m_iCountFailedTests = 0;
   m_iCountFailedTestsDatarates = 0;

   m_bSkipRateTests = false;
   if ( m_bSkipRateTests )
   {
      memcpy(&m_RadioRuntimeCapabilitiesToApply, &g_pCurrentModel->radioRuntimeCapabilities, sizeof(type_radio_runtime_capabilities_parameters));
      for( int i=m_iIndexFirstDatarateLegacyTest; i<=m_iIndexLastDatarateLegacyTest; i++ )
      {
         m_TestsInfo[i].fComputedQualityMin = m_RadioRuntimeCapabilitiesToApply.fQualitiesLegacy[0][i-m_iIndexFirstDatarateLegacyTest];
         m_TestsInfo[i].fComputedQualityMax = m_RadioRuntimeCapabilitiesToApply.fQualitiesLegacy[0][i-m_iIndexFirstDatarateLegacyTest];
      }
      for( int i=m_iIndexFirstDatarateMCSTest; i<=m_iIndexLastDatarateMCSTest; i++ )
      {
         m_TestsInfo[i].fComputedQualityMin = m_RadioRuntimeCapabilitiesToApply.fQualitiesMCS[0][i-m_iIndexFirstDatarateMCSTest];
         m_TestsInfo[i].fComputedQualityMax = m_RadioRuntimeCapabilitiesToApply.fQualitiesMCS[0][i-m_iIndexFirstDatarateMCSTest];
      }

      for( int i=m_iIndexFirstRadioFlagsTest; i<m_iIndexLastRadioFlagsTest; i++ )
      {
         m_TestsInfo[i].bSucceeded = true;
         m_TestsInfo[i].uTimeEnded = g_TimeNow;
         for( int j=0; j<hardware_get_radio_interfaces_count(); j++ )
            m_TestsInfo[i].fQualityCards[j] = m_TestsInfo[i].fComputedQualityMax;
      }

      m_iCountFailedTests = 0;
      m_iCountFailedTestsDatarates = 0;
      m_iCountSucceededTests = m_iIndexLastDatarateMCSTest+1;
      m_iCurrentTestIndex = m_iIndexFirstRadioPowersTest;
   }
   else
   {
      g_pCurrentModel->resetRadioCapabilitiesRuntime(&m_RadioRuntimeCapabilitiesToApply);
   }

   m_RadioRuntimeCapabilitiesToApply.uSupportedMCSFlags = m_uRadioFlagsToApply;

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

   if ( (iTestIndex >= m_iIndexFirstDatarateLegacyTest) && (iTestIndex <= m_iIndexLastDatarateMCSTest) )
      strcpy(szType, "DataRate");
   else if ( (iTestIndex >= m_iIndexFirstRadioFlagsTest) && (iTestIndex <= m_iIndexLastRadioFlagsTest) )
      strcpy(szType, "RadioFlags");
   else
      strcpy(szType, "TxPowers");
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
   float x = m_RenderXPos+m_sfMenuPaddingX;

   int iIndexBestQuality = -1;
   float fBestQ = 0;

   for( int iTest=0; iTest<m_iTestsCount; iTest++ )
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
   {
      if ( !hardware_radio_index_is_wifi_radio(iInt) )
         continue;

      float fQualityCard = m_TestsInfo[iTest].fQualityCards[iInt];

      if ( fQualityCard > fBestQ )
      if ( ! m_TestsInfo[iTest].bSkipTest )
      if ( iTest < m_iCurrentTestIndex )
      if ( iTest < m_iIndexFirstRadioPowersTest )
      {
         fBestQ = fQualityCard;
         iIndexBestQuality = iTest;
      }

      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + iTest*wBar, y + iInt*(hBar+hPixel*2.0), wBar-4.0*wPixel, hBar-2.0*hPixel);

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

      if ( (iTest < m_iCurrentTestIndex) && (iTest >= m_iIndexFirstRadioPowersTest) )
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
         g_pRenderEngine->drawRect(x + iTest*wBar + wPixel, y + iInt*(hBar+hPixel*2.0) + hBar - fRectH*hBar - hPixel, wBar - 4.0*wPixel - 2.0*wPixel, fRectH*(hBar-2.0*hPixel));
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

   if ( 0 != m_iUserState )
   {
      log_line("[NegociateRadioLink] Periodic loop, waiting user input, user state: %d", m_iUserState);
      static u32 s_uLastAdaptivePauseFromNegociateRadio = 0;
      if ( g_TimeNow > s_uLastAdaptivePauseFromNegociateRadio + 1000 )
      {
         s_uLastAdaptivePauseFromNegociateRadio = g_TimeNow;
         send_pause_adaptive_to_router(4000);
         _send_keep_alive_to_vehicle();
      }

      if ( m_iState == NEGOCIATE_STATE_TEST_RUNNING )
      if ( m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle != 0 )
      {
         _storeCurrentTestDataFromRadioStats();
         _computeQualities();
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
      // Resend test to vehicle
      if ( (g_TimeNow > m_TestsInfo[m_iCurrentTestIndex].uTimeLastSendToVehicle + 60) )
      {
         log_line("[NegociateRadioLink] Did not received start test confirmation from vehicle for test %d, send start again. Test started %u ms ago, test set duration is %u ms, subtest duration is %u ms, last sent start test message was %u ms ago, test is marked as ended? %s",
            m_iCurrentTestIndex,
            g_TimeNow - m_TestsInfo[m_iCurrentTestIndex].uTimeStarted, m_TestsInfo[m_iCurrentTestIndex].uDurationToTest, m_TestsInfo[m_iCurrentTestIndex].uDurationSubTest,
            g_TimeNow - m_TestsInfo[m_iCurrentTestIndex].uTimeLastSendToVehicle,
            (m_TestsInfo[m_iCurrentTestIndex].uTimeEnded != 0)?"yes":"no");
         _send_start_test_to_vehicle(m_iCurrentTestIndex);
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
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION;
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
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION;
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

   if ( -1 != m_MenuIndexCancel )
      _onCancel();

   return 0;
}

void MenuNegociateRadio::onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded)
{
   Menu::onVehicleCommandFinished(uCommandId, uCommandType, bSucceeded);

   if ( uCommandType == COMMAND_ID_SET_VIDEO_PARAMETERS )
   if ( m_iState == NEGOCIATE_STATE_WAIT_VEHICLE_APPLY_VIDEO_SETTINGS_CONFIRMATION )
   {
      log_line("[NegociateRadioLink] Received confirmation from vehicle for set video params.");

      if ( ! (m_uRadioFlagsToApply & RADIO_FLAGS_USE_MCS_DATARATES) )
      {
         m_iUserState = NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION;
         log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> MCS warning wait user", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

         addMessage2(0, L("Radio Link Quality Warning"), L("Your radio links should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."));
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
   bool bFirst = true;
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( !hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioLink )
         continue;
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPackets;
      m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad + g_SM_RadioStats.radio_interfaces[i].totalRxPacketsLost;
      if ( bFirst )
         log_line("[NegociateRadioLink] Store test data from radio stats (now on test %d): rx/(lost/bad) pckts: %d/%d", m_iCurrentTestIndex, m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i], m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i]);
      bFirst = false;
   }
}

void MenuNegociateRadio::_logTestData(int iTestIndex)
{
   char szRx[256];
   char szLost[256];
   
   strcpy(szRx, "[");
   strcpy(szLost, "[");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
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

      if ( !hardware_radio_index_is_wifi_radio(i) )
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

   log_line("[NegociateRadioLink] Test %d (type: %s) (datarate %s) %s, succeded? %s, enough rx data? %s, rx/lost packets: %s/%s",
      iTestIndex, szTestType, str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
      szState,
      m_TestsInfo[iTestIndex].bSucceeded?"yes":"no",
      m_TestsInfo[iTestIndex].bReceivedEnoughData?"yes":"no",
      szRx, szLost);
}

float MenuNegociateRadio::_getMaxComputedQualityForDatarate(int iDatarate, int* pTestIndex)
{
   float fQuality = 0.0;
   for( int iTest=m_iIndexFirstDatarateLegacyTest; iTest<=m_iIndexLastDatarateMCSTest; iTest++ )
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


float MenuNegociateRadio::_getMinComputedQualityForDatarate(int iDatarate, int* pTestIndex)
{
   float fQuality = 1.0;
   for( int iTest=m_iIndexFirstDatarateLegacyTest; iTest<=m_iIndexLastDatarateMCSTest; iTest++ )
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
   *pBuffer = m_iMainVehicleLinkIndexToTest;
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
   u8 uFlagsRuntimeCapab = g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab;
   g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab = uFlagsRuntimeCapab & (~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED);
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
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) + 2*sizeof(int) + sizeof(type_radio_runtime_capabilities_parameters);

   u8 buffer[1024];
   int iDataRates = 0;

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8* pBuffer = &(buffer[sizeof(t_packet_header)]);
   *pBuffer = 0xFF;
   pBuffer++;
   *pBuffer = NEGOCIATE_RADIO_APPLY_PARAMS;
   pBuffer++;
   *pBuffer = m_iMainVehicleLinkIndexToTest;
   pBuffer++;
   memcpy(pBuffer, &iDataRates, sizeof(int));
   pBuffer += sizeof(int);
   memcpy(pBuffer, &m_uRadioFlagsToApply, sizeof(u32));
   pBuffer += sizeof(u32);
   memcpy(pBuffer, &m_iTxPowerMwToApply, sizeof(int));
   pBuffer += sizeof(int);

   memcpy(pBuffer, &m_RadioRuntimeCapabilitiesToApply, sizeof(type_radio_runtime_capabilities_parameters));
   pBuffer += sizeof(type_radio_runtime_capabilities_parameters);

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
   
   m_uLastTimeSentVehicleOperation = g_TimeNow;
   char szRadioFlags[128];
   str_get_radio_frame_flags_description(m_uRadioFlagsToApply, szRadioFlags);
   log_line("[NegociateRadioLink] Sent [apply settings] message to vehicle. Datarate: %s, Radio flags: %s, Tx power: %d, %d bytes",
      str_format_datarate_inline(0), szRadioFlags, m_iTxPowerMwToApply, PH.total_length);
}

void MenuNegociateRadio::_startTest(int iTestIndex)
{
   char szTestType[64];
   _getTestType(m_iCurrentTestIndex, szTestType);

   if ( (iTestIndex >= m_iIndexFirstRadioFlagsTest) && (iTestIndex <= m_iIndexLastRadioFlagsTest) )
      sprintf(m_szStatusMessage3, "(Testing radio flags)");
   else if ( (iTestIndex >= m_iIndexFirstDatarateLegacyTest) && (iTestIndex <= m_iIndexLastDatarateLegacyTest) )
      sprintf(m_szStatusMessage3, "(Testing quality of %s modulation)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstDatarateMCSTest) && (iTestIndex <= m_iIndexLastDatarateMCSTest) )
      sprintf(m_szStatusMessage3, "(Testing quality of %s datarate)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstRadioPowersTest) && (iTestIndex < m_iIndexFirstRadioPowersTestMCS) )
      sprintf(m_szStatusMessage3, "(Testing actual usable radio Tx power for %s datarate)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else if ( (iTestIndex >= m_iIndexFirstRadioPowersTestMCS) && (iTestIndex <= m_iIndexLastRadioPowersTestMCS) )
      sprintf(m_szStatusMessage3, "(Testing actual usable radio Tx power for %s modulation)", str_getDataRateDescriptionAlternative(m_TestsInfo[iTestIndex].iDataRateToTest));
   else
      strcpy(m_szStatusMessage3, "");

   if ( m_TestsInfo[iTestIndex].bHasSubTests )
      log_line("[NegociateRadioLink] Starting test %d, substep %d (type: %s) (current test is %d), for radio link %d, tx power: %d, datarate: %s, radio flags: %s",
          iTestIndex, m_TestsInfo[iTestIndex].iCurrentSubTest, szTestType, m_iCurrentTestIndex,
          m_TestsInfo[iTestIndex].iVehicleRadioLink+1,
          m_TestsInfo[iTestIndex].iTxPowerMwToTest,
          str_format_datarate_inline(m_TestsInfo[iTestIndex].iDataRateToTest),
          str_get_radio_frame_flags_description2(m_TestsInfo[iTestIndex].uRadioFlagsToTest) );
   else
      log_line("[NegociateRadioLink] Starting test %d (type: %s) (current test is %d), for radio link %d, tx power: %d, datarate: %s, radio flags: %s",
          iTestIndex, szTestType, m_iCurrentTestIndex,
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
      _storeCurrentTestDataFromRadioStats();

   _computeQualities();

   m_TestsInfo[m_iCurrentTestIndex].uTimeEnded = g_TimeNow;
   m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( !hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != m_TestsInfo[m_iCurrentTestIndex].iVehicleRadioLink )
         continue;

      if ( m_TestsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] > 40 )
         m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData = true;
   }

   if ( (m_TestsInfo[m_iCurrentTestIndex].uTimeLastConfirmationFromVehicle == 0) || (! m_TestsInfo[m_iCurrentTestIndex].bReceivedEnoughData) )
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = false;
   else
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = true;

   if ( bUpdateTestState && (m_TestsInfo[m_iCurrentTestIndex].fComputedQualityMin < TEST_DATARATE_QUALITY_THRESHOLD) )
      m_TestsInfo[m_iCurrentTestIndex].bSucceeded = false;

   if ( m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
      m_iCountSucceededTests++;
   else
   {
      if ( (m_iCurrentTestIndex < m_iIndexFirstRadioFlagsTest) || (m_iCurrentTestIndex >m_iIndexLastRadioFlagsTest) )
         m_iCountFailedTests++;
      if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest) )
      {
         if ( (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest < 0) && (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest >= -3) )
            m_iCountFailedTestsDatarates++;
         if ( (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest > 0) && (m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest <= getTestDataRatesLegacy()[2]) )
            m_iCountFailedTestsDatarates++;
      }
   }

   _logTestData(m_iCurrentTestIndex);

   m_iState = NEGOCIATE_STATE_TEST_ENDED;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (E)* -> Test ended.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);


   // Failed too many tests? Cancel the negociation.
   if ( m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest )
   if ( (m_iCountFailedTestsDatarates >= MAX_FAILING_RADIO_NEGOCIATE_STEPS) && (m_iCountSucceededTests < MAX_FAILING_RADIO_NEGOCIATE_STEPS/2) )
   {
      log_softerror_and_alarm("[NegociateRadioLink] Failed 6 tests. Aborting operation.");
      log_line("[NegociateRadioLink] Cancel negociate radio with %d failing steps.", MAX_FAILING_RADIO_NEGOCIATE_STEPS);
      addMessage2(0, L("Failed to negociate radio links."), L("You radio links quality is very poor. Please fix the physical radio links quality and try again later."));
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION;
      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_bFailed = true;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (E)* -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   // Increase the next test duration for same datarate if this one failed
   if ( ! m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
   if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest) )
   if ( (m_iCurrentTestIndex % 2) == 0 )
   {
      m_TestsInfo[m_iCurrentTestIndex+1].uDurationToTest *= 2;
      log_line("[NegociateRadioLink] Increased next test duration for same datarate as this one failed.");
   }

   // Mark to skip remaining datarate tests if this one failed and it's a datarate test
   if ( ! m_TestsInfo[m_iCurrentTestIndex].bSucceeded )
   if ( (m_iCurrentTestIndex >= m_iIndexFirstDatarateLegacyTest) && (m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest) )
   if ( (m_iCurrentTestIndex % 2) == 1 )
   {
      log_line("[NegociateRadioLink] Datarate %s test %d failed. Skip remaining datarates tests.", str_format_datarate_inline(m_TestsInfo[m_iCurrentTestIndex].iDataRateToTest), m_iCurrentTestIndex);
      if ( m_iCurrentTestIndex <= m_iIndexLastDatarateLegacyTest )
      {
         for( int i=m_iCurrentTestIndex+1; i<=m_iIndexLastDatarateLegacyTest; i++ )
            _mark_test_as_skipped(i);
      }
      else if ( m_iCurrentTestIndex <= m_iIndexLastDatarateMCSTest )
      {
         for( int i=m_iCurrentTestIndex+1; i<=m_iIndexLastDatarateMCSTest; i++ )
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
      _computeQualities();
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

   if ( m_iCurrentTestIndex < m_iIndexFirstRadioPowersTest )
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
      _computeQualities();
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

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[0];
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
   log_line("[NegociateRadioLink] Advancing to next test (current test: %d (%s))...", m_iCurrentTestIndex, _getTestType(m_iCurrentTestIndex));

   m_iCurrentTestIndex++;

   // Finished all tests? Apply changes

   if ( m_iCurrentTestIndex >= m_iTestsCount )
   {
      _onFinishedTests();
      return;
   }

   // Switched to legacy datarates tests? Update radio flags to use and update status message
   if ( m_iCurrentTestIndex == m_iIndexFirstDatarateLegacyTest )
   {
      _compute_radio_flags_to_apply();
      strcpy(m_szStatusMessage2, L("Computing radio links capabilities"));
      log_line("[NegociateRadioLink] Switching to legacy datarates tests.");
   }

   // Switched to MCS radio datarates tests?
   // Update radio flags tests based on radio flags tests results
   if ( m_iCurrentTestIndex == m_iIndexFirstDatarateMCSTest )
   {
      strcpy(m_szStatusMessage2, L("Computing radio links capabilities"));
      log_line("[NegociateRadioLink] Switching to MCS datarates tests.");

      log_line("[NegociateRadioLink] Updated MCS datarates tests radio flags to: %s", str_get_radio_frame_flags_description2(m_uRadioFlagsToApply));
      for( int i=m_iIndexFirstDatarateMCSTest; i<=m_iIndexLastDatarateMCSTest; i++ )
      {
         m_TestsInfo[i].uRadioFlagsToTest = m_uRadioFlagsToApply;
         m_TestsInfo[i].uRadioFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
         m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      }
      for( int i=m_iIndexFirstRadioPowersTestMCS; i<=m_iIndexLastRadioPowersTestMCS; i++ )
      {
         m_TestsInfo[i].uRadioFlagsToTest = m_uRadioFlagsToApply;
         m_TestsInfo[i].uRadioFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
         m_TestsInfo[i].uRadioFlagsToTest |= RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      }
   }

   // Switched to tx powers tests?
   // Update which tx power tests to run
   if ( m_iCurrentTestIndex == m_iIndexFirstRadioPowersTest )
   {
      strcpy(m_szStatusMessage2, L("Testing radio powers"));
      log_line("[NegociateRadioLink] Switching to tx powers tests: %d legacy power tests, %d MCS power tests.",
         m_iIndexFirstRadioPowersTestMCS - m_iIndexFirstRadioPowersTest,
         m_iIndexLastRadioPowersTestMCS-m_iIndexFirstRadioPowersTestMCS+1);
      for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS - m_iIndexFirstRadioPowersTest); i++ )
      {
         if ( m_TestsInfo[m_iIndexFirstDatarateLegacyTest + i].bSkipTest || (m_RadioRuntimeCapabilitiesToApply.fQualitiesLegacy[0][i] < TEST_DATARATE_QUALITY_THRESHOLD) )
         {
            m_TestsInfo[m_iIndexFirstRadioPowersTest + i].bSkipTest = true;
            log_line("[NegociateRadioLink] Mark power test %d (datarate %s) to be skipped due to low legacy datarate quality.", m_iIndexFirstRadioPowersTest + i, str_format_datarate_inline(m_TestsInfo[m_iIndexFirstRadioPowersTest + i].iDataRateToTest));
         }
      }
      for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS-m_iIndexFirstRadioPowersTestMCS); i++ )
      {
         if ( m_TestsInfo[m_iIndexFirstDatarateMCSTest + i].bSkipTest || (m_RadioRuntimeCapabilitiesToApply.fQualitiesMCS[0][i] < TEST_DATARATE_QUALITY_THRESHOLD) )
         {
            m_TestsInfo[m_iIndexFirstRadioPowersTestMCS + i].bSkipTest = true;
            log_line("[NegociateRadioLink] Mark power test %d (datarate %s) to be skipped due to low MCS datarate quality.", m_iIndexFirstRadioPowersTestMCS + i, str_format_datarate_inline(m_TestsInfo[m_iIndexFirstRadioPowersTestMCS + i].iDataRateToTest));
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
   u8 uFlagsRuntimeCapab = g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab;

   if ( m_bUpdated )
   {
      // Apply new settings
      g_pCurrentModel->radioLinksParams.link_radio_flags[0] = m_uRadioFlagsToApply;
      m_RadioRuntimeCapabilitiesToApply.uSupportedMCSFlags = m_uRadioFlagsToApply;
      memcpy(&g_pCurrentModel->radioRuntimeCapabilities, &m_RadioRuntimeCapabilitiesToApply, sizeof(type_radio_runtime_capabilities_parameters));
   }
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
       g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i] = 0;
       g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[i] = 0;
   }
   g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab = uFlagsRuntimeCapab | MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
   g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;

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
   log_line("[NegociateRadioLink] Max supported video bitrate: %u bps for max supported datarates: legacy: %d, MCS: MCS-%d, DR boost: %d",
      uMaxVideoBitrate, g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate, -g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate-1, ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT));
   log_line("[NegociateRadioLink] Current video profile video bitrate: %u bps", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS);

   bool bUpdatedVideoBitrate = false;
   u32 uVideoBitrateToSet = 0;

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

      if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_OPENIPC_GOKE200) && ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_OPENIPC_GOKE210) &&
           ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZERO) && ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_PIZEROW) && ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) != BOARD_TYPE_NONE) )
      if ( (g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate < -4) &&
           (g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate > 36000000) &&
           (g_pCurrentModel->video_link_profiles[i].uTargetVideoBitrateBPS < 9000000 ) )
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

   if ( ! (m_uRadioFlagsToApply & RADIO_FLAGS_USE_MCS_DATARATES) )
   {
      m_iUserState = NEGOCIATE_USER_STATE_WAIT_MCS_CONFIRMATION;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> MCS warning wait user", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);

      addMessage2(0, L("Radio Link Quality Warning"), L("Your radio links should support MCS radio rates but the quality is poor and it was switched to legacy radio rates. Check your hardware or manually switch radio link to MCS radio rates."));
      return;
   }

   m_iState = NEGOCIATE_STATE_SET_RADIO_SETTINGS;
   m_uTimeStartedVehicleOperation = 0;
   m_uLastTimeSentVehicleOperation = 0;
   log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (F) -> Set radio settings.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
}

void MenuNegociateRadio::_computeQualities()
{
   m_RadioRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate = 0;
   m_RadioRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate = 0;

   // Store/update supported datarates

   for( int iTest=0; iTest<=m_iCurrentTestIndex; iTest++ )
   {
      m_TestsInfo[iTest].fComputedQualityMax = -1.0;
      m_TestsInfo[iTest].fComputedQualityMin = -1.0;
      if ( m_TestsInfo[iTest].bSkipTest )
         continue;
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
      {
         if ( !hardware_radio_index_is_wifi_radio(iInt) )
            continue;
         if ( g_SM_RadioStats.radio_interfaces[iInt].assignedVehicleRadioLinkId != m_TestsInfo[iTest].iVehicleRadioLink )
            continue;
         m_TestsInfo[iTest].fQualityCards[iInt] = 0.1;
         if ( m_TestsInfo[iTest].iRadioInterfacesRXPackets[iInt] + m_TestsInfo[iTest].iRadioInterfacesRxLostPackets[iInt] > 50 )
         {
            m_TestsInfo[iTest].fQualityCards[iInt]= (float)m_TestsInfo[iTest].iRadioInterfacesRXPackets[iInt]/((float)m_TestsInfo[iTest].iRadioInterfacesRXPackets[iInt] + (float)m_TestsInfo[iTest].iRadioInterfacesRxLostPackets[iInt]);
            if ( (m_TestsInfo[iTest].fComputedQualityMax < 0.0) || (m_TestsInfo[iTest].fQualityCards[iInt] > m_TestsInfo[iTest].fComputedQualityMax) )
              m_TestsInfo[iTest].fComputedQualityMax = m_TestsInfo[iTest].fQualityCards[iInt];
            if ( (m_TestsInfo[iTest].fComputedQualityMin < 0.0) || (m_TestsInfo[iTest].fQualityCards[iInt] < m_TestsInfo[iTest].fComputedQualityMin) )
              m_TestsInfo[iTest].fComputedQualityMin = m_TestsInfo[iTest].fQualityCards[iInt];
         }
         else if ( m_TestsInfo[iTest].iRadioInterfacesRXPackets[iInt] > 0 )
             m_TestsInfo[iTest].fComputedQualityMin = 0.1;
         else
             m_TestsInfo[iTest].fComputedQualityMin = 0.1;
      }
      if ( (iTest >= m_iIndexFirstDatarateLegacyTest) && (iTest <= m_iIndexLastDatarateMCSTest) )
      if ( m_TestsInfo[iTest].fComputedQualityMin >= TEST_DATARATE_QUALITY_THRESHOLD )
      {
         if ( m_TestsInfo[iTest].iDataRateToTest > 0 )
         if ( m_TestsInfo[iTest].iDataRateToTest > m_RadioRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate )
            m_RadioRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate = m_TestsInfo[iTest].iDataRateToTest;
         if ( m_TestsInfo[iTest].iDataRateToTest < 0 )
         if ( m_TestsInfo[iTest].iDataRateToTest < m_RadioRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate )
            m_RadioRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate = m_TestsInfo[iTest].iDataRateToTest;
      }
   }

   //------------------------------------------------
   // Store radio data rates qualities

   int iIndex = 0;
   if ( ! m_bSkipRateTests )
   {
      iIndex = 0;
      for( int i=0; i<getTestDataRatesCountLegacy(); i++ )
      {
         int iDataRate = getTestDataRatesLegacy()[i];
         int iTest = 0;
         float fQuality = _getMaxComputedQualityForDatarate(iDataRate, &iTest);
         m_RadioRuntimeCapabilitiesToApply.fQualitiesLegacy[0][iIndex] = fQuality;
         iIndex++;
      }

      iIndex = 0;
      for( int i=0; i<getTestDataRatesCountMCS(); i++ )
      {
         int iDataRate = getTestDataRatesMCS()[i];
         int iTest = 0;
         float fQuality = _getMaxComputedQualityForDatarate(iDataRate, &iTest);
         m_RadioRuntimeCapabilitiesToApply.fQualitiesMCS[0][iIndex] = fQuality;
         iIndex++;
      }
   }

   //----------------------------------------
   // Store tx powers
   
   iIndex = m_iIndexFirstRadioPowersTest;
   for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS-m_iIndexFirstRadioPowersTest); i++ )
   {
      int iPower = -1;
      if ( ! m_TestsInfo[iIndex].bSkipTest )
      if ( m_TestsInfo[iIndex].bSucceeded )
         iPower = m_TestsInfo[iIndex].iTxPowerLastGood;

      m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] = iPower;
      iIndex++;
   }

   iIndex = m_iIndexFirstRadioPowersTestMCS;
   for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS-m_iIndexFirstRadioPowersTestMCS); i++ )
   {
      int iPower = -1;
      if ( ! m_TestsInfo[iIndex].bSkipTest )
      if ( m_TestsInfo[iIndex].bSucceeded )
         iPower = m_TestsInfo[iIndex].iTxPowerLastGood;

      m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] = iPower;
      iIndex++;
   }

   for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES-1; i++ )
   {
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] <= 0 )
      {
         if ( i == 0 )
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][0] = m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][1];
         else if ( ((i+1) <= (m_iIndexFirstRadioPowersTestMCS-m_iIndexFirstRadioPowersTest)) && (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i+1] > 0) )
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] = (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1]+m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i+1])/2;
         else if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1] > 0)
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] = m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1] / 2;
      }
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] <= 0 )
      {
         if ( i == 0 )
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][0] = m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][1];
         else if ( ((i+1) <= (m_iIndexLastRadioPowersTestMCS-m_iIndexFirstRadioPowersTestMCS)) && (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i+1] > 0) )
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] = (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1]+m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i+1])/2;
         else if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1] > 0)
            m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] = m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1] / 2;
      }

      if ( i > 3 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] > 0 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1] > 0 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] > (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1]*12)/10 )
         m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i] = (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i-1]*12)/10;

      if ( i > 3 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] > 0 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1] > 0 )
      if ( m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] > (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1]*12)/10 )
         m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i] = (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i-1]*12)/10;
   }
}

// Returns true if radio flags to apply are different than model
bool MenuNegociateRadio::_compute_radio_flags_to_apply()
{
   float fQualityMCSMin = 1.0;
   float fQualityMCSMax = 0.0;
   if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest].fComputedQualityMin < fQualityMCSMin )
      fQualityMCSMin = m_TestsInfo[m_iIndexFirstRadioFlagsTest].fComputedQualityMin;
   if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest+1].fComputedQualityMin < fQualityMCSMin )
      fQualityMCSMin = m_TestsInfo[m_iIndexFirstRadioFlagsTest+1].fComputedQualityMin;        

   if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest].fComputedQualityMax > fQualityMCSMax )
      fQualityMCSMax = m_TestsInfo[m_iIndexFirstRadioFlagsTest].fComputedQualityMax;
   if ( m_TestsInfo[m_iIndexFirstRadioFlagsTest+1].fComputedQualityMax > fQualityMCSMax )
      fQualityMCSMax = m_TestsInfo[m_iIndexFirstRadioFlagsTest+1].fComputedQualityMax;        

   float fQualitySTBCMin = 1.0;
   if ( m_TestsInfo[m_iTestIndexSTBCV].fComputedQualityMin < fQualitySTBCMin )
      fQualitySTBCMin = m_TestsInfo[m_iTestIndexSTBCV].fComputedQualityMin;
   if ( m_TestsInfo[m_iTestIndexSTBCV+1].fComputedQualityMin < fQualitySTBCMin )
      fQualitySTBCMin = m_TestsInfo[m_iTestIndexSTBCV+1].fComputedQualityMin;        

   float fQualitySTBCMax = 0.0;
   if ( m_TestsInfo[m_iTestIndexSTBCV].fComputedQualityMax > fQualitySTBCMax )
      fQualitySTBCMax = m_TestsInfo[m_iTestIndexSTBCV].fComputedQualityMax;
   if ( m_TestsInfo[m_iTestIndexSTBCV+1].fComputedQualityMax > fQualitySTBCMax )
      fQualitySTBCMax = m_TestsInfo[m_iTestIndexSTBCV+1].fComputedQualityMax;

   float fQualitySTBCLDPCMin = 1.0;
   if ( m_TestsInfo[m_iTestIndexSTBCLDPCV].fComputedQualityMin < fQualitySTBCLDPCMin )
      fQualitySTBCLDPCMin = m_TestsInfo[m_iTestIndexSTBCLDPCV].fComputedQualityMin;
   if ( m_TestsInfo[m_iTestIndexSTBCLDPCV+1].fComputedQualityMin < fQualitySTBCLDPCMin )
      fQualitySTBCLDPCMin = m_TestsInfo[m_iTestIndexSTBCLDPCV+1].fComputedQualityMin;        

   float fQualitySTBCLDPCMax = 0.0;
   if ( m_TestsInfo[m_iTestIndexSTBCLDPCV].fComputedQualityMax > fQualitySTBCLDPCMax )
      fQualitySTBCLDPCMax = m_TestsInfo[m_iTestIndexSTBCLDPCV].fComputedQualityMax;
   if ( m_TestsInfo[m_iTestIndexSTBCLDPCV+1].fComputedQualityMax > fQualitySTBCLDPCMax )
      fQualitySTBCLDPCMax = m_TestsInfo[m_iTestIndexSTBCLDPCV+1].fComputedQualityMax;


   log_line("[NegociateRadioLink] Compute radio flags: MCS min/max quality: %.3f / %.3f", fQualityMCSMin, fQualityMCSMax);
   log_line("[NegociateRadioLink] Compute radio flags: MCS STBC min/max quality: %.3f / %.3f", fQualitySTBCMin, fQualitySTBCMax);
   log_line("[NegociateRadioLink] Compute radio flags: MCS STBC-LDPC min/max quality: %.3f / %.3f", fQualitySTBCLDPCMin, fQualitySTBCLDPCMax);

   bool bUpdatedRadioFlags = false;

   if ( m_uRadioFlagsToApply & RADIO_FLAGS_USE_LEGACY_DATARATES )
   if ( !(g_pCurrentModel->radioLinksParams.link_radio_flags[0] & RADIO_FLAGS_USE_LEGACY_DATARATES) )
   {
      log_line("[NegociateRadioLink] Compute radio flags: Model will switch from MCS to legacy data rates.");
      bUpdatedRadioFlags = true;
   }

   if ( m_uRadioFlagsToApply & RADIO_FLAGS_USE_MCS_DATARATES )
   if ( !(g_pCurrentModel->radioLinksParams.link_radio_flags[0] & RADIO_FLAGS_USE_MCS_DATARATES) )
   {
      log_line("[NegociateRadioLink] Compute radio flags: Model will switch from legacy to MCS data rates.");
      bUpdatedRadioFlags = true;
   }

   if ( (m_uRadioFlagsToApply & (RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_USE_LEGACY_DATARATES)) ==
        (g_pCurrentModel->radioLinksParams.link_radio_flags[0] & (RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_USE_LEGACY_DATARATES)) )
      log_line("[NegociateRadioLink] Compute radio flags: Model will not switch legacy or MCS data rates. Keep using %s data rates.", (g_pCurrentModel->radioLinksParams.link_radio_flags[0] & RADIO_FLAGS_USE_MCS_DATARATES)?"MCS":"legacy");

   if ( m_uRadioFlagsToApply & RADIO_FLAGS_USE_MCS_DATARATES )
   {   
      if ( fQualitySTBCMin >= fQualityMCSMin*0.95 )
      {
         log_line("[NegociateRadioLink] Compute radio flags: STBC-Vehicle quality greater than no STBC.");
         m_uRadioFlagsToApply |= RADIO_FLAG_STBC_VEHICLE;
      }
      else
      {
         log_line("[NegociateRadioLink] Compute radio flags: STBC-Vehicle quality lower than no STBC.");
         m_uRadioFlagsToApply &= ~RADIO_FLAG_STBC_VEHICLE;
      }

      bool bUseLDPC = false;
      if ( (fQualitySTBCLDPCMin >= fQualitySTBCMin*1.001) && (fQualitySTBCLDPCMin >= fQualityMCSMin*1.001) )
         bUseLDPC = true;
      if ( (fQualitySTBCLDPCMin >= fQualitySTBCMin*1.0001) && (fQualitySTBCLDPCMin >= fQualityMCSMin*1.0001) )
      if ( (fQualitySTBCLDPCMax >= fQualitySTBCMax*1.002) && (fQualitySTBCLDPCMax >= fQualityMCSMax*1.002) )
         bUseLDPC = true;

      if ( bUseLDPC )
      {
         log_line("[NegociateRadioLink] Compute radio flags: STBC-LDPC-Vehicle quality greater than STBC and MCS.");
         m_uRadioFlagsToApply |= RADIO_FLAG_LDPC_VEHICLE | RADIO_FLAG_STBC_VEHICLE;
      }
      else
      {
         log_line("[NegociateRadioLink] Compute radio flags: STBC-LDPC-Vehicle quality lower than STBC or MCS.");
         m_uRadioFlagsToApply &= ~RADIO_FLAG_LDPC_VEHICLE;
      }

      if ( (m_uRadioFlagsToApply & RADIO_FLAG_STBC_VEHICLE) != (g_pCurrentModel->radioLinksParams.link_radio_flags[0] & RADIO_FLAG_STBC_VEHICLE) )
      {
         if ( m_uRadioFlagsToApply & RADIO_FLAG_STBC_VEHICLE )
            log_line("[NegociateRadioLink] Compute radio flags: STBC-Vehicle flag was added. Will apply settings.");
         else
            log_line("[NegociateRadioLink] Compute radio flags: STBC-Vehicle flag was removed. Will apply settings.");
         bUpdatedRadioFlags = true;
      }
      else
         log_line("[NegociateRadioLink] Compute radio flags: STBC-Vehicle flag was not changed.");

      if ( (m_uRadioFlagsToApply & RADIO_FLAG_LDPC_VEHICLE) != (g_pCurrentModel->radioLinksParams.link_radio_flags[0] & RADIO_FLAG_LDPC_VEHICLE) )
      {
         if ( m_uRadioFlagsToApply & RADIO_FLAG_LDPC_VEHICLE )
            log_line("[NegociateRadioLink] Compute radio flags: STBC-LDPC-Vehicle flag was added. Will apply settings.");
         else
            log_line("[NegociateRadioLink] Compute radio flags: STBC-LDPC-Vehicle flag was removed. Will apply settings.");
         bUpdatedRadioFlags = true;
      }
      else
         log_line("[NegociateRadioLink] Compute radio flags: STBC-LDPC-Vehicle flag was not changed.");
   }

   char szFlagsBefore[256];
   char szFlagsAfter[156];
   str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[0], szFlagsBefore);
   if ( m_uRadioFlagsToApply == g_pCurrentModel->radioLinksParams.link_radio_flags[0] )
   {
      bUpdatedRadioFlags = false;
      log_line("[NegociateRadioLink] Compute radio flags: No change in radio flags (now: %s)", szFlagsBefore);
   }
   else
   {
      str_get_radio_frame_flags_description(m_uRadioFlagsToApply, szFlagsAfter);
      log_line("[NegociateRadioLink] Compute radio flags: Radio flags will be updated from model: %s to %s",
         szFlagsBefore, szFlagsAfter);
   }
   return bUpdatedRadioFlags;
}

// Returns true if settings to apply are different than model
bool MenuNegociateRadio::_compute_settings_to_apply()
{
   _computeQualities();

   m_RadioRuntimeCapabilitiesToApply.uFlagsRuntimeCapab = g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab;

   int iTest6 = -1, iTest12 = -1, iTest18 = -1, iTestMCS0Max = -1, iTestMCS1Max = -1, iTestMCS2Max = -1, iTestMCS0Min = -1, iTestMCS1Min = -1, iTestMCS2Min = -1;
   float fQualityMax6 = _getMaxComputedQualityForDatarate(6000000, &iTest6);
   float fQualityMax12 = _getMaxComputedQualityForDatarate(12000000, &iTest12);
   float fQualityMax18 = _getMaxComputedQualityForDatarate(18000000, &iTest18);
   float fQualityMaxMCS0 = _getMaxComputedQualityForDatarate(-1, &iTestMCS0Max);
   float fQualityMaxMCS1 = _getMaxComputedQualityForDatarate(-2, &iTestMCS1Max);
   float fQualityMaxMCS2 = _getMaxComputedQualityForDatarate(-3, &iTestMCS2Max);
   
   float fQualityMin6 = _getMinComputedQualityForDatarate(6000000, &iTest6);
   float fQualityMin12 = _getMinComputedQualityForDatarate(12000000, &iTest12);
   float fQualityMin18 = _getMinComputedQualityForDatarate(18000000, &iTest18);
   float fQualityMinMCS0 = _getMinComputedQualityForDatarate(-1, &iTestMCS0Min);
   float fQualityMinMCS1 = _getMinComputedQualityForDatarate(-2, &iTestMCS1Min);
   float fQualityMinMCS2 = _getMinComputedQualityForDatarate(-3, &iTestMCS2Min);

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
   for( int i=0; i<iCountRatesLegacy; i++ )
   for( int k=0; k<2; k++ )
   {
      int iTestIndex = i*2 + k + m_iIndexFirstDatarateLegacyTest;
      char szTmp[64];
      sprintf(szTmp, "[%d: (%.3f-%.3f) %s]",
          getTestDataRatesLegacy()[i], m_TestsInfo[iTestIndex].fComputedQualityMin, m_TestsInfo[iTestIndex].fComputedQualityMax, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"));
      if ( i != 0 )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
   }   
   log_line("[NegociateRadioLink] Compute settings to apply: Test rates results (legacy): %s", szBuff);

   szBuff[0] = 0;
   for( int i=0; i<getTestDataRatesCountMCS(); i++ )
   for( int k=0; k<2; k++ )
   {
      int iTestIndex = i*2 + k + m_iIndexFirstDatarateMCSTest;
      char szTmp[64];
      sprintf(szTmp, "[%d: (%.3f-%.3f) %s]",
          -getTestDataRatesMCS()[i]-1, m_TestsInfo[iTestIndex].fComputedQualityMin, m_TestsInfo[iTestIndex].fComputedQualityMax, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"));

      if ( i != 0 )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
   }   
   log_line("[NegociateRadioLink] Compute settings to apply: Test rates results (MCS): %s", szBuff);

   log_line("[NegociateRadioLink] Computed max usable rates: legacy: %d, MCS: MCS-%d", m_RadioRuntimeCapabilitiesToApply.iMaxSupportedLegacyDataRate, -m_RadioRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate-1);
   
   szBuff[0] = 0;
   int iTestIndex = m_iIndexFirstRadioPowersTest;
   for( int i=0; i<(m_iIndexFirstRadioPowersTestMCS-m_iIndexFirstRadioPowersTest); i++ )
   {
      if ( i >= getTestDataRatesCountLegacy() )
         break;
      char szTmp[64];
      sprintf(szTmp, "[%d: (%s %d mW)]",
          getTestDataRatesLegacy()[i], m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"), m_TestsInfo[iTestIndex].iTxPowerLastGood);
      if ( i != 0 )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
      iTestIndex++;
   }   
   log_line("[NegociateRadioLink] Compute settings to apply: Test powers results (legacy): %s", szBuff);

   szBuff[0] = 0;
   iTestIndex = m_iIndexFirstRadioPowersTestMCS;
   for( int i=0; i<=(m_iIndexLastRadioPowersTestMCS-m_iIndexFirstRadioPowersTestMCS); i++ )
   {
      if ( i >= getTestDataRatesCountMCS() )
         break;
      char szTmp[64];
      sprintf(szTmp, "[MCS-%d: (%s %d mW)]",
          -getTestDataRatesMCS()[i]-1, m_TestsInfo[iTestIndex].bSkipTest?"skipped":(m_TestsInfo[iTestIndex].bSucceeded?"ok":"failed"), m_TestsInfo[iTestIndex].iTxPowerLastGood);
      if ( i != 0 )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
      iTestIndex++;
   }   
   log_line("[NegociateRadioLink] Compute settings to apply: Test powers results (MCS): %s", szBuff);

   hardware_sleep_ms(5);
   log_line("[NegociateRadioLink] Checkpoint.");
   hardware_sleep_ms(100);
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
   hardware_sleep_ms(100);

   if ( bUseMCSRates && (m_RadioRuntimeCapabilitiesToApply.iMaxSupportedMCSDataRate <= -1) )
   {
      log_line("[NegociateRadioLink] Compute settings: MCS rx quality (min: %.3f max: %.3f) is greater than Legacy rx quality (min: %.3f max: %.3f)", fQualMinMCS, fQualMaxMCS, fQualMinLegacy, fQualMaxLegacy);
      m_uRadioFlagsToApply = RADIO_FLAGS_USE_MCS_DATARATES;
   }
   else
   {
      log_line("[NegociateRadioLink] Compute settings: Legacy rx quality (min: %.3f max: %.3f) is greater than MCS rx quality (min: %.3f max: %.3f)", fQualMinLegacy, fQualMaxLegacy, fQualMinMCS, fQualMaxMCS);
      m_uRadioFlagsToApply = RADIO_FLAGS_USE_LEGACY_DATARATES;
   }

   log_line("[NegociateRadioLink] Checkpoint3.");
   hardware_sleep_ms(100);
   bool bUpdatedRadioFlags = _compute_radio_flags_to_apply();
   if ( (m_uRadioFlagsToApply & (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES)) !=
        (g_pCurrentModel->radioLinksParams.link_radio_flags[0] & (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES)) )
       bUpdatedRadioFlags = true;
   bool bUpdatedDatarates = false;
   bool bUpdatedPowers = false;

   for( int k=0; k<MODEL_MAX_STORED_QUALITIES_LINKS; k++ )
   for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES; i++ )
   {
      if ( (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[k][i] != g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[k][i]) ||
           (m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[k][i] != g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[k][i]) )
      {
         bUpdatedPowers = true;
         log_line("[NegociateRadioLink] Compute settings: Tx powers values are different and must be stored and updated.");
         break;
      }
   }
   log_line("[NegociateRadioLink] Checkpoint4.");
   hardware_sleep_ms(100);

   for( int k=0; k<MODEL_MAX_STORED_QUALITIES_LINKS; k++ )
   for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES; i++ )
   {
      if ( (fabs(m_RadioRuntimeCapabilitiesToApply.fQualitiesLegacy[k][i] - g_pCurrentModel->radioRuntimeCapabilities.fQualitiesLegacy[k][i]) > 0.03) ||
           (fabs(m_RadioRuntimeCapabilitiesToApply.fQualitiesMCS[k][i] - g_pCurrentModel->radioRuntimeCapabilities.fQualitiesMCS[k][i]) > 0.03) )
      {
         bUpdatedDatarates = true;
         log_line("[NegociateRadioLink] Compute settings: Supported datarates values are different and must be stored and updated.");
         break;
      }
   }

   log_line("[NegociateRadioLink] Checkpoint 5.");
   hardware_sleep_ms(100);
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i] != 0) ||
           (g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[i] != 0) )
      {
         bUpdatedDatarates = true;
         log_line("[NegociateRadioLink] Compute settings: Model is using fixed datarates. Must switch it to auto datarates.");
         break;
      }
   }
   log_line("[NegociateRadioLink] Checkpoint 6.");
   hardware_sleep_ms(100);

   if ( (! bUpdatedRadioFlags) && (!bUpdatedDatarates) && (!bUpdatedPowers) )
   {
      log_line("[NegociateRadioLink] Compute settings: No change detected in computed radio links flags or datarates or powers.");
      return false;
   }

   log_line("[NegociateRadioLink] Updates detected for: datarates qualities: %s, radio flags: %s, tx powers: %s",
      bUpdatedDatarates?"yes":"no",
      bUpdatedRadioFlags?"yes":"no",
      bUpdatedPowers?"yes":"no");

   m_uRadioFlagsToApply |= RADIO_FLAGS_FRAME_TYPE_DATA;

   m_iTxPowerMwToApply = 0;

   g_pCurrentModel->validateRadioSettings();

   log_line("[NegociateRadioLink] Will apply datarate: auto");
   log_line("[NegociateRadioLink] Will apply radio flags: (%s)", str_get_radio_frame_flags_description2(m_uRadioFlagsToApply));
   log_line("[NegociateRadioLink] Current model radio link 1 datarate video: %d, radio flags: %s", g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[0], str_get_radio_frame_flags_description2(g_pCurrentModel->radioLinksParams.link_radio_flags[0]));

   for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES; i++ )
      log_line("[NegociateRadioLink] Will store tx power for legacy datarate %d: %d mW", i, m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwLegacy[0][i]);
   for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES; i++ )
      log_line("[NegociateRadioLink] Will store tx power for MCS datarate %d: %d mW", i, m_RadioRuntimeCapabilitiesToApply.iMaxTxPowerMwMCS[0][i]);
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

   log_line("[NegociateRadioLink] Received response from vehicle to test: %d, command: %d, current test state: %d, is flow in canceled state: %s", iRecvTestIndex, uCommand, m_iState, m_bCanceled?"yes":"no");

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

      ControllerSettings* pCS = get_ControllerSettings();
      if ( (NULL != pCS) && pCS->iDeveloperMode )
         _send_revert_flags_to_vehicle();

      m_iState = NEGOCIATE_STATE_END_TESTS;
      m_bCanceled = true;
      m_iCurrentTestIndex = -1;
      log_line("[NegociateRadioLink] State (Test: (%s) %d/%d): (R) User wait cancel -> End all tests.", _getTestType(m_iCurrentTestIndex), m_iCurrentTestIndex, m_iTestsCount-1);
      return;
   }

   if ( m_iUserState == NEGOCIATE_USER_STATE_WAIT_FAILED_CONFIRMATION )
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

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iDeveloperMode )
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

   if ( (-1 != m_MenuIndexCancel) && (m_MenuIndexCancel == m_SelectedIndex) )
   {
      _onCancel();
   }
}
