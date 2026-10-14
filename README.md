# Midterm Project - Implement `ls(1)`

This repository contains a modular C implementation of the simplified NetBSD `ls(1)` command described by the supplied manual page.

## Implemented features

The program implements all options in the assignment manual:

`-A -a -c -d -F -f -h -i -k -l -n -q -R -r -S -s -t -u -w`

It supports hidden-entry filtering, long format permissions/owners/groups/timestamps, inode and block counts, human-readable sizes, `BLOCKSIZE`, reverse/name/size/time sorting, recursive traversal, directory classification suffixes, numeric IDs, access/change-time selection, safe non-printable-name output, symbolic links, special files, error reporting and POSIX exit status.

## Build and run

```sh
make
./ls [options] [file ...]
make test
make clean
```

`make test` compiles the program and runs a smoke test covering long format, hidden files, classification, symlinks and recursive traversal. The program returns `0` when all operands succeed and a non-zero status if an operand or directory cannot be read. The code is compiled with strict warnings and portable POSIX feature macros on macOS and Linux.

## Project structure

- `include/ls.h` - shared option, entry and run-state types
- `include/options.h`, `src/options.c` - option parsing and `BLOCKSIZE`
- `include/entries.h`, `src/entries.c` - safe allocation and filesystem metadata
- `include/sort.h`, `src/sort.c` - stable sorting by name, size or time
- `include/format.h`, `src/format.c` - permission, owner, size, time and name output
- `include/list.h`, `src/list.c` - directory reading and recursive traversal
- `src/main.c` - operand classification and program entry point
- `Makefile` - build, test and clean commands

## Repository

GitHub: https://github.com/LeDuyKhang2004/LeDuyKhang_24IT105_MidTerm
