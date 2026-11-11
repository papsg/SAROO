/* Write the pinned FatFs uppercase mapping in portable little-endian order. */
#include "ff.h"
#include <stdio.h>

static int emit(FILE *out, DWORD value)
{
    DWORD mapped = ff_wtoupper(value);
    unsigned char bytes[] = {mapped, mapped >> 8, mapped >> 16, mapped >> 24};
    return fwrite(bytes, 1, sizeof(bytes), out) == sizeof(bytes) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *out = fopen(argv[1], "wb");
    if (!out) return 2;

    int failed = 0;
    for (DWORD value = 0; value <= 0x10ffff && !failed; value++)
        failed = emit(out, value);
    const DWORD outside[] = {0x110000, 0xffffff, 0x7fffffff, 0x80000000, 0xffffffff};
    for (unsigned i = 0; i < sizeof(outside) / sizeof(outside[0]) && !failed; i++)
        failed = emit(out, outside[i]);
    if (fclose(out)) return 1;
    return failed;
}
