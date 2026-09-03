CC ?= cc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_XOPEN_SOURCE=700 -Iinclude -Ikernel -Iportable/posix

KERNEL_SOURCES := kernel/tasks.c portable/posix/port.c
BUILD_DIR := build

.PHONY: all test example clean
all: test example

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_p0: $(KERNEL_SOURCES) tests/test_p0_tasks.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/example_01: $(KERNEL_SOURCES) examples/01_task_create/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test: $(BUILD_DIR)/test_p0
	./$<

example: $(BUILD_DIR)/example_01
	./$<

clean:
	rm -rf $(BUILD_DIR)

