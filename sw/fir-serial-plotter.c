#include <stdint.h>

#include "config.h"
#include "print.h"
#include "uart.h"
#include "util.h"

#define FIR_TAPS 16
#define FIR_SAMPLES 160
#define INPUT_BUF_LEN 32

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

static const int16_t fir_coeff_default[FIR_TAPS] = {
    -1586,  1037,  2173,   528, -2496, -1370,  6023, 13670,
    13670,  6023, -1370, -2496,   528,  2173,  1037, -1586
};

static int16_t fir_coeff[FIR_TAPS];
static int16_t fir_input[FIR_SAMPLES];
static int16_t fir_state_ps[FIR_TAPS];

static uint32_t rng_state = 0x12345678u;

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

static int read_line(char *buf, int max_len) {
    int len = 0;
    while (1) {
        char c = (char)uart_read();

        if (c == '\r' || c == '\n') {
            put_crlf();
            break;
        }

        if ((c == '\b' || c == 127) && len > 0) {
            len--;
            uart_write('\b');
            uart_write(' ');
            uart_write('\b');
            continue;
        }

        if (len < (max_len - 1)) {
            buf[len++] = c;
            uart_write((uint8_t)c);
        }
    }

    buf[len] = '\0';
    return len;
}

static int parse_int32(const char *s, int32_t *out) {
    if (!s || !*s) {
        return 0;
    }

    int sign = 1;
    int idx = 0;
    int32_t value = 0;

    if (s[idx] == '-') {
        sign = -1;
        idx++;
    } else if (s[idx] == '+') {
        idx++;
    }

    if (s[idx] == '\0') {
        return 0;
    }

    while (s[idx] != '\0') {
        char c = s[idx];
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10 + (int32_t)(c - '0');
        idx++;
    }

    *out = sign * value;
    return 1;
}

static int16_t clamp_i16(int32_t value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
}

static void fir_sw_clear_state(void) {
    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_state_ps[i] = 0;
    }
}

static int16_t fir_sw_process_sample(int16_t x_n) {
    for (int i = FIR_TAPS - 1; i > 0; --i) {
        fir_state_ps[i] = fir_state_ps[i - 1];
    }
    fir_state_ps[0] = x_n;

    int64_t acc = 0;
    for (int i = 0; i < FIR_TAPS; ++i) {
        acc += (int32_t)fir_coeff[i] * (int32_t)fir_state_ps[i];
    }

    return clamp_i16((int32_t)(acc >> 15));
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static int16_t rng_sample(void) {
    uint32_t raw = rng_next() >> 16;
    int32_t value = (int32_t)(raw % 2001u) - 1000;
    return (int16_t)value;
}

static void generate_random_input(void) {
    for (int i = 0; i < FIR_SAMPLES; ++i) {
        fir_input[i] = rng_sample();
    }
}

static void print_coeffs(void) {
    put_crlf();
    put_str("Koefisien FIR saat ini:");
    put_crlf();
    for (int i = 0; i < FIR_TAPS; ++i) {
        printf("h[%02d] = %d\r\n", i, (int)fir_coeff[i]);
    }
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
    for (int i = 0; i < FIR_SAMPLES; ++i) {
        fir_hw_write_reg(FIR_REG_INPUT_IDX, (uint32_t)i);
        fir_hw_write_reg(FIR_REG_INPUT_DATA, (uint32_t)(int32_t)fir_input[i]);
    }
    fence();
}

static void fir_hw_start(void) {
    fir_hw_write_reg(FIR_REG_CTRL, FIR_CTRL_START);
}

static void fir_hw_wait_done(void) {
    while ((fir_hw_read_reg(FIR_REG_STATUS) & (1u << 1)) == 0u) {
        ;
    }
}

static int32_t fir_hw_read_output(int index) {
    fir_hw_write_reg(FIR_REG_OUTPUT_IDX, (uint32_t)index);
    fence();
    return (int32_t)(*reg32(FIR_BASE_ADDR, FIR_REG_OUTPUT_DATA));
}

static int run_compare(void) {
    fir_sw_clear_state();
    fir_hw_clear();
    fir_hw_write_coeffs();
    fir_hw_set_length(FIR_SAMPLES);
    fir_hw_write_input_buffer();
    fir_hw_start();
    fir_hw_wait_done();

    int mismatches = 0;

    put_crlf();
    put_str("n  x[n]  y_hw  y_ps");
    put_crlf();

    for (int i = 0; i < FIR_SAMPLES; ++i) {
        int16_t x = fir_input[i];
        int16_t y_ps = fir_sw_process_sample(x);
        int32_t hw = fir_hw_read_output(i);
        int32_t ps = (int32_t)y_ps;

        if (hw != ps) {
            mismatches++;
        }

        printf("%03d %d %d %d\r\n", i, (int)x, (int)hw, (int)ps);
    }

    put_crlf();
    printf("SUM: total=%d match=%d mismatch=%d\r\n",
           FIR_SAMPLES,
           FIR_SAMPLES - mismatches,
           mismatches);

    if (mismatches == 0) {
        put_str("STATUS: MATCH");
    } else {
        put_str("STATUS: MISMATCH");
    }
    put_crlf();

    return mismatches;
}

static void set_coeff_manual(void) {
    char line[INPUT_BUF_LEN];
    put_crlf();
    put_str("Masukkan 16 koefisien FIR (integer Q15):");
    put_crlf();

    for (int i = 0; i < FIR_TAPS; ++i) {
        printf("h[%02d] = ", i);
        if (read_line(line, INPUT_BUF_LEN) <= 0) {
            i--;
            continue;
        }

        int32_t value = 0;
        if (!parse_int32(line, &value)) {
            put_str("Input tidak valid, ulangi.");
            put_crlf();
            i--;
            continue;
        }

        fir_coeff[i] = clamp_i16(value);
    }

    put_str("Koefisien tersimpan.");
    put_crlf();
}

int main(void) {
    uart_init();

    // Make startup text visible before entering any blocking read path.
    put_str("\r\n");
    put_flush();

    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_coeff[i] = fir_coeff_default[i];
    }
    generate_random_input();
    fir_sw_clear_state();

    char line[INPUT_BUF_LEN];

    put_crlf();
    put_str("FIR HW/PS compare");
    put_crlf();
    put_str("k coeff, g input, r run, p print");
    put_crlf();
    put_flush();

    while (1) {
        put_crlf();
        put_str("Menu: k p g r c q");
        put_crlf();
        put_str("Pilih menu: ");
        put_flush();

        if (read_line(line, INPUT_BUF_LEN) <= 0) {
            continue;
        }

        char cmd = line[0];
        switch (cmd) {
            case 'k':
            case 'K':
                set_coeff_manual();
                break;

            case 'p':
            case 'P':
                print_coeffs();
                break;

            case 'g':
            case 'G':
                generate_random_input();
                put_str("Input siap.");
                put_crlf();
                break;

            case 'r':
            case 'R': {
                int mismatches = run_compare();
                if (mismatches == 0) {
                    put_str("Hasil hardware cocok dengan PS.");
                } else {
                    put_str("Ada selisih hardware vs PS.");
                }
                put_crlf();
                break;
            }

            case 'c':
            case 'C':
                fir_hw_clear();
                fir_sw_clear_state();
                put_str("State cleared.");
                put_crlf();
                break;

            case 'q':
            case 'Q':
                put_str("Keluar.");
                put_crlf();
                return 0;

            default:
                put_str("Menu tidak dikenal.");
                put_crlf();
                break;
        }
    }

    return 0;
}