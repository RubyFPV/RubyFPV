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

#include "telemetry_mavlink.h"
#include "telemetry.h"
#include "../base/hardware_procs.h"
#include "../base/parse_fc_telemetry.h"
#include "../base/models.h"
#include "../base/ruby_ipc.h"
#include "../../mavlink/common/mavlink.h"
#include "shared_vars.h"
#include "timers.h"

void broadcast_vehicle_stats();
void save_model();
bool isRadioLinksInitInProgress();

bool s_bDidSentMAVLinkSetup = false;
u32 s_uMAVLinkSetupTime = 0;
bool s_bOnArmEventHandled = false;
bool s_bLogNextMAVLinkMessage = true;
static int s_iCountTelemetryMAVLinkWriteErrors = 0;

extern t_packet_header_ruby_telemetry_extended_v6 sPHRTE;
u32 s_SentTelemetryCounter = 0;
long int s_lLastPosLat = 0;
long int s_lLastPosLon = 0;
extern bool home_set;
extern long int home_lat;
extern long int home_lon;
extern u32 s_CountMessagesFromFCPerSecond;
extern t_packet_header_rc_info_downstream* s_pPHDownstreamInfoRC; // Info to send back to ground
extern int s_fIPCToRouter;

void _telemetry_mavlink_send_setup()
{
   if ( (telemetry_get_serial_port_file() < 0) || (NULL == g_pCurrentModel) )
      return;

   if ( s_bDidSentMAVLinkSetup )
      return;

   int componentId = MAV_COMP_ID_MISSIONPLANNER;
   //int componentId = MAV_COMP_ID_SYSTEM_CONTROL;
   //int componentId = 255;

   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID )
      parse_telemetry_allow_any_sysid(1);
   else
      parse_telemetry_allow_any_sysid(0);

   parse_telemetry_force_always_armed((g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED)? true: false);

   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_REMOVE_DUPLICATE_FC_MESSAGES )
      parse_telemetry_remove_duplicate_messages(true);
   else
      parse_telemetry_remove_duplicate_messages(false);

   bool bUseLocalVSpeed = false;
   int li = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   if ( (li >= 0) && (li < MODEL_MAX_OSD_SCREENS) )
   if ( g_pCurrentModel->osd_params.osd_flags2[li] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED )
      bUseLocalVSpeed = true;

   parse_telemetry_set_show_local_vspeed(bUseLocalVSpeed);

   int len = 0;
   u8 serialBufferOut[300];
   mavlink_message_t msgHeartBeat;
   mavlink_message_t msgDataStreams;
   mavlink_message_t msgMsgInterval;
   mavlink_message_t msgHighLatency;

   
   log_line("[Telem] Initializing MAVLink with flight controller...");
   if ( ! (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_REQUEST_DATA_STREAMS) )
   {
      log_line("[Telem] Telemetry is set as read only with no requests for data streams. Do nothing.");
      return;
   }

   
   mavlink_msg_heartbeat_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgHeartBeat, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0,0,0);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgHeartBeat);
   
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
   }
   else
      s_iCountTelemetryMAVLinkWriteErrors = 0;
   
   int rate = g_pCurrentModel->telemetry_params.iUpdateRateHz;
   if ( rate < 1 ) rate = 1;
   if ( rate > 10 ) rate = 10;
   mavlink_msg_request_data_stream_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgDataStreams, g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_AUTOPILOT1, MAV_DATA_STREAM_ALL, rate, 1);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgDataStreams);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
      return;
   }
   else
      s_iCountTelemetryMAVLinkWriteErrors = 0;
   log_line("[Telem] Requested all MAVLink data streams");
   

   char szParam[32];
   strcpy(szParam, "RC_OVERRIDE_TIME");

   if ( g_pCurrentModel->rc_params.rc_enabled )
   {      
      float fTimeout = 1.0/g_pCurrentModel->rc_params.rc_frames_per_second;
      fTimeout *= 2.5;
      log_line("[Telem] Sending to FC an RC_OVERRIDE timeout of %.2f seconds", fTimeout);
      mavlink_message_t msgSetParam;
      mavlink_msg_param_set_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgSetParam,
                               g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, szParam, fTimeout, MAV_PARAM_TYPE_REAL32);

      len = mavlink_msg_to_send_buffer(serialBufferOut, &msgSetParam);
      if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
      {
         s_iCountTelemetryMAVLinkWriteErrors++;
         if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
            log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
         return;
      }
      else
         s_iCountTelemetryMAVLinkWriteErrors = 0;
   }

   mavlink_message_t msgReqParam;
   mavlink_msg_param_request_read_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgReqParam,
                               g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, szParam, -1);

   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgReqParam);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
      return;
   }
   else
      s_iCountTelemetryMAVLinkWriteErrors = 0;
  
   //mavlink_message_t msgReqParamList;
   //mavlink_msg_param_request_list_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
   //                            uint8_t target_system, uint8_t target_component) 

/*
   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_ATTITUDE, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_SCALED_IMU, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_RAW_IMU, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
*/
   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_HIGH_LATENCY, 1000000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
      return;
   }
   else
      s_iCountTelemetryMAVLinkWriteErrors = 0;

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_HIGH_LATENCY2, 1000000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
      return;
   }
   else
     s_iCountTelemetryMAVLinkWriteErrors = 0;
/*
   log_line("Requested MAVLink message interval.");
*/

   mavlink_command_long_t high_latency = {0};
   high_latency.target_system = g_pCurrentModel->telemetry_params.vehicle_mavlink_id;
   high_latency.target_component = MAV_COMP_ID_AUTOPILOT1;
   high_latency.command = MAV_CMD_CONTROL_HIGH_LATENCY;
   high_latency.confirmation = 0;
   high_latency.param1 = 1;        
 
   mavlink_msg_command_long_encode(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgHighLatency, &high_latency);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgHighLatency);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
   {
      s_iCountTelemetryMAVLinkWriteErrors++;
      if ( s_iCountTelemetryMAVLinkWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write to serial port to FC");
      return;
   }
   else
      s_iCountTelemetryMAVLinkWriteErrors = 0;

   log_line("[Telem] Requested MAVLink high latency messages.");

/*
   mavlink_command_long_t cmd_mode = {0};
   cmd_mode.target_system = g_pCurrentModel->telemetry_params.vehicle_mavlink_id;
   cmd_mode.target_component = MAV_COMP_ID_AUTOPILOT1;
   cmd_mode.command = MAV_CMD_DO_SET_MODE;
   cmd_mode.confirmation = 0;
   cmd_mode.param1 = MAV_MODE_MANUAL_DISARMED;        
 
   mavlink_message_t msg;
   mavlink_msg_command_long_encode(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, &cmd_mode);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
 
   log_line("Requested stab mode.");
*/

   log_line("[Telem] Initialized MAVLink with flight controller, controller mavid: %d, vehicle mavid: %d", g_pCurrentModel->telemetry_params.controller_mavlink_id, g_pCurrentModel->telemetry_params.vehicle_mavlink_id);
   s_bDidSentMAVLinkSetup = true;
   s_uMAVLinkSetupTime = g_TimeNow;
}

void telemetry_mavlink_on_open_port(int iSerialPortFile)
{
   _telemetry_mavlink_send_setup();
}

void telemetry_mavlink_on_close()
{
   s_bDidSentMAVLinkSetup = false;
   s_uMAVLinkSetupTime = 0;
}


void telemetry_mavlink_periodic_loop()
{
   if ( ! s_bDidSentMAVLinkSetup )
   if ( g_TimeNow > s_uMAVLinkSetupTime + 2000 )
   {
      s_uMAVLinkSetupTime = g_TimeNow;
      _telemetry_mavlink_send_setup();
   }
}

void telemetry_mavlink_on_second_lapse()
{
   int ihb = get_heartbeat_msg_count();
   int isys = get_system_msg_count();
   if ( ihb > 15 ) ihb = 15;
   if ( isys > 15 ) isys = 15;
   reset_heartbeat_msg_count();
   reset_system_msg_count();

   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
   if ( NULL == pFCTelem )
      return;
   pFCTelem->fc_hudmsgpersec = ((u8)ihb) | (((u8)isys)<<4);

   if ( pFCTelem->flight_mode != 0 )
   if ( pFCTelem->flight_mode & FLIGHT_MODE_ARMED )
      pFCTelem->arm_time++;
   
   g_pCurrentModel->updateStatsEverySecond(pFCTelem);

   if ( pFCTelem->flight_mode != 0 )
   {
      if ( pFCTelem->flight_mode & FLIGHT_MODE_ARMED )
      {
         if ( (pFCTelem->arm_time >= 1) && (pFCTelem->arm_time < 5) )
         {
            if ( ! s_bOnArmEventHandled )
            {
               s_bOnArmEventHandled = true;

               char szBuff[128];
               snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ARMED);
               hw_execute_bash_command(szBuff, NULL);

               g_pCurrentModel->m_Stats.uTotalFlights++;
               log_line("Armed Event. Saving model, total flights: %d", g_pCurrentModel->m_Stats.uTotalFlights);
               g_pCurrentModel->m_Stats.uCurrentFlightTime = 1; // seconds
               g_pCurrentModel->m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
               g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent = 0; // 0.1 miliAmps (1/10000 amps);

               g_pCurrentModel->m_Stats.uCurrentMaxAltitude = 0; // meters
               g_pCurrentModel->m_Stats.uCurrentMaxDistance = 0; // meters
               g_pCurrentModel->m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
               g_pCurrentModel->m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)
               save_model();
            }
         }

         if ( pFCTelem->arm_time > 1 )
         {
            // speed is in 0.01m/s
            // total distance is in 0.01m
            float speed = pFCTelem->hspeed-100000.0;
            pFCTelem->total_distance += speed;
         }
         else
            pFCTelem->total_distance = 0;
      }
      else
      {
         s_bOnArmEventHandled = false;
         if ( pFCTelem->arm_time != 0 )
         {
            char szBuff[128];
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ARMED);
            hw_execute_bash_command(szBuff, NULL);
         }
         pFCTelem->arm_time = 0;
      }
   }

   broadcast_vehicle_stats();
}

bool telemetry_mavlink_on_new_serial_data(u8* pData, int iDataLength)
{
   if ( (NULL == pData) || (NULL == g_pCurrentModel) || (iDataLength <= 0) )
      return false;
   if ( parse_telemetry_from_fc(pData, iDataLength, telemetry_get_fc_telemetry_header(), &sPHRTE, g_pCurrentModel->vehicle_type, g_pCurrentModel->telemetry_params.fc_telemetry_type) )
   {
      set_time_last_mavlink_message_from_fc(g_TimeNow);
      return true;
   }
   return false;
}


void _preprocess_fc_telemetry(t_packet_header_fc_telemetry* pPHFCT)
{
   pPHFCT->fc_telemetry_type = g_pCurrentModel->telemetry_params.fc_telemetry_type;

   pPHFCT->uFCFlags = pPHFCT->uFCFlags & (~FC_TELE_FLAGS_HAS_GPS_FIX);
   if ( pPHFCT->gps_fix_type >= GPS_FIX_TYPE_2D_FIX )
      pPHFCT->uFCFlags = pPHFCT->uFCFlags | FC_TELE_FLAGS_HAS_GPS_FIX;
   else if ( (g_pCurrentModel->iGPSCount > 1) && ( pPHFCT->extra_info[2] != 0xFF )
             && ( pPHFCT->extra_info[1] > 0 ) && (pPHFCT->extra_info[1] != 0xFF) && ( pPHFCT->extra_info[2] >= GPS_FIX_TYPE_2D_FIX ) )
      pPHFCT->uFCFlags = pPHFCT->uFCFlags | FC_TELE_FLAGS_HAS_GPS_FIX;        

   bool bHasAnyGPSGoodInfo = false;
   if ( pPHFCT->satelites > 2 && pPHFCT->hdop < 9000 )
      bHasAnyGPSGoodInfo = true;
   if ( (g_pCurrentModel->iGPSCount > 1) 
             && ( pPHFCT->extra_info[1] > 2 ) && (pPHFCT->extra_info[1] != 0xFF)
             && ( ((int)pPHFCT->extra_info[3]) * 255 + pPHFCT->extra_info[4] < 9000 ) )
      bHasAnyGPSGoodInfo = true;

   if ( (!has_received_gps_info()) || (! has_received_flight_mode()) || (! bHasAnyGPSGoodInfo) )
   {
      pPHFCT->uFCFlags = pPHFCT->uFCFlags & (~FC_TELE_FLAGS_POS_CURRENT);
      pPHFCT->uFCFlags = pPHFCT->uFCFlags & (~FC_TELE_FLAGS_POS_HOME);
      return;
   }

   s_lLastPosLat = pPHFCT->latitude;
   s_lLastPosLon = pPHFCT->longitude;
   
   if ( (pPHFCT->gps_fix_type >= GPS_FIX_TYPE_2D_FIX) &&
        ( (pPHFCT->latitude > 5) || (pPHFCT->latitude < -5) ) &&
        ( (pPHFCT->longitude > 5) || (pPHFCT->longitude < -5) ) )
   {
      if ( (! home_set) || (! (pPHFCT->flight_mode & FLIGHT_MODE_ARMED) ) )
      {
         home_set = true;
         home_lat = pPHFCT->latitude;
         home_lon = pPHFCT->longitude;        
      }

      // Update distance from home
      if ( home_set )
      {
         //log_line("lat: %u, lon: %u, hlat: %u, hlon: %u", dpfct.latitude, dpfct.longitude, home_lat, home_lon);
         pPHFCT->distance = 100 * distance_meters_between( home_lat/10000000.0, home_lon/10000000.0, pPHFCT->latitude/10000000.0, pPHFCT->longitude/10000000.0 );
         if ( pPHFCT->distance/100.0/1000.0 > 500.0 )
         {
            home_lat = pPHFCT->latitude;
            home_lon = pPHFCT->longitude;
         }
         //log_line("home set, distance: %d", dpfct.distance);
      }
   }
   else if ( (g_pCurrentModel->iGPSCount > 1) && ( pPHFCT->extra_info[2] != 0xFF )
             && ( pPHFCT->extra_info[1] > 0 ) && (pPHFCT->extra_info[1] != 0xFF)
             && ( pPHFCT->extra_info[2] >= GPS_FIX_TYPE_2D_FIX ) &&
                ( (pPHFCT->latitude > 5) || (pPHFCT->latitude < -5) ) )
   {
      if ( (! home_set) || (! (pPHFCT->flight_mode & FLIGHT_MODE_ARMED) ) )
      {
         home_set = true;
         home_lat = pPHFCT->latitude;
         home_lon = pPHFCT->longitude;
      }

      // Update distance from home
      if ( home_set )
      {
         //log_line("lat: %u, lon: %u, hlat: %u, hlon: %u", dpfct.latitude, dpfct.longitude, home_lat, home_lon);
         pPHFCT->distance = 100 * distance_meters_between( home_lat/10000000.0, home_lon/10000000.0, pPHFCT->latitude/10000000.0, pPHFCT->longitude/10000000.0 );
         //log_line("home set, distance: %d", dpfct.distance);
      }
   }

   if ( s_SentTelemetryCounter%10 )
   {
      pPHFCT->uFCFlags = pPHFCT->uFCFlags | FC_TELE_FLAGS_POS_CURRENT;
      pPHFCT->uFCFlags = pPHFCT->uFCFlags & (~FC_TELE_FLAGS_POS_HOME);
   }
   else if ( home_set )
   {
      // Send home position every 10 frames
      //log_line("Send home pos lat: %u, lon: %u", home_lat, home_lon);
      pPHFCT->uFCFlags = pPHFCT->uFCFlags & (~FC_TELE_FLAGS_POS_CURRENT);
      pPHFCT->uFCFlags = pPHFCT->uFCFlags | FC_TELE_FLAGS_POS_HOME;
      pPHFCT->latitude = home_lat;
      pPHFCT->longitude = home_lon;
   }
}

void telemetry_mavlink_send_to_controller()
{
   if ( (NULL == g_pCurrentModel) || (!g_bRouterReady) )
      return;

   s_SentTelemetryCounter++;

   t_packet_header PH;

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
   t_packet_header_fc_extra* pFCTelemExtra = telemetry_get_fc_extra_telemetry_header();
   _preprocess_fc_telemetry(pFCTelem);
   
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED )
      pFCTelem->flight_mode |= FLIGHT_MODE_ARMED;

   pFCTelem->extra_info[5]++;
   pFCTelem->extra_info[6] = (s_CountMessagesFromFCPerSecond>255)?255:s_CountMessagesFromFCPerSecond;

   // Do we have a new message from FC?

   bool bSendFCMessage = false;
   if ( (get_last_message_time() > 0) && (0 != get_last_message()[0]) )
   if ( get_last_message_time() + TIMEOUT_FC_MESSAGE > g_TimeNow )   
      bSendFCMessage = true;

   if ( bSendFCMessage )
   {
      if ( s_bLogNextMAVLinkMessage )
         log_line("Sending FC message from vehicle to station: [%s]", get_last_message());
      s_bLogNextMAVLinkMessage = false;
      pFCTelem->uFCFlags = pFCTelem->uFCFlags | FC_TELE_FLAGS_HAS_MESSAGE;

      pFCTelemExtra->chunk_index = 0;
      memset(pFCTelemExtra->text, 0, FC_MESSAGE_MAX_LENGTH);
      strcpy((char*)(pFCTelemExtra->text), get_last_message());
   }
   else
   {
      s_bLogNextMAVLinkMessage = true;
      pFCTelem->uFCFlags = pFCTelem->uFCFlags & (~FC_TELE_FLAGS_HAS_MESSAGE);
   }

   pFCTelem->uFCFlags = pFCTelem->uFCFlags & (~FC_TELE_FLAGS_RC_FAILSAFE);

   #ifdef FEATURE_ENABLE_RC
   if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
   if ( s_pPHDownstreamInfoRC->is_failsafe )
      pFCTelem->uFCFlags = pFCTelem->uFCFlags | FC_TELE_FLAGS_RC_FAILSAFE;
   #endif

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry);
   if ( bSendFCMessage )
   {
      PH.packet_type = PACKET_TYPE_FC_TELEMETRY_EXTENDED;
      PH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) + (u16)sizeof(t_packet_header_fc_extra);
   }

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, &PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), pFCTelem, sizeof(t_packet_header_fc_telemetry));
   if ( bSendFCMessage )
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_fc_telemetry), pFCTelemExtra, sizeof(t_packet_header_fc_extra));

   if ( g_bRouterReady && (! isRadioLinksInitInProgress()) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   // Set lat, lon to last values as can have home pos in them if we just sent home pos to controller.

   pFCTelem->latitude = s_lLastPosLat;
   pFCTelem->longitude = s_lLastPosLon;
}
