#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>
/*
#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
*/

#include <termios.h>
#include <unistd.h>

#include <inttypes.h>
#include <asm/ioctls.h>
//#include <asm/termbits.h>
#include <sys/ioctl.h>

bool bQuit = false;
int fSerial = -1;


unsigned char reverse(unsigned char b)
 {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

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

   log_init("TestSerial");
   log_enable_stdout();
   log_line("\nStarted.\n");

   //hw_execute_bash_command("stty -F /dev/ttyUSB0 raw pass8 115200", NULL);

   int baudRate = 115200;
   //int fSerial = open ("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY );
   int fSerial = open ("/dev/ttyUSB0", O_RDWR | O_NOCTTY );

   if ( -1 == fSerial )
   {
      log_softerror_and_alarm("Failed to open serial port %s", "/dev/ttyUSB0");
      return -1;
   }



   struct termios options;
   tcgetattr(fSerial, &options);
   cfmakeraw(&options);

   cfsetospeed(&options, B115200);

   options.c_cflag &= ~CSIZE;
   //options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag |= CS7; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   //options.c_cflag |= PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   //options.c_cflag |= CSTOPB; // 1 stop bit
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   //options.c_cflag |= CREAD | CLOCAL; // Set local mode on

   //options.c_lflag &= ~ECHO; // no echo
   //options.c_lflag &= ~ECHOE; // no echo
   //options.c_lflag &= ~ECHONL; // no echo
   //options.c_lflag &= ~ICANON;
   //options.c_lflag &= ~ISIG;

   //options.c_iflag = IGNBRK;
   //options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   //options.c_iflag |= IXANY;
   options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

   options.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
   options.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

   //options.c_cc[VMIN] = 0;
   //options.c_cc[VTIME] = 5;

   //options.c_cflag &= ~CBAUD;
   //options.c_cflag |= BOTHER;
   //options.c_ispeed = 115200;
   //options.c_ospeed = 115200;

   if ( tcsetattr(fSerial, TCSANOW, &options) != 0 )
   {
     log_line("Error %i from tcsetattr: %s\n", errno, strerror(errno));
   }

   struct termios options2;
   cfmakeraw(&options2);
   tcgetattr(fSerial, &options2);
   int sp = cfgetispeed(&options2);
   log_line("Current speed: %d, (exp: %d) match: %d", sp, B115200, sp == B115200 );


   log_line("Opened serial port %s with baudrate %ld bps.", "/dev/ttyUSB0", baudRate);

   u8 bufferIn[1024];

   while (!bQuit)
   {
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 10000; // 10 ms
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(fSerial, &readset);
      int res = select(fSerial+1, &readset, NULL, NULL, &to);
      if ( res < 0 )
         return false;
      if ( ! FD_ISSET(fSerial, &readset) )
         continue;

      int length = read(fSerial, &(bufferIn[0]), 1024);
      if ( 0 == length )
         continue;

      for( int i=0; i<length-32; i++ )
      {
         //bufferIn[i] = reverse(bufferIn[i]);
         //bufferIn[i] = ~(bufferIn[i]);

         if ( bufferIn[i] == bufferIn[i+32] )
            log_line("Match on: %x", bufferIn[i]);
      }
      log_line("Parsing serial %d bytes", length);
      log_buffer(bufferIn, length);
      for( int i=0; i<length; i++ )
         if ( bufferIn[i] == 0x20 || bufferIn[i] == 0x40 )
         {
            log_line("\n\nFrame start\n\n");
         }
   }
   close(fSerial);
   log_line("\nEnded\n");
   exit(0);
}
