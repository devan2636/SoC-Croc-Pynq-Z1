// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philippe Sauter <phsauter@iis.ee.ethz.ch>

#pragma once

#include <stdarg.h>

extern void putchar(char);

// Lightweight printf: supports %% %x %u %d %c %s and optional zero-padding width (e.g. %04x).
void printf(const char *fmt, ...);