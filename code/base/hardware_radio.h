#pragma once
#include "config.h"
#include "hardware_radio_txpower.h"
#include "../radio/radiotap.h"
#include "../radio/radiopackets2.h"

#define MAX_RADIO_HW_PARAMS 16

#define RADIO_TYPE_OTHER 0
#define RADIO_TYPE_RALINK 1
#define RADIO_TYPE_ATHEROS 2
#define RADIO_TYPE_REALTEK 3
#define RADIO_TYPE_MEDIATEK 4
#define RADIO_TYPE_SIK 5
#define RADIO_TYPE_SERIAL 6

#define RADIO_HW_DRIVER_ATHEROS 1       // ath9k_htc
#define RADIO_HW_DRIVER_RALINK 2        // rt2800usb, only 2.4Ghz band
#define RADIO_HW_DRIVER_MEDIATEK 3      // mt7601u
#define RADIO_HW_DRIVER_REALTEK_RTL88XXAU 4       // rtl88xxau
#define RADIO_HW_DRIVER_REALTEK_RTL8812AU 5       // rtl8812au
#define RADIO_HW_DRIVER_REALTEK_8812AU 6          // 8812au
#define RADIO_HW_DRIVER_REALTEK_RTL88X2BU 7       // rtl88x2bu
#define RADIO_HW_DRIVER_SERIAL_SIK 8
#define RADIO_HW_DRIVER_SERIAL 9
#define RADIO_HW_DRIVER_REALTEK_8812EU 10          // 88x2eu
#define RADIO_HW_DRIVER_REALTEK_RTL8814AU 11
#define RADIO_HW_DRIVER_REALTEK_8733BU 15          // 88733bu


// 0 is generic card model
#define CARD_MODEL_TPLINK722N       1
#define CARD_MODEL_ALFA_AWUS036NHA  2
#define CARD_MODEL_ALFA_AWUS036NH   3
#define CARD_MODEL_ALFA_AWUS036ACH  4
#define CARD_MODEL_ASUS_AC56        5
#define CARD_MODEL_BLUE_STICK       6
#define CARD_MODEL_RTL8812AU_DUAL_ANTENNA 7
#define CARD_MODEL_NETGEAR_A6100    8
#define CARD_MODEL_TENDA_U12        9
#define CARD_MODEL_RTL8812AU_AF1   10
#define CARD_MODEL_ZIPRAY 11
#define CARD_MODEL_ARCHER_T2UPLUS 12
#define CARD_MODEL_ALFA_AWUS036ACS  14
#define CARD_MODEL_BLUE_8812EU    15
#define CARD_MODEL_ATHEROS_GENERIC 16
#define CARD_MODEL_RTL8812AU_GENERIC 17
#define CARD_MODEL_RTL8812AU_OIPC_USIGHT 18
#define CARD_MODEL_RTL8812AU_OIPC_USIGHT2 19
#define CARD_MODEL_RTL8733BU    20
#define CARD_MODEL_RTL8814AU      21

#define CARD_MODEL_SIK_RADIO 100
#define CARD_MODEL_SERIAL_RADIO 101
#define CARD_MODEL_SERIAL_RADIO_ELRS 102


#define RADIO_HW_SUPPORTED_BAND_23 1
#define RADIO_HW_SUPPORTED_BAND_24 2
#define RADIO_HW_SUPPORTED_BAND_25 4
#define RADIO_HW_SUPPORTED_BAND_58 8
#define RADIO_HW_SUPPORTED_BAND_433 16
#define RADIO_HW_SUPPORTED_BAND_868 32
#define RADIO_HW_SUPPORTED_BAND_915 64

// Used for radio links and radio interfaces
#define RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ((u32)(((u32)0x01)))
#define RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA  ((u32)(((u32)0x01)<<2))
#define RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY  ((u32)(((u32)0x01)<<4))
#define RADIO_HW_CAPABILITY_FLAG_CAN_RX ((u32)(((u32)0x01)<<5))
#define RADIO_HW_CAPABILITY_FLAG_CAN_TX ((u32)(((u32)0x01)<<6))
#define RADIO_HW_CAPABILITY_FLAG_DISABLED ((u32)(((u32)0x01)<<7))
#define RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY ((u32)(((u32)0x01)<<9))
#define RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK ((u32)(((u32)0x01)<<10))
#define RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_SIK ((u32)(((u32)0x01)<<11))
#define RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS ((u32)(((u32)0x01)<<12))
#define RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W ((u32)(((u32)0x01)<<13))
#define RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W ((u32)(((u32)0x01)<<14))

#define RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD ((u32)(((u32)0x01)))

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   int nChannel;
   int nChannelFlags;
   int nFreq;
   int nDataRateBPSMCS; // positive: bps, negative: mcs rate, 0: never
   int nRadiotapFlags;
   int nAntennaCount;
   int nDbmLast[MAX_RADIO_ANTENNAS];
   int nDbmLastChange[MAX_RADIO_ANTENNAS];
   int nDbmAvg[MAX_RADIO_ANTENNAS];
   int nDbmMin[MAX_RADIO_ANTENNAS];
   int nDbmMax[MAX_RADIO_ANTENNAS];
   int nDbmNoiseLast[MAX_RADIO_ANTENNAS];
   int nDbmNoiseAvg[MAX_RADIO_ANTENNAS];
   int nDbmNoiseMin[MAX_RADIO_ANTENNAS];
   int nDbmNoiseMax[MAX_RADIO_ANTENNAS];
   u32 uLastTimeCapture[MAX_RADIO_ANTENNAS];
} ALIGN_STRUCT_SPEC_INFO type_runtime_radio_rx_info;


void reset_runtime_radio_rx_info(type_runtime_radio_rx_info* pRuntimeRadioRxInfo);
void reset_runtime_radio_rx_info_dbminfo(type_runtime_radio_rx_info* pRuntimeRadioRxInfo);

typedef struct
{
   pcap_t *ppcap;
   int selectable_fd;
   int n80211HeaderLength;
   int nRadioType;
   int nPort;
   int iErrorCount;
   type_runtime_radio_rx_info radioHwRxInfo;
} ALIGN_STRUCT_SPEC_INFO type_runtime_radio_interface_info;


typedef struct
{
   int phy_index;
   int iCardModel;
   int isSupported;
   int isEnabled;
   u8 supportedBands;  // bits 0-3: 2.3, 2.4, 2.5, 5.8 bands
   int isHighCapacityInterface;
   int isSerialRadio;
   int isConfigurable;
   int isTxCapable;
   int iCurrentDataRateBPS;
   u32 uCurrentFrequencyKhz;
   int lastFrequencySetFailed;
   u32 uFailedFrequencyKhz;
   u32 uExtraFlags;
   char szName[32];
   char szDescription[64];
   char szDriver[32];
   char szMAC[MAX_MAC_LENGTH]; // MAC or serial port name or SPI name
   char szProductId[12];
   char szUSBPort[MAX_RADIO_PORT_NAME_LENGTH];  // [A-X], [A-X][1-9], ... or serial port name or SPI name
   int iRadioType;
   int iRadioDriver;
   u32 uHardwareParamsList[MAX_RADIO_HW_PARAMS];
   int openedForRead;
   int openedForWrite;
   type_runtime_radio_interface_info runtimeInterfaceInfoRx;
   type_runtime_radio_interface_info runtimeInterfaceInfoTx;
} ALIGN_STRUCT_SPEC_INFO radio_hw_info_t;


typedef struct
{
   u8 radio_type;
   u8 tx_power;
   u8 tx_powerAtheros;
   u8 tx_powerRTL;
   u8 slot_time;
   u8 thresh62;
   u32 extraInfo; // not used, for future use;
} ALIGN_STRUCT_SPEC_INFO radio_info_wifi_t;


typedef struct
{
   int iBusNb;
   int iPortNb;
   int iDeviceNb;
   char szProductId[32];
   char szName[64];
   int iDriver;
} ALIGN_STRUCT_SPEC_INFO usb_radio_interface_info_t;


void hardware_save_radio_info();
int hardware_load_radio_info();
int hardware_load_radio_info_into_buffers(int* piOutputTotalCount, int* piOutputSupportedCount, radio_hw_info_t* pRadioInfoArray);
void hardware_log_radio_info(radio_hw_info_t* pRadioInfo, int iCount);
void hardware_radio_remove_stored_config();

void hardware_reset_radio_enumerated_flag();
int hardware_enumerate_radio_interfaces();
int hardware_enumerate_radio_interfaces_step(int iStep);
int hardware_radio_get_class_net_adapters_count();

int hardware_load_driver_rtl8812au();
int hardware_load_driver_rtl8812eu();
int hardware_load_driver_rtl8733bu();
int hardware_load_driver_rtl8814au();
int hardware_radio_load_radio_modules(int iEchoToConsole);
int hardware_install_driver_rtl8812au(int iEchoToConsole);
int hardware_install_driver_rtl8812eu(int iEchoToConsole);
int hardware_install_driver_rtl8733bu(int iEchoToConsole);
int hardware_install_driver_rtl8814au(int iEchoToConsole);
void hardware_install_drivers(int iEchoToConsole);
int hardware_initialize_radio_interface(int iInterfaceIndex, u32 uDelayMS);

int hardware_radio_get_driver_id_card_model(int iCardModel);
int hardware_radio_get_driver_id_for_product_id(const char* szProdId);

int hardware_get_radio_interfaces_count();
int hardware_get_supported_radio_interfaces_count();
radio_hw_info_t* hardware_get_radio_info_array();
int hardware_add_radio_interface_info(radio_hw_info_t* pRadioInfo);
int hardware_get_radio_index_by_name(const char* szName);
int hardware_get_radio_index_from_mac(const char* szMAC);
int hardware_radio_has_low_capacity_links();
int hardware_radio_has_rtl8812au_cards();
int hardware_radio_has_rtl8812eu_cards();
int hardware_radio_has_rtl8733bu_cards();
int hardware_radio_has_rtl8814au_cards();
int hardware_radio_has_atheros_cards();
int hardware_radio_driver_is_rtl8812au_card(int iDriver);
int hardware_radio_driver_is_rtl8812eu_card(int iDriver);
int hardware_radio_driver_is_rtl8733bu_card(int iDriver);
int hardware_radio_driver_is_rtl8814au_card(int iDriver);
int hardware_radio_driver_is_atheros_card(int iDriver);

const char* hardware_get_radio_name(int iRadioIndex);
const char* hardware_get_radio_description(int iRadioIndex);

int hardware_radio_type_is_ieee(int iRadioType);
int hardware_radio_type_is_sikradio(int iRadioType);
int hardware_radio_is_wifi_radio(radio_hw_info_t* pRadioInfo);
int hardware_radio_is_serial_radio(radio_hw_info_t* pRadioInfo);
int hardware_radio_is_elrs_radio(radio_hw_info_t* pRadioInfo);
int hardware_radio_is_sik_radio(radio_hw_info_t* pRadioInfo);
int hardware_radio_index_is_high_capacity(int iRadioIndex);
int hardware_radio_index_is_wifi_radio(int iRadioIndex);
int hardware_radio_index_is_serial_radio(int iHWInterfaceIndex);
int hardware_radio_index_is_elrs_radio(int iHWInterfaceIndex);
int hardware_radio_index_is_sik_radio(int iHWInterfaceIndex);

int hardware_radioindex_supports_frequency(int iRadioIndex, u32 freqKhz);
int hardware_radio_supports_frequency(radio_hw_info_t* pRadioInfo, u32 freqKhz);

int hardware_get_radio_count_opened_for_read();
radio_hw_info_t* hardware_get_radio_info(int iRadioIndex);
radio_hw_info_t* hardware_get_radio_info_for_usb_port(const char* szUSBPort);
radio_hw_info_t* hardware_get_radio_info_from_mac(const char* szMAC);

#ifdef __cplusplus
}  
#endif 