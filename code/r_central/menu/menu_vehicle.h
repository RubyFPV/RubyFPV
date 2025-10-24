#pragma once
#include "menu_objects.h"


class MenuVehicle: public Menu
{
   public:
      MenuVehicle();
      virtual ~MenuVehicle();
      virtual void Render();
      virtual void onShow();
      virtual bool periodicLoop();
      virtual void onSelectItem();

   protected:    
      u16 m_Flags;
      float m_fIconSize;
      void addTopDescription();

      int m_IndexGeneral;
      int m_IndexCamera, m_IndexVideo, m_IndexWiFiDirect, m_IndexAudio;
      int m_IndexRC;
      int m_IndexOSD;
      int m_IndexTelemetry;
      int m_IndexDataLink;
      int m_IndexManagement;
      int m_IndexFunctions;
      int m_IndexAlarms;
      int m_IndexRelay;
      int m_IndexPeripherals;
      int m_IndexCPU;
      int m_IndexRadio;
      int m_IndexLogType;
      int m_IndexDeveloper;

      bool m_bWaitingForVehicleInfo;
};