#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static int trace[16];
static int trace_length;
static TaskHandle_t first_handle;

static void vPriorityTask(void *parameters)
{
    int identity = *(int *)parameters;
    for (int round = 0; round < 3; ++round) {
        trace[trace_length++] = identity * 10 + round + 1;
        if (identity == 1 && round == 0) {
            vTaskPrioritySet(NULL, 3U);
            assert(uxTaskPriorityGet(NULL) == 3U);
        }
        taskYIELD();
    }
}

static void vLowPriorityTask(void *parameters)
{
    (void)parameters;
    trace[trace_length++] = 31;
    taskYIELD();
    trace[trace_length++] = 32;
}

int main(void)
{
    int first = 1;
    int second = 2;

    assert(xTaskCreate(vPriorityTask, "high-a", configMINIMAL_STACK_SIZE,
                       &first, 3U, &first_handle) == pdPASS);
    assert(xTaskCreate(vPriorityTask, "high-b", configMINIMAL_STACK_SIZE,
                       &second, 3U, NULL) == pdPASS);
    assert(xTaskCreate(vLowPriorityTask, "low", configMINIMAL_STACK_SIZE,
                       NULL, 1U, NULL) == pdPASS);
    assert(uxTaskPriorityGet(first_handle) == 3U);

    vTaskStartScheduler();

    assert(trace_length == 8);
    assert(trace[0] == 11);
    assert(trace[1] == 21);
    assert(trace[2] == 12);
    assert(trace[3] == 22);
    assert(trace[4] == 13);
    assert(trace[5] == 23);
    assert(trace[6] == 31);
    assert(trace[7] == 32);
    assert(eTaskGetState(first_handle) == eDeleted);
    puts("P1 priority/round-robin/idle tests passed");
    return 0;
}

