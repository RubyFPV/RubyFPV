#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRadioLinkELRS: public Menu
{
   public:
      MenuVehicleRadioLinkELRS(int iRadioLink);
      virtual ~MenuVehicleRadioLinkELRS();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();

   private:
      void sendRadioLinkFlags(int linkIndex);

      int m_iRadioLink;

      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      bool m_bHasValidInterface;
      int m_IndexEnabled;
      int m_IndexAirDataRate;
      int m_IndexAirPacketSize;
};