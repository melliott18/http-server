CC = clang
AR ?= ar
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -g
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -pthread
LDLIBS += -pthread
PYTHON ?= python3
BUILD_DIR ?= build
REPORT_DIR ?= test-results

SUPPORT_SOURCES = buffered_socket connection fdwrapper hashtable list listener_socket queue request response rwlock
SUPPORT_OBJECTS = $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(SUPPORT_SOURCES)))
OBJECTS = $(BUILD_DIR)/httpserver.o $(SUPPORT_OBJECTS)
LIBRARY = $(BUILD_DIR)/libhttp-support.a
BINARY = $(BUILD_DIR)/httpserver

.PHONY: all clean test test-workloads test-regression
all: $(BINARY)

$(BINARY): $(BUILD_DIR)/httpserver.o $(LIBRARY)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(LIBRARY): $(SUPPORT_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

test: test-regression test-workloads

test-regression: $(BINARY)
	$(PYTHON) tests/test_httpserver.py --binary $(abspath $(BINARY)) --report-dir $(abspath $(REPORT_DIR))/regression

test-workloads: $(BINARY)
	$(PYTHON) tests/run_workloads.py --binary $(abspath $(BINARY)) --report-dir $(abspath $(REPORT_DIR))/workloads

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)
