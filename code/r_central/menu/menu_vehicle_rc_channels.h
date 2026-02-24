#pragma once
#include "../../base/hardware.h"
#include "../../base/ctrl_interfaces.h"
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_checkbox.h"

typedef struct
{
   MenuItem* pItemTitle;
   MenuItemRange*  pItemMin;
   MenuItemRange*  pItemMax;
   MenuItemRange*  pItemMid;
   MenuItemCheckbox* pItemReverse;
   MenuItemCheckbox* pItemToggle;
   MenuItem* pItemClear;
   MenuItem* pItemAssign;

   int m_IndexTitle;
   int m_IndexMin;
   int m_IndexMax;
   int m_IndexMid;

   int m_IndexReverse;
   int m_IndexToggle;
   int m_IndexClear;
   int m_IndexAssign;

} ALIGN_STRUCT_SPEC_INFO t_menu_group_rc_channel;


class MenuVehicleRCChannels: public Menu
{
   public:
      MenuVehicleRCChannels();
      virtual ~MenuVehicleRCChannels();
      virtual void Render();
      virtual void onShow();
      virtual int onBack();
      virtual bool periodicLoop();
      virtual void onSelectItem();
      virtual void valuesToUI();

   protected:
      void onClickAssign(int nChannel);
      void onAssignButton(int buttonIndex);
      void onAssignAxe(int axeIndex);

   private:
      void populateRCInfo(rc_parameters_t* pRCInfo);

      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      t_menu_group_rc_channel m_ItemsChannels[24];
      int m_ChannelCount;

      int m_JoystickIndex;
      t_ControllerInputInterface* m_pJoystick;
      int m_CurrentRCValues[MAX_RC_CHANNELS];
      u32 m_TimeLastRCCompute;
      int m_ButtonsBeforeAssign[MAX_JOYSTICK_BUTTONS];
      int m_AxesBeforeAssign[MAX_JOYSTICK_BUTTONS];
      int  m_iCurrentChannelToAssign;
      bool m_bWaitingForInput;
      Popup* m_pPopupAssignment;
      u32 m_TimeLastJoystickCheck;
};