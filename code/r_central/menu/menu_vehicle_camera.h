#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleCamera: public Menu
{
   public:
      MenuVehicleCamera();
      virtual ~MenuVehicleCamera();
      void showCompact();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
            
   private:
      void resetIndexes();
      void addItems();
      void updateUIValues();
      bool canSendLiveUpdates(int iItemIndex);
      void sendCameraParams(int itemIndex, bool bQuick);
      bool binCalibrationFileIsStandardFile(const char* szBinCalibrationFile);
      bool binCalibrationFileIsUserFile(const char* szBinCalibrationFile);
      void populateUserCalibrationFiles();
      void updateCalibrationFileSelection();
      void importCalibrationFiles();
      void onCalibrationFileSelectionChanged();
      void uploadCalibrationFile(int iType, const char* szCalibrationFile);

      MenuItemSlider* m_pItemsSlider[25];
      MenuItemSelect* m_pItemsSelect[25];
      MenuItemRange* m_pItemsRange[25];

      int m_iLastCameraType;
      int m_IndexCamera;
      int m_IndexForceMode;
      int m_IndexProfile;
      int m_IndexBrightness, m_IndexContrast, m_IndexSaturation, m_IndexSharpness;
      int m_IndexHue;
      int m_IndexEV, m_IndexEVValue;
      int m_IndexAGC;
      int m_IndexExposureMode, m_IndexExposureValue, m_IndexWhiteBalance;
      int m_IndexWBTemp;
      int m_IndexAnalogGains;
      int m_IndexMetering, m_IndexDRC;
      int m_IndexISO, m_IndexISOValue;
      int m_IndexShutterMode, m_IndexShutterValue;
      int m_IndexWDR;
      int m_IndexDayNight;
      int m_IndexVideoStab, m_IndexFlip, m_IndexReset;
      int m_IndexIRCut, m_IndexOpenIPCDayNight;
      int m_IndexOpenIPC3A;
      int m_IndexCalibrateHDMI;
      int m_IndexOpenIPCBinProfile;
      int m_IndexShowFull;

      bool m_bShowCompact;
      bool m_bDidAnyLiveUpdates;

      int m_iCountStandardCalibrationFiles;
      int m_iCountUserCalibrationFiles;
      int m_iSelectionIndexUnknownCalibrationFile;
      int m_iSelectionIndexImportCalibrationFiles;
};