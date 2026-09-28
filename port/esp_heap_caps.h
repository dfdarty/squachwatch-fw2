// SquachWatch on the FREE-WILi 2: heap figures for the diagnostics screen
// and for detection.cpp's low-memory seatbelt, from the real allocator.
#pragma once
#include <cstddef>
#include <cstdint>
#define MALLOC_CAP_8BIT     (1 << 2)
#define MALLOC_CAP_DMA      (1 << 3)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_SPIRAM   (1 << 10)
#define MALLOC_CAP_DEFAULT  (1 << 12)
uint32_t fw2_heap_free();
uint32_t fw2_heap_largest();
inline size_t heap_caps_get_largest_free_block(uint32_t) { return fw2_heap_largest(); }
inline size_t heap_caps_get_free_size(uint32_t) { return fw2_heap_free(); }
inline size_t heap_caps_get_minimum_free_size(uint32_t) { return fw2_heap_free(); }
inline void* heap_caps_malloc(size_t n, uint32_t) { return malloc(n); }
inline void* heap_caps_calloc(size_t n, size_t s, uint32_t) { return calloc(n, s); }
inline void  heap_caps_free(void* p) { free(p); }
