#ifndef RYU_H
#define RYU_H

#include <stddef.h>

// ryu_string converts a double into a string representation that is copied
// into the provided C string buffer.
//
// Returns the number of characters, not including the null-terminator, needed
// to store the double into the C string buffer.
// If the returned length is greater than nbytes-1, then only a parital copy
// occurred.
// 
// The fmt argument can be 'f' for full decimal or 'e' for scientific notation.
size_t ryu_string(double d, char fmt, char dst[], size_t nbytes);

#endif
