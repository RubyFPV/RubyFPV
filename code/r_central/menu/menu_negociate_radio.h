#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"

#define MAX_NEGOCIATE_TESTS 100

typedef struct
{
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
   u32 uTimeLastSendToVehicle;
   u32 uTimeLastConfirmationFromVehicle;
   int iCountSendStarts;
   bool bSucceeded;

   int iRadioInterfacesRXPackets[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxLostPackets[MAX_RADIO_INTERFACES];
   bool bReceivedEnoughData;
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
      virtual void valuesToUI();
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
      float _getMaxComputedQualityForDatarate(int iDatarate, int* pTestIndex);
      float _getMinComputedQualityForDatarate(int iDatarate, int* pTestIndex);
      void _computeQualities();
      bool _compute_radio_flags_to_apply();
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

      int m_iMainVehicleLinkIndexToTest;
      int m_iState;
      int m_iUserState;
      bool m_bCanceled;
      bool m_bFailed;
      bool m_bUpdated;

      type_radio_runtime_capabilities_parameters m_RadioRuntimeCapabilitiesToApply;
      video_parameters_t m_VideoParamsToApply;
      type_video_link_profile m_VideoProfilesToApply[MAX_VIDEO_LINK_PROFILES];
      u32 m_uRadioFlagsToApply;
      int m_iTxPowerMwToApply;
      u32 m_uTimeStartedVehicleOperation;
      u32 m_uLastTimeSentVehicleOperation;

      type_negociate_radio_step m_TestsInfo[MAX_NEGOCIATE_TESTS];
      int m_iTestsCount;
      int m_iIndexFirstRadioFlagsTest;
      int m_iIndexLastRadioFlagsTest;
      int m_iIndexFirstDatarateLegacyTest;
      int m_iIndexFirstDatarateMCSTest;
      int m_iIndexLastDatarateLegacyTest;
      int m_iIndexLastDatarateMCSTest;
      int m_iIndexFirstRadioPowersTest;
      int m_iIndexFirstRadioPowersTestMCS;
      int m_iIndexLastRadioPowersTestMCS;
      int m_iTestIndexSTBCV;
      int m_iTestIndexLDPVV;
      int m_iTestIndexSTBCLDPCV;
      int m_iCurrentTestIndex;
      int m_iCountSucceededTests;
      int m_iCountFailedTests;
      int m_iCountFailedTestsDatarates;
};