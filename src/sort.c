#include "sort.h"
#include "entries.h"

#include <stdlib.h>
#include <string.h>

struct timespec entry_time(const Entry *entry, TimeField field) {
#if defined(__APPLE__)
    if (field == TIME_ATIME) return entry->st.st_atimespec;
    if (field == TIME_CTIME) return entry->st.st_ctimespec;
    return entry->st.st_mtimespec;
#else
    if (field == TIME_ATIME) return entry->st.st_atim;
    if (field == TIME_CTIME) return entry->st.st_ctim;
    return entry->st.st_mtim;
#endif
}

static int compare(const Entry *left, const Entry *right, const Options *options) {
    int result = 0;
    if (options->sort_field == SORT_SIZE) {
        result = (left->st.st_size < right->st.st_size) -
                 (left->st.st_size > right->st.st_size);
    } else if (options->sort_field == SORT_TIME) {
        struct timespec a = entry_time(left, options->time_field);
        struct timespec b = entry_time(right, options->time_field);
        result = (a.tv_sec < b.tv_sec) - (a.tv_sec > b.tv_sec);
        if (result == 0) result = (a.tv_nsec < b.tv_nsec) - (a.tv_nsec > b.tv_nsec);
    }
    if (result == 0) {
        int name_order = strcmp(left->name, right->name);
        result = (name_order > 0) - (name_order < 0);
    }
    return options->reverse ? -result : result;
}

/* Stable merge sort avoids incompatible BSD/GNU qsort_r interfaces and keeps
   O(n log n) sorting even for large directories. */
static void merge_sort(Entry *items, Entry *scratch, size_t count,
                       const Options *options) {
    if (count < 2) return;
    size_t middle = count / 2;
    merge_sort(items, scratch, middle, options);
    merge_sort(items + middle, scratch + middle, count - middle, options);
    size_t left = 0, right = middle, out = 0;
    while (left < middle && right < count) {
        scratch[out++] = compare(&items[left], &items[right], options) <= 0
                       ? items[left++] : items[right++];
    }
    while (left < middle) scratch[out++] = items[left++];
    while (right < count) scratch[out++] = items[right++];
    memcpy(items, scratch, count * sizeof(*items));
}

void entries_sort(EntryList *list, const Options *options) {
    if (options->no_sort || list->count < 2) return;
    Entry *scratch = xreallocarray(NULL, list->count, sizeof(*scratch));
    merge_sort(list->items, scratch, list->count, options);
    free(scratch);
}
