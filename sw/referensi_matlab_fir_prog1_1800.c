// Devandri Suherman - 2026
// Bandung Institute of Technology
// Reference FIR filter implementation in C for PYNQ-Z1.
// This program reads batches of input samples and reference outputs from UART,
// processes them through a FIR filter (either in software or hardware), and sends
// the results back over UART for comparison with MATLAB reference outputs.

#include <stdint.h>

#include "config.h"
#include "print.h"
#include "uart.h"
#include "util.h"

#define FIR_TAPS 16
#define BATCH_SIZE 60
#define NUM_BATCHES 30

#define FIR_BASE_ADDR USER_FIR_BASE_ADDR

#define FIR_REG_CTRL         0
#define FIR_REG_STATUS       4
#define FIR_REG_COEFF_IDX    8
#define FIR_REG_COEFF_DATA   12
#define FIR_REG_INPUT_IDX    16
#define FIR_REG_INPUT_DATA   20
#define FIR_REG_OUTPUT_IDX   24
#define FIR_REG_OUTPUT_DATA  28
#define FIR_REG_LENGTH       32

#define FIR_CTRL_START   0x1u
#define FIR_CTRL_CLEAR   0x2u

// Set to 1 only if the currently programmed bitstream includes FIR HW IP.
#define USE_HW_FIR 0
#define MATLAB_PLOT_OFFSET 0

static const int16_t fir_coeff[FIR_TAPS] = {
    -1586, 1037, 2173, 528, -2496, -1370, 6023, 13670,
    13670, 6023, -1370, -2496, 528, 2173, 1037, -1586
};

static int16_t fir_state[FIR_TAPS];
static int16_t input_batch[BATCH_SIZE];
static int16_t ref_batch[BATCH_SIZE];

static void fir_clear_state(void) {
    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_state[i] = 0;
    }
}

static int16_t fir_process(int16_t x_n) {
    for (int i = FIR_TAPS - 1; i > 0; --i) {
        fir_state[i] = fir_state[i - 1];
    }
    fir_state[0] = x_n;

    int64_t acc = 0;
    for (int i = 0; i < FIR_TAPS; ++i) {
        acc += (int32_t)fir_coeff[i] * (int32_t)fir_state[i];
    }

    return (int16_t)((int32_t)(acc >> 15));
}

static void put_str(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
    }
}

static void put_crlf(void) {
    uart_write('\r');
    uart_write('\n');
}

static void put_flush(void) {
    uart_write_flush();
}

static void fir_hw_write_reg(int offset, uint32_t value) {
    *reg32(FIR_BASE_ADDR, offset) = value;
}

static uint32_t fir_hw_read_reg(int offset) {
    return *reg32(FIR_BASE_ADDR, offset);
}

static void fir_hw_clear(void) {
    fir_hw_write_reg(FIR_REG_CTRL, FIR_CTRL_CLEAR);
    fence();
}

static void fir_hw_set_length(uint32_t length) {
    fir_hw_write_reg(FIR_REG_LENGTH, length);
}

static void fir_hw_write_coeffs(void) {
    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_hw_write_reg(FIR_REG_COEFF_IDX, (uint32_t)i);
        fir_hw_write_reg(FIR_REG_COEFF_DATA, (uint32_t)(int32_t)fir_coeff[i]);
    }
    fence();
}

static void fir_hw_write_input_buffer(void) {
    for (int i = 0; i < BATCH_SIZE; ++i) {
        fir_hw_write_reg(FIR_REG_INPUT_IDX, (uint32_t)i);
        fir_hw_write_reg(FIR_REG_INPUT_DATA, (uint32_t)(int32_t)input_batch[i]);
    }
    fence();
}

static void fir_hw_start(void) {
    fir_hw_write_reg(FIR_REG_CTRL, FIR_CTRL_START);
}

static int fir_hw_wait_done_with_timeout(void) {
    for (uint32_t t = 0; t < 20000000u; ++t) {
        if ((fir_hw_read_reg(FIR_REG_STATUS) & (1u << 1)) != 0u) {
            return 1;
        }
    }
    return 0;
}

static int32_t fir_hw_read_output(int index) {
    fir_hw_write_reg(FIR_REG_OUTPUT_IDX, (uint32_t)index);
    fence();
    return (int32_t)(*reg32(FIR_BASE_ADDR, FIR_REG_OUTPUT_DATA));
}

static int is_ws(char c) {
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static int read_int32_uart(int32_t *out) {
    int sign = 1;
    int got_digit = 0;
    int32_t value = 0;
    char c;

    do {
        c = (char)uart_read();
    } while (is_ws(c));

    if (c == '-') {
        sign = -1;
        c = (char)uart_read();
    } else if (c == '+') {
        c = (char)uart_read();
    }

    while ((c >= '0') && (c <= '9')) {
        got_digit = 1;
        value = (value * 10) + (int32_t)(c - '0');
        c = (char)uart_read();
    }

    if (!got_digit) {
        return 0;
    }

    *out = (sign < 0) ? -value : value;
    return 1;
}

static int16_t sat_i16(int32_t x) {
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

static int read_batch_stream(void) {
    int32_t x;
    int32_t y_ref;

    for (int i = 0; i < BATCH_SIZE; ++i) {
        if (!read_int32_uart(&x) || !read_int32_uart(&y_ref)) {
            return 0;
        }
        input_batch[i] = sat_i16(x);
        ref_batch[i] = sat_i16(y_ref);
    }

    return 1;
}

int main(void) {
    uart_init();

    put_crlf();
    put_str("REF FIR STREAM MODE - 30x60");
    put_crlf();
    put_str("send each batch as 60 lines: x y_ref");
    put_crlf();
    put_str("output format: x y_out y_ref_plus_1000");
    put_crlf();
    put_flush();

    while (1) {
#if USE_HW_FIR
        // Program HW FIR once per cycle so coefficients are not reloaded every batch.
        fir_hw_clear();
        fir_hw_write_coeffs();
        fir_hw_set_length(BATCH_SIZE);
    #else
        // Keep FIR state continuous across batches in one cycle.
        fir_clear_state();
#endif

        for (int batch = 0; batch < NUM_BATCHES; ++batch) {
            printf("[READY] batch %d/%d\\r\\n", batch + 1, NUM_BATCHES);
            put_flush();

            if (!read_batch_stream()) {
                printf("ERR: parse batch %d\\r\\n", batch);
                put_flush();
                continue;
            }

#if USE_HW_FIR
            fir_hw_write_input_buffer();
            fir_hw_start();

            if (!fir_hw_wait_done_with_timeout()) {
                printf("ERR timeout batch %d status=0x%08x\\r\\n", batch, (unsigned int)fir_hw_read_reg(FIR_REG_STATUS));
                put_flush();
                continue;
            }

            for (int i = 0; i < BATCH_SIZE; ++i) {
                int16_t x = input_batch[i];
                int16_t y_matlab = ref_batch[i];
                int32_t y_hw = fir_hw_read_output(i);
                printf("%d %d %d\\r\\n", (int)x, (int)y_hw, (int)(y_matlab + MATLAB_PLOT_OFFSET));
            }
#else
            for (int i = 0; i < BATCH_SIZE; ++i) {
                int16_t x = input_batch[i];
                int16_t y_sw = fir_process(x);
                int16_t y_matlab = ref_batch[i];
                printf("%d %d %d\\r\\n", (int)x, (int)y_sw, (int)(y_matlab + MATLAB_PLOT_OFFSET));
            }
#endif

            printf("[DONE] batch %d/%d\\r\\n", batch + 1, NUM_BATCHES);
            put_flush();
        }

        printf("[CYCLE_DONE] 30 batches complete\\r\\n");
        put_flush();
    }

    return 0;
}
