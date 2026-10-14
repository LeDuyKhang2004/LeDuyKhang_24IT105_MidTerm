#include "list.h"
#include "entries.h"
#include "format.h"
#include "sort.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Heap-backed DFS avoids call-stack overflow and closes each directory before
   visiting its children. An ancestor chain detects filesystem cycles. */
typedef struct Ancestor {
    dev_t device;
    ino_t inode;
    size_t references;
    struct Ancestor *parent;
} Ancestor;

typedef struct Task {
    char *path;
    bool header;
    Ancestor *parent;
    struct Task *next;
} Task;

static void release_ancestor(Ancestor *ancestor) {
    while (ancestor && --ancestor->references == 0) {
        Ancestor *parent = ancestor->parent;
        free(ancestor);
        ancestor = parent;
    }
}

static bool visible_name(const char *name, const Options *options) {
    if (name[0] != '.') return true;
    if (options->all) return true;
    return options->almost_all && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static void push_task(Task **stack, const char *path, bool header, Ancestor *parent) {
    Task *task = xreallocarray(NULL, 1, sizeof(*task));
    task->path = xstrdup(path);
    task->header = header;
    task->parent = parent;
    if (parent) ++parent->references;
    task->next = *stack;
    *stack = task;
}

static void visit(Task *task, Task **stack, const Options *options, RunState *state) {
    DIR *directory = opendir(task->path);
    if (!directory) { report_error(state, task->path); return; }
    struct stat st;
    if (fstat(dirfd(directory), &st) != 0) {
        report_error(state, task->path);
        closedir(directory);
        return;
    }
    for (Ancestor *p = task->parent; p; p = p->parent) {
        if (p->device == st.st_dev && p->inode == st.st_ino) {
            fprintf(stderr, "ls: %s: directory cycle detected\n", task->path);
            state->status = 1;
            closedir(directory);
            return;
        }
    }
    EntryList entries = {0};
    for (;;) {
        errno = 0;
        struct dirent *item = readdir(directory);
        if (!item) {
            if (errno) report_error(state, task->path);
            break;
        }
        if (!visible_name(item->d_name, options)) continue;
        char *full = join_path(task->path, item->d_name);
        Entry entry;
        if (entry_load(&entry, full, item->d_name, false, options, state))
            entries_push(&entries, entry);
        free(full);
    }
    if (closedir(directory) != 0) report_error(state, task->path);
    entries_sort(&entries, options);
    if (task->header) {
        if (state->printed) putchar('\n');
        print_name(task->path, options);
        fputs(":\n", stdout);
        state->printed = true;
    }
    if (options->long_format || (options->show_blocks && options->terminal)) {
        print_total(&entries, options);
        state->printed = true;
    }
    for (size_t i = 0; i < entries.count; ++i) {
        print_entry(&entries.items[i], options, state);
        state->printed = true;
    }
    if (options->recursive) {
        Ancestor *current = xreallocarray(NULL, 1, sizeof(*current));
        *current = (Ancestor){st.st_dev, st.st_ino, 1, task->parent};
        if (current->parent) ++current->parent->references;
        /* Reverse push preserves the chosen listing order on the LIFO stack. */
        for (size_t i = entries.count; i > 0; --i) {
            Entry *entry = &entries.items[i - 1];
            if (!S_ISDIR(entry->st.st_mode) || strcmp(entry->name, ".") == 0 ||
                strcmp(entry->name, "..") == 0) continue;
            push_task(stack, entry->path, true, current);
        }
        release_ancestor(current);
    }
    entries_free(&entries);
}

void list_directory(const char *path, const Options *options, bool header,
                    RunState *state) {
    Task *stack = NULL;
    push_task(&stack, path, header, NULL);
    while (stack) {
        Task *task = stack;
        stack = task->next;
        visit(task, &stack, options, state);
        release_ancestor(task->parent);
        free(task->path);
        free(task);
    }
}
