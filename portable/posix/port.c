#include "tasks_internal.h"
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>

static struct sigaction old_tick_action;
static sigset_t tick_signal_set;
static unsigned critical_nesting;
static BaseType_t tick_started;

static ucontext_t scheduler_context;

static void prvTickSignalHandler(int signal_number)
{
    (void)signal_number;
    vTaskTickISR();
}

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

void vPortYieldFromISR(struct tskTaskControlBlock *task)
{
    configASSERT(swapcontext(&task->port_context.native, &scheduler_context) == 0);
}

void vPortEnterCritical(void)
{
    if (critical_nesting == 0U) {
        configASSERT(sigprocmask(SIG_BLOCK, &tick_signal_set, NULL) == 0);
    }
    ++critical_nesting;
}

void vPortExitCritical(void)
{
    configASSERT(critical_nesting > 0U);
    --critical_nesting;
    if (critical_nesting == 0U) {
        configASSERT(sigprocmask(SIG_UNBLOCK, &tick_signal_set, NULL) == 0);
    }
}

BaseType_t xPortStartTick(void)
{
    struct sigaction action;
    struct itimerval timer;
    long interval_us = 1000000L / (long)configTICK_RATE_HZ;

    if (interval_us <= 0L) {
        return pdFAIL;
    }

    sigemptyset(&tick_signal_set);
    sigaddset(&tick_signal_set, SIGALRM);
    action.sa_handler = prvTickSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGALRM, &action, &old_tick_action) != 0) {
        return pdFAIL;
    }

    timer.it_value.tv_sec = interval_us / 1000000L;
    timer.it_value.tv_usec = interval_us % 1000000L;
    timer.it_interval = timer.it_value;
    if (setitimer(ITIMER_REAL, &timer, NULL) != 0) {
        (void)sigaction(SIGALRM, &old_tick_action, NULL);
        return pdFAIL;
    }
    tick_started = pdTRUE;
    return pdPASS;
}

void vPortStopTick(void)
{
    struct itimerval timer = {0};
    if (tick_started == pdTRUE) {
        (void)setitimer(ITIMER_REAL, &timer, NULL);
        (void)sigaction(SIGALRM, &old_tick_action, NULL);
        tick_started = pdFALSE;
    }
}
