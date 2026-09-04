#include "tasks_internal.h"
#include "portmacro.h"
#include <stdint.h>

#if defined(__GNUC__)
#define MINI_WEAK __attribute__((weak))
#else
#define MINI_WEAK
#endif

#define MINI_SYSTICK_CSR       (*(volatile uint32_t *)0xE000E010UL)
#define MINI_SYSTICK_RVR       (*(volatile uint32_t *)0xE000E014UL)
#define MINI_SYSTICK_CVR       (*(volatile uint32_t *)0xE000E018UL)
#define MINI_SCB_ICSR         (*(volatile uint32_t *)0xE000ED04UL)
#define MINI_SCB_SHPR3         (*(volatile uint32_t *)0xE000ED20UL)
#define MINI_SYSTICK_ENABLE    (1UL << 0)
#define MINI_SYSTICK_TICKINT   (1UL << 1)
#define MINI_SYSTICK_CLKSOURCE (1UL << 2)

static uint32_t critical_nesting;
static uint32_t critical_saved_primask;

static void prvConfigureExceptionPriorities(void)
{
    uint32_t priority = (uint32_t)configKERNEL_INTERRUPT_PRIORITY & 0xFFU;

    MINI_SCB_SHPR3 = (priority << 16) | (priority << 24);
}

static uint32_t prvReadPrimask(void)
{
    uint32_t value;

    __asm volatile("mrs %0, primask" : "=r"(value) :: "memory");
    return value;
}

static uint32_t prvReadIpsr(void)
{
    uint32_t value;

    __asm volatile("mrs %0, ipsr" : "=r"(value) :: "memory");
    return value;
}

static void prvWaitForever(void)
{
    for (;;) {
        __asm volatile("wfi" ::: "memory");
    }
}

MINI_WEAK void vBoardExit(int status)
{
    (void)status;
    prvWaitForever();
}

MINI_WEAK void vBoardAssertFailed(const char *file, int line)
{
    (void)file;
    (void)line;
}

static void prvPortTaskTrampoline(TCB_t *task)
{
    vTaskRunEntry(task);
    vPortTaskExitError();
}

void vPortInitialiseTask(struct tskTaskControlBlock *task)
{
    configASSERT(task != NULL);
    configASSERT(task->stack != NULL);
    configASSERT((task->stack_size / sizeof(uint32_t)) >=
                 MINI_CORTEX_M_STACK_FRAME_WORDS);
    vPortBuildInitialStackFrame(&task->port_context,
                                (uint32_t *)task->stack,
                                task->stack_size / sizeof(uint32_t),
                                (uintptr_t)task,
                                (uintptr_t)prvPortTaskTrampoline,
                                (uintptr_t)vPortTaskExitError);
    configASSERT(task->port_context.saved_psp != NULL);
}

void vPortRunTask(struct tskTaskControlBlock *task)
{
    /* Cortex-M enters a task through SVC and switches through PendSV. */
    (void)task;
}

void vPortYieldTask(struct tskTaskControlBlock *task)
{
    (void)task;
    MINI_SCB_ICSR = (1UL << 28);
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
}

void vPortYieldFromISR(struct tskTaskControlBlock *task)
{
    vPortYieldTask(task);
}

void vPortWaitForTick(void)
{
    __asm volatile("wfi" ::: "memory");
}

BaseType_t xPortStartTick(void)
{
    uint32_t reload;

    if ((configTICK_RATE_HZ == 0U) ||
        (configSYSTICK_CLOCK_HZ < configTICK_RATE_HZ)) {
        return pdFAIL;
    }
    reload = configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ;
    if ((reload == 0U) || ((reload - 1U) > 0x00FFFFFFUL)) {
        return pdFAIL;
    }
    prvConfigureExceptionPriorities();
    MINI_SYSTICK_RVR = reload - 1U;
    MINI_SYSTICK_CVR = 0U;
    MINI_SYSTICK_CSR = MINI_SYSTICK_CLKSOURCE |
                       MINI_SYSTICK_TICKINT | MINI_SYSTICK_ENABLE;
    return pdPASS;
}

void vPortStopTick(void)
{
    MINI_SYSTICK_CSR = 0U;
    MINI_SYSTICK_RVR = 0U;
    MINI_SYSTICK_CVR = 0U;
}

void vPortStartScheduler(void)
{
    /* PendSV and SysTick are both below application interrupts in this
     * teaching port.  The low byte is ignored on cores with fewer priority
     * bits, as required by the ARM exception priority encoding. */
    prvConfigureExceptionPriorities();
    vTaskSwitchContext();
    configASSERT(pxTaskGetCurrent() != NULL);
    /* The first SVC return is the transition into a real task context. */
    vTaskSetContextActive(pdTRUE);
    __asm volatile("svc 0" ::: "memory");
    prvWaitForever();
}

void vPortEndScheduler(void)
{
    vPortStopTick();
    vBoardExit(0);
    prvWaitForever();
}

BaseType_t xPortIsInsideISR(void)
{
    return (prvReadIpsr() != 0U) ? pdTRUE : pdFALSE;
}

void vPortAssertCalled(const char *file, int line)
{
    vBoardAssertFailed(file, line);
    vBoardExit(1);
    prvWaitForever();
}

void vPortTaskExitError(void)
{
    vPortAssertCalled("task-return", 0);
}

void vPortSaveCurrentTaskStack(uint32_t *saved_psp)
{
    TCB_t *task = pxTaskGetCurrent();

    configASSERT(task != NULL);
    configASSERT(saved_psp != NULL);
    task->port_context.saved_psp = saved_psp;
}

uint32_t *pxPortGetCurrentTaskStack(void)
{
    TCB_t *task = pxTaskGetCurrent();

    configASSERT(task != NULL);
    configASSERT(task->port_context.saved_psp != NULL);
    return task->port_context.saved_psp;
}

void vPortEnterCritical(void)
{
    uint32_t primask = prvReadPrimask();

    if (critical_nesting == 0U) {
        critical_saved_primask = primask;
    }
    __asm volatile("cpsid i" ::: "memory");
    ++critical_nesting;
}

void vPortExitCritical(void)
{
    configASSERT(critical_nesting > 0U);
    --critical_nesting;
    if (critical_nesting == 0U) {
        if ((critical_saved_primask & 1U) == 0U) {
            __asm volatile("cpsie i" ::: "memory");
        } else {
            __asm volatile("cpsid i" ::: "memory");
        }
    }
}
