#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdio.h>

static TimerHandle_t heartbeat_timer;
static volatile unsigned callback_count;
static const char heartbeat_id[] = "heartbeat";

static void vHeartbeatCallback(TimerHandle_t timer)
{
    ++callback_count;
    printf("timer: %s tick=%u callback=%u\n",
           (const char *)pvTimerGetTimerID(timer),
           (unsigned)xTaskGetTickCount(),
           callback_count);
    if (callback_count >= 3U) {
        configASSERT(xTimerStop(timer, 0U) == pdPASS);
    }
}

static void vTimerDemoTask(void *parameters)
{
    TickType_t start_tick;

    (void)parameters;
    start_tick = xTaskGetTickCount();
    while ((TickType_t)(xTaskGetTickCount() - start_tick) < 8U) {
        vTaskDelay(1U);
    }
}

int main(void)
{
    heartbeat_timer = xTimerCreate("heartbeat", 2U, pdTRUE,
                                   (void *)heartbeat_id,
                                   vHeartbeatCallback);
    configASSERT(heartbeat_timer != NULL);
    configASSERT(xTimerStart(heartbeat_timer, 0U) == pdPASS);
    configASSERT(xTaskCreate(vTimerDemoTask, "timer-demo",
                             configMINIMAL_STACK_SIZE, NULL, 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    printf("timer: callbacks=%u active=%d\n",
           callback_count,
           (int)xTimerIsTimerActive(heartbeat_timer));
    return 0;
}
