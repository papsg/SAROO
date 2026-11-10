/* paps-g 50100444: independent routes and real disc producer integration. */
#define main copy_fixture_main
#include "cdc_copy.c"
#undef main

static u8 disc_bytes[0x8000];
static u8 *sector_buffer=disc_bytes;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static int fad_to_track(u32 fad) { (void)fad;assert(0);return 0; }
static void subcode_convert(void) { assert(0); }
static void fs_lock(void) { assert(0); }
static void fs_unlock(void) { assert(0); }
FRESULT f_lseek(FIL *file,FSIZE_t offset) { (void)file;(void)offset;assert(0);return FR_OK; }
FRESULT f_read(FIL *file,void *data,UINT count,UINT *actual)
{ (void)file;(void)data;(void)count;(void)actual;assert(0);return FR_OK; }
#define memcpy32 counted_memcpy
#include "routing.inc"

static int expected_route(unsigned id,unsigned back)
{
    if(id==1)return 3;
    if(id>=2 && id<=4)return back?0:4;
    return -1;
}
static void routed_shapes(void)
{
    for(unsigned moving=0;moving<2;moving++)for(unsigned back=0;back<2;back++)
    for(unsigned start=0;start<8;start++)for(unsigned count=1;count<=8-start;count++) {
        setup(8,moving?0:8);cdb.last_buffer=19;
        cdb.cddev_filter=5;cdb.filter[11].c_false=5;cdb.filter[12].c_false=13;
        cdb.filter[5]=(FILTER){.mode=1,.fid=2,.c_true=3,.c_false=6};
        cdb.filter[6]=(FILTER){.mode=0x40,.fad=152,.range=3,.c_true=back?0:4,.c_false=7};
        cdb.filter[7].c_true=0xff;
        command(moving,0,5,start,count);assert(execute(moving)==HIRQ_ECPY);
        assert(cdb.cddev_filter==0xff && cdb.filter[11].c_false==0xff);
        assert(cdb.filter[12].c_false==13 && cdb.last_buffer==19);
        unsigned expected[5][16]={{0}},length[5]={0},stored=0;
        for(unsigned i=0;i<8;i++)if(!moving || i<start || i>=start+count)
            expected[0][length[0]++]=i;
        for(unsigned i=start;i<start+count;i++) {
            int dst=expected_route(i,back);
            if(dst>=0) { expected[dst][length[dst]++]=i;stored++; }
        }
        for(unsigned p=0;p<5;p++)if(p!=2)check_list(p,expected[p],length[p],moving);
        assert(copied_bytes==(moving?0:stored*2352));
        assert(allocation_count==(moving?0:stored) && !log_count);
        assert(cdb.block_free==(int)(moving?count-stored:8-stored));
        check_owners();check_reply(0);
    }
}
static void discard_and_admission(void)
{
    for(unsigned moving=0;moving<2;moving++) {
        setup(8,moving?0:8);cdb.filter[5].c_true=0xff;cdb.last_buffer=19;
        command(moving,0,5,0,8);assert(execute(moving)==HIRQ_ECPY);
        assert(!copied_bytes && !allocation_count && cdb.block_free==8 && cdb.last_buffer==19);
        assert(cdb.part[0].numblocks==(moving?0:8));check_owners();
    }
    setup(8,8);cdb.filter[5]=(FILTER){.mode=1,.fid=0,.c_true=5,.c_false=0xff};
    command(0,0,5,0,8);assert(execute(0)==HIRQ_ECPY);
    assert(!copied_bytes && !allocation_count && !log_count);check_owners();
    setup(8,0);cdb.filter[5].c_true=0xff;cdb.last_buffer=19;
    command(0,0,5,0,8);CDBLOCK before;memcpy(&before,&cdb,sizeof cdb);
    assert(!execute(0) && (reply[0]>>8)==(STAT_WAIT|STAT_PAUSE));
    assert(!memcmp(&before,&cdb,sizeof cdb) && !copied_bytes);
}
static void disc_metadata(void)
{
    for(unsigned format=0;format<5;format++) {
        setup(0,200);memset(disc_bytes,0x5a,sizeof disc_bytes);
        TRACK_INFO track={.fad_0=150,.fad_end=1000};
        track.mode=format==4?3:1;track.sector_size=format==0?2048:2352;
        cdb.tracks=&track;cdb.play_track=&track;
        buf_fad_start=150;buf_fad_end=151;buf_fad_offset=0;buf_fad_size=track.sector_size;
        disc_bytes[15]=format>=2?2:1;
        disc_bytes[16]=7;disc_bytes[17]=9;disc_bytes[18]=format==3?0x28:8;disc_bytes[19]=0x11;
        BLOCK sector;memset(&sector,0xa5,sizeof sector);
        assert(!get_sector(150,&sector) && sector.fad==150);
        int mode2=format==2 || format==3;
        assert(sector.fn==(mode2?7:0) && sector.cn==(mode2?9:0));
        cdb.cddev_filter=0;cdb.sector_size=2048;
        assert(!filter_sector(&track,&sector));BLOCK *source=cdb.part[0].head;
        cdb.filter[5]=(FILTER){.mode=0x41,.fad=150,.range=1,.fid=sector.fn,.c_true=7,.c_false=0xff};
        command(0,0,5,0,1);assert(execute(0)==HIRQ_ECPY);
        BLOCK *copy=cdb.part[7].head;assert(copy && copy!=source && copy->fad==150);
        assert(copy->fn==source->fn && copy->size==source->size);
        assert(!memcmp(copy->data,source->data,2352));check_owners();
        if(!mode2) {
            cdb.filter[5].fid=1;command(0,0,5,0,1);assert(execute(0)==HIRQ_ECPY);
            assert(cdb.part[7].numblocks==1);check_owners();
        }
    }
}
static void compare_predicate(BLOCK *block,int expected)
{
#ifdef TEST_HAS_SHARED_ROUTE
    assert(sector_destination(5,block,0)==expected);
#else
    TRACK_INFO track={.mode=1,.sector_size=2352};block->data[15]=2;
    cdb.cddev_filter=5;int result=filter_sector(&track,block);
    assert(result==0 || result==1);
    assert((result==0?cdb.last_buffer:0xff)==expected);
    if(!result)remove_block(&cdb.part[7],0);
#endif
}
static void predicates(void)
{
    setup(1,199);BLOCK *b=&cdb.block[0];
    for(unsigned mode=0;mode<128;mode++)for(unsigned bad=0;bad<32;bad++) {
        cdb.filter[5]=(FILTER){.mode=mode,.fid=b->fn+!!(bad&1),.chan=b->cn+!!(bad&2),
            .smmask=0xff,.smval=b->sm+!!(bad&4),.cimask=0xff,.cival=b->ci+!!(bad&8),
            .fad=b->fad+!!(bad&16),.range=1,.c_true=7,.c_false=0xff};
        int match=(mode&bad&15)==0;if(mode&16)match=!match;
        if((mode&64) && (bad&16))match=0;
        compare_predicate(b,match?7:0xff);
    }
}
static void normalized_non_mode2(void)
{
    for(unsigned format=0;format<3;format++) {
        setup(0,200);TRACK_INFO track={.mode=format==2?3:1,.sector_size=format?2352:2048};
        u8 data[2352]={0};data[15]=format==2?2:1;
        BLOCK b={.data=data,.size=track.sector_size,.fad=150};
        cdb.cddev_filter=5;
        for(unsigned field=0;field<4;field++) {
            FILTER *f=&cdb.filter[5];*f=(FILTER){.mode=1u<<field,.fid=1,.chan=1,
                .smmask=1,.smval=1,.cimask=1,.cival=1,.c_true=7,.c_false=0xff};
            assert(filter_sector(&track,&b)==1 && cdb.last_buffer==0xff);
            assert(cdb.block_free==200 && !copied_bytes);check_owners();
        }
    }
}
static void range_representation(void)
{
    setup(1,199);BLOCK *b=&cdb.block[0];
    const u32 addresses[]={149,150,159,160,0xfffffffcu};
    for(unsigned i=0;i<5;i++) {
        cdb.filter[5]=(FILTER){.mode=0x40,.fad=i==4?0xfffffff5u:150,
            .range=i==4?32:10,.c_true=7,.c_false=0xff};
        b->fad=addresses[i];compare_predicate(b,i==1 || i==2 || i==4?7:0xff);
    }
    cdb.filter[5].range=0;compare_predicate(b,0xff);
    /* Last case tests full-width cached arithmetic, not a physical24-bit packet. */
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<6);
    if(test==0)routed_shapes();else if(test==1)discard_and_admission();
    else if(test==2)disc_metadata();else if(test==3)predicates();
    else if(test==4)normalized_non_mode2();else range_representation();
    puts("pass: shared routing, original selection, metadata, discard and ownership");return 0;
}
