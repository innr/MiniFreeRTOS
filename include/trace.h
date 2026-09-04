#ifndef MINI_TRACE_H
#define MINI_TRACE_H

#include "task.h"

typedef enum {
    eTraceTaskSwitch,
    eTraceTaskReady,
    eTraceTaskBlocked,
    eTraceTaskWake,
    eTraceTaskDeleted,
    eTraceTick,
    eTraceQueueSend,
    eTraceQueueReceive,
    eTraceSemaphoreTake,
    eTraceSemaphoreGive,
    eTraceMutexInherit,
    eTraceMutexRelease,
    eTraceTimerCommandQueued,
    eTraceTimerCommandApplied,
    eTraceTimerCallback
} TraceEventType_t;

typedef struct {
    uint32_t sequence;
    TickType_t tick;
    TraceEventType_t type;
    TaskHandle_t task;
    TaskHandle_t related_task;
    const void *object;
    UBaseType_t value;
} TraceEvent_t;

void vTraceReset(void);
BaseType_t xTraceRead(TraceEvent_t *event);
UBaseType_t uxTraceGetCount(void);
uint32_t ulTraceGetDroppedCount(void);
const char *pcTraceEventTypeName(TraceEventType_t type);

#endif
