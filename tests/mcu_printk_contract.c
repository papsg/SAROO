/* Exercise the production MCU formatter with bounded transport stubs. */
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

static char transmitted[256];
static unsigned sends;

static void capture(char *text)
{
    /* The real UART calls strlen too. ASan catches an unterminated buffer. */
    size_t size = strlen(text);
    assert(size < sizeof transmitted);
    memcpy(transmitted, text, size + 1);
    sends++;
}

void uart4_puts(char *text) { capture(text); }
void _puts(char *text) { capture(text); }

static int format(char *out, int capacity, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int count = vsnprintk(out, capacity, fmt, args);
    va_end(args);
    return count;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const int scenario = atoi(argv[1]);
    char out[64];

    if (scenario == 0) {
        assert(printk("Load %s", "ok") == (int)sizeof "Load ok");
        assert(sends == 1 && strcmp(transmitted, "Load ok") == 0);
        assert(format(out, sizeof out, "%08x", 0x1234) == 9);
        assert(strcmp(out, "00001234") == 0);
    } else if (scenario == 1) {
        char path[246];
        memset(path, 'x', sizeof path - 1);
        path[sizeof path - 1] = 0;
        assert(printk("Load disc: {%s}", path) == 256);
        assert(sends == 1 && strlen(transmitted) == 255);
    } else if (scenario == 2) {
        assert(format(out, sizeof out, "%d", INT_MIN) == 12);
        assert(strcmp(out, "-2147483648") == 0);
    } else if (scenario == 3) {
        assert(format(out, sizeof out, "%-5d", -3) == 6);
        assert(strcmp(out, "-3   ") == 0);
        assert(format(out, sizeof out, "%+05d", 3) == 6);
        assert(strcmp(out, "+0003") == 0);
    } else if (scenario == 4) {
        memset(out, 0xa5, sizeof out);
        assert(format(out, 8, "%9999999999d", 3) == 8);
        assert(out[7] == 0);
        assert((unsigned char)out[8] == 0xa5);
    } else if (scenario == 5) {
        memset(out, 0xa5, sizeof out);
        assert(format(out, 1, "%s", "abc") == 1 && out[0] == 0);
        assert((unsigned char)out[1] == 0xa5);
        assert(format(out, 0, "%s", "abc") == 0);
        assert(format(out, -1, "%s", "abc") == 0);
        assert(format(NULL, 1, "%s", "abc") == 0);
        assert(format(out, 8, NULL) == 0);
        assert(printk(NULL) == 0 && sends == 0);
    } else {
        return 2;
    }

    puts("pass");
    return 0;
}
