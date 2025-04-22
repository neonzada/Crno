CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -O2 -lm

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/*.c,%(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile
$(BUILD_DIR)/$.o: $(SRC_DIR)/$.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(BUILD_DIR)/*.o $(BIN_DIR)/*.exe

.PHONY: all clean run
