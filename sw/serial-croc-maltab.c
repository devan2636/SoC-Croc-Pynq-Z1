#include <stdint.h>
#include "print.h"
#include "uart.h"

#define FIR_TAPS 16
#define TEST_SAMPLES 60

static const int16_t fir_coeff[FIR_TAPS] = {
    -1586,  1037,  2173,   528, -2496, -1370,  6023, 13670, 
    13670,  6023, -1370, -2496,   528,  2173,  1037, -1586
};

// Data Input Gaussian (x1000)
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

// DATA BARU: Hasil Filter MATLAB (x1000) sebagai referensi
static const int16_t matlab_ref[TEST_SAMPLES] = {
    26,   -59,   -55,    63,   179,   -12,  -224,  -268,
    304,   822,   746,  -257, -1034,  -773,    63,   481,
    225,   263,   925,  1370,   868,  -160,  -978,  -714,
   -185,   253,  -130,  -606,  -712,  -719,  -236,   187,
    157,   -18,  -317,  -527,  -784,  -901,  -118,  1134,
   1826,  1326,   152,  -297,  -377,   -56,  -345,  -773,
   -524,  -108,   363,   337,   -73,  -175,   -75,   378,
    902,  1255,  1396,  1348
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
    
    while (1) {
        fir_clear_state();
        for (int i = 0; i < TEST_SAMPLES; i++) {
            int16_t x = test_input[i];
            int16_t y_croc = fir_process(x);
            int16_t y_matlab = matlab_ref[i];

            // Kirim 3 kolom data: Raw_Input Output_CROC Ref_MATLAB
            // Format: "%d %d %d\r\n"
            printf("%d %d %d\r\n", (int)x, (int)y_croc, (int)y_matlab+2000);

            for(volatile int d = 0; d < 15000; d++); // Delay biar plotter santai
        }
        for(volatile int d = 0; d < 200000; d++); // Jeda sebelum repeat
    }
    return 0;
}