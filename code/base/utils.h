#pragma once
#include <pthread.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/hardware.h"
#include "../base/shared_mem_i2c.h"


typedef struct
{
   // Worker thead info
   bool bConfiguringSiKThreadWorking; // reinitialization worker thread is active
   int iThreadRetryCounter;
   bool bMustReinitSiKInterfaces; // true if SiK interfaces must be reinitialized (using a worker thread)
   int iMustReconfigureSiKInterfaceIndex; // 0 or positive if SiK interface must be reconfigured and reinitialized (using a worker thread)
   u32  uTimeLastSiKReinitCheck;
   u32  uTimeIntervalSiKReinitCheck;
   u32  uSiKInterfaceIndexThatBrokeDown;

   // Helper configure tool info
   bool bConfiguringToolInProgress; // SiK configuring is in progress using the helper tool
   u32  uTimeStartConfiguring; // start time of the SiK configure helper tool
   
   bool bInterfacesToReopen[MAX_RADIO_INTERFACES]; // SiK interfaces to reopen after the worker thread or helper tool finishes

} ALIGN_STRUCT_SPEC_INFO t_sik_radio_state;

typedef struct
{
   u32 uMinimumTimeToSwitchLower;
   u32 uMinimumTimeToSwitchHigher;
   u32 uMinimumGoodTimeToSwitchHigher;
   u32 uTimeToLookBackForECUsed;
   u32 uTimeToLookBackForECMax;
   int iPercentageECUsed;
   int iPercentageECMax;

   u32 uTimeToLookBackForRetr;
   int iMaxRetr;

   u32 uTimeToLookBackForRxLost;
   int iMaxRxLostPercent;

   int iMinimRSSIThreshold;
   int iMinimSNRThreshold;
} ALIGN_STRUCT_SPEC_INFO type_adaptive_metrics;

bool ruby_is_first_pairing_done();
void ruby_set_is_first_pairing_done();

void reset_sik_state_info(t_sik_radio_state* pState);

float compute_controller_rc_value(Model* pModel, int nChannel, float prevRCValue, t_shared_mem_i2c_controller_rc_in* pRCIn, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface, u32 miliSec);
u32 get_rc_channel_failsafe_type(Model* pModel, int nChannel);
int get_rc_channel_failsafe_value(Model* pModel, int nChannel, int prevRCValue);

float compute_output_rc_value(Model* pModel, int nChannel, float prevRCValue, float fNormalizedValue, u32 miliSec);

void log_current_full_radio_configuration(Model* pModel);

bool radio_utils_set_interface_frequency(Model* pModel, int iRadioIndex, int iAssignedModelRadioLink, u32 uFrequencyKhz, shared_mem_process_stats* pProcessStats, u32 uDelayMs);
bool radio_utils_set_datarate_atheros(Model* pModel, int iCard, int datarate_bps, u32 uDelayMs);

int check_write_filesystem();

long metersBetweenPlaces(double lat1, double lon1, double lat2, double lon2);
long distance_meters_between(double lat1, double lon1, double lat2, double lon2);

void compute_adaptive_metrics(type_adaptive_metrics* pAdaptiveMetrics, int iAdaptiveStrength, u32 uAdaptiveWeights);
void log_adaptive_metrics(Model* pModel, type_adaptive_metrics* pAdaptiveMetrics, int iAdaptiveStrength, u32 uAdaptiveWeights);

void utils_log_radio_packets_sizes();
