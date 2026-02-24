#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"

class MenuControllerPlugins: public Menu
{
   public:
      MenuControllerPlugins();
      virtual ~MenuControllerPlugins();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void readPlugins();
      void importFromUSB();

      MenuItemSelect* m_pItemsSelect[50];
      MenuItemSelect* m_pItemsSelectCore[20];
      int m_IndexCorePlugins[20];
      int m_IndexOSDPlugins[MAX_OSD_PLUGINS+2];
      int m_IndexImport;
      int m_IndexSelectedPlugin;

      int m_iCountCorePlugins;
      int m_iCountOSDPlugins;
};