#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static SemaphoreHandle_t inversion_mutex;
static SemaphoreHandle_t multi_mutex_a;
static SemaphoreHandle_t multi_mutex_b;
static SemaphoreHandle_t order_mutex;
static SemaphoreHandle_t timeout_mutex;
static SemaphoreHandle_t abort_mutex;
static TaskHandle_t abort_waiter;

static volatile BaseType_t recursive_result;
static volatile BaseType_t nonowner_give_result;
static volatile BaseType_t low_give_result;
static volatile BaseType_t high_take_result;
static volatile TickType_t high_take_tick;
static volatile UBaseType_t low_inherited_priority;
static volatile UBaseType_t low_restored_priority;
static volatile unsigned long medium_runs;

static volatile BaseType_t multi_high_a_result;
static volatile BaseType_t multi_high_b_result;
static volatile BaseType_t multi_give_a_result;
static volatile BaseType_t multi_give_b_result;
static volatile UBaseType_t multi_before_release;
static volatile UBaseType_t multi_after_a;
static volatile UBaseType_t multi_after_b;

static volatile BaseType_t order_high_result;
static volatile BaseType_t order_low_result;
static volatile BaseType_t order_first;
static volatile BaseType_t order_second;

static volatile BaseType_t timeout_take_result;
static volatile BaseType_t timeout_owner_give_result;
static volatile UBaseType_t timeout_owner_priority;
static volatile TickType_t timeout_start;
static volatile TickType_t timeout_end;

static volatile BaseType_t abort_take_result;
static volatile BaseType_t abort_action_result;
static volatile BaseType_t abort_owner_give_result;
static volatile UBaseType_t abort_owner_priority;

static void vInversionLowTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(inversion_mutex, 0U) == pdPASS);
    recursive_result = xSemaphoreTake(inversion_mutex, 0U);
    vTaskDelay(4U);
    low_inherited_priority = uxTaskPriorityGet(NULL);
    low_give_result = xSemaphoreGive(inversion_mutex);
    low_restored_priority = uxTaskPriorityGet(NULL);
}

static void vInversionHighTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    nonowner_give_result = xSemaphoreGive(inversion_mutex);
    high_take_result = xSemaphoreTake(inversion_mutex, 10U);
    high_take_tick = xTaskGetTickCount();
    if (high_take_result == pdPASS) {
        assert(xSemaphoreGive(inversion_mutex) == pdPASS);
    }
}

static void vInversionMediumTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    while ((high_take_result == pdFALSE) &&
           (xTaskGetTickCount() < 7U)) {
        ++medium_runs;
    }
}

static void vMultiOwnerTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(multi_mutex_a, 0U) == pdPASS);
    assert(xSemaphoreTake(multi_mutex_b, 0U) == pdPASS);
    vTaskDelay(4U);
    multi_before_release = uxTaskPriorityGet(NULL);
    multi_give_a_result = xSemaphoreGive(multi_mutex_a);
    multi_after_a = uxTaskPriorityGet(NULL);
    multi_give_b_result = xSemaphoreGive(multi_mutex_b);
    multi_after_b = uxTaskPriorityGet(NULL);
}

static void vMultiHighATask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    multi_high_a_result = xSemaphoreTake(multi_mutex_a, 10U);
    if (multi_high_a_result == pdPASS) {
        assert(xSemaphoreGive(multi_mutex_a) == pdPASS);
    }
}

static void vMultiHighBTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    multi_high_b_result = xSemaphoreTake(multi_mutex_b, 10U);
    if (multi_high_b_result == pdPASS) {
        assert(xSemaphoreGive(multi_mutex_b) == pdPASS);
    }
}

static void vOrderOwnerTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(order_mutex, 0U) == pdPASS);
    vTaskDelay(4U);
    assert(xSemaphoreGive(order_mutex) == pdPASS);
}

static void vOrderHighTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    order_high_result = xSemaphoreTake(order_mutex, 10U);
    if (order_high_result == pdPASS) {
        order_first = 3;
        assert(xSemaphoreGive(order_mutex) == pdPASS);
    }
}

static void vOrderLowTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    order_low_result = xSemaphoreTake(order_mutex, 10U);
    if (order_low_result == pdPASS) {
        order_second = 2;
        assert(xSemaphoreGive(order_mutex) == pdPASS);
    }
}

static void vTimeoutOwnerTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(timeout_mutex, 0U) == pdPASS);
    vTaskDelay(5U);
    timeout_owner_priority = uxTaskPriorityGet(NULL);
    timeout_owner_give_result = xSemaphoreGive(timeout_mutex);
}

static void vTimeoutWaiterTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    timeout_start = xTaskGetTickCount();
    timeout_take_result = xSemaphoreTake(timeout_mutex, 2U);
    timeout_end = xTaskGetTickCount();
}

static void vAbortOwnerTask(void *parameters)
{
    (void)parameters;
    assert(xSemaphoreTake(abort_mutex, 0U) == pdPASS);
    vTaskDelay(6U);
    abort_owner_priority = uxTaskPriorityGet(NULL);
    abort_owner_give_result = xSemaphoreGive(abort_mutex);
}

static void vAbortWaiterTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(1U);
    abort_take_result = xSemaphoreTake(abort_mutex, 10U);
}

static void vAborterTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    abort_action_result = xTaskAbortDelay(abort_waiter);
}

int main(void)
{
    inversion_mutex = xSemaphoreCreateMutex();
    multi_mutex_a = xSemaphoreCreateMutex();
    multi_mutex_b = xSemaphoreCreateMutex();
    order_mutex = xSemaphoreCreateMutex();
    timeout_mutex = xSemaphoreCreateMutex();
    abort_mutex = xSemaphoreCreateMutex();
    assert(inversion_mutex != NULL);
    assert(multi_mutex_a != NULL);
    assert(multi_mutex_b != NULL);
    assert(order_mutex != NULL);
    assert(timeout_mutex != NULL);
    assert(abort_mutex != NULL);

    assert(xTaskCreate(vInversionHighTask, "inv-high", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vMultiHighATask, "multi-high-a", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vOrderHighTask, "order-high", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vAborterTask, "aborter", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vInversionMediumTask, "inv-medium", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vMultiHighBTask, "multi-high-b", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vOrderLowTask, "order-low", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vTimeoutWaiterTask, "timeout-waiter", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vAbortWaiterTask, "abort-waiter", configMINIMAL_STACK_SIZE,
                       NULL, 2U, &abort_waiter) == pdPASS);
    assert(xTaskCreate(vInversionLowTask, "inv-low", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vMultiOwnerTask, "multi-owner", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vOrderOwnerTask, "order-owner", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vTimeoutOwnerTask, "timeout-owner", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vAbortOwnerTask, "abort-owner", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert(recursive_result == pdFAIL);
    assert(nonowner_give_result == pdFAIL);
    assert(low_give_result == pdPASS);
    assert(low_inherited_priority == 3U);
    assert(low_restored_priority == 1U);
    assert(high_take_result == pdPASS);
    assert(high_take_tick <= 5U);
    assert(medium_runs > 0UL);

    assert(multi_before_release == 3U);
    assert(multi_after_a == 2U);
    assert(multi_after_b == 1U);
    assert(multi_give_a_result == pdPASS);
    assert(multi_give_b_result == pdPASS);
    assert(multi_high_a_result == pdPASS);
    assert(multi_high_b_result == pdPASS);

    assert(order_high_result == pdPASS);
    assert(order_low_result == pdPASS);
    assert(order_first == 3);
    assert(order_second == 2);

    assert(timeout_take_result == pdFAIL);
    assert(timeout_owner_priority == 1U);
    assert(timeout_owner_give_result == pdPASS);
    assert((TickType_t)(timeout_end - timeout_start) >= 2U);

    assert(abort_action_result == pdPASS);
    assert(abort_take_result == pdFAIL);
    assert(abort_owner_priority == 1U);
    assert(abort_owner_give_result == pdPASS);
    puts("P5 mutex/priority-inheritance tests passed");
    return 0;
}
