#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static volatile unsigned long runs[2];
static TickType_t critical_tick_before;
static TickType_t critical_tick_after;
static const TickType_t stop_tick = 20U;

static void vBusyTask(void *parameters)
{
    unsigned identity = *(unsigned *)parameters;

    if (identity == 0U) {
        struct timespec pause = {0, 5000000L};
        taskENTER_CRITICAL();
        critical_tick_before = xTaskGetTickCount();
        (void)nanosleep(&pause, NULL);
        critical_tick_after = xTaskGetTickCount();
        taskEXIT_CRITICAL();
    }

    while (xTaskGetTickCount() < stop_tick) {
        ++runs[identity];
    }
}

int main(void)
{
    unsigned first = 0U;
    unsigned second = 1U;

    assert(xTaskCreate(vBusyTask, "busy-a", configMINIMAL_STACK_SIZE,
                       &first, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vBusyTask, "busy-b", configMINIMAL_STACK_SIZE,
                       &second, 2U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert(runs[0] > 0UL);
    assert(runs[1] > 0UL);
    assert(critical_tick_before == critical_tick_after);
    assert(xTaskGetTickCount() >= stop_tick);
    puts("P2 tick/preemption/critical-section tests passed");
    return 0;
}

