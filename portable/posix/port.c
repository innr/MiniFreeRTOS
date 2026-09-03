#include "tasks_internal.h"
#include <stdint.h>

static ucontext_t scheduler_context;

static void prvPortTaskTrampoline(uintptr_t raw_task)
{
    vTaskRunEntry((TCB_t *)raw_task);
}

void vPortInitialiseTask(struct tskTaskControlBlock *task)
{
    configASSERT(task != NULL);
    configASSERT(getcontext(&task->port_context.native) == 0);
    task->port_context.native.uc_stack.ss_sp = task->stack;
    task->port_context.native.uc_stack.ss_size = task->stack_size;
    task->port_context.native.uc_link = &scheduler_context;
    makecontext(&task->port_context.native,
                (void (*)(void))prvPortTaskTrampoline,
                1,
                (uintptr_t)task);
}

void vPortRunTask(struct tskTaskControlBlock *task)
{
    configASSERT(swapcontext(&scheduler_context, &task->port_context.native) == 0);
}

void vPortYieldTask(struct tskTaskControlBlock *task)
{
    configASSERT(swapcontext(&task->port_context.native, &scheduler_context) == 0);
}

