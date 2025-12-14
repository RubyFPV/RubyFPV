#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuPreferencesUI: public Menu
{
   public:
      MenuPreferencesUI(bool bShowOnlyOSD = false);
      virtual void onShow();     
      virtual void Render();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onSelectItem();
      virtual void valuesToUI();
      
   private:
      void addItems();
      
      bool m_bShowOnlyOSD;
      MenuItemSelect* m_pItemsSelect[25];
      MenuItemSlider* m_pItemsSlider[10];
      int m_IndexScaleMenu, m_IndexMenuStacked;
      int m_IndexMenusCompact;
      int m_IndexOSDSize, m_IndexOSDFlip;
      int m_IndexInvertColors;
      int m_IndexColorPickerOSD;
      int m_IndexColorPickerOSDOutline;
      int m_IndexOSDOutlineThickness;
      int m_IndexColorPickerAHI;
      int m_IndexOSDFont;
      int m_IndexOSDFontBold;
      int m_IndexMonitor;
      int m_IndexUnits;
      int m_IndexUnitsHeight;
      int m_IndexPersistentMessages;
      int m_IndexLogWindow;
      int m_IndexMSPOSDFont;
      int m_IndexMSPOSDSize;
      int m_IndexMSPOSDDeltaX;
      int m_IndexMSPOSDDeltaY;
      int m_IndexLanguage;
};
