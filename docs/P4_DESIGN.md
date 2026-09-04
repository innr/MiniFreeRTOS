# P4 Design Delta: Queues and Semaphores

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task contexts, P1 ready/event ordering, P2 tick/preemption,
and P3 delay/timeout lists

## 1. Scope

P4 introduces inter-task communication and synchronization on top of the P3
blocked-task and timeout machinery:

- fixed-length queues that copy items by value;
- blocking send and receive with finite tick timeouts;
- binary semaphores;
- counting semaphores;
- priority-ordered event wait lists shared by queue and semaphore objects.

P4 does not add queue overwrite, queue sets, mutex ownership, priority
inheritance, ISR-safe `FromISR` APIs, or a kernel heap allocator.

## 2. Public API

### Queue API (`include/queue.h`)

```c
typedef struct mini_queue *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t queue_length,
                           UBaseType_t item_size);
BaseType_t xQueueSend(QueueHandle_t queue,
                      const void *item,
                      TickType_t ticks_to_wait);
BaseType_t xQueueReceive(QueueHandle_t queue,
                         void *buffer,
                         TickType_t ticks_to_wait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
```

`xQueueCreate` requires both dimensions to be non-zero and allocates a ring
buffer of `queue_length * item_size` bytes. `xQueueSend` copies one item into
the next free slot; `xQueueReceive` copies the oldest item out. The queue owns
the bytes, so the caller's item may be a task-local variable.

### Semaphore API (`include/semphr.h`)

```c
typedef struct mini_queue *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max_count,
                                           UBaseType_t initial_count);
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore,
                          TickType_t ticks_to_wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);
```

Binary semaphores start empty (`count == 0`) and have a maximum count of one.
Counting semaphores require `0 <= initial_count <= max_count`. An initial count
of zero is useful for producer/consumer handoff and matches the usual
FreeRTOS counting-semaphore contract. Giving a full
semaphore fails; taking an empty semaphore blocks or fails according to the
timeout.

All blocking timeouts are finite and use the P3 half-range rule:
`ticks_to_wait < INT32_MAX`. A zero timeout performs one immediate attempt and
never blocks.

## 3. Kernel object model

`kernel/queue.c` owns one opaque object representation for queues and
semaphores:

```c
typedef enum {
    eQueueData,
    eBinarySemaphore,
    eCountingSemaphore
} QueueKind_t;

struct mini_queue {
    QueueKind_t kind;
    uint8_t *storage;       /* NULL for semaphores */
    size_t storage_size;
    UBaseType_t length;
    UBaseType_t item_size;
    UBaseType_t head;       /* next write slot */
    UBaseType_t tail;       /* next read slot */
    UBaseType_t count;
    UBaseType_t max_count;
    TaskEventList_t send_waiters;
    TaskEventList_t receive_waiters;
};
```

Queue handles and semaphore handles are opaque public pointers. A queue uses
`send_waiters` when full and `receive_waiters` when empty. A semaphore uses only
`receive_waiters` for blocked takes.

## 4. Event wait lists and TCB additions

P4 extends each TCB with one event-list linkage and wait result:

```c
TCB_t *event_previous;
TCB_t *event_next;
TaskEventList_t *event_list;
void *wait_object;
TaskWaitReason_t wait_reason;
BaseType_t wait_result;
BaseType_t wait_has_timeout;
```

`TaskEventList_t` is an intrusive list owned by a queue/semaphore. Its head is
the highest-priority waiter; equal-priority waiters are FIFO. A blocked task may
be in one event list and, when a timeout is requested, the P3 delay list at the
same time.

The scheduler exposes internal hooks to `queue.c`:

- place the current task on an event list and optional delay list;
- remove one highest-priority waiter and make it ready;
- remove a task from its event list during timeout or delay abort;
- report/clear the event result and calculate remaining timeout.

The public API remains independent of these internal list structures.

## 5. Queue operation semantics

### Send

1. Enter a critical section.
2. If the queue has space, copy the item, advance `head`, increment `count`,
   and wake one receive waiter if present.
3. If full and `ticks_to_wait == 0`, leave the critical section and return
   `pdFAIL`.
4. If full and waiting is allowed, place the current task on `send_waiters`
   with an absolute P3 timeout, leave the critical section, and yield.
5. On resume, a successful event wake retries the send with the remaining
   timeout; a timeout returns `pdFAIL`.

### Receive

1. Enter a critical section.
2. If the queue is non-empty, copy the oldest item, advance `tail`, decrement
   `count`, and wake one send waiter if present.
3. If empty and `ticks_to_wait == 0`, leave the critical section and return
   `pdFAIL`.
4. If empty and waiting is allowed, place the current task on
   `receive_waiters`, leave the critical section, and yield.
5. On resume, a successful event wake retries the receive with the remaining
   timeout; a timeout returns `pdFAIL`.

The retry is intentional: another ready task may consume the resource before a
woken waiter gets CPU time. It preserves the caller's original timeout budget.

## 6. Semaphore operation semantics

`xSemaphoreTake` is the receive-side algorithm without a data copy: decrement
`count` when non-zero, otherwise wait on `receive_waiters` and retry after an
event wake. `xSemaphoreGive` increments `count` when below `max_count`, wakes
one highest-priority waiter, and requests an immediate context switch if that
waiter outranks the giver.

The count is incremented before waking. The resumed taker consumes the token in
the normal take path, which keeps one consistent ownership rule for all waiters.

## 7. Timeout and wakeup interaction

The P3 tick handler already removes expired delay-list entries before choosing a
new task. For a task that is also in an event list, timeout processing removes it
from that event list, marks `wait_result = pdFALSE`, and moves it to the ready
list. An event operation removes the task from both lists, marks
`wait_result = pdTRUE`, and makes it ready. The blocked API clears the wait
metadata after it resumes.

Blocking and wakeup always happen inside the existing nested critical section;
the context switch occurs only after leaving it. A higher-priority unblocked
task causes an immediate yield when preemption is enabled. Equal-priority
wakeups follow the P1 FIFO ready-list rule.

## 8. Error policy

- `xQueueCreate` returns `NULL` for zero dimensions, allocation failure, or a
  size multiplication overflow.
- Queue/semaphore operations assert non-NULL handles and buffers and reject an
  operation used with the wrong object kind by returning `pdFAIL`.
- Giving a full semaphore returns `pdFAIL`; taking/giving with a zero timeout
  never blocks.
- Invalid blocking context (outside a running task) and out-of-range timeouts
  call `configASSERT`, matching P3 task-delay contracts.
- Runtime success/failure uses `pdPASS`/`pdFAIL`; no errno is added.

## 9. Directory changes

```text
include/queue.h
include/semphr.h
kernel/queue.c
tests/test_p4_queue.c
tests/test_p4_semaphore.c
examples/04_ipc/main.c
docs/P4_DESIGN.md
```

The POSIX port is unchanged except for the scheduler hooks already required by
P3. No POSIX type appears in public queue/semaphore interfaces.

## 10. Verification plan and acceptance criteria

The P4 tests must demonstrate:

1. queue data is copied by value and received FIFO;
2. a receiver blocks on an empty queue and wakes when a sender posts;
3. a sender blocks on a full queue and wakes when a receiver frees a slot;
4. queue timeout returns `pdFAIL` after the requested ticks;
5. binary semaphore starts empty, wakes its highest-priority taker, and rejects
   a second give while full;
6. counting semaphore honors maximum and initial counts and wakes blocked takers;
7. semaphore timeout returns `pdFAIL` without corrupting the count;
8. P0–P3 regression tests and examples remain green;
9. `make all` passes with `-Wall -Wextra -Wpedantic -Werror` and UBSan.
