#include "list.h"
#include "entries.h"
#include "format.h"
#include "sort.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool visible_name(const char *name, const Options *options) {
    if (name[0] != '.') return true;
    if (options->all) return true;
    if (options->almost_all) return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
    return false;
}

static void list_one_directory(const char *path, const Options *options,
                               bool header, RunState *state) {
    DIR *directory = opendir(path);
    if (directory == NULL) { report_error(state, path); return; }
    EntryList entries = {0};
    struct dirent *item;
    while ((item = readdir(directory)) != NULL) {
        if (!visible_name(item->d_name, options)) continue;
        char *full = join_path(path, item->d_name);
        Entry entry;
        if (entry_load(&entry, full, item->d_name, false, options, state)) entries_push(&entries, entry);
        free(full);
    }
    closedir(directory);
    entries_sort(&entries, options);
    if (header) { printf("%s:\n", path); state->printed = true; }
    if (options->long_format && entries.count) print_total(&entries, options);
    for (size_t i = 0; i < entries.count; ++i) print_entry(&entries.items[i], options, state);
    if (header && options->recursive) putchar('\n');
    if (options->recursive) {
        for (size_t i = 0; i < entries.count; ++i) {
            if (!S_ISDIR(entries.items[i].st.st_mode)) continue;
            if (strcmp(entries.items[i].name, ".") == 0 || strcmp(entries.items[i].name, "..") == 0) continue;
            list_one_directory(entries.items[i].path, options, true, state);
        }
    }
    entries_free(&entries);
}

void list_directory(const char *path, const Options *options, bool header,
                    RunState *state) {
    list_one_directory(path, options, header, state);
}
