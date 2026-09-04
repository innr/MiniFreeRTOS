#ifndef MINI_PORTMACRO_H
#define MINI_PORTMACRO_H

#include <ucontext.h>

typedef struct {
    ucontext_t native;
} PortContext_t;

struct tskTaskControlBlock;
void vPortInitialiseTask(struct tskTaskControlBlock *task);
void vPortRunTask(struct tskTaskControlBlock *task);
void vPortYieldTask(struct tskTaskControlBlock *task);
void vPortYieldFromISR(struct tskTaskControlBlock *task);
void vPortWaitForTick(void);
BaseType_t xPortStartTick(void);
void vPortStopTick(void);

#endif
