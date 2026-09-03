#include "tasks_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TCB_t *task_table[configMAX_TASKS];
static UBaseType_t task_count;
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
        (task_count >= configMAX_TASKS)) {
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
    task->creation_number = task_count;
    (void)snprintf(task->name, sizeof(task->name), "%s", name);
    vPortInitialiseTask(task);
    task_table[task_count++] = task;

    if (created_task != NULL) {
        *created_task = task;
    }
    return pdPASS;
}

static TCB_t *prvSelectNextReadyTask(void)
{
    for (UBaseType_t checked = 0U; checked < task_count; ++checked) {
        UBaseType_t index = (next_task_index + checked) % task_count;
        if (task_table[index]->state == eReady) {
            next_task_index = (index + 1U) % task_count;
            return task_table[index];
        }
    }
    return NULL;
}

void vTaskStartScheduler(void)
{
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

