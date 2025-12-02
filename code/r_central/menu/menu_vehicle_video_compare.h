#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_radio.h"
#include "menu_item_checkbox.h"

class MenuVehicleVideoCompare: public Menu
{
   public:
      MenuVehicleVideoCompare();
      virtual ~MenuVehicleVideoCompare();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void valuesToUI();
            
   private:
      void addRenderLine(const char* szTitle, const char* szVal1, const char* szVal2, const char* szVal3, const char* szVal4, const char* szVal5 );
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];      
      MenuItemRadio* m_pItemsRadio[10];

      int m_iIndexOk;
      float m_fCurrentYPos;
      int m_iCurrentLine;
};