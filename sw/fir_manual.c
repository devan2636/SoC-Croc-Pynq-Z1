#include <stdint.h>

#include "print.h"
#include "uart.h"

#define FIR_TAPS 16
#define INPUT_BUF_LEN 48

static int16_t fir_coeff[FIR_TAPS];
static int16_t fir_state[FIR_TAPS];

static void put_str(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
    }
}

static void put_crlf(void) {
    uart_write('\r');
    uart_write('\n');
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

static int16_t clamp_i16(int32_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

static int32_t clamp_i32_from_i64(int64_t v) {
    if (v > 2147483647LL) {
        return 2147483647;
    }
    if (v < -2147483648LL) {
        return -2147483648LL;
    }
    return (int32_t)v;
}

static void fir_clear_state(void) {
    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_state[i] = 0;
    }
}

static int32_t fir_process_sample(int16_t x_n) {
    for (int i = FIR_TAPS - 1; i > 0; --i) {
        fir_state[i] = fir_state[i - 1];
    }
    fir_state[0] = x_n;

    int64_t acc = 0;
    for (int i = 0; i < FIR_TAPS; ++i) {
        acc += (int32_t)fir_coeff[i] * (int32_t)fir_state[i];
    }

    return clamp_i32_from_i64(acc);
}

static void print_coeff(void) {
    put_crlf();
    put_str("Koefisien FIR saat ini:");
    put_crlf();
    for (int i = 0; i < FIR_TAPS; ++i) {
        printf("h[%02d] = %d\r\n", i, (int)fir_coeff[i]);
    }
}

static void set_coeff_manual(void) {
    char line[INPUT_BUF_LEN];

    put_crlf();
    put_str("Input 16 koefisien FIR (int16):");
    put_crlf();

    for (int i = 0; i < FIR_TAPS; ++i) {
        while (1) {
            printf("h[%02d] = ", i);
            read_line(line, INPUT_BUF_LEN);

            int32_t v = 0;
            if (!parse_int32(line, &v)) {
                put_str("Input tidak valid, ulangi.");
                put_crlf();
                continue;
            }

            fir_coeff[i] = clamp_i16(v);
            break;
        }
    }

    fir_clear_state();
    put_str("Koefisien tersimpan. State FIR di-reset.");
    put_crlf();
}

static void run_single_sample(void) {
    char line[INPUT_BUF_LEN];

    put_str("x[n] = ");
    read_line(line, INPUT_BUF_LEN);

    int32_t x = 0;
    if (!parse_int32(line, &x)) {
        put_str("Input sample tidak valid.");
        put_crlf();
        return;
    }

    int16_t x_i16 = clamp_i16(x);
    int32_t y = fir_process_sample(x_i16);

    printf("x=%d -> y=%d (0x%08x)\r\n", (int)x_i16, (int)y, (unsigned int)y);
}

static void run_impulse_test(void) {
    char line[INPUT_BUF_LEN];

    put_str("Jumlah sampel impulse test = ");
    read_line(line, INPUT_BUF_LEN);

    int32_t count = 0;
    if (!parse_int32(line, &count) || count <= 0 || count > 128) {
        put_str("Jumlah sampel tidak valid (1..128).");
        put_crlf();
        return;
    }

    fir_clear_state();
    put_str("Impulse test dimulai (x0=1, sisanya 0)");
    put_crlf();

    for (int32_t n = 0; n < count; ++n) {
        int16_t x = (n == 0) ? 1 : 0;
        int32_t y = fir_process_sample(x);
        printf("n=%02d x=%d y=%d\r\n", (int)n, (int)x, (int)y);
    }
}

int main(void) {
    uart_init();

    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_coeff[i] = 0;
    }
    fir_clear_state();

    char line[INPUT_BUF_LEN];

    put_crlf();
    put_str("FIR Manual 16-Tap (Program Terpisah)");
    put_crlf();
    put_str("Semua koefisien diinput manual via UART.");
    put_crlf();

    while (1) {
        put_crlf();
        put_str("Menu: [k] set coeff  [p] print coeff  [x] input sample  [i] impulse test  [c] clear state  [q] keluar");
        put_crlf();
        put_str("Pilih menu: ");

        int len = read_line(line, INPUT_BUF_LEN);
        if (len <= 0) {
            continue;
        }

        char cmd = line[0];
        if (cmd == 'q' || cmd == 'Q') {
            put_str("Keluar dari FIR Manual.");
            put_crlf();
            break;
        }

        switch (cmd) {
            case 'k':
            case 'K':
                set_coeff_manual();
                break;
            case 'p':
            case 'P':
                print_coeff();
                break;
            case 'x':
            case 'X':
                run_single_sample();
                break;
            case 'i':
            case 'I':
                run_impulse_test();
                break;
            case 'c':
            case 'C':
                fir_clear_state();
                put_str("State FIR di-reset.");
                put_crlf();
                break;
            default:
                put_str("Menu tidak valid.");
                put_crlf();
                break;
        }

        uart_write_flush();
    }

    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
