#pragma once
#include "menu_items.h"

class MenuItemLegend: public MenuItem
{
   public:
     MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth);
     MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth, bool bSmall);
     virtual ~MenuItemLegend();

     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);

     virtual float getValueWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);


   protected:
      bool m_bSmall;
      float m_fMarginX;
      float m_fMaxWidth;
};
