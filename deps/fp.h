// https://github.com/tidwall/fp
//
// Copyright 2025 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#ifndef FP_H
#define FP_H

#include <stddef.h>
#include <stdbool.h>

size_t fp_dtoa(double d, char fmt, char dst[], size_t n);
size_t fp_ftoa(float f, char fmt, char dst[], size_t n);
bool fp_atod(const char *data, size_t len, double *x);
bool fp_atof(const char *data, size_t len, float *x);

struct fp_info {
    bool ok;     // number is valid
    bool sign;   // has sign. Is a negative number
    size_t frac; // has dot. Index of '.' or zero if none
    size_t exp;  // has exponent. Index of 'e' or zero if none
    size_t len;  // number of bytes parsed
};

struct fp_info fp_parse(const char *data, size_t len);

#endif
