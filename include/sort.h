#ifndef SORT_H
#define SORT_H
#include "ls.h"
#include <time.h>
struct timespec entry_time(const Entry *entry, TimeField field);
void entries_sort(EntryList *list, const Options *options);
#endif
