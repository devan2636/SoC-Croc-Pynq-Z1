#include "timer.h"
#include "gpio.h"
#include "uart.h"
#include "print.h"
#include "util.h"

#define LED_MASK 0x000F

#define BTN0 (1u << 0)
#define BTN1 (1u << 1)
#define BTN2 (1u << 2)
#define BTN3 (1u << 3)

typedef enum {
    MODE_STOP = 0,
    MODE_LD3_TO_LD0,
    MODE_LD0_TO_LD3,
    MODE_RANDOM
} led_mode_t;

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int main() {
    uart_init();
    printf("\rBTN LED controller ready\n\r");
    printf("BTN0: stop, BTN1: LD3->LD0, BTN2: LD0->LD3, BTN3: random\n\r");
    uart_write_flush();

    // LD3..LD0 use GPIO[3:0].
    gpio_set_direction(LED_MASK, LED_MASK);
    gpio_enable(LED_MASK);

    led_mode_t mode = MODE_STOP;
    uint32_t idx = 0;
    uint32_t rng_state = (uint32_t)get_mcycle();
    if (rng_state == 0) {
        rng_state = 0x1A2B3C4D;
    }

    const uint32_t seq_ld3_to_ld0[4] = {0x8, 0x4, 0x2, 0x1};
    const uint32_t seq_ld0_to_ld3[4] = {0x1, 0x2, 0x4, 0x8};

    while (1) {
        uint32_t btn = gpio_read() & LED_MASK;

        // Priority follows the specification order.
        if (btn & BTN0) {
            mode = MODE_STOP;
        } else if (btn & BTN1) {
            mode = MODE_LD3_TO_LD0;
        } else if (btn & BTN2) {
            mode = MODE_LD0_TO_LD3;
        } else if (btn & BTN3) {
            mode = MODE_RANDOM;
        }

        switch (mode) {
            case MODE_STOP:
                gpio_write(0x0);
                break;
            case MODE_LD3_TO_LD0:
                gpio_write(seq_ld3_to_ld0[idx]);
                idx = (idx + 1) & 0x3;
                break;
            case MODE_LD0_TO_LD3:
                gpio_write(seq_ld0_to_ld3[idx]);
                idx = (idx + 1) & 0x3;
                break;
            case MODE_RANDOM: {
                uint32_t r = xorshift32(&rng_state);
                gpio_write(1u << (r & 0x3));
                break;
            }
            default:
                gpio_write(0x0);
                mode = MODE_STOP;
                break;
        }

        sleep_ms(150);
    }

    return 0;
}