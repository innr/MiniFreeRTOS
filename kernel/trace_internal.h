#ifndef MINI_TRACE_INTERNAL_H
#define MINI_TRACE_INTERNAL_H

#include "trace.h"
#include "portmacro.h"

void vTraceRecordEvent(TraceEventType_t type,
                       TaskHandle_t task,
                       TaskHandle_t related_task,
                       const void *object,
                       UBaseType_t value);
void vTraceRecordEventFromISR(TraceEventType_t type,
                              TaskHandle_t task,
                              TaskHandle_t related_task,
                              const void *object,
                              UBaseType_t value,
                              TickType_t tick);

#if configTRACE_ENABLED
#define TRACE_RECORD(type, task, related_task, object, value) \
    vTraceRecordEvent((type), (task), (related_task), (object), (value))
#define TRACE_RECORD_TICK(tick, task) \
    vTraceRecordEventFromISR(eTraceTick, (task), NULL, NULL, \
                             (UBaseType_t)(tick), (tick))
#else
#define TRACE_RECORD(type, task, related_task, object, value) \
    do { (void)(type); (void)(task); (void)(related_task); \
         (void)(object); (void)(value); } while (0)
#define TRACE_RECORD_TICK(tick, task) \
    do { (void)(tick); (void)(task); } while (0)
#endif

#endif
