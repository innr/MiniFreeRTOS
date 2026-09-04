#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

static volatile unsigned sample_count;

static void vSensorTask(void *parameters)
{
    (void)parameters;
    for (unsigned sample = 0U; sample < 4U; ++sample) {
        ++sample_count;
        printf("sensor sample=%u tick=%u\n",
               sample_count, (unsigned)xTaskGetTickCount());
        vTaskDelay(5U);
    }
}

static void vStatusTask(void *parameters)
{
    (void)parameters;
    while (sample_count < 4U) {
        vTaskDelay(1U);
    }
}

int main(void)
{
    configASSERT(xTaskCreate(vSensorTask, "sensor",
                             configMINIMAL_STACK_SIZE, NULL, 2U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vStatusTask, "status",
                             configMINIMAL_STACK_SIZE, NULL, 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    printf("delay lesson complete at tick=%u\n", (unsigned)xTaskGetTickCount());
    return 0;
}
