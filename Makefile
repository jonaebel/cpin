CC = clang
CFLAGS = -Wall -Wextra -g -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L $(INCLUDES)
INCLUDES = -Iinclude
SRC = src/*.c
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/cpin


all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p $(TARGET_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET_DIR)/*

run: $(TARGET)
	$(TARGET)

asan:
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SRC) -o $(TARGET_DIR)/cpin_asan

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/cpin

uninstall:
	rm -f /usr/local/bin/cpin

.PHONY: all asan valgrind clean run install uninstall
