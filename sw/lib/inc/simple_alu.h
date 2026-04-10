#include <stdint.h>
#include "util.h"
#include "config.h"

static inline __attribute__((always_inline))
uint32_t pack_inputs(int16_t num1, int16_t num2) {
    return ((uint32_t)(uint16_t)num1 << 16) | (uint16_t)num2;
}

int32_t add_sw(int16_t num1, int16_t num2);

int32_t sub_sw(int16_t num1, int16_t num2);

int32_t mult_sw(int16_t num1, int16_t num2);

int32_t div_sw(int16_t num1, int16_t num2);

#define USER_REG16_OFFSET 20
#define ADD2_INPUT_OFFSET 24
#define ADD2_OUTPUT_OFFSET 28

static inline __attribute__((always_inline))
void user_reg16_write(uint16_t value) {
    *reg32(USER_ALU_BASE_ADDR, USER_REG16_OFFSET) = (uint32_t)value;
}

static inline __attribute__((always_inline))
uint16_t user_reg16_read(void) {
    return (uint16_t)(*reg32(USER_ALU_BASE_ADDR, USER_REG16_OFFSET) & 0xFFFFu);
}

static inline __attribute__((always_inline))
void add2_input_write(uint16_t value) {
    *reg32(USER_ALU_BASE_ADDR, ADD2_INPUT_OFFSET) = (uint32_t)value;
}

static inline __attribute__((always_inline))
uint16_t add2_input_read(void) {
    return (uint16_t)(*reg32(USER_ALU_BASE_ADDR, ADD2_INPUT_OFFSET) & 0xFFFFu);
}

static inline __attribute__((always_inline))
uint16_t add2_output_read(void) {
    return (uint16_t)(*reg32(USER_ALU_BASE_ADDR, ADD2_OUTPUT_OFFSET) & 0xFFFFu);
}

/**
 * @brief Addition of 16 bit integer fixed point
 *
 * @param num1 The first 16-bit data to be calculated
 * @param num2 The second 16-bit data to be calculated
 * @return int32_t The fixed point addition result
 */
static inline __attribute__((always_inline))
int32_t add_hw(int16_t num1, int16_t num2) {
    uint32_t payload = pack_inputs(num1, num2);
    *reg32(USER_ALU_BASE_ADDR, 0) = payload;
    return *reg32(USER_ALU_BASE_ADDR, 16);
}

/**
 * @brief Substraction of 16 bit integer fixed point
 *
 * @param num1 The first 16-bit data to be calculated
 * @param num2 The second 16-bit data to be calculated
 * @return int32_t The fixed point substraction result
 */
static inline __attribute__((always_inline))
int32_t sub_hw(int16_t num1, int16_t num2) {
    uint32_t payload = pack_inputs(num1, num2);
    *reg32(USER_ALU_BASE_ADDR, 4) = payload;
    return *reg32(USER_ALU_BASE_ADDR, 16);
}

/**
 * @brief Multiplication of 16 bit integer fixed point
 *
 * @param num1 The first 16-bit data to be calculated
 * @param num2 The second 16-bit data to be calculated
 * @return int32_t The fixed point multipliation result
 */
static inline __attribute__((always_inline))
int32_t mult_hw(int16_t num1, int16_t num2) {
    uint32_t payload = pack_inputs(num1, num2);
    *reg32(USER_ALU_BASE_ADDR, 8) = payload;
    return *reg32(USER_ALU_BASE_ADDR, 16);
}

/**
 * @brief Division of 16 bit integer fixed point
 *
 * @param num1 The first 16-bit data to be calculated
 * @param num2 The second 16-bit data to be calculated
 * @return int32_t The fixed point division result
 */
static inline __attribute__((always_inline))
int32_t div_hw(int16_t num1, int16_t num2) {
    uint32_t payload = pack_inputs(num1, num2);
    *reg32(USER_ALU_BASE_ADDR, 12) = payload;
    return *reg32(USER_ALU_BASE_ADDR, 16);
}