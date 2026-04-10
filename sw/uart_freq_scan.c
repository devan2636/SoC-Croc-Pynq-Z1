// This program is free software: you can redistribute it and/or modify
// Modified by Devandri Suherman on 2024-06-01.
// Master student's of Electrical Engineering and Informatics
// Bandung Institute of Technology

#include <stdint.h>

#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#define LED_MASK 0xFu

static void uart_set_divisor_raw(uint16_t divisor) {
    uint8_t dlo = (uint8_t)(divisor & 0xFFu);
    uint8_t dhi = (uint8_t)((divisor >> 8) & 0xFFu);

    *reg8(UART_BASE_ADDR, UART_INTR_ENABLE_REG_OFFSET) = 0x00;
    *reg8(UART_BASE_ADDR, UART_LINE_CONTROL_REG_OFFSET) = 0x80; // DLAB=1
    *reg8(UART_BASE_ADDR, UART_DLAB_LSB_REG_OFFSET) = dlo;
    *reg8(UART_BASE_ADDR, UART_DLAB_MSB_REG_OFFSET) = dhi;
    *reg8(UART_BASE_ADDR, UART_LINE_CONTROL_REG_OFFSET) = 0x03; // 8N1
    *reg8(UART_BASE_ADDR, UART_FIFO_CONTROL_REG_OFFSET) = 0x07; // enable+clear FIFO
    *reg8(UART_BASE_ADDR, UART_MODEM_CONTROL_REG_OFFSET) = 0x00; // no autoflow
}

static void puts_crlf(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
    }
    uart_write('\r');
    uart_write('\n');
}

typedef struct {
    const char *label;
    uint16_t divisor;
    uint32_t led;
} baud_probe_t;

static const baud_probe_t k_probes[] = {
    {"CLK=70MHz BAUD=115200 DIV=38", 38, 0x1u},
    {"CLK=75MHz BAUD=115200 DIV=41", 41, 0x2u},
    {"CLK=80MHz BAUD=115200 DIV=43", 43, 0x3u},
    {"CLK=85MHz BAUD=115200 DIV=46", 46, 0x4u},
    {"CLK=90MHz BAUD=115200 DIV=49", 49, 0x5u},
    {"CLK=95MHz BAUD=115200 DIV=52", 52, 0x6u},
    {"CLK=100MHz BAUD=115200 DIV=54", 54, 0x7u},
    {"CLK=105MHz BAUD=115200 DIV=57", 57, 0x8u},
    {"CLK=110MHz BAUD=115200 DIV=60", 60, 0x9u},
    {"CLK=115MHz BAUD=115200 DIV=63", 63, 0xAu},
    {"CLK=120MHz BAUD=115200 DIV=65", 65, 0xBu},
};

int main(void) {
    gpio_set_direction(LED_MASK, LED_MASK);
    gpio_enable(LED_MASK);
    gpio_write(0x0u);

    while (1) {
        for (uint32_t i = 0; i < (sizeof(k_probes) / sizeof(k_probes[0])); ++i) {
            uart_set_divisor_raw(k_probes[i].divisor);
            gpio_write(k_probes[i].led);

            for (int r = 0; r < 6; ++r) {
                puts_crlf("=== UART FREQ SCAN ===");
                puts_crlf(k_probes[i].label);
                puts_crlf("If this line is readable, this divisor matches your SoC clock.");
                uart_write_flush();
                sleep_ms(300);
            }
        }
    }

    return 0;
}
