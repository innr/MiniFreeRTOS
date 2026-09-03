#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>

static int trace[8];
static int trace_length;
static TaskHandle_t first_handle;
static TaskHandle_t second_handle;

static void vTraceTask(void *parameters)
{
    int identity = *(int *)parameters;
    int private_counter = 10 * identity;

    trace[trace_length++] = ++private_counter;
    taskYIELD();
    trace[trace_length++] = ++private_counter;
}

int main(void)
{
    int first = 1;
    int second = 2;

    assert(xTaskCreate(vTraceTask, "first", configMINIMAL_STACK_SIZE,
                       &first, 1U, &first_handle) == pdPASS);
    assert(xTaskCreate(vTraceTask, "second", configMINIMAL_STACK_SIZE,
                       &second, 1U, &second_handle) == pdPASS);
    assert(uxTaskGetNumberOfTasks() == 2U);

    vTaskStartScheduler();

    assert(trace_length == 4);
    assert(trace[0] == 11);
    assert(trace[1] == 21);
    assert(trace[2] == 12);
    assert(trace[3] == 22);
    assert(eTaskGetState(first_handle) == eDeleted);
    assert(eTaskGetState(second_handle) == eDeleted);
    assert(pcTaskGetName(first_handle) != NULL);
    puts("P0 task/context tests passed");
    return 0;
}

