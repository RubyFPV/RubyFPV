#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_edit.h"
#include "menu_item_slider.h"

class MenuVehicleGeneral: public Menu
{
   public:
      MenuVehicleGeneral();
      virtual ~MenuVehicleGeneral();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onSelectItem();

   private:
      MenuItemEdit* m_pItemEditName;
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemSlider* m_pItemsSlider[10];
      int m_IndexVehicleType;
      int m_IndexBoardType;

      void populate();
      void addTopDescription();
};