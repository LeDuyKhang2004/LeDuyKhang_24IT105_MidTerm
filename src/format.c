#include "format.h"
#include "entries.h"

#include <ctype.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef major
#define major(dev) ((unsigned int)(((dev) >> 24) & 0xff))
#endif
#ifndef minor
#define minor(dev) ((unsigned int)((dev) & 0xffffff))
#endif

static void mode_string(mode_t mode, char output[11]) {
    output[0] = S_ISREG(mode) ? '-' : S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' :
                S_ISCHR(mode) ? 'c' : S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' :
                S_ISSOCK(mode) ? 's' : '?';
    output[1] = mode & S_IRUSR ? 'r' : '-'; output[2] = mode & S_IWUSR ? 'w' : '-';
    output[3] = mode & S_IXUSR ? 'x' : '-'; output[4] = mode & S_IRGRP ? 'r' : '-';
    output[5] = mode & S_IWGRP ? 'w' : '-'; output[6] = mode & S_IXGRP ? 'x' : '-';
    output[7] = mode & S_IROTH ? 'r' : '-'; output[8] = mode & S_IWOTH ? 'w' : '-';
    output[9] = mode & S_IXOTH ? 'x' : '-';
    if (mode & S_ISUID) output[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) output[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) output[9] = mode & S_IXOTH ? 't' : 'T';
    output[10] = '\0';
}

void print_name(const char *name, const Options *options) {
    const unsigned char *p = (const unsigned char *)name;
    for (; *p; ++p) {
        if (options->quote_names && (!isprint(*p) || *p == '\t')) putchar('?');
        else putchar(*p);
    }
}

static void print_size(off_t value, const Options *options) {
    if (!options->human) { printf("%lld", (long long)value); return; }
    const char *units = "BKMGTPE";
    double size = (double)value;
    size_t unit = 0;
    while (size >= 1024.0 && unit < 6) { size /= 1024.0; ++unit; }
    if (unit == 0) printf("%lldB", (long long)value);
    else if (size < 10.0) printf("%.1f%c", size, units[unit]);
    else printf("%.0f%c", size, units[unit]);
}

static void print_time(const Entry *entry, const Options *options) {
    time_t value = options->time_field == TIME_ATIME ? entry->st.st_atime :
                   options->time_field == TIME_CTIME ? entry->st.st_ctime : entry->st.st_mtime;
    struct tm local_time;
    localtime_r(&value, &local_time);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%b %e %H:%M", &local_time);
    fputs(buffer, stdout);
}

static const char *owner_name(uid_t value, bool numeric) {
    static char buffer[32];
    if (!numeric) { struct passwd *pw = getpwuid(value); if (pw) return pw->pw_name; }
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)value); return buffer;
}

static const char *group_name(gid_t value, bool numeric) {
    static char buffer[32];
    if (!numeric) { struct group *gr = getgrgid(value); if (gr) return gr->gr_name; }
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)value); return buffer;
}

static char classification(mode_t mode) {
    if (S_ISDIR(mode)) return '/'; if (S_ISLNK(mode)) return '@';
    if (S_ISFIFO(mode)) return '|'; if (S_ISSOCK(mode)) return '=';
    if (S_ISCHR(mode) || S_ISBLK(mode)) return '%';
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) return '*';
    return '\0';
}

void print_entry(const Entry *entry, const Options *options, RunState *state) {
    if (options->inode) printf("%llu ", (unsigned long long)entry->st.st_ino);
    if (options->show_blocks) {
        uintmax_t bytes = (uintmax_t)entry->st.st_blocks * 512;
        uintmax_t blocks = (bytes + options->block_size - 1) / options->block_size;
        if (options->human) print_size((off_t)bytes, options); else printf("%ju", blocks);
        putchar(' ');
    }
    if (options->long_format) {
        char mode[11]; mode_string(entry->st.st_mode, mode);
        printf("%s %3lu %-8s %-8s ", mode, (unsigned long)entry->st.st_nlink,
               owner_name(entry->st.st_uid, options->numeric_ids),
               group_name(entry->st.st_gid, options->numeric_ids));
        if (S_ISCHR(entry->st.st_mode) || S_ISBLK(entry->st.st_mode))
            printf("%u,%u ", major(entry->st.st_rdev), minor(entry->st.st_rdev));
        else { print_size(entry->st.st_size, options); putchar(' '); }
        print_time(entry, options); putchar(' ');
    }
    print_name(entry->name, options);
    if (options->classify) { char suffix = classification(entry->st.st_mode); if (suffix) putchar(suffix); }
    if (options->long_format && S_ISLNK(entry->st.st_mode)) {
        char target[PATH_MAX]; ssize_t length = readlink(entry->path, target, sizeof(target) - 1);
        if (length >= 0) { target[length] = '\0'; fputs(" -> ", stdout); print_name(target, options); }
    }
    putchar('\n');
    if (ferror(stdout)) state->status = 1;
}

void print_total(const EntryList *list, const Options *options) {
    uintmax_t total = 0;
    for (size_t i = 0; i < list->count; ++i) {
        uintmax_t bytes = (uintmax_t)list->items[i].st.st_blocks * 512;
        total += (bytes + options->block_size - 1) / options->block_size;
    }
    printf("total %ju\n", total);
}
