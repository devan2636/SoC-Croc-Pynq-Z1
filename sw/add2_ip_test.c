#include <stdint.h>

#include "print.h"
#include "simple_alu.h"
#include "uart.h"

int main(void) {
    uart_init();

    const uint16_t patterns[] = {0x0000u, 0x0001u, 0x0010u, 0x1234u, 0x7FFEu, 0xFFFFu};
    const uint32_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    uint32_t fail_count = 0;

    printf("\r\nADD2 IP test (16-bit)\r\n");
    printf("IN  offset: 0x%02x\r\n", (unsigned int)ADD2_INPUT_OFFSET);
    printf("OUT offset: 0x%02x\r\n", (unsigned int)ADD2_OUTPUT_OFFSET);
    printf("Format: HEX + DEC(unsigned/signed)\r\n");

    for (uint32_t i = 0; i < num_patterns; ++i) {
        uint16_t in = patterns[i];
        uint16_t expected = (uint16_t)(in + 2u);

        add2_input_write(in);

        uint16_t in_rb = add2_input_read();
        uint16_t out = add2_output_read();

         printf("IDX=%02u IN=0x%04x(%u/%d) IN_RB=0x%04x(%u/%d) OUT=0x%04x(%u/%d) EXP=0x%04x(%u/%d) ",
               (unsigned int)i,
               (unsigned int)in,
             (unsigned int)in,
             (int)((int16_t)in),
               (unsigned int)in_rb,
             (unsigned int)in_rb,
             (int)((int16_t)in_rb),
               (unsigned int)out,
             (unsigned int)out,
             (int)((int16_t)out),
             (unsigned int)expected,
             (unsigned int)expected,
             (int)((int16_t)expected));

        if ((in_rb != in) || (out != expected)) {
            fail_count++;
            printf("-> FAIL\r\n");
        } else {
            printf("-> OK\r\n");
        }
    }

    if (fail_count == 0) {
        printf("PASS: ADD2 IP valid untuk semua pola.\r\n");
    } else {
        printf("FAIL: jumlah error = %u\r\n", (unsigned int)fail_count);
    }

    uart_write_flush();

    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
