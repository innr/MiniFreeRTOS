#ifndef MINI_CORTEX_M_PORTMACRO_H
#define MINI_CORTEX_M_PORTMACRO_H

#include "FreeRTOS.h"
#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define MINI_PORT_NORETURN __attribute__((noreturn))
#else
#define MINI_PORT_NORETURN
#endif

typedef struct {
    uint32_t *saved_psp;
} PortContext_t;

#define MINI_CORTEX_M_STACK_FRAME_WORDS 16U
#define MINI_CORTEX_M_INITIAL_XPSR      0x01000000UL

struct tskTaskControlBlock;

void vPortInitialiseTask(struct tskTaskControlBlock *task);
void vPortRunTask(struct tskTaskControlBlock *task);
void vPortYieldTask(struct tskTaskControlBlock *task);
void vPortYieldFromISR(struct tskTaskControlBlock *task);
void vPortWaitForTick(void);
BaseType_t xPortStartTick(void);
void vPortStopTick(void);
void vPortStartScheduler(void) MINI_PORT_NORETURN;
void vPortEndScheduler(void) MINI_PORT_NORETURN;
BaseType_t xPortIsInsideISR(void);
void vPortAssertCalled(const char *file, int line) MINI_PORT_NORETURN;
void vPortTaskExitError(void) MINI_PORT_NORETURN;

void vPortSaveCurrentTaskStack(uint32_t *saved_psp);
uint32_t *pxPortGetCurrentTaskStack(void);

/* Architecture-neutral helper kept separate so the stack ABI can be tested
 * by the host build without assembling ARM instructions. */
void vPortBuildInitialStackFrame(PortContext_t *context,
                                 uint32_t *stack,
                                 size_t stack_words,
                                 uintptr_t task_argument,
                                 uintptr_t task_entry,
                                 uintptr_t task_exit);

#endif
