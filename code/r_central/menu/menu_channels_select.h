#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_checkbox.h"

extern u32 s_uChannelsSelect433Band;
extern u32 s_uChannelsSelect868Band;
extern u32 s_uChannelsSelect23Band;
extern u32 s_uChannelsSelect24Band;
extern u32 s_uChannelsSelect25Band;
extern u32 s_uChannelsSelect58Band;

class MenuChannelsSelect: public Menu
{
   public:
      MenuChannelsSelect(u32 uFrequencyBands, int iId);
      virtual ~MenuChannelsSelect();
      virtual void valuesToUI();
      virtual void Render();
      virtual int onBack();
      virtual void onSelectItem();

   private:
      MenuItemSlider* m_pItemsSlider[4];
      MenuItemSelect* m_pItemsSelect[14];
      MenuItemRange* m_pItemsRange[14];

      u32 m_uFrequencyBands;
};