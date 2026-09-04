#ifndef MINI_TASKS_INTERNAL_H
#define MINI_TASKS_INTERNAL_H

#include "task.h"
#include "portmacro.h"

typedef struct tskTaskControlBlock TCB_t;

typedef struct {
    TCB_t *head;
    TCB_t *tail;
    UBaseType_t length;
} TaskEventList_t;

typedef enum {
    eTaskWaitNone,
    eTaskWaitQueueSend,
    eTaskWaitQueueReceive,
    eTaskWaitSemaphoreTake
} TaskWaitReason_t;

typedef struct {
    TCB_t *head;
    TCB_t *tail;
    UBaseType_t length;
} ReadyList_t;

struct tskTaskControlBlock {
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
    TCB_t *ready_previous;
    TCB_t *ready_next;
    BaseType_t in_ready_list;
    TickType_t wake_tick;
    TCB_t *delay_previous;
    TCB_t *delay_next;
    BaseType_t in_delay_list;
    TCB_t *event_previous;
    TCB_t *event_next;
    TaskEventList_t *event_list;
    void *wait_object;
    TaskWaitReason_t wait_reason;
    BaseType_t wait_result;
    BaseType_t wait_has_timeout;
};

void vTaskRunEntry(TCB_t *task);
void vTaskTickISR(void);
void vTaskSetTickCountForTest(TickType_t tick);
void vTaskEventListInit(TaskEventList_t *list);
void vTaskBlockCurrent(TaskEventList_t *list,
                       void *wait_object,
                       TaskWaitReason_t wait_reason,
                       TickType_t ticks_to_wait);
BaseType_t xTaskUnblockOne(TaskEventList_t *list);
TickType_t xTaskGetWaitRemaining(TCB_t *task);
void vTaskClearWaitState(TCB_t *task);

#endif
