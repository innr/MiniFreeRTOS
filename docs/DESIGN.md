# MiniFreeRTOS Design Document v1.0 (Frozen)

Status: **Frozen for implementation**  
Target: POSIX PC first, Cortex-M later  
Language: C11

## 1. Purpose

MiniFreeRTOS is a from-scratch, FreeRTOS-like teaching kernel. Its public names,
concepts, and examples resemble FreeRTOS so knowledge transfers to the real
system, while its implementation remains deliberately small and readable.

It is not ABI-compatible, safety-certified, or intended for products.

## 2. Learning goals

The project must let a learner inspect, modify, test, and explain:

1. how a task's CPU context and stack are represented;
2. how priorities and ready lists select the next task;
3. how tick interrupts cause delay wakeups and preemption;
4. why queues, semaphores, and mutexes block rather than spin;
5. how priority inheritance limits priority inversion;
6. how deterministic heap allocators work;
7. why software timers use a service task;
8. which kernel pieces are portable and which are CPU-specific.

## 3. Architectural boundaries

| Layer | Responsibility | May depend on |
|---|---|---|
| `include/` | FreeRTOS-style public API | configuration only |
| `kernel/` | scheduler, lists, IPC, timers, heap | public types + port contract |
| `portable/posix/` | PC context switch and tick source | POSIX APIs |
| `portable/cortex_m/` | future PendSV/SysTick/SVC port | CMSIS/core registers |
| `examples/` | one concept per runnable lesson | public API only |
| `tests/` | deterministic behavioral verification | public API; internals only for white-box tests |

No POSIX type or function may appear in `kernel/` public interfaces.

## 4. Public API target

- Tasks: `xTaskCreate`, `vTaskStartScheduler`, `vTaskDelay`,
  `vTaskDelayUntil`, `taskYIELD`, `vTaskPrioritySet`, `uxTaskPriorityGet`
- Queues: `xQueueCreate`, `xQueueSend`, `xQueueReceive`
- Semaphores: binary, counting, mutex, recursive mutex
- Timers: `xTimerCreate`, `xTimerStart`, `xTimerStop`
- Memory: `pvPortMalloc`, `vPortFree`

APIs will be introduced only in the phase that implements their semantics.

## 5. Task model

Each TCB owns a port context, stack, name, base/current priority, state, wake
tick, event-list linkage, and ready-list linkage. States are:

`eReady`, `eRunning`, `eBlocked`, `eSuspended`, and `eDeleted`.

Returning from a task function is treated as task deletion. A deleted task can
never become ready again.

## 6. Scheduler model

- Priorities range from `0` through `configMAX_PRIORITIES - 1`.
- Highest numeric priority wins.
- Equal-priority tasks use round-robin time slicing.
- Blocking immediately offers the CPU to another ready task.
- The idle task is always ready once introduced in P1.
- Tick arithmetic uses unsigned wrap-safe comparisons.

P0 uses explicit cooperative `taskYIELD()` so context mechanics remain visible.
P2 adds asynchronous POSIX tick preemption and critical sections.

## 7. Synchronization model

Queues are the base IPC primitive. Semaphores and mutexes share queue machinery
where that improves clarity. Mutexes track ownership and recursively recompute
inherited priority across every held mutex when waiters change.

ISR-safe `...FromISR` APIs are deferred until the preemptive port exists.

## 8. Time and timers

P2 provides `configTICK_RATE_HZ`. Delayed tasks are ordered by wake time. Tick
wrap uses two delayed lists, following the same high-level idea as FreeRTOS.
Software timer callbacks execute in a dedicated timer service task, never in the
tick handler.

## 9. Memory

The curriculum implements three allocators separately:

- `heap_1`: allocate only;
- `heap_2`: free without coalescing;
- `heap_4`: ordered free list with adjacent-block coalescing.

Only one allocator is linked at a time. Application task stacks use the selected
allocator after the memory phase; early phases use the host allocator explicitly.

## 10. Error and diagnostic policy

- Programmer contract failures call `configASSERT`.
- Runtime operations use FreeRTOS-style success/failure values.
- Optional trace hooks report task transitions without changing scheduling.
- Tests must not depend on wall-clock timing unless specifically testing the port.

## 11. Directory plan

```text
MiniFreeRTOS/
├── include/
├── kernel/
├── portable/posix/
├── portable/cortex_m/       # added in P9
├── examples/01_task_create/
├── tests/
└── docs/
```

## 12. Delivery phases

| Phase | Deliverable | Exit test |
|---|---|---|
| P0 | TCB, create, start, yield, task return | two stacks retain independent local state |
| P1 | ready lists, priorities, idle, time slice | priority and round-robin tests |
| P2 | periodic tick, preemption, critical sections | CPU-bound task cannot starve peer |
| P3 | delay and timeout lists | wake order and tick-wrap tests |
| P4 | queues and semaphores | blocking, wakeup, timeout tests |
| P5 | mutexes and priority inheritance | bounded inversion test |
| P6 | heap_1/2/4 | exhaustion, fragmentation, coalescing tests |
| P7 | timer service task | one-shot, periodic, command tests |
| P8 | trace and teaching labs | all PC lessons reproducible |
| P9 | Cortex-M port | QEMU/board context and tick tests |

## 13. P0 acceptance criteria

1. Public code uses FreeRTOS-style task names and types.
2. Two tasks have independent stacks and resume after `taskYIELD()`.
3. Equal tasks alternate deterministically in creation order.
4. A returning task enters `eDeleted` and is never scheduled again.
5. Scheduler exits cleanly after all teaching tasks return.
6. `make test` and `make example` pass with warnings treated as errors.

## 14. Explicit non-goals for P0

No priority scheduling, tick, delay, IPC, dynamic kernel allocator, interrupts,
SMP, or Cortex-M assembly. These belong to later, independently reviewed phases.

