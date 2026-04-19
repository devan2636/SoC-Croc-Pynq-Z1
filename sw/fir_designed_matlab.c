#include <stdint.h>
#include "print.h"
#include "uart.h"

#define FIR_TAPS 16
#define INPUT_BUF_LEN 48
#define TEST_SAMPLES 60

// Koefisien hasil scaling MATLAB (Num * 32768)
static const int16_t fir_coeff_default[FIR_TAPS] = {
    -1586,  1037,  2173,   528, -2496, -1370,  6023, 13670, 
    13670,  6023, -1370, -2496,   528,  2173,  1037, -1586
};

// Data Input Gaussian Eksak (data_asli * 1000) untuk benchmark
static const int16_t test_input_gaussian[TEST_SAMPLES] = {
    -538,  867,  976,  337, -996, -523, -1297,  917,
     177,  755, -592, 1844, 1817, -124, -1111, -681,
      14,  -60, -661,  306, -409, -1282, -285,  -65,
    1000, -801,   92,  274, -2134,  404, -959, -326,
    2479, 1539,  827, -160, -858,  398, -225, -1315,
     -22, -880, 1109, -464,  665, -530,   41,  155,
    -321, 2146,  800, 1196, 2098,  388, 1045, -1372,
    -958, -2669, -803, -451
};

static int16_t fir_coeff[FIR_TAPS];
static int16_t fir_state[FIR_TAPS];

// --- Fungsi Helper Sama Seperti Sebelumnya ---
static void put_str(const char *s) { while (*s) uart_write((uint8_t)*s++); }
static void put_crlf(void) { uart_write('\r'); uart_write('\n'); }

static int read_line(char *buf, int max_len) {
    int len = 0;
    while (1) {
        char c = (char)uart_read();
        if (c == '\r' || c == '\n') { put_crlf(); break; }
        if ((c == '\b' || c == 127) && len > 0) {
            len--; uart_write('\b'); uart_write(' '); uart_write('\b');
            continue;
        }
        if (len < (max_len - 1)) { buf[len++] = c; uart_write((uint8_t)c); }
    }
    buf[len] = '\0';
    return len;
}

static int parse_int32(const char *s, int32_t *out) {
    if (!s || !*s) return 0;
    int sign = 1, idx = 0; int32_t value = 0;
    if (s[idx] == '-') { sign = -1; idx++; }
    else if (s[idx] == '+') { idx++; }
    while (s[idx] != '\0') {
        if (s[idx] < '0' || s[idx] > '9') return 0;
        value = value * 10 + (int32_t)(s[idx] - '0');
        idx++;
    }
    *out = sign * value;
    return 1;
}

static int16_t clamp_i16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static void fir_clear_state(void) {
    for (int i = 0; i < FIR_TAPS; ++i) fir_state[i] = 0;
}

// --- LOGIKA UTAMA DISESUAIKAN ---
static int16_t fir_process_sample(int16_t x_n) {
    // Geser State
    for (int i = FIR_TAPS - 1; i > 0; --i) {
        fir_state[i] = fir_state[i - 1];
    }
    fir_state[0] = x_n;

    // Konvolusi MAC (Multiply-Accumulate)
    int64_t acc = 0;
    for (int i = 0; i < FIR_TAPS; ++i) {
        acc += (int32_t)fir_coeff[i] * (int32_t)fir_state[i];
    }

    // SCALING: Karena koefisien dikali 32768 (Q15), hasil harus dibagi balik
    // Shift kanan 15 adalah pembagian dengan 32768
    int32_t result = (int32_t)(acc >> 15);
    
    return clamp_i16(result);
}

static void run_gaussian_benchmark(void) {
    put_crlf();
    put_str("Running Gaussian Benchmark (60 Samples)...");
    put_crlf();
    
    fir_clear_state();
    for (int i = 0; i < TEST_SAMPLES; i++) {
        int16_t x = test_input_gaussian[i];
        int16_t y = fir_process_sample(x);
        printf("n=%02d | x=%d | y=%d\r\n", i, (int)x, (int)y);
    }
    put_str("Benchmark Selesai.");
    put_crlf();
}

int main(void) {
    uart_init();
    
    // Load koefisien default dari MATLAB ke RAM
    for (int i = 0; i < FIR_TAPS; ++i) {
        fir_coeff[i] = fir_coeff_default[i];
    }
    fir_clear_state();

    char line[INPUT_BUF_LEN];
    put_crlf();
    put_str("CROC FIR Filter - MATLAB Sync Mode");
    put_crlf();

    while (1) {
        put_crlf();
        put_str("Menu: [k] set coeff [g] run gaussian 60 [x] input sample [i] impulse test [c] clear [q] quit");
        put_crlf();
        put_str("Pilih: ");

        if (read_line(line, INPUT_BUF_LEN) <= 0) continue;
        char cmd = line[0];

        switch (cmd) {
            case 'g': case 'G': run_gaussian_benchmark(); break;
            case 'k': case 'K': /* fungsi set_coeff_manual seperti sebelumnya */ break;
            case 'x': case 'X': {
                put_str("x[n] = ");
                read_line(line, INPUT_BUF_LEN);
                int32_t val;
                if(parse_int32(line, &val)) printf("y = %d\r\n", (int)fir_process_sample(clamp_i16(val)));
                break;
            }
            case 'i': case 'I': /* fungsi impulse_test */ break;
            case 'c': case 'C': fir_clear_state(); put_str("State Cleared."); break;
            case 'q': return 0;
        }
    }
}