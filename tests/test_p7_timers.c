#include "FreeRTOS.h"
#include "task.h"
#include "tasks_internal.h"
#include "timers.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static TimerHandle_t saturation_timer;
static TimerHandle_t one_shot_timer;
static TimerHandle_t periodic_timer;
static TimerHandle_t change_timer;
static TimerHandle_t reset_timer;
static TimerHandle_t stop_timer;
static TimerHandle_t order_a_timer;
static TimerHandle_t order_b_timer;
static TimerHandle_t wrap_timer;

static volatile unsigned one_shot_count;
static volatile TickType_t one_shot_tick;
static volatile unsigned periodic_count;
static volatile BaseType_t periodic_active_after_stop;
static volatile unsigned change_count;
static volatile TickType_t change_tick;
static volatile TickType_t change_command_tick;
static volatile unsigned reset_count;
static volatile TickType_t reset_tick;
static volatile TickType_t reset_command_tick;
static volatile unsigned stop_count;
static volatile unsigned wrap_count;
static volatile TickType_t wrap_tick;
static volatile unsigned order_length;
static volatile unsigned order_log[2];
static volatile BaseType_t controller_done;
static int one_shot_id;
static int changed_id;

static void vOneShotCallback(TimerHandle_t timer)
{
    ++one_shot_count;
    one_shot_tick = xTaskGetTickCount();
    assert(pvTimerGetTimerID(timer) == &one_shot_id);
}

static void vPeriodicCallback(TimerHandle_t timer)
{
    ++periodic_count;
    if (periodic_count >= 3U) {
        assert(xTimerStop(timer, 0U) == pdPASS);
    }
}

static void vChangeCallback(TimerHandle_t timer)
{
    ++change_count;
    change_tick = xTaskGetTickCount();
    assert(pvTimerGetTimerID(timer) == &changed_id);
}

static void vResetCallback(TimerHandle_t timer)
{
    (void)timer;
    ++reset_count;
    reset_tick = xTaskGetTickCount();
}

static void vStopCallback(TimerHandle_t timer)
{
    (void)timer;
    ++stop_count;
}

static void vOrderACallback(TimerHandle_t timer)
{
    (void)timer;
    if (order_length < (sizeof(order_log) / sizeof(order_log[0]))) {
        order_log[order_length++] = 1U;
    }
}

static void vOrderBCallback(TimerHandle_t timer)
{
    (void)timer;
    if (order_length < (sizeof(order_log) / sizeof(order_log[0]))) {
        order_log[order_length++] = 2U;
    }
}

static void vWrapCallback(TimerHandle_t timer)
{
    (void)timer;
    ++wrap_count;
    wrap_tick = xTaskGetTickCount();
}

static void vTimerControllerTask(void *parameters)
{
    TickType_t start_tick;

    (void)parameters;
    start_tick = xTaskGetTickCount();
    assert(xTimerStart(one_shot_timer, 2U) == pdPASS);
    assert(xTimerStart(periodic_timer, 2U) == pdPASS);
    assert(xTimerStart(change_timer, 2U) == pdPASS);
    assert(xTimerStart(reset_timer, 2U) == pdPASS);
    assert(xTimerStart(stop_timer, 2U) == pdPASS);
    assert(xTimerStart(order_a_timer, 2U) == pdPASS);
    assert(xTimerStart(order_b_timer, 2U) == pdPASS);
    assert(xTimerStart(wrap_timer, 2U) == pdPASS);
    assert(xTimerStop(saturation_timer, 2U) == pdPASS);

    vTaskDelay(1U);
    change_command_tick = xTaskGetTickCount();
    assert(xTimerChangePeriod(change_timer, 2U, 2U) == pdPASS);
    reset_command_tick = xTaskGetTickCount();
    assert(xTimerReset(reset_timer, 2U) == pdPASS);
    vTaskDelay(1U);
    assert(xTimerStop(stop_timer, 2U) == pdPASS);

    while ((TickType_t)(xTaskGetTickCount() - start_tick) < 12U) {
        vTaskDelay(1U);
    }
    periodic_active_after_stop = xTimerIsTimerActive(periodic_timer);
    controller_done = pdTRUE;
}

int main(void)
{
    unsigned accepted_commands = 0U;

    assert(xTimerCreate(NULL, 1U, pdFALSE, NULL, vOneShotCallback) == NULL);
    assert(xTimerCreate("zero", 0U, pdFALSE, NULL, vOneShotCallback) == NULL);
    assert(xTimerCreate("long", (TickType_t)INT32_MAX,
                        pdFALSE, NULL, vOneShotCallback) == NULL);
    assert(xTimerCreate("mode", 1U, (BaseType_t)2,
                        NULL, vOneShotCallback) == NULL);

    vTaskSetTickCountForTest(UINT32_MAX - 2U);
    saturation_timer = xTimerCreate("saturation", (TickType_t)(INT32_MAX - 1),
                                    pdFALSE, NULL, vOneShotCallback);
    assert(saturation_timer != NULL);
    while (accepted_commands < configTIMER_QUEUE_LENGTH) {
        assert(xTimerStart(saturation_timer, 0U) == pdPASS);
        ++accepted_commands;
    }
    assert(xTimerStart(saturation_timer, 0U) == pdFAIL);
    assert(xTimerIsTimerActive(saturation_timer) == pdFALSE);

    one_shot_timer = xTimerCreate("one-shot", 2U, pdFALSE,
                                  &one_shot_id, vOneShotCallback);
    periodic_timer = xTimerCreate("periodic", 1U, pdTRUE,
                                  NULL, vPeriodicCallback);
    change_timer = xTimerCreate("change", 5U, pdFALSE,
                                &changed_id, vChangeCallback);
    reset_timer = xTimerCreate("reset", 4U, pdFALSE, NULL, vResetCallback);
    stop_timer = xTimerCreate("stop", 4U, pdFALSE, NULL, vStopCallback);
    order_a_timer = xTimerCreate("order-a", 2U, pdFALSE, NULL, vOrderACallback);
    order_b_timer = xTimerCreate("order-b", 1U, pdFALSE, NULL, vOrderBCallback);
    wrap_timer = xTimerCreate("wrap", 4U, pdFALSE, NULL, vWrapCallback);
    assert(one_shot_timer != NULL);
    assert(periodic_timer != NULL);
    assert(change_timer != NULL);
    assert(reset_timer != NULL);
    assert(stop_timer != NULL);
    assert(order_a_timer != NULL);
    assert(order_b_timer != NULL);
    assert(wrap_timer != NULL);
    vTimerSetTimerID(change_timer, &changed_id);
    assert(pvTimerGetTimerID(change_timer) == &changed_id);

    assert(xTaskCreate(vTimerControllerTask, "timer-test",
                       configMINIMAL_STACK_SIZE, NULL, 1U, NULL) == pdPASS);
    assert(xTimerIsTimerActive(one_shot_timer) == pdFALSE);
    vTaskStartScheduler();

    assert(controller_done == pdTRUE);
    assert(one_shot_count == 1U);
    assert((TickType_t)(one_shot_tick - (UINT32_MAX - 2U)) >= 2U);
    assert(periodic_count == 3U);
    assert(periodic_active_after_stop == pdFALSE);
    assert(change_count == 1U);
    assert((TickType_t)(change_tick - change_command_tick) >= 2U);
    assert(reset_count == 1U);
    assert((TickType_t)(reset_tick - reset_command_tick) >= 4U);
    assert(stop_count == 0U);
    assert(order_length == 2U);
    assert(order_log[0] == 2U);
    assert(order_log[1] == 1U);
    assert(wrap_count == 1U);
    assert(wrap_tick < 4U);
    assert(xTimerIsTimerActive(one_shot_timer) == pdFALSE);
    assert(xTimerIsTimerActive(change_timer) == pdFALSE);
    assert(xTimerIsTimerActive(reset_timer) == pdFALSE);
    assert(xTimerIsTimerActive(stop_timer) == pdFALSE);
    assert(xTimerIsTimerActive(order_a_timer) == pdFALSE);
    assert(xTimerIsTimerActive(order_b_timer) == pdFALSE);
    assert(xTimerIsTimerActive(wrap_timer) == pdFALSE);
    assert(xTimerIsTimerActive(saturation_timer) == pdFALSE);
    puts("P7 software timer tests passed");
    return 0;
}
