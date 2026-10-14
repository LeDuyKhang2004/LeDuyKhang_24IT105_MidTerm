#ifndef ENTRIES_H
#define ENTRIES_H
#include "ls.h"
void *xreallocarray(void *ptr, size_t count, size_t size);
char *xstrdup(const char *text);
char *join_path(const char *directory, const char *name);
void report_error(RunState *state, const char *path);
bool entry_load(Entry *entry, const char *path, const char *name,
                bool operand, const Options *options, RunState *state);
void entry_free(Entry *entry);
void entries_push(EntryList *list, Entry entry);
void entries_free(EntryList *list);
#endif
