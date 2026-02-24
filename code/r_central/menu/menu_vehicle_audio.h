#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleAudio: public Menu
{
   public:
      MenuVehicleAudio();
      virtual ~MenuVehicleAudio();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void valuesToUI();

   private:
      void addItems();
      void sendParams(bool bOneWay);
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange*  m_pItemsRange[10];

      int m_IndexOIPCMic;
      int m_IndexEnable;
      int m_IndexVolume;
      int m_IndexQuality;

      int m_IndexDevBufferingSize;
      int m_IndexDevPacketLength;
      int m_IndexDevDataPackets;
      int m_IndexDevECPackets;
};