#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystem: public Menu
{
   public:
      MenuSystem();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexAlarms;
      int m_IndexAllParams;
      int m_IndexDevices;
      int m_IndexLogs;
      int m_IndexExport;
      int m_IndexImport;
      int m_IndexAutoExport;
      int m_IndexAbout;
      int m_IndexDeveloper;
      int m_IndexDevOptionsVehicle;
      int m_IndexDevOptionsController;
      int m_IndexReset;
};