#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleRCCamera: public Menu
{
   public:
      MenuVehicleRCCamera();
      virtual ~MenuVehicleRCCamera();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      void send_params();

      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexPitch, m_IndexRoll, m_IndexYaw;
      int m_IndexRelative, m_IndexSpeed;
};