#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

#define COLORPICKER_TYPE_OSD 0
#define COLORPICKER_TYPE_OSD_OUTLINE 1
#define COLORPICKER_TYPE_AHI 2

class MenuColorPicker: public Menu
{
   public:
      MenuColorPicker();
      virtual ~MenuColorPicker();
      virtual void Render();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void valuesToUI();

      int m_ColorType;

   private:
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemRange* m_pItemsRange[10];
      int m_IndexColor[4];
      double m_Color[4];
};