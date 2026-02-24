#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"

#include <time.h>
#include <sys/resource.h>

bool quit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
}


int main(int argc, char *argv[])
{
   if ( argc < 3 )
   {
      printf("\nYou must specify a port number and a message\n14 - uplink , 15 - downlink\n");
      return -1;
   }
   setpriority(PRIO_PROCESS, 0, -10);

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TX_TEST_PORT");
   log_enable_stdout();

   radio_init_link_structures();
   radio_enable_crc_gen(1);


   int port = atoi(argv[1]);
   char* szMsg = argv[2];
   printf("\nSending on port: %d, message: %s (%d bytes)\n", port, szMsg, (int)strlen(szMsg)+1);

   printf("\nCreating socket...\n");
   int res = radio_open_interface_for_write(0);
   if ( res <= 0 )
   {
      printf("\nFailed to open socket for write.\n");
      return -1;
   }

   radio_set_out_datarate(12, 0, get_current_timestamp_ms());
   printf("Datarate: %d;\n", 12);

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_length = sizeof(t_packet_header) + strlen(szMsg)+1;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), szMsg, strlen(szMsg)+1);

   u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
   int totalLength = radio_build_new_raw_ieee_packet(0, rawPacket, packet, PH.total_length, port, 0);

   if ( 0 == radio_write_raw_ieee_packet(0, rawPacket, totalLength, 0) )
      printf("Failed to write to radio interface.\n");

   hardware_sleep_ms(50);
   radio_close_interface_for_write(0);
   printf("\nSent %d bytes on port: %d\n", PH.total_length, port);
   return (0);
}
