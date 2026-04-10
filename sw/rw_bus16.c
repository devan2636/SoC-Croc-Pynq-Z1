// This program is free software: you can redistribute it and/or modify
// Modified by Devandri Suherman on 2024-06-01.
// Master student's of Electrical Engineering and Informatics
// Bandung Institute of Technology

#include <stdint.h>

#include "print.h"
#include "simple_alu.h"
#include "timer.h"
#include "uart.h"

static uint16_t lfsr_next(uint16_t state) {
    uint16_t bit = (uint16_t)(((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1u);
    return (uint16_t)((state >> 1) | (bit << 15));
}

static void print_hex16(uint16_t value) {
    printf("%x", (unsigned int)value);
}

static void print_step_prefix(const char *label, uint32_t index) {
    printf(label);
    printf(" 0x%x: ", (unsigned int)index);
}

static void delay_cycles(volatile uint32_t cycles) {
    while (cycles--) {
        asm volatile("nop");
    }
}

int main(void) {
    uart_init();

    uint16_t wr_log[16];
    uint16_t rd_log[16];
    uint8_t ok_log[16];
    uint32_t case_count = 0;

    printf("\r\n16-bit bus register RW test\r\n");
    printf("Offset 0x%x / USER_REG16\r\n", (unsigned int)USER_REG16_OFFSET);
    uart_write_flush();

    const uint16_t patterns[] = {0x0000u, 0x1234u, 0xABCDu, 0xFFFFu, 0x00F0u, 0x5AA5u};
    const uint32_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    uint32_t fail_count = 0;

    for (uint32_t i = 0; i < num_patterns; ++i) {
        uint16_t wr = patterns[i];
        print_step_prefix("STEP", i);
        printf("before write\r\n");
        uart_write_flush();

        print_step_prefix("STEP", i);
        printf("write 0x");
        print_hex16(wr);
        printf("\r\n");
        user_reg16_write(wr);
        print_step_prefix("STEP", i);
        printf("after write\r\n");
        uart_write_flush();
        delay_cycles(2000);

        print_step_prefix("STEP", i);
        printf("before read\r\n");
        uart_write_flush();

        uint16_t rd = user_reg16_read();
        print_step_prefix("STEP", i);
        printf("read  0x");
        print_hex16(rd);
        printf("\r\n");

        if (rd != wr) {
            ++fail_count;
            print_step_prefix("STEP", i);
            printf("FAIL\r\n");
            ok_log[case_count] = 0;
        } else {
            print_step_prefix("STEP", i);
            printf("OK\r\n");
            ok_log[case_count] = 1;
        }
        wr_log[case_count] = wr;
        rd_log[case_count] = rd;
        case_count++;
        uart_write_flush();
    }

    uint16_t lfsr = 0xACE1u;
    for (uint32_t i = 0; i < 8; ++i) {
        lfsr = lfsr_next(lfsr);
        uint16_t wr = lfsr;
        print_step_prefix("RAND", i);
        printf("before write\r\n");
        uart_write_flush();

        print_step_prefix("RAND", i);
        printf("write 0x");
        print_hex16(wr);
        printf("\r\n");
        user_reg16_write(wr);
        print_step_prefix("RAND", i);
        printf("after write\r\n");
        uart_write_flush();
        delay_cycles(2000);

        print_step_prefix("RAND", i);
        printf("before read\r\n");
        uart_write_flush();

        uint16_t rd = user_reg16_read();
        print_step_prefix("RAND", i);
        printf("read  0x");
        print_hex16(rd);
        printf("\r\n");

        if (rd != wr) {
            ++fail_count;
            print_step_prefix("RAND", i);
            printf("FAIL\r\n");
            ok_log[case_count] = 0;
        } else {
            print_step_prefix("RAND", i);
            printf("OK\r\n");
            ok_log[case_count] = 1;
        }
        wr_log[case_count] = wr;
        rd_log[case_count] = rd;
        case_count++;
        uart_write_flush();
    }

    printf("\r\nIDX | WR | RD | OK/FAIL\r\n");
    for (uint32_t i = 0; i < case_count; ++i) {
        printf("%02u | 0x%04x | 0x%04x | %s\r\n",
               (unsigned int)i,
               (unsigned int)wr_log[i],
               (unsigned int)rd_log[i],
               ok_log[i] ? "OK" : "FAIL");
    }

    if (fail_count == 0) {
        printf("PASS: semua read/write register 16-bit sesuai.\r\n");
    } else {
        printf("FAIL: mismatch terdeteksi.\r\n");
    }
    uart_write_flush();

    while (1) {
        sleep_ms(1000);
    }

    return 0;
}
