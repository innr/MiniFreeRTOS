# P3 Design Delta: Delay, Blocked State, and Timeout Wakeup

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task contexts, P1 ready lists/Idle Task, and P2 tick/preemption

## 1. Scope

P3 adds relative task delays and the scheduler state needed to stop a task from
using the CPU until its timeout expires. A delayed task is removed from the
ready set, placed in an ordered delay list, and moved back to its priority ready
list by the tick path.

The phase also adds an absolute-period helper and an explicit delay-abort API so
the later queue/semaphore phase can reuse the timeout machinery.

P3 does not add event lists, queues, semaphores, mutexes, memory allocators, or
ISR-safe blocking APIs.

## 2. Public API

```c
void vTaskDelay(TickType_t ticks_to_delay);
void vTaskDelayUntil(TickType_t *previous_wake_time,
                     TickType_t time_increment);
BaseType_t xTaskAbortDelay(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
```

### `vTaskDelay`

- Must be called by the currently running task after the scheduler starts.
- `ticks_to_delay == 0` is a cooperative yield and does not enter `eBlocked`.
- For a non-zero delay, the wake tick is `current_tick + ticks_to_delay`
  using unsigned `TickType_t` arithmetic.
- The current task transitions `eRunning -> eBlocked`, yields the CPU, and
  returns only after it has been moved back to `eReady` and scheduled again.
- Relative delays are limited to less than half the tick range
  (`ticks_to_delay < INT32_MAX`) so wrap-safe comparisons are unambiguous.

### `vTaskDelayUntil`

- `*previous_wake_time` is the task's absolute next deadline.
- The function advances that deadline by `time_increment` before deciding
  whether to block.
- If the deadline is still in the future, it delays for the remaining ticks.
- If the task is already late, it yields once without adding another delay;
  this preserves a periodic task's phase instead of accumulating drift.

### `xTaskAbortDelay`

- A task in `eBlocked` is removed from the delay list, marked `eReady`, and
  inserted into its priority ready list.
- Returns `pdPASS` when a blocked delay was aborted, otherwise `pdFAIL`.
- If the unblocked task has a higher priority than the running task and
  preemption is enabled, the caller yields after leaving the critical section.
- Aborting a task does not change its priority or task parameters.

## 3. TCB and list data structures

Each TCB gains:

```c
TickType_t wake_tick;
TCB_t *delay_previous;
TCB_t *delay_next;
BaseType_t in_delay_list;
```

The kernel owns one intrusive, deadline-sorted `DelayList_t`:

```c
typedef struct {
    TCB_t *head;
    TCB_t *tail;
    UBaseType_t length;
} DelayList_t;
```

The list is sorted using a signed half-range comparison:

```c
tick_before(a, b)  := (int32_t)(a - b) < 0
tick_reached(now, t) := (int32_t)(now - t) >= 0
```

This keeps deadlines that cross `UINT32_MAX -> 0` in the correct order. The
half-range API limit is part of the teaching contract; a production port may
instead use FreeRTOS-style current/overflow delayed lists.

## 4. State transitions and scheduler interaction

### Delay path

1. Enter the existing port critical section to prevent a tick from observing a
   partially updated TCB.
2. Compute and store `wake_tick`.
3. Mark the running task `eBlocked` and insert it into `DelayList_t`.
4. Leave the critical section before switching contexts, so the scheduler
   context keeps `SIGALRM` enabled on the POSIX port.
5. Yield to the scheduler. No ready-list insertion occurs for the blocked task.

When the task is eventually selected, the scheduler marks it `eRunning` and the
`vTaskDelay` call resumes at its original instruction after the context switch.

### Tick path

On every tick, in order:

1. increment the global tick count;
2. remove every delay-list head for which `tick_reached(now, wake_tick)` is
   true, mark it `eReady`, and append it to its priority ready list;
3. apply P2 preemption to the currently running task.

Wakeups happen before selecting the next task, so a newly woken higher-priority
task can run immediately on that tick. Equal-priority wakeups follow P1 FIFO
ready-list order.

### Idle behavior

If all application tasks are blocked, the P1 Idle Task remains ready. Each tick
preempts idle, advances time, and checks the delay list. On the POSIX port idle
waits in a port-provided `pause()` hook between ticks instead of spinning; the
kernel remains independent of POSIX headers. The scheduler exits only after all
application tasks are `eDeleted`, as before.

## 5. Critical sections and POSIX limitation

Delay-list insertion/removal and state changes are protected with the existing
nested `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` API. The critical section
ends before `swapcontext()` so a blocked task cannot accidentally save a signal-
masked scheduler context.

The POSIX implementation still uses `swapcontext()` from `SIGALRM` for teaching;
this is not an async-signal-safe production scheduler. Cortex-M will later use
PendSV/SysTick-safe primitives.

## 6. Error policy

- Invalid task handles passed to `xTaskAbortDelay` return `pdFAIL`.
- Aborting a non-blocked task returns `pdFAIL`.
- Programmer contract violations (`NULL` period pointer, delay from outside a
  running task, unbalanced critical sections, or an out-of-range delay) call
  `configASSERT`.
- Runtime success/failure uses `pdPASS`/`pdFAIL`; no errno is introduced.

## 7. Verification plan

The P3 test binary runs with POSIX preemption enabled and must verify:

1. a delayed task does not execute before its deadline;
2. a one-tick delay resumes after exactly one tick;
3. multiple delayed tasks wake in deadline order, with FIFO order for ties;
4. a higher-priority task waking from delay preempts the idle/lower-priority
   task on that tick;
5. all application tasks can block while idle keeps the clock advancing;
6. a delay crossing `UINT32_MAX` wakes at the wrapped deadline;
7. `vTaskDelayUntil` avoids cumulative period drift;
8. `xTaskAbortDelay` returns the task to ready state and can trigger a priority
   handoff;
9. P0, P1, and P2 regression tests remain green.

Builds use C11, `-Wall -Wextra -Wpedantic -Werror`, and an UndefinedBehaviorSanitizer
run. AddressSanitizer remains optional because the POSIX `ucontext` teaching port
is incompatible with LeakSanitizer's stack accounting.

## 8. Acceptance criteria

- `vTaskDelay` implements the `eRunning -> eBlocked -> eReady` lifecycle.
- Tick processing wakes all expired tasks without losing a tick at wraparound.
- A blocked task consumes no ready-list slot and cannot be selected until wakeup.
- Delay abort and periodic-delay APIs have deterministic tests.
- `make all` passes with warnings treated as errors.
- No POSIX headers leak into public kernel interfaces.
