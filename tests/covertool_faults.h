/* Native-test instrumentation only; never linked into the release utility. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __APPLE__
#include <copyfile.h>
#endif
int cover_test_mkstemp(char *);
ssize_t cover_test_pread(int, void *, size_t, off_t);
ssize_t cover_test_pwrite(int, const void *, size_t, off_t);
int cover_test_fsync(int);
int cover_test_close(int);
int cover_test_rename(const char *, const char *);
int cover_test_ftruncate(int, off_t);
int cover_test_fclose(FILE *);
void *cover_test_calloc(size_t, size_t);
void *cover_test_malloc(size_t);
void cover_test_free(void *);
void cover_test_probe(void);
#ifdef __APPLE__
int cover_test_fcopyfile(int, int, copyfile_state_t, copyfile_flags_t);
#endif
#ifndef COVER_TEST_IMPLEMENTATION
#define mkstemp cover_test_mkstemp
#define pread cover_test_pread
#define pwrite cover_test_pwrite
#define fsync cover_test_fsync
#define close cover_test_close
#define rename cover_test_rename
#define ftruncate cover_test_ftruncate
#define fclose cover_test_fclose
#define calloc cover_test_calloc
#define malloc cover_test_malloc
#define free cover_test_free
#ifdef __APPLE__
#define fcopyfile cover_test_fcopyfile
#endif
#endif
