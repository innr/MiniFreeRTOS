# P5 Design Delta: Mutexes and Priority Inheritance

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task contexts, P1 ready/event ordering, P2 tick/preemption,
P3 delay/timeout lists, and P4 queue/semaphore wait hooks

## 1. Scope

P5 adds an ownership-aware mutex on top of the P4 synchronization primitives:

- FreeRTOS-style mutex creation through `semphr.h`;
- non-recursive ownership and owner-only release;
- priority inheritance when a higher-priority task waits for a mutex;
- disinheritance on release, timeout, or delay abort;
- priority-list reordering when a task's effective priority changes.

The phase keeps the implementation deliberately small for teaching. It does
not add recursive mutexes, priority-ceiling protocols, deadlock detection,
ISR-safe APIs, mutex deletion, or a fully transitive inheritance chain. A task
may hold more than one mutex; its inherited priority is the maximum waiter
priority across all mutexes it owns. Dependency chains in which an inherited
owner is itself blocked on another mutex are outside the P5 guarantee and are
not used by the acceptance tests.

## 2. Public API

The existing `SemaphoreHandle_t`, `xSemaphoreTake`, and `xSemaphoreGive`
interfaces remain the common synchronization API. P5 adds one declaration to
`include/semphr.h`:

```c
SemaphoreHandle_t xSemaphoreCreateMutex(void);
```

Mutexes are intentionally represented by `SemaphoreHandle_t`, matching the
FreeRTOS programming model. A mutex starts available and has one owner at a
time. `xSemaphoreTake` acquires it; `xSemaphoreGive` releases it.

### Contract

- A successful take sets the caller as owner.
- A non-owner give returns `pdFAIL` and does not change the mutex.
- A task taking a mutex it already owns receives `pdFAIL`; P5 is
  non-recursive, so it never self-blocks.
- A zero timeout performs one immediate attempt and never blocks.
- A finite timeout uses the P3 half-range rule:
  `ticks_to_wait < INT32_MAX`.
- Queue/semaphore-style immediate operations that do not block may run before
  the scheduler starts. A mutex take or give must run in a task context because
  ownership is tied to a TCB; a mutex operation outside the running task
  returns `pdFAIL` or asserts when the call would block.

## 3. Object model

P4's private `struct mini_queue` remains the common object representation in
`kernel/queue.c`. P5 extends it as follows:

```c
typedef enum {
    eQueueData,
    eBinarySemaphore,
    eCountingSemaphore,
    eMutex
} QueueKind_t;

struct mini_queue {
    QueueKind_t kind;
    uint8_t *storage;
    size_t storage_size;
    UBaseType_t length;
    UBaseType_t item_size;
    UBaseType_t head;
    UBaseType_t tail;
    UBaseType_t count;
    UBaseType_t max_count;
    TaskEventList_t send_waiters;
    TaskEventList_t receive_waiters;
    TCB_t *owner;                 /* used only for eMutex */
    struct mini_queue *registry_next; /* all allocated mutexes */
};
```

A mutex is initialized with `count == 1`, `max_count == 1`, and `owner ==
NULL`. On take, `count` becomes zero and `owner` points at the caller. On give,
the owner is cleared before a waiter is woken. The mutex registry is an
internal singly linked list; P5 has no object-destruction API, so the registry
does not contain dangling entries.

## 4. TCB priority and wait-state additions

P4 already stores the effective priority in `TCB_t::priority`. P5 adds:

```c
UBaseType_t base_priority;      /* task's configured priority */
UBaseType_t inherited_priority; /* highest active mutex waiter */
```

`TaskWaitReason_t` gains `eTaskWaitMutexTake`.

The three values have one invariant:

```text
priority == max(base_priority, inherited_priority)
```

`xTaskCreate` initializes `base_priority` and `priority` to the requested
priority, while `inherited_priority` starts at `tskIDLE_PRIORITY` to represent
that no mutex waiter is active. The existing `uxTaskPriorityGet` continues to
report the effective `priority`. The existing `vTaskPrioritySet` changes
`base_priority` and immediately recomputes the effective value, so a held mutex
can never be used to permanently override a later application priority change.

The scheduler exposes two internal helpers to the synchronization code:

- `vTaskSetEffectivePriority(task, priority)` changes a task's effective
  priority and repositions it in the ready list or event wait list as needed;
- `vTaskSetInheritedPriority(task, priority)` records the inherited value and
  applies `max(base_priority, priority)` through the first helper.

Both helpers are used while the caller holds the existing critical section.
When a blocked task is moved within an event list, equal-priority insertion
remains FIFO.

## 5. Priority inheritance policy

When a task blocks on an owned mutex, the mutex owner inherits the waiter's
current effective priority if it is higher. The owner is moved to the
corresponding ready-list priority if it is ready; if it is delayed or running,
only the effective value changes.

The effective inherited value is recomputed by scanning all mutexes whose
`owner` is the task and taking the maximum priority at each mutex's event-list
head. This supports multiple held mutexes without adding another intrusive
list to the TCB:

```text
new_inherited = max(head(waiters(mutex)) .priority
                    for every mutex owned by task)
new_effective = max(base_priority, new_inherited)
```

If a waiter is removed by a successful handoff, timeout, or delay abort, the
old owner is recomputed against the remaining waiters. P5 does not propagate
inheritance through a chain of owners; such a chain is explicitly outside the
acceptance contract.

## 6. Take algorithm

`xSemaphoreTake(mutex, ticks_to_wait)` follows this sequence inside the
existing critical section:

1. If `owner == NULL`, set `owner = current`, decrement `count`, and return
   `pdPASS`.
2. If `owner == current`, return `pdFAIL` (non-recursive mutex).
3. If `ticks_to_wait == 0`, return `pdFAIL` without changing the owner.
4. Put the current task on `receive_waiters` with
   `eTaskWaitMutexTake`, then recompute the owner inheritance from the new
   wait list.
5. Leave the critical section and yield.
6. On an event wake, clear the old wait metadata, preserve the remaining
   timeout, and retry from step 1. On timeout or abort, clear the metadata,
   recompute the owner, and return `pdFAIL`.

The retry is intentional: another task may acquire the mutex before the woken
task gets CPU time. Ownership is assigned only by the successful retry, so
there is one ownership rule for both immediate and woken takes.

## 7. Give algorithm

`xSemaphoreGive(mutex)` is valid only for the current owner:

1. Verify `owner == current`; otherwise return `pdFAIL` and leave all state
   unchanged.
2. Save the current owner, clear `owner`, and restore `count` to one.
3. Remove and ready the highest-priority waiter, if one exists.
4. Recompute the saved owner's inherited/effective priority from any other
   mutexes it still owns and from the remaining waiters.
5. Request a context switch after leaving the critical section when the
   woken waiter outranks the now-disinherited owner.

The waiter retries the take path and becomes the new owner. If it loses a race
to another ready task, it may block again using its remaining timeout.

Disinheritance happens before the give operation can return to user code. This
is important when the released mutex's waiter has a higher priority than the
old owner.

## 8. Timeout, abort, and scheduler interaction

P3 timeout processing already removes a task from the delay list and P4
removes it from its event list. P5 adds a wait-end hook so a mutex waiter being
removed by timeout or `xTaskAbortDelay` immediately triggers recomputation of
its owner's inherited priority. The blocked API then clears the remaining
wait metadata before returning to user code.

All ownership changes, event-list changes, and priority recomputation happen
inside the existing nested critical section. Context switches happen only
after the section is exited. A higher-priority waiter is selected according to
the P4 priority-ordered event list; equal priorities preserve FIFO order.

A task must release every mutex before returning from its task function. P5
will assert this teaching contract at task exit rather than silently releasing
another task's ownership.

## 9. Error policy

- `xSemaphoreCreateMutex` returns `NULL` on allocation failure.
- Taking/giving a queue, binary semaphore, or counting semaphore through the
  mutex-only path returns `pdFAIL`; existing P4 wrong-kind behavior remains.
- A non-owner give and a recursive take return `pdFAIL` without side effects.
- A mutex take that would block outside a running task calls `configASSERT`.
- Invalid timeout values call `configASSERT`, matching P3/P4.
- A task that exits while owning a mutex calls `configASSERT`.
- Runtime success/failure uses `pdPASS`/`pdFAIL`; no errno is added.

## 10. Planned directory changes

```text
include/semphr.h                 # xSemaphoreCreateMutex
kernel/tasks_internal.h          # base/effective priority hooks
kernel/tasks.c                   # priority reordering and wait-end hook
kernel/queue.c                   # mutex object, ownership, inheritance
tests/test_p5_mutex.c            # ownership, inversion, timeout, disinherit
examples/05_mutex/main.c         # observable priority-inversion lesson
docs/P5_DESIGN.md
Makefile                         # P5 target and regression commands
README.md                        # current milestone and lesson entry
docs/ROADMAP.md                  # mark P5 complete after merge
```

The POSIX port remains unchanged. Public headers continue to expose no POSIX
types.

## 11. Verification plan and acceptance criteria

The P5 test binary must demonstrate:

1. a newly created mutex is available and a successful take establishes its
   owner;
2. recursive take and non-owner give fail without corrupting ownership;
3. a high-priority waiter boosts a low-priority owner above a medium-priority
   CPU-bound task;
4. releasing the mutex wakes the highest-priority waiter and restores the old
   owner's base priority;
5. multiple mutexes retain the maximum inherited priority until the relevant
   waits disappear;
6. a timed-out or aborted waiter is removed and no longer keeps the owner
   boosted;
7. P0-P4 regression tests and examples remain green;
8. `make all` passes with `-Wall -Wextra -Wpedantic -Werror` and UBSan.

Implementation starts only after this document is reviewed and marked
**Frozen for implementation**.
