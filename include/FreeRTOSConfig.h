#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configMAX_TASKS          16U
#define configMAX_TASK_NAME_LEN  16U
#define configMINIMAL_STACK_SIZE (32U * 1024U)
#define configMAX_PRIORITIES     8U
#define configTICK_RATE_HZ       1000U
#define configUSE_TIME_SLICING   1U

#ifndef configMAX_TIMER_NAME_LEN
#define configMAX_TIMER_NAME_LEN 16U
#endif

#ifndef configMAX_SYSTEM_TASKS
#define configMAX_SYSTEM_TASKS    2U
#endif

#ifndef configTIMER_QUEUE_LENGTH
#define configTIMER_QUEUE_LENGTH  8U
#endif

#ifndef configTIMER_TASK_PRIORITY
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1U)
#endif

#ifndef configTIMER_TASK_STACK_DEPTH
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE
#endif

#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE     (1024U * 1024U)
#endif

#ifndef configHEAP_SCHEME
#define configHEAP_SCHEME          4U
#endif

#ifndef portBYTE_ALIGNMENT
#define portBYTE_ALIGNMENT         8U
#endif

#ifndef configUSE_PREEMPTION
#define configUSE_PREEMPTION     1U
#endif

#endif
