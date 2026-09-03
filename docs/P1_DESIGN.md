# P1 Design Delta: Priority Scheduler, Round Robin, and Idle Task

Status: **Frozen for implementation**

## Scope

P1 extends the cooperative P0 scheduler without adding wall-clock time,
interrupts, delays, or IPC. A task gives up its cooperative time slice by calling
`taskYIELD()`.

## Selection algorithm

1. Start scanning at `next_task_index`.
2. Among all `eReady` tasks, find the greatest numeric priority.
3. Select the first task with that priority in the circular scan order.
4. Advance `next_task_index` immediately after the selected task.

This gives strict priority selection and round-robin behavior for equal priority
tasks. A lower-priority task cannot run while any higher-priority task is ready.

## Idle task

The kernel creates one internal task at priority zero when the scheduler starts.
It has a normal TCB and stack, but is not counted against the application task
limit. The idle function only yields. If every application task is `eDeleted`,
idle calls `vTaskEndScheduler()` so deterministic PC tests terminate.

## Priority API

P1 adds:

```c
void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority);
UBaseType_t uxTaskPriorityGet(TaskHandle_t task);
```

Passing `NULL` means the current task. Changing a priority takes effect at the
next scheduling point; P2 will add immediate rescheduling from tick/critical
section exits.

## State rules

- Scheduler dispatch: `eReady -> eRunning`.
- `taskYIELD()`: `eRunning -> eReady`.
- Task function return: `eRunning -> eDeleted`.
- Idle remains `eReady` between its short executions.

## Acceptance criteria

1. A ready priority-3 task always runs before a ready priority-1 task.
2. Two priority-3 tasks alternate at each `taskYIELD()`.
3. Lower priority work runs only after higher priority tasks finish.
4. Idle runs as the fallback and stops the deterministic test when all user tasks
   return.
5. Priority getter/setter operate on the current task and task handles.
6. Existing P0 context tests remain green.

## Non-goals

No asynchronous preemption, tick counter, time slicing driven by a timer,
blocking states, delayed lists, or ISR APIs. Those belong to P2/P3.

