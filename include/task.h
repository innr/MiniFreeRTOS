#ifndef MINI_TASK_H
#define MINI_TASK_H

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);
typedef struct tskTaskControlBlock *TaskHandle_t;

#define tskIDLE_PRIORITY ((UBaseType_t)0U)

typedef enum {
    eRunning = 0,
    eReady,
    eBlocked,
    eSuspended,
    eDeleted,
    eInvalid
} eTaskState;

BaseType_t xTaskCreate(TaskFunction_t task_code,
                       const char *name,
                       uint32_t stack_depth,
                       void *parameters,
                       UBaseType_t priority,
                       TaskHandle_t *created_task);
void vTaskStartScheduler(void);
void vTaskEndScheduler(void);
void vTaskYield(void);
#define taskYIELD() vTaskYield()

TaskHandle_t xTaskGetCurrentTaskHandle(void);
const char *pcTaskGetName(TaskHandle_t task);
eTaskState eTaskGetState(TaskHandle_t task);
UBaseType_t uxTaskGetNumberOfTasks(void);
void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority);
UBaseType_t uxTaskPriorityGet(TaskHandle_t task);

#endif
