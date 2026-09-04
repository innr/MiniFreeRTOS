#ifndef MINI_TIMERS_H
#define MINI_TIMERS_H

#include "FreeRTOS.h"

typedef struct mini_timer *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t timer);

TimerHandle_t xTimerCreate(const char *name,
                           TickType_t period,
                           BaseType_t auto_reload,
                           void *timer_id,
                           TimerCallbackFunction_t callback);
BaseType_t xTimerStart(TimerHandle_t timer, TickType_t ticks_to_wait);
BaseType_t xTimerStop(TimerHandle_t timer, TickType_t ticks_to_wait);
BaseType_t xTimerReset(TimerHandle_t timer, TickType_t ticks_to_wait);
BaseType_t xTimerChangePeriod(TimerHandle_t timer,
                              TickType_t new_period,
                              TickType_t ticks_to_wait);
BaseType_t xTimerIsTimerActive(TimerHandle_t timer);
void *pvTimerGetTimerID(TimerHandle_t timer);
void vTimerSetTimerID(TimerHandle_t timer, void *timer_id);

#endif
