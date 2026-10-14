#ifndef LIST_H
#define LIST_H
#include "ls.h"
void list_directory(const char *path, const Options *options, bool header,
                    RunState *state);
#endif
