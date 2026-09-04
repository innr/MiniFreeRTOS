#include "tasks_internal.h"
#include "portable.h"
#include "queue.h"
#include "timers.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if configMAX_TIMER_NAME_LEN == 0U
#error "configMAX_TIMER_NAME_LEN must be non-zero"
#endif

#if configTIMER_QUEUE_LENGTH == 0U
#error "configTIMER_QUEUE_LENGTH must be non-zero"
#endif

#if configTIMER_TASK_PRIORITY >= configMAX_PRIORITIES
#error "configTIMER_TASK_PRIORITY must be less than configMAX_PRIORITIES"
#endif

typedef enum {
    eTimerCommandStart,
    eTimerCommandStop,
    eTimerCommandReset,
    eTimerCommandChangePeriod
} TimerCommandKind_t;

typedef struct {
    TimerCommandKind_t kind;
    TimerHandle_t timer;
    TickType_t period;
} TimerCommand_t;

struct mini_timer {
    char name[configMAX_TIMER_NAME_LEN];
    TickType_t period;
    TickType_t expiry_tick;
    BaseType_t auto_reload;
    BaseType_t active;
    void *timer_id;
    TimerCallbackFunction_t callback;
    struct mini_timer *next_active;
    struct mini_timer *next_registry;
};

static QueueHandle_t timer_command_queue;
static TaskHandle_t timer_service_task;
static BaseType_t timer_service_initialized;
static TimerHandle_t timer_registry;
static TimerHandle_t active_timers;

static void vTimerServiceTask(void *parameters);

static BaseType_t prvTimerIsRegistered(TimerHandle_t timer)
{
    for (TimerHandle_t cursor = timer_registry; cursor != NULL;
         cursor = cursor->next_registry) {
        if (cursor == timer) {
            return pdTRUE;
        }
    }
    return pdFALSE;
}

static BaseType_t prvTimerPeriodIsValid(TickType_t period)
{
    return ((period > 0U) &&
            (period < (TickType_t)INT32_MAX)) ? pdTRUE : pdFALSE;
}

static BaseType_t prvEnsureTimerService(void)
{
    QueueHandle_t command_queue;
    TaskHandle_t service_task = NULL;

    taskENTER_CRITICAL();
    if (timer_service_initialized == pdTRUE) {
        taskEXIT_CRITICAL();
        return pdPASS;
    }

    command_queue = xQueueCreate(configTIMER_QUEUE_LENGTH,
                                 (UBaseType_t)sizeof(TimerCommand_t));
    if (command_queue == NULL) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    if (xTaskCreateSystem(vTimerServiceTask,
                          "TIMER",
                          configTIMER_TASK_STACK_DEPTH,
                          NULL,
                          configTIMER_TASK_PRIORITY,
                          &service_task) != pdPASS) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    timer_command_queue = command_queue;
    timer_service_task = service_task;
    timer_service_initialized = pdTRUE;
    taskEXIT_CRITICAL();
    return pdPASS;
}

TimerHandle_t xTimerCreate(const char *name,
                           TickType_t period,
                           BaseType_t auto_reload,
                           void *timer_id,
                           TimerCallbackFunction_t callback)
{
    TimerHandle_t timer;

    if ((name == NULL) || (callback == NULL) ||
        (prvTimerPeriodIsValid(period) == pdFALSE) ||
        ((auto_reload != pdFALSE) && (auto_reload != pdTRUE))) {
        return NULL;
    }
    if (prvEnsureTimerService() != pdPASS) {
        return NULL;
    }

    timer = pvPortMalloc(sizeof(*timer));
    if (timer == NULL) {
        return NULL;
    }
    (void)memset(timer, 0, sizeof(*timer));
    (void)snprintf(timer->name, sizeof(timer->name), "%s", name);
    timer->period = period;
    timer->auto_reload = auto_reload;
    timer->timer_id = timer_id;
    timer->callback = callback;

    taskENTER_CRITICAL();
    timer->next_registry = timer_registry;
    timer_registry = timer;
    taskEXIT_CRITICAL();
    return timer;
}

static BaseType_t prvSendTimerCommand(TimerHandle_t timer,
                                      TimerCommandKind_t kind,
                                      TickType_t period,
                                      TickType_t ticks_to_wait)
{
    TimerCommand_t command;
    TaskHandle_t current_task;
    BaseType_t valid;

    if ((ticks_to_wait >= (TickType_t)INT32_MAX) ||
        (timer_command_queue == NULL)) {
        return pdFAIL;
    }
    current_task = xTaskGetCurrentTaskHandle();
    if ((current_task == NULL) && (ticks_to_wait != 0U)) {
        return pdFAIL;
    }
    if ((current_task == timer_service_task) && (ticks_to_wait != 0U)) {
        return pdFAIL;
    }

    taskENTER_CRITICAL();
    valid = prvTimerIsRegistered(timer);
    taskEXIT_CRITICAL();
    if (valid == pdFALSE) {
        return pdFAIL;
    }

    command.kind = kind;
    command.timer = timer;
    command.period = period;
    return xQueueSend(timer_command_queue, &command, ticks_to_wait);
}

BaseType_t xTimerStart(TimerHandle_t timer, TickType_t ticks_to_wait)
{
    return prvSendTimerCommand(timer, eTimerCommandStart, 0U, ticks_to_wait);
}

BaseType_t xTimerStop(TimerHandle_t timer, TickType_t ticks_to_wait)
{
    return prvSendTimerCommand(timer, eTimerCommandStop, 0U, ticks_to_wait);
}

BaseType_t xTimerReset(TimerHandle_t timer, TickType_t ticks_to_wait)
{
    return prvSendTimerCommand(timer, eTimerCommandReset, 0U, ticks_to_wait);
}

BaseType_t xTimerChangePeriod(TimerHandle_t timer,
                              TickType_t new_period,
                              TickType_t ticks_to_wait)
{
    if (prvTimerPeriodIsValid(new_period) == pdFALSE) {
        return pdFAIL;
    }
    return prvSendTimerCommand(timer, eTimerCommandChangePeriod,
                               new_period, ticks_to_wait);
}

BaseType_t xTimerIsTimerActive(TimerHandle_t timer)
{
    BaseType_t result = pdFALSE;

    if (timer == NULL) {
        return pdFALSE;
    }
    taskENTER_CRITICAL();
    if (prvTimerIsRegistered(timer) == pdTRUE) {
        result = timer->active;
    }
    taskEXIT_CRITICAL();
    return result;
}

void *pvTimerGetTimerID(TimerHandle_t timer)
{
    void *timer_id = NULL;

    if (timer == NULL) {
        return NULL;
    }
    taskENTER_CRITICAL();
    if (prvTimerIsRegistered(timer) == pdTRUE) {
        timer_id = timer->timer_id;
    }
    taskEXIT_CRITICAL();
    return timer_id;
}

void vTimerSetTimerID(TimerHandle_t timer, void *timer_id)
{
    if (timer == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    if (prvTimerIsRegistered(timer) == pdTRUE) {
        timer->timer_id = timer_id;
    }
    taskEXIT_CRITICAL();
}

static void prvActiveTimerInsert(TimerHandle_t timer)
{
    configASSERT(timer->active == pdFALSE);
    timer->next_active = active_timers;
    active_timers = timer;
    timer->active = pdTRUE;
}

static void prvActiveTimerRemove(TimerHandle_t timer)
{
    TimerHandle_t previous = NULL;
    TimerHandle_t cursor = active_timers;

    configASSERT(timer->active == pdTRUE);
    while ((cursor != NULL) && (cursor != timer)) {
        previous = cursor;
        cursor = cursor->next_active;
    }
    configASSERT(cursor == timer);
    if (previous == NULL) {
        active_timers = timer->next_active;
    } else {
        previous->next_active = timer->next_active;
    }
    timer->next_active = NULL;
    timer->active = pdFALSE;
}

static TimerHandle_t prvFindDueTimer(TickType_t now)
{
    for (TimerHandle_t timer = active_timers; timer != NULL;
         timer = timer->next_active) {
        if ((int32_t)(timer->expiry_tick - now) <= 0) {
            return timer;
        }
    }
    return NULL;
}

static TickType_t prvTimerWaitTicks(TickType_t now)
{
    TickType_t wait_ticks = (TickType_t)(INT32_MAX - 1);

    for (TimerHandle_t timer = active_timers; timer != NULL;
         timer = timer->next_active) {
        int32_t delta = (int32_t)(timer->expiry_tick - now);
        if (delta <= 0) {
            return 0U;
        }
        if ((TickType_t)delta < wait_ticks) {
            wait_ticks = (TickType_t)delta;
        }
    }
    return wait_ticks;
}

static void prvApplyTimerCommand(const TimerCommand_t *command)
{
    TimerHandle_t timer = command->timer;
    TickType_t now = xTaskGetTickCount();

    configASSERT(timer != NULL);
    configASSERT(prvTimerIsRegistered(timer) == pdTRUE);
    switch (command->kind) {
    case eTimerCommandStart:
    case eTimerCommandReset:
        if (timer->active == pdTRUE) {
            prvActiveTimerRemove(timer);
        }
        timer->expiry_tick = now + timer->period;
        prvActiveTimerInsert(timer);
        break;
    case eTimerCommandStop:
        if (timer->active == pdTRUE) {
            prvActiveTimerRemove(timer);
        }
        break;
    case eTimerCommandChangePeriod:
        configASSERT(prvTimerPeriodIsValid(command->period) == pdTRUE);
        if (timer->active == pdTRUE) {
            prvActiveTimerRemove(timer);
        }
        timer->period = command->period;
        timer->expiry_tick = now + timer->period;
        prvActiveTimerInsert(timer);
        break;
    default:
        configASSERT(pdFALSE);
        break;
    }
}

static void prvExpireDueTimers(void)
{
    for (;;) {
        TimerHandle_t timer;
        TickType_t previous_expiry;
        BaseType_t auto_reload;
        TimerCallbackFunction_t callback;

        taskENTER_CRITICAL();
        timer = prvFindDueTimer(xTaskGetTickCount());
        if (timer == NULL) {
            taskEXIT_CRITICAL();
            return;
        }
        previous_expiry = timer->expiry_tick;
        auto_reload = timer->auto_reload;
        callback = timer->callback;
        prvActiveTimerRemove(timer);
        taskEXIT_CRITICAL();

        callback(timer);

        taskENTER_CRITICAL();
        if ((auto_reload == pdTRUE) && (timer->active == pdFALSE)) {
            timer->expiry_tick = previous_expiry + timer->period;
            prvActiveTimerInsert(timer);
        }
        taskEXIT_CRITICAL();
    }
}

static void vTimerServiceTask(void *parameters)
{
    (void)parameters;
    for (;;) {
        TimerCommand_t command;
        TickType_t wait_ticks;

        taskENTER_CRITICAL();
        wait_ticks = prvTimerWaitTicks(xTaskGetTickCount());
        taskEXIT_CRITICAL();
        if (xQueueReceive(timer_command_queue, &command, wait_ticks) == pdPASS) {
            taskENTER_CRITICAL();
            prvApplyTimerCommand(&command);
            taskEXIT_CRITICAL();
        } else {
            prvExpireDueTimers();
        }
    }
}
