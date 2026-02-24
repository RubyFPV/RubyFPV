#pragma once
#include "menu_objects.h"
#include "menu_item_vehicle.h"

class MenuSpectator: public Menu
{
   public:
      MenuSpectator();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      int m_IndexSelectedVehicle;

};