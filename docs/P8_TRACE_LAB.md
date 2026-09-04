# P8 Lab: Read the Kernel Timeline

## Goal

This lesson turns MiniFreeRTOS from a black box into a timeline. You will
observe the same state transitions that the scheduler and IPC code use
internally:

    ready -> running -> blocked -> wake -> ready -> deleted

The trace is diagnostic only. It does not replace the scheduler and it cannot
make an operation succeed.

## Prerequisites

Run from the repository root:

~~~sh
make test
make example
~~~

The commands use the default heap_4 backend. The allocator lesson below
repeats the run with heap_1 and heap_2.

## Lab 1: Scheduler

Run examples/01_task_create and examples/02_preemption.

Questions:

1. Which event identifies the first selected task?
2. In the cooperative lesson, what causes the next task-switch?
3. In the preemptive lesson, what changes when neither task calls
   taskYIELD()?

Expected observation: a task-switch event names the selected task and its
priority in value. P2's tick preemption inserts the running task back into a
ready list before selecting another task.

## Lab 2: Time and delays

Run examples/03_delay.

Questions:

1. Which task-blocked event has value=eTaskWaitNone and a null object?
2. Which tick is attached to the matching task-wake event?
3. How does vTaskDelayUntil() differ from repeatedly delaying from “now”?

Expected observation: the delay event has no wait object, while an IPC timeout
has the IPC handle as its object. Both carry the current tick in the event.

## Lab 3: Queues and semaphores

Run examples/04_ipc.

Questions:

1. Which task performs each queue-send and queue-receive?
2. Which blocked task is woken by the queue operation?
3. How do semaphore-give and semaphore-take expose the completion signal?

Expected observation: queue events report the message count after the
operation; a successful operation is followed by an object-driven wake when a
waiter exists.

## Lab 4: Priority inversion

Run examples/05_mutex.

Questions:

1. Which waiter causes mutex-inherit?
2. Which owner is named by the event's task field?
3. What priority is restored by mutex-release?

Expected observation: the low-priority owner is raised to the high-priority
waiter's effective priority. The release event identifies the owner and the
next waiter before the scheduler yields.

## Lab 5: Memory

Run the same lesson with all three allocators:

~~~sh
make HEAP_SCHEME=1 example
make HEAP_SCHEME=2 example
make HEAP_SCHEME=4 example
~~~

The trace does not instrument allocator internals in P8. Compare the printed
free-byte values instead, then explain why allocation and coalescing are
resource-management events rather than scheduling events. This boundary keeps
the trace small and makes it clear which subsystem owns each observation.

## Lab 6: Software timers

Run examples/07_timers.

Questions:

1. When is timer-command-queued recorded?
2. Which task applies the command?
3. Which task owns the timer-callback event?

Expected observation: commands are queued by the application (or by a
callback), applied by the TIMER service task, and callbacks execute in that
service-task context rather than in the tick signal handler.

## Lab 7: Integrated CSV trace

Produce and inspect the complete trace:

~~~sh
make trace > /tmp/minifreertos-trace.csv
python3 tools/trace_report.py /tmp/minifreertos-trace.csv
~~~

The CSV contains the event sequence, tick, event name, task names, object
pointer, and event-specific value. Pointer values are process-local labels;
use the event type and task name when comparing two runs.

To include every tick, use:

~~~sh
make TRACE_TICKS=1 trace > /tmp/minifreertos-ticks.csv
python3 tools/trace_report.py /tmp/minifreertos-ticks.csv
~~~

The fixed ring keeps only the newest configTRACE_BUFFER_LENGTH events. The
report's dropped count tells you whether the timeline was longer than the
configured buffer.

## Extension exercises

1. Change the high task's priority and predict the next task-switch.
2. Change the timer period and identify the command and callback tick delta.
3. Increase or decrease configTRACE_BUFFER_LENGTH and explain the dropped
   count.
4. Compile with -DconfigTRACE_ENABLED=0U and verify that the application
   output is unchanged while the trace stream contains no events.
