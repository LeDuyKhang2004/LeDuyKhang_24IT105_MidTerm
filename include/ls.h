#ifndef LS_H
#define LS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef enum { TIME_MTIME, TIME_ATIME, TIME_CTIME } TimeField;
typedef enum { SORT_NAME, SORT_SIZE, SORT_TIME } SortField;

typedef struct {
    bool all, almost_all, classify, inode, long_format, numeric_ids;
    bool recursive, reverse, show_blocks, directory_as_file, no_sort;
    bool human, quote_names, terminal;
    uintmax_t block_size;
    TimeField time_field;
    SortField sort_field;
} Options;

typedef struct {
    char *name;
    char *path;
    struct stat st;
} Entry;

typedef struct {
    Entry *items;
    size_t count, capacity;
} EntryList;

typedef struct {
    int status;
    bool printed;
} RunState;

#endif
