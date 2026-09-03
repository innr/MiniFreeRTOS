# P2 Design Delta: Tick, Interrupt Simulation, and Preemption

Status: **Frozen for implementation**

## Scope

P2 adds a periodic POSIX `SIGALRM` source to simulate a hardware tick interrupt.
The handler increments the kernel tick and performs a context switch through the
same port boundary used by cooperative yield.

## Port contract

`portable/posix/port.c` owns:

- `sigaction(SIGALRM, ...)` installation;
- `setitimer(ITIMER_REAL, ...)` periodic source;
- tick signal masking for critical sections;
- saving the interrupted task context from the signal handler;
- switching to the scheduler context.

The kernel does not include POSIX headers. It exposes only `vTaskTickISR()` to
the port.

## Tick behavior

At every delivered tick:

1. increment `xTaskGetTickCount()`;
2. if a task is currently running and preemption is enabled, append it to the
   tail of its Ready List;
3. switch to the scheduler context;
4. select the highest-priority ready task, using P1's round-robin rules.

This produces equal-priority time slicing even when a task never calls
`taskYIELD()`.

## Critical sections

`taskENTER_CRITICAL()` blocks `SIGALRM` with a nesting counter. Nested critical
sections are allowed. The signal remains pending while blocked and is delivered
after the outermost `taskEXIT_CRITICAL()`, so the tick is not lost from the
learner's point of view.

This is a PC teaching mechanism, not a production signal-safe scheduler. POSIX
does not guarantee that `swapcontext()` is async-signal-safe; the limitation is
explicit so the later Cortex-M port can show the hardware-safe equivalent.

## Configuration

`configUSE_PREEMPTION=1` enables the POSIX tick. P0/P1 regression binaries are
compiled with it disabled so their cooperative traces remain deterministic. The
P2 test enables it explicitly.

## Acceptance criteria

1. A CPU-bound task that never calls `taskYIELD()` is interrupted by the tick.
2. Two equal-priority CPU-bound tasks both make progress.
3. Tick count is monotonic and reaches the test target.
4. Nested/outer critical masking prevents a tick from switching the task inside
   the critical section.
5. P0 and P1 tests remain green with preemption disabled.
6. P2 test passes with warnings treated as errors.

## Non-goals

No delayed lists, task blocking, ISR-safe queue APIs, tick hooks, tickless idle,
or Cortex-M assembly. Delays and timeout semantics are P3.

