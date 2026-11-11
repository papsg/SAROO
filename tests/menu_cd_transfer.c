/* Adapted from paps-g 4f7cd16b. Actual menu code with a modeled CD FIFO. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u16 cr1, cr2, cr3, cr4; } CDCMD;
typedef struct { int fad, size; u8 unit, gap, fn, attr; } CdcFile;
enum { HIRQ_CMOK=1, HIRQ_DRDY=2, HIRQ_ESEL=0x40,
       STATUS_OPEN=6, STATUS_NODISC=7, STATUS_RETRY=8, STATUS_ERROR=9, STATUS_FATAL=10 };
int cdc_end_trans(int *words);

#ifdef TEST_COMMAND
static unsigned CR1, CR2, CR3, CR4, HIRQ;
static int first_timeout, ready_timeout, cleanup_timeout;
static unsigned reply_status, calls, ready_calls;
static unsigned direct_end, reply_words, late_ready;
static void clear_hirq(int mask) { HIRQ &= ~mask; }
static void printk(const char *format, ...) { (void)format; }
static int wait_hirq(int mask)
{
    if(mask != HIRQ_CMOK) {
        ++ready_calls;
        assert(mask == HIRQ_DRDY || mask == HIRQ_ESEL);
        return ready_timeout;
    }
    ++calls;
    if(calls == 1) {
        assert(CR1 == (direct_end ? 0x0600u : 0x6300u) && !CR2 && !CR3 &&
               CR4 == (direct_end ? 0u : 1u));
        if(first_timeout) return first_timeout;
        CR1 = reply_status | (direct_end ? reply_words >> 16 : 0);
        CR2 = direct_end ? reply_words & 0xffff : 123;
        CR3 = CR4 = 0;
    } else {
        assert(calls == 2 && CR1 == 0x0600 && !CR2 && !CR3 && !CR4);
        if (late_ready) HIRQ |= HIRQ_DRDY;
        if(cleanup_timeout) return cleanup_timeout;
        CR1 = 0; CR2 = 1024;
    }
    HIRQ |= HIRQ_CMOK;
    return 0;
}
#include "commands.inc"

static int command(int wait)
{
    CDCMD request={0x6300,0,0,1}, response={0};
    calls=ready_calls=0; HIRQ=0xffff;
    return cdc_cmd(wait, &request, &response, "fixture");
}

static void test_end_errors(void)
{
    const int errors[] = {-3, -5, -6};
    direct_end = 1;
    for (unsigned failure = 0; failure < 3; ++failure) {
        for (unsigned output = 0; output < 2; ++output) {
            const unsigned initial = 0xa537;
            HIRQ = initial;
            calls = ready_calls = 0;
            first_timeout = failure == 0 ? -3 : 0;
            reply_status = failure == 1 ? 0xff00 : 0x8100;
            int words = 0x12345678;
            assert(cdc_end_trans(output ? &words : NULL) == errors[failure]);
            assert(words == 0x12345678 && calls == 1 && !ready_calls);
            assert(HIRQ == (first_timeout ? initial & ~HIRQ_CMOK : initial));
        }
    }
}

static void test_end_success(void)
{
    const unsigned counts[] = {0, 1, 1024, 0xffff, 0x10000, 0x7fffff, 0xfffffe, 0xffffff};
    direct_end = 1;
    reply_status = 0x0100;
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; ++i) {
        for (unsigned output = 0; output < 2; ++output) {
            for (unsigned ready = 0; ready < 2; ++ready) {
                const unsigned initial = 0xa535;
                HIRQ = initial | (ready ? HIRQ_DRDY : 0);
                calls = ready_calls = 0;
                reply_words = counts[i];
                int words = -1;
                assert(cdc_end_trans(output ? &words : NULL) == 0);
                assert(words == (output ? (int)counts[i] : -1));
                assert(HIRQ == initial && calls == 1 && !ready_calls);
            }
        }
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if(!strcmp(argv[1], "end-errors")) {
        test_end_errors();
    } else if(!strcmp(argv[1], "end-success")) {
        test_end_success();
    } else if(!strcmp(argv[1], "cleanup")) {
        for(unsigned failure=0; failure<2; ++failure) {
            first_timeout=0; ready_timeout=-3; cleanup_timeout=failure ? -3 : 0;
            reply_status=0x4100;
            assert(command(HIRQ_DRDY) == -3);
            assert(calls == 2 && ready_calls == 1 && !(HIRQ & HIRQ_DRDY));
        }
    } else if(!strcmp(argv[1], "late-ready")) {
        late_ready = 1;
        ready_timeout = -3;
        reply_status = 0x4100;
        for (unsigned failure = 0; failure < 2; ++failure) {
            cleanup_timeout = failure ? -3 : 0;
            assert(command(HIRQ_DRDY) == -3);
            assert(calls == 2 && ready_calls == 1);
            assert(!!(HIRQ & HIRQ_DRDY) == failure);
        }
    } else {
        assert(!strcmp(argv[1], "ordinary"));
        first_timeout=-3;
        assert(command(HIRQ_DRDY) == -3 && calls == 1 && !ready_calls);
        first_timeout=0; reply_status=0xff00;
        assert(command(HIRQ_DRDY) == -5 && calls == 1 && !ready_calls);
        reply_status=0x8100;
        assert(command(HIRQ_DRDY) == -6 && calls == 1 && !ready_calls);
        reply_status=0x0100; ready_timeout=-3;
        assert(command(HIRQ_ESEL) == -3 && calls == 1 && ready_calls == 1);
        assert(command(HIRQ_DRDY) == -3 && calls == 1 && ready_calls == 1);
        reply_status=0x4100;
        ready_timeout=0;
        assert(!command(HIRQ_DRDY) && calls == 1 && ready_calls == 1);
    }
    return 0;
}
#else
static _Alignas(8) u8 stream[0x80000], hook_ram[0x54f00], hook_game[0x54f00];
static u32 hook_install[2];
static unsigned fifo_pos, fifo_limit, read_words, long_reads;
static struct step { unsigned cmd; int result; u32 words; unsigned offset, extent, status; } steps[4096];
static unsigned used, next;
static unsigned play_fad=2612, play_sectors;
static unsigned CR1;

static void reset(void)
{
    used=next=fifo_pos=read_words=long_reads=0;
    CR1=0;
    fifo_limit=sizeof stream;
    for(unsigned i=0;i<sizeof stream;++i) stream[i]=(i*73+19)%251;
}
static void expect(unsigned cmd, int result, unsigned words)
{
    assert(used < sizeof steps / sizeof *steps);
    steps[used++]=(struct step){cmd,result,words,0,0,0};
}
static int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *name)
{
    (void)name;
    assert(next < used);
    struct step *step=&steps[next++];
    assert(cmd->cr1 == step->cmd);
    assert(wait == ((step->cmd==0x6300 || step->cmd==0x7300) ? HIRQ_DRDY :
                    (step->cmd==0x6000 || step->cmd==0x4800 || step->cmd==0x3000) ? HIRQ_ESEL : 0));
    if((cmd->cr1 & 0xff00) == 0x1000) {
        assert(cmd->cr2 == (play_fad & 0xffff));
        assert((((unsigned)(cmd->cr3 & 0x7f)<<16)|cmd->cr4) == play_sectors);
    }
    if(cmd->cr1 == 0x6300) {
        assert(!cmd->cr2 && !cmd->cr3 && cmd->cr4 == 1);
        fifo_pos=step->offset; fifo_limit=step->offset+step->extent;
    }
    CR1=step->status<<8;
    *resp=(CDCMD){CR1,step->words,0,0};
    if(cmd->cr1 == 0x5100) resp->cr4=step->words;
    if(cmd->cr1 == 0x0600) resp->cr1=(step->words>>16)&0xff;
    return step->result;
}
static void clear_hirq(int mask) { (void)mask; }
static u32 read32(void)
{
    assert(fifo_pos+4 <= fifo_limit);
    /* Native scalar value yields the same byte stream on this host as the
     * SH-2's big-endian longword load/store pair. Halfword tails use wire order. */
    u32 value; memcpy(&value,stream+fifo_pos,4);
    fifo_pos+=4; read_words+=2; ++long_reads;
    return value;
}
static u16 __attribute__((unused)) read16(void)
{
    assert(fifo_pos+2 <= fifo_limit);
    u16 value=(stream[fifo_pos]<<8)|stream[fifo_pos+1];
    fifo_pos+=2; ++read_words;
    return value;
}
#define REG(addr) (assert((addr)==0x25818000), read32())
#define REG16(addr) (assert((addr)==0x25898000), read16())
#include "transfers.inc"

static void test_copy(void)
{
    const unsigned sizes[]={0,1,2,3,4,5,7,8,9,11,12,13,63,64,65,2047,2048,2049,32768};
    for(unsigned alignment=0;alignment<4;++alignment) for(unsigned k=0;k<sizeof sizes/sizeof *sizes;++k) {
        unsigned length=sizes[k], prefix=32+alignment;
        u8 *buffer=malloc(prefix+length+32); assert(buffer);
        memset(buffer,0xcc,prefix+length+32); reset();
        fifo_limit=(length+1)&~1u;
        cdc_trans_data(buffer+prefix,length);
        assert(!memcmp(buffer+prefix,stream,length));
        for(unsigned i=0;i<prefix;++i) assert(buffer[i]==0xcc);
        for(unsigned i=0;i<32;++i) assert(buffer[prefix+length+i]==0xcc);
        assert(read_words==(length+1)/2);
        assert(long_reads==(alignment ? 0 : length/4));
        free(buffer);
    }
    reset(); cdc_trans_data(NULL,0); cdc_trans_data(NULL,-1);
    assert(!read_words);
}

static void setup(unsigned length, int failure, int error)
{
    const unsigned commands[]={0x6000,0x4800,0x3000,0x1080|(play_fad>>16)};
    play_sectors=length ? (length-1)/2048+1 : 0;
    for(unsigned i=0;i<4;++i) {
        expect(commands[i],(int)i==failure ? error : 0,0);
        if((int)i==failure) return;
    }
}
static void sector(unsigned index, unsigned bytes, int failure, int error, unsigned reported_words)
{
    expect(0x5100, failure==4 ? error : 0,1);
    if(failure==4) return;
    expect(0x6300, failure==5 ? error : 0,0);
    steps[used-1].offset=index*bytes; steps[used-1].extent=bytes;
    if(failure==5) return;
    expect(0x0600, failure==6 ? error : 0,reported_words);
}
static void read_script(unsigned size, unsigned wire_bytes)
{
    setup(size,-1,0);
    for(unsigned i=0;i<(size+2047)/2048;++i) sector(i,wire_bytes,-1,0,wire_bytes/2);
}
static void test_read(void)
{
    const unsigned sizes[]={405,1,2,3,2047,2048,2049,4097,32768};
    for(unsigned alignment=0;alignment<4;++alignment) for(unsigned form=0;form<2;++form)
    for(unsigned k=0;k<sizeof sizes/sizeof *sizes;++k) {
        unsigned size=sizes[k],prefix=32+alignment,wire=form ? 2324 : 2048;
        u8 *buffer=malloc(prefix+size+32); assert(buffer);
        memset(buffer,0xcc,prefix+size+32);reset();read_script(size,wire);
        assert(cdc_read_sector(play_fad,size,buffer+prefix)==(int)size);
        assert(next==used);
        for(unsigned i=0;i<size;++i) assert(buffer[prefix+i]==stream[(i/2048)*wire+i%2048]);
        for(unsigned i=0;i<prefix;++i) assert(buffer[i]==0xcc);
        for(unsigned i=0;i<32;++i) assert(buffer[prefix+size+i]==0xcc);
        assert(read_words==(size+1)/2);free(buffer);
    }
}
static void test_errors(void)
{
    const int errors[]={-3,-5,-6};
    for(int stage=0;stage<7;++stage) for(unsigned e=0;e<3;++e) {
        u8 buffer[32];memset(buffer,0xcc,sizeof buffer);reset();setup(3,stage,errors[e]);
        if(stage>=4) sector(0,2048,stage,errors[e],1024);
        assert(cdc_read_sector(play_fad,3,buffer)==errors[e]);
        assert(next==used && read_words==(stage==6 ? 2u : 0u));
        if(stage!=6) for(unsigned i=0;i<sizeof buffer;++i) assert(buffer[i]==0xcc);
    }
}
static void test_counts(void)
{
    const unsigned counts[]={0,1,1023,1025,1161,1163,65535,0x010400,0xffffff};
    u8 buffer[32];
    for(unsigned i=0;i<sizeof counts/sizeof *counts;++i) {
        reset();setup(3,-1,0);sector(0,2048,-1,0,counts[i]);
        assert(cdc_read_sector(play_fad,3,buffer)==-1 && next==used);
    }
}
static void test_admission(void)
{
    u8 buffer[4]={0};reset();
    assert(cdc_read_sector(play_fad,0,NULL)==0 && !next);
    assert(cdc_read_sector(play_fad,-1,buffer)<0 && !next);
    assert(cdc_read_sector(play_fad,1,NULL)<0 && !next);
    setup(0x7fffffff,3,-5);
    assert(cdc_read_sector(play_fad,0x7fffffff,buffer)==-5 && next==used && !read_words);
}
static void test_files(void)
{
    _Static_assert(sizeof(CdcFile)==12,"wire record extent");
    _Alignas(8) u8 buffer[254*12+32];
    for(unsigned all=0;all<2;++all) for(unsigned words=0;words<65536;++words) {
        /* Avoid refilling an unrelated 512 KiB stream for every malformed reply. */
        used=next=fifo_pos=read_words=long_reads=0;fifo_limit=sizeof stream;
        memset(buffer,0xcc,sizeof buffer);
        expect(0x7300,0,words);expect(0x0600,0,words);
        int valid=all ? words<=254*6 && words%6==0 : words==6;
        assert(cdc_get_file_info(all ? 0xffffff : 17,(CdcFile*)buffer)==(valid ? 0 : -1));
        assert(next==used && read_words==(valid ? words : 0));
        if(valid) assert(!memcmp(buffer,stream,words*2));
        for(unsigned i=valid ? words*2 : 0;i<sizeof buffer;++i) assert(buffer[i]==0xcc);
    }
    const int errors[]={-3,-5,-6};
    for(unsigned e=0;e<3;++e) {
        reset();expect(0x7300,errors[e],6);
        assert(cdc_get_file_info(17,(CdcFile*)buffer)==errors[e] && next==used && !read_words);
        reset();expect(0x7300,0,6);expect(0x0600,errors[e],6);
        assert(cdc_get_file_info(17,(CdcFile*)buffer)==errors[e] && next==used);
        reset();expect(0x7300,0,7);expect(0x0600,errors[e],6);
        assert(cdc_get_file_info(17,(CdcFile*)buffer)==-1 && next==used && !read_words);
    }
    reset();assert(cdc_get_file_info(17,NULL)==-1 && !next);
#ifdef HAS_FILE_INFO_COUNT
    for(unsigned count=0;count<=254;++count) {
        reset();expect(0x7300,0,count*6);expect(0x0600,0,count*6);
        assert(cdc_file_info(-1,(CdcFile*)buffer)==(int)count && next==used);
    }
#endif
}
static void test_hooks(void)
{
    memset(hook_ram,0xa5,sizeof hook_ram);memset(hook_game,0xcc,sizeof hook_game);
    KAITEI_DAISENSOU_handle2();
    for(unsigned i=0;i<sizeof hook_game;++i) assert(hook_game[i]==0xcc);
    reset();read_script(sizeof hook_ram,2048);KAITEI_DAISENSOU_handle1();
    assert(next==used);KAITEI_DAISENSOU_handle2();
    assert(!memcmp(hook_game,stream,sizeof hook_game));
    /* A successfully cached preload remains reusable within this session. */
    memset(hook_game,0xcc,sizeof hook_game);KAITEI_DAISENSOU_handle2();
    assert(!memcmp(hook_game,stream,sizeof hook_game));
    KAITEI_DAISENSOU_patch();
    assert(hook_install[0]==(u32)(uintptr_t)KAITEI_DAISENSOU_handle1);
    assert(hook_install[1]==(u32)(uintptr_t)KAITEI_DAISENSOU_handle2);
    memset(hook_game,0xcc,sizeof hook_game);KAITEI_DAISENSOU_handle2();
    for(unsigned i=0;i<sizeof hook_game;++i) assert(hook_game[i]==0xcc);
    reset();read_script(sizeof hook_ram,2048);KAITEI_DAISENSOU_handle1();
    assert(next==used);
    for(int stage=0;stage<7;++stage) {
        memset(hook_game,0xcc,sizeof hook_game);reset();setup(sizeof hook_ram,stage,-3);
        if(stage>=4) sector(0,2048,stage,-3,1024);
        KAITEI_DAISENSOU_handle1();assert(next==used);KAITEI_DAISENSOU_handle2();
        for(unsigned i=0;i<sizeof hook_game;++i) assert(hook_game[i]==0xcc);
    }
}
static void test_terminal_read(void)
{
    _Alignas(8) u8 buffer[2049 + 32];
    for (unsigned flags = 0; flags < 0x80; flags += 0x10) {
        for (unsigned status = 0; status < 16; ++status) {
            int terminal = status == STATUS_OPEN || status == STATUS_NODISC ||
                           status == STATUS_ERROR || status == STATUS_FATAL;
            for (unsigned prefix = 0; prefix < 2; ++prefix) {
                unsigned length = prefix * 2048 + 1;
                reset();
                memset(buffer, 0xcc, sizeof buffer);
                setup(length, -1, 0);
                if (prefix) sector(0, 2048, -1, 0, 1024);
                expect(0x5100, 0, 0);
                steps[used - 1].status = flags | status;
                if (!terminal) sector(prefix, 2048, -1, 0, 1024);
                assert(cdc_read_sector(play_fad, length, buffer) == (terminal ? -1 : (int)length));
                unsigned copied = terminal ? prefix * 2048 : length;
                assert(next == used && read_words == (copied + 1) / 2);
                assert(!memcmp(buffer, stream, copied));
                for (unsigned i = copied; i < sizeof buffer; ++i)
                    assert(buffer[i] == 0xcc);
            }
        }
    }

    for (unsigned status = STATUS_OPEN; status <= STATUS_FATAL; ++status) {
        if (status == STATUS_RETRY) continue;
        reset();
        memset(buffer, 0xcc, sizeof buffer);
        setup(1, -1, 0);
        unsigned query = used;
        sector(0, 2048, -1, 0, 1024);
        steps[query].status = status;
        assert(cdc_read_sector(play_fad, 1, buffer) == 1 && next == used);
        assert(read_words == 1 && buffer[0] == stream[0] && buffer[1] == 0xcc);
    }
}

int main(int argc,char **argv)
{
    assert(argc==2);reset();
    if(!strcmp(argv[1],"copy")) test_copy();
    else if(!strcmp(argv[1],"read")) test_read();
    else if(!strcmp(argv[1],"errors")) test_errors();
    else if(!strcmp(argv[1],"counts")) test_counts();
    else if(!strcmp(argv[1],"admission")) test_admission();
    else if(!strcmp(argv[1],"files")) test_files();
    else if(!strcmp(argv[1],"hooks")) test_hooks();
    else if(!strcmp(argv[1],"terminal")) test_terminal_read();
    else assert(0);
    return 0;
}
#endif
