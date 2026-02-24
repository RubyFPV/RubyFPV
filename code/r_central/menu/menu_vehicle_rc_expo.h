#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_checkbox.h"

#define DISPLAY_CURVE_VALUES 42

typedef struct
{
   MenuItem* pItemTitle;
   MenuItemRange*  pItemValue;

   int m_IndexTitle;
   int m_IndexValue;
} ALIGN_STRUCT_SPEC_INFO t_menu_group_rc_expo;


class MenuVehicleRCExpo: public Menu
{
   public:
      MenuVehicleRCExpo();
      virtual ~MenuVehicleRCExpo();
      virtual void Render();
      virtual void onShow();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      void computeDisplayCurve(int nChannel);

      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      t_menu_group_rc_expo m_ItemsChannels[24];
      int m_ChannelCount;

      int m_iDisplayCurveValues[DISPLAY_CURVE_VALUES];
      int m_IndexComputedChannel;
};