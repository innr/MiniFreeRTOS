#include "tasks_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TCB_t *task_table[configMAX_TASKS + 1U];
static ReadyList_t ready_lists[configMAX_PRIORITIES];
static UBaseType_t task_count;
static UBaseType_t application_task_count;
static TCB_t *current_task;
static TCB_t *idle_task;
static BaseType_t scheduler_running;
static BaseType_t task_context_active;
static volatile TickType_t tick_count;

static void prvReadyListInsert(TCB_t *task);
static void prvReadyListRemove(TCB_t *task);

void vAssertCalled(const char *file, int line)
{
    (void)fprintf(stderr, "MiniFreeRTOS assertion failed at %s:%d\n", file, line);
    abort();
}

BaseType_t xTaskCreate(TaskFunction_t task_code,
                       const char *name,
                       uint32_t stack_depth,
                       void *parameters,
                       UBaseType_t priority,
                       TaskHandle_t *created_task)
{
    if ((task_code == NULL) || (name == NULL) ||
        (stack_depth < configMINIMAL_STACK_SIZE) ||
        (priority >= configMAX_PRIORITIES) ||
        (application_task_count >= configMAX_TASKS)) {
        return pdFAIL;
    }

    TCB_t *task = calloc(1U, sizeof(*task));
    if (task == NULL) {
        return pdFAIL;
    }
    task->stack = malloc(stack_depth);
    if (task->stack == NULL) {
        free(task);
        return pdFAIL;
    }

    task->task_code = task_code;
    task->parameters = parameters;
    task->stack_size = stack_depth;
    task->priority = priority;
    task->state = eReady;
    task->creation_number = application_task_count;
    (void)snprintf(task->name, sizeof(task->name), "%s", name);
    vPortInitialiseTask(task);
    task_table[task_count++] = task;
    ++application_task_count;
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

static BaseType_t prvAllApplicationTasksDeleted(void)
{
    for (UBaseType_t index = 0U; index < task_count; ++index) {
        if (task_table[index]->is_idle == pdFALSE &&
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
        taskYIELD();
    }
}

static BaseType_t prvCreateIdleTask(void)
{
    if (idle_task != NULL) {
        return pdPASS;
    }
    TCB_t *task = calloc(1U, sizeof(*task));
    if (task == NULL) {
        return pdFAIL;
    }
    task->stack = malloc(configMINIMAL_STACK_SIZE);
    if (task->stack == NULL) {
        free(task);
        return pdFAIL;
    }
    task->task_code = prvIdleTask;
    task->stack_size = configMINIMAL_STACK_SIZE;
    task->priority = tskIDLE_PRIORITY;
    task->state = eReady;
    task->is_idle = pdTRUE;
    (void)snprintf(task->name, sizeof(task->name), "%s", "IDLE");
    vPortInitialiseTask(task);
    task_table[task_count++] = task;
    prvReadyListInsert(task);
    idle_task = task;
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
    while (scheduler_running == pdTRUE) {
        TCB_t *next = prvSelectNextReadyTask();
        if (next == NULL) {
            break;
        }
        current_task = next;
        current_task->state = eRunning;
        task_context_active = pdTRUE;
        vPortRunTask(current_task);
        task_context_active = pdFALSE;
        current_task = NULL;
    }
#if configUSE_PREEMPTION
    vPortStopTick();
#endif
    scheduler_running = pdFALSE;
}

void vTaskEndScheduler(void)
{
    scheduler_running = pdFALSE;
    if (current_task != NULL) {
        current_task->state = eReady;
        prvReadyListInsert(current_task);
        vPortYieldTask(current_task);
    }
}

void vTaskYield(void)
{
    configASSERT(scheduler_running == pdTRUE);
    configASSERT(current_task != NULL);
    current_task->state = eReady;
    prvReadyListInsert(current_task);
    vPortYieldTask(current_task);
}

void vTaskRunEntry(TCB_t *task)
{
    configASSERT(task == current_task);
    task->task_code(task->parameters);
    task->state = eDeleted;
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

void vTaskTickISR(void)
{
    ++tick_count;
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
    configASSERT(selected != NULL);
    configASSERT(priority < configMAX_PRIORITIES);
    if (selected->state == eReady) {
        prvReadyListRemove(selected);
        selected->priority = priority;
        prvReadyListInsert(selected);
        return;
    }
    selected->priority = priority;
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t task)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    configASSERT(selected != NULL);
    return selected->priority;
}
