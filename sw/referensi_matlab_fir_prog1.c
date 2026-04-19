#include <stdint.h>

#include "config.h"
#include "print.h"
#include "uart.h"
#include "util.h"

#define FIR_TAPS 16
#define TEST_SAMPLES 60

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
#define FIR_DEBUG_CHECKPOINTS 1
// Set to 1 only if the currently programmed bitstream includes FIR HW IP.
#define USE_HW_FIR 0
#define MATLAB_PLOT_OFFSET 1000

static const int16_t fir_coeff[FIR_TAPS] = {
    -1586, 1037, 2173, 528, -2496, -1370, 6023, 13670,
    13670, 6023, -1370, -2496, 528, 2173, 1037, -1586
};

static const int16_t test_input[TEST_SAMPLES] = {
    -538, 867, 976, 337, -996, -523, -1297, 917,
    177, 755, -592, 1844, 1817, -124, -1111, -681,
    14, -60, -661, 306, -409, -1282, -285, -65,
    1000, -801, 92, 274, -2134, 404, -959, -326,
    2479, 1539, 827, -160, -858, 398, -225, -1315,
    -22, -880, 1109, -464, 665, -530, 41, 155,
    -321, 2146, 800, 1196, 2098, 388, 1045, -1372,
    -958, -2669, -803, -451
};

static const int16_t matlab_ref[TEST_SAMPLES] = {
    26, -59, -55, 63, 179, -12, -224, -268,
    304, 822, 746, -257, -1034, -773, 63, 481,
    225, 263, 925, 1370, 868, -160, -978, -714,
    -185, 253, -130, -606, -712, -719, -236, 187,
    157, -18, -317, -527, -784, -901, -118, 1134,
    1826, 1326, 152, -297, -377, -56, -345, -773,
    -524, -108, 363, 337, -73, -175, -75, 378,
    902, 1255, 1396, 1348
};

static int16_t fir_state[FIR_TAPS];

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

#if FIR_DEBUG_CHECKPOINTS
static void dbg(const char *s) {
    put_str("[DBG] ");
    put_str(s);
    put_crlf();
    put_flush();
}
#else
static void dbg(const char *s) {
    (void)s;
}
#endif

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
    for (int i = 0; i < TEST_SAMPLES; ++i) {
        fir_hw_write_reg(FIR_REG_INPUT_IDX, (uint32_t)i);
        fir_hw_write_reg(FIR_REG_INPUT_DATA, (uint32_t)(int32_t)test_input[i]);
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

int main(void) {
    uart_init();

    put_crlf();
    put_str("REF FIR AUTO RUN");
    put_crlf();
    put_str("format: x y_hw y_matlab");
    put_crlf();
    put_flush();

    while (1) {
#if USE_HW_FIR
        dbg("S0 loop start");
        fir_hw_clear();
        dbg("S1 clear ok");
        fir_hw_write_coeffs();
        dbg("S2 coeff ok");
        fir_hw_set_length(TEST_SAMPLES);
        dbg("S3 length ok");
        fir_hw_write_input_buffer();
        dbg("S4 input ok");
        fir_hw_start();
        dbg("S5 start ok");

        if (!fir_hw_wait_done_with_timeout()) {
            printf("ERR timeout status=0x%08x\r\n", (unsigned int)fir_hw_read_reg(FIR_REG_STATUS));
            put_flush();
            for (volatile int d = 0; d < 300000; ++d) {
                ;
            }
            continue;
        }
        dbg("S6 done ok");

        for (int i = 0; i < TEST_SAMPLES; ++i) {
            int16_t x = test_input[i];
            int16_t y_matlab = matlab_ref[i];
            int32_t y_hw = fir_hw_read_output(i);

            printf("%d %d %d\r\n", (int)x, (int)y_hw, (int)(y_matlab + MATLAB_PLOT_OFFSET));

            for (volatile int d = 0; d < 15000; ++d) {
                ;
            }
        }
#else
        // Fallback mode: same mechanism as referensi_fir.c (SW FIR only).
        for (int i = 0; i < TEST_SAMPLES; ++i) {
            int16_t x = test_input[i];
            int16_t y_sw = fir_process(x);
            int16_t y_matlab = matlab_ref[i];

            printf("%d %d %d\r\n", (int)x, (int)y_sw, (int)(y_matlab + MATLAB_PLOT_OFFSET));

            for (volatile int d = 0; d < 15000; ++d) {
                ;
            }
        }
#endif

        put_flush();
#if USE_HW_FIR
        dbg("S7 output done");
#endif

        for (volatile int d = 0; d < 200000; ++d) {
            ;
        }
    }

    return 0;
}
