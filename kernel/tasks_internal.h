#ifndef MINI_TASKS_INTERNAL_H
#define MINI_TASKS_INTERNAL_H

#include "task.h"
#include "portmacro.h"

typedef struct tskTaskControlBlock {
    PortContext_t port_context;
    TaskFunction_t task_code;
    void *parameters;
    uint8_t *stack;
    size_t stack_size;
    char name[configMAX_TASK_NAME_LEN];
    UBaseType_t priority;
    eTaskState state;
    UBaseType_t creation_number;
    BaseType_t is_idle;
} TCB_t;

void vTaskRunEntry(TCB_t *task);

#endif
