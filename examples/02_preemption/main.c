#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

static volatile unsigned long run_count[2];
static const TickType_t stop_tick = 30U;

static void vCpuBoundTask(void *parameters)
{
    unsigned id = *(unsigned *)parameters;
    while (xTaskGetTickCount() < stop_tick) {
        ++run_count[id];
    }
}

int main(void)
{
    unsigned first = 0U;
    unsigned second = 1U;

    configASSERT(xTaskCreate(vCpuBoundTask, "CPU-A",
                             configMINIMAL_STACK_SIZE, &first, 2U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vCpuBoundTask, "CPU-B",
                             configMINIMAL_STACK_SIZE, &second, 2U, NULL) == pdPASS);
    vTaskStartScheduler();

    (void)printf("ticks=%u CPU-A=%lu CPU-B=%lu\n",
                 (unsigned)xTaskGetTickCount(), run_count[0], run_count[1]);
    return 0;
}

