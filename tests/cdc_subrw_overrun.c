/* Actual publisher, command/FIFO/DataEnd and reset bodies from the maintained
 * publication fixture. Zero packs isolate overwrite signaling from pack CRC. */
/* paps-g 329466e4 / SSR-064; public adaptation by papsg. */
#include "bounds-harness.inc"
static u8 sidecar[96], ring_bytes[1536], qdata[10];
static u8 buf_subcode_mode;
static u32 buf_fad_start;
static TRACK_INFO *get_track_info(int code) { (void)code; return NULL; }
#include "subrw-joined.inc"
static unsigned checks,failures;
static void check(int value,const char *name)
{ checks++; if(!value && failures++<8)fprintf(stderr,"failed: %s\n",name); }
#define CHECK(x) check(!!(x),#x)
static unsigned queued(void) {return (cdb.subrw_wp-cdb.subrw_rp)&63;}
static void ready(unsigned rp)
{
    setup(0,200);memset(ring_bytes,0,sizeof ring_bytes);memset(sidecar,0,sizeof sidecar);
    cdb.status=STAT_PLAY;cdb.play_type=0;cdb.SUBH=qdata;
    cdb.subcode_buf=sidecar;cdb.subrw_buf=ring_bytes;cdb.subrw_rp=cdb.subrw_wp=rp;
    fifo_words=fifo_status=fifo_rcnt=0;
}


static void begin_rw(unsigned flags)
{
    cdb.cr1=0x2001; fifo_words=fifo_status=0;
    CHECK(get_subcode()==HIRQ_DRDY && cdb.trans_type==TRANS_SUBRW);
    CHECK(reply[0]==(0x4000 | (cdb.status<<8)) && reply[1]==12 && !reply[2]);
    /* Original Sega CDC_TgetScdRwch extracts response byte 7: CR4 low byte.
     * The independent SDK header defines CDC_SCD_OVER as 0x02. */
    CHECK(reply[3]==flags && ((unsigned)reply[3]&255)==flags);
    CHECK(fifo_words==12);
    for(unsigned i=0;i<12;i++)CHECK(!fifo_output[i]);
}
static void finish_rw(void)
{
    CHECK(end_trans()==HIRQ_EHST && !cdb.trans_type && reply[1]==12);
    CHECK(!reply[2] && !reply[3]);
}
static void frame(unsigned index)
{
    buf_subcode_mode=2; buf_fad_start=150+index;
    memset(sidecar,0,sizeof sidecar);
    publish_subrw(150+index);
}
static void start(unsigned rp,unsigned frames)
{
    ready(rp);
    for(unsigned i=0;i<frames;i++)frame(i);
}
static void ordinary_cases(void)
{
    for(unsigned rp=0;rp<64;rp++) {
        start(rp,0);cdb.cr1=0x2001;fifo_words=0;
        CHECK(!get_subcode() && !fifo_words && !cdb.trans_type);
        CHECK(reply[0]>>8==(STAT_WAIT|STAT_PLAY));
        for(unsigned frames=15;frames<=17;frames++) {
            start(rp,frames);
            CHECK(queued()==(frames==15?60:63));
            begin_rw(frames==15?0:2);finish_rw();
            begin_rw(0);finish_rw(); /* One report for the preceding loss burst. */
        }
        start(rp,0);
        for(unsigned i=0;i<16;i++) {
            frame(i);
            if(i==7)for(unsigned j=0;j<32;j++){begin_rw(0);finish_rw();}
        }
        CHECK(queued()==32); begin_rw(0);finish_rw();
    }
}
static void lifetime_cases(void)
{
    for(unsigned rp=0;rp<64;rp++) {
        start(rp,16); u8 q[10]; cdb.SUBH=q; cdb.cr1=0x2000;fifo_words=0;
        CHECK(get_subcode()==HIRQ_DRDY && cdb.trans_type==TRANS_SUBQ && !reply[3]);
        CHECK(end_trans()==HIRQ_EHST);
        cdb.cr1=0x2002;CHECK(!get_subcode() && !cdb.trans_type); /* Invalid type. */
        begin_rw(2);
        frame(16); /* More unread packs lost while the prior FIFO is owned. */
        cdb.cr1=0x2001;CHECK(!get_subcode() && cdb.trans_type==TRANS_SUBRW);
        finish_rw();begin_rw(2);finish_rw();begin_rw(0);finish_rw();

        start(rp,16);cdb.play_type=0;cdb.cr1=0x402;
        CHECK(!init_cdblock() && queued()==63);
        begin_rw(2);finish_rw(); /* Non-reset INIT retains the queue. */
        start(rp,16);cdb.play_type=0;cdb.cr1=0x401;
        CHECK(init_cdblock()==(HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST));
        CHECK(!queued());cdb.status=STAT_PLAY;
        frame(0);begin_rw(0);finish_rw(); /* Software reset retires the queue. */
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);
    if(atoi(argv[1]))lifetime_cases();else ordinary_cases();
    printf("R-W flags: %u checks, %u failures across all 64 cursor positions\n",checks,failures);
    return failures?1:0;
}
