#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>

#include <sys/ioctl.h>
extern "C" {
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
}
bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}

u32 revert(u32 input)
{
   u32 out = (input & 0xFF) << 8;
   out = out | ((input>>8) & 0xFF);
   return out;
}

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestI2C");
   log_enable_stdout();
   log_line("\nStarted.\n");

int file;
int adapter_nr = 1;
char filename[20];
snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
file = open(filename, O_RDWR);
if (file < 0) {
  exit(1);
}
  int addr = 0x41;
if (ioctl(file, I2C_SLAVE, addr) < 0) {
  exit(1);
}

u8 buf[10];


buf[0] = 0x05;
buf[1] = ((4096>>8) & 0xFF);
buf[2] = (4096 & 0xFF);
write(file, buf, 3);

u16 val = (0x2000) | (0x1800) | (0x0180) | (0x0018) | (0x07);

buf[0] = 0x00;
buf[1] = ((val>>8) & 0xFF);
buf[2] = (val & 0xFF);
write(file, buf, 3);

buf[0] = 0x05;
buf[1] = ((4096>>8) & 0xFF);
buf[2] = (4096 & 0xFF);

while ( ! bQuit )
{
   u32 resv;
   u32 resc;
   u32 resp;

   write(file, buf, 3);
   hardware_sleep_ms(50);

   resv = i2c_smbus_read_word_data(file, 0x02);
   resv = revert(resv);

   write(file, buf, 3);
   hardware_sleep_ms(50);

   resp = i2c_smbus_read_word_data(file, 0x03);
   resp = revert(resp);

   write(file, buf, 3);
   hardware_sleep_ms(50);

   resc = i2c_smbus_read_word_data(file, 0x04);
   resc = revert(resc);

       u32 val32 = resv;
       val32 = (val32>>3);
       float amp = resc*0.1;
       float power = resp*2.0/1000.0f;
       float volt = 1000.0*power/amp;
     log_line("V: %f, %u, C mA: %f, P: %f", volt, val32, amp, power);

      hardware_sleep_ms(500);
   }
   log_line("\nEnded\n");
   exit(0);
}
