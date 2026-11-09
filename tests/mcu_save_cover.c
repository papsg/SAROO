/* Adapted from paps-g 681387bc: actual public C/FatFs and synthetic media.
 * File faults and asynchronous display acknowledgments are fixtures. */
#define SAROO_COVER_INTEGRATION
#define MCU_DISCOVERY_TEST_ENTRY cover_discovery_test_main
#include "mcu_save_discovery.c"
typedef uint16_t u16;
_Alignas(8) static struct { u8 before[32],data[0x100000],after[32]; } gfx;
#define TEST_GFX_BASE ((uintptr_t)gfx.data)
static unsigned c_reads,c_seeks,c_opens,c_closes,c_ip,c_delays,c_barriers;
static int c_fault,c_release,c_version,c_bad_ip;
static FIL *c_live;
static u8 c_busy[0x20000];
static FRESULT cover_open(FIL *fp,const TCHAR *name,BYTE mode)
{
    c_opens++;
    if(c_fault==6) return FR_DISK_ERR;
    assert(!c_live && mode==FA_READ && !strcmp(name,"/SAROO/cover.bin"));
    FRESULT result=f_open(fp,name,mode);
    if(!result)c_live=fp;
    return result;
}
static FRESULT cover_close(FIL *fp)
{
    c_closes++;assert(c_live==fp);
    if(c_fault==7)return FR_DISK_ERR; /* Keep the real file open on failure. */
    FRESULT result=f_close(fp);
    if(!result)c_live=NULL;
    return result;
}
static FRESULT cover_read(FIL *fp,void *out,UINT size,UINT *count)
{
    c_reads++;assert(c_live==fp);
    uintptr_t offset=(u8*)out-gfx.data;
    assert((offset==0xf0000 && size<=0x10000) ||
           (offset==0xc00 && size<=512*240+0x400));
    if(offset==0xc00)assert(!gfx.data[0] && !gfx.data[2]);
    if(c_fault==3){*count=0;return FR_DISK_ERR;}
    if(c_fault==4)size--;
    if(c_fault==5){assert(!f_read(fp,out,size/2,count));return FR_DISK_ERR;}
    return f_read(fp,out,size,count);
}
static FRESULT cover_seek(FIL *fp,FSIZE_t offset)
{
    c_seeks++;assert(c_live==fp);
    if(c_fault==1)return FR_DISK_ERR;
    return f_lseek(fp,c_fault==2 ? offset+1 : offset);
}
static void make_ip(u8 *out,int index,int version)
{
    memset(out,0,256);memcpy(out,"SEGA SEGASATURN ",16);
    char id[17];snprintf(id,sizeof(id),"G%04d V1.0",index);
    memset(out+0x20,' ',16);memcpy(out+0x20,id,strlen(id));
    memset(out+0x30,version+1,64);
}
static u32 ip_sum(int version)
{
    unsigned byte=version+1;
    return ((64+2080*byte)%65521)<<16 | ((1+64*byte)%65521);
}
static int get_disc_ip(int index,u8 *out)
{
    c_ip++;assert(index>=0 && index<total_disc);
    if(c_bad_ip==1)return -1;
    make_ip(out,index,c_version);
    if(c_bad_ip==2)out[0]='X';
    if(c_bad_ip==3)memcpy(out+0x20,"ABCDEFGHIJKL    ",16);
    return 0;
}
static void osDelay(unsigned ticks)
{
    assert(ticks==1 && c_delays<11);
    assert(!memcmp(gfx.data,c_busy,sizeof(c_busy)));
    c_delays++;
    if(c_release && c_delays==(unsigned)c_release)gfx.data[0]=gfx.data[2]=0;
}
static void test_barrier(void) { c_barriers++;assert(!gfx.data[0] && !gfx.data[2]); }
#define __DMB test_barrier
#define f_open cover_open
#define f_close cover_close
#define f_read cover_read
#define f_lseek cover_seek
#define save_io cover_io
#include "cover.inc"
#undef save_io
#undef f_lseek
#undef f_read
#undef f_close
#undef f_open

static void ack(void) { gfx.data[0]=gfx.data[2]=0; }
static void put16(u8 *p,unsigned v) { p[0]=v;p[1]=v>>8; }
static void put32(u8 *p,u32 v) { for(int i=0;i<4;i++)p[i]=v>>(i*8); }
static unsigned get16(const u8 *p) { return p[0]|(unsigned)p[1]<<8; }
static u8 pixel(unsigned i,int version) { return (u8)(i*17+version*29+3); }
_Alignas(8) static struct { u8 before[32],data[0x40000],after[32]; } screen;
#define VDP2_VRAM ((uintptr_t)screen.data)
static u32 TVSTAT=8,PRINA,CCCTL,BGON;
static int fbw=320,fbh=240;
static u32 display_palette[256];
static u8 *dma_out,*dma_in;
static int dma_bytes,dma_pending;
static unsigned dma_reads;
static u32 display_status(void)
{
    dma_reads++;
    if(dma_pending) {
        assert(gfx.data[0]); /* Ownership must not be returned during DMA. */
        if(--dma_pending)return 0x3000;
        memcpy(dma_out,dma_in,dma_bytes);
    }
    return 0;
}
#define SDSTAT display_status()
static void scu_dmemcpy(void *out,void *in,int size,int channel)
{
    assert(channel==2 && size>0);
    assert((u8*)out>=screen.data+0x20000 && (u8*)out+size<=screen.data+0x20000+512*240);
    assert((u8*)in>=gfx.data+0x1000 && (u8*)in+size<=gfx.data+0x1000+512*240);
    while(display_status()&0x3000) {} /* Actual helper waits for the preceding row. */
    dma_out=out;dma_in=in;dma_bytes=size;dma_pending=3;
}
static u32 LE16(void *p) { return get16(p); }
static void nbg1_set_cram(int index,int r,int g,int b)
{
    assert(index>=0 && index<256);display_palette[index]=(b<<16)|(g<<8)|r;
}
static void vdp2_win0(int scr,int outside,int sx,int sy,int ex,int ey)
{
    (void)outside;
    assert(scr==-1 || (scr==1 && sx>=0 && sy>=0 && ex<320 && ey<240));
}
#include "cover-display.inc"
static void rendered(int w,int h,int version)
{
    gif_timer();assert(!gfx.data[0] && !gfx.data[2] && !dma_pending);
    int y=h>128?24:88;
    for(int row=0;row<240;row++)for(int col=0;col<512;col++) {
        u8 expected=0;
        if(col>=176 && col<320 && col<176+w && row>=y && row<y+h)
            expected=pixel(1024+(row-y)*w+col-176,version);
        assert(screen.data[0x20000+row*512+col]==expected);
    }
    for(unsigned i=0;i<256;i++)assert(display_palette[i]==
        ((u32)pixel(i*4,version)<<16 | (u32)pixel(i*4+1,version)<<8 | pixel(i*4+2,version)));
    for(unsigned i=0;i<32;i++)assert(screen.before[i]==0xa5 && screen.after[i]==0xa5);
}
static void pack(int w,int h,int header,int index,int two)
{
    FIL fp;UINT count;u8 *table=calloc(1,65536);assert(table);
    for(int i=0;i<=header;i++)snprintf((char*)table+i*32,12,"N%05d",i);
    for(int j=0;j<=two;j++) {
        u8 *entry=table+(header+j)*32;
        snprintf((char*)entry,12,"G%04d",index);
        put32(entry+12,ip_sum(j));put32(entry+16,65536+j*(w*h+1024));
        put16(entry+20,w);put16(entry+22,h);
    }
    assert(!f_open(&fp,"/SAROO/cover.bin",FA_CREATE_ALWAYS|FA_WRITE));
    assert(!f_write(&fp,table,65536,&count) && count==65536);free(table);
    unsigned size=w*h+1024;u8 *data=malloc(size);assert(data);
    for(int j=0;j<=two;j++) {
        for(unsigned i=0;i<size;i++)data[i]=pixel(i,j);
        assert(!f_write(&fp,data,size,&count) && count==size);
    }
    free(data);assert(!f_close(&fp));
}
static void edit_header(int offset,u32 value,int width)
{
    FIL fp;UINT count;u8 b[4];put32(b,value);
    assert(!f_open(&fp,"/SAROO/cover.bin",FA_WRITE));
    assert(!f_lseek(&fp,offset) && !f_write(&fp,b,width,&count) && count==(unsigned)width);
    assert(!f_close(&fp));
}
static void ready(void)
{
    cover_init();assert(c_live && c_reads);
}
static void displayed(int w,int h,int version)
{
    assert(gfx.data[0]==3 && gfx.data[2]==1);
    assert(get16(gfx.data+4)==(unsigned)w && get16(gfx.data+6)==(unsigned)h);
    assert(!get16(gfx.data+8) && !get16(gfx.data+10));
    assert(get16(gfx.data+12)==176 && get16(gfx.data+14)==(h>128?24:88));
    for(unsigned i=0;i<1024;i++)assert(gfx.data[0x100+i]==pixel(i,version));
    for(unsigned i=0;i<(unsigned)(w*h);i++)assert(gfx.data[0x1000+i]==pixel(1024+i,version));
}
static void absent(void)
{
    assert(gfx.data[0]==4 && !gfx.data[2]);
    assert(get16(gfx.data+4)==128 && get16(gfx.data+6)==128);
    for(unsigned i=0;i<128*128;i++)assert(!gfx.data[0x1000+i]);
}
static void cover_guards(void)
{
    for(unsigned i=0;i<32;i++)assert(gfx.before[i]==0xa5 && gfx.after[i]==0xa5);
    /* Nothing in the frame/cache/header gaps may be used as accidental storage. */
    for(unsigned i=0x20000;i<0xe0000;i++)assert(gfx.data[i]==0xa5);
    guards();
}
static void valid_cover(int value)
{
    const unsigned widths[]={128,128,1,127,512,128,128};
    const unsigned heights[]={128,192,1,97,240,128,128};
    unsigned w=widths[value],h=heights[value];
    pack(w,h,0,0,0);ready();load_cover(0);displayed(w,h,0);rendered(w,h,0);
    unsigned reads=c_reads,ips=c_ip;ack();load_cover(0);
    assert(!gfx.data[0] && c_reads==reads && c_ip==ips);
    load_cover(0x8000);displayed(w,h,0);rendered(w,h,0);
    assert(c_reads==reads+1 && c_ip==ips);
}
static void header_case(int value)
{
    pack(128,128,value==0?2047:0,0,0);
    if(value==1)edit_header(12,0,4);
    if(value==2) {
        for(int i=0;i<12;i++)edit_header(i,'A'+i,1);
        c_bad_ip=3;
    }
    if(value==3){edit_header(20,65535,2);edit_header(22,65535,2);}
    if(value==4)edit_header(20,0,2);
    if(value==5)edit_header(22,241,2);
    if(value==6)edit_header(16,65535,4);
    if(value==7)edit_header(20,513,2);
    if(value==8)edit_header(22,0,2);
    if(value==9)edit_header(16,0xffffffff,4);
    if(value==10)edit_header(16,65536+128*128+1,4);
    if(value==11) {
        FIL fp;assert(!f_open(&fp,"/SAROO/cover.bin",FA_WRITE));
        assert(!f_lseek(&fp,65536+128*128+1023) && !f_truncate(&fp) && !f_close(&fp));
    }
    ready();
    if(value==12)put16(gfx.data+0xe0000,2049);
    load_cover(0);
    if(value<3 && value!=1)displayed(128,128,0);
    else {absent();assert(c_reads==1);}
}
static void index_case(int value)
{
    int selected=0;pack(128,128,0,0,0);ready();
    if(value==0)selected=-1;
    if(value==1)selected=-32768;
    if(value==2)selected=65536;
    if(value==3)selected=1;
    if(value==4)selected=32767;
    if(value==5)category_current=-1;
    if(value==6)category_current=12;
    if(value==7){category_current=1;category_num=1;}
    if(value==8)total_disc=-1;
    if(value==9)total_disc=2560;
    if(value==10)total_disc=0;
    load_cover(selected);absent();assert(c_reads==1 && !c_ip);
}
static void catalog_case(int value)
{
    unsigned entries=value==1?2559:1;
    assert(!f_mkdir("/SAROO/ISO/Category"));strcpy(category,"Category");category_num=12;
    for(unsigned i=0;i<entries;i++) {
        char name[32];snprintf(name,sizeof(name),"Title%u",i);
        create("/SAROO/ISO/Category",name,1);
    }
    pack(128,128,0,entries-1,1);ready();
    for(int i=0;i<(value<2?12:2);i++) {
        ack();reset_scan();
        if(i && value==2){
            assert(!f_rename("/SAROO/ISO/Category/Title0","/SAROO/ISO/Category/Other"));
            c_version=1;
        }
        if(i && value>=4) {
            if(value==4) {
                assert(!f_unlink("/SAROO/ISO/Category/Title0"));
                assert(!list_disc(0,0) && !total_disc);
                load_cover(0);absent();ack();
                create("/SAROO/ISO/Category","Replacement",1);
            } else {
                create("/SAROO/BIN","One.bin",0);
                if(value==6)injected=3;
                int scanned=list_bins(0);
                if(value==6)assert(scanned<0 && !total_disc);
                else assert(!scanned && total_disc==1);
                assert(!f_rename("/SAROO/ISO/Category/Title0","/SAROO/ISO/Category/Replacement"));
            }
            c_version=1;reset_scan();
        }
        if(i && value==3)injected=3;
        int result=list_disc(value<2?i:0,0);
        if(i && value==3) {
            assert(result<0 && !total_disc);load_cover(0);absent();ack();
            reset_scan();assert(!list_disc(0,0));
        } else assert(!result && total_disc==(int)entries);
        unsigned ips=c_ip;load_cover(entries-1);displayed(128,128,c_version);assert(c_ip==ips+1);
    }
    assert(!memcmp(gfx.data+0xf0000,"G",1));
}
static void init_case(int value)
{
    if(value==0){load_cover(0);absent();assert(!c_reads && !c_opens);return;}
    pack(128,128,0,0,0);
    if(value==1)assert(!f_unlink("/SAROO/cover.bin"));
    if(value==2) {
        FIL fp;assert(!f_open(&fp,"/SAROO/cover.bin",FA_WRITE));
        assert(!f_lseek(&fp,65535) && !f_truncate(&fp) && !f_close(&fp));
    }
    if(value==3)c_fault=4;
    if(value==4)c_fault=6;
    if(value>=5) {
        ready();if(value==5)c_fault=7;
    }
    cover_init();
    if(value==6) {
        for(int i=0;i<3;i++)cover_init();
        assert(c_opens==5 && c_closes==4 && c_live);load_cover(0);displayed(128,128,0);return;
    }
    load_cover(0);absent();assert(!c_ip);
    if(value==5)assert(c_live && c_opens==1 && c_closes==1);
    if(value==2)return;
    ack();c_fault=0;if(value==1)pack(128,128,0,0,0);
    cover_init();load_cover(0);displayed(128,128,0);
}
static void fault_case(int value)
{
    pack(128,128,0,0,0);ready();
    if(value<5)c_fault=value+1;
    if(value==5)c_bad_ip=1;
    if(value==6)c_bad_ip=2;
    if(value==7)disk_read_fault=1;
    load_cover(0);absent();ack();c_fault=c_bad_ip=0;
    if(value==7) {
        assert(cover_fp.err);load_cover(0);absent();ack();cover_init();
    }
    load_cover(0);displayed(128,128,0);
}
static void busy_case(int value)
{
    pack(128,128,0,0,0);ready();
    if(value==3) {
        load_cover(0);displayed(128,128,0);
        memcpy(c_busy,gfx.data,sizeof(c_busy));unsigned reads=c_reads;
        load_cover(0);assert(!c_delays && reads==c_reads && !memcmp(c_busy,gfx.data,sizeof(c_busy)));
        return;
    }
    gfx.data[0]=value==2?0:3;gfx.data[2]=1;
    memcpy(c_busy,gfx.data,sizeof(c_busy));c_release=value==0?2:0;
    load_cover(0);
    if(value==0){displayed(128,128,0);assert(c_delays==2);}
    else {
        assert(c_delays==10 && c_reads==1 && !memcmp(c_busy,gfx.data,sizeof(c_busy)));
        ack();load_cover(0);displayed(128,128,0);
    }
}
int main(int argc,char **argv)
{
    assert(argc==4);const char *mode=argv[2];int value=atoi(argv[3]);
    disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(!f_mount(&fs,"0:",1));
    assert(!f_mkdir("/SAROO") && !f_mkdir("/SAROO/ISO") && !f_mkdir("/SAROO/BIN"));
    memset(&memory,0xa5,sizeof(memory));memset(&gfx,0xa5,sizeof(gfx));
    memset(&screen,0xa5,sizeof(screen));ack();
    fault="none";total_disc=1;
    if(!strcmp(mode,"cover-valid"))valid_cover(value);
    else if(!strcmp(mode,"cover-header"))header_case(value);
    else if(!strcmp(mode,"cover-index"))index_case(value);
    else if(!strcmp(mode,"cover-catalog"))catalog_case(value);
    else if(!strcmp(mode,"cover-init"))init_case(value);
    else if(!strcmp(mode,"cover-fault"))fault_case(value);
    else {assert(!strcmp(mode,"cover-busy"));busy_case(value);}
    cover_guards();assert(!fclose(disk));
    printf("%s %d: reads=%u seeks=%u opens=%u closes=%u previews=%u waits=%u\n",
           mode,value,c_reads,c_seeks,c_opens,c_closes,c_ip,c_delays);
    return 0;
}
