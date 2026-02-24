#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleOSD: public Menu
{
   public:
      MenuVehicleOSD();
      virtual ~MenuVehicleOSD();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

      void showCompact();

   private:
      void addItems();
      int m_IndexSettings;
      int m_IndexAlarms;
      int m_IndexPlugins;
      int m_IndexOSDScreen;
      int m_IndexOSDLayout;
      int m_IndexOSDEnabled;
      int m_IndexOSDFontSize, m_IndexOSDTransparency, m_IndexBgOnTexts;
      int m_IndexHighlightChangeElements;
      int m_IndexDontShowFCMessages;
      int m_IndexOSDShowMSPOSD;
      int m_IndexOSDElements;
      int m_IndexOSDWidgets;
      int m_IndexOSDPlugins;
      int m_IndexOSDStats;
      //int m_IndexOSDReset;
      int m_IndexOSDController;
      int m_IndexShowFull;

      MenuItemSlider* m_pItemsSlider[3];
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemRange* m_pItemsRange[10];

      bool m_bShowCompact;
};