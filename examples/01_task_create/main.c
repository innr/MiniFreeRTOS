#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

static void vCounterTask(void *parameters)
{
    const char *label = parameters;
    int private_counter = 0;

    for (int round = 0; round < 3; ++round) {
        ++private_counter;
        (void)printf("task=%s private_counter=%d\n", label, private_counter);
        taskYIELD();
    }
}

int main(void)
{
    configASSERT(xTaskCreate(vCounterTask, "Task-A",
                             configMINIMAL_STACK_SIZE, "A", 1U, NULL) == pdPASS);
    configASSERT(xTaskCreate(vCounterTask, "Task-B",
                             configMINIMAL_STACK_SIZE, "B", 1U, NULL) == pdPASS);
    vTaskStartScheduler();
    return 0;
}

