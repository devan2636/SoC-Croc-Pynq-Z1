#include "timer.h"
#include "gpio.h"
#include "uart.h"
#include "print.h"
#include "util.h"

int main() {
    uart_init(); // setup the uart peripheral
    printf("\rTest LED Blink LD3 LD2 LD1 LD0\n\r");
    uart_write_flush(); // wait until uart has finished sending

    // Set LD3..LD0 as outputs.
    gpio_set_direction(0x000F, 0x000F);
    gpio_enable(0x000F);

    const uint32_t leds[] = {0x8, 0x4, 0x2, 0x1};
    const uint32_t delay_ms = 200;

    while (1) {
        for (uint32_t i = 0; i < 4; ++i) {
            gpio_write(leds[i]);
            sleep_ms(delay_ms);
        }
    }

    return 0;
}