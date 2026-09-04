# MiniFreeRTOS Teaching Labs

Each phase has one runnable lesson and one question to answer. Start with the
concept, inspect the small implementation, then change one parameter and
rerun the lesson.

| Phase | Lesson | Main idea |
|---|---|---|
| P0 | examples/01_task_create | Independent stacks and cooperative context switching |
| P1 | examples/02_preemption | Priority selection and equal-priority time slicing |
| P2 | examples/02_preemption | POSIX tick simulation and preemption |
| P3 | examples/03_delay | Delayed tasks, wakeups, and wrap-safe deadlines |
| P4 | examples/04_ipc | Copy-by-value queues and blocking waiters |
| P5 | examples/05_mutex | Ownership, priority inheritance, and disinheritance |
| P6 | examples/06_heap | Allocation, alignment, freeing, and heap statistics |
| P7 | examples/07_timers | Timer service task and asynchronous commands |
| P8 | examples/08_trace | Explain the whole run from a kernel event timeline |
| P9 | examples/09_cortex_m | SVC launch, SysTick, PendSV, and a freestanding board lesson |

## P9: Cortex-M exception path

The P9 lesson is built separately because it targets ARM rather than the host:

~~~sh
make -f Makefile.cortex_m all
make -f Makefile.cortex_m inspect
make -f Makefile.cortex_m qemu
~~~

Questions:

1. Which values are restored by SVC before the first task executes?
2. Why does SysTick request PendSV instead of saving registers itself?
3. Where is the old PSP saved, and how does `vTaskSwitchContext()` remain
   independent of the register frame?

Expected observation: the consumer starts through SVC, a delayed producer is
woken by SysTick, and PendSV preserves each task's independent stack while the
queue transfer completes. The QEMU board adapter uses semihosting only for the
lesson console and exit status; it is not part of the kernel API.

For the detailed questions and expected observations, read
docs/P8_TRACE_LAB.md. The P8 trace report is optional for earlier lessons,
but it is the fastest way to see why a task changed state.

## Suggested study loop

1. Run the lesson with make example or its individual Makefile target.
2. Read the matching design delta under docs/.
3. Locate the state transition in kernel/ and the context operation in
   portable/posix/.
4. Make one controlled change.
5. Run the regression tests and compare the trace.
