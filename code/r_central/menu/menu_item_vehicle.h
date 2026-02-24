#pragma once
#include "menu_items.h"

class MenuItemVehicle: public MenuItem
{
   public:
     MenuItemVehicle(const char* title);
     MenuItemVehicle(const char* title, const char* tooltip);
     virtual ~MenuItemVehicle();

     void setVehicleIndex(int vehicleIndex, bool bSpectator);

     virtual void beginEdit();
     virtual void endEdit(bool bCanceled);

     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);
     virtual float getValueWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      int m_iVehicleIndex;
      bool m_bIsSpectator;
};
