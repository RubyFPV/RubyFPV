#pragma once
#include "menu_objects.h"

class MenuSystemHardware: public Menu
{
   public:
      MenuSystemHardware();
      virtual void onShow();
      virtual void Render();
      virtual bool periodicLoop();
      virtual void onSelectItem();

   private:
      float renderVehicleInfo(float xPos, float yPos, float width);
      float renderControllerInfo(float xPos, float yPos, float width);

      int m_nSearchI2CDeviceAddress;
      int m_IndexGetVehicleUSBInfo;
      int m_IndexGetControllerUSBInfo;
};