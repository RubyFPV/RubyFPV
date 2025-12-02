#pragma once

bool relay_controller_is_vehicle_id_relayed_vehicle(Model* pMainModel, u32 uVehicleId);

bool relay_controller_must_display_video_from(Model* pMainModel, u32 uRelayedVehicleId);
bool relay_controller_must_display_remote_video(Model* pMainModel);
bool relay_controller_must_display_main_video(Model* pMainModel);
Model* relay_controller_get_relayed_vehicle_model(Model* pMainModel);
bool relay_controller_must_forward_telemetry_from(Model* pMainModel, u32 uRelayedVehicleId);


bool relay_vehicle_must_forward_video_from_relayed_vehicle(Model* pMainModel, u32 uRelayedVehicleId);
bool relay_vehicle_must_forward_own_video(Model* pMainModel);
bool relay_vehicle_must_forward_relayed_video(Model* pMainModel);
