#ifndef OPTIONS_H
#define OPTIONS_H
#include "ls.h"
void options_init(Options *options);
int options_parse(Options *options, int argc, char **argv);
#endif
