#pragma once
#include "menu_objects.h"

class MenuSystemAllParams: public Menu
{
   public:
      MenuSystemAllParams();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();

   private:
      float renderVehicleCamera(float xPos, float yPos, float width, float fScale);
      float renderVehicleRC(float xPos, float yPos, float width, float fScale);
      float renderRadioInfo(float xPos, float yPos, float width, float height, float fScale);
      float renderDataRates(float xPos, float yPos, float width, float fScale);
      float renderProcesses(float xPos, float yPos, float width, float fScale);
      float renderSoftware(float xPos, float yPos, float width, float fScale);
      float renderDeveloperFlags(float xPos, float yPos, float width, float fScale);
      float renderControllerParams(float xPos, float yPos, float width, float fScale);
      float renderCPUParams(float xPos, float yPos, float width, float fScale);

      bool m_bGotIP;
      char m_szIP[256];
};