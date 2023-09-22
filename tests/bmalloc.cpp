#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmissing-exception-spec"

extern "C" {
    void *bmalloc(size_t size);
    void bfree(void *ptr);
}

void* operator new(size_t size) {
    return bmalloc(size);
}
 
void operator delete(void* ptr) {
    bfree(ptr);
}
