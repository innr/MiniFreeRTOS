#include "FreeRTOS.h"
#include "board.h"
#include "queue.h"
#include "task.h"
#include "trace.h"

static QueueHandle_t message_queue;
static volatile unsigned producer_done;
static volatile unsigned produced_count;
static volatile unsigned consumed_count;
static volatile BaseType_t lesson_failed;

static void vProducerTask(void *parameters)
{
    (void)parameters;
    for (unsigned value = 1U; value <= 3U; ++value) {
        if (xQueueSend(message_queue, &value, 5U) != pdPASS) {
            lesson_failed = pdTRUE;
            break;
        }
        ++produced_count;
        vTaskDelay(2U);
    }
    producer_done = 1U;
}

static void vConsumerTask(void *parameters)
{
    (void)parameters;
    for (unsigned expected = 1U; expected <= 3U; ++expected) {
        unsigned value = 0U;

        if (xQueueReceive(message_queue, &value, 20U) != pdPASS) {
            lesson_failed = pdTRUE;
            break;
        }
        if (value != expected) {
            lesson_failed = pdTRUE;
            break;
        }
        ++consumed_count;
    }

    while (producer_done == 0U) {
        vTaskDelay(1U);
    }
    if ((produced_count != 3U) || (consumed_count != 3U) ||
        (uxTraceGetCount() == 0U)) {
        lesson_failed = pdTRUE;
    }
    vBoardConsoleWrite(lesson_failed == pdFALSE ?
                       "P9 PASS: SVC/PendSV/SysTick/queue/trace\n" :
                       "P9 FAIL\n");
    if (lesson_failed != pdFALSE) {
        vBoardExit(1);
    }
    vTaskEndScheduler();
}

int main(void)
{
    vBoardConsoleWrite("MiniFreeRTOS P9 Cortex-M lesson\n");
    vTraceReset();
    message_queue = xQueueCreate(1U, (UBaseType_t)sizeof(unsigned));
    if ((message_queue == NULL) ||
        (xTaskCreate(vConsumerTask, "consumer", configMINIMAL_STACK_SIZE,
                     NULL, 3U, NULL) != pdPASS) ||
        (xTaskCreate(vProducerTask, "producer", configMINIMAL_STACK_SIZE,
                     NULL, 2U, NULL) != pdPASS)) {
        vBoardConsoleWrite("P9 setup failed\n");
        vBoardExit(1);
    }
    vTaskStartScheduler();
    vBoardExit(1);
    return 1;
}
