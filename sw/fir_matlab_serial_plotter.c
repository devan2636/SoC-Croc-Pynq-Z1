#include <stdint.h>

#include "config.h"
#include "print.h"
#include "uart.h"
#include "util.h"

#define FIR_TAPS 16
#define FIR_SAMPLES 60

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

static const int16_t fir_coeff_q15[FIR_TAPS] = {
    -1586,  1037,  2173,   528, -2496, -1370,  6023, 13670,
    13670,  6023, -1370, -2496,   528,  2173,  1037, -1586
};

/* data_asli scaled by 1000 (first 60 samples) */
static const int16_t input_fixed[FIR_SAMPLES] = {
    -538, 867, 976, 337, -996, -523, -1297, 917,
    177, 755, -592, 1844, 1817, -124, -1111, -681,
    14, -60, -661, 306, -409, -1282, -285, -65,
    1000, -801, 92, 274, -2134, 404, -959, -326,
    2479, 1539, 827, -160, -858, 398, -225, -1315,
    -22, -880, 1109, -464, 665, -530, 41, 155,
    -321, 2146, 800, 1196, 2098, 388, 1045, -1372,
    -958, -2669, -803, -451
};

/* data_filter scaled by 1000 (first 60 samples) */
static const int16_t matlab_fixed[FIR_SAMPLES] = {
    26, -59, -56, 63, 179, -12, -224, -268,
    304, 823, 747, -258, -1034, -773, 63, 481,
    225, 263, 925, 1370, 869, -160, -978, -714,
    -185, 253, -130, -606, -712, -719, -236, 187,
    157, -18, -317, -527, -785, -901, -118, 1134,
    1826, 1326, 152, -297, -377, -56, -345, -773,
    -524, -108, 363, 337, -73, -175, -75, 378,
    902, 1255, 1397, 1349
};

static void put_str(const char *s) {
    while (*s) {
        uart_write((uint8_t)*s++);
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
        fir_hw_write_reg(FIR_REG_COEFF_DATA, (uint32_t)(int32_t)fir_coeff_q15[i]);
    }
    fence();
}

static void fir_hw_write_input_buffer(void) {
    for (int i = 0; i < FIR_SAMPLES; ++i) {
        fir_hw_write_reg(FIR_REG_INPUT_IDX, (uint32_t)i);
        fir_hw_write_reg(FIR_REG_INPUT_DATA, (uint32_t)(int32_t)input_fixed[i]);
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

int main(void) {
    uart_init();

    fir_hw_clear();
    fir_hw_write_coeffs();
    fir_hw_set_length(FIR_SAMPLES);
    fir_hw_write_input_buffer();
    fir_hw_start();
    fir_hw_wait_done();

    /* CSV header for Serial Plotter */
    put_str("sinyal_asli,sinyal_output_matlab,sinyal_output_fir\r\n");

    for (int i = 0; i < FIR_SAMPLES; ++i) {
        int32_t y_fir = fir_hw_read_output(i);
        printf("%d,%d,%d\r\n", (int)input_fixed[i], (int)matlab_fixed[i], (int)y_fir);
    }

    uart_write_flush();

    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
