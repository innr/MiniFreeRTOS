# MiniFreeRTOS

MiniFreeRTOS is a from-scratch, FreeRTOS-like teaching kernel written in C. The
first port runs on a POSIX PC, letting you learn and debug kernel concepts before
moving to Cortex-M exception-based context switching.

## Current milestone: P8

Implemented:

- FreeRTOS-style task API
- task control blocks (TCBs)
- independent task stacks
- POSIX `ucontext` port layer
- cooperative context switching with `taskYIELD()`
- task return and deleted state
- strict numeric-priority selection
- equal-priority round robin at `taskYIELD()`
- internal Idle Task and priority getter/setter
- periodic POSIX `SIGALRM` tick simulation
- preemption and equal-priority time slicing
- nested `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` masking
- `vTaskDelay()` and tick-driven blocked-to-ready wakeups
- wrap-safe ordered delay list
- `vTaskDelayUntil()` periodic deadlines
- `xTaskAbortDelay()` timeout cancellation
- fixed-length, copy-by-value queues with blocking send/receive
- priority-ordered event wait lists with finite tick timeouts
- binary and counting semaphores
- non-recursive mutexes with owner-only release
- priority inheritance and disinheritance across mutex waiters
- selectable `heap_1`, `heap_2`, and `heap_4` allocators
- aligned `pvPortMalloc()`/`vPortFree()` with free-byte statistics
- software timer service task with one-shot and auto-reload timers
- asynchronous timer start, stop, reset, and period-change commands
- allocation-free static trace ring with task, IPC, mutex, and timer events
- CSV trace lesson and standard-library host report

Not implemented yet: interrupt-safe APIs or a hardware port.

## Run the first lesson

```sh
make test
make example
```

Expected example order:

```text
task=A private_counter=1
task=B private_counter=1
task=A private_counter=2
task=B private_counter=2
task=A private_counter=3
task=B private_counter=3
```

`make example` also runs `examples/02_preemption`, where two CPU-bound tasks
never call `taskYIELD()` but both continue to make progress because the POSIX
tick interrupts them.

`make example` runs `examples/03_delay`, where a sensor task sleeps between
samples and the scheduler wakes it from the tick-driven delay list.

`make example` runs `examples/04_ipc`, where producer/consumer tasks exchange
samples through a queue and signal completion with a binary semaphore.

`make example` runs `examples/05_mutex`, where a high-priority waiter boosts a
low-priority mutex owner above a medium-priority CPU-bound task.

`make example` runs `examples/06_heap`, where allocation, alignment, free, and
heap statistics are visible. Select another allocator with
`make HEAP_SCHEME=1 example` or `make HEAP_SCHEME=2 example`.

`make example` runs `examples/07_timers`, where a timer service task invokes an
auto-reload callback and the callback stops itself after three firings.

`make trace > /tmp/minifreertos-trace.csv` runs `examples/08_trace` and writes
a chronological CSV event stream. Summarize it with
`python3 tools/trace_report.py /tmp/minifreertos-trace.csv`. Use
`make TRACE_TICKS=1 trace` when a lesson needs one event per tick.

Start with `docs/DESIGN.md`, `docs/P1_DESIGN.md`, `docs/P2_DESIGN.md`,
`docs/P3_DESIGN.md`, `docs/P4_DESIGN.md`, `docs/P5_DESIGN.md`, and
`docs/P6_DESIGN.md`, `docs/P7_DESIGN.md`, and `docs/P8_DESIGN.md`, then read
`docs/P8_TRACE_LAB.md` and `docs/LABS.md` before reading
`kernel/tasks.c` beside
`portable/posix/port.c`. The boundary between those two files is the most
important lesson in P0: the kernel decides *which* task runs, while the port
implements *how* CPU context changes.
