/* Inject failures at real file boundaries, including legacy stdio paths, to
 * demonstrate the old data-loss behavior with the same generated scenarios. */
#define SAROO_IO_SHIM_IMPLEMENTATION
#include "savetool_io_faults.h"
#include <errno.h>
#include <string.h>
static int staged = -1, writes;
static FILE *legacy;
static int fault(const char *value)
{
    const char *selected = getenv("SAROO_TEST_IO_FAULT");
    return selected && strcmp(selected, value) == 0;
}
int test_mkstemp(char *name)
{
    if (fault("create")) { errno = EACCES; return -1; }
    staged = mkstemp(name);
    return staged;
}
ssize_t test_write(int fd, const void *buf, size_t count)
{
    if (fd == staged) {
        if (fault("partial")) {
            if (writes++) { errno = ENOSPC; return -1; }
            return write(fd, buf, count > 7 ? 7 : count);
        }
        if (fault("zero")) return 0;
        if (fault("short") && !writes++) return write(fd, buf, count > 7 ? 7 : count);
        if (fault("interrupt") && !writes++) { errno = EINTR; return -1; }
    }
    return write(fd, buf, count);
}
int test_fsync(int fd)
{
    struct stat st;
    if ((fd == staged && fault("sync")) ||
        (fault("directory") && fstat(fd, &st) == 0 && S_ISDIR(st.st_mode))) {
        errno = EIO;
        return -1;
    }
    return fsync(fd);
}
int test_close(int fd)
{
    int is_stage = fd == staged;
    if (is_stage) staged = -1;
    int result = close(fd);
    if (is_stage && fault("close")) { errno = EIO; return -1; }
    return result;
}
int test_rename(const char *from, const char *to)
{
    if (fault("rename")) { errno = EACCES; return -1; }
    return rename(from, to);
}
FILE *test_fopen(const char *name, const char *mode)
{
    if (!strcmp(mode, "wb") && fault("create")) { errno = EACCES; return NULL; }
    FILE *fp = fopen(name, mode);
    if (!strcmp(mode, "wb")) legacy = fp;
    return fp;
}
size_t test_fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
    if (fp == legacy) {
        if (fault("partial") || fault("short")) {
            size_t bytes = size * count;
            errno = ENOSPC;
            return size ? fwrite(buf, 1, bytes > 7 ? 7 : bytes, fp) / size : 0;
        }
        if (fault("zero") || fault("interrupt")) { errno = EINTR; return 0; }
    }
    return fwrite(buf, size, count, fp);
}
int test_fclose(FILE *fp)
{
    int selected = fp == legacy;
    if (selected) legacy = NULL;
    int result = fclose(fp);
    if (selected && fault("close")) { errno = EIO; return EOF; }
    return result;
}
#ifdef __APPLE__
int test_fcopyfile(int from, int to, copyfile_state_t state, copyfile_flags_t flags)
{
    if (to == staged && fault("metadata")) { errno = EIO; return -1; }
    return fcopyfile(from, to, state, flags);
}
#endif
