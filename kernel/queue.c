#include "tasks_internal.h"
#include "queue.h"
#include "semphr.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    eQueueData,
    eBinarySemaphore,
    eCountingSemaphore
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
};

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
    Queue_t *semaphore = calloc(1U, sizeof(*semaphore));
    if (semaphore == NULL) {
        return NULL;
    }
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
    queue = calloc(1U, sizeof(*queue));
    if (queue == NULL) {
        return NULL;
    }
    queue->storage_size = (size_t)queue_length * (size_t)item_size;
    queue->storage = malloc(queue->storage_size);
    if (queue->storage == NULL) {
        free(queue);
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
            (void)memcpy(slot, item, queue->item_size);
            queue->head = (queue->head + 1U) % queue->length;
            ++queue->count;
            should_yield = xTaskUnblockOne(&queue->receive_waiters);
            taskEXIT_CRITICAL();
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
            (void)memcpy(buffer, slot, queue->item_size);
            queue->tail = (queue->tail + 1U) % queue->length;
            --queue->count;
            should_yield = xTaskUnblockOne(&queue->send_waiters);
            taskEXIT_CRITICAL();
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

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore_handle,
                          TickType_t ticks_to_wait)
{
    Queue_t *semaphore = semaphore_handle;
    TCB_t *current = NULL;

    configASSERT(semaphore != NULL);
    if ((semaphore->kind != eBinarySemaphore) &&
        (semaphore->kind != eCountingSemaphore)) {
        return pdFAIL;
    }
    prvAssertTimeoutIsValid(ticks_to_wait);
    for (;;) {
        taskENTER_CRITICAL();
        if (semaphore->count > 0U) {
            --semaphore->count;
            taskEXIT_CRITICAL();
            return pdPASS;
        }
        if (ticks_to_wait == 0U) {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }
        current = prvCurrentTask();
        vTaskBlockCurrent(&semaphore->receive_waiters, semaphore,
                          eTaskWaitSemaphoreTake, ticks_to_wait);
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
    BaseType_t should_yield;

    configASSERT(semaphore != NULL);
    if ((semaphore->kind != eBinarySemaphore) &&
        (semaphore->kind != eCountingSemaphore)) {
        return pdFAIL;
    }
    taskENTER_CRITICAL();
    if (semaphore->count >= semaphore->max_count) {
        taskEXIT_CRITICAL();
        return pdFAIL;
    }
    ++semaphore->count;
    should_yield = xTaskUnblockOne(&semaphore->receive_waiters);
    taskEXIT_CRITICAL();
    if (should_yield == pdTRUE) {
        vTaskYield();
    }
    return pdPASS;
}
