#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static QueueHandle_t empty_queue;
static QueueHandle_t full_queue;
static volatile BaseType_t receiver_result;
static volatile BaseType_t sender_result;
static volatile BaseType_t timeout_result;
static volatile int receiver_value;
static volatile int first_value;
static volatile int second_value;
static volatile TickType_t sender_wait_start;
static volatile TickType_t sender_wait_end;
static volatile TickType_t timeout_start;
static volatile TickType_t timeout_end;

static void vReceiverTask(void *parameters)
{
    int value = 0;
    (void)parameters;
    receiver_result = xQueueReceive(empty_queue, &value, 10U);
    if (receiver_result == pdPASS) {
        receiver_value = value;
    }
}

static void vSenderTask(void *parameters)
{
    int first = 11;
    int second = 22;
    (void)parameters;

    assert(xQueueSend(full_queue, &first, 0U) == pdPASS);
    first = -1;
    sender_wait_start = xTaskGetTickCount();
    sender_result = xQueueSend(full_queue, &second, 10U);
    sender_wait_end = xTaskGetTickCount();
}

static void vCoordinatorTask(void *parameters)
{
    int source = 99;
    int value = 0;
    (void)parameters;

    vTaskDelay(2U);
    assert(xQueueSend(empty_queue, &source, 0U) == pdPASS);
    assert(xQueueReceive(full_queue, &value, 0U) == pdPASS);
    first_value = value;
    assert(xQueueReceive(full_queue, &value, 0U) == pdPASS);
    second_value = value;

    timeout_start = xTaskGetTickCount();
    timeout_result = xQueueReceive(empty_queue, &value, 2U);
    timeout_end = xTaskGetTickCount();
}

int main(void)
{
    QueueHandle_t setup_queue = xQueueCreate(1U, sizeof(int));
    int setup_value = 7;
    int setup_copy = 0;

    empty_queue = xQueueCreate(1U, sizeof(int));
    full_queue = xQueueCreate(1U, sizeof(int));
    assert(setup_queue != NULL);
    assert(empty_queue != NULL);
    assert(full_queue != NULL);
    assert(xQueueSend(setup_queue, &setup_value, 0U) == pdPASS);
    assert(xQueueReceive(setup_queue, &setup_copy, 0U) == pdPASS);
    assert(setup_copy == setup_value);

    assert(xTaskCreate(vReceiverTask, "receiver", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vSenderTask, "sender", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vCoordinatorTask, "coordinator", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert(receiver_result == pdPASS);
    assert(receiver_value == 99);
    assert(sender_result == pdPASS);
    assert((TickType_t)(sender_wait_end - sender_wait_start) >= 1U);
    assert(first_value == 11);
    assert(second_value == 22);
    assert(timeout_result == pdFAIL);
    assert((TickType_t)(timeout_end - timeout_start) >= 2U);
    assert(uxQueueMessagesWaiting(empty_queue) == 0U);
    assert(uxQueueMessagesWaiting(full_queue) == 0U);
    puts("P4 queue/blocking/timeout tests passed");
    return 0;
}
