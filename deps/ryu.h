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
// The format is one of 
//   'e' (-d.ddddedd, a decimal exponent)
//   'E' (-d.ddddEdd, a decimal exponent)
//   'f' (-ddd.dddd, no exponent)
//   'g' ('e' for large exponents, 'f' otherwise) 
//   'G' ('E' for large exponents, 'f' otherwise)
//   'j' ('g' for large exponents, 'f' otherwise) (matches javascript format)
//   'J' ('G' for large exponents, 'f' otherwise) (matches javascript format)
size_t ryu_string(double d, char fmt, char dst[], size_t nbytes);

#endif
