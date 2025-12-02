#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleCPUPriorities: public Menu
{
   public:
      MenuVehicleCPUPriorities();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      
   private:
      int m_iIndexAffinities;
      int m_iIndexAffinitiesBalance;
      int m_iIndexAffinitiesVideoCapture;
      int m_iIndexBalanceIntCores;
      int m_iIndexPrioritiesAdjustment;
      int m_iIndexPriorityRouter;
      int m_iIndexPriorityRadioRx;
      int m_iIndexPriorityRadioTx;
      int m_iIndexPriorityVideo;
      int m_iIndexPriorityTelemetry;
      int m_iIndexPriorityRC;
      int m_iIndexPriorityOthers;

      int m_iIndexIONiceRouter, m_iIndexIONiceVideo;
      
      int m_IndexQuery;
      int m_IndexRestart;
      int m_IndexResetPriorities;
      
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      type_processes_priorities m_NewProcPriorities;
};