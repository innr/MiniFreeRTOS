#include "trace_internal.h"
#include <string.h>

#if configTRACE_ENABLED

#if configTRACE_BUFFER_LENGTH == 0U
#error "configTRACE_BUFFER_LENGTH must be non-zero when tracing is enabled"
#endif

static TraceEvent_t trace_buffer[configTRACE_BUFFER_LENGTH];
static UBaseType_t trace_write_index;
static UBaseType_t trace_count;
static uint32_t trace_dropped;
static uint32_t trace_next_sequence;

static void prvRecordEvent(TraceEventType_t type,
                           TaskHandle_t task,
                           TaskHandle_t related_task,
                           const void *object,
                           UBaseType_t value,
                           TickType_t tick)
{
    TraceEvent_t *slot = &trace_buffer[trace_write_index];

    slot->sequence = trace_next_sequence++;
    slot->tick = tick;
    slot->type = type;
    slot->task = task;
    slot->related_task = related_task;
    slot->object = object;
    slot->value = value;
    trace_write_index = (trace_write_index + 1U) % configTRACE_BUFFER_LENGTH;
    if (trace_count < configTRACE_BUFFER_LENGTH) {
        ++trace_count;
    } else {
        ++trace_dropped;
    }
}

#endif

void vTraceRecordEvent(TraceEventType_t type,
                       TaskHandle_t task,
                       TaskHandle_t related_task,
                       const void *object,
                       UBaseType_t value)
{
#if configTRACE_ENABLED
    if (xPortIsInsideISR() == pdTRUE) {
        vTraceRecordEventFromISR(type, task, related_task, object, value,
                                 xTaskGetTickCount());
        return;
    }
    taskENTER_CRITICAL();
    prvRecordEvent(type, task, related_task, object, value,
                   xTaskGetTickCount());
    taskEXIT_CRITICAL();
#else
    (void)type;
    (void)task;
    (void)related_task;
    (void)object;
    (void)value;
#endif
}

void vTraceRecordEventFromISR(TraceEventType_t type,
                              TaskHandle_t task,
                              TaskHandle_t related_task,
                              const void *object,
                              UBaseType_t value,
                              TickType_t tick)
{
#if configTRACE_ENABLED
    /* The POSIX tick signal is blocked while its handler is running. */
    prvRecordEvent(type, task, related_task, object, value, tick);
#else
    (void)type;
    (void)task;
    (void)related_task;
    (void)object;
    (void)value;
    (void)tick;
#endif
}

void vTraceReset(void)
{
#if configTRACE_ENABLED
    taskENTER_CRITICAL();
    (void)memset(trace_buffer, 0, sizeof(trace_buffer));
    trace_write_index = 0U;
    trace_count = 0U;
    trace_dropped = 0U;
    trace_next_sequence = 0U;
    taskEXIT_CRITICAL();
#endif
}

BaseType_t xTraceRead(TraceEvent_t *event)
{
#if configTRACE_ENABLED
    UBaseType_t oldest_index;

    if (event == NULL) {
        return pdFAIL;
    }
    taskENTER_CRITICAL();
    if (trace_count == 0U) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    oldest_index = (trace_write_index + configTRACE_BUFFER_LENGTH -
                    trace_count) % configTRACE_BUFFER_LENGTH;
    *event = trace_buffer[oldest_index];
    --trace_count;
    taskEXIT_CRITICAL();
    return pdPASS;
#else
    (void)event;
    return pdFAIL;
#endif
}

UBaseType_t uxTraceGetCount(void)
{
#if configTRACE_ENABLED
    UBaseType_t count;

    taskENTER_CRITICAL();
    count = trace_count;
    taskEXIT_CRITICAL();
    return count;
#else
    return 0U;
#endif
}

uint32_t ulTraceGetDroppedCount(void)
{
#if configTRACE_ENABLED
    uint32_t dropped;

    taskENTER_CRITICAL();
    dropped = trace_dropped;
    taskEXIT_CRITICAL();
    return dropped;
#else
    return 0U;
#endif
}

const char *pcTraceEventTypeName(TraceEventType_t type)
{
    switch (type) {
    case eTraceTaskSwitch:
        return "task-switch";
    case eTraceTaskReady:
        return "task-ready";
    case eTraceTaskBlocked:
        return "task-blocked";
    case eTraceTaskWake:
        return "task-wake";
    case eTraceTaskDeleted:
        return "task-deleted";
    case eTraceTick:
        return "tick";
    case eTraceQueueSend:
        return "queue-send";
    case eTraceQueueReceive:
        return "queue-receive";
    case eTraceSemaphoreTake:
        return "semaphore-take";
    case eTraceSemaphoreGive:
        return "semaphore-give";
    case eTraceMutexInherit:
        return "mutex-inherit";
    case eTraceMutexRelease:
        return "mutex-release";
    case eTraceTimerCommandQueued:
        return "timer-command-queued";
    case eTraceTimerCommandApplied:
        return "timer-command-applied";
    case eTraceTimerCallback:
        return "timer-callback";
    default:
        return "unknown";
    }
}
