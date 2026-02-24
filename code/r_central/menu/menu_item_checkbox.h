#pragma once
#include "menu_items.h"

class MenuItemCheckbox: public MenuItem
{
   public:
     MenuItemCheckbox(const char* title);
     MenuItemCheckbox(const char* title, const char* tooltip);
     virtual ~MenuItemCheckbox();

     bool isChecked();
     void setChecked(bool bChecked);
     virtual void beginEdit();
     virtual void endEdit(bool bCanceled);
     virtual void onClick();

     virtual float getTitleWidth(float maxWidth);
     virtual float getValueWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      bool m_bChecked;
};
