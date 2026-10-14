CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS ?=
TARGET = ls
SOURCES = src/main.c src/options.c src/entries.c src/sort.c src/format.c src/list.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
src/%.o: src/%.c include/ls.h include/options.h include/entries.h include/sort.h include/format.h include/list.h
	$(CC) $(CFLAGS) -c $< -o $@
test: $(TARGET)
	@tmp=$$(mktemp -d); trap 'rm -rf "$$tmp"' EXIT; \
	 touch "$$tmp/z" "$$tmp/a"; mkdir "$$tmp/dir"; ln -s a "$$tmp/link"; \
	 ./$(TARGET) -laF "$$tmp" >/dev/null; ./$(TARGET) -R "$$tmp" >/dev/null
clean:
	rm -f $(TARGET) $(OBJECTS)
