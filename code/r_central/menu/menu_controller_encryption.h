#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_edit.h"

class MenuControllerEncryption: public Menu
{
   public:
      MenuControllerEncryption();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void addItems();

      MenuItemEdit* m_pItemPass;
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      int m_IndexChangePass;

      bool m_bHasPassPhrase;
};