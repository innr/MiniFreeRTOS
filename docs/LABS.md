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
