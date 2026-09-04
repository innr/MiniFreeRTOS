#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdio.h>

static SemaphoreHandle_t resource_mutex;
static volatile BaseType_t high_done;

static void vLowPriorityTask(void *parameters)
{
    (void)parameters;
    configASSERT(xSemaphoreTake(resource_mutex, 0U) == pdPASS);
    printf("low: acquired mutex at tick=%u base=%u\n",
           (unsigned)xTaskGetTickCount(), (unsigned)uxTaskPriorityGet(NULL));
    vTaskDelay(4U);
    printf("low: inherited priority=%u while high waits\n",
           (unsigned)uxTaskPriorityGet(NULL));
    configASSERT(xSemaphoreGive(resource_mutex) == pdPASS);
    printf("low: released mutex, restored priority=%u\n",
           (unsigned)uxTaskPriorityGet(NULL));
}

static void vHighPriorityTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    puts("high: waiting for mutex");
    configASSERT(xSemaphoreTake(resource_mutex, 10U) == pdPASS);
    printf("high: acquired mutex at tick=%u\n",
           (unsigned)xTaskGetTickCount());
    configASSERT(xSemaphoreGive(resource_mutex) == pdPASS);
    high_done = pdTRUE;
}

static void vMediumPriorityTask(void *parameters)
{
    (void)parameters;
    unsigned long work = 0UL;

    vTaskDelay(2U);
    while ((high_done == pdFALSE) && (xTaskGetTickCount() < 8U)) {
        ++work;
    }
    printf("medium: work=%lu high_done=%d\n", work, (int)high_done);
}

int main(void)
{
    resource_mutex = xSemaphoreCreateMutex();
    configASSERT(resource_mutex != NULL);
    configASSERT(xTaskCreate(vHighPriorityTask, "high", configMINIMAL_STACK_SIZE,
                             NULL, 3U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vMediumPriorityTask, "medium", configMINIMAL_STACK_SIZE,
                             NULL, 2U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vLowPriorityTask, "low", configMINIMAL_STACK_SIZE,
                             NULL, 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    return 0;
}
