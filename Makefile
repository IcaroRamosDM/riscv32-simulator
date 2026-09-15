# RISC-V Studio: native C core and command-line build.
# Each tests/**/test_*.c file defines one independent test executable.
# Shared test helpers belong in tests/support/. The application entry is src/main.c.

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CC := gcc
MODE ?= debug
CPPFLAGS ?=
CFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
ARGS ?=

ifeq ($(MODE),debug)
MODE_CFLAGS := -O0 -g3
MODE_LDFLAGS :=
else ifeq ($(MODE),release)
MODE_CFLAGS := -O2 -g
MODE_LDFLAGS :=
else ifeq ($(MODE),sanitize)
MODE_CFLAGS := -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
MODE_LDFLAGS := -fsanitize=address,undefined
else
$(error MODE must be debug, release, or sanitize)
endif

override BUILD_DIR := build/$(MODE)
APP_BINARY := $(BUILD_DIR)/riscv32-studio
SETTINGS_FILE := $(BUILD_DIR)/.settings
COMPILE_FLAGS := -std=c17 -Wall -Wextra -Wpedantic -Werror $(MODE_CFLAGS) $(CFLAGS)
PREPROCESS_FLAGS := -Iinclude $(CPPFLAGS)
LINK_FLAGS := $(MODE_LDFLAGS) $(LDFLAGS)

APP_SOURCE := $(wildcard src/main.c)
CORE_SOURCES := $(sort $(shell find src -type f -name '*.c' ! -path 'src/main.c'))
TEST_SOURCES := $(sort $(shell find tests -type f -name 'test_*.c' ! -path 'tests/support/*'))
SUPPORT_SOURCES := $(sort $(shell find tests -type f -path 'tests/support/*.c'))

CORE_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(CORE_SOURCES))
APP_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(APP_SOURCE))
TEST_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(TEST_SOURCES))
SUPPORT_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(SUPPORT_SOURCES))
TEST_BINARIES := $(patsubst tests/%.c,$(BUILD_DIR)/tests/%,$(TEST_SOURCES))
ALL_OBJECTS := $(CORE_OBJECTS) $(APP_OBJECTS) $(TEST_OBJECTS) $(SUPPORT_OBJECTS)

# A content-checked settings file also detects changed flags and removed sources.
# Exporting values avoids inserting their contents into shell quotations.
export BUILD_SETTING_CC := $(CC)
export BUILD_SETTING_CPPFLAGS := $(PREPROCESS_FLAGS)
export BUILD_SETTING_CFLAGS := $(COMPILE_FLAGS)
export BUILD_SETTING_LDFLAGS := $(LINK_FLAGS)
export BUILD_SETTING_LDLIBS := $(LDLIBS)
export BUILD_SETTING_SOURCES := $(CORE_SOURCES) $(APP_SOURCE) $(TEST_SOURCES) $(SUPPORT_SOURCES)

.PHONY: all app run test debug release sanitize check clean help FORCE
.SECONDARY: $(ALL_OBJECTS)

all: $(TEST_BINARIES) $(if $(APP_SOURCE),$(APP_BINARY))

app: $(APP_BINARY)

run: app
	"$(APP_BINARY)" $(ARGS)

test: $(TEST_BINARIES)
	@test -n "$(TEST_BINARIES)" || { printf '%s\n' 'No test_*.c files found under tests/.' >&2; exit 1; }
	@set -eu; for test_binary in $(TEST_BINARIES); do \
		printf 'Running %s\n' "$$test_binary"; \
		"$$test_binary"; \
	done

debug:
	+$(MAKE) MODE=debug all

release:
	+$(MAKE) MODE=release all

sanitize:
	+$(MAKE) MODE=sanitize test

check:
	+$(MAKE) MODE=debug test
	+$(MAKE) MODE=sanitize test

$(BUILD_DIR):
	mkdir -p "$@"

$(SETTINGS_FILE): FORCE | $(BUILD_DIR)
	@printf '%s\n' "$$BUILD_SETTING_CC" "$$BUILD_SETTING_CPPFLAGS" "$$BUILD_SETTING_CFLAGS" "$$BUILD_SETTING_LDFLAGS" "$$BUILD_SETTING_LDLIBS" "$$BUILD_SETTING_SOURCES" > "$@.tmp"
	@if cmp -s "$@.tmp" "$@"; then rm -f -- "$@.tmp"; else mv -f -- "$@.tmp" "$@"; fi

$(BUILD_DIR)/obj/tests/%.o: tests/%.c $(SETTINGS_FILE) Makefile
	@mkdir -p "$(@D)"
	$(CC) $(PREPROCESS_FLAGS) $(COMPILE_FLAGS) -UNDEBUG -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/obj/%.o: %.c $(SETTINGS_FILE) Makefile
	@mkdir -p "$(@D)"
	$(CC) $(PREPROCESS_FLAGS) $(COMPILE_FLAGS) -MMD -MP -c "$<" -o "$@"

ifneq ($(strip $(TEST_BINARIES)),)
$(TEST_BINARIES): $(BUILD_DIR)/tests/%: $(BUILD_DIR)/obj/tests/%.o $(CORE_OBJECTS) $(SUPPORT_OBJECTS) $(SETTINGS_FILE) Makefile
	@mkdir -p "$(@D)"
	$(CC) $(COMPILE_FLAGS) $(LINK_FLAGS) $(filter %.o,$^) $(LDLIBS) -o "$@"
endif

ifneq ($(APP_SOURCE),)
$(APP_BINARY): $(APP_OBJECTS) $(CORE_OBJECTS) $(SETTINGS_FILE) Makefile
	@mkdir -p "$(@D)"
	$(CC) $(COMPILE_FLAGS) $(LINK_FLAGS) $(filter %.o,$^) $(LDLIBS) -o "$@"
else
.PHONY: $(APP_BINARY)
$(APP_BINARY):
	@printf '%s\n' 'Application entry point src/main.c does not exist yet. Use make test.' >&2
	@exit 1
endif

clean:
	rm -rf -- build

help:
	@printf '%s\n' \
		'make                    Build tests and the application when src/main.c exists.' \
		'make test               Build and run all independent C tests.' \
		'make app                Build the application from src/main.c and the core.' \
		'make run ARGS="..."     Build and run the application with optional arguments.' \
		'make debug              Build with debugging information and -O0.' \
		'make release            Build with -O2 and debugging information.' \
		'make MODE=release test  Run tests in the optimized configuration.' \
		'make sanitize           Run tests with AddressSanitizer and UndefinedBehaviorSanitizer.' \
		'make check              Run debug and sanitizer tests.' \
		'make clean              Remove only the build/ directory.' \
		'make help               Show this help.' \
		'' \
		'New src/**/*.c and tests/**/test_*.c files are discovered automatically.' \
		'Header dependencies and changes to compiler flags trigger rebuilding.' \
		'Artifacts are separated under build/debug, build/release, and build/sanitize.' \
		'Assertions remain enabled in test files, including release builds.'

-include $(ALL_OBJECTS:.o=.d)
