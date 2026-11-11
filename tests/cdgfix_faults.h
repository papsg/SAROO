/* POSIX test fault wrappers adapted from paps-g fee3293f. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <copyfile.h>
#endif
int cdg_test_open(const char *, int, ...);
int cdg_test_mkstemp(char *);
ssize_t cdg_test_pread(int, void *, size_t, off_t);
ssize_t cdg_test_write(int, const void *, size_t);
size_t cdg_test_fread(void *, size_t, size_t, FILE *);
int cdg_test_fsync(int);
int cdg_test_close(int);
int cdg_test_fclose(FILE *);
int cdg_test_rename(const char *, const char *);
int cdg_test_fstat(int, struct stat *);
void *cdg_test_malloc(size_t);
#ifdef __APPLE__
int cdg_test_fcopyfile(int, int, copyfile_state_t, copyfile_flags_t);
#endif
#ifndef CDG_TEST_IMPLEMENTATION
#define open cdg_test_open
#define mkstemp cdg_test_mkstemp
#define pread cdg_test_pread
#define write cdg_test_write
#define fread cdg_test_fread
#define fsync cdg_test_fsync
#define close cdg_test_close
#define fclose cdg_test_fclose
#define rename cdg_test_rename
#define fstat cdg_test_fstat
#define malloc cdg_test_malloc
#ifdef __APPLE__
#define fcopyfile cdg_test_fcopyfile
#endif
#endif
