#pragma once
#include "../base/base.h"

// Phase 2: controller-side cache of the vehicle's video encoder backend
// (waybeam vs majestic), populated by COMMAND_ID_GET_VIDEO_BACKEND reply.
//
// Used to gray out the Onboard SD Recording menu and block quick-action
// dispatch when the current vehicle doesn't support onboard recording.

#define VEHICLE_BACKEND_UNKNOWN 0
#define VEHICLE_BACKEND_MAJESTIC 1
#define VEHICLE_BACKEND_WAYBEAM 2

// Store the backend reported by the vehicle with the given vehicle id.
void vehicle_backend_cache_set(u32 uVehicleId, u8 uBackend);

// Returns VEHICLE_BACKEND_UNKNOWN if no reply received yet for this vehicle.
u8   vehicle_backend_cache_get(u32 uVehicleId);

// Convenience: true iff we've confirmed this vehicle runs waybeam.
bool vehicle_backend_is_waybeam(u32 uVehicleId);

// Heuristic used before the probe lands (or if the vehicle is unreachable):
// returns true when the model's board type is OpenIPC SigmaStar, which is
// the only class of vehicle that can currently run waybeam.
struct Model;
bool vehicle_backend_likely_supports_waybeam(struct Model* pModel);

// Invalidate the cache for a vehicle (e.g. on re-pair).
void vehicle_backend_cache_clear(u32 uVehicleId);
