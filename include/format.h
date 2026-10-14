#ifndef FORMAT_H
#define FORMAT_H
#include "ls.h"
void print_name(const char *name, const Options *options);
void print_entry(const Entry *entry, const Options *options, RunState *state);
void print_total(const EntryList *list, const Options *options);
#endif
