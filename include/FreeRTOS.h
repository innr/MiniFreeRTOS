#ifndef MINI_FREERTOS_H
#define MINI_FREERTOS_H

#include <stddef.h>
#include <stdint.h>
#include "FreeRTOSConfig.h"

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
typedef size_t StackType_t;

#define pdFALSE ((BaseType_t)0)
#define pdTRUE  ((BaseType_t)1)
#define pdFAIL  pdFALSE
#define pdPASS  pdTRUE

#define configASSERT(condition) do { if (!(condition)) vAssertCalled(__FILE__, __LINE__); } while (0)
void vAssertCalled(const char *file, int line);

void vPortEnterCritical(void);
void vPortExitCritical(void);
#define taskENTER_CRITICAL() vPortEnterCritical()
#define taskEXIT_CRITICAL()  vPortExitCritical()

#endif
