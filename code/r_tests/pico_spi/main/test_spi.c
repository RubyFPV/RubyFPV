
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#define BUF_LEN         10

#define SPI_CLK  2
#define SPI_CS   5
#define SPI_MOSI 4
#define SPI_MISO 3

void printbuf(uint8_t buf[], size_t len) {
    int i;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 15)
            printf("%02x\n", buf[i]);
        else
            printf("%02x ", buf[i]);
    }

    // append trailing newline if there isn't one
    if (i % 16) {
        putchar('\n');
    }
}


int main()
{
    // Enable UART so we can print
    stdio_init_all();

   const uint LED_PIN = PICO_DEFAULT_LED_PIN;
   gpio_init(LED_PIN);
   gpio_set_dir(LED_PIN, GPIO_OUT);
   int led = 0;
   long lTimeLast = to_us_since_boot(get_absolute_time())/1000;
   int k = 0;
   while ( k<16 )
   {
    long lTime = to_us_since_boot(get_absolute_time())/1000;
     if ( lTime > lTimeLast + 200 )
      {
         lTimeLast = lTime;
         led = 1 - led;
         gpio_put(LED_PIN, led);
         printf("ld %d\n", led);
         k++;
      }
   }
   printf("SPI pins: %d, %d, %d, %d", SPI_CLK, SPI_CS, SPI_MOSI, SPI_MISO);

   printf("\nStart...\n");
    printf("SPI slave example (%d buffer)\n", BUF_LEN);

    // Enable SPI 0 at 0.5 MHz and connect to GPIOs
    spi_init(spi_default, 500 * 1000);
    spi_set_slave(spi_default, true);
    gpio_set_function(SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CS, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(SPI_MOSI, SPI_MISO, SPI_CLK, SPI_CS, GPIO_FUNC_SPI));

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    while(true)
    {
        out_buf[0] = in_buf[0]+10;
        spi_write_read_blocking(spi_default, out_buf, in_buf, 1);

        // Write to stdio whatever came in on the MOSI line.
        printf("SPI slave received: %d from the MOSI line\n", in_buf[0]);
    }
}