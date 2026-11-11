/* Adapted from paps-g fee3293f. Compiled into the host fixture only. */
#define CDG_TEST_IMPLEMENTATION
#include "cdgfix_faults.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

static unsigned fault_hits, triggered, read_calls, prefix_calls, write_calls, creations;
static size_t read_bytes, prefix_bytes, write_bytes;
static int temporary = -1;

static int fail(const char *operation)
{
    const char *value = getenv("SAROO_CDG_FAULT");
    if (!value) return 0;
    size_t length = strlen(operation);
    if (strncmp(value, operation, length) || (value[length] && value[length] != ':')) return 0;
    unsigned nth = value[length] ? (unsigned)strtoul(value + length + 1, NULL, 10) : 1;
    if (++fault_hits != nth) return 0;
    ++triggered;
    errno = EIO;
    return 1;
}

int cdg_test_open(const char *path, int flags, ...)
{
    if (fail(flags & O_DIRECTORY ? "open-directory" :
             (flags & O_ACCMODE) == O_RDWR ? "open-writable" : "open-input")) return -1;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }
    return open(path, flags, mode);
}

int cdg_test_mkstemp(char *path)
{
    ++creations;
    if (fail("create")) return -1;
    temporary = mkstemp(path);
    return temporary;
}

void *cdg_test_malloc(size_t size)
{ if (fail("alloc")) { errno = ENOMEM; return NULL; } return malloc(size); }

size_t cdg_test_fread(void *data, size_t size, size_t count, FILE *input)
{
    ++read_calls;
    if (fail("stream-zero")) return 0;
    if (count > 1 && fail("stream-short")) count /= 2;
    size_t result = fread(data, size, count, input);
    read_bytes += result * size;
    const char *path = getenv("SAROO_CDG_INPUT");
    if (fail("append")) {
        int fd = open(path, O_WRONLY | O_APPEND);
        assert(fd >= 0 && write(fd, "x", 1) == 1 && !close(fd));
    }
    if (fail("modify")) {
        int fd = open(path, O_WRONLY);
        assert(fd >= 0 && pwrite(fd, "x", 1, 0) == 1 && !close(fd));
    }
    if (fail("truncate")) assert(!truncate(path, 24));
    return result;
}

ssize_t cdg_test_pread(int fd, void *data, size_t size, off_t offset)
{
    ++prefix_calls;
    if (fail("prefix-error")) return -1;
    if (fail("prefix-zero")) return 0;
    if (fail("prefix-intr")) { errno = EINTR; return -1; }
    if (size > 1 && fail("prefix-short")) size /= 2;
    ssize_t result = pread(fd, data, size, offset);
    if (result > 0) prefix_bytes += (size_t)result;
    return result;
}

ssize_t cdg_test_write(int fd, const void *data, size_t size)
{
    ++write_calls;
    if (fail("write-error")) return -1;
    if (fail("write-zero")) return 0;
    if (fail("write-intr")) { errno = EINTR; return -1; }
    if (size > 1 && fail("write-short")) size /= 2;
    ssize_t result = write(fd, data, size);
    if (result > 0) write_bytes += (size_t)result;
    return result;
}

int cdg_test_close(int fd)
{
    struct stat st;
    assert(!fstat(fd, &st));
    const char *operation = fd == temporary ? "close-temp" :
                           S_ISDIR(st.st_mode) ? "close-directory" : "close-writable";
    if (fd == temporary) temporary = -1;
    int result = close(fd);
    if (fail(operation)) return -1;
    return result;
}

int cdg_test_fclose(FILE *input)
{ int result = fclose(input); if (fail("close-input")) return EOF; return result; }

int cdg_test_fsync(int fd)
{
    struct stat st;
    assert(!fstat(fd, &st));
    if (fail(S_ISDIR(st.st_mode) ? "sync-directory" : "sync-file")) return -1;
    return fsync(fd);
}

int cdg_test_rename(const char *a, const char *b)
{ if (fail("rename")) return -1; return rename(a, b); }

int cdg_test_fstat(int fd, struct stat *st)
{ if (fail("stat")) return -1; return fstat(fd, st); }

#ifdef __APPLE__
int cdg_test_fcopyfile(int a, int b, copyfile_state_t state, copyfile_flags_t flags)
{ if (fail("metadata")) return -1; return fcopyfile(a, b, state, flags); }
#endif

__attribute__((destructor)) static void report(void)
{
    const char *path = getenv("SAROO_CDG_STATS");
    if (!path) return;
    FILE *file = fopen(path, "w");
    assert(file);
    fprintf(file, "{\"triggered\":%u,\"creations\":%u,\"read_calls\":%u,\"read_bytes\":%zu,"
                  "\"prefix_calls\":%u,\"prefix_bytes\":%zu,\"write_calls\":%u,\"write_bytes\":%zu}\n",
                  triggered, creations, read_calls, read_bytes,
                  prefix_calls, prefix_bytes, write_calls, write_bytes);
    assert(!fclose(file));
}
