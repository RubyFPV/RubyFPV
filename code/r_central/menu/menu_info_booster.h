#pragma once
#include "menu_objects.h"


class MenuInfoBooster: public Menu
{
   public:
      MenuInfoBooster();
      virtual ~MenuInfoBooster();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();

      int m_iRadioCardIndex;

   protected:
      u32 m_idImg1;
      u32 m_idImg2;
      u32 m_idImg3;
};