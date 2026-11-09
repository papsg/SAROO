/* Adapted from paps-g f5ed0ae1; actual public backup module, host seams only. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "game_save.c"

_Alignas(8) u8 test_shared[0x23000];
u16 test_arg;
static u16 *test_command(void) { abort(); }
static unsigned forwarded;

static void system_get_date(u32 date, BUPDATE *out)
{
    assert(date == 0x12345678);
    memset(out, 0x42, sizeof(*out));
    ++forwarded;
}

static u32 system_set_date(BUPDATE *in)
{
    assert(in->year == 99);
    ++forwarded;
    return 0xabcdef;
}

int main(int argc, char **argv)
{
    assert(argc == 2 && sizeof(BUPDATE) == 6);
    if (!strcmp(argv[1], "null")) {
        sro_bup_get_date(0, NULL);
        assert(sro_bup_set_date(NULL) == 0xffffffffu);
        return 0;
    }
    if (!strcmp(argv[1], "dispatch")) {
        BUPDATE date = {99, 1, 1, 0, 0, 0};
        use_sys_bup = 1;
        sys_bup_get_date = system_get_date;
        sys_bup_set_date = system_set_date;
        assert(bup_set_date(&date) == 0xabcdef);
        bup_get_date(0x12345678, &date);
        assert(date.year == 0x42 && date.week == 0x42 && forwarded == 2);
        return 0;
    }
    if (!strcmp(argv[1], "native-dispatch")) {
        BUPDATE date;
        use_sys_bup = 0;
        bup_get_date(0, &date);
        assert(date.year == 0 && date.month == 1 && date.day == 1 && date.week == 2);
        assert(bup_set_date(&date) == 0 && forwarded == 0);
        return 0;
    }
    if (!strcmp(argv[1], "week")) {
        for(unsigned i=0;i<256;i++) {
            BUPDATE input={0,1,1,0,0,i},saved=input;
            assert(sro_bup_set_date(&input)==0 && !memcmp(&input,&saved,sizeof(input)));
        }
        return 0;
    }
    if (!strcmp(argv[1], "compatible-fields")) {
        const u32 values[]={0,1,59,60,1439,1440,15*1440+12*60+34};
        for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) {
            BUPDATE actual;
            sro_bup_get_date(values[i],&actual);
            assert(!actual.year && actual.month==1 && actual.day==values[i]/1440+1);
            assert(actual.time==(values[i]%1440)/60 && actual.min==values[i]%60);
        }
        return 0;
    }
    FILE *fp = fopen(argv[1], "rb");
    assert(fp);
    unsigned char record[11];
    unsigned count = 0, failed = 0;
    while (fread(record, sizeof(record), 1, fp) == 1) {
        unsigned type = record[0];
        u32 stamp = (u32)record[1] << 24 | (u32)record[2] << 16 |
                    (u32)record[3] << 8 | record[4];
        BUPDATE expected, actual, input, saved;
        memcpy(&expected, record + 5, sizeof(expected));
        input = expected;
        /* Week is derived when unpacking and need not be initialized to pack. */
        input.week = 0xa5;
        saved = input;
        unsigned errors = 0;
        if (type != 2) {
            if (sro_bup_set_date(&input) != stamp) errors |= 1;
            if (memcmp(&input, &saved, sizeof(input))) errors |= 2;
        }
        if (type != 1) {
            memset(&actual, 0xa5, sizeof(actual));
            sro_bup_get_date(stamp, &actual);
            if (memcmp(&actual, &expected, sizeof(actual))) errors |= 4;
        }
        if (errors) {
            if (failed < 5)
                printf("record %u type %u stamp %08x: errors=%u\n", count, type, stamp, errors);
            ++failed;
        }
        ++count;
    }
    assert(feof(fp) && !ferror(fp) && fclose(fp) == 0 && count);
    printf("date cases=%u failures=%u\n", count, failed);
    return failed ? 1 : 0;
}
