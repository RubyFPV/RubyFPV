#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_radio.h"
#include "menu_item_checkbox.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "../../base/video_capture_res.h"

class MenuVehicleVideoBidirectional: public Menu
{
   public:
      MenuVehicleVideoBidirectional();
      virtual ~MenuVehicleVideoBidirectional();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();
            
   private:
      int m_IndexOneWay;
      int m_IndexAdaptiveVideo;
      int m_IndexRetransmissions;
      int m_IndexAdaptiveVideoLevel, m_IndexAdaptiveAlgorithm, m_IndexAdaptiveUseControllerToo;
      int m_IndexVideoLinkLost, m_IndexAdaptiveAdjustmentStrength;
      int m_IndexRetransmissionsFast;
      int m_IndexRetransmissionsGuardInterval;
      int m_IndexLevelRSSI;
      int m_IndexLevelSNR;
      int m_IndexLevelRetr;
      int m_IndexLevelRXLost;
      int m_IndexLevelECUsed;
      int m_IndexLevelECMax;
      int m_IndexLevelTimeToSwitchUp;

      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemRadio* m_pItemsRadio[5];

      int m_iTempAdaptiveStrength;
      u32 m_uTempAdaptiveWeights;
      type_adaptive_metrics m_AdaptiveMetrics;
      MenuItemText* m_pMenuItemAdaptiveTimers;
      MenuItemText* m_pMenuItemTimeToSwitchUp;
      MenuItemText* m_pMenuItemRSSI;
      MenuItemText* m_pMenuItemSNR;
      MenuItemText* m_pMenuItemRetr;
      MenuItemText* m_pMenuItemRxLost;
      MenuItemText* m_pMenuItemECUsed;
      MenuItemText* m_pMenuItemECMax;
      void sendVideoSettings(bool bIsInlineFastChange);
      void _updateDevValues();
};