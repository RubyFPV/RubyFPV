#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRelay: public Menu
{
   public:
      MenuVehicleRelay();
      virtual ~MenuVehicleRelay();
      virtual void Render();
      virtual void onShow();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onSelectItem();

   protected:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      u32 m_uVehiclesID[MAX_MODELS];
      int m_iCountVehiclesIDs;
      char m_szRelayError[256];
      char m_szRelayError2[256];

      bool m_bCurrentVehicleRelayCapableLinks[MAX_RADIO_INTERFACES];
      int m_iCountCurrentVehicleRelayCapableLinks;

      int m_IndexBack;
      int m_IndexVehicle;
      int m_IndexRadioLink;
      int m_IndexQAButton;
      int m_IndexRelayType;
      int m_IndexOSDSwitch;
      int m_IndexOSDMerge;

      bool m_bIsConfigurable;
      float m_fHeightHeader;

      void _drawHeader(float yPos);
      bool _check_if_vehicle_can_be_relayed(u32 uVehicleId);
      void _onSelectedVehicle(int iVehicleIndex);
};