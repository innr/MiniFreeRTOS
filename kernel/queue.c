#include "tasks_internal.h"
#include "queue.h"
#include "semphr.h"
#include "trace_internal.h"
#include "portable.h"
#include <string.h>

typedef enum {
    eQueueData,
    eBinarySemaphore,
    eCountingSemaphore,
    eMutex
} QueueKind_t;

typedef struct mini_queue Queue_t;

struct mini_queue {
    QueueKind_t kind;
    uint8_t *storage;
    size_t storage_size;
    UBaseType_t length;
    UBaseType_t item_size;
    UBaseType_t head;
    UBaseType_t tail;
    UBaseType_t count;
    UBaseType_t max_count;
    TaskEventList_t send_waiters;
    TaskEventList_t receive_waiters;
    TCB_t *owner;
    Queue_t *registry_next;
};

static Queue_t *mutex_registry;

static void prvRecomputeInheritedPriority(TCB_t *task)
{
    UBaseType_t inherited_priority = tskIDLE_PRIORITY;
    UBaseType_t previous_priority;
    Queue_t *source_mutex = NULL;
    TCB_t *source_waiter = NULL;

    if (task == NULL) {
        return;
    }
    previous_priority = task->priority;
    for (Queue_t *mutex = mutex_registry; mutex != NULL;
         mutex = mutex->registry_next) {
        if ((mutex->owner == task) && (mutex->receive_waiters.head != NULL) &&
            (mutex->receive_waiters.head->priority > inherited_priority)) {
            inherited_priority = mutex->receive_waiters.head->priority;
            source_mutex = mutex;
            source_waiter = mutex->receive_waiters.head;
        }
    }
    vTaskSetInheritedPriority(task, inherited_priority);
    if (task->priority > previous_priority) {
        TRACE_RECORD(eTraceMutexInherit, task, source_waiter, source_mutex,
                     task->priority);
    }
}

BaseType_t xTaskOwnsMutex(TCB_t *task)
{
    if (task == NULL) {
        return pdFALSE;
    }
    for (Queue_t *mutex = mutex_registry; mutex != NULL;
         mutex = mutex->registry_next) {
        if (mutex->owner == task) {
            return pdTRUE;
        }
    }
    return pdFALSE;
}

void vTaskWaitEnded(TCB_t *task)
{
    Queue_t *mutex;

    if ((task == NULL) || (task->wait_reason != eTaskWaitMutexTake)) {
        return;
    }
    mutex = task->wait_object;
    if ((mutex != NULL) && (mutex->kind == eMutex) &&
        (mutex->owner != NULL)) {
        prvRecomputeInheritedPriority(mutex->owner);
    }
}

static void prvAssertTimeoutIsValid(TickType_t ticks_to_wait)
{
    configASSERT(ticks_to_wait < (TickType_t)INT32_MAX);
}

static TCB_t *prvCurrentTask(void)
{
    TCB_t *task = xTaskGetCurrentTaskHandle();
    configASSERT(task != NULL);
    configASSERT(task->state == eRunning);
    return task;
}

static Queue_t *prvAllocateSemaphore(QueueKind_t kind,
                                     UBaseType_t max_count,
                                     UBaseType_t initial_count)
{
    Queue_t *semaphore = pvPortMalloc(sizeof(*semaphore));
    if (semaphore == NULL) {
        return NULL;
    }
    (void)memset(semaphore, 0, sizeof(*semaphore));
    semaphore->kind = kind;
    semaphore->count = initial_count;
    semaphore->max_count = max_count;
    vTaskEventListInit(&semaphore->receive_waiters);
    vTaskEventListInit(&semaphore->send_waiters);
    return semaphore;
}

QueueHandle_t xQueueCreate(UBaseType_t queue_length,
                           UBaseType_t item_size)
{
    Queue_t *queue;

    if ((queue_length == 0U) || (item_size == 0U) ||
        ((size_t)item_size > (SIZE_MAX / (size_t)queue_length))) {
        return NULL;
    }
    queue = pvPortMalloc(sizeof(*queue));
    if (queue == NULL) {
        return NULL;
    }
    (void)memset(queue, 0, sizeof(*queue));
    queue->storage_size = (size_t)queue_length * (size_t)item_size;
    queue->storage = pvPortMalloc(queue->storage_size);
    if (queue->storage == NULL) {
        vPortFree(queue);
        return NULL;
    }
    queue->kind = eQueueData;
    queue->length = queue_length;
    queue->item_size = item_size;
    queue->max_count = queue_length;
    vTaskEventListInit(&queue->send_waiters);
    vTaskEventListInit(&queue->receive_waiters);
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue_handle,
                      const void *item,
                      TickType_t ticks_to_wait)
{
    Queue_t *queue = queue_handle;
    TCB_t *current = NULL;

    configASSERT(queue != NULL);
    configASSERT(item != NULL);
    if (queue->kind != eQueueData) {
        return pdFAIL;
    }
    prvAssertTimeoutIsValid(ticks_to_wait);
    for (;;) {
        BaseType_t should_yield;
        taskENTER_CRITICAL();
        if (queue->count < queue->length) {
            uint8_t *slot = queue->storage +
                            ((size_t)queue->head * (size_t)queue->item_size);
            UBaseType_t messages_waiting;
            (void)memcpy(slot, item, queue->item_size);
            queue->head = (queue->head + 1U) % queue->length;
            ++queue->count;
            messages_waiting = queue->count;
            should_yield = xTaskUnblockOne(&queue->receive_waiters);
            taskEXIT_CRITICAL();
            TRACE_RECORD(eTraceQueueSend, xTaskGetCurrentTaskHandle(), NULL,
                         queue, messages_waiting);
            if (should_yield == pdTRUE) {
                vTaskYield();
            }
            return pdPASS;
        }
        if (ticks_to_wait == 0U) {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }
        current = prvCurrentTask();
        vTaskBlockCurrent(&queue->send_waiters, queue,
                          eTaskWaitQueueSend, ticks_to_wait);
        taskEXIT_CRITICAL();
        vPortYieldTask(current);
        if (current->wait_result == pdFALSE) {
            vTaskClearWaitState(current);
            return pdFAIL;
        }
        ticks_to_wait = xTaskGetWaitRemaining(current);
        vTaskClearWaitState(current);
    }
}

BaseType_t xQueueReceive(QueueHandle_t queue_handle,
                         void *buffer,
                         TickType_t ticks_to_wait)
{
    Queue_t *queue = queue_handle;
    TCB_t *current = NULL;

    configASSERT(queue != NULL);
    configASSERT(buffer != NULL);
    if (queue->kind != eQueueData) {
        return pdFAIL;
    }
    prvAssertTimeoutIsValid(ticks_to_wait);
    for (;;) {
        BaseType_t should_yield;
        taskENTER_CRITICAL();
        if (queue->count > 0U) {
            uint8_t *slot = queue->storage +
                            ((size_t)queue->tail * (size_t)queue->item_size);
            UBaseType_t messages_waiting;
            (void)memcpy(buffer, slot, queue->item_size);
            queue->tail = (queue->tail + 1U) % queue->length;
            --queue->count;
            messages_waiting = queue->count;
            should_yield = xTaskUnblockOne(&queue->send_waiters);
            taskEXIT_CRITICAL();
            TRACE_RECORD(eTraceQueueReceive, xTaskGetCurrentTaskHandle(), NULL,
                         queue, messages_waiting);
            if (should_yield == pdTRUE) {
                vTaskYield();
            }
            return pdPASS;
        }
        if (ticks_to_wait == 0U) {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }
        current = prvCurrentTask();
        vTaskBlockCurrent(&queue->receive_waiters, queue,
                          eTaskWaitQueueReceive, ticks_to_wait);
        taskEXIT_CRITICAL();
        vPortYieldTask(current);
        if (current->wait_result == pdFALSE) {
            vTaskClearWaitState(current);
            return pdFAIL;
        }
        ticks_to_wait = xTaskGetWaitRemaining(current);
        vTaskClearWaitState(current);
    }
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue_handle)
{
    Queue_t *queue = queue_handle;

    configASSERT(queue != NULL);
    if (queue->kind != eQueueData) {
        return 0U;
    }
    return queue->count;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    return prvAllocateSemaphore(eBinarySemaphore, 1U, 0U);
}

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max_count,
                                            UBaseType_t initial_count)
{
    if ((max_count == 0U) || (initial_count > max_count)) {
        return NULL;
    }
    return prvAllocateSemaphore(eCountingSemaphore, max_count, initial_count);
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    Queue_t *mutex = prvAllocateSemaphore(eMutex, 1U, 1U);

    if (mutex == NULL) {
        return NULL;
    }
    mutex->registry_next = mutex_registry;
    mutex_registry = mutex;
    return mutex;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore_handle,
                          TickType_t ticks_to_wait)
{
    Queue_t *semaphore = semaphore_handle;
    TCB_t *current = NULL;
    TCB_t *owner;

    configASSERT(semaphore != NULL);
    if ((semaphore->kind != eBinarySemaphore) &&
        (semaphore->kind != eCountingSemaphore) &&
        (semaphore->kind != eMutex)) {
        return pdFAIL;
    }
    prvAssertTimeoutIsValid(ticks_to_wait);
    if (semaphore->kind == eMutex) {
        current = prvCurrentTask();
    }
    for (;;) {
        taskENTER_CRITICAL();
        if (semaphore->kind == eMutex) {
            configASSERT(((semaphore->count == 0U) &&
                          (semaphore->owner != NULL)) ||
                         ((semaphore->count == 1U) &&
                          (semaphore->owner == NULL)));
            if (semaphore->owner == current) {
                taskEXIT_CRITICAL();
                return pdFAIL;
            }
        }
        if (semaphore->count > 0U) {
            if (semaphore->kind == eMutex) {
                semaphore->owner = current;
            }
            --semaphore->count;
            UBaseType_t count_after = semaphore->count;
            taskEXIT_CRITICAL();
            TRACE_RECORD(eTraceSemaphoreTake,
                         (current != NULL) ? current : xTaskGetCurrentTaskHandle(),
                         NULL, semaphore, count_after);
            return pdPASS;
        }
        if (ticks_to_wait == 0U) {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }
        if (current == NULL) {
            current = prvCurrentTask();
        }
        owner = semaphore->owner;
        vTaskBlockCurrent(&semaphore->receive_waiters, semaphore,
                          (semaphore->kind == eMutex) ?
                              eTaskWaitMutexTake : eTaskWaitSemaphoreTake,
                          ticks_to_wait);
        if ((semaphore->kind == eMutex) && (owner != NULL)) {
            prvRecomputeInheritedPriority(owner);
        }
        taskEXIT_CRITICAL();
        vPortYieldTask(current);
        if (current->wait_result == pdFALSE) {
            vTaskClearWaitState(current);
            return pdFAIL;
        }
        ticks_to_wait = xTaskGetWaitRemaining(current);
        vTaskClearWaitState(current);
    }
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore_handle)
{
    Queue_t *semaphore = semaphore_handle;
    TCB_t *current = NULL;
    TCB_t *waiter;
    BaseType_t should_yield = pdFALSE;

    configASSERT(semaphore != NULL);
    if ((semaphore->kind != eBinarySemaphore) &&
        (semaphore->kind != eCountingSemaphore) &&
        (semaphore->kind != eMutex)) {
        return pdFAIL;
    }
    if (semaphore->kind == eMutex) {
        current = xTaskGetCurrentTaskHandle();
        if ((current == NULL) || (current->state != eRunning)) {
            return pdFAIL;
        }
    }
    taskENTER_CRITICAL();
    if (semaphore->kind == eMutex) {
        configASSERT(((semaphore->count == 0U) &&
                      (semaphore->owner != NULL)) ||
                     ((semaphore->count == 1U) &&
                      (semaphore->owner == NULL)));
        if (semaphore->owner != current) {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }
        waiter = semaphore->receive_waiters.head;
        semaphore->owner = NULL;
        semaphore->count = 1U;
        if (waiter != NULL) {
            (void)xTaskUnblockOne(&semaphore->receive_waiters);
        }
        prvRecomputeInheritedPriority(current);
#if configUSE_PREEMPTION
        if ((waiter != NULL) && (waiter->priority > current->priority)) {
            should_yield = pdTRUE;
        }
#endif
        taskEXIT_CRITICAL();
        TRACE_RECORD(eTraceMutexRelease, current, waiter, semaphore,
                     current->priority);
        if (should_yield == pdTRUE) {
            vTaskYield();
        }
        return pdPASS;
    }
    if (semaphore->count >= semaphore->max_count) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    ++semaphore->count;
    UBaseType_t count_after = semaphore->count;
    should_yield = xTaskUnblockOne(&semaphore->receive_waiters);
    taskEXIT_CRITICAL();
    TRACE_RECORD(eTraceSemaphoreGive, xTaskGetCurrentTaskHandle(), NULL,
                 semaphore, count_after);
    if (should_yield == pdTRUE) {
        vTaskYield();
    }
    return pdPASS;
}
