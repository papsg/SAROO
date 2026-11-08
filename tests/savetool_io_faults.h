/* Test-only syscall interception. Never included in the release utility. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <copyfile.h>
#endif
int test_mkstemp(char *);
ssize_t test_write(int, const void *, size_t);
int test_fsync(int);
int test_close(int);
int test_rename(const char *, const char *);
FILE *test_fopen(const char *, const char *);
size_t test_fwrite(const void *, size_t, size_t, FILE *);
int test_fclose(FILE *);
#ifdef __APPLE__
int test_fcopyfile(int, int, copyfile_state_t, copyfile_flags_t);
#endif
#ifndef SAROO_IO_SHIM_IMPLEMENTATION
#define mkstemp test_mkstemp
#define write test_write
#define fsync test_fsync
#define close test_close
#define rename test_rename
#define fopen test_fopen
#define fwrite test_fwrite
#define fclose test_fclose
#ifdef __APPLE__
#define fcopyfile test_fcopyfile
#endif
#endif
