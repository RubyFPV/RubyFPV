#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"

#define MAX_OSD_CUSTOM_PLUGINS 30

class MenuVehicleOSDPlugins: public Menu
{
   public:
      MenuVehicleOSDPlugins();
      virtual ~MenuVehicleOSDPlugins();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[MAX_OSD_CUSTOM_PLUGINS];
      int m_IndexPlugins[MAX_OSD_CUSTOM_PLUGINS];
};