#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"

#define MAX_MENU_OSD_WIDGETS 30

class MenuVehicleOSDWidgets: public Menu
{
   public:
      MenuVehicleOSDWidgets();
      virtual ~MenuVehicleOSDWidgets();
      virtual void valuesToUI();
      virtual void onShow();
      virtual bool periodicLoop();
      virtual void Render();

      virtual void onSelectItem();

      virtual int onBack();

   private:
      int m_IndexShowSpeedAlt;
      int m_IndexSpeedToSides;
      int m_IndexShowHeading;
      int m_IndexShowAltGraph;

      MenuItemSelect* m_pItemsSelect[MAX_MENU_OSD_WIDGETS+10];
      int m_IndexWidgets[MAX_MENU_OSD_WIDGETS];

};