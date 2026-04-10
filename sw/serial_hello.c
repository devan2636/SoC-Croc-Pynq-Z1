#include "uart.h"
#include "print.h"
#include "timer.h"
#include "gpio.h"

#define LD0_TX (1u << 0)
#define LD1_RX (1u << 1)
#define LED_MASK (LD0_TX | LD1_RX)

static void serial_println(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
    }
    uart_write('\r');
    uart_write('\n');
}

int main() {
    uart_init();

    // LD0 and LD1 are driven by GPIO[1:0].
    gpio_set_direction(LED_MASK, LED_MASK);
    gpio_enable(LED_MASK);
    gpio_write(0x0000);

    while (1) {
        gpio_write(LD0_TX); // LD0 on while transmitting.
        serial_println("Hello, world!");
        serial_println("Type a key to test RX echo...");
        uart_write_flush();
        gpio_write(0x0000); // LEDs off after TX completes.

        if (uart_read_ready()) {
            uint8_t c = uart_read();
            serial_println("RX byte:");
            uart_write(c);
            uart_write('\r');
            uart_write('\n');
            uart_write_flush();
            gpio_write(LD1_RX); // LD1 on for RX activity.
            sleep_ms(50);
            gpio_write(0x0000);
        }

        sleep_ms(500);
    }

    return 0;
}
