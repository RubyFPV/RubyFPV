#include "vehicle_backend_cache.h"
#include "../base/models.h"
#include "../base/hardware.h"
#include <string.h>

// Tiny linear cache: Ruby's UI only tracks a few vehicles at once; O(N) scan
// is fine. Entries with uVehicleId == 0 are unused.
#define VEHICLE_BACKEND_CACHE_SIZE 8

typedef struct
{
   u32 uVehicleId;
   u8  uBackend;
} t_vehicle_backend_entry;

static t_vehicle_backend_entry s_Cache[VEHICLE_BACKEND_CACHE_SIZE];

static int _find_slot(u32 uVehicleId)
{
   if ( 0 == uVehicleId )
      return -1;
   for ( int i = 0; i < VEHICLE_BACKEND_CACHE_SIZE; i++ )
      if ( s_Cache[i].uVehicleId == uVehicleId )
         return i;
   return -1;
}

static int _find_free_slot()
{
   for ( int i = 0; i < VEHICLE_BACKEND_CACHE_SIZE; i++ )
      if ( 0 == s_Cache[i].uVehicleId )
         return i;
   return 0; // evict slot 0 if full
}

void vehicle_backend_cache_set(u32 uVehicleId, u8 uBackend)
{
   if ( 0 == uVehicleId )
      return;
   int idx = _find_slot(uVehicleId);
   if ( idx < 0 )
      idx = _find_free_slot();
   s_Cache[idx].uVehicleId = uVehicleId;
   s_Cache[idx].uBackend = uBackend;
}

u8 vehicle_backend_cache_get(u32 uVehicleId)
{
   int idx = _find_slot(uVehicleId);
   if ( idx < 0 )
      return VEHICLE_BACKEND_UNKNOWN;
   return s_Cache[idx].uBackend;
}

bool vehicle_backend_is_waybeam(u32 uVehicleId)
{
   return (vehicle_backend_cache_get(uVehicleId) == VEHICLE_BACKEND_WAYBEAM);
}

bool vehicle_backend_likely_supports_waybeam(Model* pModel)
{
   if ( NULL == pModel )
      return false;
   u32 uBoard = (pModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK);
   return (uBoard == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q);
}

void vehicle_backend_cache_clear(u32 uVehicleId)
{
   int idx = _find_slot(uVehicleId);
   if ( idx >= 0 )
   {
      s_Cache[idx].uVehicleId = 0;
      s_Cache[idx].uBackend = VEHICLE_BACKEND_UNKNOWN;
   }
}
