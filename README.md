# Midterm Project - Implement ls(1)

**Student:** Le Duy Khang  
**Student ID:** 24IT105  
**Course:** System Programming  
**Repository:** https://github.com/LeDuyKhang2004/LeDuyKhang_24IT105_MidTerm

## 1. Objective and reference

Implement the simplified UNIX `ls(1)` command from scratch in modular C. The
behavioral specification is the **supplied three-page ls.pdf**, labelled
NetBSD 10.1, October 27, 2024. The implementation does not invoke the system
`ls` command, and does not add GNU-specific options or color output.

## 2. Build and run

Requirements: a POSIX environment (Linux or macOS), a C11 compiler (GCC or
Clang), and Make. Python 3 is needed only to run the test suite.

```sh
git clone https://github.com/LeDuyKhang2004/LeDuyKhang_24IT105_MidTerm.git
cd LeDuyKhang_24IT105_MidTerm
make
./ls
./ls -la
./ls -lh src
./ls -R include src
./ls -d .
./ls -S src
./ls -tu src
BLOCKSIZE=1024 ./ls -s src
TZ=UTC ./ls -l README.md
make test
make clean
```

Use `./ls` to run this program. Running plain `ls` normally invokes the operating
system's command. Options precede operands. Use `--` to end option parsing, for
example `./ls -- -filename`.

## 3. Implemented options

| Option | Behavior |
| --- | --- |
| `-A` | Include dotfiles except `.` and `..`; enabled automatically for root. |
| `-a` | Include all entries, including `.` and `..`. |
| `-c` | Select status-change time for `-t` sorting and `-l` output. |
| `-d` | List directory operands themselves; preserve symbolic-link operands. |
| `-F` | Add `/` for directories, `*` for executable regular files, `@` for links, `=` for sockets, `\|` for FIFOs, and `%` for whiteouts on supporting systems. |
| `-f` | Preserve directory/argument order without sorting. |
| `-h` | Display sizes and allocated space in readable base-1024 byte units. |
| `-i` | Print the inode number before each entry. |
| `-k` | Use 1024-byte units for allocated block counts; long-format file sizes remain bytes. |
| `-l` | Print type/permissions, link count, owner, group, size, time and name. |
| `-n` | Use long format with numeric UID/GID. |
| `-q` | Replace non-printable characters with `?`; default on a terminal. |
| `-R` | List descendant directories in depth-first order. |
| `-r` | Reverse the selected sort, including name tie-breaking. |
| `-S` | Sort largest size first, then by name. |
| `-s` | Print allocated space from `st_blocks`, rather than logical file size. |
| `-t` | Sort newest selected timestamp first, then by name. |
| `-u` | Select access time for `-t` sorting and `-l` output. |
| `-w` | Print filename bytes unchanged; default for redirected/piped output. |

### General listing rules

- With no operands, list the current directory. Print one entry per line.
- Print non-directory operands before directory contents, sorting the two groups
  separately. `-d` treats all operands as ordinary entries in one group.
- The rightmost option wins for `-l`/`-n`, `-q`/`-w`, `-c`/`-u`, `-R`/`-d`,
  and `-h`/`-k`. `-a` includes the dot entries even with `-A`.
- `-f` disables sorting without silently enabling `-a` or disabling long format.
- `-l` always prints a directory's `total`, including `total 0` for an empty
  directory. `-s` alone prints the total only when stdout is a terminal.
- `BLOCKSIZE` controls allocated-space units when neither `-h` nor `-k` is given.
  Positive byte sizes of at least 512, optionally with `K`, `M`, or `G`, are
  accepted. Invalid values produce a warning and fall back to 512.
- `TZ` controls displayed dates. Dates use `Mon day HH:MM` as described by the
  provided manual. Fields are separated by spaces.
- Character and block devices show major/minor device numbers in the size field.
  Long-format symbolic links include ` -> target`, including dangling links.
- Permissions include `rwx`, set-user-ID/set-group-ID (`s`/`S`), and sticky
  (`t`/`T`). Unknown user/group names fall back to numeric IDs.

### Choices where the subset manual is not explicit

Names use bytewise lexicographical order. Timestamps use nanosecond precision
when available. For combined `-S`/`-t`, the last option selects the primary key.
Human sizes use `B`, `K`, `M`, etc., with one decimal below 10 units.

A plain command-line link to a directory lists its target; `-d`, `-l`, `-n`,
or `-F` lists the link itself. Links encountered inside directories are never
followed during recursion. A trailing slash requests directory resolution from
the operating system. The implementation leaves its working directory unchanged.

The archive-state type markers `a` and `A` listed in the manual are not standard
Linux/macOS filesystem types. Whiteout formatting is compiled only where
`S_ISWHT` exists. These platform-specific file types were not available in the
Ubuntu/macOS test environments. No claim of binary-identical output to every
full NetBSD/GNU/macOS `ls` implementation is made.

## 4. Modular design

```text
Makefile
README.md
.gitignore
include/
    ls.h          Shared types: Options, Entry, EntryList, RunState
    options.h     Option parser interface
    entries.h     Allocation, metadata and entry-vector interface
    sort.h        Timestamp selection and sorting interface
    format.h      Filename, entry and total output interface
    list.h        Directory traversal interface
src/
    main.c        Parse -> classify operands -> sort -> list -> report status
    options.c     getopt parsing, option precedence and BLOCKSIZE
    entries.c     lstat/stat, dynamic path construction and ownership
    sort.c        Stable merge sort with name/size/time comparators
    format.c      Permissions, IDs, devices, sizes, dates and link targets
    list.c        Directory reading and iterative depth-first traversal
tests/
    test_ls.py    Automated behavioral regression tests
```

The main filesystem calls are `opendir`, `readdir`, `closedir`, `lstat`, `stat`,
`fstat` and `readlink`. Owner/group names use `getpwuid`/`getgrgid`; dates use
`localtime_r`/`strftime`. These library calls read metadata rather than file
contents. Sorting costs O(n log n) time and O(n) auxiliary space per directory.

Paths, entry arrays and symbolic-link targets grow dynamically; there is no
fixed `PATH_MAX` buffer that silently truncates a filename. Allocations are
checked for failure and overflow. Directory traversal uses a heap-backed task
stack, closes each directory before visiting its children, skips `.`/`..`, and
checks ancestor device/inode pairs to detect cycles. This avoids recursive
call-stack overflow and keeps the number of open directory descriptors bounded.

Errors go to stderr while other accessible operands continue to be processed.
Return values are `0` for success, `1` for filesystem/output errors, and `2` for
invalid options or allocation failure. Names are not treated as format strings;
`-q` respects printable multibyte characters in the user's character locale.

## 5. Validation

```sh
make test
make sanitize
# Return to an optimized build after sanitizer testing:
make clean
make
```

The test suite checks expected behavior against this project's manual-based
specification and filesystem metadata, rather than copying the host `ls`
output. It creates temporary fixtures and removes them after each test.

Validation performed on October 14, 2026:

- **Ubuntu 24.04 ARM64 in Parallels:** all 24 test groups pass as the ordinary
  user `LeDuyKhang`, including unreadable directories and non-UTF-8 filename bytes.
- **macOS with Apple Clang:** 23 pass; the non-UTF-8 filename fixture is skipped
  because the filesystem rejects its creation.
- **AddressSanitizer + UndefinedBehaviorSanitizer on macOS:** the same 23 tests
  pass, with the same one filesystem-related skip and no sanitizer findings.
- **AddressSanitizer + UndefinedBehaviorSanitizer on Ubuntu:** all 24 test groups
  pass without sanitizer findings.

Coverage includes all option families, precedence, nanosecond/time/size sorting,
hidden entries, mixed operands, empty directories, symlinks and dangling links,
symlink recursion avoidance, device IDs, FIFO/socket suffixes, permission bits,
sparse-file block accounting, `BLOCKSIZE`, `TZ`, terminal-versus-pipe behavior,
UTF-8/control characters, invalid arguments, 600-entry directories and a tree
120 levels deep. Root runs skip the unreadable-directory assertion because root
can bypass ordinary mode permissions.

## 6. Submission

Repository URL: https://github.com/LeDuyKhang2004/LeDuyKhang_24IT105_MidTerm

`README.md` is the project report. The repository includes only source, headers,
Makefile, report and tests. `.gitignore` excludes the executable, object files,
debug bundles, Python caches and common editor/system files. Submit the report
and repository link through the course e-learning system as instructed.
