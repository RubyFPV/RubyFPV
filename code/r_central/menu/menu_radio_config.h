#pragma once
#include "menu_objects.h"
#include "../popup.h"

class MenuRadioConfig: public Menu
{
   public:
      MenuRadioConfig();
      virtual ~MenuRadioConfig();
      virtual void Render();
      virtual void onShow();
      virtual void onMoveUp(bool bIgnoreReversion);
      virtual void onMoveDown(bool bIgnoreReversion);
      virtual void onMoveLeft(bool bIgnoreReversion);
      virtual void onMoveRight(bool bIgnoreReversion);
      virtual void onFocusedItemChanged();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);

      virtual void onReturnFromChild(int iChildMenuId, int returnValue);

      virtual void onSelectItem();

      bool m_bGoToFirstRadioLinkOnShow;

   protected:
      void showProgressInfo();
      void hideProgressInfo();
      void setTooltip(int iItemIndex, const char* szTooltip);
      void setTooltipText();

      void computeMenuItems();

      void onClickAutoTx(int iRadioLink);
      void onSelectedPreferredTxCard(int iVehicleRadioLink);

      float computeHeights();
      void computeMaxItemIndexAndCommands();

      float drawRadioHeader(float xStart, float xEnd, float yStart);
      float drawRadioLinks(float xStart, float xEnd, float yStart);
      float drawOneRadioLink(float xStart, float xEnd, float yStart, int iVehicleRadioLink);
      void  drawOneRadioLinkCapabilities(float xStart, float xEnd, float yStart, int iVehicleRadioLink, bool bIsLinkActive, bool bIsRelayLink);
      float drawRadioInterfaceController(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface);

      float drawRadioInterfaceCtrlInfo(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface, bool bSelected);

      int m_iIdFontRegular;
      int m_iIdFontSmall;
      int m_iIdFontLarge;
      Popup* m_pPopupProgress;
      MenuItemSelect* m_pItemSelectTxCard;
      int m_iTxCardSelectedIndex;
      int m_iCurrentRadioLink;
      float m_fHeaderHeight;
      float m_fYPosAutoTx[MAX_RADIO_INTERFACES];
      float m_fYPosCtrlRadioInterfaces[MAX_RADIO_INTERFACES];
      float m_fYPosVehicleRadioInterfaces[MAX_RADIO_INTERFACES];
      float m_fYPosRadioLinks[MAX_RADIO_INTERFACES];
      float m_fFooterHeight;

      u32  m_uCommandsIds[50];
      char m_szTooltips[50][256];
      char m_szCurrentTooltip[256];

      bool m_bShowOnlyControllerUnusedInterfaces;

      u32 m_uBandsSiKVehicle;
      u32 m_uBandsSiKController;
      int m_iCountVehicleRadioLinks;

      bool m_bHasSwapInterfacesCommand;
      bool m_bHasSwitchRadioLinkCommand[MAX_RADIO_INTERFACES];
      bool m_bHasDiagnoseRadioLinkCommand[MAX_RADIO_INTERFACES];
      bool m_bHasRotateRadioLinksOrderCommand;

      bool m_bComputedHeights;

      int m_iIndexCurrentItem;
      int m_iIndexMaxItem;
      int m_iHeaderItemsCount;

      float m_fHeightLinks[MAX_RADIO_INTERFACES];
      float m_fTotalHeightRadioConfig;

      bool m_bTmpHasVideoStreamsEnabled;
      bool m_bTmpHasDataStreamsEnabled;
};
