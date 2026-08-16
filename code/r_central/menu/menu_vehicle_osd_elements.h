#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_checkbox.h"

class MenuVehicleOSDElements: public Menu
{
   public:
      MenuVehicleOSDElements();
      virtual ~MenuVehicleOSDElements();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
            
   private:
      int m_IndexOSDOrientation, m_IndexShowBg;
      int m_IndexDistance, m_IndexAltitude, m_IndexSpeed, m_IndexHome, m_IndexHomeInvert, m_IndexHomeRotate;
      int m_IndexVoltage, m_IndexVoltagePerCell, m_IndexShowGPSInfo, m_IndexShowGPSPos;
      int m_IndexTotalDistance, m_IndexShowDetailedBPS;
      int m_IndexVideo, m_IndexVideoExtended, m_IndexMode, m_IndexTime;
      int m_IndexThrottle, m_IndexPitch, m_IndexCPU;
      int m_IndexHIDOSD, m_IndexRadioLinkBars, m_IndexRCRSSI;
      int m_IndexGrid;
      int m_IndexShowTxPower;
      int m_IndexRadioLinks, m_IndexRadioInterfaces, m_IndexSignalBars, m_IndexSignalBarsPosition;
      int m_IndexLinkWarningOutline;
      int m_IndexWind, m_IndexTemperature;
      int m_IndexControllerCPU;
      int m_IndexControllerVoltage;
      int m_IndexRadioLinksNumbers;
      
      MenuItemSlider* m_pItemsSlider[50];
      MenuItemSelect* m_pItemsSelect[50];
      MenuItemRange* m_pItemsRange[50];
      MenuItemCheckbox* m_pItemsCheckboxes[10];
};