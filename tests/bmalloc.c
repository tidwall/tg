////////////////////////////////////////////////////////////////////////////////
// An allocator used by benchmarking tool to track allocations.
//
// Notes:
// - For single threaded programs only.
// - Track memory stats with malloc_heap_size() and malloc_num_allocs();
// - Use with bmalloc.cpp for C++

#include <stdint.h>
#include <stdlib.h>

static uint64_t num_allocs = 0;
static uint64_t heap_size = 0;

size_t bmalloc_heap_size(void) {
    return heap_size;
}

size_t bmalloc_num_allocs(void) {
    return num_allocs;
}

void *bmalloc(size_t size) {
    void *mem = malloc(sizeof(uint64_t)+size);
    if (!mem) return NULL;
    *(uint64_t*)mem = size;
    num_allocs++;
    heap_size += size;
    return (char*)mem+sizeof(uint64_t);
}

void bfree(void *ptr) {
    if (!ptr) return;
    heap_size -= *(uint64_t*)((char*)ptr-sizeof(uint64_t));
    num_allocs--;
    free((char*)ptr-sizeof(uint64_t));
}

void *brealloc(void *ptr, size_t size) {
    if (!ptr) return bmalloc(size);
    size_t psize = *(uint64_t*)((char*)ptr-sizeof(uint64_t));
    void *mem = realloc((char*)ptr-sizeof(uint64_t), sizeof(uint64_t)+size);
    if (!mem) return NULL;
    *(uint64_t*)mem = size;
    heap_size -= psize;
    heap_size += size;
    return (char*)mem+sizeof(uint64_t);
}
