// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philippe Sauter <phsauter@iis.ee.ethz.ch>

#include "print.h"
#include "util.h"
#include "config.h"

const char hex_symbols[16] = {'0', '1', '2', '3', '4', '5', '6', '7', 
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static void putstr(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        putchar(*s++);
    }
}

static uint8_t format_dec32(char *buffer, uint32_t num) {
    uint8_t idx = 0;
    if (num == 0) {
        buffer[0] = '0';
        return 1;
    }

    while (num > 0) {
        buffer[idx++] = (char)('0' + (num % 10));
        num /= 10;
    }
    return idx;
}

static void print_reversed_with_padding(const char *buffer, uint8_t len, uint8_t width, char pad) {
    while (len < width) {
        putchar(pad);
        width--;
    }

    for (int j = len - 1; j >= 0; j--) {
        putchar(buffer[j]);
    }
}

/// @brief format number as hexadecimal digits
/// @return number of characters written to buffer
uint8_t format_hex32(char *buffer, uint32_t num) {
    uint8_t idx = 0;
    if (num == 0) {
        buffer[0] = hex_symbols[0];
        return 1;
    }

    while (num > 0) {
        buffer[idx++] = hex_symbols[num & 0xF];
        num >>= 4;
    }
    return idx;
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[12];  // holds digits while assembling in reverse order
    uint8_t idx;

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == '%') {
                putchar('%');
                fmt++;
                continue;
            }

            char pad = ' ';
            uint8_t width = 0;

            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }

            while (*fmt >= '0' && *fmt <= '9') {
                width = (uint8_t)(width * 10 + (uint8_t)(*fmt - '0'));
                fmt++;
            }

            switch (*fmt) {
                case 'x': {
                    idx = format_hex32(buffer, va_arg(args, unsigned int));
                    print_reversed_with_padding(buffer, idx, width, pad);
                    break;
                }
                case 'u': {
                    idx = format_dec32(buffer, va_arg(args, unsigned int));
                    print_reversed_with_padding(buffer, idx, width, pad);
                    break;
                }
                case 'd': {
                    int value = va_arg(args, int);
                    uint32_t magnitude;
                    if (value < 0) {
                        putchar('-');
                        magnitude = (uint32_t)(-(value + 1)) + 1u;
                    } else {
                        magnitude = (uint32_t)value;
                    }
                    idx = format_dec32(buffer, magnitude);
                    print_reversed_with_padding(buffer, idx, width, pad);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    putchar(c);
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    putstr(s ? s : "(null)");
                    break;
                }
                default: {
                    // Unknown specifier: print it literally to aid debugging.
                    putchar('%');
                    if (pad == '0') {
                        putchar('0');
                    }
                    if (width > 0) {
                        char wbuf[3];
                        uint8_t widx = format_dec32(wbuf, width);
                        for (int j = widx - 1; j >= 0; j--) {
                            putchar(wbuf[j]);
                        }
                    }
                    if (*fmt) {
                        putchar(*fmt);
                    }
                    break;
                }
            }
        } else {
            putchar(*fmt);
        }
        if (*fmt) {
            fmt++;
        }
    }

    va_end(args);
}