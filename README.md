# MiniFreeRTOS

MiniFreeRTOS is a from-scratch, FreeRTOS-like teaching kernel written in C. The
first port runs on a POSIX PC, letting you learn and debug kernel concepts before
moving to Cortex-M exception-based context switching.

## Current milestone: P0

Implemented:

- FreeRTOS-style task API
- task control blocks (TCBs)
- independent task stacks
- POSIX `ucontext` port layer
- cooperative context switching with `taskYIELD()`
- task return and deleted state

Not implemented yet: priority scheduling, tick, delays, queues, semaphores,
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

Start with `docs/DESIGN.md`, then read `kernel/tasks.c` beside
`portable/posix/port.c`. The boundary between those two files is the most
important lesson in P0: the kernel decides *which* task runs, while the port
implements *how* CPU context changes.
