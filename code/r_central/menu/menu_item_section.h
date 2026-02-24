#pragma once
#include "menu_items.h"

class MenuItemSection: public MenuItem
{
   public:
     MenuItemSection(const char* title);
     MenuItemSection(const char* title, const char* tooltip);
     virtual ~MenuItemSection();

     virtual void setEnabled(bool enabled);
     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

   private:
     float m_fMarginTop;
     float m_fMarginBottom;
};
