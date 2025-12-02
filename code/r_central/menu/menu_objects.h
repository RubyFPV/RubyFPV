#pragma once
#include "menu_items.h"
#include "menu_item_select.h"
#include "../../base/base.h"
#include "../../base/models.h"
#include "../../common/strings_loc.h"

#include "../handle_commands.h"

#define MENU_ID_SIMPLE_MESSAGE 0
#define MENU_ID_ROOT 1
#define MENU_ID_SEARCH 2
#define MENU_ID_SYSTEM 3
#define MENU_ID_CONTROLLER 4
#define MENU_ID_VEHICLES 5
#define MENU_ID_SPECTATOR 6
#define MENU_ID_PREFERENCES 7
#define MENU_ID_STORAGE 11
#define MENU_ID_CONFIRMATION 12

#define MENU_ID_VEHICLE 20
#define MENU_ID_VEHICLE_GENERAL 21
#define MENU_ID_VEHICLE_RADIO 22
#define MENU_ID_VEHICLE_VIDEO 23
#define MENU_ID_VEHICLE_CAMERA 24
#define MENU_ID_VEHICLE_OSD 25
#define MENU_ID_VEHICLE_EXPERT 26
#define MENU_ID_VEHICLE_TELEMETRY 27
#define MENU_ID_VEHICLE_EXPERT_ENCODINGS 28
#define MENU_ID_VEHICLE_RC 29
#define MENU_ID_VEHICLE_RELAY 30
#define MENU_ID_VEHICLE_ALARMS 31
#define MENU_ID_VEHICLE_CAMERA_GAINS 35
#define MENU_ID_CONTROLLER_PERIPHERALS 38
#define MENU_ID_CONTROLLER_EXPERT 39
#define MENU_ID_CONTROLLER_CPU 40
#define MENU_ID_VEHICLE_CPU 42
#define MENU_ID_VEHICLE_RADIO_INTERFACES 44
#define MENU_ID_CONTROLLER_RADIO_INTERFACES 45
#define MENU_ID_OSD_AHI 46
#define MENU_ID_PREFERENCES_UI 47
#define MENU_ID_SYSTEM_EXPERT 48
#define MENU_ID_VEHICLE_MANAGEMENT 49
#define MENU_ID_VEHICLE_IMPORT 50
#define MENU_ID_SWITCH_VEHICLE 55
#define MENU_ID_CONTROLLER_JOYSTICK 56
#define MENU_ID_SYSTEM_ALL_PARAMS 57
#define MENU_ID_VEHICLE_RC_CHANNELS 58
#define MENU_ID_COLOR_PICKER 59
#define MENU_ID_VEHICLE_RC_FAILSAFE 60
#define MENU_ID_VEHICLE_RC_EXPO 61
#define MENU_ID_CONTROLLER_VIDEO 62
#define MENU_ID_CONTROLLER_TELEMETRY 63
#define MENU_ID_UPDATE_VEHICLE_POPUP 64
#define MENU_ID_VEHICLE_RC_CAMERA 65
#define MENU_ID_VEHICLE_RADIO_INTERFACES_EXPERT 67
#define MENU_ID_DEVICE_I2C 68
#define MENU_ID_VEHICLE_OSD_STATS 69
#define MENU_ID_VEHICLE_AUDIO 70
#define MENU_ID_VEHICLE_DATA_LINK 71
#define MENU_ID_CONTROLLER_NETWORK 72
#define MENU_ID_VEHICLE_OSD_PLUGINS 73
#define MENU_ID_VEHICLE_INSTRUMENTS_GENERAL 74
#define MENU_ID_VEHICLE_OSD_ELEMENTS 75
#define MENU_ID_VEHICLE_OSD_PLUGIN 76
#define MENU_ID_CONTROLLER_PLUGINS 77
#define MENU_ID_CONTROLLER_ENCRYPTION 78
#define MENU_ID_SEARCH_CONNECT 79
#define MENU_ID_SYSTEM_HARDWARE 80
#define MENU_ID_VEHICLE_RC_INPUT 81
#define MENU_ID_CONFIRMATION_HDMI 82
#define MENU_ID_CONTROLLER_RECORDING 83
#define MENU_ID_SYSTEM_VIDEO_PROFILES 84
#define MENU_ID_VEHICLE_VIDEO_PROFILE 85
#define MENU_ID_VEHICLE_FUNCTIONS 86
#define MENU_ID_CHANNELS_SELECT 87
#define MENU_ID_INFO_BOOSTER 88
#define MENU_ID_TX_POWER_MAX 89
#define MENU_ID_DEV_LOGS 90
#define MENU_ID_CALIBRATE_HDMI 91
#define MENU_ID_SYSTEM_ALARMS 92
#define MENU_ID_RADIO_CONFIG 93
#define MENU_ID_CONTROLLER_RADIO_INTERFACE 94
#define MENU_ID_SYSTEM_DEV_STATS 95
#define MENU_ID_VEHICLE_RADIO_LINK 96
#define MENU_ID_VEHICLE_RADIO_INTERFACE 97
#define MENU_ID_VEHICLE_MANAGE_PLUGINS 98
#define MENU_ID_VEHICLE_PERIPHERALS 99
#define MENU_ID_VEHICLE_RADIO_LINK_SIK 100
#define MENU_ID_CONTROLLER_RADIO_INTERFACE_SIK 101
#define MENU_ID_VEHICLE_SELECTOR 102
#define MENU_ID_VEHICLE_OSD_WIDGETS 103
#define MENU_ID_CONFIRMATION_DELETE_LOGS 104
#define MENU_ID_VEHICLE_RADIO_CONFIG 105
#define MENU_ID_DIAGNOSE_RADIO_LINK 106
#define MENU_ID_IMPORT_ENC_KEY 107
#define MENU_ID_PREFERENCES_BUTTONS 108
#define MENU_ID_TEXT 110
#define MENU_ID_TX_RAW_POWER 111
#define MENU_ID_VEHICLE_RADIO_LINK_ELRS 112
#define MENU_ID_OSD_PLUGINS 113
#define MENU_ID_VEHICLE_OSD_WIDGET 114
#define MENU_ID_VEHICLE_CPU_OIPC 115
#define MENU_ID_VEHICLE_VIDEO_BIDIRECTIONAL 116
#define MENU_ID_TX_POWER_8812EU 117
#define MENU_ID_VEHICLE_DEV 118
#define MENU_ID_CONTROLLER_DEV 119
#define MENU_ID_VEHICLE_BOARD 120
#define MENU_ID_CONTROLLER_DEV_STATS 121
#define MENU_ID_NEGOCIATE_RADIO 122
#define MENU_ID_ABOUT 123
#define MENU_ID_CONTROLLER_RADIO 124
#define MENU_ID_CONFIRMATION_VIDEO_RATE 125
#define MENU_ID_VEHICLE_SIMPLE_SETUP 126
#define MENU_ID_CONFIRMATION_SDCARD_UPDATE 127
#define MENU_ID_VEHICLE_TELEMETRY_DETECTION 128
#define MENU_ID_CONTROLLER_UPDATE 129
#define MENU_ID_CONTROLLER_UPDATE_NET 130
#define MENU_ID_VEHICLE_RADIO_PIT 131
#define MENU_ID_VEHICLE_RADIO_RUNTIME_CAPS 132
#define MENU_ID_CONTROLLER_CPU_PRIORITIES 133
#define MENU_ID_VEHICLE_CPU_PRIORITIES 134
#define MENU_ID_PROC_INFO 135
#define MENU_ID_VEHICLE_VIDEO_COMPARE 137


#define MAX_MENU_ITEMS 150
#define MAX_MENU_CHANNELS 100

extern float MENU_TEXTLINE_SPACING; // percent of item text height
extern float MENU_ITEM_SPACING;  // percent of item text height

#define MENU_FONT_SIZE_TITLE 0.023
#define MENU_FONT_SIZE_SUBTITLE 0.019
#define MENU_FONT_SIZE_ITEMS 0.021
#define MENU_FONT_SIZE_TOPLINE 0.02
#define MENU_FONT_SIZE_TOOLTIPS 0.018
#define MENU_ROUND_MARGIN 0.3
#define MENU_SELECTION_PADDING 0.006
#define MENU_SEPARATOR_HEIGHT 0.6 // percent of current font height
#define MENU_OUTLINEWIDTH 0.0

#define MENU_MAX_TOP_LINES 40

class Menu
{
   public:
     Menu(int id, const char* title, const char* subTitle);
     virtual ~Menu();

     static int getRenderMode();
     static void setRenderMode(int iMode);

     static float getMenuPaddingX();
     static float getMenuPaddingY();
     static float getSelectionPaddingX();
     static float getSelectionPaddingY();
     static float getScaleFactor();
     
     float m_xPos; float m_yPos;
     float m_Width; float m_Height;
     char m_szTitle[256];
     char m_szSubTitle[256];
     char m_szCurrentTooltip[512];
     char* m_szTopLines[MENU_MAX_TOP_LINES];
     float m_fTopLinesDX[MENU_MAX_TOP_LINES];
     int m_TopLinesCount;
     int m_MenuId;
     int m_MenuDepth;
     float m_fAlfaWhenInBackground;
     bool m_bDisableBackgroundAlpha;
     bool m_bDisableStacking;
     bool m_bIsModal;

     int getId();
     void setId(int iId);
     void setParent(Menu* pParent);
     void invalidate();
     void setModal(bool bModal);
     bool isModal();
     void disableScrolling();
     char* getTitle();
     void setTitle(const char* szTitle);
     void setSubTitle(const char* szSubTitle);
     void removeAllTopLines();
     void addTopLine(const char* szLine, float dx=0.0f);
     void setColumnsCount(int count);
     void enableColumnSelection(bool bEnable);
     void removeAllItems();
     void removeMenuItem(MenuItem* pItem);
     int addMenuItem(MenuItem* item);
     int insertMenuItem(MenuItem* pItem, int iPosition);
     int getSelectedMenuItemIndex();
     void addSeparator();
     int addSection(const char* szSectionName);
     void enableMenuItem(int index, bool enable);
     void setTooltip(const char* szTooltip);
     void setIconId(u32 uIconId);
     u32 getOnShowTime();
     u32 getOnChildAddTime();
     u32 getOnReturnFromChildTime();
     float getMenuFontHeight();
     bool isEditingItem();

     void addExtraHeightAtStart(float fExtraH);
     void addExtraHeightAtEnd(float fExtraH);

     void disableBackAction();
     void enableBackAction();
     bool hasDisabledBackAction();
     
     virtual bool periodicLoop();
     virtual float RenderFrameAndTitle();
     virtual float RenderFrameAndTitleSticky();
     virtual float RenderItem(int index, float yPos, float dx = 0);
     bool didRenderedLastItem();
     virtual void RenderPrepare();
     virtual void Render();
     virtual void RenderEnd(float yTop);
     virtual void onAddToStack();
     virtual void onShow();
     virtual int  onBack();
     virtual void onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded);
     virtual void onSelectItem();
     virtual void onMoveUp(bool bIgnoreReversion);
     virtual void onMoveDown(bool bIgnoreReversion);
     virtual void onMoveLeft(bool bIgnoreReversion);
     virtual void onMoveRight(bool bIgnoreReversion);
     virtual void onFocusedItemChanged();
     virtual void onItemValueChanged(int itemIndex);
     virtual void onItemEndEdit(int itemIndex);
     virtual void onChildMenuAdd(Menu* pChildMenu);
     virtual void onReturnFromChild(int iChildMenuId, int returnValue);
     virtual void valuesToUI(){};

     float getSelectionWidth();
     float getUsableWidth();
     float getRenderWidth();
     float getRenderXPos();
     void resetRenderXPos();
     void startAnimationOnChildMenuAdd();
     void startAnimationOnChildMenuClosed();
     
     void addMessageWithTitle(int iId, const char* szTitle, const char* szMessage);
     void addMessage(const char* szMessage);
     void addMessage(int iId, const char* szMessage);
     void addMessage2(int iId, const char* szMessage, const char* szLine2);
     void addMessageWithTitleAndIcon(int iId, const char* szTitle, const char* szMessage, u32 uIconId);

     static void updateOTAStatus(u8 uOTAStatus, u32 uOTACounter);

   protected:
     void computeRenderSizes();
     float _computeRenderTopHeaderSize();
     float _computeRenderTopPartSize();
     float _computeRenderMaxBottomFooterSize();
     float _getMenuItemTotalRenderHeight(int iMenuItemIndex);
     void _debugDrawBoundingBoxes(float yPos);
     void updateScrollingOnSelectionChange();
     bool checkIsArmed();
     void addMessageNeedsVehcile(const char* szMessage, int iConfirmationId);
     void addUnsupportedMessageOpenIPC(const char* szMessage);
     void addUnsupportedMessageOpenIPCGoke(const char* szMessage);
     void addUnsupportedMessageOpenIPCSigmaster(const char* szMessage);
     bool uploadSoftware();
     bool _generate_upload_archive(char* szArchiveName);
     bool _uploadVehicleUpdate(const char* szArchiveToUpload);
     bool checkCancelUpload();

     MenuItemSelect* createMenuItemCardModelSelector(const char* szTitle);
     void setSelectionForCardModelSelector(MenuItemSelect* pItemSelect, int iCardModel);
     u32 getSelectedCardForCardModelSelector(MenuItemSelect* pItemSelect);

     MenuItemSelect* createMenuItemTxPowers(const char* szTitle, bool bAddAutoOption, bool bBooster2W, bool bBooster4W, int iMaxUsablePowerMw);
     void selectMenuItemTxPowersValue(MenuItemSelect* pMenuItem, bool bHasAutoOption, bool bBooster2W, bool bBooster4W, int* piCardsCurrentPowerLevelsMw, int iNumCards, int iMaxUsablePowerMw);

     static int m_siRenderMode;
     static float m_sfMenuPaddingX;
     static float m_sfMenuPaddingY;
     static float m_sfSelectionPaddingX;
     static float m_sfSelectionPaddingY;
     static float m_sfScaleFactor;
     
     bool m_bInvalidated;
     bool m_bFullWidthSelection;
     bool m_bFirstShow;
     bool m_bRenderedLastItem;
     int  m_ThisRenderCycleStartRenderItemIndex;
     int  m_ThisRenderCycleEndRenderItemIndex;
     Menu* m_pParent;
     MenuItem* m_pMenuItems[MAX_MENU_ITEMS];
     bool m_bHasSeparatorAfter[MAX_MENU_ITEMS];
     int m_ItemsCount;
     int m_iColumnsCount;
     bool m_bEnableColumnSelection;
     bool m_bIsSelectingInsideColumn;
     int m_SelectedIndex;
     float m_fSelectionWidth;
     float m_RenderXPos;
     float m_RenderYPos;
     float m_RenderTotalHeight;
     float m_RenderHeight;
     float m_RenderWidth;
     float m_RenderHeaderHeight;
     float m_RenderTitleHeight;
     float m_RenderSubtitleHeight;
     float m_RenderFooterHeight;
     float m_RenderTooltipHeight;
     float m_RenderMaxFooterHeight;
     float m_RenderMaxTooltipHeight;
     float m_fRenderItemsTotalHeight;
     float m_fRenderItemsVisibleHeight;
     float m_fRenderItemsStartYPos;
     float m_fRenderItemsStopYPos;
     float m_fExtraHeightStart;
     float m_fExtraHeightEnd;
     
     float m_fRenderScrollBarsWidth;
     bool  m_bEnableScrolling;
     bool  m_bHasScrolling;
     int   m_iIndexFirstVisibleItem;
     bool  m_bDisableBackAction;

     u32 m_uIconId;
     float m_fIconSize;

     u32 m_uOnShowTime;
     u32 m_uOnChildAddTime;
     u32 m_uOnChildCloseTime;
     
     u32 m_bIsAnimationInProgress;
     u32 m_uAnimationStartTime;
     u32 m_uAnimationLastStepTime;
     u32 m_uAnimationTotalDuration;
     float m_fAnimationTargetXPos;
     float m_fAnimationStartXPos;
     int m_iLoopCounter;
};
