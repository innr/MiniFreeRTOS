#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "trace.h"
#include "trace_internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static QueueHandle_t trace_queue;
static SemaphoreHandle_t trace_semaphore;
static SemaphoreHandle_t trace_mutex;
static TimerHandle_t trace_timer;
static TaskHandle_t low_task;
static TaskHandle_t high_task;
static TaskHandle_t medium_task;
static volatile BaseType_t scenario_done;
static volatile unsigned callback_count;
static volatile BaseType_t ipc_done;
static volatile BaseType_t mutex_done;

static void vTraceTimerCallback(TimerHandle_t timer)
{
    ++callback_count;
    if (callback_count >= 2U) {
        assert(xTimerStop(timer, 0U) == pdPASS);
    }
}

static void vTraceLowTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(trace_mutex, 0U) == pdPASS);
    vTaskDelay(4U);
    assert(xSemaphoreGive(trace_mutex) == pdPASS);
    mutex_done = pdTRUE;
}

static void vTraceHighTask(void *parameters)
{
    int sent = 17;
    int received = 0;

    (void)parameters;
    vTaskDelay(1U);
    assert(xSemaphoreTake(trace_mutex, 10U) == pdPASS);
    assert(xSemaphoreGive(trace_mutex) == pdPASS);
    assert(xQueueSend(trace_queue, &sent, 0U) == pdPASS);
    assert(xQueueReceive(trace_queue, &received, 0U) == pdPASS);
    assert(received == sent);
    assert(xSemaphoreGive(trace_semaphore) == pdPASS);
    assert(xSemaphoreTake(trace_semaphore, 0U) == pdPASS);
    ipc_done = pdTRUE;
    scenario_done = pdTRUE;
}

static void vTraceMediumTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    while ((scenario_done == pdFALSE) &&
           (xTaskGetTickCount() < 8U)) {
        /* Let the tick preemption and task-switch events remain observable. */
    }
}

static void test_ring_wrap(void)
{
    TraceEvent_t event;
    UBaseType_t total = configTRACE_BUFFER_LENGTH + 3U;

    vTraceReset();
    for (UBaseType_t index = 0U; index < total; ++index) {
        vTraceRecordEvent(eTraceTick, NULL, NULL, NULL, index);
    }
#if configTRACE_ENABLED
    assert(uxTraceGetCount() == configTRACE_BUFFER_LENGTH);
    assert(ulTraceGetDroppedCount() == 3U);
    for (UBaseType_t index = 0U; index < configTRACE_BUFFER_LENGTH; ++index) {
        assert(xTraceRead(&event) == pdPASS);
        assert(event.sequence == index + 3U);
        assert(event.value == index + 3U);
    }
    assert(xTraceRead(&event) == pdFAIL);
#else
    (void)event;
    assert(uxTraceGetCount() == 0U);
    assert(ulTraceGetDroppedCount() == 0U);
#endif
    assert(xTraceRead(NULL) == pdFAIL);
    vTraceReset();
    assert(uxTraceGetCount() == 0U);
    assert(ulTraceGetDroppedCount() == 0U);
    assert(strcmp(pcTraceEventTypeName(eTraceTaskSwitch),
                  "task-switch") == 0);
    assert(strcmp(pcTraceEventTypeName((TraceEventType_t)99), "unknown") == 0);
}

static void test_integrated_trace(void)
{
#if configTRACE_ENABLED
    TraceEvent_t event;
    BaseType_t saw_switch = pdFALSE;
    BaseType_t saw_block = pdFALSE;
    BaseType_t saw_wake = pdFALSE;
    BaseType_t saw_delete = pdFALSE;
    BaseType_t saw_queue_send = pdFALSE;
    BaseType_t saw_queue_receive = pdFALSE;
    BaseType_t saw_semaphore_take = pdFALSE;
    BaseType_t saw_semaphore_give = pdFALSE;
    BaseType_t saw_inherit = pdFALSE;
    BaseType_t saw_release = pdFALSE;
    BaseType_t saw_timer_queued = pdFALSE;
    BaseType_t saw_timer_applied = pdFALSE;
    BaseType_t saw_timer_callback = pdFALSE;
#endif

    trace_queue = xQueueCreate(1U, sizeof(int));
    trace_semaphore = xSemaphoreCreateBinary();
    trace_mutex = xSemaphoreCreateMutex();
    trace_timer = xTimerCreate("trace", 2U, pdTRUE, NULL,
                               vTraceTimerCallback);
    assert(trace_queue != NULL);
    assert(trace_semaphore != NULL);
    assert(trace_mutex != NULL);
    assert(trace_timer != NULL);

    assert(xTimerStart(trace_timer, 0U) == pdPASS);
    assert(xTaskCreate(vTraceLowTask, "trace-low",
                       configMINIMAL_STACK_SIZE, NULL, 1U, &low_task) == pdPASS);
    assert(xTaskCreate(vTraceMediumTask, "trace-medium",
                       configMINIMAL_STACK_SIZE, NULL, 2U,
                       &medium_task) == pdPASS);
    assert(xTaskCreate(vTraceHighTask, "trace-high",
                       configMINIMAL_STACK_SIZE, NULL, 3U, &high_task) == pdPASS);
    vTaskStartScheduler();

    assert(scenario_done == pdTRUE);
    assert(ipc_done == pdTRUE);
    assert(mutex_done == pdTRUE);
    assert(callback_count == 2U);

#if configTRACE_ENABLED
    while (xTraceRead(&event) == pdPASS) {
        switch (event.type) {
        case eTraceTaskSwitch:
            saw_switch = pdTRUE;
            break;
        case eTraceTaskBlocked:
            saw_block = pdTRUE;
            if (event.object == trace_mutex) {
                assert(event.task == high_task);
            }
            break;
        case eTraceTaskWake:
            saw_wake = pdTRUE;
            break;
        case eTraceTaskDeleted:
            if ((event.task == low_task) || (event.task == high_task) ||
                (event.task == medium_task)) {
                saw_delete = pdTRUE;
            }
            break;
        case eTraceQueueSend:
            if (event.object == trace_queue) {
                assert(event.task == high_task);
                saw_queue_send = pdTRUE;
            }
            break;
        case eTraceQueueReceive:
            if (event.object == trace_queue) {
                assert(event.task == high_task);
                saw_queue_receive = pdTRUE;
            }
            break;
        case eTraceSemaphoreTake:
            if (event.object == trace_semaphore) {
                assert(event.task == high_task);
                saw_semaphore_take = pdTRUE;
            }
            break;
        case eTraceSemaphoreGive:
            if (event.object == trace_semaphore) {
                assert(event.task == high_task);
                saw_semaphore_give = pdTRUE;
            }
            break;
        case eTraceMutexInherit:
            assert(event.object == trace_mutex);
            assert(event.task == low_task);
            assert(event.related_task == high_task);
            assert(event.value == 3U);
            saw_inherit = pdTRUE;
            break;
        case eTraceMutexRelease:
            if (event.object == trace_mutex) {
                assert((event.task == low_task) || (event.task == high_task));
                if (event.task == low_task) {
                    assert(event.related_task == high_task);
                    saw_release = pdTRUE;
                } else {
                    assert(event.related_task == NULL);
                }
            }
            break;
        case eTraceTimerCommandQueued:
            if (event.object == trace_timer) {
                saw_timer_queued = pdTRUE;
            }
            break;
        case eTraceTimerCommandApplied:
            if (event.object == trace_timer) {
                assert(event.task != NULL);
                assert(strcmp(pcTaskGetName(event.task), "TIMER") == 0);
                saw_timer_applied = pdTRUE;
            }
            break;
        case eTraceTimerCallback:
            if (event.object == trace_timer) {
                assert(event.task != NULL);
                assert(strcmp(pcTaskGetName(event.task), "TIMER") == 0);
                saw_timer_callback = pdTRUE;
            }
            break;
        default:
            break;
        }
    }

    assert(saw_switch == pdTRUE);
    assert(saw_block == pdTRUE);
    assert(saw_wake == pdTRUE);
    assert(saw_delete == pdTRUE);
    assert(saw_queue_send == pdTRUE);
    assert(saw_queue_receive == pdTRUE);
    assert(saw_semaphore_take == pdTRUE);
    assert(saw_semaphore_give == pdTRUE);
    assert(saw_inherit == pdTRUE);
    assert(saw_release == pdTRUE);
    assert(saw_timer_queued == pdTRUE);
    assert(saw_timer_applied == pdTRUE);
    assert(saw_timer_callback == pdTRUE);
    assert(uxTraceGetCount() == 0U);
#else
    assert(uxTraceGetCount() == 0U);
    assert(ulTraceGetDroppedCount() == 0U);
#endif
}

int main(void)
{
    test_ring_wrap();
    test_integrated_trace();
    puts("P8 trace tests passed");
    return 0;
}
