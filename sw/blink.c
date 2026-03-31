#include "timer.h"
#include "gpio.h"
#include "uart.h"
#include "print.h"
#include "util.h"

int main() {
    uart_init(); // setup the uart peripheral
    printf("Test LED Blink 123456789\n");
    uart_write_flush(); // wait until uart has finished sending

    // Set GPIO[0] as output
    gpio_set_direction(0xFFFF, 0x0001);
    gpio_enable(0x0001);

    while (1) {
        gpio_write(0x1);   // LED ON
        sleep_ms(200);

        gpio_write(0x0);   // LED OFF
        sleep_ms(200);
    }

    return 0;
}