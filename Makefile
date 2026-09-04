CC ?= cc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_XOPEN_SOURCE=700 -Iinclude -Ikernel -Iportable/posix

KERNEL_SOURCES := kernel/tasks.c portable/posix/port.c
BUILD_DIR := build
NONPREEMPTIVE_FLAGS := -DconfigUSE_PREEMPTION=0
PREEMPTIVE_FLAGS := -DconfigUSE_PREEMPTION=1

.PHONY: all test example clean
all: test example

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_p0: $(KERNEL_SOURCES) tests/test_p0_tasks.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p1: $(KERNEL_SOURCES) tests/test_p1_scheduler.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p2: $(KERNEL_SOURCES) tests/test_p2_preemption.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p3_delay: $(KERNEL_SOURCES) tests/test_p3_delay.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/test_p3_wrap: $(KERNEL_SOURCES) tests/test_p3_wrap.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_01: $(KERNEL_SOURCES) examples/01_task_create/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NONPREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_02: $(KERNEL_SOURCES) examples/02_preemption/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

$(BUILD_DIR)/example_03: $(KERNEL_SOURCES) examples/03_delay/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PREEMPTIVE_FLAGS) $^ -o $@

test: $(BUILD_DIR)/test_p0 $(BUILD_DIR)/test_p1 $(BUILD_DIR)/test_p2 \
	$(BUILD_DIR)/test_p3_delay $(BUILD_DIR)/test_p3_wrap
	./$(BUILD_DIR)/test_p0
	./$(BUILD_DIR)/test_p1
	./$(BUILD_DIR)/test_p2
	./$(BUILD_DIR)/test_p3_delay
	./$(BUILD_DIR)/test_p3_wrap

example: $(BUILD_DIR)/example_01 $(BUILD_DIR)/example_02 $(BUILD_DIR)/example_03
	./$(BUILD_DIR)/example_01
	./$(BUILD_DIR)/example_02
	./$(BUILD_DIR)/example_03

clean:
	rm -rf $(BUILD_DIR)
