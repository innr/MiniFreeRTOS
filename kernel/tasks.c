#include "tasks_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TCB_t *task_table[configMAX_TASKS + 1U];
static UBaseType_t task_count;
static UBaseType_t application_task_count;
static UBaseType_t next_task_index;
static TCB_t *current_task;
static BaseType_t scheduler_running;

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

    if (created_task != NULL) {
        *created_task = task;
    }
    return pdPASS;
}

static TCB_t *prvSelectNextReadyTask(void)
{
    TCB_t *selected = NULL;
    UBaseType_t selected_priority = 0U;
    BaseType_t found = pdFALSE;

    for (UBaseType_t checked = 0U; checked < task_count; ++checked) {
        UBaseType_t index = (next_task_index + checked) % task_count;
        TCB_t *candidate = task_table[index];
        if (candidate->state != eReady) {
            continue;
        }
        if ((found == pdFALSE) || (candidate->priority > selected_priority)) {
            selected = candidate;
            selected_priority = candidate->priority;
            found = pdTRUE;
        }
    }

    if (selected != NULL) {
        for (UBaseType_t index = 0U; index < task_count; ++index) {
            if (task_table[index] == selected) {
                next_task_index = (index + 1U) % task_count;
                break;
            }
        }
    }
    return selected;
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
    return pdPASS;
}

void vTaskStartScheduler(void)
{
    if ((scheduler_running == pdTRUE) || (prvCreateIdleTask() != pdPASS)) {
        return;
    }
    scheduler_running = pdTRUE;
    while (scheduler_running == pdTRUE) {
        TCB_t *next = prvSelectNextReadyTask();
        if (next == NULL) {
            break;
        }
        current_task = next;
        current_task->state = eRunning;
        vPortRunTask(current_task);
        current_task = NULL;
    }
    scheduler_running = pdFALSE;
}

void vTaskEndScheduler(void)
{
    scheduler_running = pdFALSE;
    if (current_task != NULL) {
        current_task->state = eReady;
        vPortYieldTask(current_task);
    }
}

void vTaskYield(void)
{
    configASSERT(scheduler_running == pdTRUE);
    configASSERT(current_task != NULL);
    current_task->state = eReady;
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

void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    configASSERT(selected != NULL);
    configASSERT(priority < configMAX_PRIORITIES);
    selected->priority = priority;
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t task)
{
    TCB_t *selected = (task != NULL) ? task : current_task;
    configASSERT(selected != NULL);
    return selected->priority;
}
