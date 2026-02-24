#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"

#define MAX_NEGOCIATE_TESTS 200

typedef struct
{
   int iVehicleRadioInterface;
   int iVehicleRadioLink;
   int iDataRateToTest;
   u32 uRadioFlagsToTest;
   int iTxPowerMwToTest;
   bool bMustTestUplink;
   bool bSkipTest;

   bool bHasSubTests;
   int iCurrentSubTest;
   u32 uDurationSubTest;

   int iTxPowerMwToTestMin;
   int iTxPowerMwToTestMax;
   int iTxPowerLastGood;

   u32 uTimeStarted;
   u32 uTimeEnded;
   u32 uDurationToTest;
   u32 uExtraStartDelay;
   u32 uTimeLastSendToVehicle;
   u32 uTimeLastConfirmationFromVehicle;
   int iCountSendStarts;
   bool bSucceeded;

   int iRadioInterfacesRXPackets[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxLostPackets[MAX_RADIO_INTERFACES];
   bool bReceivedEnoughData;
   bool bComputedQualities;
   float fQualityCards[MAX_RADIO_INTERFACES];
   float fComputedQualityMin;
   float fComputedQualityMax;
} type_negociate_radio_step;

class MenuNegociateRadio: public Menu
{
   public:
      MenuNegociateRadio();
      virtual ~MenuNegociateRadio();
      virtual void Render();
      virtual int onBack();
      virtual bool periodicLoop();
      virtual void onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded);
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

      void onReceivedVehicleResponse(u8* pPacketData, int iPacketLength);

   private:
      void _reset_tests_and_state();
      void _mark_test_as_skipped(int iTestIndex);
      void _getTestType(int iTestIndex, char* szType);
      char* _getTestType(int iTestIndex);
      void _storeCurrentTestDataFromRadioStats();
      void _logTestData(int iTestIndex);
      float _getMaxComputedQualityForDatarate(int iVehicleRadioInterface, int iDatarate, int* pTestIndex);
      float _getMinComputedQualityForDatarate(int iVehicleRadioInterface, int iDatarate, int* pTestIndex);
      void _computeQualitiesSoFarForCurrentTest();
      bool _compute_radio_flags_to_apply(int iVehicleRadioInterfaceIndex);
      bool _compute_settings_to_apply();

      void _send_keep_alive_to_vehicle();
      void _send_start_test_to_vehicle(int iTestIndex);
      void _send_end_all_tests_to_vehicle(bool bCanceled);
      void _send_revert_flags_to_vehicle();
      void _send_apply_settings_to_vehicle();

      void _startTest(int iTestIndex);
      void _endCurrentTest(bool bUpdateTestState);
      bool _currentTestUpdateWhenRunning();
      bool _updateCurrentMultiTest();
      void _advance_to_next_test();

      void _save_new_settings_to_model();
      void _onFinishedTests();
      void _onCancel();
      void _onFinishedFlow();
      void _close();
      MenuItemSelect* m_pItemsSelect[10];
      int m_MenuIndexCancel;
      char m_szStatusMessage[256];
      char m_szStatusMessage2[256];
      char m_szStatusMessage3[256];
      int m_iLoopCounter;
      u32 m_uShowTime;
      bool m_bSkipRateTests;

      int m_iCountInterfacesToTest;
      bool m_bTestInterfaceIndex[MAX_RADIO_INTERFACES];
      int m_iState;
      int m_iUserState;
      bool m_bCanceled;
      bool m_bFailed;
      bool m_bUpdated;

      type_radio_interfaces_runtime_capabilities_parameters m_RadioInterfacesRuntimeCapabilitiesToApply;
      video_parameters_t m_VideoParamsToApply;
      type_video_link_profile m_VideoProfilesToApply[MAX_VIDEO_LINK_PROFILES];
      u32 m_uRadioLinksTxFlagsToApply[MAX_RADIO_INTERFACES];
      u32 m_uRadioLinksRxFlagsToApply[MAX_RADIO_INTERFACES];
      u32 m_uRadioInterfacesSupportedRadioFlags[MAX_RADIO_INTERFACES];
      u32 m_uTimeStartedVehicleOperation;
      u32 m_uLastTimeSentVehicleOperation;

      type_negociate_radio_step m_TestsInfo[MAX_NEGOCIATE_TESTS];
      int m_iTestsCount;
      int m_iCurrentTestIndex;
      int m_iCurrentTestRadioInterfaceIndex;

      int m_iIndexFirstRadioFlagsTest[MAX_RADIO_INTERFACES];
      int m_iIndexLastRadioFlagsTest[MAX_RADIO_INTERFACES];
      int m_iIndexFirstDatarateLegacyTest[MAX_RADIO_INTERFACES];
      int m_iIndexFirstDatarateMCSTest[MAX_RADIO_INTERFACES];
      int m_iIndexLastDatarateLegacyTest[MAX_RADIO_INTERFACES];
      int m_iIndexLastDatarateMCSTest[MAX_RADIO_INTERFACES];
      int m_iIndexFirstRadioPowersTest[MAX_RADIO_INTERFACES];
      int m_iIndexFirstRadioPowersTestMCS[MAX_RADIO_INTERFACES];
      int m_iIndexLastRadioPowersTestMCS[MAX_RADIO_INTERFACES];
      int m_iIndexFirstRadioInterfaceTest[MAX_RADIO_INTERFACES];
      int m_iIndexLastRadioInterfaceTest[MAX_RADIO_INTERFACES];
      int m_iTestIndexSTBCV[MAX_RADIO_INTERFACES];
      int m_iTestIndexLDPVV[MAX_RADIO_INTERFACES];
      int m_iTestIndexSTBCLDPCV[MAX_RADIO_INTERFACES];
      int m_iCountSucceededTests[MAX_RADIO_INTERFACES];
      int m_iCountFailedTests[MAX_RADIO_INTERFACES];
      int m_iCountFailedTestsDatarates[MAX_RADIO_INTERFACES];
};
