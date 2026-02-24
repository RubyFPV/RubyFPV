#pragma once
#include "../../base/ctrl_interfaces.h"

#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../popup.h"

class MenuVehicleRC: public Menu
{
   public:
      MenuVehicleRC();
      virtual ~MenuVehicleRC();
      virtual void onShow();
      virtual int onBack();
      virtual bool periodicLoop();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      void updateUIState(bool bEnable);
      void renderLiveValues();
      void onAssignButton(int buttonIndex);
      void onAssignAxe(int axeIndex);

      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[10];

      int m_CurrentRCValues[MAX_RC_CHANNELS];
      u32 m_TimeLastRCCompute;
      u32 m_TimeLastJoystickCheck;
      int m_nIndexPrimaryHID;
      t_ControllerInputInterface* m_pJoystick;
      u32 m_uLastSBUSFrameTime;
      u8  m_uLastSBUSFrameIndex;
      bool m_bPendingEnable;
      int m_iI2CRCINAddress;
      bool m_bSBUSInverted;

      int m_iQA1Org;
      int m_iQA2Org;
      int m_iQA3Org;

      int m_ButtonsBeforeAssign[MAX_JOYSTICK_BUTTONS];
      int m_AxesBeforeAssign[MAX_JOYSTICK_BUTTONS];
      bool m_bWaitingForInput;
      Popup* m_pPopupAssignment;

      int m_IndexRCEnabled;
      int m_IndexEnableOutput;
      int m_IndexRCInputType;
      int m_IndexHIDPrimary;
      int m_IndexRCFrames;
      int m_IndexChannelsCount;
      int m_IndexChannels;
      int m_IndexThrotleReverse;
      int m_IndexExpo;
      int m_IndexRCCamera;
      int m_IndexFailsafeTime;
      int m_IndexFailsafeType;
      int m_IndexFailsafeValues;
      int m_IndexRCTranslation;
};