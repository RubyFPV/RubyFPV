#pragma once

#define RADIO_TX_MESSAGE_QUEUE_ID 117

#define	MAX_RADIO_INTERFACES 5
#define MAX_RADIO_ANTENNAS 4
#define MAX_MAC_LENGTH 20
#define MAX_RADIO_PORT_NAME_LENGTH 6
#define MIN_BOOST_INPUT_SIGNAL 3
#define MAX_BOOST_INPUT_SIGNAL 100

#define MAX_TX_POWER 71
#define MAX_MCS_INDEX 9

#define DEFAULT_TX_POWER_MW_NEGOCIATE_RADIO 5
#define DEFAULT_TX_TIME_OVERLOAD 800 // milisec

// Frequencies are in kHz
#define DEFAULT_FREQUENCY_433  443000
#define DEFAULT_FREQUENCY_868  867000
#define DEFAULT_FREQUENCY_915  914000
#define DEFAULT_FREQUENCY     2472000
#define DEFAULT_FREQUENCY_2   2467000
#define DEFAULT_FREQUENCY_3   2437000
#define DEFAULT_FREQUENCY58   5825000
#define DEFAULT_FREQUENCY58_2 5805000
#define DEFAULT_FREQUENCY58_3 5745000

#define DEFAULT_RADIO_FRAMES_FLAGS (RADIO_FLAGS_USE_MCS_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA)

#define DEFAULT_RADIO_SIK_NETID 27
#define DEFAULT_RADIO_SIK_CHANNELS 5
#define DEFAULT_RADIO_SIK_FREQ_SPREAD 1000 // in kbps
// Default sik packet size is big enough to capture the full t_packet_header in the first sik short packet received for a message
#define DEFAULT_SIK_PACKET_SIZE 24

#define DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE 24
#define DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE 10
#define DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE 127
#define DEFAULT_RADIO_SERIAL_MAX_TX_LOAD 75 // in percentages

// in bps, 0: auto, -100: lowest, positive: legacy, negative: MCS
#define DEFAULT_RADIO_DATARATE_VIDEO 0
#define DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS 18000000
#define DEFAULT_RADIO_DATARATE_DATA 0
#define DEFAULT_RADIO_DATARATE_SERIAL_AIR 4000
#define DEFAULT_RADIO_DATARATE_SIK_AIR 64000
#define DEFAULT_RADIO_DATARATE_LOWEST 6000000

#define DEFAULT_USE_PPCAP_FOR_TX 0
#define DEFAULT_BYPASS_SOCKET_BUFFERS 1
#define DEFAULT_RADIO_TX_POWER_CONTROLLER 20
#define DEFAULT_RADIO_TX_POWER 20
#define DEFAULT_RADIO_SIK_TX_POWER 11

#define DEFAULT_RADIO_LINK_LOAD_PERCENT 40
#define DEFAULT_RADIO_LINK_LOAD_PERCENT_HQ 50
#define DEFAULT_RADIO_LINK_LOAD_PERCENT_HP 30

#ifdef __cplusplus
extern "C" {
#endif 

u32* getChannels433();
int getChannels433Count();
u32* getChannels868();
int getChannels868Count();
u32* getChannels915();
int getChannels915Count();
u32* getChannels24();
int getChannels24Count();
u32* getChannels23();
int getChannels23Count();
u32* getChannels25();
int getChannels25Count();
u32* getChannels58();
int getChannels58Count();

int getBand(u32 freqKhz);
int getChannelIndexForFrequency(u32 nBand, u32 freqKhz);
int isFrequencyInBands(u32 freqKhz, u8 bands);
int getSupportedChannels(u32 supportedBands, int includeSeparator, u32* pOutChannels, int maxChannels);

int* getSiKAirDataRates();
int  getSiKAirDataRatesCount();

int* getLegacyDataRatesBPS();
int getLegacyDataRatesCount();

int getTestDataRatesCountLegacy();
int getTestDataRatesCountMCS();
int* getTestDataRatesLegacy();
int* getTestDataRatesMCS();

int getDataRateShiftedByLevels(int iDatarateBSP, int iLevelsToShift);

u32 getRealDataRateFromMCSRate(int mcsIndex, int iHT40);
u32 getRealDataRateFromRadioDataRate(int dataRateBPS, u32 uRadioFlags, int iIsDownlink);

int getRadioMinimSNRForDataRate(int iDatarate);
int getRadioMinimDBMForDataRate(int iDatarate);

#ifdef __cplusplus
}  
#endif 