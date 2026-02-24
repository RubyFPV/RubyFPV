#include "../base/shared_mem.h"
#include "../base/hardware.h"

#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>

#include <termios.h>
#include <unistd.h>

#include <inttypes.h>
#include <asm/ioctls.h>
//#include <asm/termbits.h>
#include <sys/ioctl.h>

bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}


int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( argc < 3 )
   {
      printf("\nYou must specify speed and in/out:  test_serial_link 57600 [in]/[out]\n\n");
      return -1;
   }

   int iSerialPort = -1;
   int iSpeed = atoi(argv[1]);
   bool bIn = true;
   bool bOut = false;
   if ( strcmp(argv[argc-1], "out") == 0 )
   {
      bIn = false;
      bOut = true;
   }

   log_init("TestSerialLink");
   log_enable_stdout();

   if ( ! hardware_configure_serial("/dev/ttyUSB0", iSpeed) )
      log_line("Failed to configure serial.");

   iSerialPort = hardware_open_serial_port("/dev/ttyUSB0", iSpeed);
   if ( -1 == iSerialPort )
      log_line("Failed to open serial port.");

   log_line("\nStarted on %d baudrate, input: %d, output: %d, serial fd: %d.\n", iSpeed, bIn, bOut, iSerialPort);


   u8 bufferIn[1024];
   int iBufferSize = 1024;

   hardware_serial_send_sik_command(iSerialPort, "+++");
   log_line("Sent command.");
   iBufferSize = 1024;
   int iLen = hardware_serial_wait_sik_response(iSerialPort, 3000, 1, bufferIn, &iBufferSize);

   if ( iLen <= 0 )
   {
      log_line("No response to command mode.exit.");
      close(iSerialPort);
      return 0;
   }
   bufferIn[iLen] = 0;
   log_line("Recv: [%s]",(const char*)bufferIn);
   if ( NULL == strstr((const char*)bufferIn, "OK") )
   {
      log_line("Failed to enter command mode.exit.");
      close(iSerialPort);
      return 0;
   }

   hardware_serial_send_sik_command(iSerialPort, "ATI");
   log_line("Sent command.");

   iBufferSize = 1024;
   iLen = hardware_serial_wait_sik_response(iSerialPort, 1000, 2, bufferIn, &iBufferSize);

   if ( iLen > 0 )
   {
      bufferIn[iLen] = 0;
      log_line("Recv: [%s]",(const char*)bufferIn);
   }

   hardware_serial_send_sik_command(iSerialPort, "ATO");
   log_line("Sent command.");

   iBufferSize = 1024;
   iLen = hardware_serial_wait_sik_response(iSerialPort, 1000, 2, bufferIn, &iBufferSize);

   if ( iLen > 0 )
   {
      bufferIn[iLen] = 0;
      log_line("Recv: [%s]",(const char*)bufferIn);
   }

   close(iSerialPort);
   iSerialPort -1;
   iSerialPort = hardware_open_serial_port("/dev/ttyUSB0", iSpeed);

   log_line("Reopened serial port. Waiting data.");

   while (!bQuit)
   {

      iBufferSize = 1024;
      iLen = hardware_serial_wait_sik_response(iSerialPort, 10, 0, bufferIn, &iBufferSize);

      if ( iLen > 0 )
      {
         bufferIn[iLen] = 0;
         log_line("Recv (%d): [%s]", iLen, (const char*)bufferIn);
      }
   }

   log_line("Stopping");
   if ( -1 != iSerialPort )
      close(iSerialPort);
   log_line("\nEnded\n");
   exit(0);
}
