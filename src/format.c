#include "format.h"
#include "entries.h"
#include "sort.h"

#include <errno.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#endif

static void mode_string(mode_t mode, char output[11]) {
    output[0] = S_ISREG(mode) ? '-' : S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' :
                S_ISCHR(mode) ? 'c' : S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' :
                S_ISSOCK(mode) ? 's' : '?';
#ifdef S_ISWHT
    if (S_ISWHT(mode)) output[0] = 'w';
#endif
    const mode_t bits[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                           S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    for (size_t i = 0; i < 9; ++i) output[i + 1] = mode & bits[i] ? "rwx"[i % 3] : '-';
    if (mode & S_ISUID) output[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) output[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) output[9] = mode & S_IXOTH ? 't' : 'T';
    output[10] = '\0';
}

void print_name(const char *name, const Options *options) {
    if (!options->quote_names) {
        fputs(name, stdout);
        return;
    }
    mbstate_t conversion = {0};
    size_t remaining = strlen(name);
    while (remaining > 0) {
        wchar_t character;
        size_t length = mbrtowc(&character, name, remaining, &conversion);
        if (length == (size_t)-1 || length == (size_t)-2) {
            putchar('?');
            length = 1;
            memset(&conversion, 0, sizeof(conversion));
        } else {
            if (iswprint(character)) fwrite(name, 1, length, stdout);
            else putchar('?');
        }
        name += length;
        remaining -= length;
    }
}

static void print_human(uintmax_t bytes) {
    const char *units = "BKMGTPE";
    long double size = (long double)bytes;
    size_t unit = 0;
    while (size >= 1024.0L && unit < 6) { size /= 1024.0L; ++unit; }
    /* Promote values whose rounded output would otherwise read 1024K. */
    if (size >= 1023.5L && unit < 6) { size /= 1024.0L; ++unit; }
    if (unit == 0) printf("%juB", bytes);
    else if (size < 9.95L) printf("%.1Lf%c", size, units[unit]);
    else printf("%.0Lf%c", size, units[unit]);
}

static uintmax_t allocated_bytes(const Entry *entry) {
    if (entry->st.st_blocks <= 0) return 0;
    uintmax_t blocks = (uintmax_t)entry->st.st_blocks;
    return blocks > UINTMAX_MAX / 512 ? UINTMAX_MAX : blocks * 512;
}

static void print_blocks(uintmax_t bytes, const Options *options) {
    if (options->human) print_human(bytes);
    else printf("%ju", bytes / options->block_size + (bytes % options->block_size != 0));
}

static void print_time(const Entry *entry, const Options *options) {
    time_t value = entry_time(entry, options->time_field).tv_sec;
    struct tm local_time;
    char buffer[64];
    if (localtime_r(&value, &local_time) == NULL ||
        strftime(buffer, sizeof(buffer), "%b %e %H:%M", &local_time) == 0)
        fputs("??? ?? ??:??", stdout);
    else fputs(buffer, stdout);
}

static void print_owner(uid_t uid, bool numeric) {
    struct passwd *owner = numeric ? NULL : getpwuid(uid);
    if (owner) fputs(owner->pw_name, stdout);
    else printf("%ju", (uintmax_t)uid);
}

static void print_group(gid_t gid, bool numeric) {
    struct group *group = numeric ? NULL : getgrgid(gid);
    if (group) fputs(group->gr_name, stdout);
    else printf("%ju", (uintmax_t)gid);
}

static char classification(mode_t mode) {
    if (S_ISDIR(mode)) return '/';
    if (S_ISLNK(mode)) return '@';
    if (S_ISFIFO(mode)) return '|';
    if (S_ISSOCK(mode)) return '=';
#ifdef S_ISWHT
    if (S_ISWHT(mode)) return '%';
#endif
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return '*';
    return '\0';
}

static void print_link(const Entry *entry, const Options *options, RunState *state) {
    size_t capacity = 128;
    char *target = NULL;
    for (;;) {
        target = xreallocarray(target, capacity + 1, 1);
        ssize_t length = readlink(entry->path, target, capacity);
        if (length < 0) { report_error(state, entry->path); break; }
        if ((size_t)length < capacity) {
            target[length] = '\0';
            fputs(" -> ", stdout);
            print_name(target, options);
            break;
        }
        if (capacity > (SIZE_MAX - 1) / 2) {
            errno = ENAMETOOLONG;
            report_error(state, entry->path);
            break;
        }
        capacity *= 2;
    }
    free(target);
}

void print_entry(const Entry *entry, const Options *options, RunState *state) {
    if (options->inode) printf("%ju ", (uintmax_t)entry->st.st_ino);
    if (options->show_blocks) {
        print_blocks(allocated_bytes(entry), options);
        putchar(' ');
    }
    if (options->long_format) {
        char mode[11];
        mode_string(entry->st.st_mode, mode);
        printf("%s %ju ", mode, (uintmax_t)entry->st.st_nlink);
        print_owner(entry->st.st_uid, options->numeric_ids);
        putchar(' ');
        print_group(entry->st.st_gid, options->numeric_ids);
        putchar(' ');
        if (S_ISCHR(entry->st.st_mode) || S_ISBLK(entry->st.st_mode))
            printf("%ju, %ju", (uintmax_t)major(entry->st.st_rdev), (uintmax_t)minor(entry->st.st_rdev));
        else if (options->human && entry->st.st_size >= 0)
            print_human((uintmax_t)entry->st.st_size);
        else printf("%jd", (intmax_t)entry->st.st_size);
        putchar(' ');
        print_time(entry, options);
        putchar(' ');
    }
    print_name(entry->name, options);
    if (options->classify) {
        char suffix = classification(entry->st.st_mode);
        if (suffix) putchar(suffix);
    }
    if (options->long_format && S_ISLNK(entry->st.st_mode)) print_link(entry, options, state);
    putchar('\n');
}

void print_total(const EntryList *list, const Options *options) {
    uintmax_t total = 0;
    for (size_t i = 0; i < list->count; ++i) {
        uintmax_t bytes = allocated_bytes(&list->items[i]);
        total = bytes > UINTMAX_MAX - total ? UINTMAX_MAX : total + bytes;
    }
    fputs("total ", stdout);
    print_blocks(total, options);
    putchar('\n');
}
