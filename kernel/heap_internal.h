#ifndef MINI_HEAP_INTERNAL_H
#define MINI_HEAP_INTERNAL_H

#include "portable.h"
#include <stdint.h>

#if (portBYTE_ALIGNMENT == 0U) || \
    ((portBYTE_ALIGNMENT & (portBYTE_ALIGNMENT - 1U)) != 0U)
#error "portBYTE_ALIGNMENT must be a non-zero power of two"
#endif

_Static_assert(_Alignof(max_align_t) >= portBYTE_ALIGNMENT,
               "portBYTE_ALIGNMENT exceeds the host alignment");

#define MINI_HEAP_ALIGNMENT_MASK ((size_t)portBYTE_ALIGNMENT - 1U)
#define MINI_HEAP_USABLE_SIZE \
    ((size_t)configTOTAL_HEAP_SIZE & ~MINI_HEAP_ALIGNMENT_MASK)

static inline size_t miniHeapAlignUp(size_t size)
{
    if (size > (SIZE_MAX - MINI_HEAP_ALIGNMENT_MASK)) {
        return 0U;
    }
    return (size + MINI_HEAP_ALIGNMENT_MASK) &
           ~MINI_HEAP_ALIGNMENT_MASK;
}

static inline BaseType_t miniHeapPointerInRange(const uint8_t *heap,
                                                size_t heap_size,
                                                const void *pointer)
{
    uintptr_t start = (uintptr_t)heap;
    uintptr_t address = (uintptr_t)pointer;

    if (address < start) {
        return pdFALSE;
    }
    return ((size_t)(address - start) < heap_size) ? pdTRUE : pdFALSE;
}

#endif
