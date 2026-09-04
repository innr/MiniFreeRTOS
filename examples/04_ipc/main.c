#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <stdio.h>

static QueueHandle_t sample_queue;
static SemaphoreHandle_t ready_semaphore;

static void vProducerTask(void *parameters)
{
    (void)parameters;
    for (int sample = 1; sample <= 3; ++sample) {
        (void)xQueueSend(sample_queue, &sample, 5U);
        vTaskDelay(2U);
    }
}

static void vConsumerTask(void *parameters)
{
    int sample;
    (void)parameters;
    for (unsigned index = 0U; index < 3U; ++index) {
        if (xQueueReceive(sample_queue, &sample, 10U) == pdPASS) {
            printf("consumer sample=%d tick=%u\n",
                   sample, (unsigned)xTaskGetTickCount());
        }
    }
    (void)xSemaphoreGive(ready_semaphore);
}

static void vMonitorTask(void *parameters)
{
    (void)parameters;
    if (xSemaphoreTake(ready_semaphore, 20U) == pdPASS) {
        puts("IPC lesson complete");
    }
}

int main(void)
{
    sample_queue = xQueueCreate(2U, sizeof(int));
    ready_semaphore = xSemaphoreCreateBinary();
    configASSERT(sample_queue != NULL);
    configASSERT(ready_semaphore != NULL);
    configASSERT(xTaskCreate(vProducerTask, "producer",
                             configMINIMAL_STACK_SIZE, NULL, 2U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vConsumerTask, "consumer",
                             configMINIMAL_STACK_SIZE, NULL, 3U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vMonitorTask, "monitor",
                             configMINIMAL_STACK_SIZE, NULL, 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    return 0;
}
