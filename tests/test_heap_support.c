#include "FreeRTOS.h"
#include <stdio.h>
#include <stdlib.h>

void vAssertCalled(const char *file, int line)
{
    (void)fprintf(stderr, "heap test assertion failed at %s:%d\n", file, line);
    abort();
}

void vPortEnterCritical(void)
{
}

void vPortExitCritical(void)
{
}
