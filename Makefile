CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?=
PYTHON ?= python3
TARGET = ls
SOURCES = src/main.c src/options.c src/entries.c src/sort.c src/format.c src/list.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test sanitize
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)
src/%.o: src/%.c include/ls.h include/options.h include/entries.h include/sort.h include/format.h include/list.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
test: $(TARGET)
	$(PYTHON) tests/test_ls.py ./$(TARGET)
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	rm -f $(TARGET) $(OBJECTS)
