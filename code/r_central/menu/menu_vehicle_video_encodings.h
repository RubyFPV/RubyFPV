#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleVideoEncodings: public Menu
{
   public:
      MenuVehicleVideoEncodings();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();
      
   private:
      void addItems();
      void checkAddWarningInMenu();
      bool sendVideoParams();

      int m_IndexHigherRates, m_IndexVideoBitrate;
      int m_IndexFocusMode;
      int m_IndexPacketSize, m_IndexBlockPackets, m_IndexBlockECRate, m_IndexECSchemeSpread;
      int m_IndexAutoKeyframe, m_IndexMaxKeyFrame, m_IndexKeyframeManual;
      int m_IndexH264Profile, m_IndexH264Level, m_IndexH264Refresh;
      int m_IndexRemoveH264PPS, m_IndexInsertH264PPS, m_IndexInsertH264SPSTimings;
      int m_IndexH264Slices;
      int m_IndexIPQuantizationDelta;
      int m_IndexCustomQuant, m_IndexQuantValue;
      int m_IndexEnableAdaptiveQuantization;
      int m_IndexAdaptiveH264QuantizationStrength;
      int m_IndexHDMIOutput;
      int m_IndexNoise;

      //bool m_ShowBitrateWarning;
      MenuItemSlider* m_pItemsSlider[25];
      MenuItemSelect* m_pItemsSelect[40];
      MenuItem* m_pMenuItemVideoWarning;
      MenuItem* m_pMenuItemVideoKeyframeWarning;
};