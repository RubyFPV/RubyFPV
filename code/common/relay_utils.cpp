/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "relay_utils.h"

bool relay_controller_is_vehicle_id_relayed_vehicle(Model* pMainModel, u32 uVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uVehicleId) )
      return false;

   if ( pMainModel->uVehicleId == uVehicleId )
      return false;
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || ( pMainModel->relay_params.uRelayedVehicleId != uVehicleId) )
      return false;

   return true;
}

bool relay_controller_must_display_video_from(Model* pMainModel, u32 uRelayedVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uRelayedVehicleId) )
      return false;

   bool bHasValidRelayedVehicle = false;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) && (pMainModel->relay_params.uRelayedVehicleId != 0) )
   if ( pMainModel->relay_params.uRelayedVehicleId != pMainModel->uVehicleId )
      bHasValidRelayedVehicle = true;

   if ( ! bHasValidRelayedVehicle )
      return true;

   if ( uRelayedVehicleId != pMainModel->uVehicleId )
   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) )
   if ( uRelayedVehicleId == pMainModel->relay_params.uRelayedVehicleId )
      return true;

   if ( (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) )
   if ( uRelayedVehicleId == pMainModel->uVehicleId )
      return true;

   return false;
}

bool relay_controller_must_display_remote_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return false;
   return relay_controller_must_display_video_from(pMainModel, pMainModel->relay_params.uRelayedVehicleId);
}

bool relay_controller_must_display_main_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return true;
   return relay_controller_must_display_video_from(pMainModel, pMainModel->uVehicleId);
}

Model* relay_controller_get_relayed_vehicle_model(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return NULL;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (0 == pMainModel->relay_params.uRelayedVehicleId) )
      return NULL;
   if ( pMainModel->relay_params.uRelayedVehicleId == pMainModel->uVehicleId )
      return NULL;

   Model* pModel = findModelWithId(pMainModel->relay_params.uRelayedVehicleId, 2);
   return pModel;
}


bool relay_controller_must_forward_telemetry_from(Model* pMainModel, u32 uRelayedVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uRelayedVehicleId) )
      return false;

   if ( pMainModel->uVehicleId == uRelayedVehicleId )
      return true;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
      return false;

   if ( pMainModel->relay_params.uRelayedVehicleId != uRelayedVehicleId )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY) )
      return false;

   return true;
}

bool relay_vehicle_must_forward_video_from_relayed_vehicle(Model* pMainModel, u32 uRelayedVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uRelayedVehicleId) )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( pMainModel->relay_params.uRelayedVehicleId != uRelayedVehicleId )
      return false;

   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
      return true;

   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) )
   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) )
   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
      return false;

   return true;
}

bool relay_vehicle_must_forward_own_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return false;

   // Standalone and not a relayed node

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
   if ( !( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAYED_NODE) )
      return true;

   // Standalone vehicle or relayed vehicle always sends it's own video as we can have spectators or controllers connected to it
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
      return true;

   // Relay node

   bool bIsRelayNode = false;
   if ( pMainModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( pMainModel->relay_params.uRelayedVehicleId != 0 )
      bIsRelayNode = true;

   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAY_NODE )
      bIsRelayNode = true;

   if ( bIsRelayNode )
   {
      if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
         return false;

      if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO ) )
         return true;

      if ( (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN) ||
           (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) ||
           (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
         return true;
      return false;
   }

   // Relayed node
   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAYED_NODE )
   {
      if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) )
      if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN )
         return false;
      return true;
   }

   return true;
}

bool relay_vehicle_must_forward_relayed_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return false;
   
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (0 == pMainModel->relay_params.uRelayedVehicleId) )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE ) ||
        ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE ) ||
        ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN ) ||
        ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE ) )
      return true;
   return false;
}