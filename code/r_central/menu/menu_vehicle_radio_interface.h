#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRadioInterface: public Menu
{
   public:
      MenuVehicleRadioInterface(int iRadioInterface);
      virtual ~MenuVehicleRadioInterface();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onSelectItem();

   private:
      void sendInterfaceCapabilitiesFlags(int iInterfaceIndex);
      void sendInterfaceRadioFlags(int iInterfaceIndex);

      int m_iRadioInterface;

      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      int m_IndexCardModel;
      int m_IndexName;
      int m_iIndexBoostMode;
};