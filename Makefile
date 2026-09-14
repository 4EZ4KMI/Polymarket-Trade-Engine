# Polymarket HFT trader — build (C core + engine)
CC       ?= cc
CFLAGS   ?= -O2
CFLAGS   += -std=c11 -Wall -Wextra -Wno-unused-parameter -pthread
CPPFLAGS += -Iinclude

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
CPPFLAGS += -DPMT_USE_EPOLL=1 -D_GNU_SOURCE
LDLIBS   += -lpthread -lm
else
CPPFLAGS += -DPMT_USE_KQUEUE=1
LDLIBS   += -lpthread -lm
endif

BUILD   := build
LIB     := $(BUILD)/libpmtcore.a
ENGINE  := $(BUILD)/bin/pmt_engine
REPLAY  := $(BUILD)/bin/pmt_replay

CORE_SRC := $(filter-out src/main.c src/replay/pt_replay.c,$(wildcard src/core/*.c src/util/*.c src/orderbook/*.c \
                    src/features/*.c src/strategies/*.c src/execution/*.c src/risk/*.c \
                    src/portfolio/*.c src/storage/*.c src/analytics/*.c src/net/*.c \
                    src/telemetry/*.c src/backtest/*.c))
CORE_OBJ := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(CORE_SRC))

TEST_SRCS := $(filter-out tests/test_harness.c,$(wildcard tests/test_*.c))
TEST_BINS := $(patsubst tests/test_%.c,$(BUILD)/bin/test_%,$(TEST_SRCS))

.PHONY: all core engine replay tests run_tests clean format

all: core engine replay tests

$(BUILD)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIB): $(CORE_OBJ)
	@mkdir -p $(BUILD)
	ar rcs $(LIB) $(CORE_OBJ)

core: $(LIB)

$(ENGINE): src/main.c $(LIB)
	@mkdir -p $(BUILD)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) src/main.c $(LIB) -o $@ $(LDLIBS)
	@echo "built engine: $@"

engine: $(ENGINE)

$(REPLAY): src/replay/pt_replay.c $(LIB)
	@mkdir -p $(BUILD)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) src/replay/pt_replay.c $(LIB) -o $@ $(LDLIBS)
	@echo "built replay: $@"

replay: $(REPLAY)


$(BUILD)/bin/test_%: tests/test_%.c tests/test_harness.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< tests/test_harness.c $(LIB) -o $@ $(LDLIBS)
	@echo "built $@"

tests: $(TEST_BINS)

run_tests: tests
	@rm -rf $(BUILD)/tmp && mkdir -p $(BUILD)/tmp
	@fail=0; \
	for t in $(TEST_BINS); do \
		$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILD)

format:
	clang-format -i include src tests 2>/dev/null || true