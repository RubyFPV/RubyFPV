#pragma once
#include "../../base/hardware.h"
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_edit.h"
#include "menu_item_section.h"

class MenuControllerRadioInterfaces: public Menu
{
   public:
      MenuControllerRadioInterfaces();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:

      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemEdit*   m_pItemsEdit[20];

      int m_IndexCurrentTXPreferred;
      int m_IndexTXPreferred;
      int m_IndexTXInfo;
      int m_IndexAutoTx;

      int m_IndexStartNics[MAX_RADIO_INTERFACES];
};