CC ?= cc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_XOPEN_SOURCE=700 -Iinclude -Ikernel -Iportable/posix

HEAP_SCHEME ?= 4
ifeq ($(filter 1 2 4,$(HEAP_SCHEME)),)
$(error HEAP_SCHEME must be 1, 2, or 4)
endif

HEAP_SOURCE := kernel/heap_$(HEAP_SCHEME).c
HEAP_DEFINE := -DconfigHEAP_SCHEME=$(HEAP_SCHEME)
KERNEL_SOURCES := kernel/tasks.c kernel/queue.c kernel/timers.c kernel/trace.c $(HEAP_SOURCE) \
	portable/posix/port.c
BUILD_DIR := build/heap_$(HEAP_SCHEME)
NONPREEMPTIVE_FLAGS := -DconfigUSE_PREEMPTION=0
PREEMPTIVE_FLAGS := -DconfigUSE_PREEMPTION=1
TRACE_FLAGS :=
ifeq ($(TRACE_TICKS),1)
TRACE_FLAGS += -DconfigTRACE_INCLUDE_TICKS=1U
endif

.PHONY: all test test_tools example trace clean
all: test example

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_p0: $(KERNEL_SOURCES) tests/test_p0_tasks.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p1: $(KERNEL_SOURCES) tests/test_p1_scheduler.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p2: $(KERNEL_SOURCES) tests/test_p2_preemption.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p3_delay: $(KERNEL_SOURCES) tests/test_p3_delay.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p3_wrap: $(KERNEL_SOURCES) tests/test_p3_wrap.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p4_queue: $(KERNEL_SOURCES) tests/test_p4_queue.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p4_semaphore: $(KERNEL_SOURCES) tests/test_p4_semaphore.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p5_mutex: $(KERNEL_SOURCES) tests/test_p5_mutex.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p7_timers: $(KERNEL_SOURCES) tests/test_p7_timers.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p8_trace: $(KERNEL_SOURCES) tests/test_p8_trace.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p9_port_frame: portable/cortex_m/port_frame.c \
	tests/test_p9_port_frame.c | $(BUILD_DIR)
	$(CC) -Iinclude -Iportable/cortex_m $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_p6_heap_1: kernel/heap_1.c tests/test_p6_heap.c \
	tests/test_heap_support.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DconfigHEAP_SCHEME=1 \
		-DTEST_HEAP_SCHEME=1 -DconfigTOTAL_HEAP_SIZE=4096U $^ -o $@

$(BUILD_DIR)/test_p6_heap_2: kernel/heap_2.c tests/test_p6_heap.c \
	tests/test_heap_support.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DconfigHEAP_SCHEME=2 \
		-DTEST_HEAP_SCHEME=2 -DconfigTOTAL_HEAP_SIZE=4096U $^ -o $@

$(BUILD_DIR)/test_p6_heap_4: kernel/heap_4.c tests/test_p6_heap.c \
	tests/test_heap_support.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DconfigHEAP_SCHEME=4 \
		-DTEST_HEAP_SCHEME=4 -DconfigTOTAL_HEAP_SIZE=4096U $^ -o $@

$(BUILD_DIR)/example_01: $(KERNEL_SOURCES) examples/01_task_create/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_02: $(KERNEL_SOURCES) examples/02_preemption/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_03: $(KERNEL_SOURCES) examples/03_delay/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_04: $(KERNEL_SOURCES) examples/04_ipc/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_05: $(KERNEL_SOURCES) examples/05_mutex/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_06: $(KERNEL_SOURCES) examples/06_heap/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_07: $(KERNEL_SOURCES) examples/07_timers/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_08: $(KERNEL_SOURCES) examples/08_trace/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(HEAP_DEFINE) $(CFLAGS) $(PREEMPTIVE_FLAGS) $(TRACE_FLAGS) $^ -o $@

test_tools:
	python3 tests/test_trace_report.py

test: $(BUILD_DIR)/test_p0 $(BUILD_DIR)/test_p1 $(BUILD_DIR)/test_p2 \
	$(BUILD_DIR)/test_p3_delay $(BUILD_DIR)/test_p3_wrap \
	$(BUILD_DIR)/test_p4_queue $(BUILD_DIR)/test_p4_semaphore \
	$(BUILD_DIR)/test_p5_mutex $(BUILD_DIR)/test_p7_timers \
	$(BUILD_DIR)/test_p8_trace \
	$(BUILD_DIR)/test_p6_heap_1 \
	$(BUILD_DIR)/test_p6_heap_2 $(BUILD_DIR)/test_p6_heap_4 \
	$(BUILD_DIR)/test_p9_port_frame test_tools
	./$(BUILD_DIR)/test_p0
	./$(BUILD_DIR)/test_p1
	./$(BUILD_DIR)/test_p2
	./$(BUILD_DIR)/test_p3_delay
	./$(BUILD_DIR)/test_p3_wrap
	./$(BUILD_DIR)/test_p4_queue
	./$(BUILD_DIR)/test_p4_semaphore
	./$(BUILD_DIR)/test_p5_mutex
	./$(BUILD_DIR)/test_p7_timers
	./$(BUILD_DIR)/test_p8_trace
	./$(BUILD_DIR)/test_p9_port_frame
	./$(BUILD_DIR)/test_p6_heap_1
	./$(BUILD_DIR)/test_p6_heap_2
	./$(BUILD_DIR)/test_p6_heap_4

example: $(BUILD_DIR)/example_01 $(BUILD_DIR)/example_02 $(BUILD_DIR)/example_03 \
	$(BUILD_DIR)/example_04 $(BUILD_DIR)/example_05 $(BUILD_DIR)/example_06 \
	$(BUILD_DIR)/example_07 $(BUILD_DIR)/example_08
	./$(BUILD_DIR)/example_01
	./$(BUILD_DIR)/example_02
	./$(BUILD_DIR)/example_03
	./$(BUILD_DIR)/example_04
	./$(BUILD_DIR)/example_05
	./$(BUILD_DIR)/example_06
	./$(BUILD_DIR)/example_07
	./$(BUILD_DIR)/example_08

trace: $(BUILD_DIR)/example_08
	./$(BUILD_DIR)/example_08

clean:
	rm -rf build
