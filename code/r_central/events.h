#pragma once

void onModelAdded(u32 uModelId);
void onModelDeleted(u32 uModelId);

void onMainVehicleChanged(bool bRemovePreviousVehicleState);

void onEventReboot();
void onEventBeforePairing();
void onEventPaired();
void onEventBeforePairingStop();
void onEventPairingStopped();
void onEventPairingStartReceivingData(u32 uVehicleId);

void onEventArmed(u32 uVehicleId);
void onEventDisarmed(u32 uVehicleId);

void onEventRelayModeChanged();

bool onEventReceivedModelSettings(u32 uVehicleId, u8* pBuffer, int length, bool bUnsolicited);
void onEventPairingDiscardAllUIActions();
bool onEventPairingTookUIActions();
void onEventFinishedPairUIAction();
void onEventCheckForPairPendingUIActionsToTake();

