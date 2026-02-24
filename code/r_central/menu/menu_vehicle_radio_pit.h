#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRadioLinkPITModes: public Menu
{
   public:
      MenuVehicleRadioLinkPITModes();
      virtual ~MenuVehicleRadioLinkPITModes();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();

   private:
      void addItems();
      void sendRadioInterfacesFlags();

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      int m_IndexEnablePIT;
      int m_IndexPITModeArm;
      int m_IndexPITModeTemp;
      int m_IndexPITModeManual;
      int m_IndexPITModeQAButton;
      int m_iIndexPITTemperature;
};