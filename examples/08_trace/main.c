#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "trace.h"
#include <assert.h>
#include <stdio.h>

static QueueHandle_t trace_queue;
static SemaphoreHandle_t trace_mutex;
static TimerHandle_t trace_timer;
static volatile BaseType_t scenario_done;
static volatile unsigned callback_count;

static void vTraceTimerCallback(TimerHandle_t timer)
{
    ++callback_count;
    if (callback_count >= 2U) {
        configASSERT(xTimerStop(timer, 0U) == pdPASS);
    }
}

static void vTraceLowTask(void *parameters)
{
    (void)parameters;
    configASSERT(xSemaphoreTake(trace_mutex, 0U) == pdPASS);
    vTaskDelay(4U);
    configASSERT(xSemaphoreGive(trace_mutex) == pdPASS);
}

static void vTraceHighTask(void *parameters)
{
    int sent = 17;
    int received = 0;

    (void)parameters;
    vTaskDelay(1U);
    configASSERT(xSemaphoreTake(trace_mutex, 10U) == pdPASS);
    configASSERT(xSemaphoreGive(trace_mutex) == pdPASS);
    configASSERT(xQueueSend(trace_queue, &sent, 0U) == pdPASS);
    configASSERT(xQueueReceive(trace_queue, &received, 0U) == pdPASS);
    configASSERT(received == sent);
    scenario_done = pdTRUE;
}

static void vTraceMediumTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    while ((scenario_done == pdFALSE) &&
           (xTaskGetTickCount() < 8U)) {
        /* Keep a CPU-bound peer in the trace timeline. */
    }
}

static void vPrintCsvField(const char *text)
{
    putchar('"');
    if (text != NULL) {
        for (const char *cursor = text; *cursor != '\0'; ++cursor) {
            if (*cursor == '"') {
                putchar('"');
            }
            putchar(*cursor);
        }
    }
    putchar('"');
}

static void vPrintTaskField(TaskHandle_t task)
{
    const char *name = pcTaskGetName(task);

    if (name == NULL) {
        vPrintCsvField("-");
    } else {
        vPrintCsvField(name);
    }
}

static void vPrintObjectField(const void *object)
{
    char object_text[32];

    if (object == NULL) {
        vPrintCsvField("-");
        return;
    }
    (void)snprintf(object_text, sizeof(object_text), "%p", object);
    vPrintCsvField(object_text);
}

static void vDumpTrace(void)
{
    TraceEvent_t event;

    puts("sequence,tick,event,task,related_task,object,value");
    while (xTraceRead(&event) == pdPASS) {
        (void)printf("%u,%u,%s,", (unsigned)event.sequence,
                     (unsigned)event.tick,
                     pcTraceEventTypeName(event.type));
        vPrintTaskField(event.task);
        putchar(',');
        vPrintTaskField(event.related_task);
        putchar(',');
        vPrintObjectField(event.object);
        (void)printf(",%u\n", (unsigned)event.value);
    }
    (void)printf("# dropped=%u\n", (unsigned)ulTraceGetDroppedCount());
}

int main(void)
{
    trace_queue = xQueueCreate(1U, sizeof(int));
    trace_mutex = xSemaphoreCreateMutex();
    trace_timer = xTimerCreate("trace", 2U, pdTRUE, NULL,
                               vTraceTimerCallback);
    assert(trace_queue != NULL);
    assert(trace_mutex != NULL);
    assert(trace_timer != NULL);

    vTraceReset();
    assert(xTimerStart(trace_timer, 0U) == pdPASS);
    assert(xTaskCreate(vTraceLowTask, "trace-low",
                       configMINIMAL_STACK_SIZE, NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vTraceMediumTask, "trace-medium",
                       configMINIMAL_STACK_SIZE, NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vTraceHighTask, "trace-high",
                       configMINIMAL_STACK_SIZE, NULL, 3U, NULL) == pdPASS);
    vTaskStartScheduler();
    vDumpTrace();
    return 0;
}
