CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?=

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/shell.c \
       $(SRC_DIR)/tokenizer.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/ast.c \
       $(SRC_DIR)/executor.c \
       $(SRC_DIR)/round_robin.c \
       $(SRC_DIR)/jobs.c \
       $(SRC_DIR)/signals.c \
       $(SRC_DIR)/terminal.c \
       $(SRC_DIR)/history.c \
       $(SRC_DIR)/expansion.c \
       $(SRC_DIR)/builtins.c \
       $(SRC_DIR)/redirection.c \
       $(SRC_DIR)/utils.c

CORE_OBJS = $(BUILD_DIR)/shell.o \
            $(BUILD_DIR)/tokenizer.o \
            $(BUILD_DIR)/parser.o \
            $(BUILD_DIR)/ast.o \
            $(BUILD_DIR)/executor.o \
            $(BUILD_DIR)/round_robin.o \
            $(BUILD_DIR)/jobs.o \
            $(BUILD_DIR)/signals.o \
            $(BUILD_DIR)/terminal.o \
            $(BUILD_DIR)/history.o \
            $(BUILD_DIR)/expansion.o \
            $(BUILD_DIR)/builtins.o \
            $(BUILD_DIR)/redirection.o \
            $(BUILD_DIR)/utils.o

OBJS = $(BUILD_DIR)/main.o $(CORE_OBJS)

TARGET = shellcraft

TEST_SRCS = $(TEST_DIR)/test_tokenizer.c \
            $(TEST_DIR)/test_parser.c \
            $(TEST_DIR)/test_expansion.c \
            $(TEST_DIR)/test_builtins.c \
            $(TEST_DIR)/test_redirection.c

TEST_BINS = $(BUILD_DIR)/test_tokenizer \
            $(BUILD_DIR)/test_parser \
            $(BUILD_DIR)/test_expansion \
            $(BUILD_DIR)/test_builtins \
            $(BUILD_DIR)/test_redirection

.PHONY: all clean debug release test self-test sanitize valgrind format install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

release: CFLAGS += -O2 -DNDEBUG
release: clean all

sanitize: CFLAGS += -fsanitize=address,undefined -g -O1
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: clean all

$(BUILD_DIR)/test_tokenizer: $(TEST_DIR)/test_tokenizer.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BUILD_DIR)/tokenizer.o $(BUILD_DIR)/utils.o $(LDFLAGS)

$(BUILD_DIR)/test_parser: $(TEST_DIR)/test_parser.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BUILD_DIR)/parser.o $(BUILD_DIR)/tokenizer.o $(BUILD_DIR)/ast.o $(BUILD_DIR)/utils.o $(LDFLAGS)

$(BUILD_DIR)/test_expansion: $(TEST_DIR)/test_expansion.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(CORE_OBJS) $(LDFLAGS)

$(BUILD_DIR)/test_builtins: $(TEST_DIR)/test_builtins.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(CORE_OBJS) $(LDFLAGS)

$(BUILD_DIR)/test_redirection: $(TEST_DIR)/test_redirection.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BUILD_DIR)/redirection.o $(BUILD_DIR)/ast.o $(BUILD_DIR)/utils.o $(LDFLAGS)

test: all $(TEST_BINS)
	@echo "================ Running Unit Tests ================"
	@./$(BUILD_DIR)/test_tokenizer
	@./$(BUILD_DIR)/test_parser
	@./$(BUILD_DIR)/test_expansion
	@./$(BUILD_DIR)/test_builtins
	@./$(BUILD_DIR)/test_redirection
	@echo "================ Running Integration Tests ================"
	@bash tests/test_pipeline.sh
	@bash tests/test_jobs.sh
	@bash tests/test_signals.sh
	@bash tests/test_history.sh
	@bash tests/test_command_substitution.sh
	@bash tests/golden_session.sh
	@echo "All tests passed successfully!"

self-test: all
	./$(TARGET) --self-test

valgrind: all
	bash scripts/valgrind_test.sh

format:
	@which clang-format >/dev/null 2>&1 && clang-format -i $(SRC_DIR)/*.c $(INC_DIR)/*.h $(TEST_DIR)/*.c || echo "clang-format not available, skipping"

install: all
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
