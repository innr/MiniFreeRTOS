# P8 Design Delta: Trace Tools and Teaching Labs

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task contexts, P1 scheduler, P2 tick preemption, P3 delays,
P4 queues and semaphores, P5 mutexes, P6 heaps, and P7 software timers

## 1. Purpose and scope

P8 makes the kernel observable. A learner should be able to run one lesson,
capture the important kernel transitions, and answer *why* a task ran, blocked,
woke, or caused another task to run. The trace facility is diagnostic only: it
must never choose a different task, wake a different waiter, or make a kernel
operation fail.

P8 has four deliverables:

1. a fixed-size, allocation-free kernel event recorder;
2. a small public reader API and a stable CSV stream produced by an example;
3. a dependency-free host report tool for turning the stream into a readable
   timeline and summary;
4. a lab workbook that covers the existing P0-P7 lessons and a new integrated
   trace lesson.

The first implementation remains deliberately text based. A GUI, a live
socket protocol, a production tracing format, and a Cortex-M trace backend are
future work and are not part of P8.

## 2. Non-goals

P8 does not add new scheduling, IPC, timer, or memory semantics. It does not
add task deletion APIs, ISR-safe application APIs, SMP support, GDB integration,
ETM/ITM output, or a binary trace file format. The POSIX signal handler remains
the tick source, and trace callbacks never run from that handler.

## 3. Configuration

Add guarded defaults to `include/FreeRTOSConfig.h`:

```c
#ifndef configTRACE_ENABLED
#define configTRACE_ENABLED          1U
#endif

#ifndef configTRACE_BUFFER_LENGTH
#define configTRACE_BUFFER_LENGTH    256U
#endif

#ifndef configTRACE_INCLUDE_TICKS
#define configTRACE_INCLUDE_TICKS    0U
#endif
```

`configTRACE_ENABLED` removes recording work when set to zero. When tracing is
enabled, `configTRACE_BUFFER_LENGTH` must be non-zero. The event buffer is
static and is not obtained from `heap_1`, `heap_2`, or `heap_4`, so enabling
trace cannot make a task or queue allocation fail. Tick events are disabled by
default because they are high volume; every other event still carries its
current tick. A lesson may enable them with
`-DconfigTRACE_INCLUDE_TICKS=1U` when it wants a tick-by-tick view.

## 4. Public trace API

Add `include/trace.h`:

```c
typedef enum {
    eTraceTaskSwitch,
    eTraceTaskReady,
    eTraceTaskBlocked,
    eTraceTaskWake,
    eTraceTaskDeleted,
    eTraceTick,
    eTraceQueueSend,
    eTraceQueueReceive,
    eTraceSemaphoreTake,
    eTraceSemaphoreGive,
    eTraceMutexInherit,
    eTraceMutexRelease,
    eTraceTimerCommandQueued,
    eTraceTimerCommandApplied,
    eTraceTimerCallback
} TraceEventType_t;

typedef struct {
    uint32_t sequence;
    TickType_t tick;
    TraceEventType_t type;
    TaskHandle_t task;
    TaskHandle_t related_task;
    const void *object;
    UBaseType_t value;
} TraceEvent_t;

void vTraceReset(void);
BaseType_t xTraceRead(TraceEvent_t *event);
UBaseType_t uxTraceGetCount(void);
uint32_t ulTraceGetDroppedCount(void);
const char *pcTraceEventTypeName(TraceEventType_t type);
```

The `task` and `related_task` fields are opaque handles. Their names and
priorities can be resolved with the existing task API while the process is
alive. `object` is an opaque queue, semaphore, mutex, or timer handle. The
kernel does not dereference any trace object, and tracing does not take
ownership of it.

`value` is event-specific:

| Event type | `task` | `related_task` | `object` | `value` |
|---|---|---|---|---|
| task switch | selected task | previous task, if any | `NULL` | selected priority |
| task ready/wake/blocked/deleted | affected task | `NULL` | wait object, if any | state or wait reason |
| tick | current task, if any | `NULL` | `NULL` | tick value |
| queue send/receive | calling task | `NULL` | queue handle | messages waiting after operation |
| semaphore take/give | calling task | `NULL` | semaphore handle | count after operation |
| mutex inherit | owner | waiter | mutex handle | new effective priority |
| mutex release | releasing task | next waiter, if any | mutex handle | owner priority after release |
| timer command queued/applied | caller/service task | `NULL` | timer handle | command kind |
| timer callback | timer service task | `NULL` | timer handle | one-shot or auto-reload flag |

Only completed operations are recorded for queues and synchronization objects.
A blocked operation is visible through the task-blocked and task-wake events.
Timer command events intentionally distinguish enqueue from application so the
asynchronous service-task boundary is visible.

## 5. Ring-buffer semantics

`kernel/trace.c` owns a static array of `TraceEvent_t` with
`configTRACE_BUFFER_LENGTH` entries. Recording is non-blocking and performs no
allocation, queue operation, or task yield.

- Each record receives a monotonically increasing `sequence` number.
- Events are read oldest-first with `xTraceRead`; a successful read consumes
  one event and returns `pdPASS`.
- `uxTraceGetCount` reports unread events, capped at the buffer length.
- When the buffer is full, the newest event overwrites the oldest one and
  `ulTraceGetDroppedCount` increases by one. The kernel operation that caused
  the overwrite still succeeds.
- `vTraceReset` clears unread events, the dropped counter, and the sequence
  counter. It is intended for use before starting a scheduler or after it has
  stopped.
- An invalid `TraceEventType_t` passed to `pcTraceEventTypeName` returns
  `"unknown"`.

The writer briefly masks the POSIX tick signal while updating the ring. The
tick path uses the same-thread signal context and never calls an allocating or
blocking routine. The reader also masks the signal so a foreground lesson can
drain the ring while the scheduler is running. `xTraceRead` is not an ISR API.

When `configTRACE_ENABLED == 0U`, the public functions remain linkable but
recording, counting, and dumping are no-ops/empty results. The instrumentation
sites compile away through internal `TRACE_RECORD(...)` macros.

## 6. Instrumentation points

Instrumentation is placed at state-commit points, after the kernel has made a
change and before it can yield:

| Component | Events |
|---|---|
| `kernel/tasks.c` | task switch, ready insertion, block, wake, delete, optional tick |
| `kernel/queue.c` | successful queue send/receive, semaphore take/give, mutex inheritance/release |
| `kernel/timers.c` | command enqueue, command application, callback entry |

The task-switch event is emitted by the scheduler when it selects the next
ready task. A task-blocked event carries the wait reason; a wake event is
emitted for both timeout wakeups and object-driven unblocks, with the event
object identifying the distinction. A timer callback event is emitted
immediately before the user callback and therefore proves that the callback is
running in the timer-service-task context.

Trace recording must not call an API that can yield or mutate a ready/event
list. Existing critical-section boundaries remain the source of truth for
atomicity; trace code only observes the already-committed fields.

## 7. CSV stream contract

`examples/08_trace` runs one deterministic scenario using a queue, a mutex,
two application tasks, and an auto-reload software timer. After the scheduler
stops it prints a header followed by one line per unread event:

```text
sequence,tick,event,task,related_task,object,value
```

Names are escaped as CSV fields. A null task is printed as `-`; an object is
printed as a stable process-local hexadecimal pointer; numeric fields are
printed in decimal. The example prints a final metadata line:

```text
# dropped=<number>
```

The stream is intentionally easy to inspect with standard shell tools and is
not promised to be a long-term file format. The event names come from
`pcTraceEventTypeName`, so the host tool does not duplicate the enum table.

## 8. Host report tool

Add `tools/trace_report.py`, using only the Python standard library. It reads
the CSV stream from a path or stdin and prints:

1. total events and dropped events;
2. event counts grouped by event name;
3. a chronological compact timeline (`tick task event object value`);
4. per-task switch counts and first/last observed tick.

Malformed data is reported with a non-zero exit status and a line number. The
report never assumes pointer values are stable between runs. A documented
command is:

```sh
make trace > /tmp/minifreertos-trace.csv
python3 tools/trace_report.py /tmp/minifreertos-trace.csv
```

The Makefile target may use `TRACE_TICKS=1` to compile the example with
`configTRACE_INCLUDE_TICKS=1U`; the default output omits per-tick records.

## 9. Teaching labs

Add `docs/P8_TRACE_LAB.md` and `docs/LABS.md`. The workbook reuses the
existing examples instead of creating a second kernel API:

| Lab | Run | Questions the trace should answer |
|---|---|---|
| Scheduler | `examples/01_task_create`, `02_preemption` | Which task was selected? When does time slicing matter? |
| Time | `examples/03_delay` | Which event blocks a task, and what tick wakes it? |
| IPC | `examples/04_ipc` | Which send/receive causes a waiter to wake? |
| Priority inversion | `examples/05_mutex` | When is inheritance applied and removed? |
| Memory | `examples/06_heap` with `HEAP_SCHEME=1/2/4` | Which allocator changes capacity/fragmentation, and why is it not a scheduling event? |
| Timers | `examples/07_timers` | Which task applies a timer command, and where does the callback run? |
| Integrated trace | `examples/08_trace` | Can the complete event sequence explain the final output? |

Each lab contains a goal, one command, an expected observation, and a small
extension exercise (for example, changing one priority or period). The labs
must not require a debugger, a GUI, or wall-clock sleeps beyond the existing
POSIX tick simulation.

## 10. Planned directory changes

```text
include/FreeRTOSConfig.h       # trace configuration defaults
include/trace.h                # public event and reader API
kernel/trace.c                 # static ring buffer
kernel/tasks.c                 # task and tick instrumentation
kernel/queue.c                 # queue/semaphore/mutex instrumentation
kernel/timers.c                # timer command/callback instrumentation
tests/test_p8_trace.c          # ring and integrated event assertions
examples/08_trace/main.c       # deterministic CSV producer
tools/trace_report.py          # standard-library host report
docs/P8_TRACE_LAB.md           # guided exercises
docs/LABS.md                   # lesson index for P0-P8
docs/ROADMAP.md                # mark P8 complete only after merge
README.md                     # trace commands and current milestone
Makefile                       # trace target and P8 regression target
```

No POSIX type is added to the public trace header. The POSIX port is changed
only if a small helper is needed to mask the tick signal; scheduling and tick
semantics remain in `kernel/tasks.c`.

## 11. Verification plan

The P8 test and lesson gates are:

1. ring wrap retains the newest `configTRACE_BUFFER_LENGTH` records and counts
   every overwrite;
2. reset returns count, dropped count, and sequence to their initial state;
3. disabled tracing leaves the public API empty and does not change scheduler
   results;
4. a deterministic task scenario contains task-switch, block, wake, and
   delete events in causal order;
5. queue, semaphore, mutex-inheritance, and timer events identify the correct
   actor and object;
6. timer callback events identify the timer service task rather than the tick
   signal context;
7. `examples/08_trace` emits valid CSV and `trace_report.py` accepts it;
8. all existing P0-P7 tests and examples pass for `heap_1`, `heap_2`, and
   `heap_4`;
9. warnings-as-errors, UBSan, the static allocation scan, and the trace tool's
   malformed-input test pass;
10. every lab has a reproducible command and an expected observation.

Implementation starts only after this document is reviewed and its status is
changed to **Frozen for implementation**.
