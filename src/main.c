#include "entries.h"
#include "format.h"
#include "list.h"
#include "options.h"
#include "sort.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>

int main(int argc, char **argv) {
    /* Character classification follows the user's locale; names sort by bytes. */
    (void)setlocale(LC_CTYPE, "");
    Options options;
    options_init(&options);
    int first = options_parse(&options, argc, argv);
    if (first < 0) return 2;

    EntryList files = {0}, directories = {0};
    RunState state = {0};
    int count = argc - first;
    for (int i = 0; i < (count ? count : 1); ++i) {
        const char *path = count ? argv[first + i] : ".";
        Entry entry;
        if (!entry_load(&entry, path, path, true, &options, &state)) continue;
        if (S_ISDIR(entry.st.st_mode) && !options.directory_as_file)
            entries_push(&directories, entry);
        else
            entries_push(&files, entry);
    }
    entries_sort(&files, &options);
    entries_sort(&directories, &options);
    for (size_t i = 0; i < files.count; ++i) {
        print_entry(&files.items[i], &options, &state);
        state.printed = true;
    }
    for (size_t i = 0; i < directories.count; ++i)
        list_directory(directories.items[i].path, &options,
                       count > 1 || options.recursive, &state);
    entries_free(&files);
    entries_free(&directories);
    if (fflush(stdout) == EOF || ferror(stdout)) {
        if (errno == 0) errno = EIO;
        report_error(&state, "standard output");
    }
    return state.status;
}
