/* Test-only I/O failures around the actual native load_file implementation. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mode, open_handles, live_allocations, seeks;
static FILE *checked_open(const char *name, const char *access)
{
    if (!name || mode == 2) return NULL;
    FILE *fp = fopen(name, access);
    if (fp) ++open_handles;
    return fp;
}
static int checked_seek(FILE *fp, long offset, int origin)
{
    ++seeks;
    if (mode == 3 && seeks == 1) return 1;
    if (mode == 4 && seeks == 2) return -1;
    return fseek(fp, offset, origin);
}
static long checked_tell(FILE *fp)
{
    if (mode == 5) return -1;
    if (mode == 6) {
#if LONG_MAX > INT_MAX
        return (long)INT_MAX + 1L;
#else
        return -1;
#endif
    }
    if (mode == 11) return 268435457L;
    return ftell(fp);
}
static void *checked_malloc(size_t count)
{
    if (mode == 7) return NULL;
    void *p = malloc(count);
    if (p) ++live_allocations;
    return p;
}
static void checked_free(void *p)
{
    if (p) --live_allocations;
    free(p);
}
static size_t checked_read(void *p, size_t size, size_t count, FILE *fp)
{
    if (mode == 8) return fread(p, 1, size * count - 1, fp) / size;
    return fread(p, size, count, fp);
}
static int checked_error(FILE *fp) { return mode == 9 ? 37 : ferror(fp); }
static int checked_close(FILE *fp)
{
    --open_handles;
    int result = fclose(fp);
    return mode == 10 ? EOF : result;
}

#define fopen checked_open
#define fseek checked_seek
#define ftell checked_tell
#define malloc checked_malloc
#define free checked_free
#define fread checked_read
#define ferror checked_error
#define fclose checked_close
#define main savetool_main
#include "main.c"
#undef main
#undef fopen
#undef fseek
#undef ftell
#undef malloc
#undef free
#undef fread
#undef ferror
#undef fclose

int main(int argc, char **argv)
{
    assert(argc == 3);
    mode = atoi(argv[2]);
    if (mode >= 12) {
        SAVEINFO *save = load_saveraw(argv[1]);
        assert(save && get_be32(&save->data_size) == 1048576);
        for (unsigned i = 0; i < 1048576; ++i) assert(save->dbuf[i] == 0xA5);
        assert(open_handles == 0 && live_allocations == 0);
        puts("complete maximum payload accepted without retaining its input allocation");
        return 0;
    }
    int size = 77;
    u8 *result = load_file(mode == 1 ? NULL : argv[1], &size);
    if (!mode) {
        const char sample[] = "loader bytes\n";
        assert(result && size == (int)sizeof(sample) - 1);
        assert(memcmp(result, sample, size) == 0);
        checked_free(result);
    } else {
        assert(result == NULL && size == 0);
    }
    assert(open_handles == 0 && live_allocations == 0);
    puts("actual load_file ownership and error outcome passed");
    return 0;
}
