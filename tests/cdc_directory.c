/* MCU-CDC-011: paps-g selected8e465233; public actual-source/list/payload fixtures. */
#define CDC_OWNERSHIP_HOOKS
#define log_mask fixture_log_mask
#define CDC_BOUNDS_MAIN bounds_fixture_main
#include "cdc_bounds.c"
#undef main
static void memcpy32(void *out,const void *in,unsigned count) { assert(!irq_depth);copied_bytes+=count;memcpy(out,in,count); }
#define pause_drive pause_disc
#include "directory.inc"
#undef pause_drive
static inline void publish_subrw(u32 fad) { (void)fad; }
#include "dispatch.inc"
static int failed_sector(int fad,BLOCK *blk) { (void)fad;cdb.status=STAT_ERROR;return blk?-2:-1; }
static __attribute__((unused)) void set_peri_report(void)
{
    assert(cdb.status==STAT_ERROR && cdb.play_type==PLAYTYPE_DIR);
}
#define get_sector failed_sector
#include "input-error.inc"
#undef get_sector
static u32 buf_fad_start,buf_fad_end;
static unsigned pause_signals;
static void set_pause_ok(void) { pause_signals++; }
#include "pause.inc"

static TRACK_INFO directory_track={.mode=1};
static u8 sector[2048];
static u32 finfo[254*3];
static void le32(u8 *p,u32 n) { for(unsigned i=0;i<4;i++)p[i]=n>>(8*i); }
static unsigned record(u8 *p,unsigned id,unsigned length)
{
    assert(length>=34 && length<=254 && !(length&1));
    memset(p,0,length);p[0]=length;p[25]=id<2?2:0;p[32]=1;p[33]=id<2?id:'A';
    le32(p+2,1000+id);le32(p+10,4096+id);
    return length;
}
static void ready(unsigned sel,u32 bytes,u32 fid)
{
    setup(0,200);file_pause_hook=NULL;boundary_log_hook=NULL;fixture_log_mask=0;
    cdb.cdir_lba=100;cdb.cdir_size=bytes;cdb.rdir_state=3;
    cdb.cdir_pfid=fid;cdb.cdir_pfnum=0;legacy_file_info_wire(finfo);
    cdb.filter[sel].c_true=255;cdb.filter[sel].c_false=255;
    cdb.cr3=sel<<8|(fid>>16);cdb.cr4=fid;
    assert(read_dir()==0 && cdb.play_type==PLAYTYPE_DIR && cdb.dir_filter==sel);
    assert(cdb.cddev_filter==sel && cdb.filter[sel].c_true==sel);
    cdb.play_track=&directory_track;
}
static int feed(void)
{
    BLOCK w={.data=sector,.size=2048,.fad=cdb.fad};
    cdb.play_track=&directory_track;
    int next=dispatch(w);
    if(next==1)cdb.fad++;
    check_owners();return next;
}
static void extents(void)
{
    FILE_INFO absent={.lba=123};
    assert(!read_directory_record(&absent,NULL,0) && absent.lba==123);
    for(unsigned avail=0;avail<=256;avail++)for(unsigned length=0;length<256;length++) {
        u8 *p=malloc(avail?avail:1);assert(p);memset(p,0,avail);
        if(avail)p[0]=length;if(avail>32)p[32]=1;
        FILE_INFO out={.lba=0xdeadbeef};
        int n=read_directory_record(&out,p,avail);
        assert(n==((avail>=34 && length>=34 && length<=avail)?(int)length:0));
        if(!n)assert(out.lba==0xdeadbeef);free(p);
    }
    for(unsigned len=34;len<=255;len++)for(unsigned name=0;name<=255;name++) {
        u8 p[256]={0};p[0]=len;p[32]=name;FILE_INFO out;
        assert(!!read_directory_record(&out,p,len)==(name && 33+name+!(name&1)<=len));
    }
    u8 p[34];record(p,4,34);le32(p+2,0xfedcba98);le32(p+10,0xdeadbeef);FILE_INFO out;
    assert(read_directory_record(&out,p,34)==34 && out.lba==0xfedcba98 && out.size==0xdeadbeef);
    p[1]=2;assert(read_directory_record(&out,p,34)==34 && out.lba==0xfedcba9a);
    le32(p+2,0xffffffff);p[1]=1;out.lba=123;
    assert(!read_directory_record(&out,p,34) && out.lba==123);
}
static void pages(void)
{
    const unsigned counts[]={2,3,255,256,257,300,65540};
    const unsigned starts[]={0,1,2,3,255,256,257,65536,0xffffff};
    for(unsigned sel=0;sel<24;sel++)for(unsigned n=0;n<7;n++)for(unsigned s=0;s<9;s++) {
        unsigned total=counts[n],start=starts[s]<2?2:starts[s],nsec=(total+7)/8;
        ready(sel,nsec*2048,starts[s]);
        unsigned id=0,reads=0;
        while(cdb.play_type==PLAYTYPE_DIR) {
            memset(sector,0,sizeof sector);
            for(unsigned j=0;j<8 && id<total;j++)record(sector+j*254,id++,254);
            reads++;assert(reads<=nsec);feed();assert(cdb.block_free==200);
        }
        unsigned count=total>start?total-start:0;if(count>254)count=254;
        assert(cdb.status==STAT_PAUSE && cdb.rdir_state==3 && cdb.cdir_pfnum==count);
        assert(cdb.cdir_pfid==start && cdb.cdir_drend==(total<=start+254));
        assert(cdb.fi[0].lba==1000 && cdb.fi[1].lba==1001);
        for(unsigned i=0;i<count;i++)assert(cdb.fi[i+2].lba==1000+start+i);
        assert(get_file_scope()==0 && reply[1]==count);
        assert(((u32)(reply[2]&255)<<16|reply[3])==start);
    }
    /* Sega permits directory access with just one free sector. */
    ready(0,300*2048,2000);
    for(unsigned i=0;i<199;i++){
        BLOCK *b=alloc_block();assert(b);b->size=2048;append_block(&cdb.part[23],b);
    }
    unsigned id=0,reads=0;
    while(cdb.play_type==PLAYTYPE_DIR){
        memset(sector,0,sizeof sector);
        for(unsigned j=0;j<8;j++)record(sector+j*254,id++,254);
        reads++;assert(reads<=300);feed();
        assert(cdb.block_free==1 && cdb.part[23].numblocks==199);
    }
    assert(reads==282 && cdb.rdir_state==3 && cdb.cdir_pfnum==254 && !cdb.cdir_drend);
    assert(cdb.fi[2].lba==3000 && cdb.fi[255].lba==3253);
}
static void malformed(void)
{
    for(unsigned kind=0;kind<7;kind++) {
        ready(7,2048,2);memset(sector,0,sizeof sector);
        record(sector,0,34);record(sector+34,1,34);record(sector+68,2,34);
        if(kind==0)sector[0]=0;
        if(kind==1)sector[0]=33;
        if(kind==2)sector[32]=250;
        if(kind==3)sector[33]=1;
        if(kind==4)sector[25]=0;
        if(kind==5){memset(sector+34,0,2048-34);}
        if(kind==6){cdb.cdir_size=70;sector[68]=34;}
        assert(feed()==0 && cdb.status==STAT_ERROR && !cdb.rdir_state && !cdb.cdir_pfnum);
        assert(cdb.block_free==200);
    }
    ready(0,2048,2);assert(handle_diread()==0 && cdb.status==STAT_ERROR);
    /* A record straddling the sector boundary cannot borrow the next sector. */
    ready(0,4096,2);memset(sector,0,2048);
    unsigned offset=0;for(unsigned id=0;id<8;id++)offset+=record(sector+offset,id,254);
    assert(offset==2032);sector[offset]=34;
    assert(feed()==0 && cdb.status==STAT_ERROR && !cdb.cdir_pfnum);
}
static void descriptors(void)
{
    for(unsigned kind=0;kind<10;kind++) {
        setup(0,200);file_pause_hook=NULL;cdb.cr3=5<<8|255;cdb.cr4=65535;legacy_file_info_wire(finfo);
        assert(change_dir()==0 && cdb.rdir_state==1 && cdb.dir_filter==5);
        memset(sector,0,2048);memcpy(sector+1,"CD001",5);sector[6]=1;
        if(kind==0){sector[0]=0;assert(feed()==1 && cdb.fad==167 && cdb.filter[5].range==2);}
        memset(sector,0,2048);sector[0]=1;memcpy(sector+1,"CD001",5);sector[6]=1;sector[129]=8;
        record(sector+156,0,34);le32(sector+158,100);le32(sector+166,300*2048);
        if(kind==1)sector[1]='X';if(kind==2)sector[6]=2;if(kind==3)sector[0]=255;
        if(kind==4)sector[156]=33;if(kind==5)le32(sector+158,0xffffff);
        if(kind==6)le32(sector+166,0);if(kind==7)sector[129]=4;
        if(kind==8)sector[189]=1;if(kind==9)sector[157]=1;
        int next=feed();
        if(!kind || kind==9)assert(next==2 && cdb.fad==(kind==9?251:250) && cdb.cdir_size==300*2048 && cdb.root_size==300*2048 && cdb.rdir_state==2);
        else assert(!next && cdb.status==STAT_ERROR && !cdb.rdir_state);
        assert(cdb.block_free==200);
    }
}

static void raw_mode1_descriptor(void)
{
    setup(0, 200);
    cdb.cr3 = 5 << 8 | 255;
    cdb.cr4 = 65535;
    assert(change_dir() == 0 && cdb.rdir_state == 1 && cdb.dir_filter == 5);
    _Alignas(4) u8 raw[2352] = {0};
    raw[15] = 1;
    u8 *data = raw + 16;
    data[0] = 1;
    memcpy(data + 1, "CD001", 5);
    data[6] = 1;
    data[129] = 8;
    record(data + 156, 0, 34);
    le32(data + 158, 100);
    le32(data + 166, 4096);
    cdb.play_track = &directory_track;
    BLOCK w = {.data = raw, .size = sizeof raw, .fad = cdb.fad};
    assert(dispatch(w) == 2 && cdb.rdir_state == 2 && cdb.fad == 250);
    assert(cdb.root_lba == 100 && cdb.root_size == 4096 && cdb.block_free == MAX_BLOCKS);
    assert(!memcmp(sector_bytes[0], raw, sizeof raw));
    check_owners();
}

static void raw_image_fifo(unsigned first)
{
    const unsigned lengths[] = {2048, 2336, 2340, 2352};
    TRACK_INFO tracks[3] = {{.mode = 1}, {.mode = 3}, {.mode = 2}};
    _Alignas(4) u8 raw[3][2352];
    for (unsigned n = 0; n < 3; ++n) {
        for (unsigned i = 0; i < sizeof raw[n]; ++i)
            raw[n][i] = (u8)(i * 11 + i / 17 + 43 * n);
        raw[n][15] = n ? 2 : 1;
        raw[n][18] = 0x20; /* Valid XA on data track, sample bytes on audio. */
    }
    for (unsigned l = 0; l < sizeof lengths / sizeof *lengths; ++l) {
        unsigned length = lengths[l];
        seed(0, 0);
        cdb.sector_size = length;
        cdb.filter[0].c_true = 0;
        unsigned total = 0;
        for (unsigned i = 0; i < 3; ++i) {
            unsigned n = (i + first) % 3;
            BLOCK w = {.data = raw[n], .size = sizeof raw[n], .fad = 150 + i,
                       .sm = n == 2 ? 0x20 : 0};
            assert(filter_sector(&tracks[n], &w) == 0);
            total += length == 2048 && n == 2 ? 2324 : length;
        }
        unsigned selector = 0;
        if (first) {
            selector = 4;
            command(first == 2, 0, selector, 0, 3);
            assert(execute(first == 2) == HIRQ_ECPY);
            assert(cdb.part[selector].numblocks == 3);
        }
        cdb.cr2 = 0;
        cdb.cr3 = selector << 8;
        cdb.cr4 = 3;
        assert(cal_actual_size() == HIRQ_ESEL);
        size_reply(total / 2);
        assert(get_sector_data() == HIRQ_DRDY);
        for (unsigned i = 0; i < 3; ++i) trans_handle();
        assert(cdb.trans_finish && fifo_words == total / 2 && cdb.cdwnum == (int)total);
        unsigned pos = 0;
        for (unsigned i = 0; i < 3; ++i) {
            unsigned n = (i + first) % 3;
            unsigned offset = length == 2048 ? (n == 0 ? 16 : 24) : 2352 - length;
            unsigned bytes = length == 2048 && n == 2 ? 2324 : length;
            const u8 *data = raw[n] + offset;
            for (unsigned b = 0; b < bytes; b += 2)
                assert(fifo_output[pos++] == (data[b] | ((u16)data[b + 1] << 8)));
        }
        assert(pos == fifo_words && end_trans() == HIRQ_EHST);
        assert((((unsigned)(reply[0] & 255) << 16) | reply[1]) == total / 2);
        check_owners();
    }
}

static void cooked_image_tail(void)
{
    const unsigned lengths[] = {2048, 2336, 2340, 2352};
    _Alignas(4) u8 payload[2048];
    _Alignas(4) u8 gap[2048] = {0};
    for (unsigned i = 0; i < sizeof payload; ++i) payload[i] = (u8)(i * 13 + i / 17);
    for (unsigned l = 0; l < sizeof lengths / sizeof *lengths; ++l) {
        unsigned length = lengths[l];
        seed(0, 0);
        cdb.sector_size = length;
        memset(cdb.block[0].data, 0x5a, 2352);
        memset(cdb.block[1].data, 0xa5, 2352);
        BLOCK w = {.data = payload, .size = sizeof payload, .fad = 150};
        assert(filter_sector(&directory_track, &w) == 0);
        w.data = gap; /* A synthetic data-track gap. */
        w.fad++;
        assert(filter_sector(&directory_track, &w) == 0);
        cdb.cr2 = 0;
        cdb.cr4 = 2;
        assert(cal_actual_size() == HIRQ_ESEL);
        size_reply(length);
        assert(get_sector_data() == HIRQ_DRDY);
        trans_handle();
        trans_handle();
        assert(cdb.trans_finish && fifo_words == length && cdb.cdwnum == (int)(length * 2));
        unsigned offset = length == 2048 ? 24 : 2352 - length;
        for (unsigned i = 0; i < length; i += 2) {
            unsigned at = offset + i;
            u16 expected = at >= 24 && at < 2072 ?
                           payload[at - 24] | ((u16)payload[at - 23] << 8) : 0;
            assert(fifo_output[i / 2] == expected);
            assert(fifo_output[length / 2 + i / 2] == 0);
        }
        assert(end_trans() == HIRQ_EHST);
        check_owners();
    }
}

static void check_file_words(unsigned first, const u8 expected[12])
{
    for (unsigned k = 0; k < 6; ++k)
        assert(fifo_output[first + k] == ((u16)expected[2*k + 1] << 8 | expected[2*k]));
}

static void queries(void)
{
    setup(0,200);legacy_file_info_wire(finfo);cdb.rdir_state=3;cdb.cdir_pfid=65536;cdb.cdir_pfnum=254;
    for (unsigned i = 0; i < 256; ++i)
        cdb.fi[i] = (FILE_INFO){.lba=0x123400+i*0x101, .size=0x1020304+i*0x10203,
                               .unit=1+(i&15), .gap=i>>4, .fid=i, .attr=i^0x5a};
    FILE_INFO snapshot[256];
    memcpy(snapshot, cdb.fi, sizeof(snapshot));
    const u32 ids[]={0,1,65536,65789,2,65535,65790,0xffffff};
    for(unsigned j=0;j<8;j++) {
        u32 fid=ids[j];cdb.cr3=fid>>16;cdb.cr4=fid;fifo_words=0;
        fifo_rcnt=0xffff; st_control=0x0200 | STIRQ_CMD; st_status=0x1234;
        check_fifo_publication=1;
        int result=get_file_info();
        check_fifo_publication=0;
        if (j>=4 && j<=6) {
            assert(!result && reply[0]==STAT_REJECT<<8 && !fifo_words);
            assert(fifo_rcnt==0xffff && st_control==(0x0200 | STIRQ_CMD) && st_status==0x1234);
            continue;
        }
        unsigned index=fid==0xffffff?2:fid<2?fid:fid-65536+2;
        unsigned count=fid==0xffffff?254:1;
        assert(result==HIRQ_DRDY && reply[0]==(0x4000 | cdb.status<<8) && reply[1]==count*6 && !reply[2] && !reply[3]);
        assert(fifo_words==count*6 && fifo_rcnt==0 && st_control==(STIRQ_DAT | STIRQ_CMD) && st_status==STIRQ_DAT);
        for (unsigned i = 0; i < count; ++i) {
            const FILE_INFO *f = &snapshot[index+i];
            const u32 fad = f->lba + 150, size = f->size;
            const u8 expected[12] = {fad>>24, fad>>16, fad>>8, fad, size>>24, size>>16, size>>8, size,
                                     f->gap, f->unit, f->fid, f->attr & 0xfa};
            check_file_words(i*6, expected);
        }
        assert(!memcmp(snapshot, cdb.fi, sizeof(snapshot)));
        CDBLOCK before;
        memcpy(&before, &cdb, sizeof(before));
        assert(!get_file_info() && reply[0]==(STAT_WAIT|cdb.status)<<8);
        assert(!memcmp(&before, &cdb, sizeof(before)) && fifo_words==count*6);
        while(!cdb.trans_finish)trans_handle();
        fifo_status=0;assert(end_trans()==HIRQ_EHST && reply[1]==count*6);
    }
    cdb.play_type=PLAYTYPE_DIR;
    for(unsigned j=0;j<5;j++) {
        int (*f[])(void)={change_dir,read_dir,get_file_scope,get_file_info,read_file};
        cdb.cr3=0;cdb.cr4=0;assert(!f[j]() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
    }
    cdb.play_type=0;cdb.rdir_state=0;assert(!get_file_scope() && reply[0]==STAT_REJECT<<8);
}
static int replace_file_metadata(void) { cdb.fi[2]=(FILE_INFO){.lba=900,.size=1};return 0; }
static void file_reads(void)
{
    const u32 sizes[]={0,1,2047,2048,2049,0x7fffffff,0xfffff800,0xffffffff};
    for(unsigned sel=0;sel<24;sel++)for(unsigned n=0;n<8;n++) {
        u32 count=(sizes[n]>>11)+!!(sizes[n]&2047);
        u32 offsets[]={0,count?count-1:0,count,0xffffff};
        for(unsigned k=0;k<4;k++) {
            setup(1,199);file_pause_hook=NULL;cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
            cdb.fi[2]=(FILE_INFO){.lba=100,.size=sizes[n]};cdb.filter[sel].c_true=255;
            cdb.cr1=offsets[k]>>16;cdb.cr2=offsets[k];cdb.cr3=sel<<8;cdb.cr4=2;
            int ok=offsets[k]<count;assert(read_file()==0);
            if(ok)assert(cdb.play_type==PLAYTYPE_FILE && cdb.fad==250+offsets[k] && cdb.play_fad_end==250+count && cdb.cddev_filter==sel && cdb.filter[sel].c_true==sel);
            else assert(!cdb.play_type && reply[0]==STAT_REJECT<<8);
            check_owners();
        }
    }
    setup(0,200);cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
    cdb.fi[2]=(FILE_INFO){.lba=100,.size=4097};cdb.cr4=2;
    file_pause_hook=replace_file_metadata;assert(!read_file());
    assert(cdb.play_type==PLAYTYPE_FILE && cdb.play_fad_start==250 && cdb.play_fad_end==253);
    file_pause_hook=NULL;check_owners();
}
static int failed_pause(void) { return -1; }
static int complete_pause(void) { cdb.play_type=0;cdb.pause_request=0;return 0; }
static void aborts(void)
{
    ready(0,4096,2);BLOCK *held=alloc_block();assert(held);held->size=2352;append_block(&cdb.part[0],held);
    file_pause_hook=failed_pause;
    assert(abort_file()==0 && cdb.play_type==PLAYTYPE_DIR);
    file_pause_hook=complete_pause;
    FILTER f=cdb.filter[0];assert(abort_file()==HIRQ_EFLS && !cdb.rdir_state && !cdb.cdir_pfnum);
    assert(!memcmp(&f,&cdb.filter[0],sizeof f) && cdb.block_free==199 && cdb.part[0].head==held);
    cdb.play_type=PLAYTYPE_SECTOR;assert(abort_file()==HIRQ_EFLS && cdb.play_type==PLAYTYPE_SECTOR);
    setup(0,200);cdb.rdir_state=3;cdb.cdir_pfnum=1;cdb.cdir_pfid=2;
    cdb.fi[2]=(FILE_INFO){.lba=100,.size=2048,.attr=0};cdb.cr3=0;cdb.cr4=2;
    assert(!change_dir() && reply[0]==STAT_REJECT<<8 && cdb.rdir_state==3);
    cdb.fi[2].attr=2;cdb.play_type=PLAYTYPE_FILE;file_pause_hook=failed_pause;
    assert(!change_dir() && cdb.play_type==PLAYTYPE_FILE && cdb.rdir_state==3);
    file_pause_hook=NULL;
}
static BLOCK *private_block;
static void steal_public(void)
{
    private_block=&cdb.block[0];assert(private_block->size!=-1);
    free_partition(&cdb.part[0]);
    BLOCK *b=alloc_block();assert(b && b!=private_block);
    b->size=2048;append_block(&cdb.part[1],b);
}
static void next_directory(void)
{
    assert(cdb.rdir_state==3 && cdb.play_type==0 && hirq==HIRQ_EFLS);
    cdb.cr3=4<<8;cdb.cr4=2;assert(read_dir()==0);
    cdb.play_track=&directory_track;
}
static void next_after_error(void)
{
    assert(!cdb.rdir_state && !cdb.play_type && cdb.status==STAT_ERROR);
    cdb.root_lba=100;cdb.root_size=2048;cdb.cr3=4<<8|255;cdb.cr4=65535;
    assert(change_dir()==0);cdb.play_track=&directory_track;
}
static void preemption(void)
{
    ready(0,2048,2);memset(sector,0,2048);record(sector,0,34);record(sector+34,1,34);
    BLOCK w={.data=sector,.size=2048,.fad=cdb.fad};assert(filter_sector(&directory_track,&w)==0);
    irq_restore_hook=steal_public;
    assert(handle_diread()==0 && cdb.rdir_state==3 && cdb.fi[0].lba==1000);
    assert(cdb.block_free==199 && cdb.part[1].numblocks==1);check_owners();
    irq_restore_hook=next_directory;assert(finish_read(1)==0);
    assert(cdb.play_type==PLAYTYPE_DIR && cdb.dir_filter==4 && cdb.play_track==&directory_track);
    irq_restore_hook=next_after_error;assert(input_error_step()==0);
    assert(cdb.play_type==PLAYTYPE_DIR && cdb.dir_filter==4 && cdb.play_track==&directory_track);
    irq_restore_hook=next_after_error;assert(initial_error_step()==0);
    assert(cdb.play_type==PLAYTYPE_DIR && cdb.dir_filter==4 && cdb.play_track==&directory_track);
    for(unsigned target=1;target<24;target++) {
        ready(0,2048,2);cdb.filter[0].c_true=target;
        assert(feed()==0 && cdb.status==STAT_ERROR && cdb.block_free==199);
        assert(cdb.part[target].numblocks==1);check_owners();
    }
    for(unsigned target=24;target<256;target++) {
        ready(0,2048,2);cdb.filter[0].c_true=target;
        assert(feed()==0 && cdb.status==STAT_ERROR && cdb.block_free==200);
    }
}
static void new_request_after_ack(void)
{
    assert(!cdb.play_type && !cdb.pause_request && !cdb.rdir_state && !cdb.cdir_pfnum);
    cdb.play_type=PLAYTYPE_FILE;cdb.pause_request=1;cdb.status=STAT_BUSY;
}
static void request_pause(void) { assert(cdb.play_type==PLAYTYPE_DIR);cdb.pause_request=1; }
static void pauses(void)
{
    ready(0,4096,2);cdb.cdir_pfnum=7;cdb.pause_request=1;
    buf_fad_start=150;buf_fad_end=170;pause_signals=0;
    irq_restore_hook=new_request_after_ack;acknowledge_disc_pause();
    assert(cdb.play_type==PLAYTYPE_FILE && cdb.pause_request==1 && cdb.status==STAT_BUSY);
    assert(buf_fad_start==150 && buf_fad_end==170 && pause_signals==1 && !irq_depth);
    /* A command can request pause while the descriptor's privately owned
     * block is being parsed. The internal phase transition cannot retire it. */
    setup(0,200);cdb.cr3=255;cdb.cr4=65535;assert(!change_dir());
    memset(sector,0,2048);sector[0]=1;memcpy(sector+1,"CD001",5);sector[6]=1;sector[129]=8;
    record(sector+156,0,34);le32(sector+158,100);le32(sector+166,4096);
    irq_restore_hook=request_pause;assert(feed()==2 && cdb.pause_request==1 && cdb.rdir_state==2);
    acknowledge_disc_pause();assert(!cdb.play_type && !cdb.pause_request && !cdb.rdir_state);
    assert(cdb.block_free==200);check_owners();
}
static void logging(void)
{
    ready(0,2048,2);memset(sector,0,2048);
    record(sector,0,34);record(sector+34,1,34);record(sector+68,2,254);
    sector[68+32]=220;memset(sector+68+33,'Z',220);
    u8 before[2048];memcpy(before,sector,sizeof before);
    fixture_log_mask=_FILEIO;fixture_log_used=0;
    assert(feed()==0 && cdb.rdir_state==3 && cdb.cdir_pfnum==1);
    assert(!memcmp(before,sector,sizeof before));
    assert(!memcmp(before,sector_bytes[0]+24,sizeof before));
    assert(strstr(fixture_log_text,"handle_diread 2\n  cdir_lba=00000064 size=00000001\n"));
    assert(strstr(fixture_log_text,"  fid   0:  lba=000003e8 size=00001000 attr=02  1 \n"));
    assert(strstr(fixture_log_text,"  fid   2:  lba=000003ea size=00001002 attr=00  218 ZZZ"));
    assert(strstr(fixture_log_text,"  cdir_pfid=2 fnum=1\n"));
    unsigned names=0;for(unsigned i=0;i<fixture_log_used;i++)names+=fixture_log_text[i]=='Z';assert(names==218);
    assert(copied_bytes==2048 && cdb.block_free==200 && !irq_depth);check_owners();
}
static void span_controls(void)
{
    const u32 lbas[]={0,1,0xffffffu-150,0xffffffu-149,0xffffffffu};
    const u32 bytes[]={0,1,2048,2049,0xffffffffu};
    for(unsigned l=0;l<5;l++)for(unsigned n=0;n<5;n++) {
        setup(0,200);cdb.rdir_state=3;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
        cdb.fi[2]=(FILE_INFO){.lba=lbas[l],.size=bytes[n]};cdb.cr4=2;
        u32 sectors=(bytes[n]>>11)+!!(bytes[n]&2047);
        uint64_t end=(uint64_t)lbas[l]+150+sectors;
        int valid=sectors && (uint64_t)lbas[l]+150<=0xffffffu && end<=0x1000000u;
        CDBLOCK before;memcpy(&before,&cdb,sizeof before);assert(!read_file());
        if(valid)assert(cdb.play_type==PLAYTYPE_FILE && cdb.play_fad_end==end);
        else assert(reply[0]==STAT_REJECT<<8 && !memcmp(&before,&cdb,sizeof cdb));
        check_owners();
    }
}
_Alignas(8) static u8 cd_memory[0x76000];
#define TEST_CD_MEMORY ((uintptr_t)cd_memory)
#include "directory-startup.inc"

static void directory_startup(void)
{
    memset(cd_memory, 0xa5, sizeof(cd_memory));
    ss_sw_init();
    u8 *after_pool = cd_memory + 200 * 2352;
    assert(cdb.block[0].data == cd_memory && cdb.block[199].data + 2352 == after_pool);
    assert((u8 *)cdb.TOC == after_pool + 3072 && (u8 *)cdb.SUBH == after_pool + 3488);
#ifdef TEST_AXI_FILE_INFO
    assert((u8 *)cdb.fi == after_pool && (u8 *)(cdb.fi + 256) == (u8 *)cdb.TOC);
#else
    assert(cdb.FINFO == after_pool);
#endif
    assert(!cdb.rdir_state && !cdb.cdir_pfnum && !cdb.trans_type);
    for (unsigned i = 0; i < 3072; ++i) assert(after_pool[i] == 0xa5);
}

int main(int argc,char **argv)
{

    assert(argc==2);switch(atoi(argv[1])) {
    case 0:extents();break;case 1:pages();break;case 2:malformed();break;case 3:descriptors();break;
    case 4:queries();break;case 5:file_reads();break;case 6:aborts();break;case 7:preemption();break;
    case 8:pauses();break;case 9:logging();break;case 10:span_controls();break;
    case 11:directory_startup();break;
    case 12:raw_mode1_descriptor();break;
    case 13:raw_image_fifo(0);raw_image_fifo(1);raw_image_fifo(2);break;
    case 14:cooked_image_tail();break;
    default:abort();}
#ifdef TEST_AXI_FILE_INFO
    if (cdb.fi == directory_memory.table)
        assert(directory_memory.before == 0x76543210 && directory_memory.after == 0x76543210);
#endif
    puts("pass: actual streamed parser, commands, routes and worker steps");return 0;
}
