#include <stdint.h>
#include "print.h"
#include "uart.h"

#define FIR_TAPS 16
#define TEST_SAMPLES 60

// Koefisien Default (MATLAB Q15)
static const int16_t fir_coeff[FIR_TAPS] = {
    -1586,  1037,  2173,   528, -2496, -1370,  6023, 13670, 
    13670,  6023, -1370, -2496,   528,  2173,  1037, -1586
};

// Data Input Gaussian Eksak
static const int16_t test_input[TEST_SAMPLES] = {
    -538,  867,  976,  337, -996, -523, -1297,  917,
     177,  755, -592, 1844, 1817, -124, -1111, -681,
      14,  -60, -661,  306, -409, -1282, -285,  -65,
    1000, -801,   92,  274, -2134,  404, -959, -326,
    2479, 1539,  827, -160, -858,  398, -225, -1315,
     -22, -880, 1109, -464,  665, -530,   41,  155,
    -321, 2146,  800, 1196, 2098,  388, 1045, -1372,
    -958, -2669, -803, -451
};

static int16_t fir_state[FIR_TAPS];

static void fir_clear_state(void) {
    for (int i = 0; i < FIR_TAPS; ++i) fir_state[i] = 0;
}

static int16_t fir_process(int16_t x_n) {
    for (int i = FIR_TAPS - 1; i > 0; --i) fir_state[i] = fir_state[i - 1];
    fir_state[0] = x_n;
    int64_t acc = 0;
    for (int i = 0; i < FIR_TAPS; ++i) acc += (int32_t)fir_coeff[i] * (int32_t)fir_state[i];
    return (int16_t)((int32_t)(acc >> 15));
}

int main(void) {
    uart_init();
    fir_clear_state();

    // Loop Utama: Langsung kirim data ke Plotter
    while (1) {
        for (int i = 0; i < TEST_SAMPLES; i++) {
            int16_t x = test_input[i];
            int16_t y = fir_process(x);

            // Format Plotter: Input[Spasi]Output[Newline]
            printf("%d %d\r\n", (int)x, (int)y);

            // Delay agar grafik tidak terlalu cepat (sesuaikan jika perlu)
            for(volatile int d = 0; d < 10000; d++);
        }
        
        // Jeda antar siklus 60 data
        for(volatile int d = 0; d < 100000; d++);
        fir_clear_state(); // Reset state agar grafik mulai dari transisi lagi
    }

    return 0;
}