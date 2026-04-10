#include <stdint.h>

#include "print.h"
#include "simple_alu.h"
#include "uart.h"

#define INPUT_BUF_LEN 32
#define DIV_ZERO_SENTINEL 0xCAFEF00D
#define ALU_DEBUG_CHECKPOINTS 0

static void put_str(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
    }
}

static void put_crlf(void) {
    uart_write('\r');
    uart_write('\n');
}

#if ALU_DEBUG_CHECKPOINTS
#define DBG(msg)            \
    do {                    \
        put_str("[DBG] "); \
        put_str(msg);       \
        put_crlf();         \
        uart_write_flush(); \
    } while (0)
#else
#define DBG(msg) do { } while (0)
#endif

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
            // Erase last char on terminal
            uart_write('\b');
            uart_write(' ');
            uart_write('\b');
            continue;
        }

        if (len < (max_len - 1)) {
            buf[len++] = c;
            uart_write((uint8_t)c); // echo
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

static int32_t q16_16_to_milli(int32_t q) {
    // Convert fixed-point Q16.16 to signed milli-unit for display.
    int64_t scaled = (int64_t)q * 1000;
    if (scaled >= 0) {
        return (int32_t)((scaled + 32768) >> 16);
    }
    return (int32_t)((scaled - 32768) >> 16);
}

static void print_q16_16(const char *label, int32_t q) {
    int32_t milli = q16_16_to_milli(q);
    int32_t abs_milli = (milli < 0) ? -milli : milli;
    int32_t int_part = abs_milli / 1000;
    int32_t frac_part = abs_milli % 1000;

    printf("%s raw=0x%08x dec=", label, (unsigned int)q);
    if (milli < 0) {
        uart_write('-');
    }
    printf("%d.%03d", (int)int_part, (int)frac_part);
    put_crlf();
}

static int16_t clamp_to_q8_8(int32_t n) {
    if (n > 127) {
        n = 127;
    } else if (n < -128) {
        n = -128;
    }
    return (int16_t)(n << 8);
}

int main(void) {
    uart_init();
    DBG("uart_init done");

    char line[INPUT_BUF_LEN];

    put_crlf();
    put_str("ALU FULL (HW+SW) - operasi: + - *");
    put_crlf();
    put_str("Input berupa integer, akan dikonversi ke Q8.8.");
    put_crlf();
    put_str("Rentang aman input: -128 s.d. 127");
    put_crlf();

    while (1) {
        put_crlf();
        put_str("Pilih operasi [+ - *] (q untuk keluar): ");
        int len = read_line(line, INPUT_BUF_LEN);
        if (len <= 0) {
            continue;
        }
        DBG("operator line received");

        char op = line[0];
        if (op == 'q' || op == 'Q') {
            put_str("Keluar dari ALU FULL.");
            put_crlf();
            break;
        }

        if (!(op == '+' || op == '-' || op == '*')) {
            put_str("Operasi tidak valid.");
            put_crlf();
            continue;
        }

        put_str("Masukkan A (integer): ");
        read_line(line, INPUT_BUF_LEN);
        int32_t a_int = 0;
        if (!parse_int32(line, &a_int)) {
            put_str("Input A tidak valid.");
            put_crlf();
            continue;
        }
        DBG("operand A parsed");

        put_str("Masukkan B (integer): ");
        read_line(line, INPUT_BUF_LEN);
        int32_t b_int = 0;
        if (!parse_int32(line, &b_int)) {
            put_str("Input B tidak valid.");
            put_crlf();
            continue;
        }
        DBG("operand B parsed");

        int16_t a_q = clamp_to_q8_8(a_int);
        int16_t b_q = clamp_to_q8_8(b_int);

        int32_t hw = 0;
        int32_t sw = 0;
        DBG("starting HW/SW compute");

        switch (op) {
            case '+':
                hw = add_hw(a_q, b_q);
                sw = add_sw(a_q, b_q);
                break;
            case '-':
                hw = sub_hw(a_q, b_q);
                sw = sub_sw(a_q, b_q);
                break;
            case '*':
                hw = mult_hw(a_q, b_q);
                sw = mult_sw(a_q, b_q);
                break;
            default:
                break;
        }
        DBG("HW/SW compute finished");

        put_crlf();
        printf("A=%d (Q8.8=0x%04x), B=%d (Q8.8=0x%04x)",
               (int)a_int,
               (unsigned int)(uint16_t)a_q,
               (int)b_int,
               (unsigned int)(uint16_t)b_q);
        put_crlf();

        if ((uint32_t)hw == DIV_ZERO_SENTINEL && (uint32_t)sw == DIV_ZERO_SENTINEL) {
            put_str("HW: division by zero");
            put_crlf();
            put_str("SW: division by zero");
            put_crlf();
        } else {
            print_q16_16("HW", hw);
            print_q16_16("SW", sw);
        }

        if (hw == sw) {
            put_str("STATUS: MATCH");
        } else {
            put_str("STATUS: MISMATCH");
        }
        put_crlf();
        uart_write_flush();
    }

    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
