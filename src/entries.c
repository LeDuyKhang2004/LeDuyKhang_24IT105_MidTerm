#include "entries.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xreallocarray(void *ptr, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) {
        fputs("ls: allocation size overflow\n", stderr);
        exit(2);
    }
    void *result = realloc(ptr, count * size);
    if (result == NULL) {
        fputs("ls: out of memory\n", stderr);
        exit(2);
    }
    return result;
}

char *xstrdup(const char *text) {
    size_t size = strlen(text) + 1;
    char *copy = xreallocarray(NULL, size, 1);
    memcpy(copy, text, size);
    return copy;
}

char *join_path(const char *directory, const char *name) {
    size_t a = strlen(directory), b = strlen(name);
    if (b > SIZE_MAX - 2 || a > SIZE_MAX - b - 2) {
        fputs("ls: pathname too long\n", stderr);
        exit(2);
    }
    char *path = xreallocarray(NULL, a + b + 2, 1);
    memcpy(path, directory, a);
    if (a > 0 && directory[a - 1] != '/') path[a++] = '/';
    memcpy(path + a, name, b + 1);
    return path;
}

void report_error(RunState *state, const char *path) {
    int saved_errno = errno;
    fprintf(stderr, "ls: %s: %s\n", path, strerror(saved_errno));
    state->status = 1;
}

bool entry_load(Entry *entry, const char *path, const char *name,
                bool operand, const Options *options, RunState *state) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        report_error(state, path);
        return false;
    }
    /* A plain directory-link operand lists its target. Long/classified listings
       preserve the link itself; directory entries are never dereferenced. */
    if (operand && S_ISLNK(st.st_mode) && !options->directory_as_file &&
        !options->long_format && !options->classify) {
        struct stat target;
        if (stat(path, &target) == 0 && S_ISDIR(target.st_mode)) st = target;
    }
    entry->name = xstrdup(name);
    entry->path = xstrdup(path);
    entry->st = st;
    return true;
}

void entry_free(Entry *entry) {
    free(entry->name);
    free(entry->path);
}

void entries_push(EntryList *list, Entry entry) {
    if (list->count == list->capacity) {
        if (list->capacity > SIZE_MAX / 2) {
            fputs("ls: too many entries\n", stderr);
            exit(2);
        }
        list->capacity = list->capacity ? list->capacity * 2 : 32;
        list->items = xreallocarray(list->items, list->capacity, sizeof(*list->items));
    }
    list->items[list->count++] = entry;
}

void entries_free(EntryList *list) {
    for (size_t i = 0; i < list->count; ++i) entry_free(&list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}
