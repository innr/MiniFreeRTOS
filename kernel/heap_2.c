#include "heap_internal.h"
#include <stddef.h>
#include <stdint.h>

#if configHEAP_SCHEME != 2U
#error "heap_2.c requires configHEAP_SCHEME=2"
#endif

typedef struct HeapBlock {
    size_t size;
    struct HeapBlock *next;
    uint32_t magic;
} HeapBlock_t;

enum {
    eBlockFree = 0x46524545U,
    eBlockAllocated = 0x414C4C4FU
};

_Static_assert((sizeof(HeapBlock_t) % portBYTE_ALIGNMENT) == 0U,
               "HeapBlock_t must preserve payload alignment");
_Static_assert(MINI_HEAP_USABLE_SIZE >=
                   (sizeof(HeapBlock_t) + (size_t)portBYTE_ALIGNMENT),
               "configTOTAL_HEAP_SIZE is too small");

_Alignas(portBYTE_ALIGNMENT) static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
static HeapBlock_t *free_list;
static size_t free_bytes;
static BaseType_t heap_initialized;

static void prvHeapInit(void)
{
    if (heap_initialized == pdTRUE) {
        return;
    }
    free_list = (HeapBlock_t *)ucHeap;
    free_list->size = MINI_HEAP_USABLE_SIZE;
    free_list->next = NULL;
    free_list->magic = eBlockFree;
    free_bytes = MINI_HEAP_USABLE_SIZE;
    heap_initialized = pdTRUE;
}

static BaseType_t prvBlockPointerIsValid(const void *pointer,
                                         HeapBlock_t **block_out)
{
    HeapBlock_t *block;
    uintptr_t heap_start;
    uintptr_t payload_address;
    size_t payload_offset;
    size_t block_offset;

    if ((pointer == NULL) ||
        (miniHeapPointerInRange(ucHeap, MINI_HEAP_USABLE_SIZE, pointer) ==
         pdFALSE)) {
        return pdFALSE;
    }
    if (((uintptr_t)pointer % (uintptr_t)portBYTE_ALIGNMENT) != 0U) {
        return pdFALSE;
    }
    heap_start = (uintptr_t)ucHeap;
    payload_address = (uintptr_t)pointer;
    payload_offset = (size_t)(payload_address - heap_start);
    if (payload_offset < sizeof(HeapBlock_t)) {
        return pdFALSE;
    }
    block_offset = payload_offset - sizeof(HeapBlock_t);
    block = (HeapBlock_t *)(ucHeap + block_offset);
    if ((block->magic != eBlockAllocated) ||
        (block->size < (sizeof(HeapBlock_t) + (size_t)portBYTE_ALIGNMENT)) ||
        (block->size > MINI_HEAP_USABLE_SIZE) ||
        ((block->size % (size_t)portBYTE_ALIGNMENT) != 0U) ||
        (block->size > (MINI_HEAP_USABLE_SIZE - block_offset))) {
        return pdFALSE;
    }
    *block_out = block;
    return pdTRUE;
}

void *pvPortMalloc(size_t size)
{
    HeapBlock_t *previous = NULL;
    HeapBlock_t *block;
    HeapBlock_t *remainder;
    size_t aligned_size;
    size_t total_size;

    if (size == 0U) {
        return NULL;
    }
    aligned_size = miniHeapAlignUp(size);
    if ((aligned_size == 0U) ||
        (aligned_size > (SIZE_MAX - sizeof(HeapBlock_t)))) {
        return NULL;
    }
    total_size = aligned_size + sizeof(HeapBlock_t);
    total_size = miniHeapAlignUp(total_size);
    if (total_size == 0U) {
        return NULL;
    }

    taskENTER_CRITICAL();
    prvHeapInit();
    block = free_list;
    while ((block != NULL) && (block->size < total_size)) {
        previous = block;
        block = block->next;
    }
    if (block == NULL) {
        taskEXIT_CRITICAL();
        return NULL;
    }
    if ((block->size - total_size) >=
        (sizeof(HeapBlock_t) + (size_t)portBYTE_ALIGNMENT)) {
        remainder = (HeapBlock_t *)((uint8_t *)block + total_size);
        remainder->size = block->size - total_size;
        remainder->next = block->next;
        remainder->magic = eBlockFree;
        if (previous == NULL) {
            free_list = remainder;
        } else {
            previous->next = remainder;
        }
    } else {
        total_size = block->size;
        if (previous == NULL) {
            free_list = block->next;
        } else {
            previous->next = block->next;
        }
    }
    configASSERT((free_bytes >= total_size) &&
                 (block->magic == eBlockFree));
    free_bytes -= total_size;
    block->next = NULL;
    block->size = total_size;
    block->magic = eBlockAllocated;
    taskEXIT_CRITICAL();
    return (void *)((uint8_t *)block + sizeof(HeapBlock_t));
}

void vPortFree(void *pointer)
{
    HeapBlock_t *block;
    HeapBlock_t *previous = NULL;
    HeapBlock_t *cursor;

    if (pointer == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    prvHeapInit();
    configASSERT(prvBlockPointerIsValid(pointer, &block) == pdTRUE);
    cursor = free_list;
    while ((cursor != NULL) &&
           ((uintptr_t)cursor < (uintptr_t)block)) {
        previous = cursor;
        cursor = cursor->next;
    }
    block->magic = eBlockFree;
    block->next = cursor;
    if (previous == NULL) {
        free_list = block;
    } else {
        previous->next = block;
    }
    free_bytes += block->size;
    taskEXIT_CRITICAL();
}

size_t xPortGetFreeHeapSize(void)
{
    size_t result;

    taskENTER_CRITICAL();
    prvHeapInit();
    result = free_bytes;
    taskEXIT_CRITICAL();
    return result;
}
