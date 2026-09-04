# P7 Design Delta: Software Timer Service Task

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task contexts, P2 tick preemption, P3 wrap-safe delays,
P4 queues, P5 system-object allocation, and P6 selected heaps

## 1. Scope

P7 adds a FreeRTOS-like software timer layer. Timer callbacks run in a
dedicated timer service task rather than in the POSIX tick signal handler.
This makes the execution context explicit and keeps the tick path limited to
tick accounting, timeout wakeups, and preemption.

The phase supports:

- one-shot and auto-reload timers;
- timer creation with a name, period, callback, and user ID;
- asynchronous start, stop, reset, and period-change commands;
- active-state and timer-ID accessors;
- wrap-safe `TickType_t` deadlines;
- a configurable command queue and timer service task;
- deterministic POSIX tests and a runnable lesson example.

P7 deliberately does not add ISR-safe timer APIs, timer deletion, static timer
allocation, callback cancellation from another callback, or callback worker
tasks. Timer objects live until the teaching process exits, matching the
project's existing no-object-deletion policy for queues and mutexes.

## 2. Public API

Add `include/timers.h` with an opaque handle and the following API:

```c
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
```

### 2.1 API contract

- `name`, `callback`, and a non-zero `period` are required. The period must
  be less than `INT32_MAX`, preserving the P3 half-range rule.
- `auto_reload` must be `pdFALSE` or `pdTRUE`.
- `xTimerCreate` returns `NULL` when validation, heap allocation, or service
  infrastructure creation fails.
- Start and reset both schedule the timer for `now + period`. Starting an
  already-active timer therefore moves its deadline forward.
- Stop makes the timer inactive when its command is processed. Stopping an
  inactive timer succeeds as an idempotent command.
- `xTimerChangePeriod` changes the period and starts the timer at
  `now + new_period`, even if it was inactive. A zero period is rejected.
- The command APIs return `pdPASS` when their command is accepted by the
  timer queue, and `pdFAIL` when the handle/argument is invalid or the queue
  cannot accept the command within `ticks_to_wait`.
- Before the scheduler starts, only `ticks_to_wait == 0` is valid for command
  APIs; a non-zero wait cannot block outside a task context and returns
  `pdFAIL`.
- `xTimerIsTimerActive` observes the state last applied by the service task;
  a successful command enqueue is asynchronous and may not be visible until
  the service task runs.
- Timer IDs are opaque application pointers and are never dereferenced by the
  kernel. `vTimerSetTimerID` changes the value seen by later callbacks.

## 3. Configuration

Add guarded defaults to `include/FreeRTOSConfig.h`:

```c
#ifndef configMAX_TIMER_NAME_LEN
#define configMAX_TIMER_NAME_LEN 16U
#endif

#ifndef configMAX_SYSTEM_TASKS
#define configMAX_SYSTEM_TASKS 2U
#endif

#ifndef configTIMER_QUEUE_LENGTH
#define configTIMER_QUEUE_LENGTH 8U
#endif

#ifndef configTIMER_TASK_PRIORITY
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1U)
#endif

#ifndef configTIMER_TASK_STACK_DEPTH
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE
#endif
```

`configMAX_SYSTEM_TASKS` reserves slots for the Idle Task and the timer
service task. The timer name limit is independent from the task name limit.
The service-task priority must be less than
`configMAX_PRIORITIES`; the queue length and stack depth must be non-zero.
The existing application-task limit in `configMAX_TASKS` is unchanged.

## 4. Timer object and command model

`kernel/timers.c` owns the timer object and command queue. The object is
allocated from the selected P6 heap and contains:

```c
struct mini_timer {
    char name[configMAX_TIMER_NAME_LEN];
    TickType_t period;
    TickType_t expiry_tick;
    BaseType_t auto_reload;
    BaseType_t active;
    void *timer_id;
    TimerCallbackFunction_t callback;
    struct mini_timer *next_active;
};
```

The service queue stores commands by value, so a command never points at a
temporary stack object:

```c
typedef enum {
    eTimerCommandStart,
    eTimerCommandStop,
    eTimerCommandReset,
    eTimerCommandChangePeriod
} TimerCommandKind_t;

typedef struct {
    TimerCommandKind_t kind;
    TimerHandle_t timer;
    TickType_t period;
} TimerCommand_t;
```

The active list is private to the service task. Public command functions only
validate arguments and enqueue a command. This single-writer rule prevents a
tick signal or an application task from observing a partially updated timer
list.

## 5. Service-task lifecycle

Timer infrastructure is created lazily by the first `xTimerCreate` call:

1. Allocate the fixed-length command queue with `xQueueCreate`.
2. Create `vTimerServiceTask` through an internal system-task creation hook.
3. Allocate and initialize the requested timer object from `pvPortMalloc`.

The service task is marked as a system task, not an application task. It is
counted in `uxTaskGetNumberOfTasks`, but it does not consume the
`configMAX_TASKS` application limit and does not keep the scheduler alive once
all application tasks are deleted. The existing Idle Task remains the other
reserved system slot.

The task runs at `configTIMER_TASK_PRIORITY` and never returns:

```text
compute time until the nearest active timer
        |
receive one command with that timeout
        |
command received? -- yes --> apply command, then loop
        |
        no (deadline reached)
        |
expire every due timer and run callbacks, then loop
```

When no timer is active, the task waits for nearly the full signed tick
half-range on the command queue. A command sent to the queue wakes it early,
so the service task does not poll on every tick. If a command changes the
nearest deadline, the next receive timeout is recomputed immediately.

## 6. Timer state transitions

All active-list changes happen in the service task:

| Command/event | State change |
|---|---|
| start/reset | inactive or active → active; `expiry_tick = now + period` |
| change period | period replaced; active → active at `now + new_period` |
| stop | active → inactive; remove from active list |
| one-shot expiry | active → inactive before callback |
| auto-reload expiry | remove before callback, then reinsert at the next period |

At expiry, the service task removes the timer before invoking its callback.
For an auto-reload timer, the next deadline is based on the prior deadline
(`old_expiry + period`) so callback execution time does not accumulate drift.
Commands queued by a callback are processed on the next service-task loop and
therefore can stop, reset, or change the timer deterministically.

Callbacks execute in timer-service-task context. They must not call blocking
APIs or perform unbounded work; a long callback delays every other software
timer. P7 does not provide a callback-specific stack or an ISR callback mode.

## 7. Tick and wrap interaction

The POSIX `SIGALRM` handler remains unchanged except for calling the existing
`vTaskTickISR`. Timer callbacks never run from that signal handler.

Timer periods are limited to `< INT32_MAX`. The service task compares
`(int32_t)(expiry_tick - now)`:

- a value `<= 0` means the timer is due;
- a positive value is the remaining wait in ticks;
- the minimum positive value across active timers determines the queue receive
  timeout.

This is the same half-range rule used by P3 delay lists and remains correct
when `TickType_t` wraps from `UINT32_MAX` to zero.

## 8. Task-kernel integration

`kernel/tasks.c` adds a private system-task creation path used by
`kernel/timers.c`:

- `TCB_t` gains an `is_system` marker;
- the task table grows to `configMAX_TASKS + configMAX_SYSTEM_TASKS` entries;
- application creation still checks only `configMAX_TASKS`;
- `prvAllApplicationTasksDeleted` ignores both Idle and timer service tasks;
- the system creation hook uses the selected P6 heap just like normal tasks.

The timer service task blocks on the existing P4 queue API, so no new wait-list
or context-switch mechanism is introduced. Queue wakeups and timer deadlines
are handled by the existing P2/P3 scheduler paths.

## 9. Error and concurrency policy

- Invalid handles, null callbacks, zero periods, invalid auto-reload values,
  and periods `>= INT32_MAX` return `NULL`/`pdFAIL` without changing state.
- Command queue saturation returns `pdFAIL` after the requested finite wait;
  no command is silently overwritten.
- No timer API is ISR-safe in P7. Calling a blocking command API without a
  running task returns `pdFAIL` rather than attempting a context switch.
- Timer object fields read by application code (`active`, ID) are protected by
  the existing nested critical section. Callbacks and active-list mutations
  are serialized by the service task.
- Timer objects are not freed in P7, so a callback can safely retain its own
  handle for the process lifetime.

## 10. Planned directory changes

```text
include/FreeRTOSConfig.h       # system-task and timer defaults
include/timers.h               # timer types and public API
kernel/tasks_internal.h        # system-task creation hook and TCB marker
kernel/tasks.c                 # system-task slots and exit filtering
kernel/timers.c                # command queue, timer list, service task
tests/test_p7_timers.c         # one-shot, periodic, command, and wrap tests
examples/07_timers/main.c      # observable callback lesson
docs/P7_DESIGN.md
docs/ROADMAP.md
README.md
Makefile                      # P7 target and regression commands
```

The POSIX port and the P6 heap backends remain unchanged.

## 11. Verification plan

The P7 test binary must demonstrate:

1. invalid timer arguments are rejected;
2. a one-shot timer invokes its callback once and exposes its timer ID;
3. an auto-reload timer invokes repeatedly and can be stopped from its
   callback;
4. reset and period-change commands move the observed deadline;
5. a stopped timer does not invoke a late callback;
6. multiple timers expire in deadline order;
7. a timer started near `UINT32_MAX` fires correctly after tick wrap;
8. command queue saturation returns a failure instead of corrupting state;
9. P0-P6 tests and examples remain green;
10. default warnings-as-errors and UBSan builds pass.

Implementation starts only after this document is reviewed and marked
**Frozen for implementation**.
