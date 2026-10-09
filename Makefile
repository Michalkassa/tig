CC       ?= cc
WARN     := -Wall -Wextra -Wpedantic
STD      := -std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS   ?= -O2 $(WARN)
LDFLAGS  ?=
# Separate build dirs per flavour so sanitizer and normal objects never mix.
BUILD    ?= build
BIN      ?= tig

SRC      := $(wildcard src/*.c)
LIB_OBJ  := $(patsubst src/%.c,$(BUILD)/%.o,$(filter-out src/main.c,$(SRC)))
TEST_SRC := $(wildcard tests/unit/*.c)
TEST_OBJ := $(patsubst tests/unit/%.c,$(BUILD)/tests/%.o,$(TEST_SRC))
HEADERS  := $(wildcard src/*.h) $(wildcard tests/unit/*.h)

$(BIN): $(BUILD)/main.o $(LIB_OBJ)
	$(CC) $(STD) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD)/unit_tests: $(TEST_OBJ) $(LIB_OBJ)
	$(CC) $(STD) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(STD) $(CFLAGS) -c -o $@ $<

$(BUILD)/tests/%.o: tests/unit/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(STD) $(CFLAGS) -c -o $@ $<

# test: unit tests, then end-to-end tests against the built binary.
test: unit e2e

unit: $(BUILD)/unit_tests
	./$(BUILD)/unit_tests

e2e: $(BIN)
	@set -e; for t in tests/e2e/*.sh; do \
		[ "$$t" = tests/e2e/lib.sh ] || TIG="$(CURDIR)/$(BIN)" sh "$$t"; \
	done

# check: warnings as errors + address/undefined-behaviour sanitizers.
SAN := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
check:
	$(MAKE) test BUILD=build/san BIN=build/san/tig \
		CFLAGS="-O1 -g $(WARN) -Werror $(SAN)" LDFLAGS="$(SAN)"

# hooks: run the test suite before every git commit.
hooks:
	git config core.hooksPath .githooks

clean:
	rm -rf build tig

.PHONY: test unit e2e check hooks clean
