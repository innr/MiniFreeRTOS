#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static SemaphoreHandle_t binary_semaphore;
static SemaphoreHandle_t counting_semaphore;
static SemaphoreHandle_t full_counting_semaphore;
static SemaphoreHandle_t empty_counting_semaphore;
static volatile BaseType_t binary_immediate_result;
static volatile BaseType_t binary_take_result;
static volatile BaseType_t binary_give_results[3];
static volatile BaseType_t counting_take_results[2];
static volatile BaseType_t counting_give_results[3];
static volatile BaseType_t full_counting_give_result;
static volatile BaseType_t timeout_result;
static volatile TickType_t timeout_start;
static volatile TickType_t timeout_end;

static void vBinaryTakerTask(void *parameters)
{
    (void)parameters;
    binary_immediate_result = xSemaphoreTake(binary_semaphore, 0U);
    binary_take_result = xSemaphoreTake(binary_semaphore, 10U);
}

static void vCountingWorkerTask(void *parameters)
{
    (void)parameters;
    counting_take_results[0] = xSemaphoreTake(counting_semaphore, 0U);
    counting_take_results[1] = xSemaphoreTake(counting_semaphore, 10U);
}

static void vTimeoutTakerTask(void *parameters)
{
    (void)parameters;
    timeout_start = xTaskGetTickCount();
    timeout_result = xSemaphoreTake(empty_counting_semaphore, 2U);
    timeout_end = xTaskGetTickCount();
}

static void vBinaryGiverTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(2U);
    binary_give_results[0] = xSemaphoreGive(binary_semaphore);
    binary_give_results[1] = xSemaphoreGive(binary_semaphore);
    binary_give_results[2] = xSemaphoreGive(binary_semaphore);
}

static void vCountingGiverTask(void *parameters)
{
    (void)parameters;
    vTaskDelay(3U);
    counting_give_results[0] = xSemaphoreGive(counting_semaphore);
    counting_give_results[1] = xSemaphoreGive(counting_semaphore);
    counting_give_results[2] = xSemaphoreGive(counting_semaphore);
    full_counting_give_result = xSemaphoreGive(full_counting_semaphore);
}

int main(void)
{
    SemaphoreHandle_t setup_semaphore = xSemaphoreCreateBinary();

    binary_semaphore = xSemaphoreCreateBinary();
    counting_semaphore = xSemaphoreCreateCounting(2U, 1U);
    full_counting_semaphore = xSemaphoreCreateCounting(2U, 2U);
    empty_counting_semaphore = xSemaphoreCreateCounting(2U, 0U);
    assert(setup_semaphore != NULL);
    assert(binary_semaphore != NULL);
    assert(counting_semaphore != NULL);
    assert(full_counting_semaphore != NULL);
    assert(empty_counting_semaphore != NULL);
    assert(xSemaphoreCreateCounting(0U, 0U) == NULL);
    assert(xSemaphoreCreateCounting(2U, 3U) == NULL);
    assert(xSemaphoreGive(setup_semaphore) == pdPASS);
    assert(xSemaphoreTake(setup_semaphore, 0U) == pdPASS);
    assert(xSemaphoreTake(setup_semaphore, 0U) == pdFAIL);

    assert(xTaskCreate(vBinaryTakerTask, "binary-taker", configMINIMAL_STACK_SIZE,
                       NULL, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vCountingWorkerTask, "counting-worker", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vTimeoutTakerTask, "timeout-taker", configMINIMAL_STACK_SIZE,
                       NULL, 2U, NULL) == pdPASS);
    assert(xTaskCreate(vBinaryGiverTask, "binary-giver", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(xTaskCreate(vCountingGiverTask, "counting-giver", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);

    vTaskStartScheduler();

    assert(binary_immediate_result == pdFAIL);
    assert(binary_take_result == pdPASS);
    assert(binary_give_results[0] == pdPASS);
    assert(binary_give_results[1] == pdPASS);
    assert(binary_give_results[2] == pdFAIL);
    assert(counting_take_results[0] == pdPASS);
    assert(counting_take_results[1] == pdPASS);
    assert(counting_give_results[0] == pdPASS);
    assert(counting_give_results[1] == pdPASS);
    assert(counting_give_results[2] == pdPASS);
    assert(full_counting_give_result == pdFAIL);
    assert(timeout_result == pdFAIL);
    assert((TickType_t)(timeout_end - timeout_start) >= 2U);
    puts("P4 binary/counting semaphore tests passed");
    return 0;
}
