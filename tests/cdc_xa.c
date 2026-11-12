/* Selected MCU-CDC-013: paps-g 05852d90; independent iterative/64-bit oracles. */
static void file_hirq(unsigned mask);
#include "directory-harness.inc"
#include "advance.inc"

static unsigned file_events, efls_writes, pend_writes, bful_writes;
static int pend_delay;
static int hw_delay(int delay) { (void)delay; return 0; }
static void file_hirq(unsigned mask)
{
    hirq = mask;
    file_events |= mask;
    efls_writes += !!(mask & HIRQ_EFLS);
    pend_writes += !!(mask & HIRQ_PEND);
    bful_writes += !!(mask & HIRQ_BFUL);
}
#include "file-end.inc"

static unsigned xa_record(u8 *p,unsigned name,unsigned su,unsigned flags,unsigned xa_flags,unsigned fn)
{
    unsigned base=33+(name|1),length=base+su;
    assert(name && length<=255);memset(p,0,length);
    p[0]=length;p[25]=flags;p[32]=name;memset(p+33,'N',name);
    le32(p+2,100);le32(p+10,2048);
    if(su>=8){p[base+4]=xa_flags;p[base+6]='X';p[base+7]='A';}
    if(su>=9)p[base+8]=fn;
    return length;
}
static void metadata_bounds(void)
{
    for(unsigned name=1;name<=207;name++)for(unsigned su=0;su<=16;su++){
        unsigned length=33+(name|1)+su;if(length>255)continue;
        u8 *p=malloc(length);assert(p);xa_record(p,name,su,0xff,0x98,37);
        FILE_INFO fi={.lba=999};int n=read_directory_record(&fi,p,length);
        if(su>=8 && su<14)assert(!n && fi.lba==999);
        else {
            assert(n==(int)length && fi.lba==100 && fi.size==2048);
            assert(fi.fid==(su>=14?37:0) && fi.attr==(su>=14?0x9b:2));
        }
        if(su>=8){p[33+(name|1)+6]='Q';assert(read_directory_record(&fi,p,length)==(int)length && fi.fid==0 && fi.attr==2);}
        free(p);
    }
    for(unsigned unit=0;unit<256;unit++)for(unsigned gap=0;gap<256;gap++){
        u8 p[48];xa_record(p,1,14,0,0x28,37);p[26]=unit;p[27]=gap;p[1]=unit?unit:1;
        FILE_INFO fi={.lba=999};int n=read_directory_record(&fi,p,48);
        if(!unit && gap)assert(!n && fi.lba==999);
        else assert(n==48 && fi.lba==100+p[1]+(unit?gap:0));
        if(unit>1){p[1]=1;assert(!read_directory_record(&fi,p,48));}
    }
    u8 p[48];xa_record(p,1,14,0,0x28,37);p[1]=255;p[26]=255;p[27]=255;
    le32(p+2,0xffffffff-509);FILE_INFO fi={.lba=999};assert(!read_directory_record(&fi,p,48) && fi.lba==999);
}
static void attributes(void)
{
    for(unsigned iso=0;iso<256;iso++)for(unsigned xa=0;xa<256;xa++){
        u8 p[48];xa_record(p,1,14,iso,xa,iso^xa);FILE_INFO fi;
        assert(read_directory_record(&fi,p,48)==48);
        assert(fi.attr==((iso&2)|(xa&0xf8)|1) && fi.fid==(iso^xa));
        setup(0,200);cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;cdb.fi[2]=fi;legacy_file_info_wire(finfo);
        cdb.cr3=0;cdb.cr4=2;fifo_words=0;fifo_status=0;assert(get_file_info()==HIRQ_DRDY);
        const u8 expected[12]={0,0,0,250,0,0,8,0,0,0,iso^xa,(iso&2)|(xa&0xf8)};
        assert(fifo_words == 6);
        check_file_words(0, expected);
        while(!cdb.trans_finish)trans_handle();fifo_status=0;assert(end_trans()==HIRQ_EHST);
    }
}
static void directory_flags(void)
{
    for(unsigned xa=0;xa<2;xa++)for(unsigned iso=0;iso<2;iso++)for(unsigned dir=0;dir<2;dir++){
        setup(0,200);file_pause_hook=NULL;
        u8 p[48];unsigned len=xa_record(p,1,xa?14:0,iso?2:0,dir?0x80:0,0);
        assert(read_directory_record(&cdb.fi[2],p,len)==(int)len);
        cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;cdb.cr3=0;cdb.cr4=2;
        assert(change_dir()==0);
        assert((cdb.play_type==PLAYTYPE_DIR)==(xa?dir:iso));
    }
    for(unsigned valid=0;valid<2;valid++){
        ready(0,2048,2);memset(sector,0,2048);
        xa_record(sector,1,14,2,valid?0x80:0,0);sector[33]=0;
        xa_record(sector+48,1,14,0,0x80,0);sector[81]=1;
        assert(feed()==0 && cdb.rdir_state==(valid?3:0));
    }
}
static int start_file(unsigned unit,unsigned gap,u32 bytes,u32 offset,u32 lba,unsigned sel,unsigned fn)
{
    setup(0,200);file_pause_hook=NULL;cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
    cdb.fi[2]=(FILE_INFO){.lba=lba,.size=bytes,.unit=unit,.gap=gap,.fid=fn};
    cdb.cr1=offset>>16;cdb.cr2=offset;cdb.cr3=sel<<8;cdb.cr4=2;
    assert(read_file()==0);return cdb.play_type==PLAYTYPE_FILE;
}
static void geometry(void)
{
    for(unsigned unit=1;unit<256;unit++)for(unsigned gap=0;gap<256;gap++){
        unsigned total=unit+3,physical[259],pos=250,in_unit=0;
        for(unsigned i=0;i<total;i++){
            physical[i]=pos++;
            if(++in_unit==unit){pos+=gap;in_unit=0;}
        }
        unsigned offsets[]={0,1,unit-1,unit,total-1};
        for(unsigned k=0;k<5;k++){
            unsigned off=offsets[k],sel=(unit+gap+k)%24;
            assert(start_file(unit,gap,total*2048,off,100,sel,37));
            assert(cdb.fad==physical[off] && cdb.play_fad_end==physical[total-1]+1);
            assert(cdb.filter[sel].fad==physical[off] && cdb.filter[sel].range==physical[total-1]+1-physical[off]);
            for(unsigned i=off;i<total;i++){
                assert(cdb.fad==physical[i]);advance();
            }
            assert(cdb.fad==physical[total-1]+1);
        }
    }
}
static void wide_extents(void)
{
    const u32 sizes[]={0,1,2048,2049,0x7fffffff,0xfffff800,0xffffffff};
    const unsigned units[]={0,1,2,255},gaps[]={0,1,255};
    const u32 lbas[]={0,100,0xffffff-150,0xffffff-151,0xffffffff};
    for(unsigned n=0;n<7;n++)for(unsigned u=0;u<4;u++)for(unsigned g=0;g<3;g++)for(unsigned a=0;a<5;a++){
        uint64_t count=((uint64_t)sizes[n]+2047)/2048;
        u32 offsets[]={0,count?count-1:0,count,0xffffff};
        uint64_t span=count?count+(units[u]?(count-1)/units[u]*gaps[g]:0):0;
        for(unsigned o=0;o<4;o++){
            int valid=count && offsets[o]<count && (units[u] || !gaps[g]) && (uint64_t)lbas[a]+150+span<=0x1000000;
            setup(1,199);file_pause_hook=NULL;cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
            cdb.fi[2]=(FILE_INFO){.lba=lbas[a],.size=sizes[n],.unit=units[u],.gap=gaps[g]};
            cdb.cr1=offsets[o]>>16;cdb.cr2=offsets[o];cdb.cr3=0;cdb.cr4=2;CDBLOCK before=cdb;
            assert(!read_file());
            if(valid)assert(cdb.play_type==PLAYTYPE_FILE && cdb.play_fad_end==lbas[a]+150+span);
            else assert(reply[0]==STAT_REJECT<<8 && !memcmp(&before,&cdb,sizeof cdb));
            check_owners();
        }
    }
}
static void file_number(void)
{
    for(unsigned fn=0;fn<256;fn++){
        assert(start_file(0,0,2048,0,100,7,37));memset(sector,fn,2048);
        BLOCK w={.data=sector,.size=2048,.fad=250,.fn=fn};cdb.play_track=&directory_track;
        assert(dispatch(w)==1);assert(cdb.part[7].numblocks==(fn==37));
        if(fn==37)assert(!memcmp(cdb.part[7].head->data+24,sector,2048));
        check_owners();
    }
    assert(start_file(2,3,4*2048,0,100,7,37));
    const u32 expected[]={250,251,255,256};
    for(unsigned i=0;i<4;i++) {
        assert(cdb.fad==expected[i]);memset(sector,10+i,sizeof sector);
        BLOCK w={.data=sector,.size=2048,.fad=cdb.fad,.fn=i&1?38:37};
        cdb.play_track=&directory_track;assert(dispatch(w)==1);advance();check_owners();
    }
    assert(cdb.fad==257 && cdb.part[7].numblocks==2 && copied_bytes==4096 && allocation_count==2);
    assert(cdb.part[7].head->fad==250 && cdb.part[7].tail->fad==255);
    for(unsigned i=0;i<2048;i++) {
        assert(cdb.part[7].head->data[24+i]==10 && cdb.part[7].tail->data[24+i]==12);
    }
    for(unsigned fad=249;fad<=251;fad+=2){
        assert(start_file(0,0,2048,0,100,7,37));BLOCK w={.data=sector,.size=2048,.fad=fad,.fn=37};cdb.play_track=&directory_track;
        assert(dispatch(w)==1 && !cdb.part[7].numblocks);
    }
}
static void full_pool(void)
{
    assert(start_file(2,3,4*2048,0,100,0,37));
    for(unsigned i=0;i<200;i++){BLOCK *b=alloc_block();assert(b);b->size=2048;append_block(&cdb.part[23],b);}
    u32 expected[]={250,251,255,256};
    for(unsigned i=0;i<4;i++){
        BLOCK w={.data=sector,.size=2048,.fad=cdb.fad,.fn=37};cdb.play_track=&directory_track;
        u32 boundary=cdb.file_next_unit;
        assert(dispatch(w)==0 && cdb.fad==expected[i] && cdb.file_next_unit==boundary && cdb.play_wait);
        remove_block(&cdb.part[23],0);
        assert(dispatch(w)==1);advance();
        BLOCK *b=cdb.part[0].head;assert(b && b->fad==expected[i]);
        unlink_block(&cdb.part[0],NULL,b);append_block(&cdb.part[23],b);
        check_owners();assert(!cdb.block_free);
    }
    assert(cdb.fad==257);
}
static int replace_metadata(void)
{
    cdb.fi[2]=(FILE_INFO){.lba=9999,.size=0,.unit=1,.gap=20,.fid=8};cdb.play_type=0;return 0;
}
static void pause_snapshot(void)
{
    setup(0,200);cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;cdb.max_repeat=9;cdb.repcnt=4;
    cdb.fi[2]=(FILE_INFO){.lba=100,.size=4*2048,.unit=2,.gap=3,.fid=37};
    cdb.cr1=0;cdb.cr2=1;cdb.cr3=0;cdb.cr4=2;file_pause_hook=replace_metadata;
    assert(!read_file() && cdb.fad==251 && cdb.play_fad_end==257);
    assert(cdb.file_unit==2 && cdb.file_gap==3 && cdb.file_next_unit==252 && cdb.filter[0].fid==37);
    assert(!cdb.max_repeat && !cdb.repcnt);file_pause_hook=NULL;
    cdb.play_type=PLAYTYPE_SECTOR;cdb.fad=251;cdb.file_next_unit=252;advance();assert(cdb.fad==252);
}
static void stale_setup(void)
{
    setup(0,200);cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
    cdb.max_repeat=9;cdb.repcnt=4;cdb.file_gap=7;cdb.file_unit=3;cdb.file_next_unit=12345;
    cdb.fi[2]=(FILE_INFO){.lba=100,.size=2048};cdb.cr4=2;file_pause_hook=NULL;
    assert(!read_file() && !cdb.max_repeat && !cdb.repcnt && !cdb.file_gap && !cdb.file_unit);
    setup(0,200);cdb.root_lba=100;cdb.root_size=2048;cdb.max_repeat=9;cdb.file_gap=7;
    cdb.cr3=255;cdb.cr4=65535;assert(!change_dir());
    assert(cdb.play_type==PLAYTYPE_DIR && !cdb.max_repeat && !cdb.file_gap);
}

/* File completion controls adapted from paps-g a506699c. */
static void run_file(unsigned sel, unsigned unit, unsigned gap, int pressure, int aborting)
{
    const unsigned total = 7;
    const unsigned occupied = (sel + 1) % MAX_SELECTORS;
    assert(start_file(unit, gap, total * 2048, 0, 100, sel, 37));
    if (pressure) {
        for (unsigned i = 0; i < MAX_BLOCKS; ++i) {
            BLOCK *b = alloc_block();
            assert(b);
            b->size = 2048;
            append_block(&cdb.part[occupied], b);
        }
    }
    file_events = efls_writes = pend_writes = bful_writes = 0;
    for (unsigned i = 0; i < total; ++i) {
        unsigned fad = 250 + i + (unit ? i / unit * gap : 0);
        memset(sector, 0x30 + i, sizeof sector);
        BLOCK w = {.data = sector, .size = 2048, .fad = fad, .fn = 37};
        cdb.play_track = &directory_track;
        assert(cdb.fad == fad && cdb.play_type == PLAYTYPE_FILE);
        if (pressure) {
            for (unsigned retry = 0; retry < 3; ++retry) {
                u32 boundary = cdb.file_next_unit;
                cdb.status = STAT_PLAY;
                cdb.play_wait = 0;
                assert(dispatch(w) == 0);
                assert((file_events & (HIRQ_EFLS | HIRQ_PEND)) == 0);
                assert(bful_writes == i * 3 + retry + 1);
                assert(cdb.status == STAT_PAUSE && cdb.play_wait);
                assert(cdb.fad == fad);
                assert(cdb.file_next_unit == boundary && cdb.play_type == PLAYTYPE_FILE);
                assert(!cdb.part[sel].numblocks && cdb.part[occupied].numblocks == MAX_BLOCKS);
                check_owners();
            }
            if (aborting) {
                unsigned before = cdb.block_free;
                file_pause_hook = complete_pause;
                assert(abort_file() == HIRQ_EFLS && !cdb.play_type);
                assert(cdb.block_free == before && cdb.part[occupied].numblocks == MAX_BLOCKS);
                check_owners();
                return;
            }
            remove_block(&cdb.part[occupied], 0);
        }
        cdb.status = STAT_PLAY;
        cdb.play_wait = 0;
        assert(dispatch(w) == 1 && !(file_events & (HIRQ_EFLS | HIRQ_PEND)));
        BLOCK *b = cdb.part[sel].tail;
        assert(b && b->fad == fad && !memcmp(b->data + 24, sector, sizeof sector));
        advance();
        if (pressure) {
            unlink_block(&cdb.part[sel], NULL, b);
            append_block(&cdb.part[occupied], b);
        }
        check_owners();
        assert(finish_step() == 0);
        if (i + 1 < total) assert(!efls_writes && !pend_writes && cdb.play_type == PLAYTYPE_FILE);
    }
    assert(efls_writes == 1 && pend_writes == 1);
    assert(!cdb.play_type && cdb.status == STAT_PAUSE && !cdb.play_track);
    assert(cdb.fad == cdb.play_fad_end);
    assert(finish_step() == 0 && efls_writes == 1);
    check_owners();
}

int main(int argc,char **argv)
{
    assert(argc==2);switch(atoi(argv[1])){
    case 0:metadata_bounds();break;case 1:attributes();break;case 2:directory_flags();break;case 3:geometry();break;
    case 4:wide_extents();break;case 5:file_number();break;case 6:full_pool();break;case 7:pause_snapshot();break;
    case 8:stale_setup();break;
    case 9:
        for (unsigned sel = 0; sel < MAX_SELECTORS; ++sel) run_file(sel, 0, 0, 1, 0);
        break;
    case 10:
        for (unsigned unit = 1; unit <= 7; ++unit) {
            for (unsigned gap = 0; gap < 256; ++gap)
                run_file((unit + gap) % MAX_SELECTORS, unit, gap, 1, 0);
        }
        break;
    case 11:
        for (unsigned sel = 0; sel < MAX_SELECTORS; ++sel) run_file(sel, 2, 3, 1, 1);
        break;
    case 12:
        for (unsigned sel = 0; sel < MAX_SELECTORS; ++sel) run_file(sel, 0, 0, 0, 0);
        break;
    default:abort();}
    puts("pass: actual XA metadata, routing and worker geometry");return 0;
}
