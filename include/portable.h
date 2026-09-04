#ifndef MINI_PORTABLE_H
#define MINI_PORTABLE_H

#include "FreeRTOS.h"

void *pvPortMalloc(size_t size);
void vPortFree(void *pointer);
size_t xPortGetFreeHeapSize(void);

#endif
