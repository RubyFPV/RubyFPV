#include "../base/hardware.h"
#include "../base/shared_mem.h"
#include "../radio/radiolink.h"

#include <time.h>
#include <sys/resource.h>

int portNumber = 1;
data_packet_ruby_telemetry* pdpvt;

void process_data(u8* buffer, int length)
{
   data_packet_header dph;
   memcpy(&dph, buffer, sizeof(dph) );
   memcpy(pdpvt, buffer+sizeof(dph), sizeof(data_packet_ruby_telemetry) );
   if ( (dph.packetType == PACKET_TYPE_RUBY_TELEMETRY) && radio_validate_crc((data_packet_header*)buffer) )
   {
      //log_line("CRC valid for a telemetry packet");
      //log_line("Found a vehicle. Name: %s, Id: %u", pdpvt->vehicle_name, pdpvt->uVehicleId);
      printf("1\n");
      exit(1);
   }
}

void process_all_data(u8* buffer, int length)
{
   data_packet_header dph;
   memcpy(&dph, buffer, sizeof(dph) );
   memcpy(pdpvt, buffer+sizeof(dph), sizeof(data_packet_ruby_telemetry) );
   if ( (dph.packetType == PACKET_TYPE_TELEMETRY_ALL) && radio_validate_crc((data_packet_header*)buffer) )
   {
      //log_line("CRC valid for a telemetry packet");
      //log_line("Found a vehicle. Name: %s, Id: %u", pdpvt->vehicle_name, pdpvt->uVehicleId);
      printf("1\n");
      exit(1);
   }
}

int main(int argc, char *argv[])
{
   setpriority(PRIO_PROCESS, 0, -10);
   log_disable();
   //log_init("RX_LinkStatus_Ping");
   //log_line("\nStarted.\n");
   if ( argc < 2 )
   {
      //log_line("Too few params. Add wlan interfaces");
      printf("0\n");
      exit(0);
   }

   hardware_enumerate_radio_interfaces();
   init_wlan_interfaces_for_read(argc, argv, portNumber);

   pdpvt = shared_mem_ruby_telemetry_open(false);
   reset_data_packet_ruby_telemetry(pdpvt);

   // Try for 10 sec(s) to find data
   for( int j=0; j<100; j++ )
   {
      printf(".");
      fflush(stdout);
      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 1e5; // 100ms
      int maxfd = -1;
      FD_ZERO(&readset);
      for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( pNICInfo->openedForRead )
         {
            FD_SET(pNICInfo->monitor_interface.selectable_fd, &readset);
            if ( pNICInfo->monitor_interface.selectable_fd > maxfd )
               maxfd = pNICInfo->monitor_interface.selectable_fd;
         }
      }
      int n = select(maxfd+1, &readset, NULL, NULL, &to);
      if(n == 0)
         continue;
      for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if(FD_ISSET(pNICInfo->monitor_interface.selectable_fd, &readset))
         {
            printf("\nReceived some data\n");
            int length = 0;
            u8* buffer = NULL;
            buffer = process_wlan_data_in(i, &length, NULL);
            if ( NULL != buffer )
            {
               printf("\nProcessed received data\n");
               if ( length == (sizeof(data_packet_header) + sizeof(data_packet_ruby_telemetry)) )
               {
                  printf("\nData is: ruby telemetry.\n");
                  process_data(buffer, length);
               }
               if ( length == (sizeof(data_packet_header) + sizeof(data_packet_ruby_telemetry)+sizeof(data_packet_fc_telemetry)) )
               {
                  printf("\nData is: ruby telemetry all.\n");
                  process_all_data(buffer, length);
               }
            }
         }
      }
   }
   //log_line("Vehicle not found");
   close_wlan_interfaces_for_read();
   printf("0\n");
   exit(0);
}
