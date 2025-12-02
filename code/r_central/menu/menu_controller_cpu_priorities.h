#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerCPUPriorities: public Menu
{
   public:
      MenuControllerCPUPriorities();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);      
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_iIndexAffinities;
      int m_iIndexPrioritiesAdjustment;
      int m_iIndexPriorityRouter;
      int m_iIndexPriorityRadioRx;
      int m_iIndexPriorityRadioTx;
      int m_iIndexPriorityCentral;
      int m_iIndexPriorityRC;
      int m_iIndexPriorityVideoRx;
      int m_iIndexPriorityVideoRec;
      int m_iIndexPriorityOthers;
      int m_iIndexIONiceRouter, m_iIndexIONiceRxVideo; 

      int m_IndexQuery;
      int m_IndexResetPriorities;

     
};