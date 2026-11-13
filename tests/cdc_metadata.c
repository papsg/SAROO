/* MCU-CDC-005: adapted from paps-g 50100444, independent producer oracles. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"

static CDBLOCK cdb;
_Alignas(4) static u8 payload[MAX_BLOCKS][2352], disc_bytes[0x8000];
static u8 *sector_buffer=disc_bytes;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static u8 buf_subcode_mode;
#include "track_fixture.h"
TEST_INDEX_TABLE;
static u16 fifo_input[4*2352/2], fifo_status, st_control, st_status, hirq, reply[4];
static unsigned fifo_words,fifo_pos,prefix_bytes;
static int irq_depth,min_num;
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#undef FIFO_RCNT
#define SSCR1 reply[0]
#define SSCR2 reply[1]
#define SSCR3 reply[2]
#define SSCR4 reply[3]
#define FIFO_RCNT 0u
#undef HIRQ
#undef ST_CTRL
#undef ST_STAT
#undef FIFO_STAT
#undef FIFO_DATA
#define HIRQ hirq
#define ST_CTRL st_control
#define ST_STAT st_status
#define FIFO_STAT fifo_status
#define FIFO_DATA read_fifo()
#define SSLOG(...) ((void)0)
#define printk(...) ((void)0)
static int disable_irq(void) { return irq_depth++; }
static void restore_irq(int prior) { assert(irq_depth==prior+1);irq_depth=prior; }
static u16 read_fifo(void)
{
    assert(!irq_depth && fifo_pos<fifo_words && fifo_status);
    fifo_status--;return fifo_input[fifo_pos++];
}
static void disk_task_wakeup(void) { assert(0); }
static void fill_fifo(u8 *data,int size) { (void)data;(void)size;assert(0); }
static int fad_to_track(u32 fad) { (void)fad;assert(0);return 0; }
static void subcode_convert(void) { assert(0); }
static void fs_lock(void) { assert(0); }
static void fs_unlock(void) { assert(0); }
FRESULT f_lseek(FIL *file,FSIZE_t offset) { (void)file;(void)offset;assert(0);return FR_OK; }
FRESULT f_read(FIL *file,void *data,UINT count,UINT *actual)
{ (void)file;(void)data;(void)count;(void)actual;assert(0);return FR_OK; }
void *clear_prefix(void *to,int byte,size_t length)
{ assert(!irq_depth);prefix_bytes+=length;return memset(to,byte,length); }
#undef memset
#define memset clear_prefix
#include "metadata.inc"
#undef memset

static void setup(unsigned poison)
{
    memset(&cdb,0,sizeof cdb);memset(payload,poison,sizeof payload);
    for(unsigned i=0;i<MAX_BLOCKS;i++) {
        cdb.block[i].data=payload[i];cdb.block[i].size=-1;
        cdb.block[i].fad=0xdeadbeef;
        cdb.block[i].fn=cdb.block[i].cn=cdb.block[i].sm=cdb.block[i].ci=poison;
    }
    for(unsigned i=0;i<MAX_SELECTORS;i++) {
        cdb.filter[i].c_true=i;cdb.filter[i].c_false=0xff;
    }
    cdb.block_free=MAX_BLOCKS;cdb.sector_size=2048;cdb.status=STAT_PAUSE;
    fifo_words=fifo_pos=prefix_bytes=0;irq_depth=0;hirq=0;
}

static void disc_cases(int mode2_only)
{
    for(unsigned format=0;format<5;format++) {
        if(mode2_only && format!=2 && format!=3)continue;
        setup(0xa5);memset(disc_bytes,0x5a,sizeof disc_bytes);
        TRACK_INFO track={.fad_0=150,.fad_start=150,.fad_end=1000,.max_index=1};
        track.mode=format==4?3:1;track.sector_size=format==0?2048:2352;
        cdb.play_track=&track;cdb.tracks=&track;
        buf_fad_start=150;buf_fad_end=151;buf_fad_offset=0;
        buf_fad_size=track.sector_size;
        disc_bytes[15]=format>=2?2:1;
        disc_bytes[16]=7;disc_bytes[17]=9;disc_bytes[18]=format==3?0x28:8;disc_bytes[19]=0x11;
        u8 before[sizeof disc_bytes];memcpy(before,disc_bytes,sizeof before);
        BLOCK sector;memset(&sector,0xa5,sizeof sector);
        assert(get_sector(150,&sector)==0 && sector.fad==150);
        int mode2=format==2 || format==3;
        assert(sector.fn==(mode2?7:0) && sector.cn==(mode2?9:0));
        assert(sector.sm==(mode2?disc_bytes[18]:0) && sector.ci==(mode2?0x11:0));
        assert(sector.data==disc_bytes && sector.size==(int)track.sector_size);
        assert(!memcmp(before,disc_bytes,sizeof before) && !prefix_bytes && !irq_depth);
    }
}

/* Complete scripted input offered through <=2048-word FIFO windows.
 * Register occupancy is modeled; physical bus/IRQ timing is not. */
static void host_cases(int check_metadata,int headers)
{
    const unsigned sizes[]={2048,2336,2340,2352};
    const u8 stamps[][3]={{0x12,0x34,0x56},{0,0,0},{0xfa,0x9b,0xfe}};
    const u32 addresses[]={56606,0,727739};
    unsigned variants=headers?3:1;
    for(unsigned type=0;type<4;type++) for(unsigned v=0;v<variants;v++)
    for(unsigned raw_mode=0;raw_mode<(headers?3:1);raw_mode++) {
        unsigned size=sizes[type],offset=type==0?24:2352-size;
        for(unsigned pass=0;pass<2;pass++) {
            unsigned poison=pass?0x5a:0xa5;setup(poison);
            u8 expected[4][2352];
            for(unsigned n=0;n<4;n++) {
                for(unsigned j=0;j<2352;j++)expected[n][j]=(u8)(j*13+n);
                memcpy(expected[n]+12,stamps[v],3);expected[n][15]=headers?raw_mode:2;
                expected[n][16]=7+n;expected[n][17]=9+n;
                expected[n][18]=8;expected[n][19]=0x11;
                for(unsigned j=offset;j<offset+size;j+=2)
                    fifo_input[fifo_words++]=expected[n][j]|((u16)expected[n][j+1]<<8);
                memset(expected[n],0,offset);
                memset(expected[n]+offset+size,poison,2352-offset-size);
            }
            cdb.put_sector_size=size;unsigned turns=0;
            for(unsigned request=0;request<2;request++) {
                cdb.cr3=1<<8;cdb.cr4=request?1:3;
                assert(put_sector_data()==HIRQ_DRDY);
                unsigned limit=(request?4:3)*size/2;
                while(cdb.trans_bk) {
                    assert(++turns<20 && fifo_pos<limit);
                    unsigned available=limit-fifo_pos;
                    fifo_status=available>=2048?0x0800:available;
                    trans_handle();
                }
                assert(end_trans()==HIRQ_EHST);
            }
            assert(fifo_pos==fifo_words && cdb.block_free==196);
            assert(cdb.part[1].numblocks==4 && !irq_depth);
            BLOCK *b=cdb.part[1].head;
            for(unsigned n=0;n<4;n++,b=b->next) {
                assert(b && (b->size & ~BLOCK_RAW_MODE1)==(int)size);
                assert(!memcmp(b->data+offset,expected[n]+offset,size));
                if(check_metadata) {
                    unsigned mode2=type>=2 && (!headers || raw_mode==2);
                    assert(b->fad==(type>=2?addresses[v]:0));
                    assert(b->fn==(mode2?7+n:0) && b->cn==(mode2?9+n:0));
                    assert(b->sm==(mode2?8:0) && b->ci==(mode2?0x11:0));
                    assert(!memcmp(b->data,expected[n],2352));
                }
            }
            assert(!b);
            if(check_metadata)assert(prefix_bytes==4*offset);
            /* Metadata/payload oracles are retained from005. The dedicated PUT
             * fixture also checks ownership/publication and transfer counts. */
        }
    }
}

int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<5);
    if(test==0)disc_cases(0);
    else if(test==1)disc_cases(1);
    else host_cases(test!=3,test==4);
    puts("pass: actual producer metadata, payload, prefix and logical FAD controls");
    return 0;
}
