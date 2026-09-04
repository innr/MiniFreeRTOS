# MiniFreeRTOS

MiniFreeRTOS is a from-scratch, FreeRTOS-like teaching kernel written in C. The
first port runs on a POSIX PC, letting you learn and debug kernel concepts before
moving to Cortex-M exception-based context switching.

## Current milestone: P3

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

Not implemented yet: queues, semaphores,
mutexes, heap variants, software timers, or interrupts.

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

Start with `docs/DESIGN.md`, `docs/P1_DESIGN.md`, `docs/P2_DESIGN.md`, and
`docs/P3_DESIGN.md`, then read `kernel/tasks.c` beside
`portable/posix/port.c`. The boundary between those two files is the most
important lesson in P0: the kernel decides *which* task runs, while the port
implements *how* CPU context changes.
