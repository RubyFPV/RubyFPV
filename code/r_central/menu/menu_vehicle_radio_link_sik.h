#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRadioLinkSiK: public Menu
{
   public:
      MenuVehicleRadioLinkSiK(int iRadioLink);
      virtual ~MenuVehicleRadioLinkSiK();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();

   private:
      void sendLinkCapabilitiesFlags(int linkIndex);
      void sendRadioLinkFlags(int linkIndex);

      int m_iRadioLink;

      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      u32 m_SupportedChannels[MAX_MENU_CHANNELS];
      int m_SupportedChannelsCount;

      bool m_bHasValidInterface;
      int m_IndexEnabled;
      int m_IndexFrequency;
      int m_IndexUsage;
      int m_IndexDataRate;
      int m_iPacketSize;
      int m_IndexSiKECC;
      int m_IndexSiKLBT;
      int m_IndexSiKMCSTR;
};