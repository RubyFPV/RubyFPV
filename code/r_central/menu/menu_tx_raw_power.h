#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXRawPower: public Menu
{
   public:
      MenuTXRawPower();
      virtual ~MenuTXRawPower();
      virtual void onShow();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void onSelectItem();

      bool m_bShowVehicleSide;

   protected:
      void addItems();
      void addItemsVehicle();
      void addItemsController();
      MenuItemSelect* createItemCard(bool bVehicle, u32 uBoardType, int iCountLinks, int iRadioLinkIndex, int iCardIndex, int iCardModel, int iRawPower);
      void computeSendPowerToVehicle(int iCardIndex);
      void computeApplyControllerPower(int iCardIndex);

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[50];

      MenuItemSelect* m_pItemSelectVehicleCards[MAX_RADIO_INTERFACES];
      MenuItemSelect* m_pItemSelectVehicleCardsBoostMode[MAX_RADIO_INTERFACES];
      MenuItemSelect* m_pItemSelectControllerCards[MAX_RADIO_INTERFACES];

      int m_iIndexVehicleCards[MAX_RADIO_INTERFACES];
      int m_iIndexVehicleCardsBoostMode[MAX_RADIO_INTERFACES];

      int m_IndexControllerTxPowerMode;
      int m_IndexControllerTxPowerSingle;
      int m_IndexControllerTxPowerRadioLinks[MAX_RADIO_INTERFACES];
};