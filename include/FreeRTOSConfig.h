#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configMAX_TASKS          16U
#define configMAX_TASK_NAME_LEN  16U
#define configMINIMAL_STACK_SIZE (32U * 1024U)
#define configMAX_PRIORITIES     8U
#define configTICK_RATE_HZ       1000U
#define configUSE_TIME_SLICING   1U

#ifndef configUSE_PREEMPTION
#define configUSE_PREEMPTION     1U
#endif

#endif
