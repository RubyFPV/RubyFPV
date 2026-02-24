#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_checkbox.h"

typedef struct
{
   MenuItem* pItemTitle;
   MenuItemSelect*  pItemType;
   MenuItemRange*  pItemValue;

   int m_IndexTitle;
   int m_IndexType;
   int m_IndexValue;
} ALIGN_STRUCT_SPEC_INFO t_menu_group_rc_failsafe;


class MenuVehicleRCFailsafe: public Menu
{
   public:
      MenuVehicleRCFailsafe();
      virtual ~MenuVehicleRCFailsafe();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      t_menu_group_rc_failsafe m_ItemsChannels[24];
      int m_ChannelCount;
};