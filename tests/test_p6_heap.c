#include "FreeRTOS.h"
#include "portable.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST_HEAP_SCHEME
#error "TEST_HEAP_SCHEME must be defined"
#endif

static void assert_aligned(const void *pointer)
{
    assert(pointer != NULL);
    assert(((uintptr_t)pointer % (uintptr_t)portBYTE_ALIGNMENT) == 0U);
}

int main(void)
{
    void *first;
    void *second;
    void *third;
    void *filler[32] = {0};
    void *large;
    size_t initial_free;
    size_t allocated_free;
    size_t exhausted_free;
    size_t released_free;
    size_t filler_count = 0U;
    uintptr_t first_address;
    uintptr_t second_address;
    uintptr_t third_address;

    initial_free = xPortGetFreeHeapSize();
    assert(initial_free == (size_t)configTOTAL_HEAP_SIZE);
    assert(pvPortMalloc(0U) == NULL);
    assert(pvPortMalloc(SIZE_MAX) == NULL);

    first = pvPortMalloc(256U);
    second = pvPortMalloc(256U);
    third = pvPortMalloc(256U);
    assert_aligned(first);
    assert_aligned(second);
    assert_aligned(third);
    first_address = (uintptr_t)first;
    second_address = (uintptr_t)second;
    third_address = (uintptr_t)third;
    assert(first_address + 256U <= second_address);
    assert(second_address + 256U <= third_address);
    (void)memset(first, 0x11, 256U);
    (void)memset(second, 0x22, 256U);
    (void)memset(third, 0x33, 256U);
    allocated_free = xPortGetFreeHeapSize();
    assert(allocated_free < initial_free);

    while ((filler_count < (sizeof(filler) / sizeof(filler[0]))) &&
           ((filler[filler_count] = pvPortMalloc(256U)) != NULL)) {
        assert_aligned(filler[filler_count]);
        ++filler_count;
    }
    assert(filler_count > 0U);
    assert(pvPortMalloc(256U) == NULL);
    exhausted_free = xPortGetFreeHeapSize();
    assert(exhausted_free < allocated_free);

    vPortFree(NULL);
    vPortFree(first);
    vPortFree(second);
    released_free = xPortGetFreeHeapSize();

#if TEST_HEAP_SCHEME == 1
    assert(released_free == exhausted_free);
    large = pvPortMalloc(400U);
    assert(large == NULL);
#elif TEST_HEAP_SCHEME == 2
    assert(released_free > exhausted_free);
    large = pvPortMalloc(400U);
    assert(large == NULL);
    large = pvPortMalloc(128U);
    assert(large == first);
#elif TEST_HEAP_SCHEME == 4
    assert(released_free > exhausted_free);
    large = pvPortMalloc(400U);
    assert(large == first);
#else
#error "Unsupported heap test scheme"
#endif

    puts("P6 heap allocator test passed");
    return 0;
}
