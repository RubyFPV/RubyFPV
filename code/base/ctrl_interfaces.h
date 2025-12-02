#pragma once
#include "../base/hardware.h"
#include "../base/models.h"

#define CONTROLLER_INTERFACES_STAMP_ID "V.7.2"
#define CONTROLLER_INTERFACES_MAX_LIST_SIZE 100

#define CONTROLLER_MAX_INPUT_INTERFACES 40

#ifdef __cplusplus
extern "C" {
#endif 


typedef struct
{
   char szInterfaceName[MAX_JOYSTICK_INTERFACE_NAME];
   u32 uId;
   u8 countAxes;
   u8 countButtons;
   bool bCalibrated;
   int axesMinValue[MAX_JOYSTICK_AXES];
   int axesMaxValue[MAX_JOYSTICK_AXES];
   int axesCenterValue[MAX_JOYSTICK_AXES];
   int axesCenterZone[MAX_JOYSTICK_AXES]; // as percentage number, in 0.1% increments
   int buttonsReleasedValue[MAX_JOYSTICK_BUTTONS];
   int buttonsMinValue[MAX_JOYSTICK_BUTTONS];
   int buttonsMaxValue[MAX_JOYSTICK_BUTTONS];

   // Not persistent
   int currentHardwareIndex;
}  t_ControllerInputInterface;


typedef struct
{
   int cardModel; // 0 or positive - autodetected, negative - user set
   char* szUserDefinedName;
   char szMAC[MAX_MAC_LENGTH];
   u32 capabilities_flags;
   //Deprecated in 9.5
   //int datarateBPSMCS; // positive: bps; negative: mcs rate; 0: none
   int iRawPowerLevel;
   int iInternal;
} t_ControllerRadioInterfaceInfo;

typedef struct
{
   t_ControllerRadioInterfaceInfo listRadioInterfaces[CONTROLLER_INTERFACES_MAX_LIST_SIZE];
   int radioInterfacesCount;

   char listMACTXPreferredOrdered[CONTROLLER_INTERFACES_MAX_LIST_SIZE][MAX_MAC_LENGTH];
   int listMACTXPreferredCount;

   t_ControllerInputInterface inputInterfaces[CONTROLLER_MAX_INPUT_INTERFACES];
   int inputInterfacesCount;

} ControllerInterfacesSettings;

int save_ControllerInterfacesSettings();
int load_ControllerInterfacesSettings();
void reset_ControllerInterfacesSettings();
ControllerInterfacesSettings* get_ControllerInterfacesSettings();
void controllerRadioInterfacesLogInfo();

t_ControllerRadioInterfaceInfo* controllerGetRadioCardInfo(const char* szMAC);
int controllerIsCardDisabled(const char* szMAC);
int controllerIsCardRXOnly(const char* szMAC);
int controllerIsCardTXOnly(const char* szMAC);
int controllerIsCardTXPreferred(const char* szMAC);

void controllerSetCardDisabled(const char* szMAC);
void controllerRemoveCardDisabled(const char* szMAC);
void controllerSetCardTXRX(const char* szMAC);
void controllerSetCardRXOnly(const char* szMAC);
void controllerSetCardTXOnly(const char* szMAC);
void controllerSetCardTXPreferred(const char* szMAC);
void controllerRemoveCardTXPreferred(const char* szMAC);
int controllerGetTXPreferredIndexForCard(const char* szMAC);

void controllerSetCardInternal(const char* szMAC, bool bInternal);
bool controllerIsCardInternal(const char* szMAC);

bool controllerInterfacesCheckPowerLevels();

int controllerAddedNewRadioInterfaces();

u32 controllerGetCardFlags(const char* szMAC);
void controllerSetCardFlags(const char* szMAC, u32 flags);

void controllerGetCardUserDefinedNameOrType(radio_hw_info_t* pRadioHWInfo, char* szOutput);
void controllerGetCardUserDefinedNameOrShortType(radio_hw_info_t* pRadioHWInfo, char* szOutput);
char* controllerGetCardUserDefinedName(const char* szMAC);
void controllerSetCardUserDefinedName(const char* szMAC, const char* szName);

//int controllerComputeRXTXCards(Model* pModel, int iSearchFreq, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX);

void controllerInterfacesEnumJoysticks();
t_ControllerInputInterface* controllerInterfacesGetAt(int index);

#ifdef __cplusplus
}  
#endif 