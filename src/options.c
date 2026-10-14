#include "options.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uintmax_t environment_block_size(void) {
    const char *value = getenv("BLOCKSIZE");
    if (value == NULL || *value == '\0') return 512;
    char *end;
    errno = 0;
    uintmax_t number = strtoull(value, &end, 10);
    uintmax_t scale = 1;
    if (end == value && (*end == 'k' || *end == 'K' ||
                         *end == 'm' || *end == 'M' ||
                         *end == 'g' || *end == 'G')) number = 1;
    if (*end == 'k' || *end == 'K') { scale = 1024; ++end; }
    else if (*end == 'm' || *end == 'M') { scale = 1024 * 1024; ++end; }
    else if (*end == 'g' || *end == 'G') { scale = 1024 * 1024 * 1024; ++end; }
    if (errno == ERANGE || *value == '-' || *end != '\0' || number == 0 ||
        number > UINTMAX_MAX / scale || number * scale < 512) {
        fprintf(stderr, "ls: invalid BLOCKSIZE '%s'; using 512\n", value);
        return 512;
    }
    return number * scale;
}

void options_init(Options *options) {
    memset(options, 0, sizeof(*options));
    options->almost_all = geteuid() == 0;
    options->terminal = isatty(STDOUT_FILENO) != 0;
    options->quote_names = options->terminal;
    options->block_size = 512;
}

int options_parse(Options *options, int argc, char **argv) {
    bool size_override = false;
    int option;
    opterr = 0;
    while ((option = getopt(argc, argv, "+AacdFfhiklnqRrSstuw")) != -1) {
        switch (option) {
        case 'A': options->almost_all = true; break;
        case 'a': options->all = true; break;
        case 'c': options->time_field = TIME_CTIME; break;
        case 'd': options->directory_as_file = true; options->recursive = false; break;
        case 'F': options->classify = true; break;
        case 'f': options->no_sort = true; break;
        case 'h': options->human = true; size_override = true; break;
        case 'i': options->inode = true; break;
        case 'k':
            options->human = false;
            options->block_size = 1024;
            size_override = true;
            break;
        case 'l': options->long_format = true; options->numeric_ids = false; break;
        case 'n': options->long_format = true; options->numeric_ids = true; break;
        case 'q': options->quote_names = true; break;
        case 'R': options->recursive = true; options->directory_as_file = false; break;
        case 'r': options->reverse = true; break;
        case 'S': options->sort_field = SORT_SIZE; break;
        case 's': options->show_blocks = true; break;
        case 't': options->sort_field = SORT_TIME; break;
        case 'u': options->time_field = TIME_ATIME; break;
        case 'w': options->quote_names = false; break;
        default:
            fprintf(stderr, "ls: illegal option -- %c\n", optopt ? optopt : option);
            fprintf(stderr, "usage: ls [-AacdFfhiklnqRrSstuw] [file ...]\n");
            return -1;
        }
    }
    if (!size_override) options->block_size = environment_block_size();
    return optind;
}
