#include "FreeRTOS.h"
#include "task.h"
#include "tasks_internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static volatile TickType_t delay_start;
static volatile TickType_t delay_end;

static void vWrapTask(void *parameters)
{
    (void)parameters;
    delay_start = xTaskGetTickCount();
    vTaskDelay(4U);
    delay_end = xTaskGetTickCount();
}

int main(void)
{
    vTaskSetTickCountForTest(UINT32_MAX - 2U);
    assert(xTaskCreate(vWrapTask, "wrap", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert(delay_start == (TickType_t)(UINT32_MAX - 2U));
    assert(delay_end < delay_start);
    assert((TickType_t)(delay_end - delay_start) >= 4U);
    puts("P3 tick-wrap tests passed");
    return 0;
}
