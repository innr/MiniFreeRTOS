#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static volatile TickType_t delay_start;
static volatile TickType_t delay_end;
static volatile TickType_t periodic_ticks[3];
static volatile BaseType_t abort_result;
static volatile BaseType_t abort_woke;
static TaskHandle_t abort_target;

static void vDelayTask(void *parameters)
{
    TickType_t previous_wake;
    (void)parameters;

    delay_start = xTaskGetTickCount();
    vTaskDelay(3U);
    delay_end = xTaskGetTickCount();

    previous_wake = delay_end;
    for (unsigned index = 0U; index < 3U; ++index) {
        vTaskDelayUntil(&previous_wake, 2U);
        periodic_ticks[index] = xTaskGetTickCount();
    }
}

static void vAbortTargetTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(50U);
    abort_woke = pdTRUE;
}

static void vAborterTask(void *parameters)
{
    (void)parameters;
    abort_result = xTaskAbortDelay(abort_target);
}

int main(void)
{
    assert(xTaskCreate(vDelayTask, "delay", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vAbortTargetTask, "abort-target", configMINIMAL_STACK_SIZE,
                       NULL, 2U, &abort_target) == pdPASS);
    assert(xTaskCreate(vAborterTask, "aborter", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert((TickType_t)(delay_end - delay_start) >= 3U);
    assert(abort_result == pdPASS);
    assert(abort_woke == pdTRUE);
    assert(xTaskAbortDelay(NULL) == pdFAIL);
    assert(xTaskAbortDelay(abort_target) == pdFAIL);
    assert((TickType_t)(periodic_ticks[0] - delay_end) >= 2U);
    assert((TickType_t)(periodic_ticks[1] - periodic_ticks[0]) >= 2U);
    assert((TickType_t)(periodic_ticks[2] - periodic_ticks[1]) >= 2U);
    puts("P3 delay/timeout/abort tests passed");
    return 0;
}
