#pragma once
#include "../../base/hardware.h"
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_edit.h"
#include "menu_item_section.h"
#include "../popup.h"

class MenuControllerRadioInterfaceSiK: public Menu
{
   public:
      MenuControllerRadioInterfaceSiK(int iInterfaceIndex);
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onSelectItem();

   private:
      void showProgressInfo();
      void hideProgressInfo();
      bool checkFlagsConsistency();
      bool setCardFlags();

      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemEdit*   m_pItemsEdit[10];
      Popup* m_pPopupProgress;

      int m_iInterfaceIndex;

      int m_IndexEnabled;
      int m_IndexName;
      int m_IndexInternal;

};