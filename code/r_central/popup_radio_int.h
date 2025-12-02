#pragma once
#include "popup.h"

class PopupRadioInterface: public Popup
{
   public:
      PopupRadioInterface(int iVehicleLinkIndex);
      virtual ~PopupRadioInterface();

      virtual void Render();

   protected:
      virtual void computeSize();
      int m_iVehicleRadioLinkIndex;
      int m_iVehicleRadioInterfaceIndex;
      bool m_bCalibrated;
      float m_fHeightGraph;
      int m_iMaxMeasuredTxPowerMw;
};
