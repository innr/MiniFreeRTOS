#include "tasks_internal.h"
#include "portable.h"
#include "trace_internal.h"
#include <string.h>

_Static_assert(configMAX_SYSTEM_TASKS >= 2U,
               "configMAX_SYSTEM_TASKS must reserve idle and timer tasks");

static TCB_t *task_table[configMAX_TASKS + configMAX_SYSTEM_TASKS];
static ReadyList_t ready_lists[configMAX_PRIORITIES];
static UBaseType_t task_count;
static UBaseType_t application_task_count;
static TCB_t *current_task;
static TCB_t *idle_task;
static BaseType_t scheduler_running;
static BaseType_t task_context_active;
static volatile TickType_t tick_count;

typedef struct {
    TCB_t *head;
    TCB_t *tail;
    UBaseType_t length;
} DelayList_t;

static DelayList_t delayed_list;

static void prvReadyListInsert(TCB_t *task);
static void prvReadyListRemove(TCB_t *task);
static void prvDelayListInsert(TCB_t *task);
static void prvDelayListRemove(TCB_t *task);
static void prvWakeExpiredTasks(void);
static void prvEventListInsert(TaskEventList_t *list, TCB_t *task);
static void prvEventListRemove(TCB_t *task);
static TCB_t *prvSelectNextReadyTask(void);
static BaseType_t prvCreateTask(TaskFunction_t task_code,
                                const char *name,
                                uint32_t stack_depth,
                                void *parameters,
                                UBaseType_t priority,
                                BaseType_t is_system,
                                BaseType_t is_idle,
                                TaskHandle_t *created_task);

static void prvCopyName(char *destination, size_t destination_size,
                        const char *source)
{
    size_t index = 0U;

    if (destination_size == 0U) {
        return;
    }
    while ((index + 1U < destination_size) && (source[index] != '\0')) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static BaseType_t prvTickBefore(TickType_t lhs, TickType_t rhs)
{
    return ((int32_t)(lhs - rhs) < 0) ? pdTRUE : pdFALSE;
}

static BaseType_t prvTickReached(TickType_t now, TickType_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? pdTRUE : pdFALSE;
}

void vAssertCalled(const char *file, int line)
{
    vPortAssertCalled(file, line);
}

BaseType_t xTaskCreate(TaskFunction_t task_code,
                       const char *name,
                       uint32_t stack_depth,
                       void *parameters,
                       UBaseType_t priority,
                       TaskHandle_t *created_task)
{
    return prvCreateTask(task_code, name, stack_depth, parameters, priority,
                         pdFALSE, pdFALSE, created_task);
}

BaseType_t xTaskCreateSystem(TaskFunction_t task_code,
                             const char *name,
                             uint32_t stack_depth,
                             void *parameters,
                             UBaseType_t priority,
                             TaskHandle_t *created_task)
{
    return prvCreateTask(task_code, name, stack_depth, parameters, priority,
                         pdTRUE, pdFALSE, created_task);
}

static BaseType_t prvCreateTask(TaskFunction_t task_code,
                                const char *name,
                                uint32_t stack_depth,
                                void *parameters,
                                UBaseType_t priority,
                                BaseType_t is_system,
                                BaseType_t is_idle,
                                TaskHandle_t *created_task)
{
    if ((task_code == NULL) || (name == NULL) ||
        (stack_depth < configMINIMAL_STACK_SIZE) ||
        (priority >= configMAX_PRIORITIES) ||
        (task_count >= (configMAX_TASKS + configMAX_SYSTEM_TASKS)) ||
        ((is_system == pdFALSE) &&
         (application_task_count >= configMAX_TASKS)) ||
        ((is_system == pdTRUE) &&
         ((task_count - application_task_count) >= configMAX_SYSTEM_TASKS))) {
        return pdFAIL;
    }

    TCB_t *task = pvPortMalloc(sizeof(*task));
    if (task == NULL) {
        return pdFAIL;
    }
    (void)memset(task, 0, sizeof(*task));
    task->stack = pvPortMalloc(stack_depth);
    if (task->stack == NULL) {
        vPortFree(task);
        return pdFAIL;
    }

    task->task_code = task_code;
    task->parameters = parameters;
    task->stack_size = stack_depth;
    task->priority = priority;
    task->base_priority = priority;
    task->inherited_priority = tskIDLE_PRIORITY;
    task->state = eReady;
    task->creation_number = application_task_count;
    task->is_idle = is_idle;
    task->is_system = is_system;
    prvCopyName(task->name, sizeof(task->name), name);
    vPortInitialiseTask(task);
    task_table[task_count++] = task;
    if (is_system == pdFALSE) {
        ++application_task_count;
    }
    prvReadyListInsert(task);

    if (created_task != NULL) {
        *created_task = task;
    }
    return pdPASS;
}

static void prvReadyListInsert(TCB_t *task)
{
    ReadyList_t *list = &ready_lists[task->priority];
    configASSERT(task->in_ready_list == pdFALSE);
    task->ready_previous = list->tail;
    task->ready_next = NULL;
    if (list->tail != NULL) {
        list->tail->ready_next = task;
    } else {
        list->head = task;
    }
    list->tail = task;
    ++list->length;
    task->in_ready_list = pdTRUE;
    TRACE_RECORD(eTraceTaskReady, task, NULL, NULL, task->priority);
}

static void prvReadyListRemove(TCB_t *task)
{
    ReadyList_t *list = &ready_lists[task->priority];
    configASSERT(task->in_ready_list == pdTRUE);
    if (task->ready_previous != NULL) {
        task->ready_previous->ready_next = task->ready_next;
    } else {
        list->head = task->ready_next;
    }
    if (task->ready_next != NULL) {
        task->ready_next->ready_previous = task->ready_previous;
    } else {
        list->tail = task->ready_previous;
    }
    task->ready_previous = NULL;
    task->ready_next = NULL;
    task->in_ready_list = pdFALSE;
    configASSERT(list->length > 0U);
    --list->length;
}

static void prvDelayListInsert(TCB_t *task)
{
    TCB_t *cursor;

    configASSERT(task->in_delay_list == pdFALSE);
    cursor = delayed_list.head;
    while ((cursor != NULL) &&
           (prvTickBefore(task->wake_tick, cursor->wake_tick) == pdFALSE)) {
        cursor = cursor->delay_next;
    }

    if (cursor == NULL) {
        task->delay_previous = delayed_list.tail;
        task->delay_next = NULL;
        if (delayed_list.tail != NULL) {
            delayed_list.tail->delay_next = task;
        } else {
            delayed_list.head = task;
        }
        delayed_list.tail = task;
    } else {
        task->delay_previous = cursor->delay_previous;
        task->delay_next = cursor;
        if (cursor->delay_previous != NULL) {
            cursor->delay_previous->delay_next = task;
        } else {
            delayed_list.head = task;
        }
        cursor->delay_previous = task;
    }
    ++delayed_list.length;
    task->in_delay_list = pdTRUE;
}

static void prvDelayListRemove(TCB_t *task)
{
    configASSERT(task->in_delay_list == pdTRUE);
    if (task->delay_previous != NULL) {
        task->delay_previous->delay_next = task->delay_next;
    } else {
        delayed_list.head = task->delay_next;
    }
    if (task->delay_next != NULL) {
        task->delay_next->delay_previous = task->delay_previous;
    } else {
        delayed_list.tail = task->delay_previous;
    }
    task->delay_previous = NULL;
    task->delay_next = NULL;
    task->in_delay_list = pdFALSE;
    configASSERT(delayed_list.length > 0U);
    --delayed_list.length;
}

static void prvEventListInsert(TaskEventList_t *list, TCB_t *task)
{
    TCB_t *cursor;

    configASSERT(list != NULL);
    configASSERT(task->event_list == NULL);
    cursor = list->head;
    while ((cursor != NULL) && (cursor->priority >= task->priority)) {
        cursor = cursor->event_next;
    }

    if (cursor == NULL) {
        task->event_previous = list->tail;
        task->event_next = NULL;
        if (list->tail != NULL) {
            list->tail->event_next = task;
        } else {
            list->head = task;
        }
        list->tail = task;
    } else {
        task->event_previous = cursor->event_previous;
        task->event_next = cursor;
        if (cursor->event_previous != NULL) {
            cursor->event_previous->event_next = task;
        } else {
            list->head = task;
        }
        cursor->event_previous = task;
    }
    ++list->length;
    task->event_list = list;
}

static void prvEventListRemove(TCB_t *task)
{
    TaskEventList_t *list = task->event_list;

    configASSERT(list != NULL);
    if (task->event_previous != NULL) {
        task->event_previous->event_next = task->event_next;
    } else {
        list->head = task->event_next;
    }
    if (task->event_next != NULL) {
        task->event_next->event_previous = task->event_previous;
    } else {
        list->tail = task->event_previous;
    }
    task->event_previous = NULL;
    task->event_next = NULL;
    task->event_list = NULL;
    configASSERT(list->length > 0U);
    --list->length;
}

void vTaskEventListInit(TaskEventList_t *list)
{
    configASSERT(list != NULL);
    list->head = NULL;
    list->tail = NULL;
    list->length = 0U;
}

void vTaskSetEffectivePriority(TCB_t *task, UBaseType_t priority)
{
    TaskEventList_t *event_list;

    configASSERT(task != NULL);
    configASSERT(priority < configMAX_PRIORITIES);
    if (task->priority == priority) {
        return;
    }

    if (task->state == eReady) {
        prvReadyListRemove(task);
        task->priority = priority;
        prvReadyListInsert(task);
        return;
    }

    if ((task->state == eBlocked) && (task->event_list != NULL)) {
        event_list = task->event_list;
        prvEventListRemove(task);
        task->priority = priority;
        prvEventListInsert(event_list, task);
        return;
    }

    task->priority = priority;
}

void vTaskSetInheritedPriority(TCB_t *task, UBaseType_t priority)
{
    UBaseType_t effective_priority;

    configASSERT(task != NULL);
    configASSERT(priority < configMAX_PRIORITIES);
    task->inherited_priority = priority;
    effective_priority = task->base_priority;
    if (task->inherited_priority > effective_priority) {
        effective_priority = task->inherited_priority;
    }
    vTaskSetEffectivePriority(task, effective_priority);
}

void vTaskBlockCurrent(TaskEventList_t *list,
                       void *wait_object,
                       TaskWaitReason_t wait_reason,
                       TickType_t ticks_to_wait)
{
    TCB_t *task = current_task;

    configASSERT(scheduler_running == pdTRUE);
    configASSERT(task != NULL);
    configASSERT(task->state == eRunning);
    configASSERT(list != NULL);
    configASSERT(task->event_list == NULL);
    configASSERT(task->in_delay_list == pdFALSE);
    configASSERT(ticks_to_wait > 0U);
    configASSERT(ticks_to_wait < (TickType_t)INT32_MAX);

    task->state = eBlocked;
    task->wait_object = wait_object;
    task->wait_reason = wait_reason;
    task->wait_result = pdFALSE;
    task->wait_has_timeout = pdTRUE;
    task->wake_tick = tick_count + ticks_to_wait;
    prvEventListInsert(list, task);
    prvDelayListInsert(task);
    TRACE_RECORD(eTraceTaskBlocked, task, NULL, wait_object,
                 (UBaseType_t)wait_reason);
}

BaseType_t xTaskUnblockOne(TaskEventList_t *list)
{
    TCB_t *task;

    configASSERT(list != NULL);
    task = list->head;
    if (task == NULL) {
        return pdFALSE;
    }
    configASSERT(task->state == eBlocked);
    prvEventListRemove(task);
    if (task->in_delay_list == pdTRUE) {
        prvDelayListRemove(task);
    }
    task->wait_result = pdTRUE;
    task->state = eReady;
    prvReadyListInsert(task);
    TRACE_RECORD(eTraceTaskWake, task, NULL, task->wait_object,
                 (UBaseType_t)task->wait_reason);
#if configUSE_PREEMPTION
    if ((current_task != NULL) && (current_task->state == eRunning) &&
        (task->priority > current_task->priority)) {
        return pdTRUE;
    }
#endif
    return pdFALSE;
}

TickType_t xTaskGetWaitRemaining(TCB_t *task)
{
    configASSERT(task != NULL);
    if ((task->wait_has_timeout == pdFALSE) ||
        (prvTickBefore(tick_count, task->wake_tick) == pdFALSE)) {
        return 0U;
    }
    return task->wake_tick - tick_count;
}

void vTaskClearWaitState(TCB_t *task)
{
    configASSERT(task != NULL);
    configASSERT(task->event_list == NULL);
    configASSERT(task->in_delay_list == pdFALSE);
    task->wake_tick = 0U;
    task->wait_object = NULL;
    task->wait_reason = eTaskWaitNone;
    task->wait_result = pdFALSE;
    task->wait_has_timeout = pdFALSE;
}

static void prvWakeExpiredTasks(void)
{
    while ((delayed_list.head != NULL) &&
           (prvTickReached(tick_count, delayed_list.head->wake_tick) == pdTRUE)) {
        TCB_t *task = delayed_list.head;
        configASSERT(task->state == eBlocked);
        prvDelayListRemove(task);
        if (task->event_list != NULL) {
            prvEventListRemove(task);
            vTaskWaitEnded(task);
        }
        task->wait_result = pdFALSE;
        task->state = eReady;
        prvReadyListInsert(task);
        TRACE_RECORD(eTraceTaskWake, task, NULL, task->wait_object,
                     (UBaseType_t)task->wait_reason);
    }
}

static TCB_t *prvSelectNextReadyTask(void)
{
    for (int priority = (int)configMAX_PRIORITIES - 1; priority >= 0; --priority) {
        ReadyList_t *list = &ready_lists[priority];
        if (list->head != NULL) {
            TCB_t *selected = list->head;
            prvReadyListRemove(selected);
            return selected;
        }
    }
    return NULL;
}

TCB_t *pxTaskGetCurrent(void)
{
    return current_task;
}

BaseType_t xTaskIsSchedulerRunning(void)
{
    return scheduler_running;
}

void vTaskSetContextActive(BaseType_t active)
{
    task_context_active = active;
}

void vTaskSwitchContext(void)
{
    TCB_t *previous_task = current_task;
    TCB_t *next_task = prvSelectNextReadyTask();

    if (next_task == NULL) {
        current_task = NULL;
        task_context_active = pdFALSE;
        return;
    }
    current_task = next_task;
    current_task->state = eRunning;
    TRACE_RECORD(eTraceTaskSwitch, current_task, previous_task, NULL,
                 current_task->priority);
}

static BaseType_t prvAllApplicationTasksDeleted(void)
{
    for (UBaseType_t index = 0U; index < task_count; ++index) {
        if ((task_table[index]->is_idle == pdFALSE) &&
            (task_table[index]->is_system == pdFALSE) &&
            task_table[index]->state != eDeleted) {
            return pdFALSE;
        }
    }
    return pdTRUE;
}

static void prvIdleTask(void *parameters)
{
    (void)parameters;
    while (scheduler_running == pdTRUE) {
        if (prvAllApplicationTasksDeleted() == pdTRUE) {
            vTaskEndScheduler();
            return;
        }
#if configUSE_PREEMPTION
        vPortWaitForTick();
#else
        taskYIELD();
#endif
    }
}

static BaseType_t prvCreateIdleTask(void)
{
    TaskHandle_t created_task = NULL;

    if (idle_task != NULL) {
        return pdPASS;
    }
    if (prvCreateTask(prvIdleTask, "IDLE", configMINIMAL_STACK_SIZE, NULL,
                      tskIDLE_PRIORITY, pdTRUE, pdTRUE,
                      &created_task) != pdPASS) {
        return pdFAIL;
    }
    idle_task = created_task;
    return pdPASS;
}

void vTaskStartScheduler(void)
{
    if ((scheduler_running == pdTRUE) || (prvCreateIdleTask() != pdPASS)) {
        return;
    }
    scheduler_running = pdTRUE;
    task_context_active = pdFALSE;
#if configUSE_PREEMPTION
    configASSERT(xPortStartTick() == pdPASS);
#endif
    vPortStartScheduler();
#if configUSE_PREEMPTION
    vPortStopTick();
#endif
    scheduler_running = pdFALSE;
    task_context_active = pdFALSE;
}

void vTaskEndScheduler(void)
{
    scheduler_running = pdFALSE;
    if (current_task != NULL) {
        current_task->state = eReady;
        prvReadyListInsert(current_task);
    }
    vPortEndScheduler();
}

void vTaskYield(void)
{
    configASSERT(scheduler_running == pdTRUE);
    configASSERT(current_task != NULL);
    current_task->state = eReady;
    prvReadyListInsert(current_task);
    vPortYieldTask(current_task);
}

void vTaskDelay(TickType_t ticks_to_delay)
{
    TCB_t *task;

    configASSERT(scheduler_running == pdTRUE);
    configASSERT(current_task != NULL);
    if (ticks_to_delay == 0U) {
        vTaskYield();
        return;
    }
    configASSERT(ticks_to_delay < (TickType_t)INT32_MAX);

    task = current_task;
    taskENTER_CRITICAL();
    task->wake_tick = tick_count + ticks_to_delay;
    task->wait_object = NULL;
    task->wait_reason = eTaskWaitNone;
    task->wait_result = pdFALSE;
    task->wait_has_timeout = pdFALSE;
    task->state = eBlocked;
    prvDelayListInsert(task);
    TRACE_RECORD(eTraceTaskBlocked, task, NULL, NULL,
                 (UBaseType_t)eTaskWaitNone);
    taskEXIT_CRITICAL();
    vPortYieldTask(task);
    vTaskClearWaitState(task);
}

void vTaskDelayUntil(TickType_t *previous_wake_time,
                     TickType_t time_increment)
{
    TickType_t next_wake_time;

    configASSERT(previous_wake_time != NULL);
    configASSERT(time_increment < (TickType_t)INT32_MAX);
    next_wake_time = *previous_wake_time + time_increment;
    *previous_wake_time = next_wake_time;
    if (prvTickBefore(tick_count, next_wake_time) == pdTRUE) {
        vTaskDelay(next_wake_time - tick_count);
    } else {
        vTaskYield();
    }
}

BaseType_t xTaskAbortDelay(TaskHandle_t task)
{
    BaseType_t should_yield = pdFALSE;

    if (task == NULL) {
        return pdFAIL;
    }
    taskENTER_CRITICAL();
    if ((task->state != eBlocked) || (task->in_delay_list == pdFALSE)) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    prvDelayListRemove(task);
    if (task->event_list != NULL) {
        prvEventListRemove(task);
        vTaskWaitEnded(task);
    }
    task->wait_result = pdFALSE;
    task->state = eReady;
    prvReadyListInsert(task);
    TRACE_RECORD(eTraceTaskWake, task, NULL, task->wait_object,
                 (UBaseType_t)task->wait_reason);
#if configUSE_PREEMPTION
    if ((scheduler_running == pdTRUE) && (current_task != NULL) &&
        (current_task->state == eRunning) &&
        (task->priority > current_task->priority)) {
        should_yield = pdTRUE;
    }
#endif
    taskEXIT_CRITICAL();

    if (should_yield == pdTRUE) {
        vTaskYield();
    }
    return pdPASS;
}

void vTaskRunEntry(TCB_t *task)
{
    configASSERT(task == current_task);
    task->task_code(task->parameters);
    configASSERT(xTaskOwnsMutex(task) == pdFALSE);
    task->state = eDeleted;
    TRACE_RECORD(eTraceTaskDeleted, task, NULL, NULL, 0U);
    vPortYieldTask(task);
    configASSERT(pdFALSE);
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return current_task;
}

const char *pcTaskGetName(TaskHandle_t task)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    return (selected != NULL) ? selected->name : NULL;
}

eTaskState eTaskGetState(TaskHandle_t task)
{
    return (task != NULL) ? task->state : eInvalid;
}

UBaseType_t uxTaskGetNumberOfTasks(void)
{
    return task_count;
}

TickType_t xTaskGetTickCount(void)
{
    return tick_count;
}

void vTaskSetTickCountForTest(TickType_t tick)
{
    configASSERT(scheduler_running == pdFALSE);
    tick_count = tick;
}

void vTaskTickISR(void)
{
    ++tick_count;
#if configTRACE_INCLUDE_TICKS
    TRACE_RECORD_TICK(tick_count, current_task);
#endif
    prvWakeExpiredTasks();
    if (scheduler_running != pdTRUE ||
        task_context_active != pdTRUE ||
        current_task == NULL) {
        return;
    }
#if configUSE_PREEMPTION
    if (current_task->state == eRunning) {
        current_task->state = eReady;
        prvReadyListInsert(current_task);
        vPortYieldFromISR(current_task);
    }
#endif
}

void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    UBaseType_t effective_priority;

    configASSERT(selected != NULL);
    configASSERT(priority < configMAX_PRIORITIES);
    taskENTER_CRITICAL();
    selected->base_priority = priority;
    effective_priority = selected->base_priority;
    if (selected->inherited_priority > effective_priority) {
        effective_priority = selected->inherited_priority;
    }
    vTaskSetEffectivePriority(selected, effective_priority);
    taskEXIT_CRITICAL();
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t task)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    configASSERT(selected != NULL);
    return selected->priority;
}
