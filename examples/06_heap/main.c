#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include <stdio.h>

static void vHeapLessonTask(void *parameters)
{
    void *first;
    void *second;
    size_t before;
    size_t after_alloc;
    size_t after_free;

    (void)parameters;
    before = xPortGetFreeHeapSize();
    first = pvPortMalloc(64U);
    second = pvPortMalloc(128U);
    after_alloc = xPortGetFreeHeapSize();
    printf("heap scheme=%u before=%zu after_alloc=%zu first=%p aligned=%u\n",
           (unsigned)configHEAP_SCHEME, before, after_alloc, first,
           (unsigned)(((uintptr_t)first % (uintptr_t)portBYTE_ALIGNMENT) == 0U));
    printf("second=%p aligned=%u\n", second,
           (unsigned)(((uintptr_t)second % (uintptr_t)portBYTE_ALIGNMENT) == 0U));
    vPortFree(first);
    after_free = xPortGetFreeHeapSize();
    printf("after_free_first=%zu\n", after_free);
    vPortFree(second);
    printf("after_free_second=%zu\n", xPortGetFreeHeapSize());
}

int main(void)
{
    configASSERT(xTaskCreate(vHeapLessonTask, "heap", configMINIMAL_STACK_SIZE,
                             NULL, 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    return 0;
}
