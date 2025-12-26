#pragma once

#include "menu.h"
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuControllerJoystick: public Menu
{
   public:
      MenuControllerJoystick(int joystickIndex);
      virtual ~MenuControllerJoystick();
      virtual void onShow(); 
      virtual bool periodicLoop();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();

      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange* m_pItemsRange[14];

      int m_JoystickIndex;
      t_ControllerInputInterface* m_pJoystick;

      int m_IndexBack;
      int m_IndexCalibrate;
      int m_IndexCenterBand;

      bool m_bIsCalibratingCenters;
      bool m_bIsCalibratingAxes;
      bool m_bCalibrationComplete;
      bool m_bCalibrationCanceled;
      float m_fTopTextHeight;

      Popup* m_pPopupCal;
      int axesCalMinValue[MAX_JOYSTICK_AXES];
      int axesCalMaxValue[MAX_JOYSTICK_AXES];
      int axesCalCenterValue[MAX_JOYSTICK_AXES];
      int buttonsReleasedValue[MAX_JOYSTICK_BUTTONS];

};