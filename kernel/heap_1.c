#include "heap_internal.h"
#include <stdint.h>

#if configHEAP_SCHEME != 1U
#error "heap_1.c requires configHEAP_SCHEME=1"
#endif

_Static_assert(MINI_HEAP_USABLE_SIZE >= (size_t)portBYTE_ALIGNMENT,
               "configTOTAL_HEAP_SIZE is too small");

_Alignas(portBYTE_ALIGNMENT) static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
static size_t next_free;
static size_t free_bytes = MINI_HEAP_USABLE_SIZE;

void *pvPortMalloc(size_t size)
{
    size_t aligned_size;
    uintptr_t address;

    if (size == 0U) {
        return NULL;
    }
    aligned_size = miniHeapAlignUp(size);
    if (aligned_size == 0U) {
        return NULL;
    }

    taskENTER_CRITICAL();
    if (aligned_size > free_bytes) {
        taskEXIT_CRITICAL();
        return NULL;
    }
    address = (uintptr_t)ucHeap + next_free;
    next_free += aligned_size;
    free_bytes -= aligned_size;
    taskEXIT_CRITICAL();
    return (void *)address;
}

void vPortFree(void *pointer)
{
    (void)pointer;
}

size_t xPortGetFreeHeapSize(void)
{
    size_t result;

    taskENTER_CRITICAL();
    result = free_bytes;
    taskEXIT_CRITICAL();
    return result;
}
